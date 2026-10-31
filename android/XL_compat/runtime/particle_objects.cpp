#include "particle_objects.h"
#include "particle_world.h"
#include "particle_resources.h"
#include "particle_renderer.h"
#include "resource_validation.h"
#include "../particle_order.h"
#include "../particle_visibility.h"
#include "preview.h"
#include <map>
#include <SDL.h>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "game.h"
#include "rng_shared.h"
#include "dxxerror.h"
#include "../../../d2/3d/globvars.h"
}
#undef min
#undef max

namespace
{
struct Entry {
	xl_compat::ParticleEffect effect;
	xl_compat::ParticleEmitterDefinition definition;
	std::optional<xl_compat::ParticleFace> face;
	int signature;
	bool alive = true;
	xl_compat::ParticleSystem *system = nullptr;
};
std::map<int, Entry> entries;
std::list<xl_compat::ParticleSystem> systems;
xl_compat::ParticleWorld world;
Uint32 epoch = 0;
int last_update = 0, last_frame = 0;
xl_compat::ParticleAnimationClock animation_clock;
struct EffectsRandom {
	int bounded(int limit) const
	{
		return d_rand_fx() % limit;
	}
	float unit() const
	{
		return float(d_rand_fx()) / 32767.0f;
	}
};
bool native_particle(int index)
{
	return index >= 0 && index <= Highest_object_index && Objects[index].type == 20 && Objects[index].id == 0 &&
	       Objects[index].control_type == CT_NONE && Objects[index].movement_type == MT_NONE && Objects[index].render_type == RT_NONE;
}
bool bound(int index, const Entry &entry)
{
	return entry.alive && native_particle(index) && Objects[index].signature == entry.signature;
}
void stop(Entry &entry)
{
	if (!entry.system) return;
	entry.system->emitter.set_enabled(false);
	entry.system = nullptr;
}
} // namespace

void xl_compat::bind_particle_objects(const Inventory &level)
{
	std::map<int, Entry> prepared;
	for (unsigned index = 0; index < level.objects.size(); ++index) {
		const auto &object = level.objects[index];
		if (object.render != 11) continue;
		if (object.type != 20 || object.reader_id != 0 || object.reader_render != 11 || object.control || object.movement || !native_particle(int(index)))
			throw std::runtime_error("XL particle object identity changed during native loading");
		float area = 1;
		std::optional<ParticleFace> face;
		if (object.render_data.size() == 13 && object.render_data[10] > 0) {
			area = particle_face_size(particle_face(level, object.segment, object.render_data[10] - 1, true));
			face = particle_face(level, object.segment, object.render_data[10] - 1, false);
		}
		auto effect = decode_particle_effect(object, level.level_version, area);
		if (effect.kind != ParticleKind::fire) throw std::runtime_error("XL particle runtime requires authored fire support");
		if (object.segment < 0 || std::size_t(object.segment) >= level.segments.size() || Objects[index].segnum != object.segment)
			throw std::runtime_error("XL particle object references an invalid loaded segment");
		auto definition = particle_emitter_definition(effect, { object.orientation[6], object.orientation[7], object.orientation[8] }, true);
		prepared.emplace(int(index), Entry{ effect, definition, face, Objects[index].signature });
	}
	// Ordinary levels allocate no particle world and consume no effects RNG
	auto geometry = prepared.empty() ? ParticleWorld{} : compile_particle_world(level);
	systems.clear();
	entries = std::move(prepared);
	world = std::move(geometry);
	epoch = SDL_GetTicks();
	last_update = last_frame = 0;
	animation_clock = {};
}

extern "C" void xl_particle_objects_clear(void)
{
	entries.clear();
	systems.clear();
	world = {};
	last_update = last_frame = 0;
	animation_clock = {};
}

extern "C" int xl_particle_object_set_enabled(int index, int enabled)
{
	if (!xl_level_extensions_active()) return 0;
	const auto found = entries.find(index);
	if (found == entries.end() || !bound(index, found->second)) return 0;
	found->second.effect.enabled = enabled != 0;
	return 1;
}

const std::list<xl_compat::ParticleSystem> &xl_compat::particle_systems()
{
	return systems;
}

std::vector<xl_compat::ParticleQuad> xl_compat::particle_scene(const std::vector<int> &visible, const ParticlePoint &eye, const ParticleBasis &basis, int milliseconds, unsigned image_width, unsigned image_height)
{
	std::vector<ParticleQuad> quads;
	if (!xl_level_extensions_active() || systems.empty()) return quads;
	std::vector<bool> visible_flags(world.segments.size(), false);
	const int maximum_depth = particle_depth_limit(world.segments, visible, eye);
	for (int index : visible) {
		visible_flags[index] = true;
	}
	std::vector<FireParticle *> particles;
	std::vector<ParticlePoint> positions;
	for (auto &system : systems) {
		auto &emitter = system.emitter;
		const bool emitter_visible = particle_emitter_visible(emitter.segment, emitter.skybox, visible_flags);
		for (int i = 0; i < emitter.count; ++i) {
			auto &particle = emitter.slots[std::size_t((emitter.first + i) % int(emitter.slots.size()))];
			const bool shown = emitter_visible || particle_visible(particle, world.segments, visible_flags, [](const ParticlePoint &point, bool skybox, int segment) { return world.locate(point, skybox, segment); });
			if (!shown) continue;
			particles.push_back(&particle);
			positions.push_back(particle.position);
		}
	}
	const auto order = particle_draw_order(positions, eye, maximum_depth);
	const auto camera = particle_view(basis);
	const bool animate = animation_clock.begin(milliseconds);
	quads.reserve(order.size());
	for (std::size_t index : order)
		if (prepare_fire_particle_draw(*particles[index], animate)) quads.push_back(fire_particle_quad(*particles[index], camera, image_width, image_height));
	return quads;
}

extern "C" void xl_particle_render_scene(const short *segments, int count)
{
	if (!xl_level_extensions_active() || systems.empty()) return;
	try {
		const auto *image = xl_prepared_particle_image();
		if (!image || !image->texture) throw std::runtime_error("XL particle renderer has no prepared image");
		const Uint32 elapsed = SDL_GetTicks() - epoch;
		if (elapsed > INT32_MAX) throw std::runtime_error("XL particle render session exceeds the source clock range");
		const auto point = [](const vms_vector &v) { return xl_compat::ParticlePoint{ v.x, v.y, v.z }; };
		std::vector<int> visible;
		for (int i = 0; i < count; ++i)
			if (segments[i] >= 0) visible.push_back(segments[i]);
		const auto eye = point(View_position);
		auto quads = xl_compat::particle_scene(visible, eye, { point(Unscaled_matrix.rvec), point(Unscaled_matrix.uvec) }, int(elapsed), image->bitmap.width, image->bitmap.height);
		const std::array<xl_compat::ParticlePoint, 3> view{ point(View_matrix.rvec), point(View_matrix.uvec), point(View_matrix.fvec) };
		for (auto &quad : quads) xl_compat::particle_camera_quad(quad, eye, view);
		image->texture->draw(quads.data(), quads.size());
	} catch (const std::exception &error) {
		Error("XL particle scene: %s", error.what());
	}
}

void xl_compat::particle_objects_frame(int time)
{
	if (!xl_level_extensions_active() || entries.empty() || game_is_time_paused()) return;
	if (time < last_frame) throw std::runtime_error("XL particle owner clock moved backwards");
	last_frame = time;
	EffectsRandom random;
	// Pinned ObjectParticleFrame and StaticParticlesFrame both visit authored
	// effects. Face emitters are unchanged; point emitters jitter on each visit
	for (unsigned pass = 0; pass < 2; ++pass)
		for (auto &[index, entry] : entries) {
			if (!bound(index, entry)) {
				entry.alive = false;
				stop(entry);
				continue;
			}
			if (!entry.effect.enabled) {
				stop(entry);
				continue;
			}
			const auto &object = Objects[index];
			if (!entry.system && entry.definition.count && systems.size() < particle_system_limit) {
				const int segment = object.segnum;
				if (segment < 0 || std::size_t(segment) >= world.segments.size()) throw std::runtime_error("XL particle owner moved outside its loaded mine");
				systems.push_front({ index, entry.signature, FireEmitter(entry.definition, { object.pos.x, object.pos.y, object.pos.z }, entry.face ? &*entry.face : nullptr, segment, world.segments[segment].skybox, time) });
				entry.system = &systems.front();
			}
			if (entry.system && !entry.face) {
				entry.system->emitter.position = particle_emitter_position(entry.effect, { object.pos.x, object.pos.y, object.pos.z }, random);
				entry.system->emitter.segment = object.segnum;
			}
		}
	// Source's 40-Hz gate drops elapsed residue, rather than catching up in steps
	if (time - last_update < 25) return;
	last_update = time;
	for (auto it = systems.begin(); it != systems.end();) {
		if (it->emitter.drained()) {
			// An old draining system must not unbind a newer replacement
			auto &entry = entries.at(it->object);
			if (entry.system == &*it) entry.system = nullptr;
			it = systems.erase(it);
		} else {
			it->emitter.update(time, random, world, true);
			++it;
		}
	}
}

extern "C" void xl_particle_objects_frame(void)
{
	if (!xl_level_extensions_active() || entries.empty() || game_is_time_paused()) return;
	try {
		const Uint32 elapsed = SDL_GetTicks() - epoch;
		if (elapsed > INT32_MAX) throw std::runtime_error("XL particle session exceeds the source clock range");
		xl_compat::particle_objects_frame(int(elapsed));
	} catch (const std::exception &error) {
		Error("XL particle frame: %s", error.what());
	}
}
