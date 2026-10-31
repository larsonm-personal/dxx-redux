#include "particle_objects.h"
#include "particle_world.h"
#include "preview.h"
#include <map>
#include <SDL.h>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "game.h"
#include "rng_shared.h"
#include "dxxerror.h"
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
}

extern "C" void xl_particle_objects_clear(void)
{
	entries.clear();
	systems.clear();
	world = {};
	last_update = last_frame = 0;
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
