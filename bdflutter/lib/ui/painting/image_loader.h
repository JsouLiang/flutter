// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_FLOW_IMAGE_LOADER_H_
#define FLUTTER_FLOW_IMAGE_LOADER_H_

#include "third_party/skia/include/core/SkCanvas.h"
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
        virtual void Load(const std::string url, const int width, const int height, const float scale, ImageLoaderContext contextPtr, std::function<void(sk_sp<SkImage> image)> callback) = 0;
        
        FML_DISALLOW_COPY_AND_ASSIGN(ImageLoader);
    };

}  // namespace flutter
// END

#endif  // FLUTTER_FLOW_IMAGE_LOADER_H_
