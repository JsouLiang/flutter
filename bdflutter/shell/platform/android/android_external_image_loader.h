//
// Created by jay on 2019-07-31.
//

#ifndef FLUTTER_ANDROID_EXTERNAL_IMAGE_LOADER_H
#define FLUTTER_ANDROID_EXTERNAL_IMAGE_LOADER_H

#include "flutter/bdflutter/lib/ui/painting/image_loader.h"
#include "flutter/fml/macros.h"
#include "flutter/fml/platform/android/jni_weak_ref.h"
/**
 * BD ADD: android image loader
 */
namespace flutter {
class AndroidExternalImageLoader : public flutter::ImageLoader {
 public:
  AndroidExternalImageLoader(const fml::jni::JavaObjectWeakGlobalRef& android_image_loader);
  ~AndroidExternalImageLoader() override;

  void Load(const std::string url,
            const int width,
            const int height,
            const float scale,
            ImageLoaderContext contextPtr,
            std::function<void(sk_sp<SkImage> image)> callback) override;

  void LoadCodec(const std::string url, const int width, const int height, const float scale, ImageLoaderContext contextPtr, std::function<void(std::unique_ptr<NativeExportCodec> codec)> callback) override;

  void GetNextFrame(ImageLoaderContext contextPtr, int currentFrame, std::shared_ptr<NativeExportCodec> codec, std::function<void(sk_sp<SkImage>)> callback) override;
 private:
  fml::jni::JavaObjectWeakGlobalRef android_image_loader_;
  FML_DISALLOW_COPY_AND_ASSIGN(AndroidExternalImageLoader);
};
}


#endif //FLUTTER_ANDROID_EXTERNAL_IMAGE_LOADER_H
