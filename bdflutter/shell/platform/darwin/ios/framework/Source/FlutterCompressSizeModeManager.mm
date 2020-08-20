//
//  FlutterCompressSizeModeManager.m
//  sources
//
//  Created by 邱鑫玥 on 2019/9/18.
//

// BD ADD: START
#import "flutter/bdflutter/shell/platform/darwin/ios/framework/Source/FlutterCompressSizeModeManager.h"
#import <dlfcn.h>
#import <mach-o/dyld.h>
#import <mach-o/getsect.h>
#import <pthread.h>
#import "FlutterZipArchive.h"
#import "NSData+FlutterGzip.h"
#import "flutter/bdflutter/assets/zip_asset_store.h"
#import "flutter/fml/platform/darwin/scoped_nsobject.h"

typedef void (^ext_cleanupBlock_t)(void);

#if defined(DEBUG) && !defined(NDEBUG)
#define ext_keywordify \
  autoreleasepool {}
#else
#define ext_keywordify \
  try {                \
  } @catch (...) {     \
  }
#endif

#define metamacro_concat(A, B) metamacro_concat_(A, B)

#define metamacro_concat_(A, B) A##B

#define onExit                                                                          \
  ext_keywordify __strong ext_cleanupBlock_t metamacro_concat(ext_exitBlock_, __LINE__) \
      __attribute__((cleanup(ext_executeCleanupBlock), unused)) = ^

#define FlutterCompressSizeModeManagerLock(lock) \
  pthread_mutex_lock(&lock);                     \
  @onExit {                                      \
    pthread_mutex_unlock(&lock);                 \
  }

void ext_executeCleanupBlock(__strong ext_cleanupBlock_t* block) {
  (*block)();
}

static const char* kSegmentName = "__BD_DATA";
static const char* kIsolateDataSectionName = "__isolate_data";
static const char* kVMDataSectionName = "__vm_data";
static const char* kValidateSectionName = "__validate_data";
static NSString* const kDecompressedDataCacheDirectory = @"com.bytedance.flutter/decompressed_data";
static NSString* const kVMDataFileName = @"vm_snapshot_data";
static NSString* const kIsolateDataFileName = @"isolate_snapshot_data";
static NSString* const kIcudtlDataFileName = @"icudtl.dat";
static NSString* const kAssetsFlagFileName = @"flutter_assets_flag";
static NSString* const kIcudtlFlagFileName = @"flutter_icudtl_flag";
static const struct mach_header* kMachHeader = nullptr;

#pragma mark - MachHeader

static uintptr_t FirstLoadCommandPtr(const struct mach_header* mh) {
  switch (mh->magic) {
    case MH_MAGIC:
    case MH_CIGAM:
      return (uintptr_t)(mh + 1);
    case MH_MAGIC_64:
    case MH_CIGAM_64:
      return (uintptr_t)(((struct mach_header_64*)mh) + 1);
    default:
      return 0;
  }
}

static const uint8_t* ImageUUID(const struct mach_header* mh) {
  if (mh != NULL) {
    uintptr_t cmdPtr = FirstLoadCommandPtr(mh);
    if (cmdPtr != 0) {
      for (uint32_t iCmd = 0; iCmd < mh->ncmds; iCmd++) {
        const struct load_command* loadCmd = (struct load_command*)cmdPtr;
        if (loadCmd->cmd == LC_UUID) {
          struct uuid_command* uuidCmd = (struct uuid_command*)cmdPtr;
          return uuidCmd->uuid;
        }
        cmdPtr += loadCmd->cmdsize;
      }
    }
  }

  return NULL;
}

static void ImageAdded(const struct mach_header* mh, intptr_t slide) {
  Dl_info info;
  if (dladdr(mh, &info) == 0) {
    return;
  }
  if (strstr(info.dli_fname, "App.framework") == NULL) {
    return;
  }

  const flutter_section* isolateDataSection = flutter_getsectbynamefromheader(
      (flutter_mach_header*)mh, kSegmentName, kIsolateDataSectionName);
  const flutter_section* vmDataSection =
      flutter_getsectbynamefromheader((flutter_mach_header*)mh, kSegmentName, kVMDataSectionName);

  if (isolateDataSection == NULL && vmDataSection == NULL) {
    return;
  }

  kMachHeader = mh;
}

@interface FlutterCompressSizeModeManager ()

@property(nonatomic) NSString* appUUIDString;
@property(nonatomic) NSString* cacheDirectoryForDecompressedData;
@property(nonatomic) NSString* cacheDirectoryForCurrentUUID;
@property(nonatomic) NSString* vmDataPath;
@property(nonatomic) NSString* isolateDataPath;
@property(nonatomic) NSString* validateMD5;
@property(nonatomic) NSString* unzipVmDataMD5;
@property(nonatomic) NSString* unzipIsolateValidateMD5;
@property(nonatomic) NSString* icudtlDataPath;
@property(nonatomic) NSString* assetsPath;
@property(nonatomic) NSString* assetsFlagPath;
@property(nonatomic) NSString* icudtlFlagPath;
@property(nonatomic) NSString* flutterAssets;
@property(nonatomic) NSString* zipIcudtlFilePath;
@property(nonatomic) NSString* zipAssetsFilePath;

@end

@implementation FlutterCompressSizeModeManager {
  pthread_mutex_t _mutexLock;
  fml::scoped_nsobject<NSData> _vmData;
  fml::scoped_nsobject<NSData> _isolateData;
  dispatch_queue_t _serialDecompressQueue;
}

+ (void)initialize {
  _dyld_register_func_for_add_image(&ImageAdded);
}

+ (instancetype)sharedInstance {
  static dispatch_once_t onceToken;
  static FlutterCompressSizeModeManager* instance;
  dispatch_once(&onceToken, ^{
    instance = [[FlutterCompressSizeModeManager alloc] init];
  });
  return instance;
}

- (void)dealloc {
  [_appUUIDString release];
  [_cacheDirectoryForDecompressedData release];
  [_cacheDirectoryForCurrentUUID release];
  [_vmDataPath release];
  [_isolateDataPath release];
  [_icudtlDataPath release];
  [_assetsPath release];
  [_assetsFlagPath release];
  [_icudtlFlagPath release];
  [_flutterAssets release];
  [_zipIcudtlFilePath release];
  [_zipAssetsFilePath release];
  [_validateMD5 release];
  [_unzipVmDataMD5 release];
  [_unzipIsolateValidateMD5 release];
  pthread_mutex_destroy(&_mutexLock);
  dispatch_release(_serialDecompressQueue);
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [super dealloc];
}

- (instancetype)init {
  if (self = [super init]) {
    NSArray* paths =
        NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString* path = [paths objectAtIndex:0];
    self.cacheDirectoryForDecompressedData =
        [[path stringByAppendingPathComponent:kDecompressedDataCacheDirectory] copy];

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(onMemoryWarning:)
                   name:UIApplicationDidReceiveMemoryWarningNotification
                 object:nil];

    // init lock
    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_settype(&mutexAttr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&_mutexLock, &mutexAttr);
    pthread_mutexattr_destroy(&mutexAttr);

    _serialDecompressQueue =
        dispatch_queue_create("com.bytedance.flutter.decompress", DISPATCH_QUEUE_SERIAL);

    if (kMachHeader != nullptr) {
      self.isCompressSizeMode = YES;
      const uint8_t* uuidPtr = ImageUUID(kMachHeader);
      NSString* uuidString = nil;
      if (uuidPtr != NULL) {
        NSUUID* uuid = [[[NSUUID alloc] initWithUUIDBytes:uuidPtr] autorelease];
        uuidString = [uuid UUIDString];
      }
      if (uuidString.length == 0) {
        uuidString = [NSString stringWithFormat:@"%f", [[NSDate date] timeIntervalSince1970]];
      }
      [self updateConfig:uuidString];
    } else {
      self.isCompressSizeMode = NO;
    }
  }

  return self;
}

- (void)updateConfig:(NSString*)appUUIDString {
  self.appUUIDString = [appUUIDString copy];

  self.flutterAssets = [[FlutterDartProject flutterAssetsPath] copy];
  self.zipIcudtlFilePath = [[[[NSBundle mainBundle] pathForResource:@"Frameworks/App.framework"
                                                             ofType:@""]
      stringByAppendingPathComponent:@"flutter_compress_icudtl.zip"] copy];
  self.zipAssetsFilePath = [[[[NSBundle mainBundle] pathForResource:@"Frameworks/App.framework"
                                                             ofType:@""]
      stringByAppendingPathComponent:@"flutter_compress_assets.zip"] copy];

  self.cacheDirectoryForCurrentUUID = [[self.cacheDirectoryForDecompressedData
      stringByAppendingPathComponent:self.appUUIDString] copy];
  self.vmDataPath =
      [[self.cacheDirectoryForCurrentUUID stringByAppendingPathComponent:kVMDataFileName] copy];
  self.isolateDataPath = [[self.cacheDirectoryForCurrentUUID
      stringByAppendingPathComponent:kIsolateDataFileName] copy];
  self.icudtlDataPath =
      [[self.cacheDirectoryForCurrentUUID stringByAppendingPathComponent:kIcudtlDataFileName] copy];
  self.assetsPath =
      [[self.cacheDirectoryForCurrentUUID stringByAppendingPathComponent:self.flutterAssets] copy];
  self.assetsFlagPath =
      [[self.cacheDirectoryForCurrentUUID stringByAppendingPathComponent:kAssetsFlagFileName] copy];
  self.icudtlFlagPath =
      [[self.cacheDirectoryForCurrentUUID stringByAppendingPathComponent:kIcudtlFlagFileName] copy];
}

#pragma mark - 解压Data和Assets

- (void)decompressDataAsyncIfNeeded:(BOOL)isPredecompressMode
                            monitor:(FlutterCompressSizeModeMonitor)monitor {
  dispatch_async(self->_serialDecompressQueue, ^{
    [self decompressDataIfNeeded:isPredecompressMode monitor:monitor];
  });
}

- (void)decompressDataIfNeeded:(BOOL)isPredecompressMode
                       monitor:(FlutterCompressSizeModeMonitor)monitor {
  NSError* internalError = nil;
  BOOL succeeded = YES;
  BOOL needDecompress = NO;

  if ([self isDecompressDataDamaged]) {
    needDecompress = YES;
    succeeded = [self decompressData:&internalError];
  }

  dispatch_async(dispatch_get_main_queue(), ^{
    if (monitor) {
      monitor(needDecompress, isPredecompressMode, succeeded, internalError);
    }
  });
}

- (void)removePreviousDecompressedDataAsync {
  dispatch_async(self->_serialDecompressQueue, ^{
    [self removePreviousDecompressedData];
  });
}

- (void)removePreviousDecompressedData {
  // 存在已经解压的和当前UUID不一致的数据，则清除这些历史数据
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSString* cacheDirectoryForDecompressedData = self.cacheDirectoryForDecompressedData;
  BOOL isDirectory = false;
  BOOL succeeded = [fileManager fileExistsAtPath:cacheDirectoryForDecompressedData
                                     isDirectory:&isDirectory];
  if (!succeeded || !isDirectory) {
    return;
  }
  NSArray<NSString*>* files =
      [fileManager contentsOfDirectoryAtPath:cacheDirectoryForDecompressedData error:nil];
  for (NSString* fileName in files) {
    if (!self.appUUIDString || ![fileName isEqualToString:self.appUUIDString]) {
      NSString* obsoleteFilePath =
          [cacheDirectoryForDecompressedData stringByAppendingPathComponent:fileName];
      [fileManager removeItemAtPath:obsoleteFilePath error:nil];
    }
  }
}

- (BOOL)decompressData:(NSError**)error {
  NSFileManager* fileManager = [NSFileManager defaultManager];

  // 根据App.framework的UUID创建解压缩数据的目录
  BOOL succeeded = [fileManager createDirectoryAtPath:self.cacheDirectoryForCurrentUUID
                          withIntermediateDirectories:YES
                                           attributes:nil
                                                error:error];
  if (!succeeded) {
    return NO;
  }

  // 解压缩vm_data
  if (![self isVMDataStable]) {
    succeeded = [self decompressVMData:error];
    if (!succeeded) {
      return NO;
    }
  }

  // 解压缩isolate_data
  if (![self isIsolateDataStable]) {
    succeeded = [self decompressIsolateData:error];
    if (!succeeded) {
      return NO;
    }
  }

  // 解压缩icudtl
  if (![self isIcudtlDecompressedFileValid]) {
    succeeded = [self decompressIcudtlData:error];
    if (!succeeded) {
      return NO;
    }
  }

  // 解压缩assets
  if (![self isAssetsDecompressedFileValid]) {
    succeeded = [self decompressAssetsData:error];
    if (!succeeded) {
      return NO;
    }
  }

  return YES;
}

- (BOOL)decompressVMData:(NSError**)error {
  BOOL succeeded = [[NSFileManager defaultManager] createFileAtPath:self.vmDataPath
                                                           contents:[self vmData]
                                                         attributes:nil];
  if (!succeeded) {
    if (error) {
      *error = [NSError errorWithDomain:FlutterCompressSizeModeErrorDomain
                                   code:FlutterCompressSizeModeErrorCodeFailedToWriteVMData
                               userInfo:@{NSLocalizedDescriptionKey : @"存储vm data数据失败"}];
    }
    return NO;
  } else {
    NSString* fileMD5 = [self MD5WithContentsOfFile:self.vmDataPath];
    {
      FlutterCompressSizeModeManagerLock(self->_mutexLock);
      self.unzipVmDataMD5 = [fileMD5 copy];
    }
    return YES;
  }
}

- (BOOL)decompressIsolateData:(NSError**)error {
  BOOL succeeded = [[NSFileManager defaultManager] createFileAtPath:self.isolateDataPath
                                                           contents:[self isolateData]
                                                         attributes:nil];
  if (!succeeded) {
    if (error) {
      *error = [NSError errorWithDomain:FlutterCompressSizeModeErrorDomain
                                   code:FlutterCompressSizeModeErrorCodeFailedToWriteIsolateData
                               userInfo:@{NSLocalizedDescriptionKey : @"存储isolate data数据失败"}];
    }
    return NO;
  } else {
    NSString* fileMD5 = [self MD5WithContentsOfFile:self.isolateDataPath];
    {
      FlutterCompressSizeModeManagerLock(self->_mutexLock);
      self.unzipIsolateValidateMD5 = [fileMD5 copy];
    }
    return YES;
  }
}

- (BOOL)decompressIcudtlData:(NSError**)error {
  BOOL succeeded = [FlutterZipArchive unzipFileAtPath:self.zipIcudtlFilePath
                                        toDestination:self.cacheDirectoryForCurrentUUID
                                            overwrite:YES
                                             password:nil
                                                error:error];
  if (!succeeded) {
    return NO;
  }

  succeeded = [[NSFileManager defaultManager]
      createFileAtPath:self.icudtlFlagPath
              contents:[@"YES" dataUsingEncoding:NSUTF8StringEncoding]
            attributes:nil];

  if (!succeeded) {
    if (error) {
      *error = [NSError errorWithDomain:FlutterCompressSizeModeErrorDomain
                                   code:FlutterCompressSizeModeErrorCodeFailedToWriteIcudtlData
                               userInfo:@{NSLocalizedDescriptionKey : @"存储解压后的Icudtl失败"}];
    }
    return NO;
  } else {
    return YES;
  }
}

- (BOOL)decompressAssetsData:(NSError**)error {
  BOOL succeeded = [FlutterZipArchive unzipFileAtPath:self.zipAssetsFilePath
                                        toDestination:self.cacheDirectoryForCurrentUUID
                                            overwrite:YES
                                             password:nil
                                                error:error];
  if (!succeeded) {
    return NO;
  }

  succeeded = [[NSFileManager defaultManager]
      createFileAtPath:self.assetsFlagPath
              contents:[@"YES" dataUsingEncoding:NSUTF8StringEncoding]
            attributes:nil];

  if (!succeeded) {
    if (error) {
      *error = [NSError errorWithDomain:FlutterCompressSizeModeErrorDomain
                                   code:FlutterCompressSizeModeErrorCodeFailedToWriteAssetsData
                               userInfo:@{NSLocalizedDescriptionKey : @"存储解压后的Assets失败"}];
    }
    return NO;
  } else {
    return YES;
  }
}

#pragma mark - Memory Warning

- (void)onMemoryWarning:(NSNotification*)notification {
  [self setVMData:nil];
  [self setIsolateData:nil];
}

#pragma mark - vm data & isolate data

- (NSData*)vmData {
  NSData* res = nil;

  {
    FlutterCompressSizeModeManagerLock(self->_mutexLock);
    res = _vmData.get();
  }

  if (!res) {
    unsigned long size = 0;
    uint8_t* sectionData =
        getsectiondata((flutter_mach_header*)kMachHeader, kSegmentName, kVMDataSectionName, &size);
    NSData* zippedData = [[[NSData alloc] initWithBytes:sectionData length:size] autorelease];
    res = [zippedData flutter_gunzippedData];
  }

  {
    FlutterCompressSizeModeManagerLock(self->_mutexLock);
    if (!_vmData.get()) {
      _vmData.reset([res retain]);
    }
  }

  return res;
}

- (NSData*)isolateData {
  NSData* res = nil;

  {
    FlutterCompressSizeModeManagerLock(self->_mutexLock);
    res = _isolateData.get();
  }

  if (!res) {
    unsigned long size = 0;
    uint8_t* sectionData = getsectiondata((flutter_mach_header*)kMachHeader, kSegmentName,
                                          kIsolateDataSectionName, &size);
    NSData* zippedData = [[[NSData alloc] initWithBytes:sectionData length:size] autorelease];
    res = [zippedData flutter_gunzippedData];
  }

  {
    FlutterCompressSizeModeManagerLock(self->_mutexLock);
    if (!_isolateData.get()) {
      _isolateData.reset([res retain]);
    }
  }

  return res;
}

- (void)setVMData:(NSData*)data {
  FlutterCompressSizeModeManagerLock(self->_mutexLock);
  _vmData.reset([data retain]);
}

- (void)setIsolateData:(NSData*)data {
  FlutterCompressSizeModeManagerLock(self->_mutexLock);
  _isolateData.reset([data retain]);
}

- (NSString*)validateMD5 {
  NSString* validateMD5 = nil;
  {
    FlutterCompressSizeModeManagerLock(self->_mutexLock);
    validateMD5 = _validateMD5;
  }
  if (!validateMD5) {
    unsigned long size = 0;
    uint8_t* sectionData = getsectiondata((flutter_mach_header*)kMachHeader, kSegmentName,
                                          kValidateSectionName, &size);
    NSData* validateData = [[[NSData alloc] initWithBytes:sectionData length:size] autorelease];
    validateMD5 = [[[NSString alloc] initWithData:validateData
                                         encoding:NSUTF8StringEncoding] autorelease];
  }
  {
    FlutterCompressSizeModeManagerLock(self->_mutexLock);
    if (!_validateMD5) {
      _validateMD5 = [validateMD5 copy];
    }
  }
  return _validateMD5;
}

#pragma mark - 判断各项压缩数据是否需要解压到磁盘

- (BOOL)needDecompressData {
  if (self.isCompressSizeMode) {
    return !([self isVMDataDecompressedFileValid] && [self isIsolateDataDecompressedFileValid] &&
             [self isIcudtlDecompressedFileValid] && [self isAssetsDecompressedFileValid]);
  } else {
    return NO;
  }
}

- (BOOL)isDecompressDataDamaged {
  if (self.isCompressSizeMode) {
    return !([self isVMDataStable] && [self isIsolateDataStable] &&
             [self isIcudtlDecompressedFileValid] && [self isAssetsDecompressedFileValid]);
  } else {
    return NO;
  }
}

- (BOOL)isIsolateDataStable {
  NSArray* parts = [self.validateMD5 componentsSeparatedByString:@"_"];
  NSString* isolateMD5 = parts.firstObject;
  return [isolateMD5 isEqualToString:self.unzipIsolateValidateMD5];
}

- (BOOL)isVMDataStable {
  NSArray* parts = [self.validateMD5 componentsSeparatedByString:@"_"];
  NSString* vmMD5 = parts.lastObject;
  return [vmMD5 isEqualToString:self.unzipVmDataMD5];
}

- (NSString*)unzipVmDataMD5 {
  NSString* unzipVmDataMD5 = nil;
  {
    FlutterCompressSizeModeManagerLock(self->_mutexLock);
    unzipVmDataMD5 = _unzipVmDataMD5;
  }
  if (!unzipVmDataMD5) {
    unzipVmDataMD5 = [self MD5WithContentsOfFile:_vmDataPath];
  }
  {
    FlutterCompressSizeModeManagerLock(self->_mutexLock);
    if (!_unzipVmDataMD5) {
      _unzipVmDataMD5 = [unzipVmDataMD5 copy];
    }
  }
  return _unzipVmDataMD5;
}

- (NSString*)unzipIsolateValidateMD5 {
  NSString* unzipIsolateValidateMD5 = nil;
  {
    FlutterCompressSizeModeManagerLock(self->_mutexLock);
    unzipIsolateValidateMD5 = _unzipIsolateValidateMD5;
  }
  if (!unzipIsolateValidateMD5) {
    unzipIsolateValidateMD5 = [self MD5WithContentsOfFile:self.isolateDataPath];
  }
  {
    FlutterCompressSizeModeManagerLock(self->_mutexLock);
    if (!_unzipIsolateValidateMD5) {
      _unzipIsolateValidateMD5 = [unzipIsolateValidateMD5 copy];
    }
  }
  return _unzipIsolateValidateMD5;
}

- (NSString*)MD5WithContentsOfFile:(NSString*)filePath {
  NSData* fileData = [NSData dataWithContentsOfFile:filePath];
  if (fileData) {
    return [fileData MD5];
  }
  return nil;
}

- (BOOL)isVMDataDecompressedFileValid {
  return [[NSFileManager defaultManager] fileExistsAtPath:self.vmDataPath];
}

- (BOOL)isIsolateDataDecompressedFileValid {
  return [[NSFileManager defaultManager] fileExistsAtPath:self.isolateDataPath];
}

- (BOOL)isIcudtlDecompressedFileValid {
  return [[NSFileManager defaultManager] fileExistsAtPath:self.icudtlDataPath] &&
         [[NSFileManager defaultManager] fileExistsAtPath:self.icudtlFlagPath];
}

- (BOOL)isAssetsDecompressedFileValid {
  return [[NSFileManager defaultManager] fileExistsAtPath:self.assetsPath] &&
         [[NSFileManager defaultManager] fileExistsAtPath:self.assetsFlagPath];
}

#pragma mark - 更新FlutterDartProject Settings

- (void)updateSettingsIfNeeded:(flutter::Settings&)settings
                       monitor:(FlutterCompressSizeModeMonitor _Nullable)monitor {
  if ([self isDecompressDataDamaged]) {
    [self decompressDataAsyncIfNeeded:NO monitor:monitor];
  }
  [self removePreviousDecompressedDataAsync];

  if (self.isCompressSizeMode) {
    settings.vm_snapshot_data = [self vmSnapshotDataCallback];
    settings.isolate_snapshot_data = [self isolateSnapshotDataCallback];
    settings.icu_mapper = [self icuMapperCallback];

    if ([self isAssetsDecompressedFileValid] && self.assetsPath.length > 0) {
      settings.assets_path = self.assetsPath.UTF8String;
    } else {
      settings.zip_assets_file_path = self.zipAssetsFilePath.UTF8String;
      settings.zip_assets_directory = self.flutterAssets.UTF8String;
    }
  }
}

- (flutter::MappingCallback)vmSnapshotDataCallback {
  if ([self isVMDataStable]) {
    return [scoped_manager =
                fml::scoped_nsobject<FlutterCompressSizeModeManager>([self retain])]() {
      return std::make_unique<fml::FileMapping>(
          fml::OpenFile(scoped_manager.get().vmDataPath.UTF8String, false,
                        fml::FilePermission::kRead),
          std::initializer_list<fml::FileMapping::Protection>{fml::FileMapping::Protection::kRead});
    };
  } else {
    return [scoped_manager =
                fml::scoped_nsobject<FlutterCompressSizeModeManager>([self retain])]() {
      NSData* vmData = [scoped_manager.get() vmData];
      const uint8_t* bytes = (const uint8_t*)vmData.bytes;
      return std::make_unique<fml::DataMapping>(std::vector<uint8_t>{bytes, bytes + vmData.length});
    };
  }
}

- (flutter::MappingCallback)isolateSnapshotDataCallback {
  if ([self isIsolateDataStable]) {
    return [scoped_manager =
                fml::scoped_nsobject<FlutterCompressSizeModeManager>([self retain])]() {
      return std::make_unique<fml::FileMapping>(
          fml::OpenFile(scoped_manager.get().isolateDataPath.UTF8String, false,
                        fml::FilePermission::kRead),
          std::initializer_list<fml::FileMapping::Protection>{fml::FileMapping::Protection::kRead});
    };
  } else {
    return
        [scoped_manager = fml::scoped_nsobject<FlutterCompressSizeModeManager>([self retain])]() {
          NSData* isolateData = [scoped_manager.get() isolateData];
          const uint8_t* bytes = (const uint8_t*)isolateData.bytes;
          return std::make_unique<fml::DataMapping>(
              std::vector<uint8_t>{bytes, bytes + isolateData.length});
        };
  }
}

- (flutter::MappingCallback)icuMapperCallback {
  if ([self isIcudtlDecompressedFileValid]) {
    return [scoped_manager =
                fml::scoped_nsobject<FlutterCompressSizeModeManager>([self retain])]() {
      return std::make_unique<fml::FileMapping>(
          fml::OpenFile(scoped_manager.get().icudtlDataPath.UTF8String, false,
                        fml::FilePermission::kRead),
          std::initializer_list<fml::FileMapping::Protection>{fml::FileMapping::Protection::kRead});
    };
  } else {
    return [scoped_manager =
                fml::scoped_nsobject<FlutterCompressSizeModeManager>([self retain])]() {
      flutter::ZipAssetStore zipAssetStore(scoped_manager.get().zipIcudtlFilePath.UTF8String, "");
      return zipAssetStore.GetAsMapping(kIcudtlDataFileName.UTF8String);
    };
  }
}

@end
// END
