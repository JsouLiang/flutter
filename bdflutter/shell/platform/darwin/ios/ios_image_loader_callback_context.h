#ifndef FLUTTER_SHELL_PLATFORM_IOS_IMAGE_LOADER_CALLBACK_CONTEXT_H_
#define FLUTTER_SHELL_PLATFORM_IOS_IMAGE_LOADER_CALLBACK_CONTEXT_H_

#include "flutter/shell/platform/darwin/ios/framework/Source/vsync_waiter_ios.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "flutter/common/task_runners.h"
#include "flutter/fml/make_copyable.h"
#include "flutter/lib/ui/ui_dart_state.h"

// BD ADD: START
namespace flutter {

    struct ImageLoaderCallbackContext {
        std::function<void(sk_sp<SkImage> image)> callback;
        const TaskRunners task_runners;
        ImageLoaderCallbackContext(const TaskRunners& task_runners) : callback(nullptr), task_runners(task_runners){}
        ~ ImageLoaderCallbackContext() {
            if (callback != nullptr && task_runners.IsValid()) {
                task_runners.GetUITaskRunner()->PostTask(fml::MakeCopyable(
                    [callback = std::move(callback)]() mutable {
                      callback(nullptr);
                    }));
            }
        }
    };
    
}  // namespace flutter
// END

#endif  // FLUTTER_SHELL_PLATFORM_IOS_IMAGE_LOADER_CALLBACK_CONTEXT_H_
