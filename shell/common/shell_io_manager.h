// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_COMMON_SHELL_IO_MANAGER_H_
#define FLUTTER_SHELL_COMMON_SHELL_IO_MANAGER_H_

#include <memory>

#include "flutter/flow/skia_gpu_object.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/lib/ui/io_manager.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace flutter {

class ShellIOManager final : public IOManager {
 public:
  // Convenience methods for platforms to create a GrContext used to supply to
  // the IOManager. The platforms may create the context themselves if they so
  // desire.
  static sk_sp<GrContext> CreateCompatibleResourceLoadingContext(
      GrBackend backend,
      sk_sp<const GrGLInterface> gl_interface);

  ShellIOManager(sk_sp<GrContext> resource_context,
                 fml::RefPtr<fml::TaskRunner> unref_queue_task_runner,
                 bool should_defer_decode_image_when_platform_view_invalid);

  ~ShellIOManager() override;

  fml::WeakPtr<GrContext> GetResourceContext() const override;

  // This method should be called when a resource_context first becomes
  // available. It is safe to call multiple times, and will only update
  // the held resource context if it has not already been set.
  void NotifyResourceContextAvailable(sk_sp<GrContext> resource_context);

  // This method should be called if you want to force the IOManager to
  // update its resource context reference. It should not be called
  // if there are any Dart objects that have a reference to the old
  // resource context, but may be called if the Dart VM is restarted.
  void UpdateResourceContext(sk_sp<GrContext> resource_context);

  fml::RefPtr<flutter::SkiaUnrefQueue> GetSkiaUnrefQueue() const override;

  fml::WeakPtr<ShellIOManager> GetWeakPtr();

  void UpdatePlatformViewValid(bool valid);
  bool IsResourceContextValidForDecodeImage() const override;
    
  void RegisterImageLoader(std::shared_ptr<flutter::ImageLoader> imageLoader);
    
  std::shared_ptr<flutter::ImageLoader> GetImageLoader() const override;

 private:
  // Resource context management.
  sk_sp<GrContext> resource_context_;
  std::unique_ptr<fml::WeakPtrFactory<GrContext>>
      resource_context_weak_factory_;

  // Unref queue management.
  fml::RefPtr<flutter::SkiaUnrefQueue> unref_queue_;

  fml::WeakPtrFactory<ShellIOManager> weak_factory_;
    
  std::shared_ptr<flutter::ImageLoader> imageLoader_;

  FML_DISALLOW_COPY_AND_ASSIGN(ShellIOManager);

  bool is_platform_view_valid_ = false;
  bool should_defer_decode_image_when_platform_view_invalid_ = false;
};

}  // namespace flutter

#endif  // FLUTTER_SHELL_COMMON_SHELL_IO_MANAGER_H_
