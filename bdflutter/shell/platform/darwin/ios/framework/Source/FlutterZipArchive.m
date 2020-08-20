//
//  FlutterZipArchive.m
//  FlutterZipArchive
//
//  Created by Sam Soffes on 7/21/10.
//  Copyright (c) Sam Soffes 2010-2015. All rights reserved.
//

// BD ADD: START
#import "FlutterZipArchive.h"
#include <sys/stat.h>
#include "flutter/shell/platform/darwin/ios/framework/Headers/FlutterDartProject.h"
#include "third_party/zlib/contrib/minizip/zip.h"

BOOL _fileIsSymbolicLink(const unz_file_info* fileInfo);

#ifndef API_AVAILABLE
// Xcode 7- compatibility
#define API_AVAILABLE(...)
#endif

@interface NSData (FlutterZipArchive)
- (NSString*)_base64RFC4648 API_AVAILABLE(macos(10.9), ios(7.0), watchos(2.0), tvos(9.0));
- (NSString*)_hexString;
@end

@interface NSString (FlutterZipArchive)
- (NSString*)_sanitizedPath;
@end

@implementation FlutterZipArchive

#pragma mark - Unzipping

+ (BOOL)unzipFileAtPath:(NSString*)path toDestination:(NSString*)destination {
  return [self unzipFileAtPath:path toDestination:destination delegate:nil];
}

+ (BOOL)unzipFileAtPath:(NSString*)path
          toDestination:(NSString*)destination
              overwrite:(BOOL)overwrite
               password:(nullable NSString*)password
                  error:(NSError**)error {
  return [self unzipFileAtPath:path
                 toDestination:destination
            preserveAttributes:YES
                     overwrite:overwrite
                      password:password
                         error:error
                      delegate:nil
               progressHandler:nil
             completionHandler:nil];
}

+ (BOOL)unzipFileAtPath:(NSString*)path
          toDestination:(NSString*)destination
               delegate:(nullable id<FlutterZipArchiveDelegate>)delegate {
  return [self unzipFileAtPath:path
                 toDestination:destination
            preserveAttributes:YES
                     overwrite:YES
                      password:nil
                         error:nil
                      delegate:delegate
               progressHandler:nil
             completionHandler:nil];
}

+ (BOOL)unzipFileAtPath:(NSString*)path
          toDestination:(NSString*)destination
              overwrite:(BOOL)overwrite
               password:(nullable NSString*)password
                  error:(NSError**)error
               delegate:(nullable id<FlutterZipArchiveDelegate>)delegate {
  return [self unzipFileAtPath:path
                 toDestination:destination
            preserveAttributes:YES
                     overwrite:overwrite
                      password:password
                         error:error
                      delegate:delegate
               progressHandler:nil
             completionHandler:nil];
}

+ (BOOL)unzipFileAtPath:(NSString*)path
          toDestination:(NSString*)destination
              overwrite:(BOOL)overwrite
               password:(NSString*)password
        progressHandler:(void (^)(NSString* entry,
                                  unz_file_info zipInfo,
                                  long entryNumber,
                                  long total))progressHandler
      completionHandler:
          (void (^)(NSString* path, BOOL succeeded, NSError* _Nullable error))completionHandler {
  return [self unzipFileAtPath:path
                 toDestination:destination
            preserveAttributes:YES
                     overwrite:overwrite
                      password:password
                         error:nil
                      delegate:nil
               progressHandler:progressHandler
             completionHandler:completionHandler];
}

+ (BOOL)unzipFileAtPath:(NSString*)path
          toDestination:(NSString*)destination
        progressHandler:(void (^_Nullable)(NSString* entry,
                                           unz_file_info zipInfo,
                                           long entryNumber,
                                           long total))progressHandler
      completionHandler:(void (^_Nullable)(NSString* path,
                                           BOOL succeeded,
                                           NSError* _Nullable error))completionHandler {
  return [self unzipFileAtPath:path
                 toDestination:destination
            preserveAttributes:YES
                     overwrite:YES
                      password:nil
                         error:nil
                      delegate:nil
               progressHandler:progressHandler
             completionHandler:completionHandler];
}

+ (BOOL)unzipFileAtPath:(NSString*)path
          toDestination:(NSString*)destination
     preserveAttributes:(BOOL)preserveAttributes
              overwrite:(BOOL)overwrite
               password:(nullable NSString*)password
                  error:(NSError**)error
               delegate:(nullable id<FlutterZipArchiveDelegate>)delegate {
  return [self unzipFileAtPath:path
                 toDestination:destination
            preserveAttributes:preserveAttributes
                     overwrite:overwrite
                      password:password
                         error:error
                      delegate:delegate
               progressHandler:nil
             completionHandler:nil];
}

+ (BOOL)unzipFileAtPath:(NSString*)path
          toDestination:(NSString*)destination
     preserveAttributes:(BOOL)preserveAttributes
              overwrite:(BOOL)overwrite
               password:(nullable NSString*)password
                  error:(NSError**)error
               delegate:(nullable id<FlutterZipArchiveDelegate>)delegate
        progressHandler:(void (^_Nullable)(NSString* entry,
                                           unz_file_info zipInfo,
                                           long entryNumber,
                                           long total))progressHandler
      completionHandler:(void (^_Nullable)(NSString* path,
                                           BOOL succeeded,
                                           NSError* _Nullable error))completionHandler {
  return [self unzipFileAtPath:path
                 toDestination:destination
            preserveAttributes:preserveAttributes
                     overwrite:overwrite
                nestedZipLevel:0
                      password:password
                         error:error
                      delegate:delegate
               progressHandler:progressHandler
             completionHandler:completionHandler];
}

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
                                           NSError* _Nullable error))completionHandler {
  // Guard against empty strings
  if (path.length == 0 || destination.length == 0) {
    NSDictionary* userInfo = @{NSLocalizedDescriptionKey : @"received invalid argument(s)"};
    NSError* err = [NSError errorWithDomain:FlutterCompressSizeModeErrorDomain
                                       code:FlutterCompressSizeModeErrorCodeInvalidArguments
                                   userInfo:userInfo];
    if (error) {
      *error = err;
    }
    if (completionHandler) {
      completionHandler(nil, NO, err);
    }
    return NO;
  }

  // Begin opening
  zipFile zip = unzOpen(path.fileSystemRepresentation);
  if (zip == NULL) {
    NSDictionary* userInfo = @{NSLocalizedDescriptionKey : @"failed to open zip file"};
    NSError* err = [NSError errorWithDomain:FlutterCompressSizeModeErrorDomain
                                       code:FlutterCompressSizeModeErrorCodeFailedOpenZipFile
                                   userInfo:userInfo];
    if (error) {
      *error = err;
    }
    if (completionHandler) {
      completionHandler(nil, NO, err);
    }
    return NO;
  }

  NSDictionary* fileAttributes = [[NSFileManager defaultManager] attributesOfItemAtPath:path
                                                                                  error:nil];
  unsigned long long fileSize = [[fileAttributes objectForKey:NSFileSize] unsignedLongLongValue];
  unsigned long long currentPosition = 0;

  unz_global_info globalInfo = {};
  unzGetGlobalInfo(zip, &globalInfo);

  // Begin unzipping
  int ret = 0;
  ret = unzGoToFirstFile(zip);
  if (ret != UNZ_OK && ret != UNZ_END_OF_LIST_OF_FILE) {
    NSDictionary* userInfo =
        @{NSLocalizedDescriptionKey : @"failed to open first file in zip file"};
    NSError* err = [NSError errorWithDomain:FlutterCompressSizeModeErrorDomain
                                       code:FlutterCompressSizeModeErrorCodeFailedOpenFileInZip
                                   userInfo:userInfo];
    if (error) {
      *error = err;
    }
    if (completionHandler) {
      completionHandler(nil, NO, err);
    }
    unzClose(zip);
    return NO;
  }

  BOOL success = YES;
  BOOL canceled = NO;
  int crc_ret = 0;
  unsigned char buffer[4096] = {0};
  NSFileManager* fileManager = [NSFileManager defaultManager];
  NSMutableArray<NSDictionary*>* directoriesModificationDates =
      [[[NSMutableArray alloc] init] autorelease];

  // Message delegate
  if ([delegate respondsToSelector:@selector(zipArchiveWillUnzipArchiveAtPath:zipInfo:)]) {
    [delegate zipArchiveWillUnzipArchiveAtPath:path zipInfo:globalInfo];
  }
  if ([delegate respondsToSelector:@selector(zipArchiveProgressEvent:total:)]) {
    [delegate zipArchiveProgressEvent:currentPosition total:fileSize];
  }

  NSInteger currentFileNumber = -1;
  NSError* unzippingError = nil;
  do {
    currentFileNumber++;
    if (ret == UNZ_END_OF_LIST_OF_FILE) {
      break;
    }
    @autoreleasepool {
      if (password.length == 0) {
        ret = unzOpenCurrentFile(zip);
      } else {
        ret = unzOpenCurrentFilePassword(zip, [password cStringUsingEncoding:NSUTF8StringEncoding]);
      }

      if (ret != UNZ_OK) {
        unzippingError = [[NSError
            errorWithDomain:FlutterCompressSizeModeErrorDomain
                       code:FlutterCompressSizeModeErrorCodeFailedOpenFileInZip
                   userInfo:@{NSLocalizedDescriptionKey : @"failed to open file in zip file"}]
            retain];
        success = NO;
        break;
      }

      // Reading data and write to file
      unz_file_info fileInfo;
      memset(&fileInfo, 0, sizeof(unz_file_info));

      ret = unzGetCurrentFileInfo(zip, &fileInfo, NULL, 0, NULL, 0, NULL, 0);
      if (ret != UNZ_OK) {
        unzippingError = [[NSError
            errorWithDomain:FlutterCompressSizeModeErrorDomain
                       code:FlutterCompressSizeModeErrorCodeFileInfoNotLoadable
                   userInfo:@{NSLocalizedDescriptionKey : @"failed to retrieve info for file"}]
            retain];
        success = NO;
        unzCloseCurrentFile(zip);
        break;
      }

      currentPosition += fileInfo.compressed_size;

      // Message delegate
      if ([delegate respondsToSelector:@selector
                    (zipArchiveShouldUnzipFileAtIndex:totalFiles:archivePath:fileInfo:)]) {
        if (![delegate zipArchiveShouldUnzipFileAtIndex:currentFileNumber
                                             totalFiles:(NSInteger)globalInfo.number_entry
                                            archivePath:path
                                               fileInfo:fileInfo]) {
          success = NO;
          canceled = YES;
          break;
        }
      }
      if ([delegate respondsToSelector:@selector
                    (zipArchiveWillUnzipFileAtIndex:totalFiles:archivePath:fileInfo:)]) {
        [delegate zipArchiveWillUnzipFileAtIndex:currentFileNumber
                                      totalFiles:(NSInteger)globalInfo.number_entry
                                     archivePath:path
                                        fileInfo:fileInfo];
      }
      if ([delegate respondsToSelector:@selector(zipArchiveProgressEvent:total:)]) {
        [delegate zipArchiveProgressEvent:(NSInteger)currentPosition total:(NSInteger)fileSize];
      }

      char* filename = (char*)malloc(fileInfo.size_filename + 1);
      if (filename == NULL) {
        success = NO;
        break;
      }

      unzGetCurrentFileInfo(zip, &fileInfo, filename, fileInfo.size_filename + 1, NULL, 0, NULL, 0);
      filename[fileInfo.size_filename] = '\0';

      BOOL fileIsSymbolicLink = _fileIsSymbolicLink(&fileInfo);

      NSString* strPath = [FlutterZipArchive _filenameStringWithCString:filename
                                                        version_made_by:fileInfo.version
                                                   general_purpose_flag:fileInfo.flag
                                                                   size:fileInfo.size_filename];
      if ([strPath hasPrefix:@"__MACOSX/"]) {
        // ignoring resource forks: https://superuser.com/questions/104500/what-is-macosx-folder
        unzCloseCurrentFile(zip);
        ret = unzGoToNextFile(zip);
        free(filename);
        continue;
      }

      // Check if it contains directory
      BOOL isDirectory = NO;
      if (filename[fileInfo.size_filename - 1] == '/' ||
          filename[fileInfo.size_filename - 1] == '\\') {
        isDirectory = YES;
      }
      free(filename);

      // Sanitize paths in the file name.
      strPath = [strPath _sanitizedPath];
      if (!strPath.length) {
        // if filename data is unsalvageable, we default to currentFileNumber
        strPath = @(currentFileNumber).stringValue;
      }

      NSString* fullPath = [destination stringByAppendingPathComponent:strPath];
      NSError* err = nil;
      NSDictionary* directoryAttr = nil;
      if (preserveAttributes) {
        NSDate* modDate = [[self class] _dateWithMSDOSFormat:(UInt32)fileInfo.dosDate];
        directoryAttr = @{NSFileCreationDate : modDate, NSFileModificationDate : modDate};
        [directoriesModificationDates addObject:@{@"path" : fullPath, @"modDate" : modDate}];
      }
      if (isDirectory) {
        [fileManager createDirectoryAtPath:fullPath
               withIntermediateDirectories:YES
                                attributes:directoryAttr
                                     error:&err];
      } else {
        [fileManager createDirectoryAtPath:fullPath.stringByDeletingLastPathComponent
               withIntermediateDirectories:YES
                                attributes:directoryAttr
                                     error:&err];
      }
      if (err != nil) {
        if ([err.domain isEqualToString:NSCocoaErrorDomain] && err.code == 640) {
          unzippingError = [err retain];
          unzCloseCurrentFile(zip);
          success = NO;
          break;
        }
        NSLog(@"[FlutterZipArchive] Error: %@", err.localizedDescription);
      }

      if ([fileManager fileExistsAtPath:fullPath] && !isDirectory && !overwrite) {
        // FIXME: couldBe CRC Check?
        unzCloseCurrentFile(zip);
        ret = unzGoToNextFile(zip);
        continue;
      }

      if (isDirectory && !fileIsSymbolicLink) {
        // nothing to read/write for a directory
      } else if (!fileIsSymbolicLink) {
        // ensure we are not creating stale file entries
        int readBytes = unzReadCurrentFile(zip, buffer, 4096);
        if (readBytes >= 0) {
          FILE* fp = fopen(fullPath.fileSystemRepresentation, "wb");
          while (fp) {
            if (readBytes > 0) {
              if (0 == fwrite(buffer, readBytes, 1, fp)) {
                if (ferror(fp)) {
                  NSString* message =
                      [NSString stringWithFormat:@"Failed to write file (check your free space)"];
                  NSLog(@"[FlutterZipArchive] %@", message);
                  success = NO;
                  unzippingError =
                      [[NSError errorWithDomain:FlutterCompressSizeModeErrorDomain
                                           code:FlutterCompressSizeModeErrorCodeFailedToWriteFile
                                       userInfo:@{NSLocalizedDescriptionKey : message}] retain];
                  break;
                }
              }
            } else {
              break;
            }
            readBytes = unzReadCurrentFile(zip, buffer, 4096);
            if (readBytes < 0) {
              // Let's assume error Z_DATA_ERROR is caused by an invalid password
              // Let's assume other errors are caused by Content Not Readable
              success = NO;
            }
          }

          if (fp) {
            fclose(fp);

            if (nestedZipLevel && [fullPath.pathExtension.lowercaseString isEqualToString:@"zip"] &&
                [self unzipFileAtPath:fullPath
                         toDestination:fullPath.stringByDeletingLastPathComponent
                    preserveAttributes:preserveAttributes
                             overwrite:overwrite
                        nestedZipLevel:nestedZipLevel - 1
                              password:password
                                 error:nil
                              delegate:nil
                       progressHandler:nil
                     completionHandler:nil]) {
              [directoriesModificationDates removeLastObject];
              [[NSFileManager defaultManager] removeItemAtPath:fullPath error:nil];
            } else if (preserveAttributes) {
              // Set the original datetime property
              if (fileInfo.dosDate != 0) {
                NSDate* orgDate = [[self class] _dateWithMSDOSFormat:(UInt32)fileInfo.dosDate];
                NSDictionary* attr = @{NSFileModificationDate : orgDate};

                if (attr) {
                  if (![fileManager setAttributes:attr ofItemAtPath:fullPath error:nil]) {
                    // Can't set attributes
                    NSLog(@"[FlutterZipArchive] Failed to set attributes - whilst setting "
                          @"modification date");
                  }
                }
              }

              // Set the original permissions on the file (+read/write to solve #293)
              uLong permissions = fileInfo.external_fa >> 16 | 0b110000000;
              if (permissions != 0) {
                // Store it into a NSNumber
                NSNumber* permissionsValue = @(permissions);

                // Retrieve any existing attributes
                NSMutableDictionary* attrs = [[[NSMutableDictionary alloc]
                    initWithDictionary:[fileManager attributesOfItemAtPath:fullPath
                                                                     error:nil]] autorelease];

                // Set the value in the attributes dict
                [attrs setObject:permissionsValue forKey:NSFilePosixPermissions];

                // Update attributes
                if (![fileManager setAttributes:attrs ofItemAtPath:fullPath error:nil]) {
                  // Unable to set the permissions attribute
                  NSLog(
                      @"[FlutterZipArchive] Failed to set attributes - whilst setting permissions");
                }
              }
            }
          } else {
            // if we couldn't open file descriptor we can validate global errno to see the reason
            if (errno == ENOSPC) {
              NSError* enospcError = [NSError errorWithDomain:NSPOSIXErrorDomain
                                                         code:ENOSPC
                                                     userInfo:nil];
              unzippingError = [enospcError retain];
              unzCloseCurrentFile(zip);
              success = NO;
              break;
            }
          }
        } else {
          // Let's assume error Z_DATA_ERROR is caused by an invalid password
          // Let's assume other errors are caused by Content Not Readable
          success = NO;
          break;
        }
      } else {
        // Assemble the path for the symbolic link
        NSMutableString* destinationPath = [NSMutableString string];
        int bytesRead = 0;
        while ((bytesRead = unzReadCurrentFile(zip, buffer, 4096)) > 0) {
          buffer[bytesRead] = 0;
          [destinationPath appendString:@((const char*)buffer)];
        }
        if (bytesRead < 0) {
          // Let's assume error Z_DATA_ERROR is caused by an invalid password
          // Let's assume other errors are caused by Content Not Readable
          success = NO;
          break;
        }

        // Check if the symlink exists and delete it if we're overwriting
        if (overwrite) {
          if ([fileManager fileExistsAtPath:fullPath]) {
            NSError* error = nil;
            BOOL removeSuccess = [fileManager removeItemAtPath:fullPath error:&error];
            if (!removeSuccess) {
              NSString* message =
                  [NSString stringWithFormat:@"Failed to delete existing symbolic link at \"%@\"",
                                             error.localizedDescription];
              NSLog(@"[FlutterZipArchive] %@", message);
              success = NO;
              unzippingError =
                  [[NSError errorWithDomain:FlutterCompressSizeModeErrorDomain
                                       code:error.code
                                   userInfo:@{NSLocalizedDescriptionKey : message}] retain];
            }
          }
        }

        // Create the symbolic link (making sure it stays relative if it was relative before)
        int symlinkError = symlink([destinationPath cStringUsingEncoding:NSUTF8StringEncoding],
                                   [fullPath cStringUsingEncoding:NSUTF8StringEncoding]);

        if (symlinkError != 0) {
          // Bubble the error up to the completion handler
          NSString* message = [NSString
              stringWithFormat:
                  @"Failed to create symbolic link at \"%@\" to \"%@\" - symlink() error code: %d",
                  fullPath, destinationPath, errno];
          NSLog(@"[FlutterZipArchive] %@", message);
          success = NO;
          unzippingError =
              [[NSError errorWithDomain:NSPOSIXErrorDomain
                                   code:symlinkError
                               userInfo:@{NSLocalizedDescriptionKey : message}] retain];
        }
      }

      crc_ret = unzCloseCurrentFile(zip);
      if (crc_ret == UNZ_CRCERROR) {
        // CRC ERROR
        success = NO;
        break;
      }
      ret = unzGoToNextFile(zip);

      // Message delegate
      if ([delegate respondsToSelector:@selector
                    (zipArchiveDidUnzipFileAtIndex:totalFiles:archivePath:fileInfo:)]) {
        [delegate zipArchiveDidUnzipFileAtIndex:currentFileNumber
                                     totalFiles:(NSInteger)globalInfo.number_entry
                                    archivePath:path
                                       fileInfo:fileInfo];
      } else if ([delegate respondsToSelector:@selector
                           (zipArchiveDidUnzipFileAtIndex:
                                               totalFiles:archivePath:unzippedFilePath:)]) {
        [delegate zipArchiveDidUnzipFileAtIndex:currentFileNumber
                                     totalFiles:(NSInteger)globalInfo.number_entry
                                    archivePath:path
                               unzippedFilePath:fullPath];
      }

      if (progressHandler) {
        progressHandler(strPath, fileInfo, currentFileNumber, globalInfo.number_entry);
      }
    }
  } while (ret == UNZ_OK && success);

  // Close
  unzClose(zip);

  // The process of decompressing the .zip archive causes the modification times on the folders
  // to be set to the present time. So, when we are done, they need to be explicitly set.
  // set the modification date on all of the directories.
  if (success && preserveAttributes) {
    NSError* err = nil;
    for (NSDictionary* d in directoriesModificationDates) {
      if (![[NSFileManager defaultManager]
              setAttributes:@{NSFileModificationDate : [d objectForKey:@"modDate"]}
               ofItemAtPath:[d objectForKey:@"path"]
                      error:&err]) {
        NSLog(@"[FlutterZipArchive] Set attributes failed for directory: %@.",
              [d objectForKey:@"path"]);
      }
      if (err) {
        NSLog(@"[FlutterZipArchive] Error setting directory file modification date attribute: %@",
              err.localizedDescription);
      }
    }
  }

  // Message delegate
  if (success && [delegate respondsToSelector:@selector
                           (zipArchiveDidUnzipArchiveAtPath:zipInfo:unzippedPath:)]) {
    [delegate zipArchiveDidUnzipArchiveAtPath:path zipInfo:globalInfo unzippedPath:destination];
  }
  // final progress event = 100%
  if (!canceled && [delegate respondsToSelector:@selector(zipArchiveProgressEvent:total:)]) {
    [delegate zipArchiveProgressEvent:fileSize total:fileSize];
  }

  NSError* retErr = nil;
  if (crc_ret == UNZ_CRCERROR) {
    NSDictionary* userInfo = @{NSLocalizedDescriptionKey : @"crc check failed for file"};
    retErr = [NSError errorWithDomain:FlutterCompressSizeModeErrorDomain
                                 code:FlutterCompressSizeModeErrorCodeFileInfoNotLoadable
                             userInfo:userInfo];
  }

  if (unzippingError) {
    [unzippingError autorelease];
  }

  if (error) {
    if (unzippingError) {
      *error = unzippingError;
    } else {
      *error = retErr;
    }
  }
  if (completionHandler) {
    if (unzippingError) {
      completionHandler(path, success, unzippingError);
    } else {
      completionHandler(path, success, retErr);
    }
  }
  return success;
}

#pragma mark - Private

+ (NSString*)_filenameStringWithCString:(const char*)filename
                        version_made_by:(uint16_t)version_made_by
                   general_purpose_flag:(uint16_t)flag
                                   size:(uint16_t)size_filename {
  // Respect Language encoding flag only reading filename as UTF-8 when this is set
  // when file entry created on dos system.
  //
  // https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT
  //   Bit 11: Language encoding flag (EFS).  If this bit is set,
  //           the filename and comment fields for this file
  //           MUST be encoded using UTF-8. (see APPENDIX D)
  uint16_t made_by = version_made_by >> 8;
  BOOL made_on_dos = made_by == 0;
  BOOL languageEncoding = (flag & (1 << 11)) != 0;
  if (!languageEncoding && made_on_dos) {
    // APPNOTE.TXT D.1:
    //   D.2 If general purpose bit 11 is unset, the file name and comment should conform
    //   to the original ZIP character encoding.  If general purpose bit 11 is set, the
    //   filename and comment must support The Unicode Standard, Version 4.1.0 or
    //   greater using the character encoding form defined by the UTF-8 storage
    //   specification.  The Unicode Standard is published by the The Unicode
    //   Consortium (www.unicode.org).  UTF-8 encoded data stored within ZIP files
    //   is expected to not include a byte order mark (BOM).

    //  Code Page 437 corresponds to kCFStringEncodingDOSLatinUS
    NSStringEncoding encoding =
        CFStringConvertEncodingToNSStringEncoding(kCFStringEncodingDOSLatinUS);
    NSString* strPath = [NSString stringWithCString:filename encoding:encoding];
    if (strPath) {
      return strPath;
    }
  }

  // attempting unicode encoding
  NSString* strPath = @(filename);
  if (strPath) {
    return strPath;
  }

  // if filename is non-unicode, detect and transform Encoding
  NSData* data = [NSData dataWithBytes:(const void*)filename
                                length:sizeof(unsigned char) * size_filename];
// Testing availability of @available (https://stackoverflow.com/a/46927445/1033581)
#if __clang_major__ < 9
  // Xcode 8-
  if (floor(NSFoundationVersionNumber) > NSFoundationVersionNumber10_9_2) {
#else
  // Xcode 9+
  if (@available(macOS 10.10, iOS 8.0, watchOS 2.0, tvOS 9.0, *)) {
#endif
    // supported encodings are in [NSString availableStringEncodings]
    [NSString stringEncodingForData:data
                    encodingOptions:nil
                    convertedString:&strPath
                usedLossyConversion:nil];
  } else {
    // fallback to a simple manual detect for macOS 10.9 or older
    NSArray<NSNumber*>* encodings =
        @[ @(kCFStringEncodingGB_18030_2000), @(kCFStringEncodingShiftJIS) ];
    for (NSNumber* encoding in encodings) {
      strPath =
          [NSString stringWithCString:filename
                             encoding:(NSStringEncoding)CFStringConvertEncodingToNSStringEncoding(
                                          encoding.unsignedIntValue)];
      if (strPath) {
        break;
      }
    }
  }
  if (strPath) {
    return strPath;
  }

  // if filename encoding is non-detected, we default to something based on data
  // _hexString is more readable than _base64RFC4648 for debugging unknown encodings
  strPath = [data _hexString];
  return strPath;
}

+ (NSCalendar*)_gregorian {
  static NSCalendar* gregorian;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    gregorian = [[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
  });

  return gregorian;
}

// Format from
// http://newsgroups.derkeiler.com/Archive/Comp/comp.os.msdos.programmer/2009-04/msg00060.html Two
// consecutive words, or a longword, YYYYYYYMMMMDDDDD hhhhhmmmmmmsssss YYYYYYY is years from 1980 =
// 0 sssss is (seconds/2).
//
// 3658 = 0011 0110 0101 1000 = 0011011 0010 11000 = 27 2 24 = 2007-02-24
// 7423 = 0111 0100 0010 0011 - 01110 100001 00011 = 14 33 3 = 14:33:06
+ (NSDate*)_dateWithMSDOSFormat:(UInt32)msdosDateTime {
  // the whole `_dateWithMSDOSFormat:` method is equivalent but faster than this one line,
  // essentially because `mktime` is slow:
  // NSDate *date = [NSDate dateWithTimeIntervalSince1970:dosdate_to_time_t(msdosDateTime)];
  static const UInt32 kYearMask = 0xFE000000;
  static const UInt32 kMonthMask = 0x1E00000;
  static const UInt32 kDayMask = 0x1F0000;
  static const UInt32 kHourMask = 0xF800;
  static const UInt32 kMinuteMask = 0x7E0;
  static const UInt32 kSecondMask = 0x1F;

  NSAssert(
      0xFFFFFFFF == (kYearMask | kMonthMask | kDayMask | kHourMask | kMinuteMask | kSecondMask),
      @"[FlutterZipArchive] MSDOS date masks don't add up");

  NSDateComponents* components = [[NSDateComponents alloc] init];
  components.year = 1980 + ((msdosDateTime & kYearMask) >> 25);
  components.month = (msdosDateTime & kMonthMask) >> 21;
  components.day = (msdosDateTime & kDayMask) >> 16;
  components.hour = (msdosDateTime & kHourMask) >> 11;
  components.minute = (msdosDateTime & kMinuteMask) >> 5;
  components.second = (msdosDateTime & kSecondMask) * 2;

  NSDate* date = [self._gregorian dateFromComponents:components];
  [components release];
  return date;
}

@end

#pragma mark - Private tools for file info

BOOL _fileIsSymbolicLink(const unz_file_info* fileInfo) {
  //
  // Determine whether this is a symbolic link:
  // - File is stored with 'version made by' value of UNIX (3),
  //   as per https://www.pkware.com/documents/casestudies/APPNOTE.TXT
  //   in the upper byte of the version field.
  // - BSD4.4 st_mode constants are stored in the high 16 bits of the
  //   external file attributes (defacto standard, verified against libarchive)
  //
  // The original constants can be found here:
  //    https://minnie.tuhs.org/cgi-bin/utree.pl?file=4.4BSD/usr/include/sys/stat.h
  //
  const uLong ZipUNIXVersion = 3;
  const uLong BSD_SFMT = 0170000;
  const uLong BSD_IFLNK = 0120000;

  BOOL fileIsSymbolicLink = ((fileInfo->version >> 8) == ZipUNIXVersion) &&
                            BSD_IFLNK == (BSD_SFMT & (fileInfo->external_fa >> 16));
  return fileIsSymbolicLink;
}

#pragma mark - Private tools for unreadable encodings

@implementation NSData (FlutterZipArchive)

// `base64EncodedStringWithOptions` uses a base64 alphabet with '+' and '/'.
// we got those alternatives to make it compatible with filenames:
// https://en.wikipedia.org/wiki/Base64
// * modified Base64 encoding for IMAP mailbox names (RFC 3501): uses '+' and ','
// * modified Base64 for URL and filenames (RFC 4648): uses '-' and '_'
- (NSString*)_base64RFC4648 {
  NSString* strName = [self base64EncodedStringWithOptions:0];
  strName = [strName stringByReplacingOccurrencesOfString:@"+" withString:@"-"];
  strName = [strName stringByReplacingOccurrencesOfString:@"/" withString:@"_"];
  return strName;
}

// initWithBytesNoCopy from NSProgrammer, Jan 25 '12: https://stackoverflow.com/a/9009321/1033581
// hexChars from Peter, Aug 19 '14: https://stackoverflow.com/a/25378464/1033581
// not implemented as too lengthy: a potential mapping improvement from Moose, Nov 3 '15:
// https://stackoverflow.com/a/33501154/1033581
- (NSString*)_hexString {
  const char* hexChars = "0123456789ABCDEF";
  NSUInteger length = self.length;
  const unsigned char* bytes = self.bytes;
  char* chars = malloc(length * 2);
  if (chars == NULL) {
    // we directly raise an exception instead of using NSAssert to make sure assertion is not
    // disabled as this is irrecoverable
    [NSException raise:@"NSInternalInconsistencyException" format:@"failed malloc" arguments:nil];
    return nil;
  }
  char* s = chars;
  NSUInteger i = length;
  while (i--) {
    *s++ = hexChars[*bytes >> 4];
    *s++ = hexChars[*bytes & 0xF];
    bytes++;
  }
  NSString* str = [[[NSString alloc] initWithBytesNoCopy:chars
                                                  length:length * 2
                                                encoding:NSASCIIStringEncoding
                                            freeWhenDone:YES] autorelease];
  return str;
}

@end

#pragma mark Private tools for security

@implementation NSString (FlutterZipArchive)

// One implementation alternative would be to use the algorithm found at mz_path_resolve from
// https://github.com/nmoinvaz/minizip/blob/dev/mz_os.c, but making sure to work with unichar values
// and not ascii values to avoid breaking Unicode characters containing 2E ('.') or 2F ('/') in
// their decomposition
/// Sanitize path traversal characters to prevent directory backtracking. Ignoring these characters
/// mimicks the default behavior of the Unarchiving tool on macOS.
- (NSString*)_sanitizedPath {
  // Change Windows paths to Unix paths: https://en.wikipedia.org/wiki/Path_(computing)
  // Possible improvement: only do this if the archive was created on a non-Unix system
  NSString* strPath = [self stringByReplacingOccurrencesOfString:@"\\" withString:@"/"];

  // Percent-encode file path (where path is defined by https://tools.ietf.org/html/rfc8089)
  // The key part is to allow characters "." and "/" and disallow "%".
  // CharacterSet.urlPathAllowed seems to do the job
  // Testing availability of @available (https://stackoverflow.com/a/46927445/1033581)
#if __clang_major__ < 9
  // Xcode 8-
  if (floor(NSFoundationVersionNumber) > NSFoundationVersionNumber10_8_4) {
#else
  // Xcode 9+
  if (@available(macOS 10.9, iOS 7.0, watchOS 2.0, tvOS 9.0, *)) {
#endif
    strPath = [strPath
        stringByAddingPercentEncodingWithAllowedCharacters:NSCharacterSet
                                                               .URLPathAllowedCharacterSet];
  } else {
    strPath = [strPath stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
  }

  // `NSString.stringByAddingPercentEncodingWithAllowedCharacters:` may theorically fail:
  // https://stackoverflow.com/questions/33558933/ But because we auto-detect encoding using
  // `NSString.stringEncodingForData:encodingOptions:convertedString:usedLossyConversion:`, we
  // likely already prevent UTF-16, UTF-32 and invalid Unicode in the form of unpaired surrogate
  // chars: https://stackoverflow.com/questions/53043876/ To be on the safe side, we will still
  // perform a guard check.
  if (strPath == nil) {
    return nil;
  }

  // Add scheme "file:///" to support sanitation on names with a colon like
  // "file:a/../../../usr/bin"
  strPath = [@"file:///" stringByAppendingString:strPath];

  // Sanitize path traversal characters to prevent directory backtracking. Ignoring these characters
  // mimicks the default behavior of the Unarchiving tool on macOS.
  // "../../../../../../../../../../../tmp/test.txt" -> "tmp/test.txt"
  // "a/b/../c.txt" -> "a/c.txt"
  strPath = [NSURL URLWithString:strPath].standardizedURL.absoluteString;

  // Remove the "file:///" scheme
  strPath = [strPath substringFromIndex:8];

  // Remove the percent-encoding
  // Testing availability of @available (https://stackoverflow.com/a/46927445/1033581)
#if __clang_major__ < 9
  // Xcode 8-
  if (floor(NSFoundationVersionNumber) > NSFoundationVersionNumber10_8_4) {
#else
  // Xcode 9+
  if (@available(macOS 10.9, iOS 7.0, watchOS 2.0, tvOS 9.0, *)) {
#endif
    strPath = strPath.stringByRemovingPercentEncoding;
  } else {
    strPath = [strPath stringByReplacingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
  }

  return strPath;
}

@end
// END
