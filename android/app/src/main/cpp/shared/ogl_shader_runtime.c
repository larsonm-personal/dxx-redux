#include "ogl_shader_runtime.h"
#ifdef ANDROID
#include "android_log.h"
#endif

static void ogl_shader_log_info(const char *program_name, const char *stage,
	GLint ok, const char *message)
{
#ifdef ANDROID
	if (message[0])
		debug_log(DLOG_GRAPHICS, "shader %s %s %s: %s", program_name, stage,
			ok ? "info" : "failed", message);
#else
	(void)program_name;
	(void)stage;
	(void)ok;
	(void)message;
#endif
}

void ogl_shader_use_program(GLuint program)
{
#ifdef ANDROID
	gles3_shim_bind_program(program);
#else
	glUseProgram(program);
#endif
}

void ogl_shader_read_info_log(GLuint shader, const char *program_name,
	const char *stage, GLint ok, char *message, int message_size)
{
	GLint log_length = 0;

	message[0] = '\0';
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length <= 1)
		return;
	glGetShaderInfoLog(shader, (GLsizei)message_size, NULL, message);
	ogl_shader_log_info(program_name, stage, ok, message);
}

void ogl_program_read_info_log(GLuint program, const char *program_name,
	GLint ok, char *message, int message_size)
{
	GLint log_length = 0;

	message[0] = '\0';
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
	if (log_length <= 1)
		return;
	glGetProgramInfoLog(program, (GLsizei)message_size, NULL, message);
	ogl_shader_log_info(program_name, "link", ok, message);
}
