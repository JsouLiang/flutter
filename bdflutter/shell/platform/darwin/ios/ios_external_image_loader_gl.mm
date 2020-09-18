// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/bdflutter/shell/platform/darwin/ios/ios_external_image_loader_gl.h"

#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>

#import "flutter/bdflutter/shell/platform/darwin/ios/ios_image_loader_callback_context.h"
#include "flutter/shell/platform/darwin/ios/framework/Source/vsync_waiter_ios.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "flutter/common/task_runners.h"
#include "flutter/fml/make_copyable.h"
#include "flutter/lib/ui/ui_dart_state.h"
#include "flutter/shell/platform/darwin/ios/ios_native_export_codec.h"

// BD ADD: START
namespace flutter {

    IOSOpenGLExternalImageLoader::IOSOpenGLExternalImageLoader(NSObject<FlutterImageLoader>* imageLoader): IOSExternalImageLoader(imageLoader) {}
    
    void releaseCVOpenGLESTextureRef(SkImage::ReleaseContext releaseContext) {
        if (releaseContext) {
            CFRelease(releaseContext);
        }
    }

    void IOSOpenGLExternalImageLoader::ImageLoaderCallback(IOSImageInfo imageInfo, std::shared_ptr<ImageLoaderCallbackContext<void(sk_sp<SkImage> image)>> imageLoaderCallbackContext, EAGLContext *glcontext) {
        std::function<void(sk_sp<SkImage> image)> callback = std::move(imageLoaderCallbackContext->callback);
        imageLoaderCallbackContext->callback = nullptr;
        auto io_task_runner = imageLoaderCallbackContext->io_task_runner;
        fml::WeakPtr<GrContext> context = imageLoaderCallbackContext->resourceContext;
        if (!cache_ref_) {
            CVOpenGLESTextureCacheRef cache;
            CVReturn err = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, NULL, glcontext, NULL, &cache);
            if (err == noErr) {
                cache_ref_.Reset(cache);
            } else {
                FML_LOG(WARNING) << "Failed to create GLES texture cache: " << err;
            }
        }
        fml::CFRef<CVPixelBufferRef> bufferRef;
        CVOpenGLESTextureRef texture = nullptr;
        if (cache_ref_) {
            bufferRef.Reset(imageInfo.pixelBufferRef);
            if (bufferRef != nullptr) {
                CVReturn err = CVOpenGLESTextureCacheCreateTextureFromImage(
                    kCFAllocatorDefault, cache_ref_, bufferRef, nullptr, GL_TEXTURE_2D, GL_RGBA,
                    static_cast<int>(CVPixelBufferGetWidth(bufferRef)),
                    static_cast<int>(CVPixelBufferGetHeight(bufferRef)), GL_BGRA, GL_UNSIGNED_BYTE, 0,
                    &texture);
                if (err != noErr) {
                    FML_LOG(WARNING) << "Could not create texture from pixel buffer: " << err;
                }
            }
        }

        if (!texture) {
            io_task_runner->PostTask(fml::MakeCopyable(
                [callback = std::move(callback)]() mutable {
                  callback(nullptr);
                }));
        } else {
            GrGLTextureInfo textureInfo = {CVOpenGLESTextureGetTarget(texture),
                                           CVOpenGLESTextureGetName(texture), GL_RGBA8_OES};
            GrBackendTexture backendTexture(
                static_cast<int>(CVPixelBufferGetWidth(bufferRef)),
                static_cast<int>(CVPixelBufferGetHeight(bufferRef)),
                GrMipMapped::kNo,
                textureInfo);
            io_task_runner->PostTask(fml::MakeCopyable(
                [context = std::move(context),
                 texture = std::move(texture),
                 callback = std::move(callback),
                 backendTexture = std::move(backendTexture)]() mutable {
                    SkImage::TextureReleaseProc releaseP = &releaseCVOpenGLESTextureRef;
                    SkImage::ReleaseContext releaseC = texture;
                    sk_sp<SkImage> image =
                        SkImage::MakeFromTexture(context.get(), backendTexture,
                                                 kTopLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType,
                                                 kPremul_SkAlphaType, nullptr, releaseP, releaseC);
                    FML_DCHECK(image) << "Failed to create SkImage from Texture.";
                    callback(std::move(image));
                }));
        }
    }

    void IOSOpenGLExternalImageLoader::Load(const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(sk_sp<SkImage> image)> callback) {
        NSString* URL = [NSString stringWithCString:url.c_str() encoding:[NSString defaultCStringEncoding]];
        const auto& task_runners = loaderContext.task_runners;
        auto io_task_runner = task_runners.GetIOTaskRunner();
        fml::WeakPtr<GrContext> resourceContext = loaderContext.resourceContext;
        std::shared_ptr<ImageLoaderCallbackContext<void(sk_sp<SkImage> image)>> imageLoaderCallbackContext = std::make_shared<ImageLoaderCallbackContext<void(sk_sp<SkImage> image)>>(std::move(callback));
        imageLoaderCallbackContext->io_task_runner = io_task_runner;
        imageLoaderCallbackContext->resourceContext = resourceContext;
        // obtain eaglcontext in io thread to make sure that we use the right one
        EAGLContext *glcontext = [EAGLContext currentContext];
        void(^complete)(IOSImageInfo) = ^(IOSImageInfo imageInfo) {
            this->ImageLoaderCallback(imageInfo, imageLoaderCallbackContext, glcontext);
        };
        
        if ([imageLoader_ respondsToSelector:@selector(loadImage:width:height:scale:complete:)]) {
            [imageLoader_ loadImage:URL width:width height:height scale:scale complete:complete];
        } else if ([imageLoader_ respondsToSelector:@selector(loadImage:complete:)]) {
            [imageLoader_ loadImage:URL complete:complete];
        }
    }

    void IOSOpenGLExternalImageLoader::LoadCodec(const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(std::unique_ptr<NativeExportCodec> codec)> callback) {
        NSString* URL = [NSString stringWithCString:url.c_str() encoding:[NSString defaultCStringEncoding]];
        const auto& task_runners = loaderContext.task_runners;
        auto io_task_runner = task_runners.GetIOTaskRunner();
        std::shared_ptr<ImageLoaderCallbackContext<void(std::unique_ptr<NativeExportCodec> codec)>> imageLoaderCallbackContext = std::make_shared<ImageLoaderCallbackContext<void(std::unique_ptr<NativeExportCodec> codec)>>(callback);
        void(^complete)(NSObject<IOSImageCodec>*) = ^(NSObject<IOSImageCodec>* iosCodec) {
            std::function<void(std::unique_ptr<NativeExportCodec> codec)> contextCallback = std::move(imageLoaderCallbackContext->callback);
            imageLoaderCallbackContext->callback = nullptr;
            auto codec = std::make_unique<IOSNativeExportCodec>(iosCodec);
            
            io_task_runner->PostTask(fml::MakeCopyable(
                [callback = std::move(contextCallback),
                 codec = std::move(codec)]() mutable {
                    callback(std::move(codec));
                }));
        };

        if ([imageLoader_ respondsToSelector:@selector(loadCodec:width:height:scale:complete:)]) {
            [imageLoader_ loadCodec:URL width:width height:height scale:scale complete:complete];
        }
    }

    void IOSOpenGLExternalImageLoader::GetNextFrame(ImageLoaderContext loaderContext, int currentFrame, std::shared_ptr<NativeExportCodec> codec, std::function<void(sk_sp<SkImage> skimage)> callback) {
        IOSNativeExportCodec* iosCodec = static_cast<IOSNativeExportCodec*>(codec.get());
        const auto& task_runners = loaderContext.task_runners;
        auto io_task_runner = task_runners.GetIOTaskRunner();
        fml::WeakPtr<GrContext> resourceContext = loaderContext.resourceContext;
        std::shared_ptr<ImageLoaderCallbackContext<void(sk_sp<SkImage> skimage)>> imageLoaderCallbackContext = std::make_shared<ImageLoaderCallbackContext<void(sk_sp<SkImage> skimage)>>(callback);
        imageLoaderCallbackContext->io_task_runner = io_task_runner;
        imageLoaderCallbackContext->resourceContext = resourceContext;
        EAGLContext *glcontext = [EAGLContext currentContext];
        void(^complete)(IOSImageInfo) = ^(IOSImageInfo imageInfo) {
            this->ImageLoaderCallback(imageInfo, imageLoaderCallbackContext, glcontext);
        };

        [iosCodec->imageCodec getNextFrame:currentFrame complete:complete];
    }

}  // namespace flutter
// END
