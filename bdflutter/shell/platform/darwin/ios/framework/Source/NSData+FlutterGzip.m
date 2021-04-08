//
//  NSData+FlutterGzip.m
//  sources
//
//  Created by 邱鑫玥 on 2019/9/18.
//

// BD ADD: START
#import "NSData+FlutterGzip.h"
#include "third_party/zlib/zlib.h"
#import <CommonCrypto/CommonCrypto.h>

@implementation NSData (FlutterGzip)

- (NSData*)flutter_gunzippedData {
  if (self.length == 0 || ![self isGzippedData]) {
    return self;
  }

  z_stream stream;
  stream.zalloc = Z_NULL;
  stream.zfree = Z_NULL;
  stream.avail_in = (uint)self.length;
  stream.next_in = (Bytef*)self.bytes;
  stream.total_out = 0;
  stream.avail_out = 0;

  NSMutableData* output = nil;
  if (inflateInit2(&stream, 47) == Z_OK) {
    int status = Z_OK;
    output = [NSMutableData dataWithCapacity:self.length * 2];
    while (status == Z_OK) {
      if (stream.total_out >= output.length) {
        output.length += self.length / 2;
      }
      stream.next_out = (uint8_t*)output.mutableBytes + stream.total_out;
      stream.avail_out = (uInt)(output.length - stream.total_out);
      status = inflate(&stream, Z_SYNC_FLUSH);
    }
    if (inflateEnd(&stream) == Z_OK) {
      if (status == Z_STREAM_END) {
        output.length = stream.total_out;
      }
    }
  }

  return output;
}

- (BOOL)isGzippedData {
  const UInt8* bytes = (const UInt8*)self.bytes;
  return (self.length >= 2 && bytes[0] == 0x1f && bytes[1] == 0x8b);
}


- (NSString *)MD5 {
  CC_MD5_CTX md5;
  CC_MD5_Init(&md5);
  CC_MD5_Update(&md5, self.bytes, (CC_LONG)self.length);
  unsigned char result[CC_MD5_DIGEST_LENGTH];
  CC_MD5_Final(result, &md5);
  NSMutableString *resultValue = [NSMutableString string];
  for (int i = 0; i < CC_MD5_DIGEST_LENGTH; i++) {
    [resultValue appendFormat:@"%02x", result[i]];
  }
  return resultValue;
}


@end
// END
