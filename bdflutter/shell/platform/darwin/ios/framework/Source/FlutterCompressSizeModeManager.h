//
//  FlutterCompressSizeModeManager.h
//  sources
//
//  Created by 邱鑫玥 on 2019/9/18.
//

// BD ADD: START
#import <UIKit/UIKit.h>
#import <dlfcn.h>
#import "flutter/common/settings.h"
#import "flutter/shell/platform/darwin/ios/framework/Source/FlutterDartProject_Internal.h"

#ifdef __LP64__
typedef struct mach_header_64 flutter_mach_header;
typedef struct section_64 flutter_section;
#define flutter_getsectbynamefromheader getsectbynamefromheader_64
#else
typedef struct mach_header flutter_mach_header;
typedef struct section flutter_section;
#define flutter_getsectbynamefromheader getsectbynamefromheader
#endif

NS_ASSUME_NONNULL_BEGIN

@interface FlutterCompressSizeModeManager : NSObject

@property(nonatomic) BOOL isCompressSizeMode;

+ (instancetype)sharedInstance;

/**
 * 异步解压数据到磁盘
 *
 * @param monitor 解压到磁盘监控
 */
- (void)decompressDataAsyncIfNeeded:(BOOL)isPredecompressMode
                            monitor:(FlutterCompressSizeModeMonitor _Nullable)monitor;

/**
 * 移除之前解压过，并且和当前App.framework的UUID不同的解压文件
 */
- (void)removePreviousDecompressedDataAsync;

/**
 * 数据是否需要解压到磁盘
 *
 * @return 数据是否需要解压到磁盘
 */
- (BOOL)needDecompressData;

/**
 * 更新FlutterDartProject中的Settings
 *
 * @param settings 需要修改的Settings
 * @param monitor 解压到磁盘监控
 */
- (void)updateSettingsIfNeeded:(flutter::Settings&)settings
                       monitor:(FlutterCompressSizeModeMonitor _Nullable)monitor;

@end

NS_ASSUME_NONNULL_END
// END
