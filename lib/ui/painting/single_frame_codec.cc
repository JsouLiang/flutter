#include <utility>

// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/single_frame_codec.h"

#include "flutter/lib/ui/ui_dart_state.h"
#include "third_party/tonic/logging/dart_invoke.h"

// BD ADD:
#include "flutter/bdflutter/lib/ui/performance/performance.h"

namespace flutter {

SingleFrameCodec::SingleFrameCodec(fml::RefPtr<ImageDescriptor> descriptor,
                                   uint32_t target_width,
                                   uint32_t target_height,
                                   std::string key)
    : status_(Status::kNew),
      descriptor_(std::move(descriptor)),
      target_width_(target_width),
      target_height_(target_height),
      key_(std::move(key)) {
    codec_recorder.insert(std::make_pair(key_, this));
    allocation_size_ = GetAllocationSize();
}

SingleFrameCodec::SingleFrameCodec(flutter::Codec *codec,std::string key)
   : key_(std::move(key)){
    real_codec = codec;
    real_codec->RetainDartWrappableReference();
}

SingleFrameCodec::~SingleFrameCodec(){
    if (real_codec == nullptr) {
        codec_recorder.erase(key_);
    } else {
        real_codec->ReleaseDartWrappableReference();
    }
}

int SingleFrameCodec::frameCount() const {
  return 1;
}

int SingleFrameCodec::repetitionCount() const {
  return 0;
}

CodecType SingleFrameCodec::getClassType() const {
    return CodecType::SingleFrame;
}

Dart_Handle SingleFrameCodec::innerDecodeImage(std::function<void()> listener) {
  listeners_.emplace_back(listener);
  if (status_ == Status::kInProgress) {
      return Dart_Null();
  }
  auto dart_state = UIDartState::Current();
  auto decoder = dart_state->GetImageDecoder();

  if (!decoder) {
    return tonic::ToDart("Image decoder not available.");
  }


  // The SingleFrameCodec must be deleted on the UI thread.  Allocate a RefPtr
  // on the heap to ensure that the SingleFrameCodec remains alive until the
  // decoder callback is invoked on the UI thread.  The callback can then
  // drop the reference.
  fml::RefPtr<SingleFrameCodec>* raw_codec_ref =
          new fml::RefPtr<SingleFrameCodec>(this);

  decoder->Decode(
          descriptor_, target_width_, target_height_, [raw_codec_ref](auto image) {
              std::unique_ptr<fml::RefPtr<SingleFrameCodec>> codec_ref(raw_codec_ref);
              fml::RefPtr<SingleFrameCodec> codec(std::move(*codec_ref));
              FML_LOG(ERROR) << "sunkun SingleFrameCodec Decode Success";

              if (image.get()) {
                  auto canvas_image = fml::MakeRefCounted<CanvasImage>(codec->key_);
                  // BD ADD:
                  canvas_image->setMips(!flutter::Performance::GetInstance()->IsDisableMips());
                  canvas_image->set_image(std::move(image));

                  codec->cached_image_ = std::move(canvas_image);
              }

              // The cached frame is now available and should be returned to any
              // future callers.
              codec->status_ = Status::kComplete;

              for (std::function<void()>& callback : codec->listeners_) {
                  callback();
              }
          });

  // The encoded data is no longer needed now that it has been handed off
  // to the decoder.
  descriptor_ = nullptr;
  status_ = Status::kInProgress;

  return Dart_Null();
}

Dart_Handle SingleFrameCodec::getNextFrame(Dart_Handle callback_handle) {
  if (!Dart_IsClosure(callback_handle)) {
    return tonic::ToDart("Callback must be a function");
  }

  if (status_ == Status::kComplete) {
      tonic::DartInvoke(callback_handle,
                        {tonic::ToDart(cached_image_), tonic::ToDart(0)});
      return Dart_Null();
  }

  // already has cache in list
  auto it_find = CanvasImage::image_recorder.find(key_);
  if (it_find != CanvasImage::image_recorder.end()) {
      cached_image_ = fml::MakeRefCounted<CanvasImage>(it_find->second);
      status_ = Status::kComplete;
      tonic::DartInvoke(callback_handle,
                        {tonic::ToDart(cached_image_), tonic::ToDart(0)});
      return Dart_Null();
  }

  // This has to be valid because this method is called from Dart.
  auto dart_state = UIDartState::Current();

  pending_callbacks_.emplace_back(dart_state, callback_handle);

  if (status_ == Status::kInProgress) {
    // Another call to getNextFrame is in progress and will invoke the
    // pending callbacks when decoding completes.
    return Dart_Null();
  }

  if (real_codec != nullptr) {
      status_ = Status::kInProgress;
      return ((SingleFrameCodec *) real_codec)->innerDecodeImage([this]() {
          if (!cached_image_) {
              auto it_find = CanvasImage::image_recorder.find(key_);
              cached_image_ = fml::MakeRefCounted<CanvasImage>(it_find->second);
          }
          status_ = Status::kComplete;
          // Invoke any callbacks that were provided before the frame was decoded.
          for (const DartPersistentValue &callback : pending_callbacks_) {
              auto state = callback.dart_state().lock();
              if (!state) {
                  // This is probably because the isolate has been terminated before the
                  // image could be decoded.
                  continue;
              }
              tonic::DartState::Scope scope(state.get());

              tonic::DartInvoke(
                      callback.value(),
                      {tonic::ToDart(cached_image_), tonic::ToDart(0)});
          }
          pending_callbacks_.clear();
      });
  } else {
      return innerDecodeImage([this]() {
          for (const DartPersistentValue &callback : pending_callbacks_) {
              auto state = callback.dart_state().lock();
              if (!state) {
                  continue;
              }
              tonic::DartState::Scope scope(state.get());
              tonic::DartInvoke(
                      callback.value(),
                      {tonic::ToDart(cached_image_), tonic::ToDart(0)});
          }
          pending_callbacks_.clear();
      });
  }
}

size_t SingleFrameCodec::GetAllocationSize() const {
  if (real_codec != nullptr) {
      return real_codec->GetAllocationSize();
  }
  if (allocation_size_ >= 0) {
      return allocation_size_;
  }
  const auto& data_size = descriptor_->GetAllocationSize();
  const auto frame_byte_size =
      cached_image_ ? cached_image_->GetAllocationSize() : 0;
  return data_size + frame_byte_size + sizeof(this);
}

}  // namespace flutter
