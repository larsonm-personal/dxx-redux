/* gles3_shim.c -- GLES 1.x fixed-function compatibility shim for GLES 3.0
 *
 * Provides a CPU-side matrix stack, shader-based vertex pipeline, and
 * fixed-function state emulation for the ~40 unique GLES 1.x calls used
 * by the DXX-Redux rendering code.
 *
 * Android port -- not compiled on desktop builds.
 */

#include "gles3_shim.h"
#include "gles3_shim_array_sources.h"

/* We need the real glDrawArrays/glEnable/glDisable, not our macro redirects */
#undef glDrawArrays
#undef glEnable
#undef glDisable

#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <android/log.h>
#include "android_crash_handler.h"
#include "android_log.h"

#define TAG       "gles3_shim"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

/* ------------------------------------------------------------------ */
/* Matrix helpers                                                      */
/* ------------------------------------------------------------------ */

#define STACK_DEPTH 16

typedef struct {
	float m[STACK_DEPTH][16];
	int top;
} mat_stack;

static mat_stack mv_stack;   /* GL_MODELVIEW   */
static mat_stack proj_stack; /* GL_PROJECTION  */
static mat_stack *cur_stack = &mv_stack;

static float mvp[16];
static int mvp_dirty = 1;

static void mat4_identity(float *dst)
{
	memset(dst, 0, 16 * sizeof(float));
	dst[0] = dst[5] = dst[10] = dst[15] = 1.0f;
}

static void mat4_copy(float *dst, const float *src)
{
	memcpy(dst, src, 16 * sizeof(float));
}

static void mat4_mul(float *dst, const float *a, const float *b)
{
	float tmp[16];
	for (int i = 0; i < 4; i++)
		for (int j = 0; j < 4; j++) {
			float s = 0;
			for (int k = 0; k < 4; k++)
				s += a[i + k * 4] * b[k + j * 4];
			tmp[i + j * 4] = s;
		}
	memcpy(dst, tmp, 16 * sizeof(float));
}

static float *cur_mat(void)
{
	return cur_stack->m[cur_stack->top];
}

/* ------------------------------------------------------------------ */
/* Shader program                                                      */
/* ------------------------------------------------------------------ */

static GLuint shim_prog;
static GLint u_mvp;
static GLint u_tex;
static GLint u_tex_enabled;
static GLint u_alpha_test;
static GLint u_alpha_ref;
static GLint u_use_color_attr;
static GLint u_flat_color;
static GLint u_debug_mode;

/* Debug visualization mode, settable from JNI.
 * 0 = normal, 1 = show alpha, 2 = no texture (vertex color only),
 * 3 = texcoord visualization */
volatile int gles3_shim_debug_mode = 0;

#define ATTR_POS      0 /* matches OGL_APOS */
#define ATTR_COLOR    1 /* matches OGL_ACOLOR */
#define ATTR_TEXCOORD 2 /* matches OGL_ATEXCOORD */

/* Vertex shader -- GLES 3.0 (ESSL 300) */
static const char *vs_src =
    "#version 300 es\n"
    "layout(location=0) in vec3 aPos;\n"
    "layout(location=1) in vec4 aColor;\n"
    "layout(location=2) in vec2 aTexCoord;\n"
    "uniform mat4 uMVP;\n"
    "uniform int uUseColorAttr;\n"
    "uniform vec4 uFlatColor;\n"
    "out vec4 vColor;\n"
    "out vec2 vTexCoord;\n"
    "void main() {\n"
    "  gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "  gl_PointSize = 1.0;\n"
    "  vColor = (uUseColorAttr != 0) ? aColor : uFlatColor;\n"
    "  vTexCoord = aTexCoord;\n"
    "}\n";

/* Fragment shader */
static const char *fs_src =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec4 vColor;\n"
    "in vec2 vTexCoord;\n"
    "uniform sampler2D uTex;\n"
    "uniform int uTexEnabled;\n"
    "uniform int uAlphaTest;\n"
    "uniform float uAlphaRef;\n"
    "uniform int uDebugMode;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "  vec4 c = vColor;\n"
    "  vec4 tex = vec4(1.0);\n"
    "  if (uTexEnabled != 0)\n"
    "    tex = texture(uTex, vTexCoord);\n"
    "  if (uDebugMode == 1) {\n"
    "    fragColor = vec4(tex.aaa, 1.0);\n"
    "    return;\n"
    "  }\n"
    "  if (uDebugMode == 2) {\n"
    "    fragColor = vColor;\n"
    "    return;\n"
    "  }\n"
    "  if (uDebugMode == 3) {\n"
    "    fragColor = vec4(vTexCoord, 0.0, 1.0);\n"
    "    return;\n"
    "  }\n"
    "  c *= tex;\n"
    "  if (uAlphaTest != 0 && c.a < uAlphaRef)\n"
    "    discard;\n"
    "  fragColor = c;\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *src)
{
	char buf[512];
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	buf[0] = '\0';
	{
		GLint log_len = 0;
		glGetShaderiv(s, GL_INFO_LOG_LENGTH, &log_len);
		if (log_len > 1) {
			const char *label = type == GL_VERTEX_SHADER ? "gles3-shim vertex" : "gles3-shim fragment";
			glGetShaderInfoLog(s, sizeof(buf), NULL, buf);
			if (ok)
				LOGI("%s: %s", label, buf);
			else
				LOGE("%s: %s", label, buf);
			debug_log(DLOG_GRAPHICS, "%s: %s", label, buf);
		}
	}
	if (!ok && !buf[0]) {
		const char *label = type == GL_VERTEX_SHADER ? "gles3-shim vertex" : "gles3-shim fragment";
		LOGE("%s: compile failed with no info log", label);
		debug_log(DLOG_GRAPHICS, "%s: compile failed with no info log", label);
	}
	return s;
}

/* ------------------------------------------------------------------ */
/* State tracking                                                      */
/* ------------------------------------------------------------------ */

static int tex2d_enabled;
static int alpha_test_enabled;
static float alpha_ref = 0.0f;
static GLenum alpha_func_val = GL_ALWAYS;

static float flat_color[4] = { 1, 1, 1, 1 };

/* Client state for attribute arrays */
static int vertex_array_enabled;
static int color_array_enabled;
static int texcoord_array_enabled;

/* Deferred vertex array pointers */
static GLint va_size;
static GLenum va_type;
static GLsizei va_stride;
static const void *va_ptr;
static GLuint va_buffer;

static GLint ca_size;
static GLenum ca_type;
static GLsizei ca_stride;
static const void *ca_ptr;
static GLuint ca_buffer;

static GLint ta_size;
static GLenum ta_type;
static GLsizei ta_stride;
static const void *ta_ptr;
static GLuint ta_buffer;

static GLint ta2_size;
static GLenum ta2_type;
static GLsizei ta2_stride;
static const void *ta2_ptr;
static GLuint ta2_buffer;

static int state_dirty = 1;
static GLuint external_prog;
static GLuint current_prog;
static GLuint shim_vbo;
static unsigned char *shim_stream_data;
static int shim_stream_capacity;

static int gl_type_size(GLenum type)
{
	switch (type) {
		case GL_BYTE:
		case GL_UNSIGNED_BYTE: return 1;
		case GL_SHORT:
		case GL_UNSIGNED_SHORT: return 2;
		default: return 4; /* GL_FLOAT, GL_INT, GL_UNSIGNED_INT */
	}
}

static void gles3_shim_reserve_stream_data(int bytes)
{
	if (bytes > shim_stream_capacity) {
		unsigned char *new_data = realloc(shim_stream_data, bytes);
		if (!new_data)
			abort();
		shim_stream_data = new_data;
		shim_stream_capacity = bytes;
	}
}

void gles3_shim_bind_program(GLuint prog)
{
	if (current_prog == prog)
		return;
	glUseProgram(prog);
	current_prog = prog;
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

void gles3_shim_init(void)
{
	mv_stack.top = 0;
	proj_stack.top = 0;
	mat4_identity(mv_stack.m[0]);
	mat4_identity(proj_stack.m[0]);
	mvp_dirty = 1;
	state_dirty = 1;
	external_prog = 0;
	current_prog = 0;

	tex2d_enabled = 0;
	alpha_test_enabled = 0;
	alpha_ref = 0.0f;
	flat_color[0] = flat_color[1] = flat_color[2] = flat_color[3] = 1.0f;

	vertex_array_enabled = 0;
	color_array_enabled = 0;
	texcoord_array_enabled = 0;
	va_buffer = 0;
	ca_buffer = 0;
	ta_buffer = 0;
	ta2_size = 0;
	ta2_type = GL_FLOAT;
	ta2_stride = 0;
	ta2_ptr = NULL;
	ta2_buffer = 0;

	GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
	GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
	shim_prog = glCreateProgram();
	glAttachShader(shim_prog, vs);
	glAttachShader(shim_prog, fs);
	glLinkProgram(shim_prog);
	GLint ok = 0;
	glGetProgramiv(shim_prog, GL_LINK_STATUS, &ok);
	{
		char buf[512];
		GLint log_len = 0;
		buf[0] = '\0';
		glGetProgramiv(shim_prog, GL_INFO_LOG_LENGTH, &log_len);
		if (log_len > 1) {
			glGetProgramInfoLog(shim_prog, sizeof(buf), NULL, buf);
			if (ok)
				LOGI("gles3-shim link: %s", buf);
			else
				LOGE("gles3-shim link: %s", buf);
			debug_log(DLOG_GRAPHICS, "gles3-shim link: %s", buf);
		}
		if (!ok && !buf[0]) {
			LOGE("gles3-shim link: failed with no info log");
			debug_log(DLOG_GRAPHICS, "gles3-shim link: failed with no info log");
		}
	}
	glDeleteShader(vs);
	glDeleteShader(fs);

	u_mvp = glGetUniformLocation(shim_prog, "uMVP");
	u_tex = glGetUniformLocation(shim_prog, "uTex");
	u_tex_enabled = glGetUniformLocation(shim_prog, "uTexEnabled");
	u_alpha_test = glGetUniformLocation(shim_prog, "uAlphaTest");
	u_alpha_ref = glGetUniformLocation(shim_prog, "uAlphaRef");
	u_use_color_attr = glGetUniformLocation(shim_prog, "uUseColorAttr");
	u_flat_color = glGetUniformLocation(shim_prog, "uFlatColor");
	u_debug_mode = glGetUniformLocation(shim_prog, "uDebugMode");

	gles3_shim_bind_program(shim_prog);
	glUniform1i(u_tex, 0);
	glUniform1i(u_tex_enabled, 0);
	glUniform1i(u_alpha_test, 0);
	glUniform1f(u_alpha_ref, 0.0f);
	glUniform1i(u_use_color_attr, 0);
	glUniform4f(u_flat_color, 1, 1, 1, 1);

	/* GLES 3.0 requires a VAO to be bound for vertex attrib calls */
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	/* VBO for streaming client-side vertex data (GLES 3.0 has no client arrays) */
	glGenBuffers(1, &shim_vbo);

	LOGI("GLES3 shim initialized (prog=%u, vao=%u, vbo=%u)", shim_prog, vao, shim_vbo);
}

void gles3_shim_shutdown(void)
{
	free(shim_stream_data);
	shim_stream_data = NULL;
	shim_stream_capacity = 0;
	if (shim_vbo) {
		glDeleteBuffers(1, &shim_vbo);
		shim_vbo = 0;
	}
	if (shim_prog) {
		glDeleteProgram(shim_prog);
		shim_prog = 0;
	}
	current_prog = 0;
	external_prog = 0;
	state_dirty = 1;
}

/* ------------------------------------------------------------------ */
/* Matrix stack                                                        */
/* ------------------------------------------------------------------ */

void gles3_shim_matrix_mode(GLenum mode)
{
	cur_stack = (mode == GL_PROJECTION) ? &proj_stack : &mv_stack;
}

void gles3_shim_load_identity(void)
{
	mat4_identity(cur_mat());
	mvp_dirty = 1;
}

void gles3_shim_push_matrix(void)
{
	if (cur_stack->top < STACK_DEPTH - 1) {
		mat4_copy(cur_stack->m[cur_stack->top + 1], cur_mat());
		cur_stack->top++;
	}
}

void gles3_shim_pop_matrix(void)
{
	if (cur_stack->top > 0)
		cur_stack->top--;
	mvp_dirty = 1;
}

void gles3_shim_translatef(GLfloat x, GLfloat y, GLfloat z)
{
	float t[16];
	mat4_identity(t);
	t[12] = x;
	t[13] = y;
	t[14] = z;
	mat4_mul(cur_mat(), cur_mat(), t);
	mvp_dirty = 1;
}

void gles3_shim_scalef(GLfloat x, GLfloat y, GLfloat z)
{
	float *m = cur_mat();
	m[0] *= x;
	m[1] *= x;
	m[2] *= x;
	m[3] *= x;
	m[4] *= y;
	m[5] *= y;
	m[6] *= y;
	m[7] *= y;
	m[8] *= z;
	m[9] *= z;
	m[10] *= z;
	m[11] *= z;
	mvp_dirty = 1;
}

void gles3_shim_rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	float rad = angle * 3.14159265358979323846f / 180.0f;
	float c = cosf(rad), s = sinf(rad);
	float len = sqrtf(x * x + y * y + z * z);
	if (len < 1e-6f) return;
	x /= len;
	y /= len;
	z /= len;
	float nc = 1.0f - c;
	float r[16];
	r[0] = x * x * nc + c;
	r[4] = x * y * nc - z * s;
	r[8] = x * z * nc + y * s;
	r[12] = 0;
	r[1] = y * x * nc + z * s;
	r[5] = y * y * nc + c;
	r[9] = y * z * nc - x * s;
	r[13] = 0;
	r[2] = z * x * nc - y * s;
	r[6] = z * y * nc + x * s;
	r[10] = z * z * nc + c;
	r[14] = 0;
	r[3] = 0;
	r[7] = 0;
	r[11] = 0;
	r[15] = 1;
	mat4_mul(cur_mat(), cur_mat(), r);
	mvp_dirty = 1;
}

void gles3_shim_orthof(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
	float o[16];
	memset(o, 0, sizeof(o));
	o[0] = 2.0f / (r - l);
	o[5] = 2.0f / (t - b);
	o[10] = -2.0f / (f - n);
	o[12] = -(r + l) / (r - l);
	o[13] = -(t + b) / (t - b);
	o[14] = -(f + n) / (f - n);
	o[15] = 1.0f;
	mat4_mul(cur_mat(), cur_mat(), o);
	mvp_dirty = 1;
}

void gles3_shim_frustumf(GLfloat l, GLfloat r, GLfloat b, GLfloat t, GLfloat n, GLfloat f)
{
	float fr[16];
	memset(fr, 0, sizeof(fr));
	fr[0] = 2.0f * n / (r - l);
	fr[5] = 2.0f * n / (t - b);
	fr[8] = (r + l) / (r - l);
	fr[9] = (t + b) / (t - b);
	fr[10] = -(f + n) / (f - n);
	fr[11] = -1.0f;
	fr[14] = -2.0f * f * n / (f - n);
	mat4_mul(cur_mat(), cur_mat(), fr);
	mvp_dirty = 1;
}

void gles3_shim_load_matrixf(const GLfloat *m)
{
	mat4_copy(cur_mat(), m);
	mvp_dirty = 1;
}

void gles3_shim_mult_matrixf(const GLfloat *m)
{
	mat4_mul(cur_mat(), cur_mat(), m);
	mvp_dirty = 1;
}

/* Get the current MVP for external shader (e.g. OGL_MERGE programs) */
static void compute_mvp(void)
{
	mat4_mul(mvp, proj_stack.m[proj_stack.top], mv_stack.m[mv_stack.top]);
	mvp_dirty = 0;
}

/* ------------------------------------------------------------------ */
/* Client-state vertex arrays                                          */
/* ------------------------------------------------------------------ */

void gles3_shim_enable_client_state(GLenum cap)
{
	switch (cap) {
		case GL_VERTEX_ARRAY: vertex_array_enabled = 1; break;
		case GL_COLOR_ARRAY: color_array_enabled = 1; break;
		case GL_TEXTURE_COORD_ARRAY: texcoord_array_enabled = 1; break;
	}
	state_dirty = 1;
}

void gles3_shim_disable_client_state(GLenum cap)
{
	switch (cap) {
		case GL_VERTEX_ARRAY: vertex_array_enabled = 0; break;
		case GL_COLOR_ARRAY: color_array_enabled = 0; break;
		case GL_TEXTURE_COORD_ARRAY: texcoord_array_enabled = 0; break;
	}
	state_dirty = 1;
}

void gles3_shim_vertex_pointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
	GLint binding;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &binding);
	va_size = size;
	va_type = type;
	va_stride = stride;
	va_ptr = ptr;
	va_buffer = (GLuint) binding;
}

void gles3_shim_color_pointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
	GLint binding;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &binding);
	ca_size = size;
	ca_type = type;
	ca_stride = stride;
	ca_ptr = ptr;
	ca_buffer = (GLuint) binding;
}

void gles3_shim_texcoord_pointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
	GLint binding;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &binding);
	ta_size = size;
	ta_type = type;
	ta_stride = stride;
	ta_ptr = ptr;
	ta_buffer = (GLuint) binding;
}

void gles3_shim_external_texcoord2_pointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
	GLint binding;
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &binding);
	ta2_size = size;
	ta2_type = type;
	ta2_stride = stride;
	ta2_ptr = ptr;
	ta2_buffer = (GLuint) binding;
}

/* ------------------------------------------------------------------ */
/* Fixed-function state                                                */
/* ------------------------------------------------------------------ */

void gles3_shim_color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	flat_color[0] = r;
	flat_color[1] = g;
	flat_color[2] = b;
	flat_color[3] = a;
	state_dirty = 1;
}

void gles3_shim_alpha_func(GLenum func, GLfloat ref)
{
	alpha_func_val = func;
	alpha_ref = ref;
	state_dirty = 1;
}

void gles3_shim_shade_model(GLenum mode)
{
	(void) mode; /* always smooth in our shader */
}

void gles3_shim_tex_envi(GLenum target, GLenum pname, GLint param)
{
	(void) target;
	(void) pname;
	(void) param;
	/* always modulate in our shader */
}

void gles3_shim_glEnable(GLenum cap)
{
	if (cap == GL_TEXTURE_2D) {
		tex2d_enabled = 1;
		state_dirty = 1;
	} else if (cap == GL_ALPHA_TEST) {
		alpha_test_enabled = 1;
		state_dirty = 1;
	} else {
		glEnable(cap);
	}
}

void gles3_shim_glDisable(GLenum cap)
{
	if (cap == GL_TEXTURE_2D) {
		tex2d_enabled = 0;
		state_dirty = 1;
	} else if (cap == GL_ALPHA_TEST) {
		alpha_test_enabled = 0;
		state_dirty = 1;
	} else {
		glDisable(cap);
	}
}

/* ------------------------------------------------------------------ */
/* Flush state before draw                                             */
/* ------------------------------------------------------------------ */

void gles3_shim_flush_state(void)
{
	if (external_prog) return; /* external shader is active */

	gles3_shim_bind_program(shim_prog);

	if (mvp_dirty) compute_mvp();
	glUniformMatrix4fv(u_mvp, 1, GL_FALSE, mvp);

	glUniform1i(u_tex_enabled, tex2d_enabled);
	glUniform1i(u_alpha_test, alpha_test_enabled);
	glUniform1f(u_alpha_ref, alpha_ref);

	glUniform1i(u_use_color_attr, color_array_enabled);
	glUniform4fv(u_flat_color, 1, flat_color);
	glUniform1i(u_debug_mode, gles3_shim_debug_mode);

	state_dirty = 0;
}

/* ------------------------------------------------------------------ */
/* External shader override (for OGL_MERGE multi-texture programs)     */
/* ------------------------------------------------------------------ */

void gles3_shim_use_external(GLuint prog)
{
	if (prog) {
		external_prog = prog;
		gles3_shim_bind_program(prog);
		/* Update MVP for the external program */
		if (mvp_dirty) compute_mvp();
	} else {
		external_prog = 0;
		ta2_ptr = NULL;
		ta2_size = 0;
		ta2_stride = 0;
		ta2_type = GL_FLOAT;
		ta2_buffer = 0;
		state_dirty = 1;
	}
}

/* Expose the computed MVP matrix for external programs that need it */
const float *gles3_shim_get_mvp(void)
{
	if (mvp_dirty) compute_mvp();
	return mvp;
}

GLuint gles3_shim_get_stream_vbo(void)
{
	return shim_vbo;
}

int gles3_shim_probe_vbo_arrays(void)
{
	static const float vertices[][5] = {
		{ -0.5f, -0.5f, 0.0f, 0.0f, 0.0f },
		{ 0.5f, -0.5f, 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.5f, 0.0f, 0.5f, 1.0f },
		{ -0.4f, -0.4f, 0.0f, 0.0f, 0.0f },
		{ 0.4f, -0.4f, 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.4f, 0.0f, 0.5f, 1.0f }
	};
	struct {
		GLint size;
		GLenum type;
		GLsizei stride;
		const void *ptr;
		GLuint buffer;
	} saved_va = { va_size, va_type, va_stride, va_ptr, va_buffer },
	  saved_ca = { ca_size, ca_type, ca_stride, ca_ptr, ca_buffer },
	  saved_ta = { ta_size, ta_type, ta_stride, ta_ptr, ta_buffer };
	GLint previous_buffer;
	GLint previous_framebuffer;
	GLint previous_texture;
	GLint previous_viewport[4];
	GLuint previous_program = current_prog;
	GLuint probe_vbo = 0;
	GLuint probe_texture = 0;
	GLuint probe_framebuffer = 0;
	GLenum error;
	GLfloat previous_clear_color[4];
	GLboolean previous_blend;
	GLboolean previous_cull;
	GLboolean previous_depth;
	GLboolean previous_scissor;
	unsigned char center_pixel[4] = { 0, 0, 0, 0 };
	mat_stack saved_mv_stack = mv_stack;
	mat_stack saved_proj_stack = proj_stack;
	mat_stack *saved_cur_stack = cur_stack;
	float saved_mvp[16];
	float saved_flat_color[4];
	int saved_mvp_dirty = mvp_dirty;
	int saved_tex2d_enabled = tex2d_enabled;
	int saved_alpha_test_enabled = alpha_test_enabled;
	int saved_vertex_enabled = vertex_array_enabled;
	int saved_color_enabled = color_array_enabled;
	int saved_texcoord_enabled = texcoord_array_enabled;
	int saved_state_dirty = state_dirty;
	int passed;

	if (external_prog)
		return 0;
	memcpy(saved_mvp, mvp, sizeof(saved_mvp));
	memcpy(saved_flat_color, flat_color, sizeof(saved_flat_color));
	while (glGetError() != GL_NO_ERROR);
	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_buffer);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
	glGetIntegerv(GL_VIEWPORT, previous_viewport);
	glGetFloatv(GL_COLOR_CLEAR_VALUE, previous_clear_color);
	previous_blend = glIsEnabled(GL_BLEND);
	previous_cull = glIsEnabled(GL_CULL_FACE);
	previous_depth = glIsEnabled(GL_DEPTH_TEST);
	previous_scissor = glIsEnabled(GL_SCISSOR_TEST);

	glGenTextures(1, &probe_texture);
	glBindTexture(GL_TEXTURE_2D, probe_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA,
	             GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenFramebuffers(1, &probe_framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, probe_framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
	                       probe_texture, 0);
	error = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE
	            ? GL_NO_ERROR
	            : GL_INVALID_FRAMEBUFFER_OPERATION;
	glViewport(0, 0, 8, 8);
	glDisable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	mv_stack.top = 0;
	proj_stack.top = 0;
	mat4_identity(mv_stack.m[0]);
	mat4_identity(proj_stack.m[0]);
	cur_stack = &mv_stack;
	mvp_dirty = 1;
	tex2d_enabled = 0;
	alpha_test_enabled = 0;
	flat_color[0] = flat_color[1] = flat_color[2] = flat_color[3] = 1.0f;
	state_dirty = 1;
	glGenBuffers(1, &probe_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, probe_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	gles3_shim_vertex_pointer(3, GL_FLOAT, sizeof(vertices[0]), (const void *) 0);
	gles3_shim_texcoord_pointer(2, GL_FLOAT, sizeof(vertices[0]),
	                            (const void *) (3 * sizeof(float)));
	gles3_shim_enable_client_state(GL_VERTEX_ARRAY);
	gles3_shim_enable_client_state(GL_TEXTURE_COORD_ARRAY);
	gles3_shim_disable_client_state(GL_COLOR_ARRAY);
	gles3_shim_draw_arrays(GL_TRIANGLES, 0, 3);
	gles3_shim_draw_arrays(GL_TRIANGLES, 3, 3);
	glReadPixels(4, 4, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, center_pixel);
	if (error == GL_NO_ERROR)
		error = glGetError();
	passed = error == GL_NO_ERROR &&
	         center_pixel[0] == 255 && center_pixel[1] == 255 &&
	         center_pixel[2] == 255 && center_pixel[3] == 255;

	va_size = saved_va.size;
	va_type = saved_va.type;
	va_stride = saved_va.stride;
	va_ptr = saved_va.ptr;
	va_buffer = saved_va.buffer;
	ca_size = saved_ca.size;
	ca_type = saved_ca.type;
	ca_stride = saved_ca.stride;
	ca_ptr = saved_ca.ptr;
	ca_buffer = saved_ca.buffer;
	ta_size = saved_ta.size;
	ta_type = saved_ta.type;
	ta_stride = saved_ta.stride;
	ta_ptr = saved_ta.ptr;
	ta_buffer = saved_ta.buffer;
	vertex_array_enabled = saved_vertex_enabled;
	color_array_enabled = saved_color_enabled;
	texcoord_array_enabled = saved_texcoord_enabled;
	mv_stack = saved_mv_stack;
	proj_stack = saved_proj_stack;
	cur_stack = saved_cur_stack;
	memcpy(mvp, saved_mvp, sizeof(mvp));
	mvp_dirty = saved_mvp_dirty;
	tex2d_enabled = saved_tex2d_enabled;
	alpha_test_enabled = saved_alpha_test_enabled;
	memcpy(flat_color, saved_flat_color, sizeof(flat_color));
	state_dirty = saved_state_dirty;
	glBindBuffer(GL_ARRAY_BUFFER, (GLuint) previous_buffer);
	glDeleteBuffers(1, &probe_vbo);
	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) previous_framebuffer);
	glDeleteFramebuffers(1, &probe_framebuffer);
	glBindTexture(GL_TEXTURE_2D, (GLuint) previous_texture);
	glDeleteTextures(1, &probe_texture);
	glViewport(previous_viewport[0], previous_viewport[1],
	           previous_viewport[2], previous_viewport[3]);
	glClearColor(previous_clear_color[0], previous_clear_color[1],
	             previous_clear_color[2], previous_clear_color[3]);
	if (previous_blend) glEnable(GL_BLEND);
	if (previous_cull) glEnable(GL_CULL_FACE);
	if (previous_depth) glEnable(GL_DEPTH_TEST);
	if (previous_scissor) glEnable(GL_SCISSOR_TEST);
	gles3_shim_bind_program(previous_program);
	return passed;
}

/* ------------------------------------------------------------------ */
/* Draw wrapper                                                        */
/* ------------------------------------------------------------------ */

void gles3_shim_draw_arrays(GLenum mode, GLint first, GLsizei count)
{
	GLint previous_buffer;
	GLuint draw_buffer = 0;
	int va_active = external_prog ? (va_ptr != NULL || va_buffer != 0) : vertex_array_enabled && (va_ptr != NULL || va_buffer != 0);
	int ca_active = external_prog ? (ca_ptr != NULL || ca_buffer != 0) : color_array_enabled && (ca_ptr != NULL || ca_buffer != 0);
	int ta_active = external_prog ? (ta_ptr != NULL || ta_buffer != 0) : texcoord_array_enabled && (ta_ptr != NULL || ta_buffer != 0);
	int ta2_active = external_prog && (ta2_ptr != NULL || ta2_buffer != 0);
	const struct gles3_shim_array_source sources[] = {
		{ va_active, va_buffer },
		{ ca_active, ca_buffer },
		{ ta_active, ta_buffer },
		{ ta2_active, ta2_buffer }
	};
	int source_kind = gles3_shim_choose_array_source(sources, 4, &draw_buffer);

	glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_buffer);

	if (source_kind == GLES3_SHIM_ARRAY_SOURCE_REJECT) {
		LOGE("Rejected draw with mixed client arrays or array-buffer bindings");
		return;
	}

	if (!external_prog)
		gles3_shim_flush_state();

	if (source_kind == GLES3_SHIM_ARRAY_SOURCE_BUFFER) {
		glBindBuffer(GL_ARRAY_BUFFER, draw_buffer);
		if (va_active) {
			glVertexAttribPointer(ATTR_POS, va_size, va_type, GL_FALSE, va_stride, va_ptr);
			glEnableVertexAttribArray(ATTR_POS);
		} else {
			glDisableVertexAttribArray(ATTR_POS);
		}
		if (ca_active) {
			glVertexAttribPointer(ATTR_COLOR, ca_size, ca_type, GL_FALSE, ca_stride, ca_ptr);
			glEnableVertexAttribArray(ATTR_COLOR);
		} else {
			glDisableVertexAttribArray(ATTR_COLOR);
		}
		if (ta_active) {
			glVertexAttribPointer(ATTR_TEXCOORD, ta_size, ta_type, GL_FALSE, ta_stride, ta_ptr);
			glEnableVertexAttribArray(ATTR_TEXCOORD);
		} else {
			glDisableVertexAttribArray(ATTR_TEXCOORD);
		}
		if (external_prog) {
			if (ta2_active) {
				glVertexAttribPointer(3, ta2_size, ta2_type, GL_FALSE, ta2_stride, ta2_ptr);
				glEnableVertexAttribArray(3);
			} else {
				glDisableVertexAttribArray(3);
			}
		}
		glDrawArrays(mode, first, count);
		glBindBuffer(GL_ARRAY_BUFFER, (GLuint) previous_buffer);
		if (external_prog)
			glDisableVertexAttribArray(3);
		return;
	}

	if (!external_prog) {
		/* GLES 3.0 requires vertex data in buffer objects -- no client-side
		 * arrays.  Upload the deferred client pointers to our streaming VBO. */
		GLsizei nverts = first + count;
		int va_row = va_stride ? va_stride : va_size * gl_type_size(va_type);
		int ca_row = ca_stride ? ca_stride : ca_size * gl_type_size(ca_type);
		int ta_row = ta_stride ? ta_stride : ta_size * gl_type_size(ta_type);
		int va_bytes = (vertex_array_enabled && va_ptr) ? nverts * va_row : 0;
		int ca_bytes = (color_array_enabled && ca_ptr) ? nverts * ca_row : 0;
		int ta_bytes = (texcoord_array_enabled && ta_ptr) ? nverts * ta_row : 0;
		int total = va_bytes + ca_bytes + ta_bytes;
		int off = 0;

		glBindBuffer(GL_ARRAY_BUFFER, shim_vbo);
		gles3_shim_reserve_stream_data(total);

		if (va_bytes) {
			memcpy(shim_stream_data + off, va_ptr, va_bytes);
			glVertexAttribPointer(ATTR_POS, va_size, va_type, GL_FALSE,
			                      va_stride, (const void *) (intptr_t) off);
			glEnableVertexAttribArray(ATTR_POS);
			off += va_bytes;
		} else {
			glDisableVertexAttribArray(ATTR_POS);
		}

		if (ca_bytes) {
			memcpy(shim_stream_data + off, ca_ptr, ca_bytes);
			glVertexAttribPointer(ATTR_COLOR, ca_size, ca_type, GL_FALSE,
			                      ca_stride, (const void *) (intptr_t) off);
			glEnableVertexAttribArray(ATTR_COLOR);
			off += ca_bytes;
		} else {
			glDisableVertexAttribArray(ATTR_COLOR);
		}

		if (ta_bytes) {
			memcpy(shim_stream_data + off, ta_ptr, ta_bytes);
			glVertexAttribPointer(ATTR_TEXCOORD, ta_size, ta_type, GL_FALSE,
			                      ta_stride, (const void *) (intptr_t) off);
			glEnableVertexAttribArray(ATTR_TEXCOORD);
			off += ta_bytes;
		} else {
			glDisableVertexAttribArray(ATTR_TEXCOORD);
		}
		if (total > 0)
			glBufferData(GL_ARRAY_BUFFER, total, shim_stream_data, GL_STREAM_DRAW);
	} else {
		GLsizei nverts = first + count;
		int va_row = va_stride ? va_stride : va_size * gl_type_size(va_type);
		int ca_row = ca_stride ? ca_stride : ca_size * gl_type_size(ca_type);
		int ta_row = ta_stride ? ta_stride : ta_size * gl_type_size(ta_type);
		int ta2_row = ta2_stride ? ta2_stride : ta2_size * gl_type_size(ta2_type);
		int va_bytes = va_ptr ? nverts * va_row : 0;
		int ca_bytes = ca_ptr ? nverts * ca_row : 0;
		int ta_bytes = ta_ptr ? nverts * ta_row : 0;
		int ta2_bytes = ta2_ptr ? nverts * ta2_row : 0;
		int total = va_bytes + ca_bytes + ta_bytes + ta2_bytes;
		int off = 0;

		glBindBuffer(GL_ARRAY_BUFFER, shim_vbo);
		gles3_shim_reserve_stream_data(total);

		if (va_bytes) {
			memcpy(shim_stream_data + off, va_ptr, va_bytes);
			glVertexAttribPointer(ATTR_POS, va_size, va_type, GL_FALSE,
			                      va_stride, (const void *) (intptr_t) off);
			glEnableVertexAttribArray(ATTR_POS);
			off += va_bytes;
		} else {
			glDisableVertexAttribArray(ATTR_POS);
		}

		if (ca_bytes) {
			memcpy(shim_stream_data + off, ca_ptr, ca_bytes);
			glVertexAttribPointer(ATTR_COLOR, ca_size, ca_type, GL_FALSE,
			                      ca_stride, (const void *) (intptr_t) off);
			glEnableVertexAttribArray(ATTR_COLOR);
			off += ca_bytes;
		} else {
			glDisableVertexAttribArray(ATTR_COLOR);
		}

		if (ta_bytes) {
			memcpy(shim_stream_data + off, ta_ptr, ta_bytes);
			glVertexAttribPointer(ATTR_TEXCOORD, ta_size, ta_type, GL_FALSE,
			                      ta_stride, (const void *) (intptr_t) off);
			glEnableVertexAttribArray(ATTR_TEXCOORD);
			off += ta_bytes;
		} else {
			glDisableVertexAttribArray(ATTR_TEXCOORD);
		}

		if (ta2_bytes) {
			memcpy(shim_stream_data + off, ta2_ptr, ta2_bytes);
			glVertexAttribPointer(3, ta2_size, ta2_type, GL_FALSE,
			                      ta2_stride, (const void *) (intptr_t) off);
			glEnableVertexAttribArray(3);
			off += ta2_bytes;
		} else {
			glDisableVertexAttribArray(3);
		}
		if (total > 0)
			glBufferData(GL_ARRAY_BUFFER, total, shim_stream_data, GL_STREAM_DRAW);
	}

	glDrawArrays(mode, first, count);

	glBindBuffer(GL_ARRAY_BUFFER, (GLuint) previous_buffer);
	if (external_prog)
		glDisableVertexAttribArray(3);
}
