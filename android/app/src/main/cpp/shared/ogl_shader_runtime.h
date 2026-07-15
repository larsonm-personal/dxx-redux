#ifndef OGL_SHADER_RUNTIME_H
#define OGL_SHADER_RUNTIME_H

#include "ogl_init.h"

void ogl_shader_use_program(GLuint program);
void ogl_shader_read_info_log(GLuint shader, const char *program_name,
                              const char *stage, GLint ok, char *message, int message_size);
void ogl_program_read_info_log(GLuint program, const char *program_name,
                               GLint ok, char *message, int message_size);

#endif
