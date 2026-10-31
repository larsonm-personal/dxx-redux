#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#endif
#include "lightning_renderer.h"
#include "lightning_scene.h"
#include "particle_renderer.h"
extern "C" {
#include "3d.h"
#include "ogl_init.h"
#include "internal.h"
#include "rng_shared.h"
#include "../../../d2/3d/globvars.h"
}
#undef min
#undef max
#if !defined(OGLES) || defined(ANDROID)
#include "lightning_glow_gpu.h"
#include "particle_depth.h"

namespace
{
xl_compat::LightningGlowGpu compositor;
ParticleDepth depth;
struct EffectsRandom {
	double unit() const
	{
		return double(d_rand_fx()) / 32767.0;
	}
};
std::array<float, 16> scene_matrix()
{
	std::array<float, 16> matrix{};
#ifdef ANDROID
	std::copy_n(gles3_shim_get_mvp(), 16, matrix.begin());
#else
	GLfloat projection[16], model[16];
	glGetFloatv(GL_PROJECTION_MATRIX, projection);
	glGetFloatv(GL_MODELVIEW_MATRIX, model);
	for (unsigned column = 0; column < 4; ++column)
		for (unsigned row = 0; row < 4; ++row)
			for (unsigned k = 0; k < 4; ++k) matrix[column * 4 + row] += projection[k * 4 + row] * model[column * 4 + k];
#endif
	return matrix;
}
xl_compat::LightningVertex camera_vertex(const xl_compat::LightningVertex &vertex)
{
	const xl_compat::LightningVertex delta{ vertex[0] - float(View_position.x) / 65536.f, vertex[1] - float(View_position.y) / 65536.f, vertex[2] - float(View_position.z) / 65536.f };
	const auto dot = [&](const vms_vector &axis) {
		return delta[0] * (float(axis.x) / 65536.f) + delta[1] * (float(axis.y) / 65536.f) + delta[2] * (float(axis.z) / 65536.f);
	};
	return { dot(View_matrix.rvec), dot(View_matrix.uvec), -dot(View_matrix.fvec) };
}
} // namespace
#endif

void xl_compat::discard_lightning_context(bool abandoned)
{
#if !defined(OGLES) || defined(ANDROID)
	compositor.discard(abandoned);
	depth.discard(abandoned);
#endif
}
void xl_compat::finish_lightning_batch()
{
#if !defined(OGLES) || defined(ANDROID)
	compositor.end();
#endif
}
void xl_compat::draw_lightning_scene_item(const LightningSceneItem &item, ParticleTexture &image)
{
#if defined(OGLES) && !defined(ANDROID)
	throw std::runtime_error("XL lightning requires OpenGL 3 or OpenGL ES 3");
#else
	auto core = item.frame.core;
	auto glow = item.frame.glow;
	for (auto &vertex : core.vertices) vertex = camera_vertex(vertex);
	for (auto &vertex : glow.vertices) vertex = camera_vertex(vertex);
	const auto matrix = scene_matrix();
	std::array<float, 16> projection;
#ifdef ANDROID
	std::copy_n(gles3_shim_get_projection(), 16, projection.begin());
#else
	glGetFloatv(GL_PROJECTION_MATRIX, projection.data());
#endif
	LightningGlowState restore;
	GLint target;
	std::array<int, 4> view;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &target);
	glGetIntegerv(GL_VIEWPORT, view.data());
	const auto count = (core.vertices.size() - 5) * 4;
	if (glow.vertices.size() < count) throw std::runtime_error("XL lightning viewport lacks its source glow vertices");
	std::array<int, 4> extent{ 32767, 32767, -32767, -32767 };
	for (std::size_t i = 0; i < count; ++i) {
		const auto &vertex = glow.vertices[i];
		const auto projected = lightning_screen_point({ vertex[0], vertex[1], -vertex[2] }, projection, view[2], view[3]);
		for (unsigned axis = 0; axis < 2; ++axis) {
			extent[axis] = std::min(extent[axis], projected[axis]);
			extent[axis + 2] = std::max(extent[axis + 2], projected[axis]);
		}
	}
	EffectsRandom random;
	const auto color = lightning_draw_color(item.color, true, random);
	if (glow.distance_scale == 0) {
		compositor.draw_direct(core, glow, item.random_nodes, color, item.width, matrix, extent);
		return;
	}
	// Capture the full scene target so offset canvases retain source windowScale
	if (!compositor.batch_active()) {
		glViewport(0, 0, grd_curscreen->sc_w, grd_curscreen->sc_h);
		depth.capture();
		glViewport(view[0], view[1], view[2], view[3]);
	}
	const auto corona = image.border_handle();
	compositor.begin(target, depth.texture, depth.stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, depth.width, depth.height, view);
	compositor.draw(core, glow, item.random_nodes, color, item.width, matrix, corona, extent, { .25f, .25f });
#endif
}
