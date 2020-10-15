#ifndef FLUTTER_SHELL_PLATFORM_IOS_EXTERNAL_IMAGE_LOADER_H_
#define FLUTTER_SHELL_PLATFORM_IOS_EXTERNAL_IMAGE_LOADER_H_

#include "flutter/bdflutter/lib/ui/painting/image_loader.h"
#include "flutter/fml/platform/darwin/cf_utils.h"
#include "flutter/shell/platform/darwin/ios/framework/Headers/FlutterImageLoader.h"
#include "flutter/shell/platform/darwin/ios/rendering_api_selection.h"
#include "flutter/fml/platform/darwin/scoped_nsobject.h"

// BD ADD: START
namespace flutter {

    class IOSExternalImageLoader : public flutter::ImageLoader {
    public:

        static IOSExternalImageLoader* FromIOSRenderingAPI(IOSRenderingAPI renderingApi, NSObject<FlutterImageLoader>* imageLoader);

        IOSExternalImageLoader(NSObject<FlutterImageLoader>* imageLoader);
        
        ~IOSExternalImageLoader() override = default;
        

    protected:
        fml::scoped_nsobject<NSObject<FlutterImageLoader>> imageLoader_;
        FML_DISALLOW_COPY_AND_ASSIGN(IOSExternalImageLoader);
    };
    
}  // namespace flutter
// END

#endif  // FLUTTER_SHELL_PLATFORM_IOS_EXTERNAL_IMAGE_LOADER_GL_H_
