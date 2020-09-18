// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "flutter/fml/macros.h"
#include "flutter/lib/ui/painting/codec.h"

namespace flutter {

class NativeCodec : public Codec {
 public:
  NativeCodec(std::unique_ptr<NativeExportCodec> codec);

  ~NativeCodec() override;

  // |Codec|
  int frameCount() const override;

  // |Codec|
  int repetitionCount() const override;

  // |Codec|
  Dart_Handle getNextFrame(Dart_Handle args) override;

  static int nativeCodecCount;

  static int nativeCodecIndex;

 private:
  // Captures the state shared between the IO and UI task runners.
  //
  // The state is initialized on the UI task runner when the Dart object is
  // created. Decoding occurs on the IO task runner. Since it is possible for
  // the UI object to be collected independently of the IO task runner work,
  // it is not safe for this state to live directly on the NativeCodec.
  // Instead, the NativeCodec creates this object when it is constructed,
  // shares it with the IO task runner's decoding work, and sets the live_
  // member to false when it is destructed.
  struct State {
    State(std::unique_ptr<NativeExportCodec> codec);

    const std::shared_ptr<NativeExportCodec> codec_;
    const int frameCount_;
    const int repetitionCount_;

    // The non-const members and functions below here are only read or written
    // to on the IO thread. They are not safe to access or write on the UI
    // thread.
    int nextFrameIndex_;
    // The last decoded frame that's required to decode any subsequent frames.
    std::unique_ptr<SkBitmap> lastRequiredFrame_;

    // The index of the last decoded required frame.
    int lastRequiredFrameIndex_ = -1;


    void GetNextFrameAndInvokeCallback(
        std::unique_ptr<DartPersistentValue> callback,
        fml::RefPtr<fml::TaskRunner> ui_task_runner,
        fml::WeakPtr<GrContext> resourceContext,
        fml::RefPtr<flutter::SkiaUnrefQueue> unref_queue,
        size_t trace_id,
        sk_sp<SkImage> skImage,
        int duration);
  };

  // Shared across the UI and IO task runners.
  std::shared_ptr<State> state_;

  FML_FRIEND_MAKE_REF_COUNTED(NativeCodec);
  FML_FRIEND_REF_COUNTED_THREAD_SAFE(NativeCodec);
};

}  // namespace flutter
