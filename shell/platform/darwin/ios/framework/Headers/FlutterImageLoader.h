#ifndef FLUTTER_FLUTTERIMAGELOADER_H_
#define FLUTTER_FLUTTERIMAGELOADER_H_

#import <CoreMedia/CoreMedia.h>
#import <Foundation/Foundation.h>

#include "FlutterMacros.h"

typedef struct IOSImageInfo{
  CVPixelBufferRef _Nullable pixelBufferRef;
  void* _Nullable data;
  size_t width;
  size_t height;
  OSType pixelFormatType;
  CGImageAlphaInfo alphaInfo;
} IOSImageInfo;

NS_ASSUME_NONNULL_BEGIN

FLUTTER_EXPORT
@protocol FlutterImageLoader <NSObject>
typedef void(^callback)(IOSImageInfo);
- (void)loadImage:(NSString*)url complete:(callback)complete;
- (void)loadImage:(NSString*)url width:(NSInteger)width height:(NSInteger)height scale:(CGFloat)scale complete:(callback)complete;
@end

FLUTTER_EXPORT
@protocol FlutterImageLoaderRegistry <NSObject>
- (void)registerImageLoader:(NSObject<FlutterImageLoader>*)imageLoader;
@end

NS_ASSUME_NONNULL_END

#endif  // FLUTTER_FLUTTERIMAGELOADER_H_

