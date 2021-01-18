// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/dart_runtime_hooks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>
#include <sstream>

#include "flutter/common/settings.h"
#include "flutter/fml/build_config.h"
#include "flutter/fml/logging.h"
#include "flutter/lib/ui/plugins/callback_cache.h"
#include "flutter/lib/ui/ui_dart_state.h"
// BD ADD:
#include "flutter/lib/ui/performance.h"
#include "third_party/dart/runtime/include/bin/dart_io_api.h"
#include "third_party/dart/runtime/include/dart_api.h"
#include "third_party/dart/runtime/include/dart_tools_api.h"
#include "third_party/tonic/converter/dart_converter.h"
#include "third_party/tonic/dart_library_natives.h"
#include "third_party/tonic/dart_microtask_queue.h"
#include "third_party/tonic/dart_state.h"
#include "third_party/tonic/logging/dart_error.h"
#include "third_party/tonic/logging/dart_invoke.h"
#include "third_party/tonic/scopes/dart_api_scope.h"
#include "third_party/tonic/scopes/dart_isolate_scope.h"
// BD ADD:
#include "flutter/lib/ui/boost.h"
#include <flutter/fml/make_copyable.h>

#if defined(OS_ANDROID)
#include <android/log.h>
#elif defined(OS_IOS)
extern "C" {
// Cannot import the syslog.h header directly because of macro collision.
extern void syslog(int, const char*, ...);
}
#endif

using tonic::DartConverter;
using tonic::LogIfError;
using tonic::ToDart;

namespace flutter {

#define REGISTER_FUNCTION(name, count) {"" #name, name, count, true},
#define DECLARE_FUNCTION(name, count) \
  extern void name(Dart_NativeArguments args);

#define BUILTIN_NATIVE_LIST(V)  \
  V(Logger_PrintString, 1)      \
  V(Logger_PrintDebugString, 1) \
  V(SaveCompilationTrace, 0)    \
  V(ScheduleMicrotask, 1)       \
  V(GetCallbackHandle, 1)       \
  V(GetCallbackFromHandle, 1)   \
  /** BD ADD: START **/         \
  V(ForceGC, 0)                 \
  V(StartBoost, 2)              \
  V(FinishBoost, 1)             \
  V(PreloadFontFamilies, 2)     \
  V(DisableMips, 1)             \
  V(WarmUpZeroSizeOnce, 1)             \
  V(Performance_heapInfo, 0)    \
  V(Performance_getEngineInitApmInfo, 0)   \
  V(Performance_imageMemoryUsage, 0)       \
  V(Performance_startStackTraceSamples, 0) \
  V(Performance_stopStackTraceSamples, 0)  \
  V(Performance_getStackTraceSamples, 1)   \
  V(Performance_requestHeapSnapshot, 1)    \
  V(Performance_allocateScheduling, 3)     \
  V(Performance_allocateSchedulingStart, 0)\
  V(Performance_allocateSchedulingEnd, 0)  \
  V(Performance_getTotalExtMemInfo, 1)     \
  V(Performance_skGraphicCacheMemoryUsage, 0) \
  V(Performance_getGpuCacheUsageKBInfo, 1) \
  V(Reflect_reflectLibrary, 1)             \
  V(Reflect_libraryInvoke, 5)              \
  V(Reflect_reflectClass, 2)               \
  V(Reflect_classInvoke, 5)                \
  V(Reflect_instanceInvoke, 5)
  /** END **/

  BUILTIN_NATIVE_LIST(DECLARE_FUNCTION);

void DartRuntimeHooks::RegisterNatives(tonic::DartLibraryNatives* natives) {
  natives->Register({BUILTIN_NATIVE_LIST(REGISTER_FUNCTION)});
}

static void PropagateIfError(Dart_Handle result) {
  if (Dart_IsError(result)) {
    Dart_PropagateError(result);
  }
}

static Dart_Handle GetFunction(Dart_Handle builtin_library, const char* name) {
  Dart_Handle getter_name = ToDart(name);
  return Dart_Invoke(builtin_library, getter_name, 0, nullptr);
}

static void InitDartInternal(Dart_Handle builtin_library, bool is_ui_isolate) {
  Dart_Handle print = GetFunction(builtin_library, "_getPrintClosure");

  Dart_Handle internal_library = Dart_LookupLibrary(ToDart("dart:_internal"));

  Dart_Handle result =
      Dart_SetField(internal_library, ToDart("_printClosure"), print);
  PropagateIfError(result);

  if (is_ui_isolate) {
    // Call |_setupHooks| to configure |VMLibraryHooks|.
    Dart_Handle method_name = Dart_NewStringFromCString("_setupHooks");
    result = Dart_Invoke(builtin_library, method_name, 0, NULL);
    PropagateIfError(result);
  }

  Dart_Handle setup_hooks = Dart_NewStringFromCString("_setupHooks");

  Dart_Handle io_lib = Dart_LookupLibrary(ToDart("dart:io"));
  result = Dart_Invoke(io_lib, setup_hooks, 0, NULL);
  PropagateIfError(result);

  Dart_Handle isolate_lib = Dart_LookupLibrary(ToDart("dart:isolate"));
  result = Dart_Invoke(isolate_lib, setup_hooks, 0, NULL);
  PropagateIfError(result);
}

static void InitDartCore(Dart_Handle builtin, const std::string& script_uri) {
  Dart_Handle io_lib = Dart_LookupLibrary(ToDart("dart:io"));
  Dart_Handle get_base_url =
      Dart_Invoke(io_lib, ToDart("_getUriBaseClosure"), 0, NULL);
  Dart_Handle core_library = Dart_LookupLibrary(ToDart("dart:core"));
  Dart_Handle result =
      Dart_SetField(core_library, ToDart("_uriBaseClosure"), get_base_url);
  PropagateIfError(result);
}

static void InitDartAsync(Dart_Handle builtin_library, bool is_ui_isolate) {
  Dart_Handle schedule_microtask;
  if (is_ui_isolate) {
    schedule_microtask =
        GetFunction(builtin_library, "_getScheduleMicrotaskClosure");
  } else {
    Dart_Handle isolate_lib = Dart_LookupLibrary(ToDart("dart:isolate"));
    Dart_Handle method_name =
        Dart_NewStringFromCString("_getIsolateScheduleImmediateClosure");
    schedule_microtask = Dart_Invoke(isolate_lib, method_name, 0, NULL);
  }
  Dart_Handle async_library = Dart_LookupLibrary(ToDart("dart:async"));
  Dart_Handle set_schedule_microtask = ToDart("_setScheduleImmediateClosure");
  Dart_Handle result = Dart_Invoke(async_library, set_schedule_microtask, 1,
                                   &schedule_microtask);
  PropagateIfError(result);
}

static void InitDartIO(Dart_Handle builtin_library,
                       const std::string& script_uri) {
  Dart_Handle io_lib = Dart_LookupLibrary(ToDart("dart:io"));
  Dart_Handle platform_type =
      Dart_GetType(io_lib, ToDart("_Platform"), 0, nullptr);
  if (!script_uri.empty()) {
    Dart_Handle result = Dart_SetField(platform_type, ToDart("_nativeScript"),
                                       ToDart(script_uri));
    PropagateIfError(result);
  }
  Dart_Handle locale_closure =
      GetFunction(builtin_library, "_getLocaleClosure");
  Dart_Handle result =
      Dart_SetField(platform_type, ToDart("_localeClosure"), locale_closure);
  PropagateIfError(result);
}

void DartRuntimeHooks::Install(bool is_ui_isolate,
                               const std::string& script_uri) {
  Dart_Handle builtin = Dart_LookupLibrary(ToDart("dart:ui"));
  InitDartInternal(builtin, is_ui_isolate);
  InitDartCore(builtin, script_uri);
  InitDartAsync(builtin, is_ui_isolate);
  InitDartIO(builtin, script_uri);
}

void Logger_PrintDebugString(Dart_NativeArguments args) {
#ifndef NDEBUG
  Logger_PrintString(args);
#endif
}

// Implementation of native functions which are used for some
// test/debug functionality in standalone dart mode.
void Logger_PrintString(Dart_NativeArguments args) {
  std::stringstream stream;
  const auto& logger_prefix = UIDartState::Current()->logger_prefix();

#if !OS_ANDROID
  // Prepend all logs with the isolate debug name except on Android where that
  // prefix is specified in the log tag.
  if (logger_prefix.size() > 0) {
    stream << logger_prefix << ": ";
  }
#endif  // !OS_ANDROID

  // Append the log buffer obtained from Dart code.
  {
    Dart_Handle str = Dart_GetNativeArgument(args, 0);
    uint8_t* chars = nullptr;
    intptr_t length = 0;
    Dart_Handle result = Dart_StringToUTF8(str, &chars, &length);
    if (Dart_IsError(result)) {
      Dart_PropagateError(result);
      return;
    }
    if (length > 0) {
      stream << std::string{reinterpret_cast<const char*>(chars),
                            static_cast<size_t>(length)};
    }
  }

  const auto log_string = stream.str();
  const char* chars = log_string.c_str();
  const size_t length = log_string.size();

  // Log using platform specific mechanisms
  {
#if defined(OS_ANDROID)
    // Write to the logcat on Android.
    __android_log_print(ANDROID_LOG_INFO, logger_prefix.c_str(), "%.*s",
                        (int)length, chars);
#elif defined(OS_IOS)
    // Write to syslog on iOS.
    //
    // TODO(cbracken): replace with dedicated communication channel and bypass
    // iOS logging APIs altogether.
    syslog(1 /* LOG_ALERT */, "%.*s", (int)length, chars);
#else
    std::cout << log_string << std::endl;
#endif
  }

  if (dart::bin::ShouldCaptureStdout()) {
    // For now we report print output on the Stdout stream.
    uint8_t newline[] = {'\n'};
    Dart_ServiceSendDataEvent("Stdout", "WriteEvent",
                              reinterpret_cast<const uint8_t*>(chars), length);
    Dart_ServiceSendDataEvent("Stdout", "WriteEvent", newline, sizeof(newline));
  }
}

void SaveCompilationTrace(Dart_NativeArguments args) {
  uint8_t* buffer = nullptr;
  intptr_t length = 0;
  Dart_Handle result = Dart_SaveCompilationTrace(&buffer, &length);
  if (Dart_IsError(result)) {
    Dart_SetReturnValue(args, result);
    return;
  }

  result = Dart_NewTypedData(Dart_TypedData_kUint8, length);
  if (Dart_IsError(result)) {
    Dart_SetReturnValue(args, result);
    return;
  }

  Dart_TypedData_Type type;
  void* data = nullptr;
  intptr_t size = 0;
  Dart_Handle status = Dart_TypedDataAcquireData(result, &type, &data, &size);
  if (Dart_IsError(status)) {
    Dart_SetReturnValue(args, status);
    return;
  }

  memcpy(data, buffer, length);
  Dart_TypedDataReleaseData(result);
  Dart_SetReturnValue(args, result);
}

void ScheduleMicrotask(Dart_NativeArguments args) {
  Dart_Handle closure = Dart_GetNativeArgument(args, 0);
  UIDartState::Current()->ScheduleMicrotask(closure);
}

static std::string GetFunctionLibraryUrl(Dart_Handle closure) {
  if (Dart_IsClosure(closure)) {
    closure = Dart_ClosureFunction(closure);
    PropagateIfError(closure);
  }

  if (!Dart_IsFunction(closure)) {
    return "";
  }

  Dart_Handle url = Dart_Null();
  Dart_Handle owner = Dart_FunctionOwner(closure);
  if (Dart_IsInstance(owner)) {
    owner = Dart_ClassLibrary(owner);
  }
  if (Dart_IsLibrary(owner)) {
    url = Dart_LibraryUrl(owner);
    PropagateIfError(url);
  }
  return DartConverter<std::string>::FromDart(url);
}

static std::string GetFunctionClassName(Dart_Handle closure) {
  Dart_Handle result;

  if (Dart_IsClosure(closure)) {
    closure = Dart_ClosureFunction(closure);
    PropagateIfError(closure);
  }

  if (!Dart_IsFunction(closure)) {
    return "";
  }

  bool is_static = false;
  result = Dart_FunctionIsStatic(closure, &is_static);
  PropagateIfError(result);
  if (!is_static) {
    return "";
  }

  result = Dart_FunctionOwner(closure);
  PropagateIfError(result);

  if (Dart_IsLibrary(result) || !Dart_IsInstance(result)) {
    return "";
  }
  return DartConverter<std::string>::FromDart(Dart_ClassName(result));
}

static std::string GetFunctionName(Dart_Handle func) {
  if (Dart_IsClosure(func)) {
    func = Dart_ClosureFunction(func);
    PropagateIfError(func);
  }

  if (!Dart_IsFunction(func)) {
    return "";
  }

  bool is_static = false;
  Dart_Handle result = Dart_FunctionIsStatic(func, &is_static);
  PropagateIfError(result);
  if (!is_static) {
    return "";
  }

  result = Dart_FunctionName(func);
  PropagateIfError(result);

  return DartConverter<std::string>::FromDart(result);
}

void GetCallbackHandle(Dart_NativeArguments args) {
  Dart_Handle func = Dart_GetNativeArgument(args, 0);
  std::string name = GetFunctionName(func);
  std::string class_name = GetFunctionClassName(func);
  std::string library_path = GetFunctionLibraryUrl(func);

  // `name` is empty if `func` can't be used as a callback. This is the case
  // when `func` is not a function object or is not a static function. Anonymous
  // closures (e.g. `(int a, int b) => a + b;`) also cannot be used as
  // callbacks, so `func` must be a tear-off of a named static function.
  if (!Dart_IsTearOff(func) || name.empty()) {
    Dart_SetReturnValue(args, Dart_Null());
    return;
  }
  Dart_SetReturnValue(
      args, DartConverter<int64_t>::ToDart(DartCallbackCache::GetCallbackHandle(
                name, class_name, library_path)));
}

void GetCallbackFromHandle(Dart_NativeArguments args) {
  Dart_Handle h = Dart_GetNativeArgument(args, 0);
  int64_t handle = DartConverter<int64_t>::FromDart(h);
  Dart_SetReturnValue(args, DartCallbackCache::GetCallback(handle));
}

// BD ADD: START
void StartBoost(Dart_NativeArguments args) {
  uint16_t flags = (uint16_t)DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 0));
  int millis = DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 1));
  Boost::Current()->StartUp(flags, millis);
}

void FinishBoost(Dart_NativeArguments args) {
  uint16_t flags = (uint16_t)DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 0));
  Boost::Current()->Finish(flags);
}

void PreloadFontFamilies(Dart_NativeArguments args) {
  std::vector<std::string> font_families = DartConverter<std::vector<std::string>>::FromDart(Dart_GetNativeArgument(args, 0));
  std::string locale = DartConverter<std::string>::FromDart(Dart_GetNativeArgument(args, 1));
  Boost::Current()->PreloadFontFamilies(font_families, locale);
}

void ForceGC(Dart_NativeArguments args) {
  Boost::Current()->ForceGC();
}

void DisableMips(Dart_NativeArguments args) {
  bool disable = (bool)DartConverter<bool >::FromDart(Dart_GetNativeArgument(args, 0));
  Boost::Current()->DisableMips(disable);
}

void WarmUpZeroSizeOnce(Dart_NativeArguments args) {
  bool warmUpForOnce = (bool)DartConverter<bool >::FromDart(Dart_GetNativeArgument(args, 0));
  Boost::Current()->WarmUpZeroSizeOnce(warmUpForOnce);
}

void Performance_heapInfo(Dart_NativeArguments args) {
  Dart_SetReturnValue(args, Dart_HeapInfo());
}

void Performance_getEngineInitApmInfo(Dart_NativeArguments args) {
    vector<int64_t> info = Performance::GetInstance()->GetEngineInitApmInfo();
    Dart_Handle result = Dart_NewList(info.size());
    for (size_t index = 0; index < info.size(); ++index) {
        Dart_ListSetAt(result, index, Dart_NewInteger(info[index]));
    }
    Dart_SetReturnValue(args, result);
}

void Performance_imageMemoryUsage(Dart_NativeArguments args) {
  Dart_SetReturnValue(args, DartConverter<int64_t>::ToDart(
                          Performance::GetInstance()->GetImageMemoryUsageKB()));
}

void Performance_skGraphicCacheMemoryUsage(Dart_NativeArguments args) {
  int64_t bitmapMem = 0;
  int64_t fontMem = 0;
  int64_t imageFilter = 0;
  int64_t mallocSize = 0;
  Performance::GetInstance()->GetSkGraphicMemUsageKB(&bitmapMem, &fontMem, &imageFilter, &mallocSize);
  Dart_Handle data_handle = Dart_NewList(4);
  Dart_ListSetAt(data_handle, 0, Dart_NewInteger(bitmapMem));
  Dart_ListSetAt(data_handle, 1, Dart_NewInteger(fontMem));
  Dart_ListSetAt(data_handle, 2, Dart_NewInteger(imageFilter));
  Dart_ListSetAt(data_handle, 3, Dart_NewInteger(mallocSize));
  Dart_SetReturnValue(args, data_handle);
}

void Performance_getGpuCacheUsageKBInfo(Dart_NativeArguments args) {
  Dart_Handle callback_handle = Dart_GetNativeArgument(args, 0);
  if (!Dart_IsClosure(callback_handle)) {
    Dart_SetReturnValue(args, ToDart("Callback must be a function"));
    return;
  }
  if (Performance::GetInstance()->IsExitApp()) {
    return;
  }
  auto* dart_state = UIDartState::Current();

  const auto& task_runners = dart_state->GetTaskRunners();
  task_runners.GetIOTaskRunner()->PostTask(fml::MakeCopyable(
    [callback = std::make_unique<tonic::DartPersistentValue>(
      tonic::DartState::Current(), callback_handle),
      ui_task_runner = task_runners.GetUITaskRunner(),
      gpu_task_runner = task_runners.GetGPUTaskRunner()]() mutable {
        int64_t iOGrTotalMem = 0;
        int64_t iOGrResMem = 0;
        int64_t iOGrPurgeableMem = 0;
        Performance::GetInstance()->GetIOGpuCacheUsageKB(&iOGrTotalMem, &iOGrResMem, &iOGrPurgeableMem);
        if (!gpu_task_runner.get() || Performance::GetInstance()->IsExitApp()) {
          ui_task_runner->PostTask(fml::MakeCopyable(
            [callback = std::move(callback)]() { callback->Clear(); }));
          return;
        }
        gpu_task_runner->PostTask(fml::MakeCopyable(
          [callback = std::move(callback), ui_task_runner, iOGrTotalMem, iOGrResMem, iOGrPurgeableMem]() mutable {
            if (ui_task_runner.get()) {
              if (Performance::GetInstance()->IsExitApp()) {
                ui_task_runner->PostTask(fml::MakeCopyable(
                  [callback = std::move(callback)]() { callback->Clear(); }));
                return;
              }
              int64_t grTotalMem = 0;
              int64_t grResMem = 0;
              int64_t grPurgeableMem = 0;
              Performance::GetInstance()->GetGpuCacheUsageKB(&grTotalMem, &grResMem, &grPurgeableMem);
              ui_task_runner->PostTask(fml::MakeCopyable(
                [callback = std::move(callback), grTotalMem, grResMem, grPurgeableMem, iOGrTotalMem, iOGrResMem, iOGrPurgeableMem]() {
                  std::shared_ptr<tonic::DartState> dart_state = callback->dart_state().lock();
                  if (!dart_state || !dart_state.get() || Performance::GetInstance()->IsExitApp()) {
                    callback->Clear();
                    return;
                  }
                  tonic::DartState::Scope scope(dart_state);
                  Dart_Handle data_handle = Dart_NewList(6);
                  Dart_ListSetAt(data_handle, 0, Dart_NewInteger(grTotalMem));
                  Dart_ListSetAt(data_handle, 1, Dart_NewInteger(grResMem));
                  Dart_ListSetAt(data_handle, 2, Dart_NewInteger(grPurgeableMem));
                  Dart_ListSetAt(data_handle, 3, Dart_NewInteger(iOGrTotalMem));
                  Dart_ListSetAt(data_handle, 4, Dart_NewInteger(iOGrResMem));
                  Dart_ListSetAt(data_handle, 5, Dart_NewInteger(iOGrPurgeableMem));
                  tonic::DartInvoke(callback->value(), {data_handle});
                }));
            }
          }));
    }));
}

void Performance_getTotalExtMemInfo(Dart_NativeArguments args) {
  Dart_Handle callback_handle = Dart_GetNativeArgument(args, 0);
  if (!Dart_IsClosure(callback_handle)) {
    Dart_SetReturnValue(args, ToDart("Callback must be a function"));
    return;
  }
  Performance* performance = Performance::GetInstance();
  if (performance->IsExitApp()) {
    return;
  }
  auto* dart_state = UIDartState::Current();

  const auto& task_runners = dart_state->GetTaskRunners();
  int64_t imageMem = performance->GetImageMemoryUsageKB();
  int64_t bitmapMem = 0;
  int64_t fontMem = 0;
  int64_t imageFilter = 0;
  int64_t mallocSize = 0;
  performance->GetSkGraphicMemUsageKB(&bitmapMem, &fontMem, &imageFilter, &mallocSize);

  task_runners.GetIOTaskRunner()->PostTask(fml::MakeCopyable(
    [callback = std::make_unique<tonic::DartPersistentValue>(
      tonic::DartState::Current(), callback_handle),
      ui_task_runner = task_runners.GetUITaskRunner(),
      gpu_task_runner = task_runners.GetGPUTaskRunner(), imageMem, bitmapMem, fontMem, imageFilter, mallocSize]() mutable {
        if (!gpu_task_runner.get() || Performance::GetInstance()->IsExitApp()) {
          ui_task_runner->PostTask(fml::MakeCopyable(
            [callback = std::move(callback)]() { callback->Clear(); }));
          return;
        }
        int64_t iOGrTotalMem = 0;
        int64_t iOGrResMem = 0;
        int64_t iOGrPurgeableMem = 0;
        Performance::GetInstance()->GetIOGpuCacheUsageKB(&iOGrTotalMem, &iOGrResMem, &iOGrPurgeableMem);
        gpu_task_runner->PostTask(fml::MakeCopyable(
          [callback = std::move(callback),
            ui_task_runner, imageMem, bitmapMem, fontMem, imageFilter, iOGrTotalMem, iOGrResMem, iOGrPurgeableMem, mallocSize
          ]() mutable {
            if (ui_task_runner.get()) {
              if (Performance::GetInstance()->IsExitApp()) {
                ui_task_runner->PostTask(fml::MakeCopyable(
                  [callback = std::move(callback)]() { callback->Clear(); }));
                return;
              }
              int64_t grTotalMem = 0;
              int64_t grResMem = 0;
              int64_t grPurgeableMem = 0;
              Performance::GetInstance()->GetGpuCacheUsageKB(&grTotalMem, &grResMem, &grPurgeableMem);
              ui_task_runner->PostTask(fml::MakeCopyable(
                [callback = std::move(callback), imageMem, bitmapMem, fontMem, imageFilter, grTotalMem,
                 grResMem, grPurgeableMem, iOGrTotalMem, iOGrResMem, iOGrPurgeableMem, mallocSize]() {
                  std::shared_ptr<tonic::DartState> dart_state = callback->dart_state().lock();
                  if (!dart_state || !dart_state.get() || Performance::GetInstance()->IsExitApp()) {
                    callback->Clear();
                    return;
                  }
                  tonic::DartState::Scope scope(dart_state);
                  Dart_Handle data_handle = Dart_NewList(11);
                  Dart_ListSetAt(data_handle, 0, Dart_NewInteger(imageMem));
                  Dart_ListSetAt(data_handle, 1, Dart_NewInteger(grTotalMem));
                  Dart_ListSetAt(data_handle, 2, Dart_NewInteger(grResMem));
                  Dart_ListSetAt(data_handle, 3, Dart_NewInteger(grPurgeableMem));
                  Dart_ListSetAt(data_handle, 4, Dart_NewInteger(iOGrTotalMem));
                  Dart_ListSetAt(data_handle, 5, Dart_NewInteger(iOGrResMem));
                  Dart_ListSetAt(data_handle, 6, Dart_NewInteger(iOGrPurgeableMem));
                  Dart_ListSetAt(data_handle, 7, Dart_NewInteger(bitmapMem));
                  Dart_ListSetAt(data_handle, 8, Dart_NewInteger(fontMem));
                  Dart_ListSetAt(data_handle, 9, Dart_NewInteger(imageFilter));
                  Dart_ListSetAt(data_handle, 10, Dart_NewInteger(mallocSize));
                  tonic::DartInvoke(callback->value(), {data_handle});
                }));
            }
          }));
    }));
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

void Performance_allocateScheduling(Dart_NativeArguments args) {
  int dis_ygc = (int)DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 0));
  int dis_ygc_start = (int)DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 1));
  int dis_ygc_end = (int)DartConverter<int>::FromDart(Dart_GetNativeArgument(args, 2));
  // int new_gen_size = tonic::DartConverter<int>::FromArguments(args, 4, exception);
  // int old_gen_size = tonic::DartConverter<int>::FromArguments(args, 5, exception);
  // Dart_DelayGc(dis_ygc, dis_ygc_start, dis_ygc_end, new_gen_size, old_gen_size);
  Dart_AllocateScheduling(dis_ygc, dis_ygc_start, dis_ygc_end);
}

void Performance_allocateSchedulingStart(Dart_NativeArguments args) {
  Dart_AllocateSchedulingStart();
}

void Performance_allocateSchedulingEnd(Dart_NativeArguments args) {
  Dart_AllocateSchedulingEnd();
}

void Reflect_reflectLibrary(Dart_NativeArguments args){
    Dart_Handle lib = Dart_ReflectLibrary(Dart_GetNativeArgument(args, 0));
    Dart_SetReturnValue(args, lib);
}

void Reflect_libraryInvoke(Dart_NativeArguments args) {
    Dart_Handle lib = Dart_GetNativeArgument(args, 0);
    Dart_Handle invokeType = Dart_GetNativeArgument(args, 1);
    Dart_Handle functionName = Dart_GetNativeArgument(args, 2);
    Dart_Handle arguments = Dart_GetNativeArgument(args, 3);
    Dart_Handle names = Dart_GetNativeArgument(args, 4);
    Dart_Handle res = Dart_LibraryInvoke(lib, invokeType, functionName, arguments, names);
    Dart_SetReturnValue(args, res);
}

void Reflect_reflectClass(Dart_NativeArguments args){
    Dart_Handle cls = Dart_ReflectClass(Dart_GetNativeArgument(args, 0), Dart_GetNativeArgument(args, 1));
    Dart_SetReturnValue(args, cls);
}

void Reflect_classInvoke(Dart_NativeArguments args) {
    Dart_Handle cls = Dart_GetNativeArgument(args, 0);
    Dart_Handle invokeType = Dart_GetNativeArgument(args, 1);
    Dart_Handle functionName = Dart_GetNativeArgument(args, 2);
    Dart_Handle arguments = Dart_GetNativeArgument(args, 3);
    Dart_Handle names = Dart_GetNativeArgument(args, 4);
    Dart_Handle res = Dart_ClassInvoke(cls, invokeType, functionName, arguments, names);
    Dart_SetReturnValue(args, res);
}

void Reflect_instanceInvoke(Dart_NativeArguments args) {
    Dart_Handle instance = Dart_GetNativeArgument(args, 0);
    Dart_Handle invokeType = Dart_GetNativeArgument(args, 1);
    Dart_Handle functionName = Dart_GetNativeArgument(args, 2);
    Dart_Handle arguments = Dart_GetNativeArgument(args, 3);
    Dart_Handle names = Dart_GetNativeArgument(args, 4);
    Dart_Handle res = Dart_InstanceInvoke(instance, invokeType, functionName, arguments, names);
    Dart_SetReturnValue(args, res);
}

// END
}  // namespace flutter
