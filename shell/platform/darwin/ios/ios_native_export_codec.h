// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_SHELL_PLATFORM_IOS_NATIVE_EXPORT_CODEC_H_
#define FLUTTER_SHELL_PLATFORM_IOS_NATIVE_EXPORT_CODEC_H_

#include "flutter/lib/ui/painting/native_export_codec.h"
#include "flutter/fml/platform/darwin/cf_utils.h"
#include "flutter/shell/platform/darwin/ios/framework/Headers/FlutterImageLoader.h"
#include "flutter/fml/platform/darwin/scoped_nsobject.h"

// BD ADD: START
namespace flutter {

    class IOSNativeExportCodec : public flutter::NativeExportCodec {
    public:
        IOSNativeExportCodec(NSObject<IOSImageCodec>* imageCodec);

        ~IOSNativeExportCodec() override;

    public:
        fml::scoped_nsobject<NSObject<IOSImageCodec>> imageCodec;
    };

}  // namespace flutter
// END

#endif  // FLUTTER_SHELL_PLATFORM_IOS_NATIVE_EXPORT_CODEC_H_
