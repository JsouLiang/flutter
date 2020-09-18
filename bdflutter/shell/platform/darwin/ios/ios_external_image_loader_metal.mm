
#include "flutter/bdflutter/shell/platform/darwin/ios/ios_external_image_loader_metal.h"

#import <CoreVideo/CoreVideo.h>

#include "flutter/bdflutter/shell/platform/darwin/ios/ios_image_loader_callback_context.h"
#include "flutter/shell/platform/darwin/ios/framework/Source/vsync_waiter_ios.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "flutter/common/task_runners.h"
#include "flutter/fml/make_copyable.h"
#include "flutter/lib/ui/ui_dart_state.h"
#include "flutter/shell/platform/darwin/ios/ios_native_export_codec.h"

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

    void IOSMetalExternalImageLoader::ImageLoaderCallback(IOSImageInfo imageInfo, std::shared_ptr<ImageLoaderCallbackContext<void(sk_sp<SkImage> image)>> imageloaderCallbackContext) {
        std::function<void(sk_sp<SkImage> image)> callback = std::move(imageloaderCallbackContext->callback);
        imageloaderCallbackContext->callback = nullptr;
        auto io_task_runner = imageloaderCallbackContext->io_task_runner;
        fml::WeakPtr<GrContext> context = imageloaderCallbackContext->resourceContext;
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
                }
            ));
        }
    }

    void IOSMetalExternalImageLoader::Load(const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(sk_sp<SkImage> image)> callback) {
        NSString* urlStr = [NSString stringWithCString:url.c_str() encoding:[NSString defaultCStringEncoding]];
        const auto& task_runners = loaderContext.task_runners;
        auto io_task_runner = task_runners.GetIOTaskRunner();
        fml::WeakPtr<GrContext> context = loaderContext.resourceContext;
        std::shared_ptr<ImageLoaderCallbackContext<void(sk_sp<SkImage> image)>> imageLoaderCallbackContext = std::make_shared<ImageLoaderCallbackContext<void(sk_sp<SkImage> image)>>(std::move(callback));
        imageLoaderCallbackContext->io_task_runner = io_task_runner;
        imageLoaderCallbackContext->resourceContext = context;

        void(^complete)(IOSImageInfo) = ^(IOSImageInfo imageInfo) {
            this->ImageLoaderCallback(imageInfo, imageLoaderCallbackContext);
        };
        
        if ([imageLoader_ respondsToSelector:@selector(loadImage:width:height:scale:complete:)]) {
            [imageLoader_ loadImage:urlStr width:width height:height scale:scale complete:complete];
        } else if ([imageLoader_ respondsToSelector:@selector(loadImage:complete:)]) {
            [imageLoader_ loadImage:urlStr complete:complete];
        }
    }


    void IOSMetalExternalImageLoader::LoadCodec(const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(std::unique_ptr<NativeExportCodec> codec)> callback) {
        NSString *urlStr = [NSString stringWithCString:url.c_str() encoding:[NSString defaultCStringEncoding]];
        const auto& task_runners = loaderContext.task_runners;
        auto io_task_runner = task_runners.GetIOTaskRunner();
        std::shared_ptr<ImageLoaderCallbackContext<void(std::unique_ptr<NativeExportCodec> codec)>> imageLoaderCallbackContext = std::make_shared<ImageLoaderCallbackContext<void(std::unique_ptr<NativeExportCodec> codec)>>(callback);
        void(^complete)(NSObject<IOSImageCodec> *) = ^(NSObject<IOSImageCodec> *iOSCodec) {
            std::function<void(std::unique_ptr<NativeExportCodec> codec)> contextCallback = std::move(imageLoaderCallbackContext->callback);
            imageLoaderCallbackContext->callback = nullptr;
            auto codec = std::make_unique<IOSNativeExportCodec>(iOSCodec);

            io_task_runner->PostTask(fml::MakeCopyable(
                [callback = std::move(contextCallback),
                 codec = std::move(codec)]() mutable {
                     callback(std::move(codec));
                 }
            ));
        };

        if ([imageLoader_ respondsToSelector:@selector(loadCodec:width:height:scale:complete:)]) {
            [imageLoader_ loadCodec:urlStr width:width height:height scale:scale complete:complete];
        }
    }

    void IOSMetalExternalImageLoader::GetNextFrame(ImageLoaderContext loaderContext, int currentFrame, std::shared_ptr<NativeExportCodec> codec, std::function<void(sk_sp<SkImage> skimage)> callback) {
        IOSNativeExportCodec *iOSCodec = static_cast<IOSNativeExportCodec *>(codec.get());
        const auto& task_runners = loaderContext.task_runners;
        auto io_task_runner = task_runners.GetIOTaskRunner();
        fml::WeakPtr<GrContext> resourceContext = loaderContext.resourceContext;
        std::shared_ptr<ImageLoaderCallbackContext<void(sk_sp<SkImage> skimage)>> imageLoaderCallbackContext = std::make_shared<ImageLoaderCallbackContext<void(sk_sp<SkImage> skimage)>>(callback);
        imageLoaderCallbackContext->io_task_runner = io_task_runner;
        imageLoaderCallbackContext->resourceContext = resourceContext;
        void(^complete)(IOSImageInfo) = ^(IOSImageInfo imageInfo) {
            this->ImageLoaderCallback(imageInfo, imageLoaderCallbackContext);
        };

        [iOSCodec->imageCodec getNextFrame:currentFrame complete:complete];
    }

}  // namespace flutter

// END
