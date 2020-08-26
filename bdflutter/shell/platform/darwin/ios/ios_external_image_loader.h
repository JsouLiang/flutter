#ifndef FLUTTER_SHELL_PLATFORM_IOS_EXTERNAL_IMAGE_LOADER_H_
#define FLUTTER_SHELL_PLATFORM_IOS_EXTERNAL_IMAGE_LOADER_H_

#include "flutter/bdflutter/lib/ui/painting/image_loader.h"
#include "flutter/fml/platform/darwin/cf_utils.h"
#include "flutter/shell/platform/darwin/ios/framework/Headers/FlutterImageLoader.h"

// BD ADD: START
namespace flutter {
    
    class IOSExternalImageLoader : public flutter::ImageLoader {
    public:
        IOSExternalImageLoader(NSObject<FlutterImageLoader>* imageLoader);
        
        ~IOSExternalImageLoader() override;
        
        void Load(const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(sk_sp<SkImage> image)> callback) override;
        
    private:
        NSObject<FlutterImageLoader>* imageLoader_;
        fml::CFRef<CVOpenGLESTextureCacheRef> cache_ref_;
        FML_DISALLOW_COPY_AND_ASSIGN(IOSExternalImageLoader);
    };
    
}  // namespace flutter
// END

#endif  // FLUTTER_SHELL_PLATFORM_IOS_EXTERNAL_IMAGE_LOADER_GL_H_
