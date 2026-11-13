#include "particle_objects.h"
#include "particle_state.h"
#include "particle_world.h"
#include "particle_resources.h"
#include "particle_renderer.h"
#include "linked_audio.h"
#include "shared_audio_internal.h"
#include "../particle_admission.h"
#include "resource_validation.h"
#include "../transparency_order.h"
#include "../particle_visibility.h"
#include "../particle_buffer.h"
#include "preview.h"
#include "transparency_scene.h"
#include <map>
#include <nlohmann/json.hpp>
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
	std::visit([](auto &emitter) { emitter.set_enabled(false); }, entry.system->emitter);
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
		const auto authored = prepare_authored_particle(level, object);
		if (object.segment < 0 || std::size_t(object.segment) >= level.segments.size() || Objects[index].segnum != object.segment)
			throw std::runtime_error("XL particle object " + std::to_string(index) + " segment mismatch: authored " + std::to_string(object.segment) + ", loaded " + std::to_string(Objects[index].segnum));
		prepared.emplace(int(index), Entry{ authored.effect, authored.definition, authored.face, Objects[index].signature });
	}
	// Ordinary levels allocate no particle world and consume no effects RNG
	auto geometry = prepared.empty() ? ParticleWorld{} : compile_particle_world(level);
	xl_transparency_clear();
	systems.clear();
	entries = std::move(prepared);
	world = std::move(geometry);
	epoch = SDL_GetTicks();
	last_update = last_frame = 0;
	animation_clock = {};
}

extern "C" void xl_particle_objects_clear(void)
{
	xl_transparency_clear();
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

extern "C" int xl_particle_depth_projection(void)
{
	return xl_level_extensions_active() && !entries.empty();
}

const std::list<xl_compat::ParticleSystem> &xl_compat::particle_systems()
{
	return systems;
}

nlohmann::json xl_compat::particle_objects_snapshot()
{
	auto state = nlohmann::json::array();
	for (const auto &[index, entry] : entries)
		state.push_back({ index, entry.signature, entry.effect.enabled, bound(index, entry) });
	return state;
}

bool xl_compat::particle_audio_matches(const nlohmann::json &audio)
{
	try {
		for (const auto &owner : audio.at("linked").at("owners")) {
			const int index = owner.at("object").get<int>();
			const auto found = entries.find(index);
			if (found == entries.end() || !native_particle(index) || owner.at("signature") != Objects[index].signature) continue;
			if (found->second.effect.kind != ParticleKind::bubbles || !owner.at("external").get<bool>() ||
			    owner.at("sample") != audio_runtime::external_sample(bubble_sound_name) || owner.at("class") != 0 ||
			    owner.at("max_volume") != F1_0 / 2 || owner.at("max_distance") != 256 * F1_0 ||
			    owner.at("loop_start") != -1 || owner.at("loop_end") != -1 || owner.at("custom").get<bool>()) return false;
		}
		return true;
	} catch (const std::exception &) {
		return false;
	}
}

bool xl_compat::restore_particle_objects(const nlohmann::json &state, bool apply)
{
	try {
		if (!state.is_array() || state.size() != entries.size()) return false;
		auto prepared = entries;
		unsigned ordinal = 0;
		for (auto &[index, entry] : prepared) {
			const auto &row = state[ordinal++];
			if (!row.is_array() || row.size() != 4 || !row[0].is_number_integer() || row[0] != index || !row[1].is_number_integer() ||
			    (row[1].is_number_unsigned() && row[1].get<std::uint64_t>() > INT32_MAX) ||
			    row[1].get<std::int64_t>() < 0 || row[1].get<std::int64_t>() > INT32_MAX ||
			    !row[2].is_boolean() || !row[3].is_boolean()) return false;
			entry.signature = row[1].get<int>();
			entry.effect.enabled = row[2].get<bool>();
			entry.alive = row[3].get<bool>();
			entry.system = nullptr;
			const bool native = native_particle(index) && Objects[index].signature == entry.signature;
			if (entry.alive ? !native : native_particle(index)) return false;
		}
		if (apply) {
			// Source saves authored enable bytes, then rebuilds the level and its
			// particle manager on restore. Live clouds are transient visual state
			xl_transparency_clear();
			systems.clear();
			entries = std::move(prepared);
			epoch = SDL_GetTicks();
			last_update = last_frame = 0;
			animation_clock = {};
		}
		return true;
	} catch (const std::exception &) {
		return false;
	}
}

std::vector<xl_compat::ParticleSceneItem> xl_compat::particle_scene_items(const std::vector<int> &visible, const ParticlePoint &eye, const ParticleBasis &basis, int milliseconds, unsigned image_width, unsigned image_height, unsigned smoke_width, unsigned smoke_height, unsigned bubble_width, unsigned bubble_height)
{
	std::vector<ParticleSceneItem> quads;
	if (!xl_level_extensions_active() || systems.empty()) return quads;
	std::vector<bool> visible_flags(world.segments.size(), false);
	const int maximum_depth = particle_depth_limit(world.segments, visible, eye);
	for (int index : visible) {
		visible_flags[index] = true;
	}
	const auto camera = particle_view(basis);
	const bool animate = animation_clock.begin(milliseconds);
	for (auto &system : systems) {
		std::visit([&](auto &emitter) {
			using Emitter = std::decay_t<decltype(emitter)>;
			constexpr bool smoke = std::is_same_v<Emitter, SmokeEmitter>;
			constexpr bool waterfall = std::is_same_v<Emitter, WaterfallEmitter>;
			constexpr bool rain = std::is_same_v<Emitter, RainEmitter>;
			constexpr bool snow = std::is_same_v<Emitter, SnowEmitter>;
			const bool emitter_visible = particle_emitter_visible(emitter.segment, emitter.skybox, visible_flags);
			for (int i = 0; i < emitter.count; ++i) {
				auto &slot = emitter.slots[std::size_t((emitter.first + i) % int(emitter.slots.size()))];
				auto &particle = emitter.motion(slot);
				const bool shown = emitter_visible || particle_visible(particle, world.segments, visible_flags, [](const ParticlePoint &point, bool skybox, int segment) { return world.locate(point, skybox, segment); });
				if (!shown || !transparency_bucket({ particle.position, 10 * 65536 }, eye, maximum_depth)) continue;
				if constexpr (std::is_same_v<Emitter, BubbleEmitter>) {
					const int frame = particle.frame;
					if (prepare_bubble_particle_draw(slot, animate)) {
						ParticleSceneItem item{ particle.position, {}, float(std::hypot(double(particle.width), double(particle.height))), 2 };
						item.quad.blend = ParticleBlend::alpha;
						item.bubble = BubbleSceneDraw{ &slot, frame, bubble_width ? bubble_width : image_width, bubble_height ? bubble_height : image_height, basis };
						quads.push_back(std::move(item));
					}
				} else if constexpr (smoke) {
					if (prepare_smoke_particle_draw(slot, animate) && particle.decay > 0)
						quads.push_back({ particle.position, smoke_particle_quad(slot, camera, smoke_width ? smoke_width : image_width, smoke_height ? smoke_height : image_height), float(std::hypot(double(particle.width), double(particle.height))), 0 });
				} else if constexpr (waterfall) {
					if (prepare_waterfall_particle_draw(slot, animate))
						quads.push_back({ particle.position, waterfall_particle_quad(slot, camera, smoke_width ? smoke_width : image_width, smoke_height ? smoke_height : image_height), float(std::hypot(double(particle.width), double(particle.height))), 4 });
				} else if constexpr (rain) {
					if (prepare_weather_particle_draw(slot, animate))
						quads.push_back({ particle.position, rain_particle_quad(slot, eye, image_width, image_height), float(std::hypot(double(particle.width), double(particle.height))), 5 });
				} else if constexpr (snow) {
					if (prepare_weather_particle_draw(slot, animate)) {
						ParticleSceneItem item{ particle.position, {}, float(std::hypot(double(particle.width), double(particle.height))), 6 };
						item.quad.blend = ParticleBlend::alpha;
						item.snow = SnowSceneDraw{ &slot, image_width, image_height, basis };
						quads.push_back(std::move(item));
					}
				} else if (prepare_fire_particle_draw(particle, animate))
					quads.push_back({ particle.position, fire_particle_quad(particle, camera, image_width, image_height), float(std::hypot(double(particle.width), double(particle.height))), 3 });
			}
		},
		           system.emitter);
	}
	if (bubble_width && bubble_height)
		for (auto &item : quads)
			if (item.subtype == 0 || item.subtype == 4) item.quad.image = 1;
	return quads;
}

xl_compat::ParticleQuad xl_compat::particle_scene_quad(const ParticleSceneItem &item)
{
	if (item.snow) {
		const auto &draw = *item.snow;
		if (!draw.particle) throw std::runtime_error("XL deferred snow lost its particle");
		EffectsRandom random;
		auto quad = snow_particle_quad(*draw.particle, particle_view(draw.basis), draw.width, draw.height, random);
		if (draw.camera_space) particle_camera_quad(quad, draw.eye, draw.camera);
		return quad;
	}
	if (!item.bubble) return item.quad;
	const auto &draw = *item.bubble;
	if (!draw.particle) throw std::runtime_error("XL deferred bubble lost its particle");
	EffectsRandom random;
	// Source bubble creation disables rotation, so only the unrotated basis is
	// needed. Wiggle is applied in world X before the camera transformation
	const ParticleView view{ draw.basis, {}, false };
	auto quad = bubble_particle_quad(*draw.particle, view, draw.width, draw.height, draw.frame, random);
	quad.image = 2;
	if (draw.camera_space) particle_camera_quad(quad, draw.eye, draw.camera);
	return quad;
}

std::vector<xl_compat::ParticleQuad> xl_compat::particle_scene(const std::vector<int> &visible, const ParticlePoint &eye, const ParticleBasis &basis, int milliseconds, unsigned image_width, unsigned image_height)
{
	if (!xl_level_extensions_active() || systems.empty()) return {};
	const auto items = particle_scene_items(visible, eye, basis, milliseconds, image_width, image_height);
	std::vector<TransparencyPosition> positions;
	for (const auto &item : items) positions.push_back({ item.position, 10 * 65536 });
	std::vector<ParticleQuad> quads;
	FireParticleBuffers buffers({ float(eye[0]) / 65536.f, float(eye[1]) / 65536.f, float(eye[2]) / 65536.f });
	const auto submit = [&](const std::vector<std::size_t> &indices) {
		const bool multi = std::any_of(indices.begin(), indices.end(), [&](std::size_t index) { return items[index].subtype == 2; });
		for (auto index : indices) {
			auto quad = particle_scene_quad(items[index]);
			if (multi && (items[index].subtype == 0 || items[index].subtype == 2 || items[index].subtype == 4))
				quad.image = items[index].subtype == 2 ? 2 : 1;
			quads.push_back(std::move(quad));
		}
	};
	for (auto index : transparency_draw_order(positions, eye, particle_depth_limit(world.segments, visible, eye))) {
		const auto &item = items[index];
		const bool multiply = item.quad.blend == ParticleBlend::multiply;
		const bool alpha = item.quad.blend == ParticleBlend::alpha;
		buffers.add_kind(index, { float(item.position[0]) / 65536.f, float(item.position[1]) / 65536.f, float(item.position[2]) / 65536.f }, item.radius, item.subtype == 2 ? 2 : alpha  ? 1
		                                                                                                                                                                      : multiply ? 16
		                                                                                                                                                                                 : 5,
		                 !multiply && !alpha, submit);
	}
	buffers.flush(submit);
	return quads;
}

extern "C" void xl_particle_render_scene(const short *segments, int count)
{
	if (!xl_level_extensions_active() || systems.empty()) return;
	try {
		const auto *fire = xl_prepared_particle_image(), *smoke = xl_prepared_particle_image(0), *bubble = xl_prepared_particle_image(2);
		const auto *rain = xl_prepared_particle_image(5), *snow = xl_prepared_particle_image(6);
		const auto *image = fire ? fire : smoke ? smoke
		                              : bubble  ? bubble
		                              : rain    ? rain
		                                        : snow;
		if (!image || !image->texture) throw std::runtime_error("XL particle renderer has no prepared image");
		const Uint32 elapsed = SDL_GetTicks() - epoch;
		if (elapsed > INT32_MAX) throw std::runtime_error("XL particle render session exceeds the source clock range");
		const auto point = [](const vms_vector &v) { return xl_compat::ParticlePoint{ v.x, v.y, v.z }; };
		std::vector<int> visible;
		for (int i = 0; i < count; ++i)
			if (segments[i] >= 0) visible.push_back(segments[i]);
		const auto eye = point(View_position);
		auto items = xl_compat::particle_scene_items(visible, eye, { point(Unscaled_matrix.rvec), point(Unscaled_matrix.uvec) }, int(elapsed), image->bitmap.width, image->bitmap.height, smoke ? smoke->bitmap.width : 0, smoke ? smoke->bitmap.height : 0, bubble ? bubble->bitmap.width : 0, bubble ? bubble->bitmap.height : 0);
		const std::array<xl_compat::ParticlePoint, 3> view{ point(View_matrix.rvec), point(View_matrix.uvec), point(View_matrix.fvec) };
		for (auto &item : items) {
			const auto *selected = item.subtype == 2 ? bubble : item.subtype == 0 || item.subtype == 4 ? smoke
			                                                : item.subtype == 5                        ? rain
			                                                : item.subtype == 6                        ? snow
			                                                                                           : fire;
			if (!selected || !selected->texture) throw std::runtime_error("XL particle scene lost its subtype image");
			if (item.bubble) {
				item.bubble->eye = eye;
				item.bubble->camera = view;
				item.bubble->camera_space = true;
			} else if (item.snow) {
				item.snow->eye = eye;
				item.snow->camera = view;
				item.snow->camera_space = true;
			} else xl_compat::particle_camera_quad(item.quad, eye, view);
			xl_compat::transparency_particle(item, selected->texture.get());
		}
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
				if (entry.effect.kind == ParticleKind::bubbles) {
					BubbleEmitter emitter(entry.definition, { object.pos.x, object.pos.y, object.pos.z }, entry.face ? &*entry.face : nullptr, segment, world.segments[segment].skybox, time);
					const auto point = [](const vms_vector &v) { return ParticlePoint{ v.x, v.y, v.z }; };
					// Source Create retains all three axes; later point-jitter SetPos
					// calls pass a null orientation and leave this basis unchanged
					emitter.orientation = { point(object.orient.rvec), point(object.orient.uvec), point(object.orient.fvec) };
					systems.push_front({ index, entry.signature, std::move(emitter) });
					// Release source leaves this generic linked owner alive on disable
					xl_audio_link_external(bubble_sound_name, 0, index, 1, F1_0 / 2, 256 * F1_0, -1, -1, 0, 0);
				} else if (entry.effect.kind == ParticleKind::smoke || entry.effect.kind == ParticleKind::spray) systems.push_front({ index, entry.signature, SmokeEmitter(entry.definition, { object.pos.x, object.pos.y, object.pos.z }, entry.face ? &*entry.face : nullptr, segment, world.segments[segment].skybox, time) });
				else if (entry.effect.kind == ParticleKind::waterfall) systems.push_front({ index, entry.signature, WaterfallEmitter(entry.definition, { object.pos.x, object.pos.y, object.pos.z }, entry.face ? &*entry.face : nullptr, segment, world.segments[segment].skybox, time) });
				else if (entry.effect.kind == ParticleKind::rain || entry.effect.kind == ParticleKind::snow) {
					const auto point = [](const vms_vector &v) { return ParticlePoint{ v.x, v.y, v.z }; };
					if (entry.effect.kind == ParticleKind::rain) {
						RainEmitter emitter(entry.definition, { object.pos.x, object.pos.y, object.pos.z }, entry.face ? &*entry.face : nullptr, segment, world.segments[segment].skybox, time);
						emitter.orientation = { point(object.orient.rvec), point(object.orient.uvec), point(object.orient.fvec) };
						systems.push_front({ index, entry.signature, std::move(emitter) });
					} else {
						SnowEmitter emitter(entry.definition, { object.pos.x, object.pos.y, object.pos.z }, entry.face ? &*entry.face : nullptr, segment, world.segments[segment].skybox, time);
						emitter.orientation = { point(object.orient.rvec), point(object.orient.uvec), point(object.orient.fvec) };
						systems.push_front({ index, entry.signature, std::move(emitter) });
					}
				} else if (entry.effect.kind == ParticleKind::fire) systems.push_front({ index, entry.signature, FireEmitter(entry.definition, { object.pos.x, object.pos.y, object.pos.z }, entry.face ? &*entry.face : nullptr, segment, world.segments[segment].skybox, time) });
				else throw std::runtime_error("XL particle subtype has no native emitter behavior");
				entry.system = &systems.front();
			}
			if (entry.system && !entry.face) {
				const auto &effect = entry.effect;
				std::visit([&](auto &emitter) {
					emitter.position = particle_emitter_position(effect, { object.pos.x, object.pos.y, object.pos.z }, random);
					emitter.segment = object.segnum;
				},
				           entry.system->emitter);
			}
		}
	// Source's 40-Hz gate drops elapsed residue, rather than catching up in steps
	if (time - last_update < 25) return;
	last_update = time;
	for (auto it = systems.begin(); it != systems.end();) {
		if (std::visit([](const auto &emitter) { return emitter.drained(); }, it->emitter)) {
			// An old draining system must not unbind a newer replacement
			auto &entry = entries.at(it->object);
			if (entry.system == &*it) entry.system = nullptr;
			it = systems.erase(it);
		} else {
			try {
				std::visit([&](auto &emitter) { emitter.update(time, random, world, true); }, it->emitter);
			} catch (const std::exception &error) {
				throw std::runtime_error("object " + std::to_string(it->object) + ": " + error.what());
			}
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
