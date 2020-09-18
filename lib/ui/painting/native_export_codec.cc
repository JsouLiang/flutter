
#include "flutter/fml/build_config.h"

#if OS_ANDROID
#include "flutter/fml/platform/android/jni_util.h"
#endif

#include "flutter/lib/ui/painting/native_export_codec.h"
#include "flutter/fml/logging.h"


namespace flutter {

NativeExportCodec::NativeExportCodec() = default;

NativeExportCodec::~NativeExportCodec() {
#if OS_ANDROID
  if (frameCount_ > 1) {
    JNIEnv *env = fml::jni::AttachCurrentThread();
    env->DeleteGlobalRef(codec);
  }
#endif
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
