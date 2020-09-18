// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/bdflutter/shell/platform/darwin/ios/ios_external_image_loader.h"

#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>

#include "flutter/shell/platform/darwin/ios/framework/Source/vsync_waiter_ios.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "flutter/common/task_runners.h"
#include "flutter/fml/make_copyable.h"
#include "flutter/lib/ui/ui_dart_state.h"

#include "flutter/bdflutter/shell/platform/darwin/ios/ios_external_image_loader_gl.h"
#if FLUTTER_SHELL_ENABLE_METAL
#include "flutter/bdflutter/shell/platform/darwin/ios/ios_external_image_loader_metal.h"
#endif  // FLUTTER_SHELL_ENABLE_METAL
#include "flutter/bdflutter/shell/platform/darwin/ios/ios_image_loader_callback_context.h"

// BD ADD: START
namespace flutter {

    IOSExternalImageLoader* IOSExternalImageLoader::FromIOSRenderingAPI(IOSRenderingAPI renderingApi, NSObject<FlutterImageLoader> *imageLoader) {
        switch (renderingApi) {
            #if FLUTTER_SHELL_ENABLE_METAL
            case IOSRenderingAPI::kMetal:
                return new IOSMetalExternalImageLoader(imageLoader);
            #endif  // FLUTTER_SHELL_ENABLE_METAL
            default:
                return new IOSOpenGLExternalImageLoader(imageLoader);
        }
    }

    IOSExternalImageLoader::IOSExternalImageLoader(NSObject<FlutterImageLoader>* imageLoader)
        : imageLoader_(fml::scoped_nsobject<NSObject<FlutterImageLoader>>([imageLoader retain])) {
        FML_DCHECK(imageLoader_);
    }
    
}  // namespace flutter
// END
