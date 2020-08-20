//
// Created by jay on 2019-07-31.
//

#include "android_external_image_loader.h"
#include "platform_view_android_jni.h"
#include "flutter/fml/macros.h"
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

  void AndroidExternalImageLoader::LoadCodec(const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(std::unique_ptr<NativeExportCodec> codec)> callback) {
    JNIEnv* env = fml::jni::AttachCurrentThread();
    fml::jni::ScopedJavaLocalRef<jobject> imageLoader = android_image_loader_.get(env);
    if (imageLoader.is_null())
      return;
    CallJavaImageLoaderForCodec(imageLoader.obj(), url, width, height, scale, loaderContext, std::move(callback));
  }

  void AndroidExternalImageLoader::GetNextFrame(ImageLoaderContext loaderContext, int currentFrame, std::shared_ptr<NativeExportCodec> codec, std::function<void(sk_sp<SkImage>)> callback) {
    JNIEnv* env = fml::jni::AttachCurrentThread();
    fml::jni::ScopedJavaLocalRef<jobject> imageLoader = android_image_loader_.get(env);
    if (imageLoader.is_null())
      return;

    CallJavaImageLoaderGetNextFrame(imageLoader.obj(), loaderContext, currentFrame, std::move(codec), std::move(callback));
  }
}
