
#include "flutter/bdflutter/shell/platform/darwin/ios/ios_external_image_loader_metal.h"

#import <CoreVideo/CoreVideo.h>

#include "flutter/bdflutter/shell/platform/darwin/ios/ios_image_loader_callback_context.h"

// BD ADD: START
namespace flutter {

    IOSMetalExternalImageLoader::IOSMetalExternalImageLoader(NSObject<FlutterImageLoader>* imageLoader):IOSExternalImageLoader(imageLoader) {
        
        device_.reset([MTLCreateSystemDefaultDevice() retain]);
        if (!device_) {
            FML_DLOG(ERROR) << "Could not acquire Metal device.";
        }
    }

    void releaseCVMetalTextureRef(SkImage::ReleaseContext releaseContext) {
        if (releaseContext) {
            CFRelease(releaseContext);
        }
    }

    void IOSMetalExternalImageLoader::Load(const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(sk_sp<SkImage> image)> callback) {
        NSString* urlStr = [NSString stringWithCString:url.c_str() encoding:[NSString defaultCStringEncoding]];
        const auto& task_runners = loaderContext.task_runners;
        auto io_task_runner = task_runners.GetIOTaskRunner();
        fml::WeakPtr<GrContext> context = loaderContext.resourceContext;
        std::shared_ptr<ImageLoaderCallbackContext> imageloaderCallbackContext = std::make_shared<ImageLoaderCallbackContext>(task_runners);
        imageloaderCallbackContext->callback = std::move(callback);
        
        void(^complete)(IOSImageInfo) = ^(IOSImageInfo imageInfo) {
            std::function<void(sk_sp<SkImage> image)> callback = std::move(imageloaderCallbackContext->callback);
            imageloaderCallbackContext->callback = nullptr;
            if (!cache_ref_) {
                CVMetalTextureCacheRef cache = NULL;
                auto cv_return = CVMetalTextureCacheCreate(kCFAllocatorDefault,
                                                           NULL,
                                                           device_.get(),
                                                           NULL,
                                                           &cache);

                if (cv_return != kCVReturnSuccess) {
                    FML_LOG(WARNING) << "Failed to create Metal texture cache: " << cv_return;
                } else {
                    cache_ref_.Reset(cache);
                }
            }
            
            fml::CFRef<CVPixelBufferRef> bufferRef;
            CVMetalTextureRef texture = nullptr;
            if (cache_ref_) {
                bufferRef.Reset(imageInfo.pixelBufferRef);
                if (bufferRef != nullptr) {
                    auto cv_return = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                                               cache_ref_,
                                                                               bufferRef,
                                                                               NULL,
                                                                               MTLPixelFormatBGRA8Unorm,
                                                                               static_cast<int>(CVPixelBufferGetWidth(bufferRef)),
                                                                               static_cast<int>(CVPixelBufferGetHeight(bufferRef)),
                                                                               0u,
                                                                               &texture
                                                                               );
                    if (cv_return != kCVReturnSuccess) {
                        FML_LOG(WARNING) << "Could not create texture from pixel buffer: " << cv_return;
                    }
                }
            }
            
            if (!texture) {
                io_task_runner->PostTask(fml::MakeCopyable(
                [callback = std::move(callback)]() mutable {
                    callback(nullptr);
                }));
            } else {
                GrMtlTextureInfo textureInfo;
                textureInfo.fTexture = sk_cf_obj<const void*>{
                    [reinterpret_cast<NSObject*>(CVMetalTextureGetTexture(texture)) retain]
                };
                GrBackendTexture backendTexture(static_cast<int>(CVPixelBufferGetWidth(bufferRef)),
                                                static_cast<int>(CVPixelBufferGetHeight(bufferRef)),
                                                GrMipMapped::kNo,
                                                textureInfo
                                                );
                io_task_runner->PostTask(fml::MakeCopyable(
                [context = std::move(context),
                 texture = std::move(texture),
                 callback = std::move(callback),
                 backendTexture = std::move(backendTexture)]() mutable {
                    SkImage::TextureReleaseProc releaseP = &releaseCVMetalTextureRef;
                    SkImage::ReleaseContext releaseC = texture;
                    sk_sp<SkImage> image = SkImage::MakeFromTexture(context.get(),
                                                                    backendTexture,
                                                                    kTopLeft_GrSurfaceOrigin,
                                                                    kBGRA_8888_SkColorType,
                                                                    kPremul_SkAlphaType,
                                                                    nullptr,
                                                                    releaseP,
                                                                    releaseC);
                    FML_DCHECK(image) << "Failed to create SkImage from Texture.";
                    callback(std::move(image));
                }));
            }
        };
        
        if ([imageLoader_ respondsToSelector:@selector(loadImage:width:height:scale:complete:)]) {
            [imageLoader_ loadImage:urlStr width:width height:height scale:scale complete:complete];
        } else if ([imageLoader_ respondsToSelector:@selector(loadImage:complete:)]) {
            [imageLoader_ loadImage:urlStr complete:complete];
        }
    }

}  // namespace flutter

// END
