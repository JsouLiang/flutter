// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BD_FLUTTER_SHELL_PLATFORM_ANDROID_EXTERNAL_TEXTURE_GL_H_
#define BD_FLUTTER_SHELL_PLATFORM_ANDROID_EXTERNAL_TEXTURE_GL_H_

#include <GLES/gl.h>
#include "flutter/flow/texture.h"
#include "flutter/fml/platform/android/jni_weak_ref.h"
#include "flutter/shell/platform/android/platform_view_android_jni_impl.h"

#include <android/surface_texture.h>
//#include <android/surface_texture_jni.h>

namespace flutter {

class BdAndroidExternalTextureGL : public flutter::AndroidExternalTextureGL {
 public:
  BdAndroidExternalTextureGL(
      int64_t id,
      const fml::jni::JavaObjectWeakGlobalRef& surface_texture,
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
      ASurfaceTexture* n_surface_texture);

  BdAndroidExternalTextureGL(
      int64_t id,
      const fml::jni::JavaObjectWeakGlobalRef& surface_texture,
      std::shared_ptr<PlatformViewAndroidJNI> jni_facade);

      ~BdAndroidExternalTextureGL() override;
protected:
  void Attach(jint textureName) override;

  void Update() override;

  void Detach() override;

  void UpdateTransform() override;

 private:
  ASurfaceTexture* mSurfaceTexture;

  FML_DISALLOW_COPY_AND_ASSIGN(BdAndroidExternalTextureGL);
};

}  // namespace flutter

#endif  // BD_FLUTTER_SHELL_PLATFORM_ANDROID_EXTERNAL_TEXTURE_GL_H_
