// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/codec.h"

#include "third_party/tonic/dart_binding_macros.h"
#include "third_party/tonic/dart_library_natives.h"
#include "third_party/tonic/dart_state.h"
#include "third_party/tonic/logging/dart_invoke.h"
#include "third_party/tonic/typed_data/typed_list.h"

//BD ADD: START @tanhaiyang@bytedance.com
#include "flutter/fml/make_copyable.h"
#include "flutter/lib/ui/painting/native_codec.h"
#include "flutter/lib/ui/painting/image.h"
#include "multi_frame_codec.h"
#include "single_frame_codec.h"
// END

using tonic::DartInvoke;
using tonic::DartPersistentValue;
using tonic::ToDart;

namespace flutter {

// BD ADD: START
static constexpr const char* kGetNativeImageTraceTag FML_ALLOW_UNUSED_TYPE = "GetNativeImage";

std::map<std::string, Codec *> Codec::codec_recorder;
std::vector<std::pair<std::string, int>> Codec::cached_images;
std::vector<DartPersistentValue> Codec::dart_remove_cache_callbacks;
std::vector<DartPersistentValue> Codec::dart_get_keys_callbacks;
int Codec::max_cache_size = 100 * 1024 * 1024;
// END

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

// BD ADD: START
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

/**
 * 查找是否已有缓存的Codec，已有则给Dart返回缓存，否则返回null
 * @param args
 */
static void GetCachedImageCodec(Dart_NativeArguments args) {
    Dart_Handle callback_handle = Dart_GetNativeArgument(args, 1);
    if (!Dart_IsClosure(callback_handle)) {
        Dart_SetReturnValue(args, tonic::ToDart("Callback must be a function"));
        return;
    }
    std::string key = tonic::DartConverter<std::string>::FromDart(Dart_GetNativeArgument(args, 0));
    if (key.empty()) {
        DartInvoke(callback_handle, {Dart_Null()});
        return;
    }
    auto it_find = Codec::codec_recorder.find(key);
    if (it_find != Codec::codec_recorder.end()) {
        Codec *codec = it_find->second;
        fml::RefPtr<Codec> ui_codec;
        if (codec->getClassType() == CodecType::SingleFrame) {
            ui_codec = fml::MakeRefCounted<SingleFrameCodec>(codec, key);
        } else if (codec->getClassType() == CodecType::MultiFrame) {
            ui_codec = fml::MakeRefCounted<MultiFrameCodec>(codec);
        }
        DartInvoke(callback_handle, {ToDart(ui_codec)});
    } else {
        DartInvoke(callback_handle, {Dart_Null()});
    }
}

/**
 * 记录Dart侧的回调
 * dart_remove_cache_callbacks：通知Dart侧移除相应Key的Cache
 * dart_get_keys_callbacks：获取Dart目前所有的ImageCache的Key
 * @param args
 */
static void RegisterImageCacheCallBack(Dart_NativeArguments args) {
    auto dart_state = UIDartState::Current();
    Dart_Handle remove_callback_handle = Dart_GetNativeArgument(args, 0);
    Dart_Handle get_key_call_handle = Dart_GetNativeArgument(args, 1);
    Codec::dart_remove_cache_callbacks.emplace_back(dart_state, remove_callback_handle);
    Codec::dart_get_keys_callbacks.emplace_back(dart_state, get_key_call_handle);
}

static void NotifyRemoveCache(const std::vector<std::string>& keys) {
  for (auto &item :Codec::dart_remove_cache_callbacks) {
      auto state = item.dart_state().lock();
      if (!state) {
          continue;
      }
      tonic::DartState::Scope scope(state.get());
      tonic::DartInvoke(item.value(), {tonic::ToDart(keys)});
  }
}

static bool CacheContainsKey(std::string &key) {
    for (auto iter = Codec::cached_images.begin(); iter != Codec::cached_images.end(); iter++) {
        if ((*iter).first == key) {
            std::string cacheKey = (*iter).first;
            int cacheSize = (*iter).second;
            // 缓存命中,item 放到队尾，执行LRU策略
            Codec::cached_images.erase(iter);
            Codec::cached_images.emplace_back(cacheKey,cacheSize);
            return true;
        }
    }
    return false;
}

static void CheckCacheSize() {
    // 修正Key（read from dart）
    std::vector<std::string> allKeys;
    for (auto &item :Codec::dart_get_keys_callbacks) {
        auto state = item.dart_state().lock();
        if (!state) {
            continue;
        }
        tonic::DartState::Scope scope(state.get());
        Dart_Handle result = tonic::DartInvoke(item.value(), {});
        auto keys = tonic::DartConverter<std::vector<std::string>>::FromDart(result);
        allKeys.insert(allKeys.end(), keys.begin(), keys.end());
    }
    auto iter = Codec::cached_images.begin();
    int totalSize = 0;
    while (iter != Codec::cached_images.end()) {
        if (std::find(allKeys.begin(), allKeys.end(), (*iter).first) == allKeys.end()) {
            // noEngine Use
            iter = Codec::cached_images.erase(iter);
        } else {
            totalSize += (*iter).second;
            iter++;
        }
    }
    // checkSize->NotifyRemove
    if (totalSize > Codec::max_cache_size) {
        iter = Codec::cached_images.begin();
        std::vector<std::string> toRemove;
        while (totalSize > Codec::max_cache_size && iter!=Codec::cached_images.end()){
            toRemove.push_back((*iter).first);
            totalSize-=(*iter).second;
            iter = Codec::cached_images.erase(iter);
        }
        NotifyRemoveCache(toRemove);
    }
}

/**
 * Dart侧ImageCacheSize发生变动
 * @param args
 */
static void SetImageCacheSize(Dart_NativeArguments args) {
    Codec::max_cache_size = tonic::DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 0));
    if (Codec::max_cache_size == 0) {
        std::vector<std::string> toRemove;
        for (const auto &item:Codec::cached_images) {
            toRemove.push_back(item.first);
        }
        NotifyRemoveCache(toRemove);
        Codec::cached_images.clear();
    } else {
        CheckCacheSize();
    }
}

static void NotifyUseImage(Dart_NativeArguments args) {
    std::string key = tonic::DartConverter<std::string>::FromDart(Dart_GetNativeArgument(args, 0));
    int size = tonic::DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 1));
    if (!CacheContainsKey(key)) {
        Codec::cached_images.emplace_back(key, size);
        CheckCacheSize();
    }
}
// END

void Codec::RegisterNatives(tonic::DartLibraryNatives *natives) {
  // BD ADD: START
  natives->Register({
      {"getCachedImageCodec",               GetCachedImageCodec,               2, true},
      {"registerImageCacheCallBack",        RegisterImageCacheCallBack,        2, true},
      {"setImageCacheSize",                 SetImageCacheSize,                 1, true},
      {"notifyUseImage",                    NotifyUseImage,                    2, true},
      {"getNativeImage",                    GetNativeImage,                    5, true},
      {"instantiateNativeImageCodec",       InstantiateNativeImageCodec,       5, true},
    });
  // END
  natives->Register({FOR_EACH_BINDING(DART_REGISTER_NATIVE)});
}

}  // namespace flutter
