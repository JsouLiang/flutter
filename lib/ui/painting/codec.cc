// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/codec.h"

#include "third_party/tonic/dart_binding_macros.h"
#include "third_party/tonic/dart_library_natives.h"
#include "third_party/tonic/dart_state.h"
#include "third_party/tonic/logging/dart_invoke.h"
#include "third_party/tonic/typed_data/typed_list.h"

//BD: ADD @tanhaiyang@bytedance.com
#include "flutter/fml/make_copyable.h"
#include "flutter/lib/ui/painting/native_codec.h"
#include "flutter/lib/ui/painting/image.h"

using tonic::DartInvoke;
using tonic::DartPersistentValue;
using tonic::ToDart;

namespace flutter {

// BD ADD:
static constexpr const char* kGetNativeImageTraceTag FML_ALLOW_UNUSED_TYPE = "GetNativeImage";

IMPLEMENT_WRAPPERTYPEINFO(ui, Codec);

#define FOR_EACH_BINDING(V) \
  V(Codec, getNextFrame)    \
  V(Codec, frameCount)      \
  V(Codec, repetitionCount) \
  V(Codec, dispose)

FOR_EACH_BINDING(DART_NATIVE_CALLBACK)

void Codec::dispose() {
  ClearDartWrapper();
}

/**
 * BD ADD:
 *
 */
static void InvokeGetNativeImageCallback(fml::RefPtr<CanvasImage> image,
                                         std::unique_ptr<DartPersistentValue> callback,
                                         size_t trace_id) {
    std::shared_ptr<tonic::DartState> dart_state = callback->dart_state().lock();
    if (!dart_state) {
        TRACE_FLOW_END("flutter", kGetNativeImageTraceTag, trace_id);
        return;
    }
    tonic::DartState::Scope scope(dart_state);
    if (!image) {
        DartInvoke(callback->value(), {Dart_Null()});
    } else {
        DartInvoke(callback->value(), {ToDart(image)});
    }
    TRACE_FLOW_END("flutter", kGetNativeImageTraceTag, trace_id);
}
  
/**
 * BD ADD:
 *
 */
static void InvokeGetNativeInitCodecCallback(
        fml::RefPtr<NativeCodec> codec,
        std::unique_ptr<DartPersistentValue> callback,
        size_t trace_id) {
  std::shared_ptr<tonic::DartState> dart_state = callback->dart_state().lock();
  if (!dart_state) {
    TRACE_FLOW_END("flutter", kGetNativeImageTraceTag, trace_id);
    return;
  }
  tonic::DartState::Scope scope(dart_state);
  if (!codec) {
    DartInvoke(callback->value(), {Dart_Null()});
  } else {
    DartInvoke(callback->value(), {ToDart(codec)});
  }
  TRACE_FLOW_END("flutter", kGetNativeImageTraceTag, trace_id);
}

/**
 * BD ADD:
 *
 */
void GetNativeImage(Dart_NativeArguments args) {
  static size_t trace_counter = 1;
  const size_t trace_id = trace_counter++;
  TRACE_FLOW_BEGIN("flutter", kGetNativeImageTraceTag, trace_id);
    
  Dart_Handle callback_handle = Dart_GetNativeArgument(args, 1);
  if (!Dart_IsClosure(callback_handle)) {
    TRACE_FLOW_END("flutter", kGetNativeImageTraceTag, trace_id);
    Dart_SetReturnValue(args, ToDart("Callback must be a function"));
    return;
  }
    
  Dart_Handle exception = nullptr;
  const std::string url = tonic::DartConverter<std::string>::FromArguments(args, 0, exception);
  if (exception) {
    TRACE_FLOW_END("flutter", kGetNativeImageTraceTag, trace_id);
    Dart_SetReturnValue(args, exception);
    return;
  }
  
  const int width = tonic::DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 2));
  const int height = tonic::DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 3));
  const float scale = tonic::DartConverter<float>::FromDart(Dart_GetNativeArgument(args, 4));
    
  auto* dart_state = UIDartState::Current();

  const auto& task_runners = dart_state->GetTaskRunners();
  fml::WeakPtr<IOManager> io_manager = dart_state->GetIOManager();
  task_runners.GetIOTaskRunner()->PostTask(
      fml::MakeCopyable([io_manager,
                         url,
                         width,
                         height,
                         scale,
                         task_runners,
                         ui_task_runner = task_runners.GetUITaskRunner(),
                         queue = UIDartState::Current()->GetSkiaUnrefQueue(),
                         callback = std::make_unique<DartPersistentValue>(
                         tonic::DartState::Current(), callback_handle),
                         trace_id]() mutable {
        std::shared_ptr<flutter::ImageLoader> imageLoader = io_manager.get()->GetImageLoader();
        ImageLoaderContext contextPtr = ImageLoaderContext(task_runners, io_manager->GetResourceContext());
        
        if (!imageLoader) {
          FML_LOG(ERROR) << "ImageLoader is Null!";
          
          ui_task_runner->PostTask(
              fml::MakeCopyable([callback = std::move(callback),
                                 trace_id]() mutable {
                InvokeGetNativeImageCallback(nullptr, std::move(callback),
                                             trace_id);
              }));
          return;
        }
      
        imageLoader->Load(
            url, width, height, scale, contextPtr,
            fml::MakeCopyable([ui_task_runner,
                               queue,
                               callback = std::move(callback),
                               trace_id](sk_sp<SkImage> skimage) mutable {
              fml::RefPtr<CanvasImage> image;
              if (skimage) {
                image = CanvasImage::Create();
                image->setMips(false);
                image->set_image({skimage, queue});
              } else {
                image = nullptr;
              }
              ui_task_runner->PostTask(
                  fml::MakeCopyable([callback = std::move(callback),
                                        image = std::move(image), trace_id]() mutable {
                    InvokeGetNativeImageCallback(image, std::move(callback),
                                                 trace_id);
                  }));
            }));
      }));
}

static void InstantiateNativeImageCodec(Dart_NativeArguments args) {
  static size_t trace_counter = 1;
  const size_t trace_id = trace_counter++;
  TRACE_FLOW_BEGIN("flutter", kGetNativeImageTraceTag, trace_id);

  Dart_Handle callback_handle = Dart_GetNativeArgument(args, 1);
  if (!Dart_IsClosure(callback_handle)) {
    TRACE_FLOW_END("flutter", kGetNativeImageTraceTag, trace_id);
    Dart_SetReturnValue(args, ToDart("Callback must be a function"));
    return;
  }

  Dart_Handle exception = nullptr;
  const std::string url =
          tonic::DartConverter<std::string>::FromArguments(args, 0, exception);
  if (exception) {
    TRACE_FLOW_END("flutter", kGetNativeImageTraceTag, trace_id);
    Dart_SetReturnValue(args, exception);
    return;
  }

  const int width = tonic::DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 2));
  const int height = tonic::DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 3));
  const float scale = tonic::DartConverter<float>::FromDart(Dart_GetNativeArgument(args, 4));

  auto* dart_state = UIDartState::Current();
  const auto& task_runners = dart_state->GetTaskRunners();
  task_runners.GetIOTaskRunner()->PostTask(fml::MakeCopyable(
    [url, width, height, scale, trace_id, task_runners,
      ui_task_runner = task_runners.GetUITaskRunner(),
      io_manager = dart_state->GetIOManager(),
      callback = std::make_unique<DartPersistentValue>(dart_state, callback_handle)]() mutable {
      ImageLoaderContext contextPtr = ImageLoaderContext(task_runners, io_manager->GetResourceContext());
      if (!task_runners.IsValid()) {
        return;
      }
      // load codec
      io_manager->GetImageLoader()->LoadCodec(
        url, width, height, scale, contextPtr,
        fml::MakeCopyable([ui_task_runner,
                            callback = std::move(callback),
                            trace_id](std::unique_ptr<NativeExportCodec> codec) mutable {
          // callback to ui task
          ui_task_runner->PostTask(
            fml::MakeCopyable(
              [callback = std::move(callback),
                codec = std::move(codec), trace_id]() mutable {
                fml::RefPtr<NativeCodec> ui_codec = nullptr;
                if (codec) {
                  ui_codec = fml::MakeRefCounted<NativeCodec>(std::move(codec));
                }
                InvokeGetNativeInitCodecCallback(ui_codec, std::move(callback), trace_id);
              }));
        }));
    }));
}
// END

void Codec::RegisterNatives(tonic::DartLibraryNatives* natives) {
  // BD ADD: START
  natives->Register({
      {"getNativeImage", GetNativeImage, 5, true},
  });

  natives->Register({
    {"instantiateNativeImageCodec", InstantiateNativeImageCodec, 5, true},
    });
  // END
  natives->Register({FOR_EACH_BINDING(DART_REGISTER_NATIVE)});
}

}  // namespace flutter
