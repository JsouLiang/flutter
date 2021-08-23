// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/common/shell_group_context.h"

#include "flutter/fml/raster_thread_merger.h"

namespace flutter {

fml::RefPtr<fml::RasterThreadMerger>
ShellGroupContext::CreateOrShareThreadMerger(fml::TaskQueueId platform_id,
                                             fml::TaskQueueId raster_id) {
  std::scoped_lock lock(mutex_);
  auto new_merger = fml::RasterThreadMerger::CreateOrShareThreadMerger(
      parent_merger_, platform_id, raster_id);
  parent_merger_ = new_merger;
  return new_merger;
}

// END
}  // namespace flutter
