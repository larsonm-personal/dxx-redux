#include "ogl_init.h"
#include "oglprog.h"
#include "dxxerror.h"
#include "android_log.h"

GLuint ogl_prog_tex2, ogl_prog_tex2m;
GLuint ogl_tex2_mat, ogl_tex2m_mat;
GLint ogl_tex2_alpha_cutoff = -1;
GLint ogl_tex2_debug_mode = -1;

#ifdef ANDROID
#define OGL_TEX2_FLOAT_PRECISION "precision highp float;"
#else
#define OGL_TEX2_FLOAT_PRECISION "precision mediump float;"
#endif

GLfloat ogl_mat_ortho[16] = {
	1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1 };

static void ogl_log_shader_info(const char *prog_name, const char *stage, GLint ok, const char *msg)
{
#ifdef ANDROID
	if (msg[0])
		debug_log(DLOG_GRAPHICS, "shader %s %s %s: %s",
			prog_name, stage, ok ? "info" : "failed", msg);
#else
	(void) prog_name;
	(void) stage;
	(void) ok;
	(void) msg;
#endif
}

static void ogl_read_shader_info_log(GLuint shader, const char *prog_name, const char *stage,
	GLint ok, char *msg, int msg_size)
{
	GLint log_len = 0;
	msg[0] = '\0';
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
	if (log_len <= 1)
		return;
	glGetShaderInfoLog(shader, (GLsizei) msg_size, NULL, msg);
	ogl_log_shader_info(prog_name, stage, ok, msg);
}

static void ogl_read_program_info_log(GLuint prog, const char *prog_name,
	GLint ok, char *msg, int msg_size)
{
	GLint log_len = 0;
	msg[0] = '\0';
	glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_len);
	if (log_len <= 1)
		return;
	glGetProgramInfoLog(prog, (GLsizei) msg_size, NULL, msg);
	ogl_log_shader_info(prog_name, "link", ok, msg);
}

static GLuint ogl_mk_prog(const char *prog_name, const char *vert_src, const char *frag_src) {
	char msg[2048];
	GLint val = 0;
	GLuint vert = glCreateShader(GL_VERTEX_SHADER);
	if (!vert) {
		Error("creating vert failed");
		return 0;
	}
	glShaderSource(vert, 1, &vert_src, NULL);
	glCompileShader(vert);
	glGetShaderiv(vert, GL_COMPILE_STATUS, &val);
	ogl_read_shader_info_log(vert, prog_name, "vertex", val, msg, (int) sizeof(msg));
	if (!val) {
		Error("compiling %s vert failed: %s", prog_name,
			msg[0] ? msg : "no shader info log");
		return 0;
	}
	GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
	if (!frag) {
		Error("creating frag failed");
		return 0;
	}
	glShaderSource(frag, 1, &frag_src, NULL);
	glCompileShader(frag);
	val = 0;
	glGetShaderiv(frag, GL_COMPILE_STATUS, &val);
	ogl_read_shader_info_log(frag, prog_name, "fragment", val, msg, (int) sizeof(msg));
	if (!val) {
		Error("compiling %s frag failed: %s", prog_name,
			msg[0] ? msg : "no shader info log");
		return 0;
	}
	GLuint prog = glCreateProgram();
	if (!prog) {
		Error("creating prog failed");
		return 0;
	}
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glBindAttribLocation(prog, OGL_APOS, "apos");
	glBindAttribLocation(prog, OGL_ACOLOR, "acolor");
	glBindAttribLocation(prog, OGL_ATEXCOORD, "atexcoord");
	glBindAttribLocation(prog, OGL_ATEXCOORD2, "atexcoord2");
	glLinkProgram(prog);
	glGetProgramiv(prog, GL_LINK_STATUS, &val);
	ogl_read_program_info_log(prog, prog_name, val, msg, (int) sizeof(msg));
	if (!val) {
		Error("linking %s prog failed: %s", prog_name,
			msg[0] ? msg : "no program info log");
	}
	glDeleteShader(frag);
	glDeleteShader(vert);
	return prog;
}

void ogl_init_prog() {
	ogl_prog_tex2 = ogl_mk_prog("tex2", "attribute vec3 apos;"
		"\n attribute vec4 acolor;"
		"\n attribute vec2 atexcoord;"
		"\n attribute vec2 atexcoord2;"
		"\n varying vec2 vtexcoord;"
		"\n varying vec2 vtexcoord2;"
		"\n varying vec4 vcolor;"
		"\n uniform mat4 umat;"
		"\n void main() {"
		"\n  gl_Position = umat * vec4(apos, 1.0);"
		"\n  vcolor = acolor; vtexcoord = atexcoord; vtexcoord2 = atexcoord2;"
		"\n }",
		OGL_TEX2_FLOAT_PRECISION
		"\n varying vec2 vtexcoord;"
		"\n varying vec2 vtexcoord2;"
		"\n varying vec4 vcolor;"
		"\n uniform sampler2D utex;"
		"\n uniform sampler2D utex2;"
		"\n uniform int utex2_debug;"
		"\n uniform float utex2alpha_cutoff;"
		"\n void main() {"
		"\n  vec4 bot = texture2D(utex, vtexcoord), ovl = texture2D(utex2, vtexcoord2);"
		"\n  if (utex2_debug == 1) {"
		"\n   gl_FragColor = vec4(ovl.a, 0.0, 0.0, 1.0);"
		"\n   return;"
		"\n  }"
		"\n  if (utex2_debug == 2) {"
		"\n   gl_FragColor = vec4(ovl.rgb, 1.0);"
		"\n   return;"
		"\n  }"
		"\n  float ovla = ovl.a;"
		"\n  if (utex2alpha_cutoff > 0.0)"
		"\n   ovla = ovl.a >= utex2alpha_cutoff ? 1.0 : 0.0;"
		"\n  if (utex2_debug == 3) {"
		"\n   vec4 f = vcolor * vec4(ovl.rgb, ovla);"
		"\n   if (f.a < 0.02) discard;"
		"\n   gl_FragColor = f;"
		"\n   return;"
		"\n  }"
		"\n  vec4 c = vec4(mix(bot.rgb, ovl.rgb, ovla), bot.a + ovla - bot.a * ovla);" // same as 1 - (1 - bot.a) * (1 - ovl.a)
		"\n  vec4 f = vcolor * c;"
		"\n  if (f.a < 0.02) discard;"
		"\n  gl_FragColor = f;"
		"\n }");

	ogl_prog_tex2m = ogl_mk_prog("tex2m", "attribute vec3 apos;"
		"\n attribute vec4 acolor;"
		"\n attribute vec2 atexcoord;"
		"\n attribute vec2 atexcoord2;"
		"\n varying vec2 vtexcoord;"
		"\n varying vec2 vtexcoord2;"
		"\n varying vec4 vcolor;"
		"\n uniform mat4 umat;"
		"\n void main() {"
		"\n  gl_Position = umat * vec4(apos, 1.0);"
		"\n  vcolor = acolor; vtexcoord = atexcoord; vtexcoord2 = atexcoord2;"
		"\n }",
		OGL_TEX2_FLOAT_PRECISION
		"\n varying vec2 vtexcoord;"
		"\n varying vec2 vtexcoord2;"
		"\n varying vec4 vcolor;"
		"\n uniform sampler2D utex;"
		"\n uniform sampler2D utex2;"
		"\n uniform sampler2D utex2m;"
		"\n void main() {"
		"\n  vec4 bot = texture2D(utex, vtexcoord), ovl = texture2D(utex2, vtexcoord2);"
		"\n  vec4 c = vec4(mix(bot.rgb, ovl.rgb, ovl.a), bot.a + ovl.a - bot.a * ovl.a);" // same as 1 - (1 - bot.a) * (1 - ovl.a)
		"\n  vec4 mask = texture2D(utex2m, vtexcoord2);"
		"\n  vec4 f = vcolor * vec4(c.rgb, c.a * mask.a);"
		"\n  if (f.a < 0.02) discard;"
		"\n  gl_FragColor = f;"
		"\n }");

	ogl_tex2_mat = glGetUniformLocation(ogl_prog_tex2, "umat");
	ogl_tex2m_mat = glGetUniformLocation(ogl_prog_tex2m, "umat");
	ogl_tex2_alpha_cutoff = glGetUniformLocation(ogl_prog_tex2, "utex2alpha_cutoff");
	ogl_tex2_debug_mode = glGetUniformLocation(ogl_prog_tex2, "utex2_debug");

	glUseProgram(ogl_prog_tex2);
	glUniform1i(glGetUniformLocation(ogl_prog_tex2, "utex"), 0);
	glUniform1i(glGetUniformLocation(ogl_prog_tex2, "utex2"), 1);
	glUniform1i(ogl_tex2_debug_mode, 0);
	glUniform1f(ogl_tex2_alpha_cutoff, 0.0f);

	glUseProgram(ogl_prog_tex2m);
	glUniform1i(glGetUniformLocation(ogl_prog_tex2m, "utex"), 0);
	glUniform1i(glGetUniformLocation(ogl_prog_tex2m, "utex2"), 1);
	glUniform1i(glGetUniformLocation(ogl_prog_tex2m, "utex2m"), 2);

	glUseProgram(0);
}

void ogl_prog_set_tex2_alpha_cutoff(GLfloat cutoff) {
	if (ogl_tex2_alpha_cutoff >= 0)
		glUniform1f(ogl_tex2_alpha_cutoff, cutoff);
}

void ogl_prog_set_tex2_debug_mode(GLint mode) {
	if (ogl_tex2_debug_mode >= 0)
		glUniform1i(ogl_tex2_debug_mode, mode);
}

void ogl_done_prog() {
	if (ogl_prog_tex2m) {
		glDeleteProgram(ogl_prog_tex2m);
		ogl_prog_tex2m = 0;
	}
	if (ogl_prog_tex2) {
		glDeleteProgram(ogl_prog_tex2);
		ogl_prog_tex2 = 0;
	}
}

void ogl_prog_set_matrix(GLfloat *mat) {
	if (ogl_prog_tex2) {
		glUseProgram(ogl_prog_tex2);
		glUniformMatrix4fv(ogl_tex2_mat, 1, GL_FALSE, mat);
	}

	if (ogl_prog_tex2m) {
		glUseProgram(ogl_prog_tex2m);
		glUniformMatrix4fv(ogl_tex2m_mat, 1, GL_FALSE, mat);
	}

	glUseProgram(0);
}

void ogl_prog_set_tex2_current_matrix(const GLfloat *mat, int super)
{
	if (!mat)
		return;
	if (super) {
		if (ogl_prog_tex2m)
			glUniformMatrix4fv(ogl_tex2m_mat, 1, GL_FALSE, mat);
	} else {
		if (ogl_prog_tex2)
			glUniformMatrix4fv(ogl_tex2_mat, 1, GL_FALSE, mat);
	}
}
