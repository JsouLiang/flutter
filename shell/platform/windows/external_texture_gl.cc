// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/windows/external_texture_gl.h"

namespace flutter {

struct ExternalTextureGLState {
  // BD MOD: START
  // GLuint gl_texture = 0;
  size_t gl_size;
  GLuint* gl_texture;
  uint32_t format;
  uint32_t image2D_format;
  // END
};

ExternalTextureGL::ExternalTextureGL(
    FlutterDesktopPixelBufferTextureCallback texture_callback,
    void* user_data,
    const GlProcs& gl_procs)
    : state_(std::make_unique<ExternalTextureGLState>()),
      texture_callback_(texture_callback),
      user_data_(user_data),
      gl_(gl_procs) {}

ExternalTextureGL::~ExternalTextureGL() {
  const auto& gl = GlProcs();
  // BD MOD: START
  //  if (state_->gl_texture != 0) {
  //    gl_.glDeleteTextures(1, &state_->gl_texture);
  //  }
  for (int i = 0; i < state_->gl_size; i++) {
    if (state_->gl_texture != 0) {
      gl.glDeleteTextures(1, &state_->gl_texture[i]);
    }
  }
  delete state_->gl_texture;
  // END
}

bool ExternalTextureGL::PopulateTexture(size_t width,
                                        size_t height,
                                        FlutterOpenGLTexture* opengl_texture) {
  if (!CopyPixelBuffer(width, height)) {
    return false;
  }

  // Populate the texture object used by the engine.
  opengl_texture->target = GL_TEXTURE_2D;
  // BD MOD: START
  // opengl_texture->name = state_->gl_texture;
  // opengl_texture->format = GL_RGBA8;
  opengl_texture->name = state_->gl_texture[0];
  opengl_texture->uv = state_->gl_texture + 1;
  opengl_texture->format = state_->format;
  opengl_texture->tex_size = state_->gl_size;
  // END
  opengl_texture->destruction_callback = nullptr;
  opengl_texture->user_data = nullptr;
  opengl_texture->width = width;
  opengl_texture->height = height;

  return true;
}

bool ExternalTextureGL::CopyPixelBuffer(size_t& width, size_t& height) {
  const FlutterDesktopPixelBuffer* pixel_buffer =
      texture_callback_(width, height, user_data_);
  if (!pixel_buffer || !pixel_buffer->buffer) {
    return false;
  }
  width = pixel_buffer->width;
  height = pixel_buffer->height;

  // BD MOD: START
  //  if (state_->gl_texture == 0) {
  //    gl_.glGenTextures(1, &state_->gl_texture);
  //
  //    gl_.glBindTexture(GL_TEXTURE_2D, state_->gl_texture);
  //
  //    gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
  //    GL_CLAMP_TO_BORDER); gl_.glTexParameteri(GL_TEXTURE_2D,
  //    GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  //
  //    gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  //    gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  //
  //  } else {
  //    gl_.glBindTexture(GL_TEXTURE_2D, state_->gl_texture);
  //  }
  //  gl_.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixel_buffer->width,
  //                  pixel_buffer->height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
  //                  pixel_buffer->buffer);
  //  if (pixel_buffer->release_callback) {
  //    pixel_buffer->release_callback(pixel_buffer->release_context);
  //  }
  if (!state_->gl_texture) {
    switch (pixel_buffer->format) {
      case FlutterDesktopTextureFormat::kBD_YUV420:
        state_->gl_size = 3;
        state_->gl_texture = new GLuint[3]();
        state_->format = GL_R8;
        state_->image2D_format = GL_LUMINANCE;
        break;
      case FlutterDesktopTextureFormat::kBD_BGRA:
        state_->gl_size = 1;
        state_->gl_texture = new GLuint[1]();
        state_->format = GL_BGRA8_EXT;
        state_->image2D_format = GL_BGRA8_EXT;
        break;
      default:
        state_->gl_size = 1;
        state_->gl_texture = new GLuint[1]();
        state_->format = GL_RGBA8;
        state_->image2D_format = GL_RGBA;
        break;
    }
  }

  for (int i = 0; i < state_->gl_size; i++) {
    if (state_->gl_texture[i] == 0) {
      gl_.glGenTextures(1, &state_->gl_texture[i]);
      gl_.glBindTexture(GL_TEXTURE_2D, state_->gl_texture[i]);

      gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
      gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

      gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      gl_.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    } else {
      gl_.glBindTexture(GL_TEXTURE_2D, state_->gl_texture[i]);
    }
    size_t div = i > 0 ? 1 : 0;
    if (i == 0) {  // y data of rgba data
      gl_.glPixelStorei(
          GL_UNPACK_ROW_LENGTH,
          std::max(pixel_buffer->yStride, pixel_buffer->width >> div));
      gl_.glTexImage2D(GL_TEXTURE_2D, 0, state_->image2D_format,
                       pixel_buffer->width >> div, pixel_buffer->height >> div,
                       0, state_->image2D_format, GL_UNSIGNED_BYTE,
                       pixel_buffer->buffer);
    } else if (i == 1) {  // u data
      gl_.glPixelStorei(
          GL_UNPACK_ROW_LENGTH,
          std::max(pixel_buffer->uStride, pixel_buffer->width >> div));
      gl_.glTexImage2D(GL_TEXTURE_2D, 0, state_->image2D_format,
                       pixel_buffer->width >> div, pixel_buffer->height >> div,
                       0, state_->image2D_format, GL_UNSIGNED_BYTE,
                       pixel_buffer->ubuffer);
    } else if (i == 2) {  // v data
      gl_.glPixelStorei(
          GL_UNPACK_ROW_LENGTH,
          std::max(pixel_buffer->vStride, pixel_buffer->width >> div));
      gl_.glTexImage2D(GL_TEXTURE_2D, 0, state_->image2D_format,
                       pixel_buffer->width >> div, pixel_buffer->height >> div,
                       0, state_->image2D_format, GL_UNSIGNED_BYTE,
                       pixel_buffer->vbuffer);
    }
    gl_.glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  }
  // END
  if (pixel_buffer->release_callback) {
    pixel_buffer->release_callback(pixel_buffer->release_context);
  }
  return true;
}

}  // namespace flutter
