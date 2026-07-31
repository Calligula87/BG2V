/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2021      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

/**
 * @file  glutil.h
 * @brief OpenGL API initializer, related functions.
 */

#ifndef SOLOADER_GLUTIL_H
#define SOLOADER_GLUTIL_H

#include <vitaGL.h>

#ifdef __cplusplus
extern "C" {
#endif

void gl_init();

void gl_preload();

void gl_swap();

void gl_pointer_set(float x, float y, GLboolean visible);

EGLBoolean eglSwapBuffers_soloader(EGLDisplay display, EGLSurface surface);

EGLBoolean eglSwapInterval_soloader(EGLDisplay display, EGLint interval);

EGLBoolean eglWaitGL_soloader(void);

EGLBoolean eglWaitNative_soloader(EGLint engine);

void glCompileShader_soloader(GLuint shader);

void glShaderSource_soloader(GLuint shader, GLsizei count,
                             const GLchar **string, const GLint *_length);

void glCompressedTexImage2D_soloader(
    GLenum target, GLint level, GLenum internal_format,
    GLsizei width, GLsizei height, GLint border,
    GLsizei image_size, const GLvoid *data);

#ifdef __cplusplus
};
#endif

#endif // SOLOADER_GLUTIL_H
