// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/image.h"

#include "flutter/lib/ui/painting/image_encoding.h"
#include "third_party/tonic/converter/dart_converter.h"
#include "third_party/tonic/dart_args.h"
#include "third_party/tonic/dart_binding_macros.h"
#include "third_party/tonic/dart_library_natives.h"
// BD ADD:
#include "flutter/bdflutter/lib/ui/performance/performance.h"

namespace flutter {

typedef CanvasImage Image;

// Since _Image is a private class, we can't use IMPLEMENT_WRAPPERTYPEINFO
static const tonic::DartWrapperInfo kDartWrapperInfo_ui_Image = {
    "ui",
    "_Image",
    sizeof(Image),
};
const tonic::DartWrapperInfo& Image::dart_wrapper_info_ =
    kDartWrapperInfo_ui_Image;
// BD ADD
std::map<std::string, CanvasImage*> CanvasImage::image_recorder;

#define FOR_EACH_BINDING(V) \
  V(Image, width)           \
  V(Image, height)          \
  V(Image, toByteData)      \
  V(Image, dispose)

FOR_EACH_BINDING(DART_NATIVE_CALLBACK)

void CanvasImage::RegisterNatives(tonic::DartLibraryNatives* natives) {
  natives->Register({FOR_EACH_BINDING(DART_REGISTER_NATIVE)});
}

CanvasImage::CanvasImage() = default;

// BD ADD: START
CanvasImage::CanvasImage(std::string key) : key_(std::move(key)) {
    image_recorder.erase(key_);
    image_recorder.insert(std::make_pair(key_, this));
}

CanvasImage::CanvasImage(CanvasImage *image) {
    real_image_ = image;
    real_image_->RetainDartWrappableReference();
}

CanvasImage::~CanvasImage() {
  if (real_image_ == nullptr) {
      image_recorder.erase(key_);
      image_.reset();
  } else {
      real_image_->ReleaseDartWrappableReference();
  }
}
// END

Dart_Handle CanvasImage::toByteData(int format, Dart_Handle callback) {
  return EncodeImage(this, format, callback);
}

void CanvasImage::dispose() {
  if (real_image_ == nullptr) {
    auto hint_freed_delegate = UIDartState::Current()->GetHintFreedDelegate();
    if (hint_freed_delegate) {
        hint_freed_delegate->HintFreed(GetAllocationSize());
    }
  }

  ClearDartWrapper();
}

size_t CanvasImage::GetAllocationSize() const {
  // BD ADD: START
  return ComputeByteSize();
}

size_t CanvasImage::ComputeByteSize() const {
    if (real_image_ != nullptr) {
        return real_image_->ComputeByteSize();
    }
  // END
  if (auto image = image_.get()) {
    const auto& info = image->imageInfo();
    // BD MOD:
    // const auto kMipmapOverhead = 4.0 / 3.0;
    const auto kMipmapOverhead = isMips_ ? 4.0 / 3.0 : 1.0;
    const size_t image_byte_size = info.computeMinByteSize() * kMipmapOverhead;
    return image_byte_size + sizeof(this);
  } else {
    return sizeof(CanvasImage);
  }
}

// BD ADD: START
void CanvasImage::RetainDartWrappableReference() const {
  RefCountedDartWrappable::RetainDartWrappableReference();
  Performance::GetInstance()->AddImageMemoryUsage(ComputeByteSize() >> 10);
}

void CanvasImage::ReleaseDartWrappableReference() const {
  Performance::GetInstance()->SubImageMemoryUsage(ComputeByteSize() >> 10);
  RefCountedDartWrappable::ReleaseDartWrappableReference();
}
// END

}  // namespace flutter
