#ifndef FLUTTER_SHELL_PLATFORM_IOS_EXTERNAL_IMAGE_LOADER_GL_H_
#define FLUTTER_SHELL_PLATFORM_IOS_EXTERNAL_IMAGE_LOADER_GL_H_

#include "flutter/bdflutter/shell/platform/darwin/ios/ios_external_image_loader.h"
#include "flutter/fml/platform/darwin/cf_utils.h"
#include "flutter/shell/platform/darwin/ios/framework/Headers/FlutterImageLoader.h"
#include "flutter/bdflutter/shell/platform/darwin/ios/ios_image_loader_callback_context.h"

// BD ADD: START
namespace flutter {
    
    class IOSOpenGLExternalImageLoader : public IOSExternalImageLoader {
    public:

        IOSOpenGLExternalImageLoader(NSObject<FlutterImageLoader>* imageLoader);
        
        ~IOSOpenGLExternalImageLoader() override = default;

        void LoadCodec(const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(std::unique_ptr<NativeExportCodec> codec)> callback) override;

        void GetNextFrame(ImageLoaderContext loaderContext, int currentFrame, std::shared_ptr<NativeExportCodec> codec, std::function<void(sk_sp<SkImage> skimage)> callback) override;
        
        void Load(const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(sk_sp<SkImage> image)> callback) override;
        
    private:
        fml::CFRef<CVOpenGLESTextureCacheRef> cache_ref_;

        void ImageLoaderCallback(IOSImageInfo imageInfo, std::shared_ptr<ImageLoaderCallbackContext<void(sk_sp<SkImage> image)>> imageLoaderCallbackContext, EAGLContext *glcontext);

        FML_DISALLOW_COPY_AND_ASSIGN(IOSOpenGLExternalImageLoader);
    };
    
}  // namespace flutter
// END

#endif  // FLUTTER_SHELL_PLATFORM_IOS_EXTERNAL_IMAGE_LOADER_GL_H_
