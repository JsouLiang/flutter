//
// Created by jay on 2019-07-31.
//

#include "flutter/bdflutter/shell/platform/android/android_external_image_loader.h"
#include "flutter/shell/platform/android/platform_view_android_jni_impl.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/platform/android/jni_util.h"
/**
 * BD ADD: android image loader
 */
namespace flutter {
  AndroidExternalImageLoader::AndroidExternalImageLoader(
    const fml::jni::JavaObjectWeakGlobalRef &android_image_loader):android_image_loader_(android_image_loader) {}
  AndroidExternalImageLoader::~AndroidExternalImageLoader() {}

  void AndroidExternalImageLoader::Load(const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(sk_sp<SkImage> image)> callback) {
    JNIEnv* env = fml::jni::AttachCurrentThread();
    fml::jni::ScopedJavaLocalRef<jobject> imageLoader = android_image_loader_.get(env);
    if (imageLoader.is_null())
      return;
    CallJavaImageLoader(imageLoader.obj(), url, width, height, scale, loaderContext, std::move(callback));
  }
}
