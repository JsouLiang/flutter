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
// =======
//         ~IOSExternalImageLoader() override;

//         void LoadCodec(const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(std::unique_ptr<NativeExportCodec> codec)> callback) override;

//         void GetNextFrame(ImageLoaderContext loaderContext, int currentFrame, std::shared_ptr<NativeExportCodec> codec, std::function<void(sk_sp<SkImage> skimage)> callback) override;
       
//         void Load(const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(sk_sp<SkImage> image)> callback) override;

//     private:
//         template<typename T>
//         struct ImageLoaderCallbackContext {
//             std::function<T> callback;
//             fml::RefPtr<fml::TaskRunner> io_task_runner;
//             fml::WeakPtr<GrContext> resourceContext;
//             ImageLoaderCallbackContext(std::function<T> callback) : callback(callback){}
//             ~ ImageLoaderCallbackContext() {
//                 if (callback != nullptr) {
//                     callback(nullptr);
//                 }
//             }
//         };
        
//         void ImageLoaderCallback(IOSImageInfo imageInfo, std::shared_ptr<ImageLoaderCallbackContext<void(sk_sp<SkImage> image)>> imageLoaderCallbackContext, EAGLContext *glcontext);
        
//         fml::scoped_nsobject<NSObject<FlutterImageLoader>> imageLoader_;
//         fml::CFRef<CVOpenGLESTextureCacheRef> cache_ref_;
// >>>>>>> eea46ef369... feature: image support animated.:shell/platform/darwin/ios/ios_external_image_loader.h
        FML_DISALLOW_COPY_AND_ASSIGN(IOSExternalImageLoader);
    };
    
}  // namespace flutter
// END

#endif  // FLUTTER_SHELL_PLATFORM_IOS_EXTERNAL_IMAGE_LOADER_GL_H_
