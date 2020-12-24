// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_EXPORT_CODEC_H_
#define NATIVE_EXPORT_CODEC_H_

#include <third_party/skia/include/core/SkImage.h>


namespace flutter {

class NativeExportCodec {
 public:
  NativeExportCodec();

  virtual ~NativeExportCodec();

  // |Codec|
  int frameCount() const;

  // |Codec|
  int repetitionCount() const;

  // |Codec|
  int duration(int currentFrame) const;

public:
    int frameCount_;
    int repetitionCount_;
    int width;
    int height;
    sk_sp<SkImage> skImage;
    int* frameDurations = nullptr;
    const std::string *key = nullptr;

};

}  // namespace flutter

#endif
