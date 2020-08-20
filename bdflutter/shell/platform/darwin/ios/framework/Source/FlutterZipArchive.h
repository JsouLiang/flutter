//
//  FlutterZipArchive.h
//  FlutterZipArchive
//
//  Created by Sam Soffes on 7/21/10.
//  Copyright (c) Sam Soffes 2010-2015. All rights reserved.
//

// BD ADD: START
#ifndef _FLUTTERZIPARCHIVE_H
#define _FLUTTERZIPARCHIVE_H

#import <Foundation/Foundation.h>
#include "third_party/zlib/contrib/minizip/unzip.h"

NS_ASSUME_NONNULL_BEGIN

@protocol FlutterZipArchiveDelegate;

@interface FlutterZipArchive : NSObject

// Unzip
+ (BOOL)unzipFileAtPath:(NSString*)path toDestination:(NSString*)destination;
+ (BOOL)unzipFileAtPath:(NSString*)path
          toDestination:(NSString*)destination
               delegate:(nullable id<FlutterZipArchiveDelegate>)delegate;

+ (BOOL)unzipFileAtPath:(NSString*)path
          toDestination:(NSString*)destination
              overwrite:(BOOL)overwrite
               password:(nullable NSString*)password
                  error:(NSError**)error;

+ (BOOL)unzipFileAtPath:(NSString*)path
          toDestination:(NSString*)destination
              overwrite:(BOOL)overwrite
               password:(nullable NSString*)password
                  error:(NSError**)error
               delegate:(nullable id<FlutterZipArchiveDelegate>)delegate NS_REFINED_FOR_SWIFT;

+ (BOOL)unzipFileAtPath:(NSString*)path
          toDestination:(NSString*)destination
     preserveAttributes:(BOOL)preserveAttributes
              overwrite:(BOOL)overwrite
               password:(nullable NSString*)password
                  error:(NSError**)error
               delegate:(nullable id<FlutterZipArchiveDelegate>)delegate;

+ (BOOL)unzipFileAtPath:(NSString*)path
          toDestination:(NSString*)destination
        progressHandler:(void (^_Nullable)(NSString* entry,
                                           unz_file_info zipInfo,
                                           long entryNumber,
                                           long total))progressHandler
      completionHandler:(void (^_Nullable)(NSString* path,
                                           BOOL succeeded,
                                           NSError* _Nullable error))completionHandler;

+ (BOOL)unzipFileAtPath:(NSString*)path
          toDestination:(NSString*)destination
              overwrite:(BOOL)overwrite
               password:(nullable NSString*)password
        progressHandler:(void (^_Nullable)(NSString* entry,
                                           unz_file_info zipInfo,
                                           long entryNumber,
                                           long total))progressHandler
      completionHandler:(void (^_Nullable)(NSString* path,
                                           BOOL succeeded,
                                           NSError* _Nullable error))completionHandler;

+ (BOOL)unzipFileAtPath:(NSString*)path
          toDestination:(NSString*)destination
     preserveAttributes:(BOOL)preserveAttributes
              overwrite:(BOOL)overwrite
         nestedZipLevel:(NSInteger)nestedZipLevel
               password:(nullable NSString*)password
                  error:(NSError**)error
               delegate:(nullable id<FlutterZipArchiveDelegate>)delegate
        progressHandler:(void (^_Nullable)(NSString* entry,
                                           unz_file_info zipInfo,
                                           long entryNumber,
                                           long total))progressHandler
      completionHandler:(void (^_Nullable)(NSString* path,
                                           BOOL succeeded,
                                           NSError* _Nullable error))completionHandler;

@end

@protocol FlutterZipArchiveDelegate <NSObject>

@optional

- (void)zipArchiveWillUnzipArchiveAtPath:(NSString*)path zipInfo:(unz_global_info)zipInfo;
- (void)zipArchiveDidUnzipArchiveAtPath:(NSString*)path
                                zipInfo:(unz_global_info)zipInfo
                           unzippedPath:(NSString*)unzippedPath;

- (BOOL)zipArchiveShouldUnzipFileAtIndex:(NSInteger)fileIndex
                              totalFiles:(NSInteger)totalFiles
                             archivePath:(NSString*)archivePath
                                fileInfo:(unz_file_info)fileInfo;
- (void)zipArchiveWillUnzipFileAtIndex:(NSInteger)fileIndex
                            totalFiles:(NSInteger)totalFiles
                           archivePath:(NSString*)archivePath
                              fileInfo:(unz_file_info)fileInfo;
- (void)zipArchiveDidUnzipFileAtIndex:(NSInteger)fileIndex
                           totalFiles:(NSInteger)totalFiles
                          archivePath:(NSString*)archivePath
                             fileInfo:(unz_file_info)fileInfo;
- (void)zipArchiveDidUnzipFileAtIndex:(NSInteger)fileIndex
                           totalFiles:(NSInteger)totalFiles
                          archivePath:(NSString*)archivePath
                     unzippedFilePath:(NSString*)unzippedFilePath;

- (void)zipArchiveProgressEvent:(unsigned long long)loaded total:(unsigned long long)total;

@end

NS_ASSUME_NONNULL_END

#endif /* _FLUTTERZIPARCHIVE_H */
// END
