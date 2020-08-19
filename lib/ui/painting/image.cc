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

CanvasImage::~CanvasImage() = default;

Dart_Handle CanvasImage::toByteData(int format, Dart_Handle callback) {
  return EncodeImage(this, format, callback);
}

void CanvasImage::dispose() {
  auto hint_freed_delegate = UIDartState::Current()->GetHintFreedDelegate();
  if (hint_freed_delegate) {
    hint_freed_delegate->HintFreed(GetAllocationSize());
  }
  image_.reset();
  ClearDartWrapper();
}

size_t CanvasImage::GetAllocationSize() const {
  // BD ADD: START
  return ComputeByteSize();
}

size_t CanvasImage::ComputeByteSize() const {
  // END
  if (auto image = image_.get()) {
    const auto& info = image->imageInfo();
    const auto kMipmapOverhead = 4.0 / 3.0;
    const size_t image_byte_size = info.computeMinByteSize() * kMipmapOverhead;
    return image_byte_size + sizeof(this);
  } else {
    return sizeof(CanvasImage);
  }
}

// BD ADD: START
void CanvasImage::RetainDartWrappableReference() const {
  RefCountedDartWrappable::RetainDartWrappableReference();
  Performance::GetInstance()->AddImageMemoryUsage(ComputeByteSize());
}

void CanvasImage::ReleaseDartWrappableReference() const {
  Performance::GetInstance()->SubImageMemoryUsage(ComputeByteSize());
  RefCountedDartWrappable::ReleaseDartWrappableReference();
}
// END

}  // namespace flutter
