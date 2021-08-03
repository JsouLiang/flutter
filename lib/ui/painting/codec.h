// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_LIB_UI_PAINTING_CODEC_H_
#define FLUTTER_LIB_UI_PAINTING_CODEC_H_

#include "flutter/lib/ui/dart_wrapper.h"
#include "flutter/lib/ui/ui_dart_state.h"
#include "third_party/skia/include/codec/SkCodec.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"

using tonic::DartPersistentValue;

namespace tonic {
class DartLibraryNatives;
}  // namespace tonic

namespace flutter {


enum CodecType {
    SingleFrame,
    MultiFrame,
    Native
};
// A handle to an SkCodec object.
//
// Doesn't mirror SkCodec's API but provides a simple sequential access API.
class Codec : public RefCountedDartWrappable<Codec> {
  DEFINE_WRAPPERTYPEINFO();

 // BD ADD: START
 protected:
  Codec* real_codec = nullptr;
 // END
 public:
  virtual int frameCount() const = 0;

  virtual int repetitionCount() const = 0;

  virtual Dart_Handle getNextFrame(Dart_Handle callback_handle) = 0;

  // BD ADD
  virtual CodecType getClassType() const  = 0;

  void dispose();

  static void RegisterNatives(tonic::DartLibraryNatives* natives);

  // BD ADD: START
  static std::map<std::string, Codec *> codec_recorder;
  static std::vector<std::pair<std::string, int>> cached_images;
  static std::vector<DartPersistentValue> dart_remove_cache_callbacks;
  static std::vector<DartPersistentValue> dart_get_keys_callbacks;
  static int max_cache_size;
  // END
};

}  // namespace flutter

#endif  // FLUTTER_LIB_UI_PAINTING_CODEC_H_
