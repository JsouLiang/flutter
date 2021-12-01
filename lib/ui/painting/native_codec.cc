// BD: ADD START
// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/native_codec.h"

#include "flutter/fml/make_copyable.h"
#include "third_party/dart/runtime/include/dart_api.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/tonic/logging/dart_invoke.h"
#include "flutter/lib/ui/painting/codec.h"
#include "flutter/lib/ui/painting/image.h"

namespace flutter {
int NativeCodec::nativeCodecCount = 0;
int NativeCodec::nativeCodecIndex = 0;
NativeCodec::NativeCodec(std::unique_ptr<NativeExportCodec> codec)
    : state_(new State(std::move(codec))) {}

NativeCodec::~NativeCodec() = default;

NativeCodec::State::State(std::unique_ptr<NativeExportCodec> codec)
    : codec_(std::move(codec)),
      frameCount_(codec_->frameCount()),
      repetitionCount_(codec_->repetitionCount()),
      nextFrameIndex_(0) {}

static void InvokeNextFrameCallback(
    fml::RefPtr<CanvasImage> image,
    int duration,
  std::unique_ptr<DartPersistentValue> callback,
  size_t trace_id)
  {
    std::shared_ptr<tonic::DartState> dart_state = callback->dart_state().lock();
    if (!dart_state) {
      FML_DLOG(ERROR) << "Could not acquire Dart state while attempting to fire "
                         "next frame callback.";
      return;
    }
    tonic::DartState::Scope scope(dart_state);
    tonic::DartInvoke(callback->value(),
                      {tonic::ToDart(image), tonic::ToDart(duration)});
}

void NativeCodec::State::GetNextFrameAndInvokeCallback(
    std::unique_ptr<DartPersistentValue> callback,
    fml::RefPtr<fml::TaskRunner> ui_task_runner,
    fml::WeakPtr<GrDirectContext> resourceContext,
    fml::RefPtr<flutter::SkiaUnrefQueue> unref_queue,
    size_t trace_id,
    sk_sp<SkImage> skImage,
    int duration) {
  fml::RefPtr<CanvasImage> image = nullptr;

  if (skImage) {
    image = CanvasImage::Create();
    image->set_image({skImage, unref_queue});
  }

  if (frameCount_ > 0) {
    nextFrameIndex_ = (nextFrameIndex_ + 1) % frameCount_;
  }

  ui_task_runner->PostTask(fml::MakeCopyable([callback = std::move(callback),
                                              image = std::move(image),
                                              duration, trace_id]() mutable {
    InvokeNextFrameCallback(std::move(image), duration, std::move(callback),
                            trace_id);
  }));
}

Dart_Handle NativeCodec::getNextFrame(Dart_Handle callback_handle) {
  static size_t trace_counter = 1;
  const size_t trace_id = trace_counter++;

  if (!Dart_IsClosure(callback_handle)) {
    return tonic::ToDart("Callback must be a function");
  }

  auto* dart_state = UIDartState::Current();

  const auto& task_runners = dart_state->GetTaskRunners();

  if (state_->codec_->frameCount() == -1) {
    task_runners.GetIOTaskRunner()->PostTask(fml::MakeCopyable(
      [callback = std::make_unique<DartPersistentValue>(
        tonic::DartState::Current(), callback_handle),
        weak_state = std::weak_ptr<NativeCodec::State>(state_), trace_id,
        ui_task_runner = task_runners.GetUITaskRunner(),
        io_manager = dart_state->GetIOManager()]() mutable {
        auto state = weak_state.lock();
        if (!state) {
          ui_task_runner->PostTask(fml::MakeCopyable(
            [callback = std::move(callback)]() { callback->Clear(); }));
          return;
        }
        int duration = state->codec_->duration(state->nextFrameIndex_);
        state->GetNextFrameAndInvokeCallback(
          std::move(callback), ui_task_runner,
          io_manager->GetResourceContext(), io_manager->GetSkiaUnrefQueue(),
          trace_id,
          state->codec_->skImage,
          duration);
      })
    );
  } else {
    task_runners.GetIOTaskRunner()->PostTask(fml::MakeCopyable(
      [dart_state, trace_id,
        io_manager = dart_state->GetIOManager(),
        weak_state = std::weak_ptr<NativeCodec::State>(state_),
        ui_task_runner = dart_state->GetTaskRunners().GetUITaskRunner(),
        task_runners = dart_state->GetTaskRunners(),
        callback = std::make_unique<DartPersistentValue>(tonic::DartState::Current(), callback_handle)
      ]() mutable {
        auto state = weak_state.lock();
        if (!state) {
          ui_task_runner->PostTask(fml::MakeCopyable(
            [callback = std::move(callback)]() { callback->Clear(); }));
          return;
        }
        ImageLoaderContext loaderContext = ImageLoaderContext(task_runners, io_manager->GetResourceContext());
        io_manager->GetImageLoader()->GetNextFrame(
          loaderContext,
          state->nextFrameIndex_,
          state->codec_,
          fml::MakeCopyable(
            [context = dart_state->GetResourceContext(),
              weak_state = std::move(weak_state), io_manager,
              ui_task_runner = dart_state->GetTaskRunners().GetUITaskRunner(),
              io_task_runner = dart_state->GetTaskRunners().GetIOTaskRunner(),
              queue = io_manager.get()->GetSkiaUnrefQueue(),
              callback = std::move(callback),
              duration = state->codec_->duration(state->nextFrameIndex_),
              trace_id](sk_sp<SkImage> skimage) mutable {
              auto state = weak_state.lock();
              if (!state) {
                ui_task_runner->PostTask(fml::MakeCopyable(
                  [callback = std::move(callback)]() { callback->Clear(); }));
                return;
              }
              fml::RefPtr<CanvasImage> image;
              if (skimage) {
                image = CanvasImage::Create();
                image->set_image({skimage, queue});
              } else {
                image = nullptr;
              }
              state->GetNextFrameAndInvokeCallback(
                std::move(callback), ui_task_runner,
                io_manager->GetResourceContext(), io_manager->GetSkiaUnrefQueue(),
                trace_id, skimage, duration);
            })
        );
      }));
  }
  return Dart_Null();
}

int NativeCodec::frameCount() const {
  return state_->frameCount_;
}

int NativeCodec::repetitionCount() const {
  return state_->repetitionCount_;
}
// EMD
}  // namespace flutter
