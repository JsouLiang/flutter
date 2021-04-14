// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/darwin/ios/ios_native_export_codec.h"
#include "flutter/fml/logging.h"

// BD ADD: START
namespace flutter {
    IOSNativeExportCodec::IOSNativeExportCodec(NSObject<IOSImageCodec>* imageCodec)
        : imageCodec(fml::scoped_nsobject<NSObject<IOSImageCodec>>([imageCodec retain])) {
      FML_DCHECK(imageCodec);
        this->frameCount_ = imageCodec.frameCount;
        this->repetitionCount_ = imageCodec.repetitionCount;
        if (imageCodec.frameCount > 1) {
          NSArray* durations = [imageCodec frameDurations];
          this->frameDurations = new int[durations.count];
          for (NSUInteger i = 0; i < durations.count; i ++) {
              int duration = [durations[i] doubleValue] * 1000;
              this->frameDurations[i] = duration;
          }
        }
    }

    IOSNativeExportCodec::~IOSNativeExportCodec() {
      if (this->frameDurations) {
        delete [] this->frameDurations;
      }
    }
}  // namespace flutter
// END
