#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#endif
#include "oof_resource.h"
#include "oof_renderer.h"
#include "particle_renderer.h"
#include "preview.h"
#include "../model_spin.h"
#include <SDL.h>
extern "C" {
#include "3d.h"
#include "ogl_init.h"
#include "internal.h"
#include "dxxerror.h"
#include "texmap.h"
}

namespace
{
struct GpuModel {
	std::shared_ptr<const xl_compat::OofResource> owner;
	std::vector<std::shared_ptr<xl_compat::ParticleTexture>> skins;
	unsigned spin_frame = 0;
	uint32_t spin_tick = 0;
};
// Aliased slots share textures and animation phase, as they do in XL's model registry
std::map<const xl_compat::OofResource *, GpuModel> models;
std::map<const xl_compat::PogBitmap *, std::weak_ptr<xl_compat::ParticleTexture>> skin_textures;
} // namespace
extern "C" void xl_oof_renderer_clear(void)
{
	models.clear();
	skin_textures.clear();
}

extern "C" int xl_oof_draw(int id, vms_vector *position, vms_matrix *orientation, g3s_lrgb *light)
{
	const auto owner = xl_owned_oof_model(id);
	if (!owner || !xl_level_extensions_active()) return 0;
	try {
		if (!gl_initialized) throw std::runtime_error("OOF rendering requires OpenGL");
		auto &gpu = models[owner.get()];
		if (gpu.owner != owner) {
			GpuModel pending;
			pending.owner = owner;
			for (const auto &skin : owner->skins) {
				auto &cached = skin_textures[skin.get()];
				auto texture = cached.lock();
				if (!texture) {
					texture = std::make_shared<xl_compat::ParticleTexture>(*skin, owner->path.c_str(), false);
					cached = texture;
				}
				pending.skins.push_back(std::move(texture));
			}
			gpu = std::move(pending);
		}
		// Use the same view transform as native polygon geometry, including render zoom
		g3_start_instance_matrix(position, orientation);
		struct Instance {
			~Instance()
			{
				g3_done_instance();
			}
		} instance;
		GLint previous_buffer = 0, previous_texture = 0;
		glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_buffer);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
		const bool textured = glIsEnabled(GL_TEXTURE_2D) != 0, blended = glIsEnabled(GL_BLEND) != 0;
		const bool depth = glIsEnabled(GL_DEPTH_TEST) != 0;
		if (ogl_use_classic_depth()) glEnable(GL_DEPTH_TEST);
		const bool cloaked = tmap_drawer_ptr == draw_tmap_flat;
		const float fade = cloaked ? 1.0f - float(grd_curcanv->cv_fade_level) / GR_FADE_LEVELS * 0.9f : grd_curcanv->cv_fade_level >= GR_FADE_OFF ? 1.0f
		                                                                                                                                          : 1.0f - float(grd_curcanv->cv_fade_level) / (GR_FADE_LEVELS - 1);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glEnable(GL_BLEND);
		ogl_set_blending();
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		for (const auto &face : owner->mesh.faces) {
			auto vertices = face.triangles;
			const auto spin = owner->mesh.spinning_parts.find(face.submodel);
			for (auto &vertex : vertices) {
				if (spin != owner->mesh.spinning_parts.end()) xl_compat::spin_model_vertex(vertex, spin->second, gpu.spin_frame);
				const vms_vector local{ fix(vertex.position[0] * 65536), fix(vertex.position[1] * 65536), fix(vertex.position[2] * 65536) };
				g3s_point view{};
				g3_rotate_point(&view, &local);
				vertex.position = { view.p3_vec.x / 65536.0f, view.p3_vec.y / 65536.0f, -view.p3_vec.z / 65536.0f };
				vertex.color[0] *= light->r / 65536.0f;
				vertex.color[1] *= light->g / 65536.0f;
				vertex.color[2] *= light->b / 65536.0f;
				vertex.color[3] *= fade;
				if (cloaked) vertex.color[0] = vertex.color[1] = vertex.color[2] = 0;
			}
			if (face.texture >= 0 && !cloaked) {
				glEnable(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, gpu.skins.at(face.texture)->handle());
			} else glDisable(GL_TEXTURE_2D);
			glVertexPointer(3, GL_FLOAT, sizeof(xl_compat::OofRenderVertex), vertices[0].position.data());
			glTexCoordPointer(2, GL_FLOAT, sizeof(xl_compat::OofRenderVertex), vertices[0].uv.data());
			glColorPointer(4, GL_FLOAT, sizeof(xl_compat::OofRenderVertex), vertices[0].color.data());
			glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));
		}
		const uint32_t tick = SDL_GetTicks();
		if (!owner->mesh.spinning_parts.empty() && uint32_t(tick - gpu.spin_tick) > 1) {
			gpu.spin_tick = tick;
			gpu.spin_frame = (gpu.spin_frame + 1) % 32;
		}
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, previous_buffer);
		glBindTexture(GL_TEXTURE_2D, previous_texture);
		if (!textured) glDisable(GL_TEXTURE_2D);
		else glEnable(GL_TEXTURE_2D);
		if (!blended) glDisable(GL_BLEND);
		if (!depth) glDisable(GL_DEPTH_TEST);
		return 1;
	} catch (const std::exception &error) {
		Error("Cannot render owned OOF model %d: %s", id, error.what());
		return 1;
	}
}
