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
    template<typename T>
    struct ImageLoaderCallbackContext {
        std::function<T> callback;
        fml::RefPtr<fml::TaskRunner> io_task_runner;
        fml::WeakPtr<GrContext> resourceContext;
        ImageLoaderCallbackContext(std::function<T> callback) : callback(callback){}
        ~ ImageLoaderCallbackContext() {
            if (callback != nullptr) {
                callback(nullptr);
            }
        }
    };
    
}  // namespace flutter
// END

#endif  // FLUTTER_SHELL_PLATFORM_IOS_IMAGE_LOADER_CALLBACK_CONTEXT_H_
