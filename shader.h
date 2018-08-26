// Copyright (c) 2018 BaiQiang. All rights reserved.

#ifndef __SHADER_H__
#define __SHADER_H__

#include "glad/glad.h"

#ifdef __cplusplus
extern "C" {
#endif

char *
shader_read(const char *filename);

GLuint shader_compile(const char *source, const GLenum type);

GLuint shader_load(const char *vert_filename, const char *frag_filename);

#ifdef __cplusplus
}
#endif

#endif /* __SHADER_H__ */
