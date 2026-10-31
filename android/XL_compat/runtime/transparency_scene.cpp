#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#endif
#include "transparency_scene.h"
#include "../transparency.h"
#include "../transparency_order.h"
#include "../particle_buffer.h"
#include "../particle_visibility.h"
#include "particle_objects.h"
#include "particle_renderer.h"
#include "lightning_objects.h"
#include "lightning_renderer.h"
#include "particle_resources.h"
#include "resource_validation.h"
#include "geometry.h"
#include "preview.h"
#include <map>
#include <set>
#include <variant>
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "game.h"
#include "bm.h"
#include "piggy.h"
#include "textures.h"
#include "wall.h"
#include "playsave.h"
#include "effects.h"
#include "render.h"
#include "texmap.h"
#include "ogl_init.h"
#include "dxxerror.h"
#include "../../../d2/3d/globvars.h"
#ifdef ANDROID
#include "merged_wall_debug.h"
#endif
void render_face(int, int, int, int *, int, int, uvl *, int);
}
#undef min
#undef max

#ifdef ANDROID
#include "particle_scene_probe.h"
#endif

namespace
{
using namespace xl_compat;
struct Window {
	int left = Window_clip_left, top = Window_clip_top, right = Window_clip_right, bottom = Window_clip_bot;
	int depth = Current_seg_depth, linear = Max_linear_depth;
	void restore() const
	{
		Window_clip_left = left;
		Window_clip_top = top;
		Window_clip_right = right;
		Window_clip_bot = bottom;
		Current_seg_depth = depth;
		Max_linear_depth = linear;
	}
};
struct Face {
	int segment, side, count, base, overlay, doorway;
	std::array<int, 4> vertices{};
	std::array<uvl, 4> uvls{};
	Window window;
};
struct QueuedObject {
	int index, signature;
	Window window;
};
struct Particle {
	ParticleQuad quad;
	ParticleTexture *texture;
	ParticlePoint position{};
	float radius = 0;
};
struct Water {
	int32_t points[4][3];
	float colors[4][4];
};
using Item = std::variant<Face, QueuedObject, Particle, Water, LightningSceneItem>;
struct Bounds {
	ParticlePoint center;
	int radius = 0;
};
std::map<int, Bounds> bounds;
std::vector<Item> items;
std::vector<TransparencyPosition> positions;
std::vector<short> visible;
std::set<std::array<int, 7>> faces;
std::set<int> rendered_objects;
ParticlePoint eye;
int maximum_depth = 0;
bool collecting = false;
ParticlePoint point(const vms_vector &v)
{
	return { v.x, v.y, v.z };
}
void reset_frame()
{
	collecting = false;
	items.clear();
	positions.clear();
	visible.clear();
	faces.clear();
	rendered_objects.clear();
}
void add(Item item, const ParticlePoint &position, int bias = 0, bool clamp = false)
{
	items.push_back(std::move(item));
	positions.push_back({ position, bias, clamp });
}
bool transparent_object(const object &obj)
{
	switch (obj.render_type) {
		case RT_FIREBALL:
		case RT_WEAPON_VCLIP:
		case RT_HOSTAGE:
		case RT_POWERUP:
		case RT_LASER: return true;
		case RT_POLYOBJ:
			return (obj.type == OBJ_WEAPON && transparency_energy_projectile(obj.id)) ||
			       (obj.type == OBJ_PLAYER && obj.id < MAX_PLAYERS && (Players[obj.id].flags & PLAYER_FLAGS_CLOAKED)) ||
			       (obj.type == OBJ_ROBOT && obj.ctype.ai_info.CLOAKED);
		default: return false;
	}
}
template <class Draw, class Finish>
void drain(Draw draw, Finish finish)
{
	collecting = false;
	struct Restore {
		Window window;
		~Restore()
		{
			window.restore();
			reset_frame();
		}
	} restore;
	for (auto index : transparency_draw_order(positions, eye, maximum_depth)) {
		auto &item = items[index];
		if (const auto *face = std::get_if<Face>(&item)) face->window.restore();
		else if (const auto *object = std::get_if<QueuedObject>(&item)) {
			if (object->index > Highest_object_index || Objects[object->index].signature != object->signature || Objects[object->index].type == OBJ_NONE) continue;
			object->window.restore();
		} else if (const auto *lightning = std::get_if<LightningSceneItem>(&item)) {
			if (!lightning_scene_item_valid(*lightning)) continue;
		}
		draw(item);
	}
	finish();
}
template <class Draw>
void drain(Draw draw)
{
	drain(draw, [] {});
}
template <class DrawParticles, class DrawOther, class Before>
void drain_buffered(DrawParticles draw_particles, DrawOther draw_other, Before before)
{
	FireParticleBuffers buffers({ float(eye[0]) / 65536.f, float(eye[1]) / 65536.f, float(eye[2]) / 65536.f });
	const auto submit = [&](const std::vector<std::size_t> &indices) {
		std::vector<ParticleQuad> quads;
		ParticleTexture *texture = nullptr;
		for (auto index : indices) {
			const auto &particle = std::get<Particle>(items[index]);
			if (texture != particle.texture && !quads.empty()) {
				draw_particles(texture, quads);
				quads.clear();
			}
			texture = particle.texture;
			quads.push_back(particle.quad);
		}
		if (!quads.empty()) draw_particles(texture, quads);
	};
	drain([&](Item &item) {
		before(item);
		if (const auto *particle = std::get_if<Particle>(&item)) {
			const auto &p = particle->position;
			const bool smoke = particle->quad.blend == ParticleBlend::multiply;
			const bool alpha = particle->quad.blend == ParticleBlend::alpha;
			buffers.add_kind(std::size_t(&item - items.data()), { float(p[0]) / 65536.f, float(p[1]) / 65536.f, float(p[2]) / 65536.f }, particle->radius, alpha ? 1 : smoke ? 16 : 5, !smoke && !alpha, submit);
		} else {
			buffers.flush(submit);
			draw_other(item);
		} }, [&] { buffers.flush(submit); });
}
template <class DrawParticles, class DrawOther>
void drain_buffered(DrawParticles draw_particles, DrawOther draw_other)
{
	drain_buffered(draw_particles, draw_other, [](Item &) {});
}
} // namespace

extern "C" void xl_transparency_clear(void)
{
	reset_frame();
	bounds.clear();
}

extern "C" void xl_transparency_begin(const short *segments, int count)
{
	reset_frame();
	if (!xl_level_extensions_active() || (Game_mode & GM_MULTI)) return;
	try {
		if (count < 0 || (count && !segments)) throw std::runtime_error("Invalid XL transparency visibility list");
		eye = point(View_position);
		maximum_depth = 0;
		for (int i = 0; i < count; ++i) {
			const int segment = segments[i];
			if (segment < 0) continue;
			if (segment > Highest_segment_index) throw std::runtime_error("Invalid XL transparency segment");
			visible.push_back(short(segment));
			auto found = bounds.find(segment);
			if (found == bounds.end()) {
				Bounds compiled{ compile_classic_lighting_side(segment, 0).segment_center };
				for (int vertex : Segments[segment].verts)
					compiled.radius = std::max(compiled.radius, geometry_fixed_distance(point(Vertices[vertex]), compiled.center));
				found = bounds.emplace(segment, compiled).first;
			}
			maximum_depth = std::max(maximum_depth, transparency_segment_depth(found->second.center, found->second.radius, eye));
		}
		collecting = true;
	} catch (const std::exception &error) {
		xl_transparency_clear();
		Error("XL transparency frame: %s", error.what());
	}
}

extern "C" int xl_transparency_face(int segment, int side, int count, const int *vertices, int base, int overlay, const uvl *uvls, int doorway)
{
	if (!collecting) return 0;
	try {
		if (segment < 0 || segment > Highest_segment_index || side < 0 || side >= 6 || count < 3 || count > 4)
			throw std::runtime_error("Invalid XL transparency face");
		// Let the native renderer repair invalid texture indices before drawing
		if (base < 0 || base >= NumTextures || (overlay & 0x3fff) >= NumTextures) return 0;
		// Ask the same material consumer used at draw time, including XL's fade scale
		const bool flat = xl_preview_begin_cloak_face(segment, side) || (doorway & WID_CLOAKED_FLAG);
		if (!flat) xl_preview_begin_face(segment, side);
		const float alpha = xl_preview_alpha(1);
		xl_preview_end_face();
		const bool faded = alpha < 1;
		const bool additive = PlayerCfg.AlphaEffects && (TmapInfo[base].eclip_num == ECLIP_NUM_FUELCEN || TmapInfo[base].eclip_num == ECLIP_NUM_FORCE_FIELD);
		PIGGY_PAGE_IN(Textures[base]);
		std::optional<unsigned> top;
		if (overlay & 0x3fff) {
			PIGGY_PAGE_IN(Textures[overlay & 0x3fff]);
			top = GameBitmaps[Textures[overlay & 0x3fff].index].bm_flags;
		}
		if (!transparent_face(flat ? std::nullopt : std::optional<unsigned>(GameBitmaps[Textures[base].index].bm_flags), top, flat || faded, additive, alpha)) return 0;
#ifdef ANDROID
		android_merged_wall_clear_draw_face_context();
#endif
		std::array<int, 7> key{ segment, side, count };
		for (int i = 0; i < count; ++i) key[3 + i] = vertices[i];
		// The native renderer visits transparent faces in two geometry passes
		if (!faces.insert(key).second) return 1;
		Face face{ segment, side, count, base, overlay, doorway };
		for (int i = 0; i < count; ++i) {
			face.vertices[i] = vertices[i];
			face.uvls[i] = uvls[i];
		}
		add(face, compile_classic_lighting_side(segment, side).center, 0, true);
		return 1;
	} catch (const std::exception &error) {
		Error("XL transparency face: %s", error.what());
		return 0;
	}
}

extern "C" int xl_transparency_object(int index)
{
	if (!collecting || index < 0 || index > Highest_object_index) return 0;
	try {
		rendered_objects.insert(index);
		if (!transparent_object(Objects[index])) return 0;
		const auto &obj = Objects[index];
		add(QueuedObject{ index, obj.signature }, point(obj.pos));
		return 1;
	} catch (const std::exception &error) {
		Error("XL transparency object: %s", error.what());
		return 0;
	}
}

void xl_compat::transparency_particle(const ParticleSceneItem &item, ParticleTexture *texture)
{
	if (!collecting || !texture) throw std::runtime_error("XL particle submitted outside its transparency frame");
	add(Particle{ item.quad, texture, item.position, item.radius }, item.position, 10 * 65536);
}

extern "C" int xl_transparency_water(int segment, int side, const int32_t points[4][3], const float colors[4][4])
{
	if (!collecting) return 0;
	try {
		Water water{};
		for (int i = 0; i < 4; ++i) {
			std::copy_n(points[i], 3, water.points[i]);
			std::copy_n(colors[i], 4, water.colors[i]);
		}
		add(water, compile_classic_lighting_side(segment, side).center, 0, true);
		return 1;
	} catch (const std::exception &error) {
		Error("XL transparency water: %s", error.what());
		return 0;
	}
}

extern "C" void xl_transparency_flush(void)
{
	if (!collecting) return;
	try {
		xl_particle_render_scene(visible.data(), int(visible.size()));
		for (auto &lightning : lightning_scene_items(std::vector<int>(visible.begin(), visible.end()), std::vector<int>(rendered_objects.begin(), rendered_objects.end()), eye, maximum_depth)) {
			const auto position = lightning.position;
			add(std::move(lightning), position, 0, true);
		}
		struct ParticleSoftScope {
			ParticleSoftScope()
			{
				xl_particle_soft_begin_scene();
			}
			~ParticleSoftScope()
			{
				xl_particle_soft_end_scene();
			}
		} soft;
		struct DepthState {
			GLboolean write, enabled;
			GLint function;
			DepthState()
			{
				glGetBooleanv(GL_DEPTH_WRITEMASK, &write);
				glGetIntegerv(GL_DEPTH_FUNC, &function);
				enabled = glIsEnabled(GL_DEPTH_TEST);
			}
			~DepthState()
			{
				glDepthMask(write);
				glDepthFunc(function);
				if (!enabled) glDisable(GL_DEPTH_TEST);
			}
		} depth;
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);
		drain_buffered([&](ParticleTexture *texture, const std::vector<ParticleQuad> &batch) {
#ifdef ANDROID
			particle_probe_before(batch);
#endif
			texture->draw(batch.data(), batch.size());
#ifdef ANDROID
			particle_probe_after(unsigned(batch.size()));
#endif
		},
		               [&](Item &item) {
			               if (auto *face = std::get_if<Face>(&item)) {
#ifdef ANDROID
				               auto &segment = Segments[face->segment];
				               const int corner = segment.sides[face->side].type == SIDE_IS_TRI_02 ? 1 : 0;
				               const int triangle = face->count == 3 && face->vertices[corner] != segment.verts[Side_to_verts_int[face->side][corner]];
				               android_merged_wall_set_draw_face_context(&segment, face->side, face->base, face->overlay, face->doorway, face->count, triangle);
#endif
				               render_face(face->segment, face->side, face->count, face->vertices.data(), face->base, face->overlay, face->uvls.data(), face->doorway);
			               } else if (const auto *object = std::get_if<QueuedObject>(&item)) {
				               render_object(&Objects[object->index]);
			               } else if (auto *water = std::get_if<Water>(&item)) {
				               ogl_draw_xl_water_portal(water->points, water->colors);
			               } else if (const auto *lightning = std::get_if<LightningSceneItem>(&item)) {
				               const auto *image = xl_prepared_lightning_image();
				               if (!image || !image->texture) throw std::runtime_error("XL lightning scene lost its corona image");
				               draw_lightning_scene_item(*lightning, *image->texture);
			               }
		               },
		               [](Item &item) {
			               // Native objects either end glow or begin an incompatible sprite batch
			               // Flush at particle visitation, before its delayed buffer submission
			               if (!std::holds_alternative<LightningSceneItem>(item)) finish_lightning_batch();
		               });
		finish_lightning_batch();
	} catch (const std::exception &error) {
		discard_lightning_context(false);
		xl_transparency_clear();
		Error("XL transparency rendering: %s", error.what());
	}
}

#ifdef DXX_XL_DESCRIPTOR_TEST
#include "transparency_scene_test.h"
#endif
