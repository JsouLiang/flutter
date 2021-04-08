//
//  NSData+FlutterGzip.h
//  sources
//
//  Created by 邱鑫玥 on 2019/9/18.
//

// BD ADD: START
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSData (FlutterGzip)

- (nullable NSData*)flutter_gunzippedData;


- (nullable NSString *)MD5;

@end

NS_ASSUME_NONNULL_END
// END
