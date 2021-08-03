// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_LIB_UI_PAINTING_SINGLE_FRAME_CODEC_H_
#define FLUTTER_LIB_UI_PAINTING_SINGLE_FRAME_CODEC_H_

#include "flutter/fml/macros.h"
#include "flutter/lib/ui/painting/codec.h"
#include "flutter/lib/ui/painting/image.h"
#include "flutter/lib/ui/painting/image_decoder.h"
#include "flutter/lib/ui/painting/image_descriptor.h"

namespace flutter {

class SingleFrameCodec : public Codec {
 public:
  SingleFrameCodec(fml::RefPtr<ImageDescriptor> descriptor,
                   uint32_t target_width,
                   uint32_t target_height,
                   std::string key);

  SingleFrameCodec(Codec* codec, std::string key);

  ~SingleFrameCodec() override;

  // |Codec|
  int frameCount() const override;

  // |Codec|
  int repetitionCount() const override;

  CodecType getClassType() const override;

  Dart_Handle innerDecodeImage(std::function<void()> listener);

  // |Codec|
  Dart_Handle getNextFrame(Dart_Handle args) override;

  // |DartWrappable|
  size_t GetAllocationSize() const override;

 private:
  enum class Status { kNew, kInProgress, kComplete };
  Status status_;
  fml::RefPtr<ImageDescriptor> descriptor_;
  uint32_t target_width_;
  uint32_t target_height_;
  fml::RefPtr<CanvasImage> cached_image_;
  std::vector<DartPersistentValue> pending_callbacks_;
  std::string key_;
  size_t allocation_size_ = -1;
  std::vector<std::function<void()>> listeners_;

  FML_FRIEND_MAKE_REF_COUNTED(SingleFrameCodec);
  FML_FRIEND_REF_COUNTED_THREAD_SAFE(SingleFrameCodec);
};

}  // namespace flutter

#endif  // FLUTTER_LIB_UI_PAINTING_SINGLE_FRAME_CODEC_H_
