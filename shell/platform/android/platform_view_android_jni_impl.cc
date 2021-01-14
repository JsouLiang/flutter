// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/platform_view_android_jni_impl.h"

#include <android/native_window_jni.h>
#include <dlfcn.h>
#include <jni.h>
#include <memory>
#include <sstream>
// BD ADD: START
#include <android/bitmap.h>
#include "flutter/lib/ui/painting/image.h"
// END
#include <utility>

#include "unicode/uchar.h"

#include "flutter/assets/directory_asset_bundle.h"
#include "flutter/common/settings.h"
#include "flutter/fml/file.h"
#include "flutter/fml/mapping.h"
#include "flutter/fml/native_library.h"
#include "flutter/fml/platform/android/jni_util.h"
#include "flutter/fml/platform/android/jni_weak_ref.h"
#include "flutter/fml/platform/android/scoped_java_ref.h"
#include "flutter/fml/size.h"
#include "flutter/lib/ui/plugins/callback_cache.h"
#include "flutter/runtime/dart_service_isolate.h"
#include "flutter/shell/common/run_configuration.h"
#include "flutter/shell/platform/android/android_external_texture_gl.h"
#include "flutter/shell/platform/android/android_shell_holder.h"
#include "flutter/shell/platform/android/apk_asset_provider.h"
#include "flutter/shell/platform/android/flutter_main.h"
#include "flutter/shell/platform/android/jni/platform_view_android_jni.h"
#include "flutter/shell/platform/android/platform_view_android.h"
// BD ADD:
#include "flutter/bdflutter/shell/platform/android/android_native_export_codec.h"
#include "flutter/fml/make_copyable.h"

#define ANDROID_SHELL_HOLDER \
  (reinterpret_cast<AndroidShellHolder*>(shell_holder))

namespace flutter {

namespace {

bool CheckException(JNIEnv* env) {
  if (env->ExceptionCheck() == JNI_FALSE) {
    return true;
  }

  jthrowable exception = env->ExceptionOccurred();
  env->ExceptionClear();
  FML_LOG(ERROR) << fml::jni::GetJavaExceptionInfo(env, exception);
  env->DeleteLocalRef(exception);
  return false;
}

}  // anonymous namespace

static fml::jni::ScopedJavaGlobalRef<jclass>* g_flutter_callback_info_class =
    nullptr;

static fml::jni::ScopedJavaGlobalRef<jclass>* g_flutter_jni_class = nullptr;

static fml::jni::ScopedJavaGlobalRef<jclass>* g_texture_wrapper_class = nullptr;

static fml::jni::ScopedJavaGlobalRef<jclass>* g_java_long_class = nullptr;

/**
 * BD ADD: android image loader callback class
 */
static fml::jni::ScopedJavaGlobalRef<jclass>* g_image_loader_callback_class = nullptr;
/**
 * BD ADD: android image loader class
 */
static fml::jni::ScopedJavaGlobalRef<jclass>* g_image_loader_class = nullptr;

// Called By Native

static jmethodID g_flutter_callback_info_constructor = nullptr;
jobject CreateFlutterCallbackInformation(
    JNIEnv* env,
    const std::string& callbackName,
    const std::string& callbackClassName,
    const std::string& callbackLibraryPath) {
  return env->NewObject(g_flutter_callback_info_class->obj(),
                        g_flutter_callback_info_constructor,
                        env->NewStringUTF(callbackName.c_str()),
                        env->NewStringUTF(callbackClassName.c_str()),
                        env->NewStringUTF(callbackLibraryPath.c_str()));
}

static jfieldID g_jni_shell_holder_field = nullptr;

static jmethodID g_jni_constructor = nullptr;

static jmethodID g_long_constructor = nullptr;

static jmethodID g_handle_platform_message_method = nullptr;

static jmethodID g_handle_platform_message_response_method = nullptr;

static jmethodID g_update_semantics_method = nullptr;

static jmethodID g_update_custom_accessibility_actions_method = nullptr;

static jmethodID g_on_first_frame_method = nullptr;

static jmethodID g_on_engine_restart_method = nullptr;

static jmethodID g_create_overlay_surface_method = nullptr;

static jmethodID g_destroy_overlay_surfaces_method = nullptr;

static jmethodID g_on_begin_frame_method = nullptr;

static jmethodID g_on_end_frame_method = nullptr;

static jmethodID g_attach_to_gl_context_method = nullptr;

static jmethodID g_update_tex_image_method = nullptr;

static jmethodID g_get_transform_matrix_method = nullptr;

static jmethodID g_detach_from_gl_context_method = nullptr;

static jmethodID g_compute_platform_resolved_locale_method = nullptr;

static jmethodID g_request_dart_deferred_library_method = nullptr;

// Called By Java
static jmethodID g_on_display_platform_view_method = nullptr;

// static jmethodID g_on_composite_platform_view_method = nullptr;

static jmethodID g_on_display_overlay_surface_method = nullptr;

/**
 * BD ADD: add for c++ image load result callback
 */
static jmethodID g_native_callback_constructor = nullptr;
/**
 * BD ADD: android image loader load method
 */
static jmethodID g_image_loader_class_load = nullptr;

static jmethodID g_image_loader_class_load_gif = nullptr;
/**
 * BD ADD: android image loader release method
 */
static jmethodID g_image_loader_class_release = nullptr;
/**
 * BD ADD: lock pixel buffer from android Bitmap
 */
bool ObtainPixelsFromJavaBitmap(JNIEnv* env, jobject jbitmap, uint32_t* width, uint32_t* height, int32_t* format, uint32_t* stride, void** pixels) {
    FML_CHECK(CheckException(env));
    AndroidBitmapInfo info;
    if (ANDROID_BITMAP_RESULT_SUCCESS != AndroidBitmap_getInfo(env, jbitmap, &info)) {
        FML_LOG(ERROR)<<"ObtainPixelsFromJavaBitmap: get bitmap info failed"<<std::endl;
        return false;
    }

    if(info.format == ANDROID_BITMAP_FORMAT_NONE){
        FML_LOG(ERROR)<<"ObtainPixelsFromJavaBitmap: format not support"<<std::endl;
        return false;
    }

    *width = info.width;
    *height = info.height;
    *stride = info.stride;
    *format = info.format;
    if (ANDROID_BITMAP_RESULT_SUCCESS != AndroidBitmap_lockPixels(env, jbitmap, pixels) || *pixels == nullptr) {
        FML_LOG(ERROR)<<"ObtainPixelsFromJavaBitmap: lock dst bitmap failed"<<std::endl;
        return false;
    }
    return true;
}
/**
 * BD ADD: release image load context
 */
void ReleaseLoadContext(const void* pixels, SkImage::ReleaseContext releaseContext);

// BD ADD: START
class AndroidImageLoadContext {
public:

    ImageLoaderContext loaderContext;

    AndroidImageLoadContext(std::function<void(sk_sp<SkImage> image)> _callback, ImageLoaderContext _loaderContext, jobject _imageLoader):
    loaderContext(_loaderContext),
    androidImageLoader(_imageLoader),
    callback(std::move(_callback)){}

    AndroidImageLoadContext(std::function<void(std::unique_ptr<NativeExportCodec> codec)> _callback, ImageLoaderContext _loaderContext, jobject _imageLoader):
    loaderContext(_loaderContext),
    androidImageLoader(_imageLoader),
    codecCallback(std::move(_callback)){}

    ~AndroidImageLoadContext(){
    }

    void onLoadForCodecSuccess(JNIEnv *env, std::string cKey, jobject jCodec) {
      if (!loaderContext.task_runners.IsValid()) {
        return;
      }
      auto loaderContentRef = loaderContext;
      loaderContext.task_runners.GetUITaskRunner()->PostTask([loaderContentRef, jCodec, cKey,
                                                               androidImageLoader = androidImageLoader,
                                                               codecCallback = std::move(codecCallback)] {
        JNIEnv *env = fml::jni::AttachCurrentThread();
        AndroidNativeExportCodec *codec = new AndroidNativeExportCodec(env, cKey, jCodec);
        auto context = loaderContentRef.resourceContext;
        auto task_runners = loaderContentRef.task_runners;
        if (codec->frameCount_ == -1) {
          jobject jObject = env->NewGlobalRef(codec->codec);
          loaderContentRef.task_runners.GetIOTaskRunner()->PostTask(
            [cKey, task_runners, context,
              codecCallback = std::move(codecCallback),
               codec, jObject, jCodec, androidImageLoader]() {
              JNIEnv *env = fml::jni::AttachCurrentThread();
              codec->skImage = uploadTexture(env, jObject, context);
              if (codec->skImage){
                std::unique_ptr<NativeExportCodec> codec2(codec);
                codecCallback(std::move(codec2));
              } else {
                codecCallback(nullptr);
              }
              task_runners.GetPlatformTaskRunner()->PostTask([jCodec] {
                JNIEnv *env = fml::jni::AttachCurrentThread();
                  env->DeleteGlobalRef(jCodec);
              });
              task_runners.GetUITaskRunner()->PostTask([jObject, androidImageLoader, cKey, task_runners] {
                JNIEnv *env = fml::jni::AttachCurrentThread();
                env->DeleteGlobalRef(jObject);
                env->CallVoidMethod(androidImageLoader, g_image_loader_class_release,
                                    fml::jni::StringToJavaString(env, cKey).obj());
                task_runners.GetIOTaskRunner()->PostTask([androidImageLoader = androidImageLoader] {
                  JNIEnv *env = fml::jni::AttachCurrentThread();
                  env->DeleteGlobalRef(androidImageLoader);
                });
              });
            });
        } else {
          codec->codec = env->NewGlobalRef(codec->codec);
          std::unique_ptr<NativeExportCodec> codec2(codec);
          codecCallback(std::move(codec2));
          task_runners.GetPlatformTaskRunner()->PostTask([cKey, task_runners, jCodec, androidImageLoader] {
            JNIEnv *env = fml::jni::AttachCurrentThread();
            env->DeleteGlobalRef(jCodec);
            task_runners.GetUITaskRunner()->PostTask([task_runners, cKey, androidImageLoader] {
              JNIEnv *env = fml::jni::AttachCurrentThread();
              env->CallVoidMethod(androidImageLoader, g_image_loader_class_release,
                                  fml::jni::StringToJavaString(env, cKey).obj());
              task_runners.GetIOTaskRunner()->PostTask([androidImageLoader = androidImageLoader] {
                JNIEnv *env = fml::jni::AttachCurrentThread();
                env->DeleteGlobalRef(androidImageLoader);
              });
            });
          });
        }
      });
    }

    void onGetNextFrameSuccess(JNIEnv *env, std::string cKey, jobject jbitmap) {
      if (!loaderContext.task_runners.IsValid()) {
        return;
      }
      loaderContext.task_runners.GetIOTaskRunner()->PostTask(
        [this, cKey, jbitmap, androidImageLoader = androidImageLoader,
          callback = std::move(callback)]() {
          JNIEnv *env = fml::jni::AttachCurrentThread();
          env->CallVoidMethod(androidImageLoader, g_image_loader_class_release,
                              fml::jni::StringToJavaString(env, cKey).obj());
          env->DeleteGlobalRef(androidImageLoader);
          auto context = loaderContext.resourceContext;
          callback(uploadTexture(env, jbitmap, context));
          loaderContext.task_runners.GetPlatformTaskRunner()->PostTask([jbitmap] {
            JNIEnv *env = fml::jni::AttachCurrentThread();
            if (env->GetObjectRefType(jbitmap) == 2) {
              env->DeleteGlobalRef(jbitmap);
            }
          });
        });
    }

    void onLoadSuccess(JNIEnv *env, std::string cKey, jobject jbitmap) {
      if (!loaderContext.task_runners.IsValid()) {
        return;
      }
      loaderContext.task_runners.GetIOTaskRunner()->PostTask(
          [this, cKey, jbitmap, androidImageLoader = androidImageLoader, loaderContext = loaderContext,
           callback = std::move(callback)]() {
            JNIEnv* env = fml::jni::AttachCurrentThread();
            auto context = loaderContext.resourceContext;
            callback(uploadTexture(env, jbitmap, context));
            env->CallVoidMethod(androidImageLoader,
                                g_image_loader_class_release,
                                fml::jni::StringToJavaString(env, cKey).obj());
            env->DeleteGlobalRef(androidImageLoader);
            loaderContext.task_runners.GetPlatformTaskRunner()->PostTask(
                [jbitmap] {
                  JNIEnv* env = fml::jni::AttachCurrentThread();
                  if (env->GetObjectRefType(jbitmap) == 2) {
                    env->DeleteGlobalRef(jbitmap);
                  }
                });
          });
    }

    static sk_sp<SkImage> uploadTexture(JNIEnv *env, jobject jbitmap, fml::WeakPtr<GrDirectContext> context) {
      if (jbitmap == nullptr) {
        return nullptr;
      }
      void *pixels = nullptr;
      uint32_t width = 0;
      uint32_t height = 0;
      int32_t format = 0;
      uint32_t stride;
      if (!ObtainPixelsFromJavaBitmap(env, jbitmap, &width, &height, &format, &stride, &pixels)) {
        return nullptr;
      }
      sk_sp<SkImage> skImage;
      SkColorType ct;
      // if android
      switch (format) {
        case AndroidBitmapFormat::ANDROID_BITMAP_FORMAT_NONE:
          ct = kUnknown_SkColorType;
          break;
        case AndroidBitmapFormat::ANDROID_BITMAP_FORMAT_RGBA_8888:
          ct = kRGBA_8888_SkColorType;
          break;
        case AndroidBitmapFormat::ANDROID_BITMAP_FORMAT_RGB_565:
          ct = kRGB_565_SkColorType;
          break;
        case AndroidBitmapFormat::ANDROID_BITMAP_FORMAT_RGBA_4444:
          ct = kARGB_4444_SkColorType;
          break;
        case AndroidBitmapFormat::ANDROID_BITMAP_FORMAT_A_8:
          ct = kAlpha_8_SkColorType;
          break;
      }
      SkImageInfo sk_info =
              SkImageInfo::Make(width, height, ct, kPremul_SkAlphaType);
      size_t row_bytes = stride;
      if (row_bytes < sk_info.minRowBytes()) {
        return nullptr;
      }

      sk_sp<SkData> buffer = SkData::MakeWithProc(pixels, row_bytes * height, ReleaseLoadContext,
                                                  nullptr);
      SkPixmap pixelMap(sk_info, buffer->data(), row_bytes);
      skImage = SkImage::MakeCrossContextFromPixmap(context.get(), pixelMap, false, true);
      auto res = AndroidBitmap_unlockPixels(env, jbitmap);
//      jclass clazz = env->GetObjectClass(jbitmap);
//      env->CallVoidMethod(jbitmap, env->GetMethodID(clazz, "recycle", "()V"));
      if (ANDROID_BITMAP_RESULT_SUCCESS != res) {
        FML_LOG(ERROR)
        << "FlutterViewHandleBitmapPixels: unlock dst bitmap failed code is " + std::to_string(res)
        << std::endl;
      }
      return skImage;
    }

    void onLoadFail(JNIEnv* env, std::string cKey) {
        callback(nullptr);
    }

    void onCodecLoadFail(JNIEnv* env, std::string cKey) {
      if (!loaderContext.task_runners.IsValid()) {
        return;
      }
      loaderContext.task_runners.GetIOTaskRunner()->PostTask([callback = std::move(codecCallback),
                                                               androidImageLoader = androidImageLoader](){
        fml::jni::AttachCurrentThread()->DeleteGlobalRef(androidImageLoader);
        callback(nullptr);
      });
    }

    void onGetNextFrameFail(JNIEnv* env, std::string cKey) {
      if (!loaderContext.task_runners.IsValid()) {
        return;
      }
      loaderContext.task_runners.GetIOTaskRunner()->PostTask([callback = std::move(callback),
                                                                androidImageLoader = androidImageLoader](){
        fml::jni::AttachCurrentThread()->DeleteGlobalRef(androidImageLoader);
        callback(nullptr);
      });
    }
private:
    jobject androidImageLoader;
    std::function<void(sk_sp<SkImage> image)> callback;
    std::function<void(std::unique_ptr<NativeExportCodec> codec)> codecCallback;
};
// END

/**
 * BD ADD: global map for image load context
 */
static std::map<std::string, std::shared_ptr<AndroidImageLoadContext>> g_image_load_contexts;
static std::recursive_mutex g_mutex;

/**
 * BD ADD: get the context from g_image_load_contexts with g_mutex lock
 */
static inline auto getImageLoadContextWithLock(const std::string &cKey) {
  g_mutex.lock();
  auto loadContext = g_image_load_contexts[cKey];
  g_mutex.unlock();
  return loadContext;
}

/**
 * BD ADD: put the context into g_image_load_contexts with g_mutex lock
 */
static inline void putImageLoadContextWithLock(std::shared_ptr<AndroidImageLoadContext> context, const std::string &cKey) {
  g_mutex.lock();
  g_image_load_contexts[cKey] = context;
  g_mutex.unlock();
}

/**
 * BD ADD: erase the context from g_image_load_contexts with g_mutex lock
 */
static inline void eraseImageLoadContextWithLock(const std::string &cKey) {
  g_mutex.lock();
  g_image_load_contexts.erase(cKey);
  g_mutex.unlock();
}

/**
 * BD ADD: call android to load image
 */
void CallJavaImageLoader(jobject android_image_loader, const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(sk_sp<SkImage> image)> callback) {
  JNIEnv* env = fml::jni::AttachCurrentThread();
  auto loadContext = std::make_shared<AndroidImageLoadContext>(callback, loaderContext, env->NewGlobalRef(android_image_loader));
  auto key = url + std::to_string(reinterpret_cast<jlong>(loadContext.get()));
  putImageLoadContextWithLock(loadContext, key);
  auto callObject = env->NewObject(g_image_loader_callback_class->obj(), g_native_callback_constructor);
  auto nativeCallback = new fml::jni::ScopedJavaLocalRef<jobject>(env, callObject);
  env->CallVoidMethod(android_image_loader, g_image_loader_class_load, fml::jni::StringToJavaString(env, url).obj(),
                      reinterpret_cast<jint>(width), reinterpret_cast<jint>(height), scale, nativeCallback->obj(), fml::jni::StringToJavaString(env, key).obj());
  env->DeleteLocalRef(callObject);
}

void CallJavaImageLoaderForCodec(jobject android_image_loader, const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(std::unique_ptr<NativeExportCodec> codec)> callback) {
  JNIEnv* env = fml::jni::AttachCurrentThread();
  auto loadContext = std::make_shared<AndroidImageLoadContext>(callback, loaderContext, env->NewGlobalRef(android_image_loader));
  auto key = url + std::to_string(reinterpret_cast<jlong>(loadContext.get()));
  putImageLoadContextWithLock(loadContext, key);
  auto callObject = env->NewObject(g_image_loader_callback_class->obj(), g_native_callback_constructor);
  auto nativeCallback = new fml::jni::ScopedJavaLocalRef<jobject>(env, callObject);
  env->CallVoidMethod(android_image_loader, g_image_loader_class_load, fml::jni::StringToJavaString(env, url).obj(),
                      reinterpret_cast<jint>(width), reinterpret_cast<jint>(height), scale, nativeCallback->obj(), fml::jni::StringToJavaString(env, key).obj());
  env->DeleteLocalRef(callObject);
}


void CallJavaImageLoaderGetNextFrame(jobject android_image_loader, ImageLoaderContext loaderContext, int currentFrame, std::shared_ptr<NativeExportCodec> codec, std::function<void(sk_sp<SkImage> image)> callback) {
  JNIEnv* env = fml::jni::AttachCurrentThread();
  auto loadContext = std::make_shared<AndroidImageLoadContext>(callback, loaderContext, env->NewGlobalRef(android_image_loader));
  auto key = *codec->key + std::to_string(reinterpret_cast<jlong>(loadContext.get())) + std::to_string(currentFrame);
  putImageLoadContextWithLock(loadContext, key);
  auto callObject = env->NewObject(g_image_loader_callback_class->obj(), g_native_callback_constructor);
  auto nativeCallback = new fml::jni::ScopedJavaLocalRef<jobject>(env, callObject);
  AndroidNativeExportCodec *androidCodec = static_cast<AndroidNativeExportCodec *>(codec.get());
  env->CallVoidMethod(android_image_loader, g_image_loader_class_load_gif, reinterpret_cast<jint>(currentFrame), androidCodec->codec, nativeCallback->obj(), fml::jni::StringToJavaString(env, key).obj());
  env->DeleteLocalRef(callObject);
}

/**
 * BD ADD: called by skia to release pixel resource
 */
void ReleaseLoadContext(const void* pixels, SkImage::ReleaseContext releaseContext){
}

static jmethodID g_overlay_surface_id_method = nullptr;

static jmethodID g_overlay_surface_surface_method = nullptr;

// Mutators
static fml::jni::ScopedJavaGlobalRef<jclass>* g_mutators_stack_class = nullptr;
static jmethodID g_mutators_stack_init_method = nullptr;
static jmethodID g_mutators_stack_push_transform_method = nullptr;
static jmethodID g_mutators_stack_push_cliprect_method = nullptr;
static jmethodID g_mutators_stack_push_cliprrect_method = nullptr;

// Called By Java
static jlong AttachJNI(JNIEnv* env,
                       jclass clazz,
                       jobject flutterJNI,
                       jboolean is_background_view,
                       // BD ADD:
                       jint flag) {
  fml::jni::JavaObjectWeakGlobalRef java_object(env, flutterJNI);
  std::shared_ptr<PlatformViewAndroidJNI> jni_facade =
      std::make_shared<PlatformViewAndroidJNIImpl>(java_object);
  auto shell_holder = std::make_unique<AndroidShellHolder>(
      // BD MOD:
      // FlutterMain::Get().GetSettings(), jni_facade, is_background_view);
       FlutterMain::Get().GetSettings(), jni_facade, is_background_view, flag);
  if (shell_holder->IsValid()) {
    return reinterpret_cast<jlong>(shell_holder.release());
  } else {
    return 0;
  }
}

static void DestroyJNI(JNIEnv* env, jobject jcaller, jlong shell_holder) {
  // BD MOD
  // delete ANDROID_SHELL_HOLDER;
  ANDROID_SHELL_HOLDER->ExitApp([holder = ANDROID_SHELL_HOLDER]() { delete holder; });
}

// Signature is similar to RunBundleAndSnapshotFromLibrary but it can't change
// the bundle path or asset manager since we can only spawn with the same
// AOT.
//
// The shell_holder instance must be a pointer address to the current
// AndroidShellHolder whose Shell will be used to spawn a new Shell.
//
// This creates a Java Long that points to the newly created
// AndroidShellHolder's raw pointer, connects that Long to a newly created
// FlutterJNI instance, then returns the FlutterJNI instance.
static jobject SpawnJNI(JNIEnv* env,
                        jobject jcaller,
                        jlong shell_holder,
                        jstring jEntrypoint,
                        jstring jLibraryUrl) {
  jobject jni = env->NewObject(g_flutter_jni_class->obj(), g_jni_constructor);
  if (jni == nullptr) {
    FML_LOG(ERROR) << "Could not create a FlutterJNI instance";
    return nullptr;
  }

  fml::jni::JavaObjectWeakGlobalRef java_jni(env, jni);
  std::shared_ptr<PlatformViewAndroidJNI> jni_facade =
      std::make_shared<PlatformViewAndroidJNIImpl>(java_jni);

  auto entrypoint = fml::jni::JavaStringToString(env, jEntrypoint);
  auto libraryUrl = fml::jni::JavaStringToString(env, jLibraryUrl);

  auto spawned_shell_holder =
      ANDROID_SHELL_HOLDER->Spawn(jni_facade, entrypoint, libraryUrl);

  if (spawned_shell_holder == nullptr || !spawned_shell_holder->IsValid()) {
    FML_LOG(ERROR) << "Could not spawn Shell";
    return nullptr;
  }

  jobject javaLong = env->CallStaticObjectMethod(
      g_java_long_class->obj(), g_long_constructor,
      reinterpret_cast<jlong>(spawned_shell_holder.release()));
  if (javaLong == nullptr) {
    FML_LOG(ERROR) << "Could not create a Long instance";
    return nullptr;
  }

  env->SetObjectField(jni, g_jni_shell_holder_field, javaLong);

  return jni;
}

// BD ADD: START
static void ScheduleBackgroundFrame(JNIEnv *env, jobject jcaller, jlong shell_holder) {
  // 每个Holder都拥有自己的Settings，需要设置不同的dynamic_dill_path
  ANDROID_SHELL_HOLDER->ScheduleBackgroundFrame();
}

static void ScheduleFrameNow(JNIEnv *env, jobject jcaller, jlong shell_holder) {
  ANDROID_SHELL_HOLDER->ScheduleFrameNow();
}
// END

static void SurfaceCreated(JNIEnv* env,
                           jobject jcaller,
                           jlong shell_holder,
                           jobject jsurface) {
  // Note: This frame ensures that any local references used by
  // ANativeWindow_fromSurface are released immediately. This is needed as a
  // workaround for https://code.google.com/p/android/issues/detail?id=68174
  fml::jni::ScopedJavaLocalFrame scoped_local_reference_frame(env);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      ANativeWindow_fromSurface(env, jsurface));
  ANDROID_SHELL_HOLDER->GetPlatformView()->NotifyCreated(std::move(window));
}

static void SurfaceWindowChanged(JNIEnv* env,
                                 jobject jcaller,
                                 jlong shell_holder,
                                 jobject jsurface) {
  // Note: This frame ensures that any local references used by
  // ANativeWindow_fromSurface are released immediately. This is needed as a
  // workaround for https://code.google.com/p/android/issues/detail?id=68174
  fml::jni::ScopedJavaLocalFrame scoped_local_reference_frame(env);
  auto window = fml::MakeRefCounted<AndroidNativeWindow>(
      ANativeWindow_fromSurface(env, jsurface));
  ANDROID_SHELL_HOLDER->GetPlatformView()->NotifySurfaceWindowChanged(
      std::move(window));
}

static void SurfaceChanged(JNIEnv* env,
                           jobject jcaller,
                           jlong shell_holder,
                           jint width,
                           jint height) {
  ANDROID_SHELL_HOLDER->GetPlatformView()->NotifyChanged(
      SkISize::Make(width, height));
}

static void SurfaceDestroyed(JNIEnv* env, jobject jcaller, jlong shell_holder) {
  ANDROID_SHELL_HOLDER->GetPlatformView()->NotifyDestroyed();
}

static void RunBundleAndSnapshotFromLibrary(JNIEnv* env,
                                            jobject jcaller,
                                            jlong shell_holder,
                                            jstring jBundlePath,
                                            jstring jEntrypoint,
                                            jstring jLibraryUrl,
                                            jobject jAssetManager) {
  auto asset_manager = std::make_shared<flutter::AssetManager>();

  asset_manager->PushBack(std::make_unique<flutter::APKAssetProvider>(
      env,                                             // jni environment
      jAssetManager,                                   // asset manager
      fml::jni::JavaStringToString(env, jBundlePath))  // apk asset dir
  );

  auto entrypoint = fml::jni::JavaStringToString(env, jEntrypoint);
  auto libraryUrl = fml::jni::JavaStringToString(env, jLibraryUrl);

  ANDROID_SHELL_HOLDER->Launch(asset_manager, entrypoint, libraryUrl);
}

static jobject LookupCallbackInformation(JNIEnv* env,
                                         /* unused */ jobject,
                                         jlong handle) {
  auto cbInfo = flutter::DartCallbackCache::GetCallbackInformation(handle);
  if (cbInfo == nullptr) {
    return nullptr;
  }
  return CreateFlutterCallbackInformation(env, cbInfo->name, cbInfo->class_name,
                                          cbInfo->library_path);
}

static void SetViewportMetrics(JNIEnv* env,
                               jobject jcaller,
                               jlong shell_holder,
                               jfloat devicePixelRatio,
                               jint physicalWidth,
                               jint physicalHeight,
                               jint physicalPaddingTop,
                               jint physicalPaddingRight,
                               jint physicalPaddingBottom,
                               jint physicalPaddingLeft,
                               jint physicalViewInsetTop,
                               jint physicalViewInsetRight,
                               jint physicalViewInsetBottom,
                               jint physicalViewInsetLeft,
                               jint systemGestureInsetTop,
                               jint systemGestureInsetRight,
                               jint systemGestureInsetBottom,
                               jint systemGestureInsetLeft) {
  const flutter::ViewportMetrics metrics{
      static_cast<double>(devicePixelRatio),
      static_cast<double>(physicalWidth),
      static_cast<double>(physicalHeight),
      static_cast<double>(physicalPaddingTop),
      static_cast<double>(physicalPaddingRight),
      static_cast<double>(physicalPaddingBottom),
      static_cast<double>(physicalPaddingLeft),
      static_cast<double>(physicalViewInsetTop),
      static_cast<double>(physicalViewInsetRight),
      static_cast<double>(physicalViewInsetBottom),
      static_cast<double>(physicalViewInsetLeft),
      static_cast<double>(systemGestureInsetTop),
      static_cast<double>(systemGestureInsetRight),
      static_cast<double>(systemGestureInsetBottom),
      static_cast<double>(systemGestureInsetLeft),
  };

  ANDROID_SHELL_HOLDER->GetPlatformView()->SetViewportMetrics(metrics);
}

static jobject GetBitmap(JNIEnv* env, jobject jcaller, jlong shell_holder) {
  auto screenshot = ANDROID_SHELL_HOLDER->Screenshot(
      Rasterizer::ScreenshotType::UncompressedImage, false);
  if (screenshot.data == nullptr) {
    return nullptr;
  }

  const SkISize& frame_size = screenshot.frame_size;
  jsize pixels_size = frame_size.width() * frame_size.height();
  jintArray pixels_array = env->NewIntArray(pixels_size);
  if (pixels_array == nullptr) {
    return nullptr;
  }

  jint* pixels = env->GetIntArrayElements(pixels_array, nullptr);
  if (pixels == nullptr) {
    return nullptr;
  }

  auto* pixels_src = static_cast<const int32_t*>(screenshot.data->data());

  // Our configuration of Skia does not support rendering to the
  // BitmapConfig.ARGB_8888 format expected by android.graphics.Bitmap.
  // Convert from kRGBA_8888 to kBGRA_8888 (equivalent to ARGB_8888).
  for (int i = 0; i < pixels_size; i++) {
    int32_t src_pixel = pixels_src[i];
    uint8_t* src_bytes = reinterpret_cast<uint8_t*>(&src_pixel);
    std::swap(src_bytes[0], src_bytes[2]);
    pixels[i] = src_pixel;
  }

  env->ReleaseIntArrayElements(pixels_array, pixels, 0);

  jclass bitmap_class = env->FindClass("android/graphics/Bitmap");
  if (bitmap_class == nullptr) {
    return nullptr;
  }

  jmethodID create_bitmap = env->GetStaticMethodID(
      bitmap_class, "createBitmap",
      "([IIILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
  if (create_bitmap == nullptr) {
    return nullptr;
  }

  jclass bitmap_config_class = env->FindClass("android/graphics/Bitmap$Config");
  if (bitmap_config_class == nullptr) {
    return nullptr;
  }

  jmethodID bitmap_config_value_of = env->GetStaticMethodID(
      bitmap_config_class, "valueOf",
      "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;");
  if (bitmap_config_value_of == nullptr) {
    return nullptr;
  }

  jstring argb = env->NewStringUTF("ARGB_8888");
  if (argb == nullptr) {
    return nullptr;
  }

  jobject bitmap_config = env->CallStaticObjectMethod(
      bitmap_config_class, bitmap_config_value_of, argb);
  if (bitmap_config == nullptr) {
    return nullptr;
  }

  return env->CallStaticObjectMethod(bitmap_class, create_bitmap, pixels_array,
                                     frame_size.width(), frame_size.height(),
                                     bitmap_config);
}

static void DispatchPlatformMessage(JNIEnv* env,
                                    jobject jcaller,
                                    jlong shell_holder,
                                    jstring channel,
                                    jobject message,
                                    jint position,
                                    jint responseId) {
  ANDROID_SHELL_HOLDER->GetPlatformView()->DispatchPlatformMessage(
      env,                                         //
      fml::jni::JavaStringToString(env, channel),  //
      message,                                     //
      position,                                    //
      responseId                                   //
  );
}

static void DispatchEmptyPlatformMessage(JNIEnv* env,
                                         jobject jcaller,
                                         jlong shell_holder,
                                         jstring channel,
                                         jint responseId) {
  ANDROID_SHELL_HOLDER->GetPlatformView()->DispatchEmptyPlatformMessage(
      env,                                         //
      fml::jni::JavaStringToString(env, channel),  //
      responseId                                   //
  );
}

static void DispatchPointerDataPacket(JNIEnv* env,
                                      jobject jcaller,
                                      jlong shell_holder,
                                      jobject buffer,
                                      jint position) {
  uint8_t* data = static_cast<uint8_t*>(env->GetDirectBufferAddress(buffer));
  auto packet = std::make_unique<flutter::PointerDataPacket>(data, position);
  ANDROID_SHELL_HOLDER->GetPlatformView()->DispatchPointerDataPacket(
      std::move(packet));
}

static void DispatchSemanticsAction(JNIEnv* env,
                                    jobject jcaller,
                                    jlong shell_holder,
                                    jint id,
                                    jint action,
                                    jobject args,
                                    jint args_position) {
  ANDROID_SHELL_HOLDER->GetPlatformView()->DispatchSemanticsAction(
      env,           //
      id,            //
      action,        //
      args,          //
      args_position  //
  );
}

static void SetSemanticsEnabled(JNIEnv* env,
                                jobject jcaller,
                                jlong shell_holder,
                                jboolean enabled) {
  ANDROID_SHELL_HOLDER->GetPlatformView()->SetSemanticsEnabled(enabled);
}

static void SetAccessibilityFeatures(JNIEnv* env,
                                     jobject jcaller,
                                     jlong shell_holder,
                                     jint flags) {
  ANDROID_SHELL_HOLDER->GetPlatformView()->SetAccessibilityFeatures(flags);
}

static jboolean GetIsSoftwareRendering(JNIEnv* env, jobject jcaller) {
  return FlutterMain::Get().GetSettings().enable_software_rendering;
}

static void RegisterTexture(JNIEnv* env,
                            jobject jcaller,
                            jlong shell_holder,
                            jlong texture_id,
                            jobject surface_texture) {
  ANDROID_SHELL_HOLDER->GetPlatformView()->RegisterExternalTexture(
      static_cast<int64_t>(texture_id),                        //
      fml::jni::JavaObjectWeakGlobalRef(env, surface_texture)  //
  );
}

static void MarkTextureFrameAvailable(JNIEnv* env,
                                      jobject jcaller,
                                      jlong shell_holder,
                                      jlong texture_id) {
  ANDROID_SHELL_HOLDER->GetPlatformView()->MarkTextureFrameAvailable(
      static_cast<int64_t>(texture_id));
}

static void UnregisterTexture(JNIEnv* env,
                              jobject jcaller,
                              jlong shell_holder,
                              jlong texture_id) {
  ANDROID_SHELL_HOLDER->GetPlatformView()->UnregisterTexture(
      static_cast<int64_t>(texture_id));
}
/**
 * BD ADD: jni call to notify android  image load success
 */
static void ExternalImageLoadSuccess(JNIEnv *env,
        jobject jcaller,
        jstring key,
        jobject jBitmap) {
  auto cKey = fml::jni::JavaStringToString(env, key);
  auto loadContext = getImageLoadContextWithLock(cKey);
  eraseImageLoadContextWithLock(cKey);
  auto loaderContext = static_cast<ImageLoaderContext>(loadContext->loaderContext);
  if (!loaderContext.task_runners.IsValid()) {
    return;
  }
  auto globalJBitmap = env->NewGlobalRef(jBitmap);
  loaderContext.task_runners.GetIOTaskRunner()->PostTask([cKey,
                                                              globalJBitmap,
                                                              loadContext]() {
    if (loadContext == nullptr) {
      return;
    }
    JNIEnv *jniEnv = fml::jni::AttachCurrentThread();
    loadContext->onLoadSuccess(jniEnv, cKey, globalJBitmap);
  });
}

/**
 * BD ADD: jni call to notify android  image load success
 */
static void ExternalImageLoadForCodecSuccess(JNIEnv *env,
                                     jobject jcaller,
                                     jstring key,
                                     jobject jCodec) {
  auto cKey = fml::jni::JavaStringToString(env, key);
  auto loadContext = getImageLoadContextWithLock(cKey);
  if (loadContext == nullptr) {
    return;
  }
  loadContext->onLoadForCodecSuccess(env, cKey, env->NewGlobalRef(jCodec));

  auto loaderContext = static_cast<ImageLoaderContext>(loadContext->loaderContext);
  if (!loaderContext.task_runners.IsValid()) {
    return;
  }
  loaderContext.task_runners.GetIOTaskRunner()->PostTask(fml::MakeCopyable([cKey = std::move(cKey)](){
    eraseImageLoadContextWithLock(cKey);
  }));
}

static void ExternalImageLoadForGetNextFrameSuccess(JNIEnv *env,
                                     jobject jcaller,
                                     jstring key,
                                     jobject jBitmap) {
  auto cKey = fml::jni::JavaStringToString(env, key);
  auto loadContext = getImageLoadContextWithLock(cKey);
  if (loadContext == nullptr) {
    return;
  }
  loadContext->onGetNextFrameSuccess(env, cKey, env->NewGlobalRef(jBitmap));

  auto loaderContext = static_cast<ImageLoaderContext>(loadContext->loaderContext);
  if (!loaderContext.task_runners.IsValid()) {
    return;
  }
  loaderContext.task_runners.GetIOTaskRunner()->PostTask(fml::MakeCopyable([cKey = std::move(cKey)](){
    eraseImageLoadContextWithLock(cKey);
  }));
}

/**
 * BD ADD: jni call to notify android image load fail
 */
static void ExternalImageLoadFail(JNIEnv *env,
        jobject jcaller,
        jstring key) {
  auto cKey = fml::jni::JavaStringToString(env, key);
  auto loadContext = getImageLoadContextWithLock(cKey);
  eraseImageLoadContextWithLock(cKey);
  if (!loadContext) {
    return;
  }
  auto loaderContext = static_cast<ImageLoaderContext>(loadContext->loaderContext);
  if (!loaderContext.task_runners.IsValid()) {
    return;
  }
  loaderContext.task_runners.GetIOTaskRunner()->PostTask([env,
                                                              cKey,
                                                              loadContext]() {
    if (loadContext == nullptr) {
      return;
    }
    loadContext->onLoadFail(env, cKey);
  });
}


/**
 * BD ADD: jni call to notify android image load fail
 */
static void ExternalImageCodecLoadFail(JNIEnv *env,
                                  jobject jcaller,
                                  jstring key) {
  auto cKey = fml::jni::JavaStringToString(env, key);
  auto loadContext = getImageLoadContextWithLock(cKey);
  eraseImageLoadContextWithLock(cKey);
  if (!loadContext) {
    return;
  }

  auto loaderContext = static_cast<ImageLoaderContext>(loadContext->loaderContext);
  if (!loaderContext.task_runners.IsValid()) {
    return;
  }
  loaderContext.task_runners.GetIOTaskRunner()->PostTask([env,
                                                             cKey,
                                                             loadContext]{
    if (loadContext == nullptr) {
      return;
    }
    loadContext->onCodecLoadFail(env, cKey);
  });
}

static void ExternalImageGetNextFrameFail(JNIEnv *env,
                                  jobject jcaller,
                                  jstring key) {
  auto cKey = fml::jni::JavaStringToString(env, key);
  auto loadContext = getImageLoadContextWithLock(cKey);
  eraseImageLoadContextWithLock(cKey);
  if (!loadContext) {
    return;
  }
  auto loaderContext = static_cast<ImageLoaderContext>(loadContext->loaderContext);
  if (!loaderContext.task_runners.IsValid()) {
    return;
  }
  loaderContext.task_runners.GetIOTaskRunner()->PostTask([env,
                                                             cKey,
                                                             loadContext](){
    if (loadContext == nullptr) {
      return;
    }
    loadContext->onGetNextFrameFail(env, cKey);
  });
}

/**
 * BD ADD: register android image loader
 */
static void RegisterAndroidImageLoader(JNIEnv *env,
                            jobject jcaller,
                            jlong shell_holder,
                            jobject android_image_loader) {
  ANDROID_SHELL_HOLDER->GetPlatformView()->RegisterExternalImageLoader(
      fml::jni::JavaObjectWeakGlobalRef(env, android_image_loader)
      );
}
/**
 * BD ADD: unregister android image loader
 */
static void UnRegisterAndroidImageLoader(JNIEnv *env,
                                       jobject jcaller,
                                       jlong shell_holder) {

}

static void InvokePlatformMessageResponseCallback(JNIEnv* env,
                                                  jobject jcaller,
                                                  jlong shell_holder,
                                                  jint responseId,
                                                  jobject message,
                                                  jint position) {
  // BD MOD: START
//  ANDROID_SHELL_HOLDER->GetPlatformView()
//      ->InvokePlatformMessageResponseCallback(env,         //
//                                              responseId,  //
//                                              message,     //
//                                              position     //
//      );
  if (ANDROID_SHELL_HOLDER->GetMultiChannelEnabled()) {
    PlatformViewAndroid* view = ANDROID_SHELL_HOLDER->GetRawPlatformView();
    if (view) {
      view->InvokePlatformMessageResponseCallback(env,         //
                                                  responseId,  //
                                                  message,     //
                                                  position     //
      );
    }
  } else {
    ANDROID_SHELL_HOLDER->GetPlatformView()
        ->InvokePlatformMessageResponseCallback(env,         //
                                                responseId,  //
                                                message,     //
                                                position     //
        );
  }
  // END
}

static void InvokePlatformMessageEmptyResponseCallback(JNIEnv* env,
                                                       jobject jcaller,
                                                       jlong shell_holder,
                                                       jint responseId) {
  // BD MOD: START
//  ANDROID_SHELL_HOLDER->GetPlatformView()
//      ->InvokePlatformMessageEmptyResponseCallback(env,        //
//                                                   responseId  //
//      );
  if (ANDROID_SHELL_HOLDER->GetMultiChannelEnabled()) {
    PlatformViewAndroid* view = ANDROID_SHELL_HOLDER->GetRawPlatformView();
    if (view) {
      view->InvokePlatformMessageEmptyResponseCallback(env, responseId);
    }
  } else {
    ANDROID_SHELL_HOLDER->GetPlatformView()
        ->InvokePlatformMessageEmptyResponseCallback(env,        //
                                                     responseId  //
        );

  }
  // END
}

static void NotifyLowMemoryWarning(JNIEnv* env,
                                   jobject obj,
                                   jlong shell_holder) {
  ANDROID_SHELL_HOLDER->NotifyLowMemoryWarning();
}

static jboolean FlutterTextUtilsIsEmoji(JNIEnv* env,
                                        jobject obj,
                                        jint codePoint) {
  return u_hasBinaryProperty(codePoint, UProperty::UCHAR_EMOJI);
}

static jboolean FlutterTextUtilsIsEmojiModifier(JNIEnv* env,
                                                jobject obj,
                                                jint codePoint) {
  return u_hasBinaryProperty(codePoint, UProperty::UCHAR_EMOJI_MODIFIER);
}

static jboolean FlutterTextUtilsIsEmojiModifierBase(JNIEnv* env,
                                                    jobject obj,
                                                    jint codePoint) {
  return u_hasBinaryProperty(codePoint, UProperty::UCHAR_EMOJI_MODIFIER_BASE);
}

static jboolean FlutterTextUtilsIsVariationSelector(JNIEnv* env,
                                                    jobject obj,
                                                    jint codePoint) {
  return u_hasBinaryProperty(codePoint, UProperty::UCHAR_VARIATION_SELECTOR);
}

static jboolean FlutterTextUtilsIsRegionalIndicator(JNIEnv* env,
                                                    jobject obj,
                                                    jint codePoint) {
  return u_hasBinaryProperty(codePoint, UProperty::UCHAR_REGIONAL_INDICATOR);
}

static void LoadLoadingUnitFailure(intptr_t loading_unit_id,
                                   std::string message,
                                   bool transient) {
  // TODO(garyq): Implement
}

static void DeferredComponentInstallFailure(JNIEnv* env,
                                            jobject obj,
                                            jint jLoadingUnitId,
                                            jstring jError,
                                            jboolean jTransient) {
  LoadLoadingUnitFailure(static_cast<intptr_t>(jLoadingUnitId),
                         fml::jni::JavaStringToString(env, jError),
                         static_cast<bool>(jTransient));
}

static void LoadDartDeferredLibrary(JNIEnv* env,
                                    jobject obj,
                                    jlong shell_holder,
                                    jint jLoadingUnitId,
                                    jobjectArray jSearchPaths) {
  // Convert java->c++
  intptr_t loading_unit_id = static_cast<intptr_t>(jLoadingUnitId);
  std::vector<std::string> search_paths =
      fml::jni::StringArrayToVector(env, jSearchPaths);

  // Use dlopen here to directly check if handle is nullptr before creating a
  // NativeLibrary.
  void* handle = nullptr;
  while (handle == nullptr && !search_paths.empty()) {
    std::string path = search_paths.back();
    handle = ::dlopen(path.c_str(), RTLD_NOW);
    search_paths.pop_back();
  }
  if (handle == nullptr) {
    LoadLoadingUnitFailure(loading_unit_id,
                           "No lib .so found for provided search paths.", true);
    return;
  }
  fml::RefPtr<fml::NativeLibrary> native_lib =
      fml::NativeLibrary::CreateWithHandle(handle, false);

  // Resolve symbols.
  std::unique_ptr<const fml::SymbolMapping> data_mapping =
      std::make_unique<const fml::SymbolMapping>(
          native_lib, DartSnapshot::kIsolateDataSymbol);
  std::unique_ptr<const fml::SymbolMapping> instructions_mapping =
      std::make_unique<const fml::SymbolMapping>(
          native_lib, DartSnapshot::kIsolateInstructionsSymbol);

  ANDROID_SHELL_HOLDER->GetPlatformView()->LoadDartDeferredLibrary(
      loading_unit_id, std::move(data_mapping),
      std::move(instructions_mapping));
}

static void UpdateJavaAssetManager(JNIEnv* env,
                                   jobject obj,
                                   jlong shell_holder,
                                   jobject jAssetManager,
                                   jstring jAssetBundlePath) {
  auto asset_resolver = std::make_unique<flutter::APKAssetProvider>(
      env,                                                   // jni environment
      jAssetManager,                                         // asset manager
      fml::jni::JavaStringToString(env, jAssetBundlePath));  // apk asset dir

  ANDROID_SHELL_HOLDER->GetPlatformView()->UpdateAssetResolverByType(
      std::move(asset_resolver),
      AssetResolver::AssetResolverType::kApkAssetProvider);
}

bool RegisterApi(JNIEnv* env) {
  static const JNINativeMethod flutter_jni_methods[] = {
      // Start of methods from FlutterJNI
      {
          .name = "nativeAttach",
          // BD MOD:
          // .signature = "(Lio/flutter/embedding/engine/FlutterJNI;Z)J",
          .signature = "(Lio/flutter/embedding/engine/FlutterJNI;ZI)J",
          .fnPtr = reinterpret_cast<void*>(&AttachJNI),
      },
      // BD ADD: START
      {
          .name = "nativeScheduleBackgroundFrame",
          .signature = "(J)V",
          .fnPtr = reinterpret_cast<void*>(&ScheduleBackgroundFrame),
      },
            {
          .name = "nativeScheduleFrameNow",
          .signature = "(J)V",
          .fnPtr = reinterpret_cast<void*>(&ScheduleFrameNow),
      },
      // END
      {
          .name = "nativeDestroy",
          .signature = "(J)V",
          .fnPtr = reinterpret_cast<void*>(&DestroyJNI),
      },
      {
          .name = "nativeSpawn",
          .signature = "(JLjava/lang/String;Ljava/lang/String;)Lio/flutter/"
                       "embedding/engine/FlutterJNI;",
          .fnPtr = reinterpret_cast<void*>(&SpawnJNI),
      },
      {
          .name = "nativeRunBundleAndSnapshotFromLibrary",
          .signature = "(JLjava/lang/String;Ljava/lang/String;"
                       "Ljava/lang/String;Landroid/content/res/AssetManager;)V",
          .fnPtr = reinterpret_cast<void*>(&RunBundleAndSnapshotFromLibrary),
      },
      {
          .name = "nativeDispatchEmptyPlatformMessage",
          .signature = "(JLjava/lang/String;I)V",
          .fnPtr = reinterpret_cast<void*>(&DispatchEmptyPlatformMessage),
      },
      {
          .name = "nativeDispatchPlatformMessage",
          .signature = "(JLjava/lang/String;Ljava/nio/ByteBuffer;II)V",
          .fnPtr = reinterpret_cast<void*>(&DispatchPlatformMessage),
      },
      {
          .name = "nativeInvokePlatformMessageResponseCallback",
          .signature = "(JILjava/nio/ByteBuffer;I)V",
          .fnPtr =
              reinterpret_cast<void*>(&InvokePlatformMessageResponseCallback),
      },
      {
          .name = "nativeInvokePlatformMessageEmptyResponseCallback",
          .signature = "(JI)V",
          .fnPtr = reinterpret_cast<void*>(
              &InvokePlatformMessageEmptyResponseCallback),
      },
      {
          .name = "nativeNotifyLowMemoryWarning",
          .signature = "(J)V",
          .fnPtr = reinterpret_cast<void*>(&NotifyLowMemoryWarning),
      },

      // Start of methods from FlutterView
      {
          .name = "nativeGetBitmap",
          .signature = "(J)Landroid/graphics/Bitmap;",
          .fnPtr = reinterpret_cast<void*>(&GetBitmap),
      },
      {
          .name = "nativeSurfaceCreated",
          .signature = "(JLandroid/view/Surface;)V",
          .fnPtr = reinterpret_cast<void*>(&SurfaceCreated),
      },
      {
          .name = "nativeSurfaceWindowChanged",
          .signature = "(JLandroid/view/Surface;)V",
          .fnPtr = reinterpret_cast<void*>(&SurfaceWindowChanged),
      },
      {
          .name = "nativeSurfaceChanged",
          .signature = "(JII)V",
          .fnPtr = reinterpret_cast<void*>(&SurfaceChanged),
      },
      {
          .name = "nativeSurfaceDestroyed",
          .signature = "(J)V",
          .fnPtr = reinterpret_cast<void*>(&SurfaceDestroyed),
      },
      {
          .name = "nativeSetViewportMetrics",
          .signature = "(JFIIIIIIIIIIIIII)V",
          .fnPtr = reinterpret_cast<void*>(&SetViewportMetrics),
      },
      {
          .name = "nativeDispatchPointerDataPacket",
          .signature = "(JLjava/nio/ByteBuffer;I)V",
          .fnPtr = reinterpret_cast<void*>(&DispatchPointerDataPacket),
      },
      {
          .name = "nativeDispatchSemanticsAction",
          .signature = "(JIILjava/nio/ByteBuffer;I)V",
          .fnPtr = reinterpret_cast<void*>(&DispatchSemanticsAction),
      },
      {
          .name = "nativeSetSemanticsEnabled",
          .signature = "(JZ)V",
          .fnPtr = reinterpret_cast<void*>(&SetSemanticsEnabled),
      },
      {
          .name = "nativeSetAccessibilityFeatures",
          .signature = "(JI)V",
          .fnPtr = reinterpret_cast<void*>(&SetAccessibilityFeatures),
      },
      {
          .name = "nativeGetIsSoftwareRenderingEnabled",
          .signature = "()Z",
          .fnPtr = reinterpret_cast<void*>(&GetIsSoftwareRendering),
      },
      {
          .name = "nativeRegisterTexture",
          .signature = "(JJLio/flutter/embedding/engine/renderer/"
                       "SurfaceTextureWrapper;)V",
          .fnPtr = reinterpret_cast<void*>(&RegisterTexture),
      },
      {
          .name = "nativeMarkTextureFrameAvailable",
          .signature = "(JJ)V",
          .fnPtr = reinterpret_cast<void*>(&MarkTextureFrameAvailable),
      },
      {
          .name = "nativeUnregisterTexture",
          .signature = "(JJ)V",
          .fnPtr = reinterpret_cast<void*>(&UnregisterTexture),
      },

      // Methods for Dart callback functionality.
      {
          .name = "nativeLookupCallbackInformation",
          .signature = "(J)Lio/flutter/view/FlutterCallbackInformation;",
          .fnPtr = reinterpret_cast<void*>(&LookupCallbackInformation),
      },

      // Start of methods for FlutterTextUtils
      {
          .name = "nativeFlutterTextUtilsIsEmoji",
          .signature = "(I)Z",
          .fnPtr = reinterpret_cast<void*>(&FlutterTextUtilsIsEmoji),
      },
      {
          .name = "nativeFlutterTextUtilsIsEmojiModifier",
          .signature = "(I)Z",
          .fnPtr = reinterpret_cast<void*>(&FlutterTextUtilsIsEmojiModifier),
      },
      {
          .name = "nativeFlutterTextUtilsIsEmojiModifierBase",
          .signature = "(I)Z",
          .fnPtr =
              reinterpret_cast<void*>(&FlutterTextUtilsIsEmojiModifierBase),
      },
      {
          .name = "nativeFlutterTextUtilsIsVariationSelector",
          .signature = "(I)Z",
          .fnPtr =
              reinterpret_cast<void*>(&FlutterTextUtilsIsVariationSelector),
      },
      {
          .name = "nativeFlutterTextUtilsIsRegionalIndicator",
          .signature = "(I)Z",
          .fnPtr =
              reinterpret_cast<void*>(&FlutterTextUtilsIsRegionalIndicator),
      },
      {
          .name = "nativeLoadDartDeferredLibrary",
          .signature = "(JI[Ljava/lang/String;)V",
          .fnPtr = reinterpret_cast<void*>(&LoadDartDeferredLibrary),
      },
      {
          .name = "nativeUpdateJavaAssetManager",
          .signature =
              "(JLandroid/content/res/AssetManager;Ljava/lang/String;)V",
          .fnPtr = reinterpret_cast<void*>(&UpdateJavaAssetManager),
      },
      {
          .name = "nativeDeferredComponentInstallFailure",
          .signature = "(ILjava/lang/String;Z)V",
          .fnPtr = reinterpret_cast<void*>(&DeferredComponentInstallFailure),
       },
	  // BD ADD: add for register android image loader
      {
          .name = "nativeRegisterAndroidImageLoader",
          .signature = "(JLio/flutter/view/AndroidImageLoader;)V",
          .fnPtr = reinterpret_cast<void*>(&RegisterAndroidImageLoader),
      },
      // BD ADD: add for unregister android image loader HuWeijie
      {
          .name = "nativeUnregisterAndroidImageLoader",
          .signature = "(J)V",
          .fnPtr = reinterpret_cast<void*>(&UnRegisterAndroidImageLoader),
      },
  };

  if (env->RegisterNatives(g_flutter_jni_class->obj(), flutter_jni_methods,
                           fml::size(flutter_jni_methods)) != 0) {
    FML_LOG(ERROR) << "Failed to RegisterNatives with FlutterJNI";
    return false;
  }

  g_jni_shell_holder_field = env->GetFieldID(
      g_flutter_jni_class->obj(), "nativeShellHolderId", "Ljava/lang/Long;");

  if (g_jni_shell_holder_field == nullptr) {
    FML_LOG(ERROR) << "Could not locate FlutterJNI's nativeShellHolderId field";
    return false;
  }

  g_jni_constructor =
      env->GetMethodID(g_flutter_jni_class->obj(), "<init>", "()V");

  if (g_jni_constructor == nullptr) {
    FML_LOG(ERROR) << "Could not locate FlutterJNI's constructor";
    return false;
  }

  g_long_constructor = env->GetStaticMethodID(g_java_long_class->obj(),
                                              "valueOf", "(J)Ljava/lang/Long;");
  if (g_long_constructor == nullptr) {
    FML_LOG(ERROR) << "Could not locate Long's constructor";
    return false;
  }

  g_handle_platform_message_method =
      env->GetMethodID(g_flutter_jni_class->obj(), "handlePlatformMessage",
                       "(Ljava/lang/String;[BI)V");

  if (g_handle_platform_message_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate handlePlatformMessage method";
    return false;
  }

  g_handle_platform_message_response_method = env->GetMethodID(
      g_flutter_jni_class->obj(), "handlePlatformMessageResponse", "(I[B)V");

  if (g_handle_platform_message_response_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate handlePlatformMessageResponse method";
    return false;
  }

  g_update_semantics_method =
      env->GetMethodID(g_flutter_jni_class->obj(), "updateSemantics",
                       "(Ljava/nio/ByteBuffer;[Ljava/lang/String;)V");

  if (g_update_semantics_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate updateSemantics method";
    return false;
  }

  g_update_custom_accessibility_actions_method = env->GetMethodID(
      g_flutter_jni_class->obj(), "updateCustomAccessibilityActions",
      "(Ljava/nio/ByteBuffer;[Ljava/lang/String;)V");

  if (g_update_custom_accessibility_actions_method == nullptr) {
    FML_LOG(ERROR)
        << "Could not locate updateCustomAccessibilityActions method";
    return false;
  }

  g_on_first_frame_method =
      env->GetMethodID(g_flutter_jni_class->obj(), "onFirstFrame", "()V");

  if (g_on_first_frame_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate onFirstFrame method";
    return false;
  }

  g_on_engine_restart_method =
      env->GetMethodID(g_flutter_jni_class->obj(), "onPreEngineRestart", "()V");

  if (g_on_engine_restart_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate onEngineRestart method";
    return false;
  }

  g_create_overlay_surface_method =
      env->GetMethodID(g_flutter_jni_class->obj(), "createOverlaySurface",
                       "()Lio/flutter/embedding/engine/FlutterOverlaySurface;");

  if (g_create_overlay_surface_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate createOverlaySurface method";
    return false;
  }

  g_destroy_overlay_surfaces_method = env->GetMethodID(
      g_flutter_jni_class->obj(), "destroyOverlaySurfaces", "()V");

  if (g_destroy_overlay_surfaces_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate destroyOverlaySurfaces method";
    return false;
  }

  fml::jni::ScopedJavaLocalRef<jclass> overlay_surface_class(
      env, env->FindClass("io/flutter/embedding/engine/FlutterOverlaySurface"));
  if (overlay_surface_class.is_null()) {
    FML_LOG(ERROR) << "Could not locate FlutterOverlaySurface class";
    return false;
  }
  g_overlay_surface_id_method =
      env->GetMethodID(overlay_surface_class.obj(), "getId", "()I");
  if (g_overlay_surface_id_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate FlutterOverlaySurface#getId() method";
    return false;
  }
  g_overlay_surface_surface_method = env->GetMethodID(
      overlay_surface_class.obj(), "getSurface", "()Landroid/view/Surface;");
  if (g_overlay_surface_surface_method == nullptr) {
    FML_LOG(ERROR)
        << "Could not locate FlutterOverlaySurface#getSurface() method";
    return false;
  }

  return true;
}
// BD ADD: START
static void ExternalImageLoadSuccess(JNIEnv *env,
        jobject jcaller,
        jstring key,
        jobject jBitmap);

static void ExternalImageLoadForCodecSuccess(JNIEnv *env,
        jobject jcaller,
        jstring key,
        jobject jCodec);

static void ExternalImageLoadForGetNextFrameSuccess(JNIEnv *env,
                                             jobject jcaller,
                                             jstring key,
                                             jobject jCodec);

static void ExternalImageLoadFail(JNIEnv *env,
        jobject jcaller,
        jstring key);

static void ExternalImageCodecLoadFail(JNIEnv *env,
                                  jobject jcaller,
                                  jstring key);

static void ExternalImageGetNextFrameFail(JNIEnv *env,
                                       jobject jcaller,
                                       jstring key);

// END
bool PlatformViewAndroid::Register(JNIEnv* env) {
  if (env == nullptr) {
    FML_LOG(ERROR) << "No JNIEnv provided";
    return false;
  }

  g_flutter_callback_info_class = new fml::jni::ScopedJavaGlobalRef<jclass>(
      env, env->FindClass("io/flutter/view/FlutterCallbackInformation"));
  if (g_flutter_callback_info_class->is_null()) {
    FML_LOG(ERROR) << "Could not locate FlutterCallbackInformation class";
    return false;
  }

  g_flutter_callback_info_constructor = env->GetMethodID(
      g_flutter_callback_info_class->obj(), "<init>",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  if (g_flutter_callback_info_constructor == nullptr) {
    FML_LOG(ERROR) << "Could not locate FlutterCallbackInformation constructor";
    return false;
  }

  g_flutter_jni_class = new fml::jni::ScopedJavaGlobalRef<jclass>(
      env, env->FindClass("io/flutter/embedding/engine/FlutterJNI"));
  if (g_flutter_jni_class->is_null()) {
    FML_LOG(ERROR) << "Failed to find FlutterJNI Class.";
    return false;
  }

  g_mutators_stack_class = new fml::jni::ScopedJavaGlobalRef<jclass>(
      env,
      env->FindClass(
          "io/flutter/embedding/engine/mutatorsstack/FlutterMutatorsStack"));
  if (g_mutators_stack_class == nullptr) {
    FML_LOG(ERROR) << "Could not locate FlutterMutatorsStack";
    return false;
  }

  g_mutators_stack_init_method =
      env->GetMethodID(g_mutators_stack_class->obj(), "<init>", "()V");
  if (g_mutators_stack_init_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate FlutterMutatorsStack.init method";
    return false;
  }

  g_mutators_stack_push_transform_method =
      env->GetMethodID(g_mutators_stack_class->obj(), "pushTransform", "([F)V");
  if (g_mutators_stack_push_transform_method == nullptr) {
    FML_LOG(ERROR)
        << "Could not locate FlutterMutatorsStack.pushTransform method";
    return false;
  }

  g_mutators_stack_push_cliprect_method = env->GetMethodID(
      g_mutators_stack_class->obj(), "pushClipRect", "(IIII)V");
  if (g_mutators_stack_push_cliprect_method == nullptr) {
    FML_LOG(ERROR)
        << "Could not locate FlutterMutatorsStack.pushClipRect method";
    return false;
  }

  g_mutators_stack_push_cliprrect_method = env->GetMethodID(
      g_mutators_stack_class->obj(), "pushClipRRect", "(IIII[F)V");
  if (g_mutators_stack_push_cliprect_method == nullptr) {
    FML_LOG(ERROR)
        << "Could not locate FlutterMutatorsStack.pushClipRRect method";
    return false;
  }

  g_on_display_platform_view_method =
      env->GetMethodID(g_flutter_jni_class->obj(), "onDisplayPlatformView",
                       "(IIIIIIILio/flutter/embedding/engine/mutatorsstack/"
                       "FlutterMutatorsStack;)V");

  if (g_on_display_platform_view_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate onDisplayPlatformView method";
    return false;
  }

  g_on_begin_frame_method =
      env->GetMethodID(g_flutter_jni_class->obj(), "onBeginFrame", "()V");

  if (g_on_begin_frame_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate onBeginFrame method";
    return false;
  }

  g_on_end_frame_method =
      env->GetMethodID(g_flutter_jni_class->obj(), "onEndFrame", "()V");

  if (g_on_end_frame_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate onEndFrame method";
    return false;
  }

  g_on_display_overlay_surface_method = env->GetMethodID(
      g_flutter_jni_class->obj(), "onDisplayOverlaySurface", "(IIIII)V");

  if (g_on_display_overlay_surface_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate onDisplayOverlaySurface method";
    return false;
  }

  g_texture_wrapper_class = new fml::jni::ScopedJavaGlobalRef<jclass>(
      env, env->FindClass(
               "io/flutter/embedding/engine/renderer/SurfaceTextureWrapper"));
  if (g_texture_wrapper_class->is_null()) {
    FML_LOG(ERROR) << "Could not locate SurfaceTextureWrapper class";
    return false;
  }

  // BD ADD: START
  g_image_loader_class = new fml::jni::ScopedJavaGlobalRef<jclass>(env, env->FindClass("io/flutter/view/AndroidImageLoader"));
  if (g_image_loader_class->is_null()) {
      FML_LOG(ERROR) << "Could not locate AndroidImageLoader class";
      return false;
  }

  g_image_loader_class_load = env->GetMethodID(g_image_loader_class->obj(), "load", "(Ljava/lang/String;IIFLio/flutter/view/NativeLoadCallback;Ljava/lang/String;)V");
  if (g_image_loader_class_load == nullptr) {
    FML_LOG(ERROR) << "Could not locate AndroidImageLoader load method";
    return false;
  }

  g_image_loader_class_load_gif = env->GetMethodID(g_image_loader_class->obj(), "getNextFrame", "(ILjava/lang/Object;Lio/flutter/view/NativeLoadCallback;Ljava/lang/String;)V");
  if (g_image_loader_class_load == nullptr) {
    FML_LOG(ERROR) << "Could not locate AndroidImageLoader load method";
    return false;
  }

  g_image_loader_class_release = env->GetMethodID(g_image_loader_class->obj(), "release", "(Ljava/lang/String;)V");
  if (g_image_loader_class_release == nullptr) {
    FML_LOG(ERROR) << "Could not locate AndroidImageLoader release method";
    return false;
  }

  g_image_loader_callback_class = new fml::jni::ScopedJavaGlobalRef<jclass>(env, env->FindClass("io/flutter/view/NativeLoadCallback"));
  if (g_image_loader_callback_class->is_null()) {
      FML_LOG(ERROR) << "Could not locate NativeLoadCallback class";
      return false;
  }

  g_native_callback_constructor = env->GetMethodID(g_image_loader_callback_class->obj(), "<init>", "()V");
  if (g_native_callback_constructor == nullptr) {
    FML_LOG(ERROR) << "Could not locate NativeLoadCallback constructor";
    return false;
  }

  static const JNINativeMethod native_load_callback_methods[] = {
          {
              .name = "nativeSuccessCallback",
              .signature = "(Ljava/lang/String;Landroid/graphics/Bitmap;)V",
              .fnPtr = reinterpret_cast<void*>(&ExternalImageLoadSuccess),
          },
          {
              .name = "nativeCodecSuccessCallback",
              .signature = "(Ljava/lang/String;Lio/flutter/view/NativeCodec;)V",
              .fnPtr = reinterpret_cast<void*>(&ExternalImageLoadForCodecSuccess),
          },
          {
              .name = "nativeGetNextFrameSuccessCallback",
              .signature = "(Ljava/lang/String;Landroid/graphics/Bitmap;)V",
              .fnPtr = reinterpret_cast<void*>(&ExternalImageLoadForGetNextFrameSuccess),
          },
          {
              .name = "nativeFailCallback",
              .signature = "(Ljava/lang/String;)V",
              .fnPtr = reinterpret_cast<void*>(&ExternalImageLoadFail),
          },
          {
              .name = "nativeCodecFailCallback",
              .signature = "(Ljava/lang/String;)V",
              .fnPtr = reinterpret_cast<void*>(&ExternalImageCodecLoadFail),
          },
          {
              .name = "nativeGetNextFrameFailCallback",
              .signature = "(Ljava/lang/String;)V",
              .fnPtr = reinterpret_cast<void*>(&ExternalImageGetNextFrameFail),
          },
  };

  if (env->RegisterNatives(g_image_loader_callback_class->obj(),
                           native_load_callback_methods,
                           fml::size(native_load_callback_methods)) != 0) {
      FML_LOG(ERROR) << "Failed to RegisterNatives with NativeLoadCallback";
      return false;
  }
  // END

  // 解决5.x手机上的crash
  fml::jni::ClearException(env);

  g_attach_to_gl_context_method = env->GetMethodID(
      g_texture_wrapper_class->obj(), "attachToGLContext", "(I)V");

  if (g_attach_to_gl_context_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate attachToGlContext method";
    return false;
  }

  g_update_tex_image_method =
      env->GetMethodID(g_texture_wrapper_class->obj(), "updateTexImage", "()V");

  if (g_update_tex_image_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate updateTexImage method";
    return false;
  }

  g_get_transform_matrix_method = env->GetMethodID(
      g_texture_wrapper_class->obj(), "getTransformMatrix", "([F)V");

  if (g_get_transform_matrix_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate getTransformMatrix method";
    return false;
  }

  g_detach_from_gl_context_method = env->GetMethodID(
      g_texture_wrapper_class->obj(), "detachFromGLContext", "()V");

  if (g_detach_from_gl_context_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate detachFromGlContext method";
    return false;
  }

  g_compute_platform_resolved_locale_method = env->GetMethodID(
      g_flutter_jni_class->obj(), "computePlatformResolvedLocale",
      "([Ljava/lang/String;)[Ljava/lang/String;");

  if (g_compute_platform_resolved_locale_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate computePlatformResolvedLocale method";
    return false;
  }

  g_request_dart_deferred_library_method = env->GetMethodID(
      g_flutter_jni_class->obj(), "requestDartDeferredLibrary", "(I)V");

  if (g_request_dart_deferred_library_method == nullptr) {
    FML_LOG(ERROR) << "Could not locate requestDartDeferredLibrary method";
    return false;
  }

  g_java_long_class = new fml::jni::ScopedJavaGlobalRef<jclass>(
      env, env->FindClass("java/lang/Long"));
  if (g_java_long_class->is_null()) {
    FML_LOG(ERROR) << "Could not locate java.lang.Long class";
    return false;
  }

  return RegisterApi(env);
}

PlatformViewAndroidJNIImpl::PlatformViewAndroidJNIImpl(
    fml::jni::JavaObjectWeakGlobalRef java_object)
    : java_object_(java_object) {}

PlatformViewAndroidJNIImpl::~PlatformViewAndroidJNIImpl() = default;

void PlatformViewAndroidJNIImpl::FlutterViewHandlePlatformMessage(
    fml::RefPtr<flutter::PlatformMessage> message,
    int responseId) {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    return;
  }

  fml::jni::ScopedJavaLocalRef<jstring> java_channel =
      fml::jni::StringToJavaString(env, message->channel());

  if (message->hasData()) {
    fml::jni::ScopedJavaLocalRef<jbyteArray> message_array(
        env, env->NewByteArray(message->data().size()));
    env->SetByteArrayRegion(
        message_array.obj(), 0, message->data().size(),
        reinterpret_cast<const jbyte*>(message->data().data()));
    env->CallVoidMethod(java_object.obj(), g_handle_platform_message_method,
                        java_channel.obj(), message_array.obj(), responseId);
  } else {
    env->CallVoidMethod(java_object.obj(), g_handle_platform_message_method,
                        java_channel.obj(), nullptr, responseId);
  }

  FML_CHECK(CheckException(env));
}

void PlatformViewAndroidJNIImpl::FlutterViewHandlePlatformMessageResponse(
    int responseId,
    std::unique_ptr<fml::Mapping> data) {
  // We are on the platform thread. Attempt to get the strong reference to
  // the Java object.
  JNIEnv* env = fml::jni::AttachCurrentThread();

  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    // The Java object was collected before this message response got to
    // it. Drop the response on the floor.
    return;
  }
  if (data == nullptr) {  // Empty response.
    env->CallVoidMethod(java_object.obj(),
                        g_handle_platform_message_response_method, responseId,
                        nullptr);
  } else {
    // Convert the vector to a Java byte array.
    fml::jni::ScopedJavaLocalRef<jbyteArray> data_array(
        env, env->NewByteArray(data->GetSize()));
    env->SetByteArrayRegion(data_array.obj(), 0, data->GetSize(),
                            reinterpret_cast<const jbyte*>(data->GetMapping()));

    env->CallVoidMethod(java_object.obj(),
                        g_handle_platform_message_response_method, responseId,
                        data_array.obj());
  }

  FML_CHECK(CheckException(env));
}

void PlatformViewAndroidJNIImpl::FlutterViewUpdateSemantics(
    std::vector<uint8_t> buffer,
    std::vector<std::string> strings) {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    return;
  }

  fml::jni::ScopedJavaLocalRef<jobject> direct_buffer(
      env, env->NewDirectByteBuffer(buffer.data(), buffer.size()));
  fml::jni::ScopedJavaLocalRef<jobjectArray> jstrings =
      fml::jni::VectorToStringArray(env, strings);

  env->CallVoidMethod(java_object.obj(), g_update_semantics_method,
                      direct_buffer.obj(), jstrings.obj());

  FML_CHECK(CheckException(env));
}

void PlatformViewAndroidJNIImpl::FlutterViewUpdateCustomAccessibilityActions(
    std::vector<uint8_t> actions_buffer,
    std::vector<std::string> strings) {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    return;
  }

  fml::jni::ScopedJavaLocalRef<jobject> direct_actions_buffer(
      env,
      env->NewDirectByteBuffer(actions_buffer.data(), actions_buffer.size()));

  fml::jni::ScopedJavaLocalRef<jobjectArray> jstrings =
      fml::jni::VectorToStringArray(env, strings);

  env->CallVoidMethod(java_object.obj(),
                      g_update_custom_accessibility_actions_method,
                      direct_actions_buffer.obj(), jstrings.obj());

  FML_CHECK(CheckException(env));
}

void PlatformViewAndroidJNIImpl::FlutterViewOnFirstFrame() {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    return;
  }

  env->CallVoidMethod(java_object.obj(), g_on_first_frame_method);

  FML_CHECK(CheckException(env));
}

void PlatformViewAndroidJNIImpl::FlutterViewOnPreEngineRestart() {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    return;
  }

  env->CallVoidMethod(java_object.obj(), g_on_engine_restart_method);

  FML_CHECK(CheckException(env));
}

void PlatformViewAndroidJNIImpl::SurfaceTextureAttachToGLContext(
    JavaWeakGlobalRef surface_texture,
    int textureId) {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  fml::jni::ScopedJavaLocalRef<jobject> surface_texture_local_ref =
      surface_texture.get(env);
  if (surface_texture_local_ref.is_null()) {
    return;
  }

  env->CallVoidMethod(surface_texture_local_ref.obj(),
                      g_attach_to_gl_context_method, textureId);

  FML_CHECK(CheckException(env));
}

void PlatformViewAndroidJNIImpl::SurfaceTextureUpdateTexImage(
    JavaWeakGlobalRef surface_texture) {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  fml::jni::ScopedJavaLocalRef<jobject> surface_texture_local_ref =
      surface_texture.get(env);
  if (surface_texture_local_ref.is_null()) {
    return;
  }

  env->CallVoidMethod(surface_texture_local_ref.obj(),
                      g_update_tex_image_method);

  FML_CHECK(CheckException(env));
}

// The bounds we set for the canvas are post composition.
// To fill the canvas we need to ensure that the transformation matrix
// on the `SurfaceTexture` will be scaled to fill. We rescale and preseve
// the scaled aspect ratio.
SkSize ScaleToFill(float scaleX, float scaleY) {
  const double epsilon = std::numeric_limits<double>::epsilon();
  // scaleY is negative.
  const double minScale = fmin(scaleX, fabs(scaleY));
  const double rescale = 1.0f / (minScale + epsilon);
  return SkSize::Make(scaleX * rescale, scaleY * rescale);
}

void PlatformViewAndroidJNIImpl::SurfaceTextureGetTransformMatrix(
    JavaWeakGlobalRef surface_texture,
    SkMatrix& transform) {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  fml::jni::ScopedJavaLocalRef<jobject> surface_texture_local_ref =
      surface_texture.get(env);
  if (surface_texture_local_ref.is_null()) {
    return;
  }

  fml::jni::ScopedJavaLocalRef<jfloatArray> transformMatrix(
      env, env->NewFloatArray(16));

  env->CallVoidMethod(surface_texture_local_ref.obj(),
                      g_get_transform_matrix_method, transformMatrix.obj());
  FML_CHECK(CheckException(env));

  float* m = env->GetFloatArrayElements(transformMatrix.obj(), nullptr);
  float scaleX = m[0], scaleY = m[5];
  const SkSize scaled = ScaleToFill(scaleX, scaleY);
  SkScalar matrix3[] = {
      scaled.fWidth, m[1],           m[2],   //
      m[4],          scaled.fHeight, m[6],   //
      m[8],          m[9],           m[10],  //
  };
  env->ReleaseFloatArrayElements(transformMatrix.obj(), m, JNI_ABORT);
  transform.set9(matrix3);
}

void PlatformViewAndroidJNIImpl::SurfaceTextureDetachFromGLContext(
    JavaWeakGlobalRef surface_texture) {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  fml::jni::ScopedJavaLocalRef<jobject> surface_texture_local_ref =
      surface_texture.get(env);
  if (surface_texture_local_ref.is_null()) {
    return;
  }

  env->CallVoidMethod(surface_texture_local_ref.obj(),
                      g_detach_from_gl_context_method);

  FML_CHECK(CheckException(env));
}

void PlatformViewAndroidJNIImpl::FlutterViewOnDisplayPlatformView(
    int view_id,
    int x,
    int y,
    int width,
    int height,
    int viewWidth,
    int viewHeight,
    MutatorsStack mutators_stack) {
  JNIEnv* env = fml::jni::AttachCurrentThread();
  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    return;
  }

  jobject mutatorsStack = env->NewObject(g_mutators_stack_class->obj(),
                                         g_mutators_stack_init_method);

  std::vector<std::shared_ptr<Mutator>>::const_iterator iter =
      mutators_stack.Begin();
  while (iter != mutators_stack.End()) {
    switch ((*iter)->GetType()) {
      case transform: {
        const SkMatrix& matrix = (*iter)->GetMatrix();
        SkScalar matrix_array[9];
        matrix.get9(matrix_array);
        fml::jni::ScopedJavaLocalRef<jfloatArray> transformMatrix(
            env, env->NewFloatArray(9));

        env->SetFloatArrayRegion(transformMatrix.obj(), 0, 9, matrix_array);
        env->CallVoidMethod(mutatorsStack,
                            g_mutators_stack_push_transform_method,
                            transformMatrix.obj());
        break;
      }
      case clip_rect: {
        const SkRect& rect = (*iter)->GetRect();
        env->CallVoidMethod(
            mutatorsStack, g_mutators_stack_push_cliprect_method,
            static_cast<int>(rect.left()), static_cast<int>(rect.top()),
            static_cast<int>(rect.right()), static_cast<int>(rect.bottom()));
        break;
      }
      case clip_rrect: {
        const SkRRect& rrect = (*iter)->GetRRect();
        const SkRect& rect = rrect.rect();
        const SkVector& upper_left = rrect.radii(SkRRect::kUpperLeft_Corner);
        const SkVector& upper_right = rrect.radii(SkRRect::kUpperRight_Corner);
        const SkVector& lower_right = rrect.radii(SkRRect::kLowerRight_Corner);
        const SkVector& lower_left = rrect.radii(SkRRect::kLowerLeft_Corner);
        SkScalar radiis[8] = {
            upper_left.x(),  upper_left.y(),  upper_right.x(), upper_right.y(),
            lower_right.x(), lower_right.y(), lower_left.x(),  lower_left.y(),
        };
        fml::jni::ScopedJavaLocalRef<jfloatArray> radiisArray(
            env, env->NewFloatArray(8));
        env->SetFloatArrayRegion(radiisArray.obj(), 0, 8, radiis);
        env->CallVoidMethod(
            mutatorsStack, g_mutators_stack_push_cliprrect_method,
            (int)rect.left(), (int)rect.top(), (int)rect.right(),
            (int)rect.bottom(), radiisArray.obj());
        break;
      }
      // TODO(cyanglaz): Implement other mutators.
      // https://github.com/flutter/flutter/issues/58426
      case clip_path:
      case opacity:
        break;
    }
    ++iter;
  }

  env->CallVoidMethod(java_object.obj(), g_on_display_platform_view_method,
                      view_id, x, y, width, height, viewWidth, viewHeight,
                      mutatorsStack);

  FML_CHECK(CheckException(env));
}

void PlatformViewAndroidJNIImpl::FlutterViewDisplayOverlaySurface(
    int surface_id,
    int x,
    int y,
    int width,
    int height) {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    return;
  }

  env->CallVoidMethod(java_object.obj(), g_on_display_overlay_surface_method,
                      surface_id, x, y, width, height);

  FML_CHECK(CheckException(env));
}

void PlatformViewAndroidJNIImpl::FlutterViewBeginFrame() {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    return;
  }

  env->CallVoidMethod(java_object.obj(), g_on_begin_frame_method);

  FML_CHECK(CheckException(env));
}

void PlatformViewAndroidJNIImpl::FlutterViewEndFrame() {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    return;
  }

  env->CallVoidMethod(java_object.obj(), g_on_end_frame_method);

  FML_CHECK(CheckException(env));
}

std::unique_ptr<PlatformViewAndroidJNI::OverlayMetadata>
PlatformViewAndroidJNIImpl::FlutterViewCreateOverlaySurface() {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    return nullptr;
  }

  fml::jni::ScopedJavaLocalRef<jobject> overlay(
      env, env->CallObjectMethod(java_object.obj(),
                                 g_create_overlay_surface_method));
  FML_CHECK(CheckException(env));

  if (overlay.is_null()) {
    return std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(0,
                                                                     nullptr);
  }

  jint overlay_id =
      env->CallIntMethod(overlay.obj(), g_overlay_surface_id_method);

  jobject overlay_surface =
      env->CallObjectMethod(overlay.obj(), g_overlay_surface_surface_method);

  auto overlay_window = fml::MakeRefCounted<AndroidNativeWindow>(
      ANativeWindow_fromSurface(env, overlay_surface));

  return std::make_unique<PlatformViewAndroidJNI::OverlayMetadata>(
      overlay_id, std::move(overlay_window));
}

void PlatformViewAndroidJNIImpl::FlutterViewDestroyOverlaySurfaces() {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    return;
  }

  env->CallVoidMethod(java_object.obj(), g_destroy_overlay_surfaces_method);

  FML_CHECK(CheckException(env));
}

std::unique_ptr<std::vector<std::string>>
PlatformViewAndroidJNIImpl::FlutterViewComputePlatformResolvedLocale(
    std::vector<std::string> supported_locales_data) {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  std::unique_ptr<std::vector<std::string>> out =
      std::make_unique<std::vector<std::string>>();

  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    return out;
  }
  fml::jni::ScopedJavaLocalRef<jobjectArray> j_locales_data =
      fml::jni::VectorToStringArray(env, supported_locales_data);
  jobjectArray result = static_cast<jobjectArray>(env->CallObjectMethod(
      java_object.obj(), g_compute_platform_resolved_locale_method,
      j_locales_data.obj()));

  FML_CHECK(CheckException(env));

  int length = env->GetArrayLength(result);
  for (int i = 0; i < length; i++) {
    out->emplace_back(fml::jni::JavaStringToString(
        env, static_cast<jstring>(env->GetObjectArrayElement(result, i))));
  }
  return out;
}

double PlatformViewAndroidJNIImpl::GetDisplayRefreshRate() {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    return kUnknownDisplayRefreshRate;
  }

  jclass clazz = env->GetObjectClass(java_object.obj());
  if (clazz == nullptr) {
    return kUnknownDisplayRefreshRate;
  }

  jfieldID fid = env->GetStaticFieldID(clazz, "refreshRateFPS", "F");
  return static_cast<double>(env->GetStaticFloatField(clazz, fid));
}

bool PlatformViewAndroidJNIImpl::RequestDartDeferredLibrary(
    int loading_unit_id) {
  JNIEnv* env = fml::jni::AttachCurrentThread();

  auto java_object = java_object_.get(env);
  if (java_object.is_null()) {
    return true;
  }

  env->CallVoidMethod(java_object.obj(), g_request_dart_deferred_library_method,
                      loading_unit_id);

  FML_CHECK(CheckException(env));
  return true;
}

}  // namespace flutter
