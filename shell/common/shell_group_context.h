// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_COMMON_SHELL_GROUP_CONTEXT_H_
#define SHELL_COMMON_SHELL_GROUP_CONTEXT_H_

#include "flutter/fml/memory/ref_ptr.h"
#include "flutter/fml/memory/thread_checker.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/fml/raster_thread_merger.h"
#include "flutter/fml/status.h"
#include "flutter/fml/synchronization/sync_switch.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/thread.h"

namespace flutter {

class ShellGroupContext : public fml::RefCountedThreadSafe<ShellGroupContext> {
 public:
  ShellGroupContext() = default;

  /// Creates a new merger from parent, share the inside shared_merger member
  /// when the platform_queue_id and raster_queue_id are same, otherwise create
  /// a new shared_merger instance
  fml::RefPtr<fml::RasterThreadMerger> CreateOrShareThreadMerger(
      fml::TaskQueueId platform_id,
      fml::TaskQueueId raster_id);

 private:
  std::mutex mutex_;
  fml::RefPtr<fml::RasterThreadMerger> parent_merger_;
  FML_FRIEND_REF_COUNTED_THREAD_SAFE(ShellGroupContext);
  FML_FRIEND_MAKE_REF_COUNTED(ShellGroupContext);
  FML_DISALLOW_COPY_AND_ASSIGN(ShellGroupContext);
};

}  // namespace flutter

#endif  // SHELL_COMMON_SHELL_GROUP_CONTEXT_H_
