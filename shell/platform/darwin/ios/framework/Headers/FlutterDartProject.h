// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_FLUTTERDARTPROJECT_H_
#define FLUTTER_FLUTTERDARTPROJECT_H_

#import <Foundation/Foundation.h>

#import "FlutterMacros.h"

NS_ASSUME_NONNULL_BEGIN

// BD ADD: START
typedef void (^FlutterCompressSizeModeMonitor)(BOOL needDecompress,
                                               BOOL isPredecompressMode,
                                               BOOL succeeded,
                                               NSError* error);

extern NSErrorDomain const FlutterCompressSizeModeErrorDomain;
typedef NS_ENUM(NSInteger, FlutterCompressSizeModeErrorCode) {
  FlutterCompressSizeModeErrorCodeFailedOpenZipFile = -1,
  FlutterCompressSizeModeErrorCodeFailedOpenFileInZip = -2,
  FlutterCompressSizeModeErrorCodeFileInfoNotLoadable = -3,
  FlutterCompressSizeModeErrorCodeFileContentNotReadable = -4,
  FlutterCompressSizeModeErrorCodeFailedToWriteFile = -5,
  FlutterCompressSizeModeErrorCodeInvalidArguments = -6,
  FlutterCompressSizeModeErrorCodeFailedToWriteIsolateData = -101,
  FlutterCompressSizeModeErrorCodeFailedToWriteVMData = -102,
  FlutterCompressSizeModeErrorCodeFailedToWriteIcudtlData = -103,
  FlutterCompressSizeModeErrorCodeFailedToWriteAssetsData = -104,
};
// END

/**
 * A set of Flutter and Dart assets used by a `FlutterEngine` to initialize execution.
 */
FLUTTER_DARWIN_EXPORT
@interface FlutterDartProject : NSObject

/**
 * Initializes a Flutter Dart project from a bundle.
 */
- (instancetype)initWithPrecompiledDartBundle:(nullable NSBundle*)bundle NS_DESIGNATED_INITIALIZER;

/**
 * Unavailable - use `init` instead.
 */
- (instancetype)initFromDefaultSourceForConfiguration FLUTTER_UNAVAILABLE("Use -init instead.");

/**
 * Returns the file name for the given asset. If the bundle with the identifier
 * "io.flutter.flutter.app" exists, it will try use that bundle; otherwise, it
 * will use the main bundle.  To specify a different bundle, use
 * `-lookupKeyForAsset:asset:fromBundle`.
 *
 * @param asset The name of the asset. The name can be hierarchical.
 * @return the file name to be used for lookup in the main bundle.
 */
+ (NSString*)lookupKeyForAsset:(NSString*)asset;

/**
 * Returns the file name for the given asset.
 * The returned file name can be used to access the asset in the supplied bundle.
 *
 * @param asset The name of the asset. The name can be hierarchical.
 * @param bundle The `NSBundle` to use for looking up the asset.
 * @return the file name to be used for lookup in the main bundle.
 */
+ (NSString*)lookupKeyForAsset:(NSString*)asset fromBundle:(nullable NSBundle*)bundle;

/**
 * Returns the file name for the given asset which originates from the specified package.
 * The returned file name can be used to access the asset in the application's main bundle.
 *
 * @param asset The name of the asset. The name can be hierarchical.
 * @param package The name of the package from which the asset originates.
 * @return the file name to be used for lookup in the main bundle.
 */
+ (NSString*)lookupKeyForAsset:(NSString*)asset fromPackage:(NSString*)package;

/**
 * Returns the file name for the given asset which originates from the specified package.
 * The returned file name can be used to access the asset in the specified bundle.
 *
 * @param asset The name of the asset. The name can be hierarchical.
 * @param package The name of the package from which the asset originates.
 * @param bundle The bundle to use when doing the lookup.
 * @return the file name to be used for lookup in the main bundle.
 */
+ (NSString*)lookupKeyForAsset:(NSString*)asset
                   fromPackage:(NSString*)package
                    fromBundle:(nullable NSBundle*)bundle;

/**
 * Returns the default identifier for the bundle where we expect to find the Flutter Dart
 * application.
 */
+ (NSString*)defaultBundleIdentifier;

// BD ADD: START
+ (void)setCompressSizeModeMonitor:(FlutterCompressSizeModeMonitor)flutterCompressSizeModeMonitor;

/**
 * 是否内置压缩模式
 */
+ (BOOL)isCompressSizeMode;

/**
 * 是否需要解压数据到磁盘
 * @note 返回YES时，进入Flutter页面使用磁盘中的数据；返回NO时，进入Flutter页面使用内存中的数据
 */
+ (BOOL)needDecompressData FLUTTER_DEPRECATED(
    "Always called before decompress. No need to call manually");

/**
 * 压缩模式解压数据到磁盘，返回是否解压到磁盘成功
 * 如果之前已经解压过，则返回YES，error是nil
 * 如果之前没有解压过，则返回当次解压结果
 */
+ (BOOL)decompressData:(NSError**)error
    FLUTTER_DEPRECATED(
        "Does not support decompress synchronously. Use -predecompressData instead.");

/**
 * 预解压资源
 */
+ (void)predecompressData;

// END

@end

NS_ASSUME_NONNULL_END

#endif  // FLUTTER_FLUTTERDARTPROJECT_H_
