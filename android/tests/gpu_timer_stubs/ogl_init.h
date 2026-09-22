#ifndef TEST_GPU_TIMER_OGL_INIT_H
#define TEST_GPU_TIMER_OGL_INIT_H

typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef int GLint;
typedef int GLsizei;

#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_QUERY_RESULT 0x8866

void glGenQueries(GLsizei count, GLuint *queries);
void glGetIntegerv(GLenum name, GLint *value);
void glGetQueryObjectuiv(GLuint query, GLenum name, GLuint *value);
void glBeginQuery(GLenum target, GLuint query);
void glEndQuery(GLenum target);

#endif
