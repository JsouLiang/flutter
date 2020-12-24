
#include "flutter/lib/ui/painting/native_export_codec.h"


namespace flutter {

NativeExportCodec::NativeExportCodec() = default;

NativeExportCodec::~NativeExportCodec() {

}

int NativeExportCodec::frameCount() const {
  return frameCount_;
}

int NativeExportCodec::repetitionCount() const {
  return repetitionCount_;
}

int NativeExportCodec::duration(int currentFrame) const {
  if (!frameDurations) {
    return 0;
  }
  return frameDurations[currentFrame % frameCount_];
}

}  // namespace flutter
