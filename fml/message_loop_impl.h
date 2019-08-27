// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_FML_MESSAGE_LOOP_IMPL_H_
#define FLUTTER_FML_MESSAGE_LOOP_IMPL_H_

#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <queue>
#include <utility>

#include "flutter/fml/closure.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/memory/ref_counted.h"
#include "flutter/fml/message_loop.h"
#include "flutter/fml/synchronization/thread_annotations.h"
#include "flutter/fml/time/time_point.h"

namespace fml {
// BD ADD:
static const fml::TimePoint kZeroTimePoint;

class MessageLoopImpl : public fml::RefCountedThreadSafe<MessageLoopImpl> {
 public:
  static fml::RefPtr<MessageLoopImpl> Create();

  virtual ~MessageLoopImpl();

  virtual void Run() = 0;

  virtual void Terminate() = 0;

  virtual void WakeUp(fml::TimePoint time_point) = 0;

  void PostTask(fml::closure task, fml::TimePoint target_time);

  void AddTaskObserver(intptr_t key, fml::closure callback);

  void RemoveTaskObserver(intptr_t key);

  void DoRun();

  void DoTerminate();

  // BD ADD: START
  void PostBarrier(bool barrier_enabled);
  void PostTask(fml::closure task, fml::TimePoint target_time, bool is_low_priority);
  // END

 protected:
  // Exposed for the embedder shell which allows clients to poll for events
  // instead of dedicating a thread to the message loop.
  friend class MessageLoop;

  void RunExpiredTasksNow();

  void RunSingleExpiredTaskNow();

 protected:
  MessageLoopImpl();

 private:
  struct DelayedTask {
    size_t order;
    fml::closure task;
    fml::TimePoint target_time;

    DelayedTask(size_t p_order,
                fml::closure p_task,
                fml::TimePoint p_target_time);

    DelayedTask(const DelayedTask& other);

    ~DelayedTask();
  };

  struct DelayedTaskCompare {
    bool operator()(const DelayedTask& a, const DelayedTask& b) {
      /**
       * BD MOD: START
       * when the target_time is zero, always push the message at front of queue
       */
      // return a.target_time == b.target_time ? a.order > b.order
      //                                       : a.target_time > b.target_time;
      return a.target_time == b.target_time ? (a.target_time == kZeroTimePoint ? a.order < b.order : a.order > b.order) : a.target_time > b.target_time;
      // END
    }
  };

  using DelayedTaskQueue = std::
      priority_queue<DelayedTask, std::deque<DelayedTask>, DelayedTaskCompare>;

  std::map<intptr_t, fml::closure> task_observers_;
  std::mutex delayed_tasks_mutex_;
  DelayedTaskQueue delayed_tasks_ FML_GUARDED_BY(delayed_tasks_mutex_);
  size_t order_ FML_GUARDED_BY(delayed_tasks_mutex_);
  std::atomic_bool terminated_;

  void RegisterTask(fml::closure task, fml::TimePoint target_time);

  enum class FlushType {
    kSingle,
    kAll,
  };
  void FlushTasks(FlushType type);

  // BD ADD: START
  std::atomic_bool barrier_enabled_;
  DelayedTaskQueue low_priority_tasks_ FML_GUARDED_BY(delayed_tasks_mutex_);
  void RegisterTask(fml::closure task, fml::TimePoint target_time, bool is_low_priority);
  void FlushLowPriorityTasks(FlushType type);
  // END

  FML_DISALLOW_COPY_AND_ASSIGN(MessageLoopImpl);
};

}  // namespace fml

#endif  // FLUTTER_FML_MESSAGE_LOOP_IMPL_H_
