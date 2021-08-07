// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include "flutter/fml/message_loop_task_queues.h"

#include <iostream>
#include <memory>

#include "flutter/fml/make_copyable.h"

namespace fml {

std::mutex MessageLoopTaskQueues::creation_mutex_;

const size_t TaskQueueId::kUnmerged = ULONG_MAX;

fml::RefPtr<MessageLoopTaskQueues> MessageLoopTaskQueues::instance_;

TaskQueueEntry::TaskQueueEntry()
    : subsumed_by(_kUnmerged) {
  wakeable = NULL;
  task_observers = TaskObservers();
  delayed_tasks = DelayedTaskQueue();
}

fml::RefPtr<MessageLoopTaskQueues> MessageLoopTaskQueues::GetInstance() {
  std::scoped_lock creation(creation_mutex_);
  if (!instance_) {
    instance_ = fml::MakeRefCounted<MessageLoopTaskQueues>();
  }
  return instance_;
}

TaskQueueId MessageLoopTaskQueues::CreateTaskQueue() {
  std::lock_guard guard(queue_mutex_);
  TaskQueueId loop_id = TaskQueueId(task_queue_id_counter_);
  ++task_queue_id_counter_;
  queue_entries_[loop_id] = std::make_unique<TaskQueueEntry>();
  return loop_id;
}

MessageLoopTaskQueues::MessageLoopTaskQueues()
    : task_queue_id_counter_(0), order_(0) {}

MessageLoopTaskQueues::~MessageLoopTaskQueues() = default;

void MessageLoopTaskQueues::Dispose(TaskQueueId queue_id) {
  std::lock_guard guard(queue_mutex_);
  const auto& queue_entry = queue_entries_.at(queue_id);
  FML_DCHECK(queue_entry->subsumed_by == _kUnmerged);
  auto& subsumed_set = queue_entry->owner_of;
  for (auto& subsumed : subsumed_set) {
    queue_entries_.erase(subsumed);
  }
  // Erase owner queue_id at last to avoid &subsumed_set from being invalid
  queue_entries_.erase(queue_id);
}

void MessageLoopTaskQueues::DisposeTasks(TaskQueueId queue_id) {
  std::lock_guard guard(queue_mutex_);
  const auto& queue_entry = queue_entries_.at(queue_id);
  FML_DCHECK(queue_entry->subsumed_by == _kUnmerged);
  auto& subsumed_set = queue_entry->owner_of;
  queue_entry->delayed_tasks = {};
  for (auto& subsumed : subsumed_set) {
    queue_entries_.at(subsumed)->delayed_tasks = {};
  }
}

void MessageLoopTaskQueues::RegisterTask(TaskQueueId queue_id,
                                         const fml::closure& task,
                                         fml::TimePoint target_time) {
  std::lock_guard guard(queue_mutex_);
  size_t order = order_++;
  const auto& queue_entry = queue_entries_.at(queue_id);
  queue_entry->delayed_tasks.push({order, task, target_time});
  TaskQueueId loop_to_wake = queue_id;
  if (queue_entry->subsumed_by != _kUnmerged) {
    loop_to_wake = queue_entry->subsumed_by;
  }
  WakeUpUnlocked(loop_to_wake, GetNextWakeTimeUnlocked(loop_to_wake));
}

bool MessageLoopTaskQueues::HasPendingTasks(TaskQueueId queue_id) const {
  std::lock_guard guard(queue_mutex_);
  return HasPendingTasksUnlocked(queue_id);
}

fml::closure MessageLoopTaskQueues::GetNextTaskToRun(TaskQueueId queue_id,
                                                     fml::TimePoint from_time) {
  std::lock_guard guard(queue_mutex_);
  if (!HasPendingTasksUnlocked(queue_id)) {
    return nullptr;
  }
  TaskQueueId top_queue = _kUnmerged;
  const auto& top = PeekNextTaskUnlocked(queue_id, top_queue);

  if (!HasPendingTasksUnlocked(queue_id)) {
    WakeUpUnlocked(queue_id, fml::TimePoint::Max());
  } else {
    WakeUpUnlocked(queue_id, GetNextWakeTimeUnlocked(queue_id));
  }

  if (top.GetTargetTime() > from_time) {
    return nullptr;
  }
  fml::closure invocation = top.GetTask();
  queue_entries_.at(top_queue)->delayed_tasks.pop();
  return invocation;
}

void MessageLoopTaskQueues::WakeUpUnlocked(TaskQueueId queue_id,
                                           fml::TimePoint time) const {
  if (queue_entries_.at(queue_id)->wakeable) {
    queue_entries_.at(queue_id)->wakeable->WakeUp(time);
  }
}

size_t MessageLoopTaskQueues::GetNumPendingTasks(TaskQueueId queue_id) const {
  std::lock_guard guard(queue_mutex_);
  const auto& queue_entry = queue_entries_.at(queue_id);
  if (queue_entry->subsumed_by != _kUnmerged) {
    return 0;
  }

  size_t total_tasks = 0;
  total_tasks += queue_entry->delayed_tasks.size();

  auto& subsumed_set = queue_entry->owner_of;
  for (auto& subsumed : subsumed_set) {
    const auto& subsumed_entry = queue_entries_.at(subsumed);
    total_tasks += subsumed_entry->delayed_tasks.size();
  }
  return total_tasks;
}

void MessageLoopTaskQueues::AddTaskObserver(TaskQueueId queue_id,
                                            intptr_t key,
                                            const fml::closure& callback) {
  std::lock_guard guard(queue_mutex_);
  FML_DCHECK(callback != nullptr) << "Observer callback must be non-null.";
  queue_entries_.at(queue_id)->task_observers[key] = callback;
}

void MessageLoopTaskQueues::RemoveTaskObserver(TaskQueueId queue_id,
                                               intptr_t key) {
  std::lock_guard guard(queue_mutex_);
  queue_entries_.at(queue_id)->task_observers.erase(key);
}

std::vector<fml::closure> MessageLoopTaskQueues::GetObserversToNotify(
    TaskQueueId queue_id) const {
  std::lock_guard guard(queue_mutex_);
  std::vector<fml::closure> observers;

  if (queue_entries_.at(queue_id)->subsumed_by != _kUnmerged) {
    return observers;
  }

  for (const auto& observer : queue_entries_.at(queue_id)->task_observers) {
    observers.push_back(observer.second);
  }

  auto& subsumed_set = queue_entries_.at(queue_id)->owner_of;
  for (auto& subsumed : subsumed_set) {
    for (const auto& observer : queue_entries_.at(subsumed)->task_observers) {
      observers.push_back(observer.second);
    }
  }

  return observers;
}

void MessageLoopTaskQueues::SetWakeable(TaskQueueId queue_id,
                                        fml::Wakeable* wakeable) {
  std::lock_guard guard(queue_mutex_);
  FML_CHECK(!queue_entries_.at(queue_id)->wakeable)
      << "Wakeable can only be set once.";
  queue_entries_.at(queue_id)->wakeable = wakeable;
}

bool MessageLoopTaskQueues::Merge(TaskQueueId owner, TaskQueueId subsumed) {
  if (owner == subsumed) {
    return true;
  }
  std::lock_guard guard(queue_mutex_);
  auto& owner_entry = queue_entries_.at(owner);
  auto& subsumed_entry = queue_entries_.at(subsumed);
  auto& subsumed_set = owner_entry->owner_of;
  if (subsumed_set.find(subsumed) != subsumed_set.end()) {
    return true;
  }

  // Won't check owner_entry->owner_of, because it may contains items when
  // merged with other different queues.

  // Ensure owner_entry->subsumed_by being _kUnmerged
  if (owner_entry->subsumed_by != _kUnmerged) {
    FML_LOG(WARNING) << "Thread merging failed: owner_entry was already "
                        "subsumed by others, owner="
                     << owner << ", subsumed=" << subsumed
                     << ", owner->subsumed_by=" << owner_entry->subsumed_by;
    return false;
  }
  // Ensure subsumed_entry->owner_of being empty
  if (!subsumed_entry->owner_of.empty()) {
    FML_LOG(WARNING)
        << "Thread merging failed: subsumed_entry already owns others, owner="
        << owner << ", subsumed=" << subsumed
        << ", subsumed->owner_of.size()=" << subsumed_entry->owner_of.size();
    return false;
  }
  // Ensure subsumed_entry->subsumed_by being _kUnmerged
  if (subsumed_entry->subsumed_by != _kUnmerged) {
    FML_LOG(WARNING) << "Thread merging failed: subsumed_entry was already "
                        "subsumed by others, owner="
                     << owner << ", subsumed=" << subsumed
                     << ", subsumed->subsumed_by="
                     << subsumed_entry->subsumed_by;
    return false;
  }
  // All checking is OK, set merged state.
  owner_entry->owner_of.insert(subsumed);
  subsumed_entry->subsumed_by = owner;

  if (HasPendingTasksUnlocked(owner)) {
    WakeUpUnlocked(owner, GetNextWakeTimeUnlocked(owner));
  }

  return true;
}

bool MessageLoopTaskQueues::Unmerge(TaskQueueId owner, TaskQueueId subsumed) {
  std::lock_guard guard(queue_mutex_);
  const auto& owner_entry = queue_entries_.at(owner);
  if (owner_entry->owner_of.empty()) {
    FML_LOG(WARNING)
        << "Thread unmerging failed: owner_entry doesn't own anyone, owner="
        << owner << ", subsumed=" << subsumed;
    return false;
  }
  if (owner_entry->subsumed_by != _kUnmerged) {
    FML_LOG(WARNING)
        << "Thread unmerging failed: owner_entry was subsumed by others, owner="
        << owner << ", subsumed=" << subsumed
        << ", owner_entry->subsumed_by=" << owner_entry->subsumed_by;
    return false;
  }
  if (queue_entries_.at(subsumed)->subsumed_by == _kUnmerged) {
    FML_LOG(WARNING) << "Thread unmerging failed: subsumed_entry wasn't "
                        "subsumed by others, owner="
                     << owner << ", subsumed=" << subsumed;
    return false;
  }
  if (owner_entry->owner_of.find(subsumed) == owner_entry->owner_of.end()) {
    FML_LOG(WARNING) << "Thread unmerging failed: owner_entry didn't own the "
                        "given subsumed queue id, owner="
                     << owner << ", subsumed=" << subsumed;
    return false;
  }

  queue_entries_.at(subsumed)->subsumed_by = _kUnmerged;
  owner_entry->owner_of.erase(subsumed);

  if (HasPendingTasksUnlocked(owner)) {
    WakeUpUnlocked(owner, GetNextWakeTimeUnlocked(owner));
  }

  if (HasPendingTasksUnlocked(subsumed)) {
    WakeUpUnlocked(subsumed, GetNextWakeTimeUnlocked(subsumed));
  }

  return true;
}

bool MessageLoopTaskQueues::Owns(TaskQueueId owner,
                                 TaskQueueId subsumed) const {
  std::lock_guard guard(queue_mutex_);
  if (owner == _kUnmerged || subsumed == _kUnmerged) {
    return false;
  }
  auto& subsumed_set = queue_entries_.at(owner)->owner_of;
  return subsumed_set.find(subsumed) != subsumed_set.end();
}

std::set<TaskQueueId> MessageLoopTaskQueues::GetSubsumedTaskQueueId(
    TaskQueueId owner) const {
  std::lock_guard guard(queue_mutex_);
  return queue_entries_.at(owner)->owner_of;
}

// Subsumed queues will never have pending tasks.
// Owning queues will consider both their and their subsumed tasks.
bool MessageLoopTaskQueues::HasPendingTasksUnlocked(
    TaskQueueId queue_id) const {
  const auto& entry = queue_entries_.at(queue_id);
  bool is_subsumed = entry->subsumed_by != _kUnmerged;
  if (is_subsumed) {
    return false;
  }

  if (!entry->delayed_tasks.empty()) {
    return true;
  }

  auto& subsumed_set = entry->owner_of;
  return std::any_of(
      subsumed_set.begin(), subsumed_set.end(), [&](const auto& subsumed) {
        return !queue_entries_.at(subsumed)->delayed_tasks.empty();
      });
}

fml::TimePoint MessageLoopTaskQueues::GetNextWakeTimeUnlocked(
    TaskQueueId queue_id) const {
  TaskQueueId tmp = _kUnmerged;
  return PeekNextTaskUnlocked(queue_id, tmp).GetTargetTime();
}

const DelayedTask& MessageLoopTaskQueues::PeekNextTaskUnlocked(
    TaskQueueId owner,
    TaskQueueId& top_queue_id) const {
  FML_DCHECK(HasPendingTasksUnlocked(owner));
  const auto& entry = queue_entries_.at(owner);
  if (entry->owner_of.empty()) {
    FML_CHECK(!entry->delayed_tasks.empty());
    top_queue_id = owner;
    return entry->delayed_tasks.top();
  }

  const DelayedTaskQueue* top_task_queue = nullptr;

  auto top_task_updater = [&top_task_queue, &top_queue_id](
                              const DelayedTaskQueue* queue,
                              TaskQueueId queue_id) {
    if (queue && !queue->empty()) {
      auto& other_task = queue->top();
      if (top_task_queue == nullptr || top_task_queue->top() > other_task) {
        top_task_queue = queue;
        top_queue_id = queue_id;
      }
    }
  };

  auto& owner_tasks = entry->delayed_tasks;
  top_task_updater(&owner_tasks, owner);

  for (TaskQueueId subsumed : entry->owner_of) {
    auto& subsumed_tasks = queue_entries_.at(subsumed)->delayed_tasks;
    top_task_updater(&subsumed_tasks, subsumed);
  }
  // At least one task at the top because PeekNextTaskUnlocked() is called after
  // HasPendingTasksUnlocked()
  FML_CHECK(top_task_queue != nullptr);
  const DelayedTask& top = top_task_queue->top();
  return top;
}

}  // namespace fml
