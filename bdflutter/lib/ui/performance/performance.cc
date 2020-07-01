// BD ADD

#include "performance.h"
#include "flutter/flow/instrumentation.h"
#include "flutter/lib/ui/window/window.h"
#include "flutter/lib/ui/ui_dart_state.h"
#include "third_party/dart/runtime/include/dart_api.h"
#include "third_party/tonic/converter/dart_converter.h"
#include "third_party/tonic/dart_args.h"
#include "third_party/tonic/dart_library_natives.h"
#include "third_party/tonic/logging/dart_invoke.h"
#include "bdflutter/common/fps_recorder.h"
#include "boost.h"

using tonic::DartConverter;
using tonic::ToDart;

namespace flutter {

Performance::Performance():
    dart_image_memory_usage(0) {}

int64_t Performance::GetImageMemoryUsageKB() {
  int64_t sizeInKB = dart_image_memory_usage.load(std::memory_order_relaxed);
  return  sizeInKB > 0 ? sizeInKB : 0;
}

void Performance::SubImageMemoryUsage(int64_t sizeInKB) {
  dart_image_memory_usage.fetch_sub(sizeInKB, std::memory_order_relaxed);
}

void Performance::AddImageMemoryUsage(int64_t sizeInKB) {
  dart_image_memory_usage.fetch_add(sizeInKB, std::memory_order_relaxed);
}

void Performance::DisableMips(bool disable) {
  disable_mipmaps_ = disable;
}

bool Performance::IsDisableMips(){
  return disable_mipmaps_ ;
}

void Performance_imageMemoryUsage(Dart_NativeArguments args) {
  Dart_SetReturnValue(args, tonic::DartConverter<int64_t>::ToDart(
      Performance::GetInstance()->GetImageMemoryUsageKB()));
}

void Performance_getEngineMainEnterMicros(Dart_NativeArguments args) {
  Dart_SetIntegerReturnValue(args,
                             UIDartState::Current()->window()->client()->GetEngineMainEnterMicros());
}

void AddNextFrameCallback(Dart_Handle callback) {
  UIDartState* dart_state = UIDartState::Current();
  if (!dart_state->window()) {
    return;
  }

  tonic::DartPersistentValue* next_frame_callback =
      new tonic::DartPersistentValue(dart_state, callback);
  dart_state->window()->client()->AddNextFrameCallback(
      [next_frame_callback]() mutable {
        std::shared_ptr<tonic::DartState> dart_state_ =
            next_frame_callback->dart_state().lock();
        if (!dart_state_) {
          return;
        }
        tonic::DartState::Scope scope(dart_state_);
        tonic::DartInvokeVoid(next_frame_callback->value());

        // next_frame_callback is associated with the Dart isolate and must be
        // deleted on the UI thread
        delete next_frame_callback;
      });
}

void Performance_addNextFrameCallback(Dart_NativeArguments args) {
  tonic::DartCall(&AddNextFrameCallback, args);
}

void Performance_startRecordFps(Dart_NativeArguments args) {
  Dart_Handle exception = nullptr;
  std::string key = tonic::DartConverter<std::string>::FromArguments(args, 1, exception);
  if (exception) {
    Dart_ThrowException(exception);
    return;
  }
  FpsRecorder::Current()->StartRecordFps(key);
}

void Performance_obtainFps(Dart_NativeArguments args) {
  Dart_Handle exception = nullptr;
  std::string key = tonic::DartConverter<std::string>::FromArguments(args, 1, exception);
  if (exception) {
    Dart_ThrowException(exception);
    return;
  }
  bool stop_record = tonic::DartConverter<bool>::FromArguments(args, 2, exception);
  if (exception) {
    Dart_ThrowException(exception);
    return;
  }
  std::vector<double> fpsInfo = FpsRecorder::Current()->ObtainFpsData(key, stop_record);
  int size = 3;
  Dart_Handle data_handle = Dart_NewList(size);
  for (int i = 0; i != size; i++) {
    Dart_Handle value = Dart_NewDouble(fpsInfo[i]);
    Dart_ListSetAt(data_handle, i, value);
  }
  Dart_SetReturnValue(args, data_handle);
}

void Performance_getMaxSamples(Dart_NativeArguments args) {
  int max_samples = Stopwatch::GetMaxSamples();
  Dart_SetIntegerReturnValue(args, max_samples);
}

void performance_getFps(Dart_NativeArguments args) {
  Dart_Handle exception = nullptr;
  int thread_type =
      tonic::DartConverter<int>::FromArguments(args, 1, exception);
  int fps_type = tonic::DartConverter<int>::FromArguments(args, 2, exception);
  bool do_clear = tonic::DartConverter<bool>::FromArguments(args, 3, exception);
  std::vector<double> fpsInfo = UIDartState::Current()->window()->client()->GetFps(
      thread_type, fps_type, do_clear);
  Dart_Handle data_handle = Dart_NewList(fpsInfo.size());
  for(std::vector<int>::size_type i = 0; i != fpsInfo.size(); i++) {
    Dart_Handle value = Dart_NewDouble(fpsInfo[i]);
    Dart_ListSetAt(data_handle, i, value);
  }
  Dart_SetReturnValue(args, data_handle);
}

void Performance_startBoost(Dart_NativeArguments args) {
  uint16_t flags = static_cast<uint16_t>(tonic::DartConverter<int>::FromDart(
      Dart_GetNativeArgument(args, 1)));
  int millis = tonic::DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 2));
  Boost::Current()->StartUp(flags, millis);
}

void Performance_finishBoost(Dart_NativeArguments args) {
  uint16_t flags = static_cast<uint16_t>(tonic::DartConverter<int>::FromDart(
      Dart_GetNativeArgument(args, 1)));
  Boost::Current()->Finish(flags);
}

void Performance_forceGC(Dart_NativeArguments args) {
  Boost::Current()->ForceGC();
}

void Performance_disableMips(Dart_NativeArguments args) {
  bool disable = DartConverter<bool>::FromDart(Dart_GetNativeArgument(args, 1));
  Performance::GetInstance()->DisableMips(disable);
}

void Performance_startStackTraceSamples(Dart_NativeArguments args) {
    Dart_StartProfiling2();
}

void Performance_stopStackTraceSamples(Dart_NativeArguments args) {
   Dart_StopProfiling2();
}

void Performance_getStackTraceSamples(Dart_NativeArguments args) {
    int64_t microseconds = (int64_t)DartConverter<int64_t>::FromDart(Dart_GetNativeArgument(args, 0));
    Dart_Handle res = Dart_GetStackSamples(microseconds);
    Dart_SetReturnValue(args, res);
}

void Performance_requestHeapSnapshot(Dart_NativeArguments args) {
    const char* filePath = nullptr;
    Dart_StringToCString(Dart_GetNativeArgument(args, 0), &filePath);
    Dart_Handle res = Dart_RequestSnapshot(filePath);
    Dart_SetReturnValue(args, res);
}

void Performance::RegisterNatives(tonic::DartLibraryNatives* natives) {
  natives->Register({
      {"Performance_imageMemoryUsage", Performance_imageMemoryUsage, 1, true},
      {"Performance_getEngineMainEnterMicros", Performance_getEngineMainEnterMicros, 1, true},
      {"Performance_addNextFrameCallback", Performance_addNextFrameCallback, 2, true},
      {"Performance_startRecordFps", Performance_startRecordFps, 2, true},
      {"Performance_obtainFps", Performance_obtainFps, 3, true},
      {"Performance_getMaxSamples", Performance_getMaxSamples, 1, true},
      {"performance_getFps", performance_getFps, 4, true},
      {"Performance_startBoost", Performance_startBoost, 3, true},
      {"Performance_finishBoost", Performance_finishBoost, 2, true},
      {"Performance_forceGC", Performance_forceGC, 1, true},
      {"Performance_disableMips", Performance_disableMips, 2, true},
      {"Performance_startStackTraceSamples", Performance_startStackTraceSamples, 1, true},
      {"Performance_stopStackTraceSamples", Performance_stopStackTraceSamples, 1, true},
      {"Performance_getStackTraceSamples", Performance_getStackTraceSamples, 2, true},
      {"Performance_requestHeapSnapshot", Performance_requestHeapSnapshot, 2, true},
  });
}

}
