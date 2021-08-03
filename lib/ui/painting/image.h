// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_LIB_UI_PAINTING_IMAGE_H_
#define FLUTTER_LIB_UI_PAINTING_IMAGE_H_

#include "flutter/flow/skia_gpu_object.h"
#include "flutter/lib/ui/dart_wrapper.h"
#include "flutter/lib/ui/ui_dart_state.h"
#include "third_party/skia/include/core/SkImage.h"

namespace tonic {
class DartLibraryNatives;
}  // namespace tonic

namespace flutter {

class CanvasImage final : public RefCountedDartWrappable<CanvasImage> {
  DEFINE_WRAPPERTYPEINFO();
  FML_FRIEND_MAKE_REF_COUNTED(CanvasImage);

 public:
  ~CanvasImage() override;
  static fml::RefPtr<CanvasImage> Create() {
    return fml::MakeRefCounted<CanvasImage>();
  }

  // BD ADD: START
  static fml::RefPtr<CanvasImage> Create(std::string key) {
      return fml::MakeRefCounted<CanvasImage>(key);
  }

  static fml::RefPtr<CanvasImage> Create(CanvasImage * image) {
      return fml::MakeRefCounted<CanvasImage>(image);
  }
  static std::map<std::string, CanvasImage *> image_recorder;
  int duration = 0;
  // END

  // BD MOD :START
//  int width() { return image_.get()->width(); }
//
//  int height() { return image_.get()->height(); }
  int width() {
    if (real_image_ != nullptr) {
        return real_image_->width();
    }
    return image_.get()->width();
  }

  int height() {
    if (real_image_ != nullptr) {
        return real_image_->height();
    }
    return image_.get()->height();
  }
  // END

  Dart_Handle toByteData(int format, Dart_Handle callback);

  void dispose();

  sk_sp<SkImage> image() const {
    // BD ADD:START
      if (real_image_ != nullptr) {
          return real_image_->image();
      }
    // END
      return image_.get();
  }
  void set_image(flutter::SkiaGPUObject<SkImage> image) {
      // BD ADD:START
      if (real_image_ != nullptr) {
          real_image_->set_image(std::move(image));
          return;
      }
      // END
    image_ = std::move(image);
  }

  size_t GetAllocationSize() const override;

  // BD ADD: START
  size_t ComputeByteSize() const;
  void RetainDartWrappableReference() const override;
  void ReleaseDartWrappableReference() const override;
  
  void setMips(bool isMips) {
      if (real_image_ != nullptr) {
          real_image_->setMips(isMips);
          return;
      }
    FML_LOG(INFO) << "[BDFlutterTesting]CanvasImage::setMips value=" << isMips;
    isMips_ = isMips;
  }
  // END

  static void RegisterNatives(tonic::DartLibraryNatives* natives);

 private:
  CanvasImage();

  // BD ADD: START
  explicit CanvasImage(std::string key);

  explicit CanvasImage(CanvasImage* image);
  // END

  flutter::SkiaGPUObject<SkImage> image_;

  // BD ADD: START
  bool isMips_ = true;
  std::string key_;
  CanvasImage* real_image_ = nullptr;
  // END
};

}  // namespace flutter

#endif  // FLUTTER_LIB_UI_PAINTING_IMAGE_H_
