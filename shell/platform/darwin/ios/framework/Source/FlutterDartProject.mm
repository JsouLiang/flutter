// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FML_USED_ON_EMBEDDER

#include "flutter/shell/platform/darwin/ios/framework/Source/FlutterDartProject_Internal.h"

#include "flutter/common/task_runners.h"
#include "flutter/fml/message_loop.h"
#include "flutter/fml/platform/darwin/scoped_nsobject.h"
#include "flutter/runtime/dart_vm.h"
#include "flutter/shell/common/shell.h"
#include "flutter/shell/common/switches.h"
#include "flutter/shell/platform/darwin/common/command_line.h"
#include "flutter/shell/platform/darwin/ios/framework/Headers/FlutterViewController.h"
// BD ADD:
#include "flutter/shell/platform/darwin/ios/framework/Source/FlutterCompressSizeModeManager.h"

extern "C" {
#if FLUTTER_RUNTIME_MODE == FLUTTER_RUNTIME_MODE_DEBUG
// Used for debugging dart:* sources.
extern const uint8_t kPlatformStrongDill[];
extern const intptr_t kPlatformStrongDillSize;
#endif
}

static id<DynamicFlutterDelegate> dynamicDelegate;
static const char* kApplicationKernelSnapshotFileName = "kernel_blob.bin";
// BD ADD: START
NSString* const FlutterIsolateDataFileName = @"isolate_snapshot_data";
NSString* const FlutterVMDataFileName = @"vm_snapshot_data";
NSString* const FlutterIcudtlDataFileName = @"icudtl.dat";
static NSString* const kFLTAssetsPath = @"FLTAssetsPath";
static NSString* const kFlutterAssets = @"flutter_assets";
static FlutterCompressSizeModeMonitor kFlutterCompressSizeModeMonitor;
// END

static flutter::Settings DefaultSettingsForProcess(NSBundle* bundle = nil) {
  auto command_line = flutter::CommandLineFromNSProcessInfo();

  // Precedence:
  // 1. Settings from the specified NSBundle.
  // 2. Settings passed explicitly via command-line arguments.
  // 3. Settings from the NSBundle with the default bundle ID.
  // 4. Settings from the main NSBundle and default values.

  NSBundle* mainBundle = [NSBundle mainBundle];
  NSBundle* engineBundle = [NSBundle bundleForClass:[FlutterViewController class]];

  bool hasExplicitBundle = bundle != nil;
  if (bundle == nil) {
    bundle = [NSBundle bundleWithIdentifier:[FlutterDartProject defaultBundleIdentifier]];
  }
  if (bundle == nil) {
    bundle = mainBundle;
  }

  auto settings = flutter::SettingsFromCommandLine(command_line);

  settings.task_observer_add = [](intptr_t key, fml::closure callback) {
    fml::MessageLoop::GetCurrent().AddTaskObserver(key, std::move(callback));
  };

  settings.task_observer_remove = [](intptr_t key) {
    fml::MessageLoop::GetCurrent().RemoveTaskObserver(key);
  };

  // The command line arguments may not always be complete. If they aren't, attempt to fill in
  // defaults.

  // Flutter ships the ICU data file in the the bundle of the engine. Look for it there.
  if (settings.icu_data_path.size() == 0) {
    NSString* icuDataPath = [engineBundle pathForResource:@"icudtl" ofType:@"dat"];
    if (icuDataPath.length > 0) {
      settings.icu_data_path = icuDataPath.UTF8String;
    }
  }

  if (flutter::DartVM::IsRunningPrecompiledCode()) {
    if (hasExplicitBundle) {
      NSString* executablePath = bundle.executablePath;
      if ([[NSFileManager defaultManager] fileExistsAtPath:executablePath]) {
        settings.application_library_path = executablePath.UTF8String;
      }
    }

    // No application bundle specified.  Try a known location from the main bundle's Info.plist.
    if (settings.application_library_path.size() == 0) {
      NSString* libraryName = [mainBundle objectForInfoDictionaryKey:@"FLTLibraryPath"];
      NSString* libraryPath = [mainBundle pathForResource:libraryName ofType:@""];
      if (libraryPath.length > 0) {
        NSString* executablePath = [NSBundle bundleWithPath:libraryPath].executablePath;
        if (executablePath.length > 0) {
          settings.application_library_path = executablePath.UTF8String;
        }
      }
    }

    // In case the application bundle is still not specified, look for the App.framework in the
    // Frameworks directory.
    if (settings.application_library_path.size() == 0) {
      NSString* applicationFrameworkPath = [mainBundle pathForResource:@"Frameworks/App.framework"
                                                                ofType:@""];
      if (applicationFrameworkPath.length > 0) {
        NSString* executablePath =
            [NSBundle bundleWithPath:applicationFrameworkPath].executablePath;
        if (executablePath.length > 0) {
          settings.application_library_path = executablePath.UTF8String;
        }
      }
    }
  }

  // Checks to see if the flutter assets directory is already present.
  if (settings.assets_path.size() == 0) {
    NSFileManager* fileManager = [NSFileManager defaultManager];
    NSString* assetsName = [FlutterDartProject flutterAssetsName:bundle];
    NSString* assetsPath = nil;
    // check dynamic settings first
    if (dynamicDelegate && [dynamicDelegate respondsToSelector:@selector(assetsPath)]) {
      NSString* dynamicAssetsPath = [dynamicDelegate assetsPath];
      if (dynamicAssetsPath && [dynamicAssetsPath isKindOfClass:[NSString class]]) {
        BOOL isDirectory = NO;
        BOOL isExist = [fileManager fileExistsAtPath:dynamicAssetsPath isDirectory:&isDirectory];
        if (isDirectory && isExist) {
          NSURL* kernelURL = [NSURL URLWithString:@(kApplicationKernelSnapshotFileName)
                                    relativeToURL:[NSURL fileURLWithPath:dynamicAssetsPath]];
          if ([fileManager fileExistsAtPath:kernelURL.path]) {
            assetsPath = dynamicAssetsPath;
          }
        }
      }
    }

    if (!assetsPath || assetsPath.length == 0) {
      assetsPath = [bundle pathForResource:assetsName ofType:@""];
    }

    if (!assetsPath || assetsPath.length == 0) {
      assetsPath = [mainBundle pathForResource:assetsName ofType:@""];
    }

    if (!assetsPath || assetsPath.length == 0) {
      NSLog(@"Failed to find assets path for \"%@\"", assetsName);
    } else {
      settings.assets_path = assetsPath.UTF8String;

      // Check if there is an application kernel snapshot in the assets directory we could
      // potentially use.  Looking for the snapshot makes sense only if we have a VM that can use
      // it.
      if (!flutter::DartVM::IsRunningPrecompiledCode()) {
        NSURL* applicationKernelSnapshotURL =
            [NSURL URLWithString:@(kApplicationKernelSnapshotFileName)
                   relativeToURL:[NSURL fileURLWithPath:assetsPath]];
        if ([fileManager fileExistsAtPath:applicationKernelSnapshotURL.path]) {
          settings.application_kernel_asset = applicationKernelSnapshotURL.path.UTF8String;
        } else {
          NSLog(@"Failed to find snapshot: %@", applicationKernelSnapshotURL.path);
        }
      }
    }
  }

#if FLUTTER_RUNTIME_MODE == FLUTTER_RUNTIME_MODE_DEBUG
  // There are no ownership concerns here as all mappings are owned by the
  // embedder and not the engine.
  auto make_mapping_callback = [](const uint8_t* mapping, size_t size) {
    return [mapping, size]() { return std::make_unique<fml::NonOwnedMapping>(mapping, size); };
  };

  settings.dart_library_sources_kernel =
      make_mapping_callback(kPlatformStrongDill, kPlatformStrongDillSize);
#endif  // FLUTTER_RUNTIME_MODE == FLUTTER_RUNTIME_MODE_DEBUG

  return settings;
}

@implementation FlutterDartProject {
  fml::scoped_nsobject<NSBundle> _precompiledDartBundle;
  flutter::Settings _settings;
}

#pragma mark - Override base class designated initializers

- (instancetype)init {
  return [self initWithPrecompiledDartBundle:nil];
}

#pragma mark - Designated initializers

- (instancetype)initWithPrecompiledDartBundle:(NSBundle*)bundle {
  self = [super init];

  if (self) {
    _precompiledDartBundle.reset([bundle retain]);
    _settings = DefaultSettingsForProcess(bundle);

    // BD ADD: START
    [[FlutterCompressSizeModeManager sharedInstance] removePreviousDecompressedData];
    if ([FlutterCompressSizeModeManager sharedInstance].isCompressSizeMode) {
      NSString* decompressedDataPath = [[FlutterCompressSizeModeManager sharedInstance]
          getDecompressedDataPath:kFlutterCompressSizeModeMonitor];
      [self setDecompressedDataPath:decompressedDataPath];
      self.isValid = (decompressedDataPath.length > 0);
    } else {
      self.isValid = YES;
    }
    // END
  }

  return self;
}

// BD ADD: START
- (void)setLeakDartVMEnabled:(BOOL)enabled {
  _settings.leak_vm = enabled;
}
// END

// BD ADD: START
- (void)setDecompressedDataPath:(NSString*)path {
  if (path.length == 0) {
    return;
  }
  _settings.icu_data_path =
      [path stringByAppendingPathComponent:FlutterIcudtlDataFileName].UTF8String;
  _settings.assets_path =
      [path stringByAppendingPathComponent:[FlutterDartProject flutterAssetsPath]].UTF8String;
  _settings.isolate_snapshot_data_path =
      [path stringByAppendingPathComponent:FlutterIsolateDataFileName].UTF8String;
  _settings.vm_snapshot_data_path =
      [path stringByAppendingPathComponent:FlutterVMDataFileName].UTF8String;
}
// END

#pragma mark - Dynamic

+ (void)registerDynamicDelegate:(id<DynamicFlutterDelegate>)delegate {
  dynamicDelegate = delegate;
}

#pragma mark - Settings accessors

- (const flutter::Settings&)settings {
  return _settings;
}

- (flutter::RunConfiguration)runConfiguration {
  return [self runConfigurationForEntrypoint:nil];
}

- (flutter::RunConfiguration)runConfigurationForEntrypoint:(NSString*)entrypointOrNil {
  return [self runConfigurationForEntrypoint:entrypointOrNil libraryOrNil:nil];
}

- (flutter::RunConfiguration)runConfigurationForEntrypoint:(NSString*)entrypointOrNil
                                              libraryOrNil:(NSString*)dartLibraryOrNil {
  auto config = flutter::RunConfiguration::InferFromSettings(_settings);
  if (dartLibraryOrNil && entrypointOrNil) {
    config.SetEntrypointAndLibrary(std::string([entrypointOrNil UTF8String]),
                                   std::string([dartLibraryOrNil UTF8String]));

  } else if (entrypointOrNil) {
    config.SetEntrypoint(std::string([entrypointOrNil UTF8String]));
  }
  return config;
}

#pragma mark - Assets-related utilities

+ (NSString*)flutterAssetsName:(NSBundle*)bundle {
  if (bundle == nil) {
    bundle = [NSBundle bundleWithIdentifier:[FlutterDartProject defaultBundleIdentifier]];
  }
  if (bundle == nil) {
    bundle = [NSBundle mainBundle];
  }
  // BD MOD:
  // NSString* flutterAssetsName = [bundle objectForInfoDictionaryKey:@"FLTAssetsPath"];
  NSString* flutterAssetsName = [bundle objectForInfoDictionaryKey:kFLTAssetsPath];
  if (flutterAssetsName == nil) {
    // BD MOD:
    // flutterAssetsName = @"Frameworks/App.framework/flutter_assets";
    flutterAssetsName = [NSString stringWithFormat:@"Frameworks/App.framework/%@", kFlutterAssets];
  }
  return flutterAssetsName;
}

+ (NSString*)lookupKeyForAsset:(NSString*)asset {
  return [self lookupKeyForAsset:asset fromBundle:nil];
}

+ (NSString*)lookupKeyForAsset:(NSString*)asset fromBundle:(NSBundle*)bundle {
  NSString* flutterAssetsName = [FlutterDartProject flutterAssetsName:bundle];
  return [NSString stringWithFormat:@"%@/%@", flutterAssetsName, asset];
}

+ (NSString*)lookupKeyForAsset:(NSString*)asset fromPackage:(NSString*)package {
  return [self lookupKeyForAsset:asset fromPackage:package fromBundle:nil];
}

+ (NSString*)lookupKeyForAsset:(NSString*)asset
                   fromPackage:(NSString*)package
                    fromBundle:(NSBundle*)bundle {
  return [self lookupKeyForAsset:[NSString stringWithFormat:@"packages/%@/%@", package, asset]
                      fromBundle:bundle];
}

+ (NSString*)defaultBundleIdentifier {
  return @"io.flutter.flutter.app";
}

// BD ADD: START
+ (void)setCompressSizeModeMonitor:(FlutterCompressSizeModeMonitor)flutterCompressSizeModeMonitor {
  kFlutterCompressSizeModeMonitor = [flutterCompressSizeModeMonitor copy];
}

+ (void)predecompressData {
  [[FlutterCompressSizeModeManager sharedInstance] decompressDataAsync:nil];
}

+ (NSString*)flutterAssetsPath {
  NSBundle* bundle = [NSBundle bundleWithIdentifier:[FlutterDartProject defaultBundleIdentifier]];
  if (bundle == nil) {
    bundle = [NSBundle mainBundle];
  }
  NSString* flutterAssetsPath = [bundle objectForInfoDictionaryKey:kFLTAssetsPath];
  if (flutterAssetsPath == nil) {
    flutterAssetsPath = kFlutterAssets;
  }
  return flutterAssetsPath;
}
// END

@end
