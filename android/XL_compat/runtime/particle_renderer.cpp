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

namespace xl_compat
{
void draw_particle_quads(grs_bitmap &image, const ParticleQuad *quads, std::size_t count)
{
	if (!count) return;
#if defined(OGLES) && !defined(ANDROID)
	throw std::runtime_error("XL particles require OpenGL 3 or OpenGL ES 3");
#else
	GLboolean depth_write;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
	const auto cull = glIsEnabled(GL_CULL_FACE), blend = glIsEnabled(GL_BLEND);
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
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	for (std::size_t i = 0; i < count; ++i) {
		const auto &quad = quads[i];
		if (quad.blend == ParticleBlend::multiply) glBlendFunc(GL_DST_COLOR, GL_ZERO);
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
