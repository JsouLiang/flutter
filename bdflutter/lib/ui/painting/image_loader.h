// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_FLOW_IMAGE_LOADER_H_
#define FLUTTER_FLOW_IMAGE_LOADER_H_

#include "third_party/skia/include/core/SkCanvas.h"
#include "flutter/fml/macros.h"
#include "flutter/lib/ui/painting/native_export_codec.h"
#include "flutter/fml/memory/weak_ptr.h"
#include "flutter/common/task_runners.h"

// BD ADD: START
namespace flutter {

    struct ImageLoaderContext {
        const TaskRunners task_runners;
        fml::WeakPtr<GrContext> resourceContext;
        ImageLoaderContext(const TaskRunners& task_runners, fml::WeakPtr<GrContext> resourceContext) : task_runners(task_runners), resourceContext(std::move(resourceContext)){}
    };

    class ImageLoader {
    public:
        ImageLoader();

        virtual ~ImageLoader();

        virtual void LoadCodec(const std::string url, const int width, const int height, const float scale, ImageLoaderContext contextPtr, std::function<void(std::unique_ptr<NativeExportCodec> codec)> callback) = 0;

        virtual void GetNextFrame(ImageLoaderContext contextPtr, int currentFrame, std::shared_ptr<NativeExportCodec> codec, std::function<void(sk_sp<SkImage>)> callback) = 0;

        virtual void Load(const std::string url, const int width, const int height, const float scale, ImageLoaderContext contextPtr, std::function<void(sk_sp<SkImage> image)> callback) = 0;

        FML_DISALLOW_COPY_AND_ASSIGN(ImageLoader);
    };

}  // namespace flutter
// END

#endif  // FLUTTER_FLOW_IMAGE_LOADER_H_
