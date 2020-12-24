// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_ANDROID_NATIVE_EXPORT_CODEC_H
#define FLUTTER_ANDROID_NATIVE_EXPORT_CODEC_H

#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/lib/ui/painting/native_export_codec.h"

// BD ADD: START
namespace flutter {

class AndroidNativeExportCodec : public flutter::NativeExportCodec {
 public:
  AndroidNativeExportCodec(JNIEnv *env, std::string cKey, jobject jCodec);

  ~AndroidNativeExportCodec() override;

 public:
  jobject codec = nullptr;
};

}  // namespace flutter
// END

#endif  // FLUTTER_ANDROID_NATIVE_EXPORT_CODEC_H
