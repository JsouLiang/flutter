// Author:zhoutongwei@bytedance.com.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/android/android_external_texture_gl.h"

#include <GLES/glext.h>

#include <android/surface_texture.h>
#include <android/surface_texture_jni.h>
#include "bd_android_external_texture_gl.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "flutter/fml/trace_event.h"

namespace flutter {

BdAndroidExternalTextureGL::BdAndroidExternalTextureGL(
    int64_t id,
    const fml::jni::JavaObjectWeakGlobalRef& surface_texture,
    std::shared_ptr<PlatformViewAndroidJNI> jni_facade,
    ASurfaceTexture* n_surface_texture)
    : AndroidExternalTextureGL(id, surface_texture, jni_facade) {
  mSurfaceTexture = n_surface_texture;
}

extern "C" __attribute((weak)) void ASurfaceTexture_getTransformMatrix(
    ASurfaceTexture* st,
    float mtx[16]);
extern "C" __attribute((weak)) int ASurfaceTexture_attachToGLContext(
    ASurfaceTexture* st,
    uint32_t texName);
extern "C" __attribute((weak)) int ASurfaceTexture_detachFromGLContext(
    ASurfaceTexture* st);
extern "C" __attribute((weak)) int ASurfaceTexture_updateTexImage(
    ASurfaceTexture* st);

BdAndroidExternalTextureGL::~BdAndroidExternalTextureGL() {
  mSurfaceTexture = nullptr;
}

/*define in platform_view_android_jni.h*/
extern SkSize ScaleToFill(float scaleX, float scaleY);

// TODO 是否有必要再判断sdk version
// TODO matrix transform存在数据转换待处理
void BdAndroidExternalTextureGL::UpdateTransform() {
  void* iterate = (void*)&ASurfaceTexture_getTransformMatrix;
  if (iterate != NULL) {
    float matrix[16];
    ASurfaceTexture_getTransformMatrix(mSurfaceTexture, matrix);
    float scaleX = matrix[0], scaleY = matrix[5];
    const SkSize scaled = ScaleToFill(scaleX, scaleY);
    SkScalar matrix3[] = {
        scaled.fWidth, matrix[1],      matrix[2],   //
        matrix[4],     scaled.fHeight, matrix[6],   //
        matrix[8],     matrix[9],      matrix[10],  //
    };
    transform.set9(matrix3);
  } else {
    AndroidExternalTextureGL::UpdateTransform();
  }
}

void BdAndroidExternalTextureGL::Attach(jint textureName) {
  void* iterate = (void*)&ASurfaceTexture_getTransformMatrix;
  if (iterate != NULL) {
    ASurfaceTexture_attachToGLContext(mSurfaceTexture, textureName);
  } else {
    AndroidExternalTextureGL::Attach(textureName);
  }
}

void BdAndroidExternalTextureGL::Update() {
  void* iterate = (void*)&ASurfaceTexture_updateTexImage;
  if (iterate != NULL) {
    ASurfaceTexture_updateTexImage(mSurfaceTexture);
    UpdateTransform();
  } else {
    AndroidExternalTextureGL::Update();
  }
}

void BdAndroidExternalTextureGL::Detach() {
  void* iterate = (void*)&ASurfaceTexture_detachFromGLContext;
  if (iterate != NULL) {
    ASurfaceTexture_detachFromGLContext(mSurfaceTexture);
  } else {
    AndroidExternalTextureGL::Detach();
  }
}
}  // namespace flutter
