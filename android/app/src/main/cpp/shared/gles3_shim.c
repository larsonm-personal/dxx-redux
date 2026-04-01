/* gles3_shim.c -- GLES 1.x fixed-function compatibility shim for GLES 3.0
 *
 * Provides a CPU-side matrix stack, shader-based vertex pipeline, and
 * fixed-function state emulation for the ~40 unique GLES 1.x calls used
 * by the DXX-Redux rendering code.
 *
 * Android port -- not compiled on desktop builds.
 */

#include "gles3_shim.h"

/* We need the real glDrawArrays/glEnable/glDisable, not our macro redirects */
#undef glDrawArrays
#undef glEnable
#undef glDisable

#include <string.h>
#include <math.h>
#include <stdint.h>
#include <android/log.h>
#include "android_crash_handler.h"

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
    "precision mediump float;\n"
    "in vec4 vColor;\n"
    "in vec2 vTexCoord;\n"
    "uniform sampler2D uTex;\n"
    "uniform int uTexEnabled;\n"
    "uniform int uAlphaTest;\n"
    "uniform float uAlphaRef;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "  vec4 c = vColor;\n"
    "  if (uTexEnabled != 0)\n"
    "    c *= texture(uTex, vTexCoord);\n"
    "  if (uAlphaTest != 0 && c.a < uAlphaRef)\n"
    "    discard;\n"
    "  fragColor = c;\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *src)
{
	GLuint s = glCreateShader(type);
	glShaderSource(s, 1, &src, NULL);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char buf[512];
		glGetShaderInfoLog(s, sizeof(buf), NULL, buf);
		LOGE("shader compile: %s", buf);
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

static GLint ca_size;
static GLenum ca_type;
static GLsizei ca_stride;
static const void *ca_ptr;

static GLint ta_size;
static GLenum ta_type;
static GLsizei ta_stride;
static const void *ta_ptr;

static int state_dirty = 1;
static GLuint external_prog;
static GLuint shim_vbo;

static int gl_type_size(GLenum type)
{
	switch (type) {
		case GL_BYTE: case GL_UNSIGNED_BYTE: return 1;
		case GL_SHORT: case GL_UNSIGNED_SHORT: return 2;
		default: return 4; /* GL_FLOAT, GL_INT, GL_UNSIGNED_INT */
	}
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

	tex2d_enabled = 0;
	alpha_test_enabled = 0;
	alpha_ref = 0.0f;
	flat_color[0] = flat_color[1] = flat_color[2] = flat_color[3] = 1.0f;

	vertex_array_enabled = 0;
	color_array_enabled = 0;
	texcoord_array_enabled = 0;

	GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
	GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
	shim_prog = glCreateProgram();
	glAttachShader(shim_prog, vs);
	glAttachShader(shim_prog, fs);
	glLinkProgram(shim_prog);
	GLint ok = 0;
	glGetProgramiv(shim_prog, GL_LINK_STATUS, &ok);
	if (!ok) {
		char buf[512];
		glGetProgramInfoLog(shim_prog, sizeof(buf), NULL, buf);
		LOGE("program link: %s", buf);
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

	glUseProgram(shim_prog);
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
	if (shim_vbo) {
		glDeleteBuffers(1, &shim_vbo);
		shim_vbo = 0;
	}
	if (shim_prog) {
		glDeleteProgram(shim_prog);
		shim_prog = 0;
	}
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
	va_size = size;
	va_type = type;
	va_stride = stride;
	va_ptr = ptr;
}

void gles3_shim_color_pointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
	ca_size = size;
	ca_type = type;
	ca_stride = stride;
	ca_ptr = ptr;
}

void gles3_shim_texcoord_pointer(GLint size, GLenum type, GLsizei stride, const void *ptr)
{
	ta_size = size;
	ta_type = type;
	ta_stride = stride;
	ta_ptr = ptr;
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

	glUseProgram(shim_prog);

	if (mvp_dirty) compute_mvp();
	glUniformMatrix4fv(u_mvp, 1, GL_FALSE, mvp);

	glUniform1i(u_tex_enabled, tex2d_enabled);
	glUniform1i(u_alpha_test, alpha_test_enabled);
	glUniform1f(u_alpha_ref, alpha_ref);

	glUniform1i(u_use_color_attr, color_array_enabled);
	glUniform4fv(u_flat_color, 1, flat_color);

	state_dirty = 0;
}

/* ------------------------------------------------------------------ */
/* External shader override (for OGL_MERGE multi-texture programs)     */
/* ------------------------------------------------------------------ */

void gles3_shim_use_external(GLuint prog)
{
	if (prog) {
		external_prog = prog;
		glUseProgram(prog);
		/* Update MVP for the external program */
		if (mvp_dirty) compute_mvp();
	} else {
		external_prog = 0;
		state_dirty = 1;
	}
}

/* Expose the computed MVP matrix for external programs that need it */
const float *gles3_shim_get_mvp(void)
{
	if (mvp_dirty) compute_mvp();
	return mvp;
}

/* ------------------------------------------------------------------ */
/* Draw wrapper                                                        */
/* ------------------------------------------------------------------ */

void gles3_shim_draw_arrays(GLenum mode, GLint first, GLsizei count)
{
	gles3_shim_flush_state();

	if (!external_prog) {
		/* GLES 3.0 requires vertex data in buffer objects -- no client-side
		 * arrays.  Upload the deferred client pointers to our streaming VBO. */
		GLsizei nverts = first + count;
		int va_row = va_stride ? va_stride : va_size * gl_type_size(va_type);
		int ca_row = ca_stride ? ca_stride : ca_size * gl_type_size(ca_type);
		int ta_row = ta_stride ? ta_stride : ta_size * gl_type_size(ta_type);
		int va_bytes = (vertex_array_enabled && va_ptr) ? nverts * va_row : 0;
		int ca_bytes = (color_array_enabled  && ca_ptr) ? nverts * ca_row : 0;
		int ta_bytes = (texcoord_array_enabled && ta_ptr) ? nverts * ta_row : 0;
		int total = va_bytes + ca_bytes + ta_bytes;
		int off = 0;

		glBindBuffer(GL_ARRAY_BUFFER, shim_vbo);
		if (total > 0)
			glBufferData(GL_ARRAY_BUFFER, total, NULL, GL_STREAM_DRAW);

		if (va_bytes) {
			glBufferSubData(GL_ARRAY_BUFFER, off, va_bytes, va_ptr);
			glVertexAttribPointer(ATTR_POS, va_size, va_type, GL_FALSE,
				va_stride, (const void *)(intptr_t)off);
			glEnableVertexAttribArray(ATTR_POS);
			off += va_bytes;
		} else {
			glDisableVertexAttribArray(ATTR_POS);
		}

		if (ca_bytes) {
			glBufferSubData(GL_ARRAY_BUFFER, off, ca_bytes, ca_ptr);
			glVertexAttribPointer(ATTR_COLOR, ca_size, ca_type, GL_FALSE,
				ca_stride, (const void *)(intptr_t)off);
			glEnableVertexAttribArray(ATTR_COLOR);
			off += ca_bytes;
		} else {
			glDisableVertexAttribArray(ATTR_COLOR);
		}

		if (ta_bytes) {
			glBufferSubData(GL_ARRAY_BUFFER, off, ta_bytes, ta_ptr);
			glVertexAttribPointer(ATTR_TEXCOORD, ta_size, ta_type, GL_FALSE,
				ta_stride, (const void *)(intptr_t)off);
			glEnableVertexAttribArray(ATTR_TEXCOORD);
			off += ta_bytes;
		} else {
			glDisableVertexAttribArray(ATTR_TEXCOORD);
		}
	}

	glDrawArrays(mode, first, count);

	if (!external_prog)
		glBindBuffer(GL_ARRAY_BUFFER, 0);
}
