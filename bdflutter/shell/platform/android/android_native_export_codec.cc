// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/bdflutter/shell/platform/android/android_native_export_codec.h"
#include "flutter/fml/logging.h"

// BD ADD: START
namespace flutter {

AndroidNativeExportCodec::AndroidNativeExportCodec(JNIEnv *env, std::string cKey, jobject jCodec) {
  jclass clazz = env->GetObjectClass(jCodec);
  jint width = env->GetIntField(jCodec, env->GetFieldID(clazz, "width", "I"));
  jint height = env->GetIntField(jCodec, env->GetFieldID(clazz, "height", "I"));
  jint frameCount = env->GetIntField(jCodec, env->GetFieldID(clazz, "frameCount", "I"));
  jint repeatCount = env->GetIntField(jCodec, env->GetFieldID(clazz, "repeatCount", "I"));
  jobject jObject = env->GetObjectField(jCodec, env->GetFieldID(clazz, "codec", "Ljava/lang/Object;"));
  int* frameDurations = nullptr;
  jobject frameDurationsObj = env->GetObjectField(jCodec, env->GetFieldID(clazz, "frameDurations", "[I"));
  if (frameDurationsObj) {
    jintArray * frameDurationsSrc = reinterpret_cast<jintArray*>(&frameDurationsObj);
    jsize frameDurationsSize = env->GetArrayLength(*frameDurationsSrc);
    if (frameDurationsSize > 0){
      frameDurations = static_cast<int*>(malloc(sizeof(int) * frameDurationsSize));
      env->GetIntArrayRegion(*frameDurationsSrc, 0, frameDurationsSize, frameDurations);
    }
  }
  this->frameDurations = frameDurations;
  this->repetitionCount_ = repeatCount;
  this->frameCount_ = frameCount;
  this->width = width;
  this->height = height;
  this->key = new std::string(std::move(cKey));
  this->codec = jObject;
}

AndroidNativeExportCodec::~AndroidNativeExportCodec() {
  if (frameCount_ > 1) {
    JNIEnv *env = fml::jni::AttachCurrentThread();
    env->DeleteGlobalRef(codec);
  }
  free(frameDurations);
  delete key;
}

}// namespace flutter
// END
