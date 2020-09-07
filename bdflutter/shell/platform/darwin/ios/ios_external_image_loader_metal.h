#ifndef FLUTTER_SHELL_PLATFORM_IOS_EXTERNAL_IMAGE_LOADER_METAL_H_
#define FLUTTER_SHELL_PLATFORM_IOS_EXTERNAL_IMAGE_LOADER_METAL_H_

#include <Metal/Metal.h>
#include "flutter/bdflutter/shell/platform/darwin/ios/ios_external_image_loader.h"
#include "flutter/fml/platform/darwin/cf_utils.h"
#include "flutter/shell/platform/darwin/ios/framework/Headers/FlutterImageLoader.h"
#include "flutter/fml/platform/darwin/scoped_nsobject.h"


// BD ADD: START
namespace flutter {
    
    class IOSMetalExternalImageLoader : public IOSExternalImageLoader {
    public:

        IOSMetalExternalImageLoader(NSObject<FlutterImageLoader>* imageLoader);
        
        ~IOSMetalExternalImageLoader() override = default;
        
        void Load(const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(sk_sp<SkImage> image)> callback) override;
        
    private:
        fml::scoped_nsprotocol<id<MTLDevice>> device_;
        fml::CFRef<CVMetalTextureCacheRef> cache_ref_;
        FML_DISALLOW_COPY_AND_ASSIGN(IOSMetalExternalImageLoader);
    };
    
}  // namespace flutter
// END

#endif  // FLUTTER_SHELL_PLATFORM_IOS_EXTERNAL_IMAGE_LOADER_METAL_H_
