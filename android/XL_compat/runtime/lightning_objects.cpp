#include "lightning_objects.h"
#include "preview.h"
#include "particle_world.h"
#include "linked_audio.h"
#include "shared_audio_internal.h"
#include "slow_motion_runtime.h"
#include "../lightning_audio.h"
#include "../lightning_admission.h"
#include "../particle_admission.h"
#include "../lightning_visibility.h"
#include "../transparency_order.h"
#include <map>
#include <optional>
#include <nlohmann/json.hpp>
#include <SDL.h>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "game.h"
#include "rng_shared.h"
#include "dxxerror.h"
#include "piggy.h"
#include "../../../d2/3d/globvars.h"
}
#undef min
#undef max

namespace
{
struct Entry {
	xl_compat::LightningDefinition definition;
	int signature;
	int authored_id = 0, target_id = 0, authored_length = 0;
	bool alive = true;
	std::optional<xl_compat::LightningEmitter> emitter;
	std::vector<std::optional<xl_compat::LightningRenderFrame>> render_frames;
	int sound = 0;
	bool restored_audio = false;
	xl_compat::LightningWaypointState waypoint{};
	xl_compat::LightningPoint route_position{};
};
std::map<int, Entry> entries;
xl_compat::ParticleWorld world;
xl_compat::LightningWaypoints waypoints;
int clock_time = 0, last_tick = 0, tick_time = 0;
int last_waypoint_tick = -1;
struct EffectsRandom {
	int bounded(int limit) const
	{
		return d_rand_presentation() % limit;
	}
	double unit() const
	{
		return double(d_rand_presentation()) / 32767.0;
	}
};
bool native_effect(int index, int id)
{
	return index >= 0 && index <= Highest_object_index && Objects[index].type == 20 && Objects[index].id == id &&
	       Objects[index].control_type == CT_NONE && Objects[index].movement_type == MT_NONE && Objects[index].render_type == RT_NONE;
}
bool bound(int index, const Entry &entry)
{
	return entry.alive && native_effect(index, 1) && Objects[index].signature == entry.signature;
}
xl_compat::LightningPoint lightning_position(int index, const Entry &entry)
{
	if (entry.waypoint.node >= 0) return entry.route_position;
	const auto &position = Objects[index].pos;
	return { position.x, position.y, position.z };
}
void move_waypoint_owners()
{
	if (waypoints.owners.empty() || tick_time < 25 || last_waypoint_tick == clock_time) return;
	last_waypoint_tick = clock_time;
	const float slow_motion = xl_runtime::slow_motion_state().clocks[0].speed;
	for (const auto &[index, route] : waypoints.owners) {
		auto &entry = entries.at(index);
		auto &partner = entries.at(route.partner);
		if (bound(index, entry))
			xl_compat::move_lightning_waypoint(waypoints.nodes, entry.waypoint, bound(route.partner, partner) ? &partner.waypoint : nullptr,
			                                   entry.route_position, slow_motion);
	}
}
bool valid_waypoint_state(int object, const xl_compat::LightningWaypointState &state, const xl_compat::LightningPoint &position)
{
	if (state.node < 0 || std::size_t(state.node) >= waypoints.nodes.size()) return false;
	const auto &point = waypoints.nodes[state.node];
	if (!point.speed || point.route != waypoints.nodes[waypoints.owners.at(object).node].route) return false;
	if (state.waiting) return position == point.position;
	const auto &next = waypoints.nodes[xl_compat::waypoint_successor(point, state.reverse)];
	for (unsigned axis = 0; axis < position.size(); ++axis) {
		const auto [low, high] = std::minmax(point.position[axis], next.position[axis]);
		if (position[axis] < low || position[axis] > high) return false;
	}
	return true;
}
void create_sound(int index, const Entry &entry)
{
	if (!entry.sound) return;
	// Source retains its sound state even if channel/owner allocation fails
	xl_audio_link_external(xl_compat::lightning_sound_name, 0, index, 1, F1_0 / 2, 256 * F1_0, -1, -1, 0, 0);
	if (!entry.definition.delay && entry.definition.angle <= 0) {
		char name[] = "ff_amb_1";
		const int sample = piggy_find_sound(name);
		if (entry.restored_audio) {
			// Saved stopped loops already own their resource and must not duplicate
			using namespace xl_compat::audio_runtime;
			AudioLock lock;
			for (unsigned i = 0; i < owners.count; ++i) {
				const auto &owner = owners.entries[i];
				if ((owner.flags & xl_compat::sound_link_object) && owner.object == index && owner.signature == entry.signature && !owner.external && owner.sound == sample)
					return;
			}
		}
		if (sample != 255) xl_audio_link_object(sample, 0, index, 1, F1_0, 256 * F1_0, -1, -1, 0, 0);
	}
}
void destroy_sound(int index, const Entry &entry)
{
	using namespace xl_compat::audio_runtime;
	AudioLock lock;
	for (unsigned i = owners.count; i;) {
		const auto &owner = owners.entries[--i];
		if ((owner.flags & xl_compat::sound_link_object) && owner.object == index && owner.signature == entry.signature)
			owners.erase(i, mixer);
	}
}
} // namespace

void xl_compat::bind_lightning_objects(const Inventory &level)
{
	auto routes = prepare_lightning_waypoints(level);
	std::map<int, Entry> prepared;
	for (unsigned index = 0; index < level.objects.size(); ++index) {
		const auto &object = level.objects[index];
		if (authored_waypoint_shape(object) && (!native_effect(int(index), 3) || Objects[index].segnum != object.segment))
			throw std::runtime_error("XL waypoint controller identity changed during native loading");
		if (object.render != 12 || disabled_retyped_particle(level, object)) continue;
		if (!authored_lightning_shape(object) || !native_effect(int(index), 1))
			throw std::runtime_error("XL lightning object identity changed during native loading");
		if (object.segment < 0 || std::size_t(object.segment) >= level.segments.size() || Objects[index].segnum != object.segment)
			throw std::runtime_error("XL lightning object segment changed during native loading");
		const auto definition = prepare_authored_lightning(level, object);
		prepared.emplace(int(index), Entry{ definition, Objects[index].signature, object.render_data[7], object.render_data[8], object.render_data[2] });
	}
	for (const auto &[index, route] : routes.owners) {
		auto &entry = prepared.at(index);
		entry.waypoint.node = route.node;
		entry.route_position = routes.nodes[route.node].position;
	}
	auto geometry = prepared.empty() ? ParticleWorld{} : compile_particle_world(level);
	xl_lightning_objects_clear();
	entries = std::move(prepared);
	world = std::move(geometry);
	waypoints = std::move(routes);
}

extern "C" void xl_lightning_objects_clear(void)
{
	for (const auto &[index, entry] : entries)
		if (entry.sound > 0 || entry.restored_audio) destroy_sound(index, entry);
	entries.clear();
	world = {};
	waypoints = {};
	last_waypoint_tick = -1;
}

extern "C" void xl_lightning_objects_mute(void)
{
	for (auto &[index, entry] : entries) xl_compat::lightning_sound_mute(entry.sound);
}

void xl_compat::lightning_objects_clock(int time)
{
	if (time < clock_time) throw std::runtime_error("XL lightning clock moved backwards");
	clock_time = time;
	tick_time = time - last_tick;
	if (tick_time >= 25) last_tick = time;
}

extern "C" void xl_lightning_objects_clock(void)
{
	if (!xl_level_extensions_active() || entries.empty()) return;
	try {
		const auto now = SDL_GetTicks();
		if (now > INT32_MAX) throw std::runtime_error("XL lightning exceeds the source clock range");
		xl_compat::lightning_objects_clock(int(now));
	} catch (const std::exception &error) {
		Error("XL lightning clock: %s", error.what());
	}
}

extern "C" int xl_lightning_object_set_enabled(int index, int enabled)
{
	if (!xl_level_extensions_active()) return 0;
	const auto found = entries.find(index);
	if (found == entries.end() || !bound(index, found->second)) return 0;
	auto &entry = found->second;
	entry.definition.enabled = enabled != 0;
	if (entry.emitter) entry.emitter->enabled = entry.definition.enabled;
	return 1;
}

const xl_compat::LightningEmitter *xl_compat::lightning_object_emitter(int index)
{
	const auto found = entries.find(index);
	return found != entries.end() && bound(index, found->second) && found->second.emitter ? &*found->second.emitter : nullptr;
}

const xl_compat::LightningPoint *xl_compat::lightning_waypoint_position(int index)
{
	const auto found = entries.find(index);
	if (found == entries.end() || !bound(index, found->second) || found->second.waypoint.node < 0) return nullptr;
	return &found->second.route_position;
}

const xl_compat::LightningRenderFrame *xl_compat::lightning_object_render_frame(int index, unsigned bolt)
{
	const auto found = entries.find(index);
	if (found == entries.end() || !bound(index, found->second) || bolt >= found->second.render_frames.size()) return nullptr;
	const auto &frame = found->second.render_frames[bolt];
	return frame ? &*frame : nullptr;
}

bool xl_compat::lightning_scene_item_valid(const LightningSceneItem &item)
{
	const auto found = entries.find(item.object);
	return found != entries.end() && bound(item.object, found->second) && found->second.signature == item.signature;
}

std::vector<xl_compat::LightningSceneItem> xl_compat::lightning_scene_items(const std::vector<int> &visible, const std::vector<int> &rendered_objects, const LightningPoint &eye, int maximum_depth)
{
	std::vector<LightningSceneItem> result;
	if (!xl_level_extensions_active() || entries.empty()) return result;
	if (maximum_depth < 0) throw std::runtime_error("XL lightning scene depth is negative");
	std::vector<bool> flags(world.segments.size(), false);
	for (int segment : visible) {
		if (segment < 0 || std::size_t(segment) >= flags.size()) throw std::runtime_error("XL lightning visibility references an invalid segment");
		flags[segment] = true;
	}
	for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
		const auto &[index, entry] = *it;
		if (!bound(index, entry) || !entry.emitter || !entry.emitter->enabled) continue;
		for (unsigned ordinal = 0; ordinal < entry.emitter->bolts.size(); ++ordinal) {
			const auto &bolt = entry.emitter->bolts[ordinal];
			const auto &frame = entry.render_frames[ordinal];
			if (bolt.lifetime.nodes <= 0 || bolt.lifetime.frames < 0 || !frame) continue;
			const bool owner_rendered = std::find(rendered_objects.begin(), rendered_objects.end(), index) != rendered_objects.end();
			const bool shown = lightning_visible(owner_rendered, bolt.segment, bolt.position, bolt.end, flags, [&](const LightningPoint &position) { return world.locate(position, false, -1); });
			if (!shown) continue;
			const auto position = lightning_transparency_position(bolt.position, bolt.end, eye, maximum_depth);
			if (!position) continue;
			result.push_back({ *frame, position->position, bolt.definition.color, index, entry.signature, bolt.lifetime.random_nodes, bolt.definition.width });
		}
	}
	return result;
}

std::map<int, xl_compat::LightningSegmentLight> xl_compat::lightning_object_lights()
{
	LightningLightAccumulator accumulator;
	if (!xl_level_extensions_active()) return {};
	for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
		const auto &[index, entry] = *it;
		if (!bound(index, entry) || !entry.emitter || !entry.emitter->enabled) continue;
		for (const auto &bolt : entry.emitter->bolts)
			lightning_bolt_lights(bolt, [&](const LightningPoint &position, int segment) { return world.locate_connected(position, segment); }, [&](int segment, const LightningPoint &position, const auto &color) { accumulator.add(segment, position, color); });
	}
	return accumulator.finish();
}

void xl_compat::lightning_objects_frame()
{
	if (!xl_level_extensions_active() || game_is_time_paused()) return;
	move_waypoint_owners();
	EffectsRandom random;
	// Source's pool visits newest first; StaticFrame creates all bound owners
	// in ascending object order after the existing emitters have updated
	for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
		const auto index = it->first;
		auto &entry = it->second;
		if (!bound(index, entry)) {
			destroy_sound(index, entry);
			entry.alive = false;
			entry.emitter.reset();
			entry.render_frames.clear();
			continue;
		}
		const auto render_setup = [&](const LightningBolt &bolt) {
			const LightningVertex eye{ float(View_position.x) / 65536.0f, float(View_position.y) / 65536.0f, float(View_position.z) / 65536.0f };
			const LightningVertex forward{ float(View_matrix.fvec.x) / 65536.0f, float(View_matrix.fvec.y) / 65536.0f, float(View_matrix.fvec.z) / 65536.0f };
			LightningRenderFrame frame{};
			frame.core = lightning_core_geometry(bolt.draw_positions, [&](const LightningVertex &vertex) {
				return (vertex[0] - eye[0]) * forward[0] + (vertex[1] - eye[1]) * forward[1] + (vertex[2] - eye[2]) * forward[2];
			});
			// Both the pinned XL default and Redux perspective use zFar=5000
			frame.glow.distance_scale = lightning_distance_scale(frame.core.average_depth, 4999.0f);
			if (bolt.definition.glow)
				frame.glow = lightning_glow_geometry(bolt.draw_positions, bolt.lifetime.random_nodes, bolt.definition.width, bolt.definition.glow > 0, eye, frame.core.average_depth, 4999.0f);
			entry.render_frames[std::size_t(&bolt - entry.emitter->bolts.data())] = std::move(frame);
		};
		if (entry.emitter && lightning_emitter_frame(*entry.emitter, clock_time, tick_time, random, render_setup)) {
			const bool active_nodes = std::any_of(entry.emitter->bolts.begin(), entry.emitter->bolts.end(), [](const auto &bolt) { return bolt.lifetime.nodes > 0; });
			const auto sound_action = lightning_sound_update(entry.sound, entry.emitter->enabled, active_nodes);
			if (sound_action == LightningSoundAction::create) create_sound(index, entry);
			else if (sound_action == LightningSoundAction::destroy) destroy_sound(index, entry);
			const auto &object = Objects[index];
			lightning_emitter_move(*entry.emitter, lightning_position(index, entry), object.segnum);
		}
	}
	for (auto &[index, entry] : entries) {
		if (!bound(index, entry) || entry.emitter || entry.definition.bolts <= 0) continue;
		const auto &object = Objects[index];
		std::optional<LightningPoint> endpoint;
		if (!entry.definition.random_end && entry.target_id) {
			for (const auto &[target, candidate] : entries) {
				if (target == index || candidate.authored_id != entry.target_id || !bound(target, candidate)) continue;
				endpoint = lightning_position(target, candidate);
				break;
			}
		}
		const auto position = lightning_position(index, entry);
		endpoint = place_authored_lightning(entry.definition, position,
		                                    { object.orient.fvec.x, object.orient.fvec.y, object.orient.fvec.z }, entry.authored_length, endpoint);
		entry.emitter = create_lightning_emitter(entry.definition, position, random, object.segnum, endpoint);
		entry.render_frames.resize(entry.emitter->bolts.size());
		entry.sound = entry.definition.sound;
		create_sound(index, entry);
		entry.restored_audio = false;
	}
}

extern "C" void xl_lightning_objects_frame(void)
{
	try {
		xl_compat::lightning_objects_frame();
	} catch (const std::exception &error) {
		Error("XL lightning frame: %s", error.what());
	}
}

nlohmann::json xl_compat::lightning_objects_snapshot()
{
	auto state = nlohmann::json::array();
	for (const auto &[index, entry] : entries) {
		state.push_back({ index, entry.signature, entry.definition.enabled, bound(index, entry) });
		if (entry.waypoint.node >= 0)
			state.back().push_back({ entry.waypoint.node, entry.waypoint.reverse, entry.waypoint.waiting, entry.route_position });
	}
	return state;
}

bool xl_compat::restore_lightning_objects(const nlohmann::json &state, bool apply)
{
	try {
		if (!state.is_array() || state.size() != entries.size()) return false;
		auto prepared = entries;
		unsigned ordinal = 0;
		for (auto &[index, entry] : prepared) {
			const auto &row = state[ordinal++];
			const bool routed = entry.waypoint.node >= 0;
			if (!row.is_array() || row.size() != (routed ? 5u : 4u) || !row[0].is_number_integer() || row[0] != index || !row[1].is_number_integer() ||
			    (row[1].is_number_unsigned() && row[1].get<std::uint64_t>() > INT32_MAX) ||
			    row[1].get<std::int64_t>() < 0 || row[1].get<std::int64_t>() > INT32_MAX || !row[2].is_boolean() || !row[3].is_boolean()) return false;
			entry.signature = row[1].get<int>();
			entry.definition.enabled = row[2].get<bool>();
			entry.alive = row[3].get<bool>();
			entry.emitter.reset();
			entry.render_frames.clear();
			entry.sound = 0;
			entry.restored_audio = true;
			const bool native = native_effect(index, 1) && Objects[index].signature == entry.signature;
			if (entry.alive ? !native : native_effect(index, 1)) return false;
			if (routed) {
				const auto &phase = row[4];
				if (!phase.is_array() || phase.size() != 4 || !phase[0].is_number_integer() || phase[0] < 0 ||
				    phase[0] >= waypoints.nodes.size() || !phase[1].is_boolean() || !phase[2].is_boolean() ||
				    !phase[3].is_array() || phase[3].size() != entry.route_position.size()) return false;
				for (const auto &coordinate : phase[3])
					if (!coordinate.is_number_integer() || (coordinate.is_number_unsigned() && coordinate.get<std::uint64_t>() > INT32_MAX) ||
					    coordinate.get<std::int64_t>() < INT32_MIN || coordinate.get<std::int64_t>() > INT32_MAX) return false;
				entry.waypoint = { phase[0].get<int>(), phase[1].get<bool>(), phase[2].get<bool>() };
				entry.route_position = phase[3].get<LightningPoint>();
				if (!valid_waypoint_state(index, entry.waypoint, entry.route_position)) return false;
			}
		}
		// Source saves activation and rebuilds transient bolts after level restore
		if (apply) entries = std::move(prepared);
		return true;
	} catch (const std::exception &) {
		return false;
	}
}

bool xl_compat::lightning_audio_matches(const nlohmann::json &audio)
{
	try {
		for (const auto &owner : audio.at("linked").at("owners")) {
			const int index = owner.at("object").get<int>();
			const auto found = entries.find(index);
			if (found == entries.end() || !native_effect(index, 1) || owner.at("signature") != Objects[index].signature) continue;
			const auto &definition = found->second.definition;
			if (!definition.sound || owner.at("class") != 0 || owner.at("max_distance") != 256 * F1_0 ||
			    owner.at("loop_start") != -1 || owner.at("loop_end") != -1 || owner.at("custom").get<bool>()) return false;
			if (owner.at("external").get<bool>()) {
				if (owner.at("sample") != audio_runtime::external_sample(lightning_sound_name) || owner.at("max_volume") != F1_0 / 2) return false;
			} else {
				char name[] = "ff_amb_1";
				const int sample = piggy_find_sound(name);
				if (definition.delay || definition.angle > 0 || sample == 255 || owner.at("sound") != sample || owner.at("max_volume") != F1_0) return false;
			}
		}
		return true;
	} catch (const std::exception &) {
		return false;
	}
}
