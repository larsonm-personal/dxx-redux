#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#endif
#include "particle_renderer.h"
#include "pog_renderer.h"
#include "../particle_render.h"
extern "C" {
#include "3d.h"
#include "ogl_init.h"
#include "internal.h"
void ogl_bindbmtex(grs_bitmap *);
}
#if !defined(OGLES) || defined(ANDROID)
#include "particle_soft_renderer.h"
#endif

namespace xl_compat
{
struct ParticleTexture::State {
	static State *head;
	State *previous = nullptr, *next = nullptr;
	grs_bitmap bitmap{};
	std::string name;
	State(const PogBitmap &pixels, const char *label) : name(label)
	{
		if (const char *error = ogl_xl_rgba_prepare(int(pixels.width), int(pixels.height))) throw std::runtime_error(error);
#if !defined(OGLES) || defined(ANDROID)
		if (gl_initialized) {
			prepare_particle_soft();
			verify_particle_soft_depth();
		}
#endif
		bitmap.bm_w = short(pixels.width);
		bitmap.bm_h = short(pixels.height);
		bitmap.bm_rowsize = short(pixels.width * 4);
		bitmap.bm_type = BM_RGBA;
		bitmap.bm_flags = pixels.flags & 7;
		bitmap.bm_data = const_cast<unsigned char *>(pixels.pixels.data());
		next = head;
		if (next) next->previous = this;
		head = this;
	}
	void release(bool abandoned)
	{
		if (abandoned) bitmap.gltexture = bitmap.gltexture_mask = nullptr;
		else ogl_freebmtexture(&bitmap);
	}
	~State()
	{
		release(false);
		if (previous) previous->next = next;
		else head = next;
		if (next) next->previous = previous;
	}
};
ParticleTexture::State *ParticleTexture::State::head = nullptr;
ParticleTexture::ParticleTexture(const PogBitmap &pixels, const char *name) : state(std::make_unique<State>(pixels, name)) {}
ParticleTexture::~ParticleTexture() = default;
void ParticleTexture::discard_context(bool abandoned)
{
#if !defined(OGLES) || defined(ANDROID)
	particle_soft.discard(abandoned);
#endif
	for (auto *texture = State::head; texture; texture = texture->next) texture->release(abandoned);
}
void ParticleTexture::draw(const ParticleQuad *quads, std::size_t count)
{
	if (!count) return;
	if (!state->bitmap.gltexture || !state->bitmap.gltexture->handle) {
		GLint active, binding;
		glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
		glActiveTexture(GL_TEXTURE0);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
		const char *error = ogl_xl_rgba_load(&state->bitmap, 1, state->name.c_str());
		glBindTexture(GL_TEXTURE_2D, binding);
		glActiveTexture(active);
		if (error) throw std::runtime_error(error);
	}
#if !defined(OGLES) || defined(ANDROID)
	if (particle_soft.active) draw_soft_particles(state->bitmap, quads, count);
	else
#endif
		draw_particle_quads(state->bitmap, quads, count);
}

void draw_particle_quads(grs_bitmap &image, const ParticleQuad *quads, std::size_t count)
{
	if (!count) return;
#if defined(OGLES) && !defined(ANDROID)
	throw std::runtime_error("XL particles require OpenGL 3 or OpenGL ES 3");
#else
	GLboolean depth_write;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
	const auto cull = glIsEnabled(GL_CULL_FACE), blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST);
	GLint depth_function;
	glGetIntegerv(GL_DEPTH_FUNC, &depth_function);
	GLint src_rgb, dst_rgb, src_alpha, dst_alpha, equation_rgb, equation_alpha, texture, active;
	glGetIntegerv(GL_BLEND_SRC_RGB, &src_rgb);
	glGetIntegerv(GL_BLEND_DST_RGB, &dst_rgb);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &src_alpha);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &dst_alpha);
	glGetIntegerv(GL_BLEND_EQUATION_RGB, &equation_rgb);
	glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equation_alpha);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
#ifndef ANDROID
	const auto textured = glIsEnabled(GL_TEXTURE_2D);
#else
	const bool textured = GL_TEXTURE_2D_enabled == 1;
	const int binary = gles3_shim_set_binary_texture(0);
#endif
	xl_rgba_alpha_state alpha;
	ogl_xl_rgba_alpha_begin(1, &alpha);
	// Use the same fixed-function/shim path as the other XL local renderers
	OGL_ENABLE(TEXTURE_2D);
	ogl_bindbmtex(&image);
	glDepthMask(GL_FALSE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	for (std::size_t i = 0; i < count; ++i) {
		const auto &quad = quads[i];
		if (quad.blend == ParticleBlend::multiply) glBlendFunc(GL_DST_COLOR, GL_ZERO);
		else if (quad.blend == ParticleBlend::alpha) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		else glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
		glVertexPointer(3, GL_FLOAT, sizeof(ParticleVertex), quad.vertices[0].position.data());
		glColorPointer(4, GL_FLOAT, sizeof(ParticleVertex), quad.vertices[0].color.data());
		glTexCoordPointer(2, GL_FLOAT, sizeof(ParticleVertex), quad.vertices[0].uv.data());
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glBlendFuncSeparate(src_rgb, dst_rgb, src_alpha, dst_alpha);
	glBlendEquationSeparate(equation_rgb, equation_alpha);
	if (!blend) glDisable(GL_BLEND);
	if (cull) glEnable(GL_CULL_FACE);
	glDepthMask(depth_write);
	glDepthFunc(depth_function);
	if (!depth) glDisable(GL_DEPTH_TEST);
	glBindTexture(GL_TEXTURE_2D, texture);
	if (!textured) { OGL_DISABLE(TEXTURE_2D); }
#ifdef ANDROID
	gles3_shim_set_binary_texture(binary);
#endif
	glActiveTexture(active);
	ogl_xl_rgba_alpha_end(&alpha);
#endif
}
} // namespace xl_compat
extern "C" void xl_particle_images_discard_context(int abandoned)
{
	xl_compat::ParticleTexture::discard_context(abandoned != 0);
}

extern "C" void xl_particle_soft_begin_scene(void)
{
#if !defined(OGLES) || defined(ANDROID)
	particle_soft.active = true;
	particle_soft.captured = false;
#endif
}
extern "C" void xl_particle_soft_end_scene(void)
{
#if !defined(OGLES) || defined(ANDROID)
	particle_soft.active = false;
	particle_soft.captured = false;
#endif
}
