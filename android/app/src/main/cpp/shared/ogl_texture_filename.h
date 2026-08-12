#ifndef DXX_ANDROID_OGL_TEXTURE_FILENAME_H
#define DXX_ANDROID_OGL_TEXTURE_FILENAME_H

#include <stddef.h>

int android_ogl_texture_filename(char *filename, size_t filename_capacity,
                                 const char *basename, const char *extension);

#endif
