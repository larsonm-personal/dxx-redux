// XL volume fog owns its buffers and shaders; every pass restores the caller's GL state
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <algorithm>
#include <array>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#include "3d.h"
#include "ogl_init.h"
#include "dxxerror.h"
}
#include "fog.h"
#include "../fog_shader.h"

#if defined(OGLES) && !defined(ANDROID)
// Older fixed-function GLES builds retain stock rendering and reject authored fog
extern "C" const char *ogl_xl_fog_prepare(void)
{
	return "XL fog requires OpenGL 3 or OpenGL ES 3; this renderer uses OpenGL ES 1";
}
extern "C" void ogl_xl_fog_volumes(const short *, int) {}
extern "C" void ogl_xl_fog_composite(void) {}
extern "C" void ogl_xl_fog_destroy(int) {}
extern "C" void ogl_xl_fog_diagnostics(struct xl_fog_diagnostics *out)
{
	*out = {};
}
extern "C" const char *ogl_xl_fog_gpu_probe(struct xl_fog_gpu_result *out)
{
	*out = {};
	return ogl_xl_fog_prepare();
}
extern "C" void ogl_xl_fog_scene_request(void) {}
extern "C" void ogl_xl_fog_scene_result(struct xl_fog_scene_result *out)
{
	*out = {};
}
extern "C" const char *ogl_xl_fog_scene_error(void)
{
	return ogl_xl_fog_prepare();
}
#else
// This backend owns its VAO and program. Keep shim binding caches synchronized,
// but bypass its fixed-function draw dispatch while the fog shader is installed
#ifdef ANDROID
#undef glDrawArrays
#endif

namespace
{
void program(GLuint name)
{
#ifdef ANDROID
	gles3_shim_bind_program(name);
#else
	glUseProgram(name);
#endif
}
void enabled(GLenum capability, GLboolean value)
{
	if (value) glEnable(capability);
	else glDisable(capability);
}
void clear_depth(float value)
{
#ifdef ANDROID
	glClearDepthf(value);
#else
	glClearDepth(value);
#endif
}
struct SavedState {
	GLint read, draw, renderbuffer, viewport[4], scissor_box[4], active_texture, textures[2], current_program, vao, buffer;
	GLint blend_src_rgb, blend_dst_rgb, blend_src_alpha, blend_dst_alpha, blend_rgb, blend_alpha, cull_mode, front_face;
	GLfloat clear[4], depth_clear;
	GLboolean blend, depth, depth_write, scissor, cull, color_mask[4];
#ifndef ANDROID
	GLboolean alpha;
#endif
	SavedState()
	{
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
		glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
		glGetIntegerv(GL_VIEWPORT, viewport);
		glGetIntegerv(GL_SCISSOR_BOX, scissor_box);
		glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
		for (int unit = 0; unit < 2; ++unit) {
			glActiveTexture(GL_TEXTURE0 + unit);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures[unit]);
		}
		glActiveTexture(active_texture);
		glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
		glGetIntegerv(GL_BLEND_SRC_RGB, &blend_src_rgb);
		glGetIntegerv(GL_BLEND_DST_RGB, &blend_dst_rgb);
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src_alpha);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dst_alpha);
		glGetIntegerv(GL_BLEND_EQUATION_RGB, &blend_rgb);
		glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blend_alpha);
		glGetIntegerv(GL_CULL_FACE_MODE, &cull_mode);
		glGetIntegerv(GL_FRONT_FACE, &front_face);
		glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
		glGetFloatv(GL_DEPTH_CLEAR_VALUE, &depth_clear);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
		glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
		blend = glIsEnabled(GL_BLEND);
		depth = glIsEnabled(GL_DEPTH_TEST);
		scissor = glIsEnabled(GL_SCISSOR_TEST);
		cull = glIsEnabled(GL_CULL_FACE);
#ifndef ANDROID
		alpha = glIsEnabled(GL_ALPHA_TEST);
#endif
	}
	~SavedState()
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
		glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		glScissor(scissor_box[0], scissor_box[1], scissor_box[2], scissor_box[3]);
		for (int unit = 0; unit < 2; ++unit) {
			glActiveTexture(GL_TEXTURE0 + unit);
			glBindTexture(GL_TEXTURE_2D, textures[unit]);
		}
		glActiveTexture(active_texture);
		program(current_program);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBlendFuncSeparate(blend_src_rgb, blend_dst_rgb, blend_src_alpha, blend_dst_alpha);
		glBlendEquationSeparate(blend_rgb, blend_alpha);
		glCullFace(cull_mode);
		glFrontFace(front_face);
		glClearColor(clear[0], clear[1], clear[2], clear[3]);
		clear_depth(depth_clear);
		glDepthMask(depth_write);
		glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
		enabled(GL_BLEND, blend);
		enabled(GL_DEPTH_TEST, depth);
		enabled(GL_SCISSOR_TEST, scissor);
		enabled(GL_CULL_FACE, cull);
#ifndef ANDROID
		enabled(GL_ALPHA_TEST, alpha);
#endif
	}
	bool same(const SavedState &other) const
	{
#ifndef ANDROID
		if (alpha != other.alpha) return false;
#endif
		return read == other.read && draw == other.draw && renderbuffer == other.renderbuffer && depth_clear == other.depth_clear && active_texture == other.active_texture && current_program == other.current_program && vao == other.vao && buffer == other.buffer &&
		       blend_src_rgb == other.blend_src_rgb && blend_dst_rgb == other.blend_dst_rgb && blend_src_alpha == other.blend_src_alpha && blend_dst_alpha == other.blend_dst_alpha &&
		       blend_rgb == other.blend_rgb && blend_alpha == other.blend_alpha && cull_mode == other.cull_mode && front_face == other.front_face &&
		       blend == other.blend && depth == other.depth && depth_write == other.depth_write && scissor == other.scissor && cull == other.cull &&
		       std::equal(viewport, viewport + 4, other.viewport) && std::equal(scissor_box, scissor_box + 4, other.scissor_box) &&
		       std::equal(textures, textures + 2, other.textures) && std::equal(clear, clear + 4, other.clear) && std::equal(color_mask, color_mask + 4, other.color_mask);
	}
};
struct ReadbackState {
	GLint pack_buffer, alignment, row_length, skip_rows, skip_pixels;
	ReadbackState()
	{
		glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack_buffer);
		glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
		glGetIntegerv(GL_PACK_ROW_LENGTH, &row_length);
		glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
		glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glPixelStorei(GL_PACK_ROW_LENGTH, 0);
		glPixelStorei(GL_PACK_SKIP_ROWS, 0);
		glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	}
	~ReadbackState()
	{
		glBindBuffer(GL_PIXEL_PACK_BUFFER, pack_buffer);
		glPixelStorei(GL_PACK_ALIGNMENT, alignment);
		glPixelStorei(GL_PACK_ROW_LENGTH, row_length);
		glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
		glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels);
	}
};
struct Buffers {
	GLuint volume[2]{}, fbo[2]{}, depth = 0, depth_fbo = 0;
	int width = 0, height = 0, bits = 0, stencil = 0;
} buffers;
GLuint volume_program = 0, composite_program = 0, vao = 0, vbo = 0;
// MIN/MAX accumulation is order-independent; batch by type, cap and culling
std::array<std::vector<GLfloat>, 16> cap_batches;
xl_fog_diagnostics diagnostics{};
std::string failure;
int prepared_view[4]{}, prepared_target = 0, prepared_types = 0;
bool capable = false;
#if defined(INTROSPECT_ON) || defined(DXX_XL_DEVELOPER_PROBE)
void scene_before(GLuint scene, const GLint view[4]);
void scene_after(GLuint scene, const GLint view[4]);
#endif
const char *vertex_source = "attribute vec3 position; uniform mat4 projection; void main() { gl_Position = projection * vec4(position, 1.0); }";
const GLfloat identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
// Only the XL volumetric profile selects this projection in ogl_start_frame
const GLfloat projection[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -5001.f / 4999.f, -1, 0, 0, -10000.f / 4999.f, 0 };
const GLfloat quad[18] = { -1, -1, 0, 1, -1, 0, 1, 1, 0, -1, -1, 0, 1, 1, 0, -1, 1, 0 };

void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
void errors(const char *stage)
{
	const GLenum error = glGetError();
	if (error != GL_NO_ERROR) throw std::runtime_error(std::string(stage) + ": OpenGL error " + std::to_string(error));
}
GLuint shader(GLenum type, const std::string &source)
{
	const GLuint result = glCreateShader(type);
	require(result != 0, "Cannot allocate fog shader");
	const char *text = source.c_str();
	glShaderSource(result, 1, &text, nullptr);
	glCompileShader(result);
	GLint ok = 0;
	glGetShaderiv(result, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[1024]{};
		glGetShaderInfoLog(result, sizeof(log), nullptr, log);
		glDeleteShader(result);
		throw std::runtime_error(std::string("Cannot compile fog shader: ") + log);
	}
	return result;
}
GLuint compile(const std::string &fragment)
{
#ifdef ANDROID
	const std::string prefix = "#version 100\nprecision highp float;\nprecision highp sampler2D;\n";
#else
	const std::string prefix = "#version 120\n";
#endif
	const GLuint vert = shader(GL_VERTEX_SHADER, prefix + vertex_source);
	GLuint frag = 0, result = 0;
	try {
		frag = shader(GL_FRAGMENT_SHADER, prefix + fragment);
		result = glCreateProgram();
		require(result != 0, "Cannot allocate fog program");
		glAttachShader(result, vert);
		glAttachShader(result, frag);
		glBindAttribLocation(result, 0, "position");
		glLinkProgram(result);
		GLint ok = 0;
		glGetProgramiv(result, GL_LINK_STATUS, &ok);
		if (!ok) {
			char log[1024]{};
			glGetProgramInfoLog(result, sizeof(log), nullptr, log);
			throw std::runtime_error(std::string("Cannot link fog program: ") + log);
		}
	} catch (...) {
		if (frag) glDeleteShader(frag);
		if (result) glDeleteProgram(result);
		glDeleteShader(vert);
		throw;
	}
	glDeleteShader(vert);
	glDeleteShader(frag);
	return result;
}
void release_buffers(bool lost)
{
	if (!lost) {
		glDeleteTextures(2, buffers.volume);
		glDeleteFramebuffers(2, buffers.fbo);
		if (buffers.depth) glDeleteTextures(1, &buffers.depth);
		if (buffers.depth_fbo) glDeleteFramebuffers(1, &buffers.depth_fbo);
	}
	buffers = {};
	prepared_types = 0;
}
void texture(GLuint *name, int width, int height, GLenum internal, GLenum format, GLenum type)
{
	glGenTextures(1, name);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *name);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, format, type, nullptr);
}
void resize(int width, int height, int bits, int stencil)
{
	if (buffers.width == width && buffers.height == height && buffers.bits == bits && buffers.stencil == stencil) return;
	require(width > 0 && height > 0 && bits >= 16, "Fog requires a valid scene viewport and depth buffer");
	release_buffers(false);
	for (int pair = 0; pair < 2; ++pair) {
		texture(&buffers.volume[pair], width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT);
		glGenFramebuffers(1, &buffers.fbo[pair]);
		glBindFramebuffer(GL_FRAMEBUFFER, buffers.fbo[pair]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, buffers.volume[pair], 0);
		require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "XL fog requires renderable RGBA32F volume buffers");
	}
	const bool packed = stencil > 0;
	const GLenum internal = packed ? GL_DEPTH24_STENCIL8 : bits <= 16 ? GL_DEPTH_COMPONENT16
	                                                   : bits <= 24   ? GL_DEPTH_COMPONENT24
	                                                                  : GL_DEPTH_COMPONENT32F;
	const GLenum type = packed ? GL_UNSIGNED_INT_24_8 : bits <= 16 ? GL_UNSIGNED_SHORT
	                                                : bits <= 24   ? GL_UNSIGNED_INT
	                                                               : GL_FLOAT;
	texture(&buffers.depth, width, height, internal, packed ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT, type);
	glGenFramebuffers(1, &buffers.depth_fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, buffers.depth_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, packed ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, buffers.depth, 0);
	const GLenum none = GL_NONE;
	glDrawBuffers(1, &none);
	glReadBuffer(GL_NONE);
	require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Cannot allocate XL fog scene depth snapshot");
	errors("Fog buffer allocation");
	buffers.width = width;
	buffers.height = height;
	buffers.bits = bits;
	buffers.stencil = stencil;
}
void draw(const GLfloat *vertices, int count)
{
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, count * 3 * sizeof(GLfloat), vertices, GL_STREAM_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glDrawArrays(GL_TRIANGLES, 0, count);
}
void use(GLuint name, const GLfloat *matrix)
{
#ifndef ANDROID
	glDisable(GL_ALPHA_TEST);
#endif
	program(name);
	glUniformMatrix4fv(glGetUniformLocation(name, "projection"), 1, GL_FALSE, matrix);
}
void depth_snapshot(GLuint scene, const GLint view[4])
{
	glDisable(GL_SCISSOR_TEST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, scene);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, buffers.depth_fbo);
	// GLES multisample resolves require identical source/destination bounds
	glBlitFramebuffer(view[0], view[1], view[0] + view[2], view[1] + view[3], view[0], view[1], view[0] + view[2], view[1] + view[3], GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	errors("Fog depth snapshot");
}
void verify_depth_transfer()
{
	GLuint scene = 0, color = 0, depth = 0, probe = 0;
	auto release = [&] {
		glDeleteFramebuffers(1, &scene);
		glDeleteRenderbuffers(1, &color);
		glDeleteRenderbuffers(1, &depth);
		if (probe) glDeleteProgram(probe);
	};
	try {
		ReadbackState pack;
		probe = compile("uniform sampler2D depthTex; void main() { gl_FragColor = vec4(texture2D(depthTex, vec2(0.5)).r); }");
		glGenFramebuffers(1, &scene);
		glGenRenderbuffers(1, &color);
		glGenRenderbuffers(1, &depth);
		const GLint view[4] = { 0, 0, 1, 1 };
		for (int bits : { 16, 24 }) {
			diagnostics.depth_copy_bits = bits;
			resize(1, 1, bits, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, scene);
			glBindRenderbuffer(GL_RENDERBUFFER, color);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 1);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
			glBindRenderbuffer(GL_RENDERBUFFER, depth);
			glRenderbufferStorage(GL_RENDERBUFFER, bits == 16 ? GL_DEPTH_COMPONENT16 : GL_DEPTH_COMPONENT24, 1, 1);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
			require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Cannot check XL fog scene-depth transfer");
			glDisable(GL_SCISSOR_TEST);
			glDepthMask(GL_TRUE);
			clear_depth(.625f);
			glClear(GL_DEPTH_BUFFER_BIT);
			depth_snapshot(scene, view);
			glBindFramebuffer(GL_FRAMEBUFFER, buffers.fbo[0]);
			glViewport(0, 0, 1, 1);
			glDisable(GL_BLEND);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			use(probe, identity);
			glUniform1i(glGetUniformLocation(probe, "depthTex"), 0);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, buffers.depth);
			draw(quad, 6);
			GLfloat observed[4]{};
			glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, observed);
			diagnostics.depth_copy_value = observed[0];
			errors("Fog scene-depth transfer check");
			require(std::abs(observed[0] - .625f) < .0001f, "XL fog requires working scene-depth transfer; this graphics driver did not preserve depth");
		}
	} catch (...) {
		release();
		throw;
	}
	release();
}
void setup()
{
	if (capable) return;
#ifndef ANDROID
	require(GLEW_VERSION_3_0, "XL fog requires OpenGL 3 framebuffer and floating-point blending support");
#endif
	if (!volume_program) volume_program = compile(xl_fog_volume_glsl);
	if (!composite_program) composite_program = compile(std::string(xl_fog_math_glsl) + xl_fog_composite_glsl);
	if (!vao) glGenVertexArrays(1, &vao);
	if (!vbo) glGenBuffers(1, &vbo);
	resize(1, 1, 16, 0);
	// Exercise actual float min/max blending, not just an extension string
	const GLuint probe = compile("void main() { gl_FragColor = vec4(0.25, 0.75, 0.125, 0.875); }");
	try {
		ReadbackState pack;
		glBindFramebuffer(GL_FRAMEBUFFER, buffers.fbo[0]);
		glViewport(0, 0, 1, 1);
		glDisable(GL_SCISSOR_TEST);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glEnable(GL_BLEND);
		use(probe, identity);
		for (int mode = 0; mode < 2; ++mode) {
			glClearColor(.5f, .5f, .5f, .5f);
			glClear(GL_COLOR_BUFFER_BIT);
			glBlendEquation(mode ? GL_MAX : GL_MIN);
			draw(quad, 6);
			GLfloat pixel[4]{};
			glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixel);
			const GLfloat colors[4] = { .25f, .75f, .125f, .875f };
			for (int c = 0; c < 4; ++c) require(pixel[c] == (mode ? (std::max) (colors[c], .5f) : (std::min) (colors[c], .5f)), "XL fog requires working RGBA32F min/max blending");
		}
		errors("Fog capability probe");
	} catch (...) {
		glDeleteProgram(probe);
		throw;
	}
	glDeleteProgram(probe);
	verify_depth_transfer();
	capable = true;
	++diagnostics.context_rebuilds;
}
void volume_begin(int pair, const GLint view[4])
{
	glBindFramebuffer(GL_FRAMEBUFFER, buffers.fbo[pair]);
	glViewport(view[0], view[1], view[2], view[3]);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClearColor(1, 0, 1, 0);
	glClear(GL_COLOR_BUFFER_BIT);
}
void cap_mode(int type, bool near_cap)
{
	const int channel = (type % 2) * 2 + (near_cap ? 0 : 1);
	glColorMask(channel == 0, channel == 1, channel == 2, channel == 3);
	glBlendEquation(near_cap ? GL_MIN : GL_MAX);
}
void transform(const int32_t points[4][3], GLfloat triangles[18])
{
	GLfloat positions[4][3];
	for (int corner = 0; corner < 4; ++corner) {
		vms_vector world = { points[corner][0], points[corner][1], points[corner][2] };
		g3s_point point;
		g3_rotate_point(&point, &world);
		positions[corner][0] = f2fl(point.p3_vec.x);
		positions[corner][1] = f2fl(point.p3_vec.y);
		positions[corner][2] = -f2fl(point.p3_vec.z);
	}
	const int corners[6] = { 0, 1, 2, 0, 2, 3 };
	for (int i = 0; i < 6; ++i) std::copy(positions[corners[i]], positions[corners[i]] + 3, triangles + i * 3);
}
void fail(const std::exception &error)
{
	failure = error.what();
	++diagnostics.failures;
	prepared_types = 0;
	Error("XL fog rendering failed: %s", failure.c_str());
}
} // namespace

extern "C" const char *ogl_xl_fog_prepare(void)
{
	if (!gl_initialized) return "XL fog requires an active OpenGL renderer; headless loading cannot verify visibility";
#ifndef ANDROID
	if (!GLEW_VERSION_3_0) return "XL fog requires OpenGL 3 framebuffer and floating-point blending support";
#endif
	try {
		SavedState saved;
		setup();
		// Check the actual surface format and allocation before committing a level
		glBindFramebuffer(GL_FRAMEBUFFER, saved.draw);
		GLint bits = 0, stencil = 0;
		glGetIntegerv(GL_DEPTH_BITS, &bits);
		glGetIntegerv(GL_STENCIL_BITS, &stencil);
		require(saved.viewport[0] >= 0 && saved.viewport[1] >= 0, "Fog requires a nonnegative scene viewport");
		resize(saved.viewport[0] + saved.viewport[2], saved.viewport[1] + saved.viewport[3], bits, stencil);
		depth_snapshot(saved.draw, saved.viewport);
		++diagnostics.preparations;
		return nullptr;
	} catch (const std::exception &error) {
		failure = error.what();
		++diagnostics.failures;
		return failure.c_str();
	}
}

extern "C" void ogl_xl_fog_volumes(const short *segments, int count)
{
	prepared_types = 0;
	if (!xl_fog_active()) return;
	try {
		SavedState saved;
		setup();
		glBindFramebuffer(GL_FRAMEBUFFER, saved.draw);
		GLint bits = 0, stencil = 0;
		glGetIntegerv(GL_DEPTH_BITS, &bits);
		glGetIntegerv(GL_STENCIL_BITS, &stencil);
		require(saved.viewport[0] >= 0 && saved.viewport[1] >= 0, "Fog requires a nonnegative scene viewport");
		resize(saved.viewport[0] + saved.viewport[2], saved.viewport[1] + saved.viewport[3], bits, stencil);
		depth_snapshot(saved.draw, saved.viewport);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CW);
		glEnable(GL_BLEND);
		use(volume_program, projection);
		glUniform1i(glGetUniformLocation(volume_program, "sceneDepthTex"), 0);
		glUniform2f(glGetUniformLocation(volume_program, "windowSize"), static_cast<float>(buffers.width), static_cast<float>(buffers.height));
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, buffers.depth);
		for (auto &batch : cap_batches) batch.clear();
		for (int mode = 0; mode < 2; ++mode) {
			for (int i = count; i-- > 0;)
				for (int side = 0; side < 6; ++side) {
					int32_t points[4][3];
					int reverse = 0;
					const int type = xl_fog_boundary_points(segments[i], side, mode, points, &reverse);
					if (!type) continue;
					require(type >= 1 && type <= 4, "Invalid fog boundary type");
					GLfloat triangles[18];
					transform(points, triangles);
					auto &batch = cap_batches[((type - 1) * 2 + mode) * 2 + (reverse != 0)];
					batch.insert(batch.end(), triangles, triangles + 18);
					prepared_types |= 1 << (type - 1);
					++diagnostics.boundaries;
				}
		}
		for (int type = 0; type < 4; ++type) {
			if (!(type % 2)) volume_begin(type / 2, saved.viewport);
			for (int mode = 0; mode < 2; ++mode) {
				cap_mode(type, mode != 0);
				for (int reverse = 0; reverse < 2; ++reverse) {
					const auto &batch = cap_batches[(type * 2 + mode) * 2 + reverse];
					if (batch.empty()) continue;
					glCullFace(reverse ? GL_FRONT : GL_BACK);
					draw(batch.data(), static_cast<int>(batch.size() / 3));
				}
			}
		}
		errors("Fog volume accumulation");
		std::copy(saved.viewport, saved.viewport + 4, prepared_view);
		prepared_target = saved.draw;
		diagnostics.width = buffers.width;
		diagnostics.height = buffers.height;
		diagnostics.depth_bits = buffers.bits;
		diagnostics.last_types = prepared_types;
		++diagnostics.volume_passes;
	} catch (const std::exception &error) {
		fail(error);
	}
}

extern "C" void ogl_xl_fog_composite(void)
{
	if (!xl_fog_active() || !prepared_types) return;
	try {
		SavedState saved;
		require(saved.draw == prepared_target && std::equal(saved.viewport, saved.viewport + 4, prepared_view), "Fog view changed between geometry and composition");
		float colors[4][4]{};
		require(xl_fog_colors(colors), "Fog lost its owned level settings");
		depth_snapshot(saved.draw, saved.viewport);
#if defined(INTROSPECT_ON) || defined(DXX_XL_DEVELOPER_PROBE)
		scene_before(saved.draw, saved.viewport);
#endif
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, saved.draw);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendEquation(GL_FUNC_ADD);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		// Presentation alpha remains opaque on Android; RGB follows source blending
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
		use(composite_program, identity);
		glUniform1i(glGetUniformLocation(composite_program, "fogTex"), 0);
		glUniform1i(glGetUniformLocation(composite_program, "sceneDepthTex"), 1);
		glUniform2f(glGetUniformLocation(composite_program, "windowSize"), static_cast<float>(buffers.width), static_cast<float>(buffers.height));
		glUniform2f(glGetUniformLocation(composite_program, "scenePlanes"), 1.f, 5000.f);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, buffers.depth);
		glActiveTexture(GL_TEXTURE0);
		const float none[4]{};
		for (int pair = 0; pair < 2; ++pair) {
			if (!(prepared_types & (3 << (pair * 2)))) continue;
			glBindTexture(GL_TEXTURE_2D, buffers.volume[pair]);
			glUniform4fv(glGetUniformLocation(composite_program, "fogColor1"), 1, prepared_types & (1 << (pair * 2)) ? colors[pair * 2] : none);
			glUniform4fv(glGetUniformLocation(composite_program, "fogColor2"), 1, prepared_types & (2 << (pair * 2)) ? colors[pair * 2 + 1] : none);
			draw(quad, 6);
		}
		errors("Fog composition");
#if defined(INTROSPECT_ON) || defined(DXX_XL_DEVELOPER_PROBE)
		scene_after(saved.draw, saved.viewport);
#endif
		++diagnostics.composite_passes;
		prepared_types = 0;
	} catch (const std::exception &error) {
		fail(error);
	}
}

extern "C" void ogl_xl_fog_destroy(int lost_context)
{
	cap_batches = {};
	if (!volume_program && !composite_program && !vao && !vbo && !buffers.depth && !buffers.volume[0]) return;
	release_buffers(lost_context != 0);
	if (!lost_context) {
		if (volume_program) glDeleteProgram(volume_program);
		if (composite_program) glDeleteProgram(composite_program);
		if (vao) glDeleteVertexArrays(1, &vao);
		if (vbo) glDeleteBuffers(1, &vbo);
	}
	volume_program = composite_program = vao = vbo = 0;
	capable = false;
}
extern "C" void ogl_xl_fog_diagnostics(struct xl_fog_diagnostics *out)
{
	*out = diagnostics;
}

#if defined(INTROSPECT_ON) || defined(DXX_XL_DEVELOPER_PROBE)
#include "fog_gpu_test.h"
#include "fog_scene_test.h"
#else
extern "C" const char *ogl_xl_fog_gpu_probe(struct xl_fog_gpu_result *)
{
	return "Fog GPU controls require a diagnostic build";
}
extern "C" void ogl_xl_fog_scene_request(void) {}
extern "C" void ogl_xl_fog_scene_result(struct xl_fog_scene_result *out)
{
	*out = {};
}
extern "C" const char *ogl_xl_fog_scene_error(void)
{
	return "Fog scene controls require a diagnostic build";
}
#endif
#endif
