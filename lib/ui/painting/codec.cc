// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/codec.h"

#include <variant>

#include "flutter/common/task_runners.h"
#include "flutter/fml/logging.h"
#include "flutter/fml/make_copyable.h"
#include "flutter/fml/trace_event.h"
#include "flutter/lib/ui/painting/frame_info.h"
#include "flutter/lib/ui/painting/multi_frame_codec.h"
#include "flutter/lib/ui/painting/single_frame_codec.h"
#include "flutter/lib/ui/painting/native_codec.h"
#include "third_party/skia/include/codec/SkCodec.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/tonic/dart_binding_macros.h"
#include "third_party/tonic/dart_library_natives.h"
#include "third_party/tonic/dart_state.h"
#include "third_party/tonic/logging/dart_invoke.h"
#include "third_party/tonic/typed_data/typed_list.h"

#if OS_ANDROID
#include <sys/mman.h>
#endif

using tonic::DartInvoke;
using tonic::DartPersistentValue;
using tonic::ToDart;

namespace flutter {

namespace {

// BD ADD: LinYiyi
static constexpr const char* kGetNativeImageTraceTag FML_ALLOW_UNUSED_TYPE = "GetNativeImage";
// END

// This must be kept in sync with the enum in painting.dart
enum PixelFormat {
  kRGBA8888,
  kBGRA8888,
};

#if OS_ANDROID

// Compressed image buffers are allocated on the UI thread but are deleted on a
// decoder worker thread.  Android's implementation of malloc appears to
// continue growing the native heap size when the allocating thread is
// different from the freeing thread.  To work around this, create an SkData
// backed by an anonymous mapping.
sk_sp<SkData> MakeSkDataWithCopy(const void* data, size_t length) {
  if (length == 0) {
    return SkData::MakeEmpty();
  }

  size_t mapping_length = length + sizeof(size_t);
  void* mapping = ::mmap(nullptr, mapping_length, PROT_READ | PROT_WRITE,
                         MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  if (mapping == MAP_FAILED) {
    return SkData::MakeEmpty();
  }

  *reinterpret_cast<size_t*>(mapping) = mapping_length;
  void* mapping_data = reinterpret_cast<char*>(mapping) + sizeof(size_t);
  ::memcpy(mapping_data, data, length);

  SkData::ReleaseProc proc = [](const void* ptr, void* context) {
    size_t* size_ptr = reinterpret_cast<size_t*>(context);
    FML_DCHECK(ptr == size_ptr + 1);
    if (::munmap(const_cast<void*>(context), *size_ptr) == -1) {
      FML_LOG(ERROR) << "munmap of codec SkData failed";
    }
  };

  return SkData::MakeWithProc(mapping_data, length, proc, mapping);
}

#else

sk_sp<SkData> MakeSkDataWithCopy(const void* data, size_t length) {
  return SkData::MakeWithCopy(data, length);
}

#endif  // OS_ANDROID

}  // anonymous namespace

static std::variant<ImageDecoder::ImageInfo, std::string> ConvertImageInfo(
    Dart_Handle image_info_handle,
    Dart_NativeArguments args) {
  Dart_Handle width_handle = Dart_GetField(image_info_handle, ToDart("width"));
  if (!Dart_IsInteger(width_handle)) {
    return "ImageInfo.width must be an integer";
  }
  Dart_Handle height_handle =
      Dart_GetField(image_info_handle, ToDart("height"));
  if (!Dart_IsInteger(height_handle)) {
    return "ImageInfo.height must be an integer";
  }
  Dart_Handle format_handle =
      Dart_GetField(image_info_handle, ToDart("format"));
  if (!Dart_IsInteger(format_handle)) {
    return "ImageInfo.format must be an integer";
  }
  Dart_Handle row_bytes_handle =
      Dart_GetField(image_info_handle, ToDart("rowBytes"));
  if (!Dart_IsInteger(row_bytes_handle)) {
    return "ImageInfo.rowBytes must be an integer";
  }

  PixelFormat pixel_format = static_cast<PixelFormat>(
      tonic::DartConverter<int>::FromDart(format_handle));
  SkColorType color_type = kUnknown_SkColorType;
  switch (pixel_format) {
    case kRGBA8888:
      color_type = kRGBA_8888_SkColorType;
      break;
    case kBGRA8888:
      color_type = kBGRA_8888_SkColorType;
      break;
  }
  if (color_type == kUnknown_SkColorType) {
    return "Invalid pixel format";
  }

  int width = tonic::DartConverter<int>::FromDart(width_handle);
  if (width <= 0) {
    return "width must be greater than zero";
  }
  int height = tonic::DartConverter<int>::FromDart(height_handle);
  if (height <= 0) {
    return "height must be greater than zero";
  }

  ImageDecoder::ImageInfo image_info;
  image_info.sk_info =
      SkImageInfo::Make(width, height, color_type, kPremul_SkAlphaType);
  image_info.row_bytes =
      tonic::DartConverter<size_t>::FromDart(row_bytes_handle);

  if (image_info.row_bytes < image_info.sk_info.minRowBytes()) {
    return "rowBytes does not match the width of the image";
  }

  return image_info;
}

static void InstantiateImageCodec(Dart_NativeArguments args) {
  Dart_Handle callback_handle = Dart_GetNativeArgument(args, 1);
  if (!Dart_IsClosure(callback_handle)) {
    Dart_SetReturnValue(args, tonic::ToDart("Callback must be a function"));
    return;
  }

  Dart_Handle image_info_handle = Dart_GetNativeArgument(args, 2);

  std::optional<ImageDecoder::ImageInfo> image_info;

  if (!Dart_IsNull(image_info_handle)) {
    auto image_info_results = ConvertImageInfo(image_info_handle, args);
    if (auto value =
            std::get_if<ImageDecoder::ImageInfo>(&image_info_results)) {
      image_info = *value;
    } else if (auto error = std::get_if<std::string>(&image_info_results)) {
      Dart_SetReturnValue(args, tonic::ToDart(*error));
      return;
    }
  }

  sk_sp<SkData> buffer;

  {
    Dart_Handle exception = nullptr;
    tonic::Uint8List list =
        tonic::DartConverter<tonic::Uint8List>::FromArguments(args, 0,
                                                              exception);
    if (exception) {
      Dart_SetReturnValue(args, exception);
      return;
    }
    buffer = MakeSkDataWithCopy(list.data(), list.num_elements());
  }

  if (image_info) {
    const auto expected_size =
        image_info->row_bytes * image_info->sk_info.height();
    if (buffer->size() < expected_size) {
      Dart_SetReturnValue(
          args, ToDart("Pixel buffer size does not match image size"));
      return;
    }
  }

  const int targetWidth =
      tonic::DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 3));
  const int targetHeight =
      tonic::DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 4));

  std::unique_ptr<SkCodec> codec;
  bool single_frame;
  if (image_info) {
    single_frame = true;
  } else {
    codec = SkCodec::MakeFromData(buffer);
    if (!codec) {
      Dart_SetReturnValue(args, ToDart("Could not instantiate image codec."));
      return;
    }
    single_frame = codec->getFrameCount() == 1;
  }

  fml::RefPtr<Codec> ui_codec;

  if (single_frame) {
    ImageDecoder::ImageDescriptor descriptor;
    descriptor.decompressed_image_info = image_info;

    if (targetWidth > 0) {
      descriptor.target_width = targetWidth;
    }
    if (targetHeight > 0) {
      descriptor.target_height = targetHeight;
    }
    descriptor.data = std::move(buffer);

    ui_codec = fml::MakeRefCounted<SingleFrameCodec>(std::move(descriptor));
  } else {
    ui_codec = fml::MakeRefCounted<MultiFrameCodec>(std::move(codec));
  }

  tonic::DartInvoke(callback_handle, {ToDart(ui_codec)});
}

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
static void InvokeGetNativeImageCallback(
    fml::RefPtr<CanvasImage> image,
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
 * BD ADD: LinYiyi
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
      std::shared_ptr<flutter::ImageLoader> imageLoader = io_manager.get()->GetImageLoader();
      if (!imageLoader) {
        FML_LOG(ERROR) << "ImageLoader is Null";

        ui_task_runner->PostTask(
            fml::MakeCopyable([callback = std::move(callback),
                                  trace_id]() mutable {
              InvokeGetNativeInitCodecCallback(nullptr, std::move(callback),
                                           trace_id);
            }));
        return;
      }
      // load codec
      imageLoader->LoadCodec(
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
  // BD ADD: LinYiyi
  natives->Register({
      {"getNativeImage", GetNativeImage, 5, true},
  });

  natives->Register({
    {"instantiateNativeImageCodec", InstantiateNativeImageCodec, 5, true},
    });
  // END
  natives->Register({
      {"instantiateImageCodec", InstantiateImageCodec, 5, true},
  });
  natives->Register({FOR_EACH_BINDING(DART_REGISTER_NATIVE)});
}

}  // namespace flutter
