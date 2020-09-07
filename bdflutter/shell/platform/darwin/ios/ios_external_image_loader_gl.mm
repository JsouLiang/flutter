// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/bdflutter/shell/platform/darwin/ios/ios_external_image_loader_gl.h"

#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>

#import "flutter/bdflutter/shell/platform/darwin/ios/ios_image_loader_callback_context.h"

// BD ADD: START
namespace flutter {

    IOSOpenGLExternalImageLoader::IOSOpenGLExternalImageLoader(NSObject<FlutterImageLoader>* imageLoader): IOSExternalImageLoader(imageLoader) {}
    
    void releaseCVOpenGLESTextureRef(SkImage::ReleaseContext releaseContext) {
        if (releaseContext) {
            CFRelease(releaseContext);
        }
    }
    
    void IOSOpenGLExternalImageLoader::Load(const std::string url, const int width, const int height, const float scale, ImageLoaderContext loaderContext, std::function<void(sk_sp<SkImage> image)> callback) {
        NSString* URL = [NSString stringWithCString:url.c_str() encoding:[NSString defaultCStringEncoding]];
        const auto& task_runners = loaderContext.task_runners;
        auto io_task_runner = task_runners.GetIOTaskRunner();
        fml::WeakPtr<GrContext> context = loaderContext.resourceContext;
        std::shared_ptr<ImageLoaderCallbackContext> imageLoaderCallbackContext = std::make_shared<ImageLoaderCallbackContext>(task_runners);
        imageLoaderCallbackContext->callback = std::move(callback);
        // obtain eaglcontext in io thread to make sure that we use the right one
        EAGLContext *glcontext = [EAGLContext currentContext];
        void(^complete)(IOSImageInfo) = ^(IOSImageInfo imageInfo) {
            std::function<void(sk_sp<SkImage> image)> callback = std::move(imageLoaderCallbackContext->callback);
            imageLoaderCallbackContext->callback = nullptr;
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
        };
        
        if ([imageLoader_ respondsToSelector:@selector(loadImage:width:height:scale:complete:)]) {
            [imageLoader_ loadImage:URL width:width height:height scale:scale complete:complete];
        } else if ([imageLoader_ respondsToSelector:@selector(loadImage:complete:)]) {
            [imageLoader_ loadImage:URL complete:complete];
        }
    }
    
}  // namespace flutter
// END
