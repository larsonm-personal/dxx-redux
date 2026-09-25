#include "input_demo_world_trace.h"
#include "input_demo_state_trace.h"
#include "input_demo_ai_trace.h"
#include "input_demo_object_trace.h"

#include <string>
#include <nlohmann/json.hpp>

extern "C" {
#include "bm.h"
#include "cntrlcen.h"
#include "collide.h"
#include "effects.h"
#include "endlevel.h"
#include "endlevel_runtime.h"
#include "fuelcen.h"
#include "game.h"
#include "gameseq.h"
#include "laser.h"
#include "morph.h"
#include "player.h"
#include "secretarea.h"
#include "segment.h"
#include "automap.h"
#include "switch.h"
#include "textures.h"
#include "wall.h"
}

namespace
{
using json = nlohmann::ordered_json;

json vector(const vms_vector &v)
{
	return { v.x, v.y, v.z };
}
template <typename T, size_t N>
json scalars(const T (&values)[N])
{
	json out = json::array();
	for (const auto value : values) out.push_back(static_cast<int64_t>(value));
	return out;
}

// Copy packed engine scalars instead of binding references to unaligned storage
#define FIELD(out, value, name) (out)[#name] = static_cast<int64_t>((value).name)
#define GLOBAL(out, name)       (out)[#name] = static_cast<int64_t>(name)

json angles(const vms_angvec &v)
{
	return { v.p, v.b, v.h };
}
json matrix(const vms_matrix &v)
{
	return { vector(v.rvec), vector(v.uvec), vector(v.fvec) };
}

json endlevel_state()
{
	endlevel_runtime_state state = {};
	endlevel_get_runtime_state(&state);
	json out = json::object();
	FIELD(out, state, sequence);
	FIELD(out, state, data_loaded);
	FIELD(out, state, transition_segment);
	FIELD(out, state, exit_segment);
	FIELD(out, state, outside);
	FIELD(out, state, explosion_playing);
	FIELD(out, state, mine_destroyed);
	FIELD(out, state, movie_played);
	FIELD(out, state, current_speed);
	FIELD(out, state, desired_speed);
	out["camera"] = state.camera ? state.camera - Objects : -1;
	out["exit_point"] = vector(state.exit_point);
	out["ground_exit_point"] = vector(state.ground_exit_point);
	out["side_exit_point"] = vector(state.side_exit_point);
	out["station_position"] = vector(state.station_position);
	out["exit_orientation"] = matrix(state.exit_orientation);
	out["surface_orientation"] = matrix(state.surface_orientation);
	out["exit_angles"] = angles(state.exit_angles);
	out["player_angles"] = angles(state.player_angles);
	out["player_destination_angles"] = angles(state.player_destination_angles);
	out["camera_angles"] = angles(state.camera_angles);
	out["camera_destination_angles"] = angles(state.camera_destination_angles);
	auto &frame = out["frame"];
	FIELD(frame, state.frame, timer);
	FIELD(frame, state.frame, bank_rate);
	FIELD(frame, state.frame, explosion_wait1);
	FIELD(frame, state.frame, explosion_wait2);
	FIELD(frame, state.frame, ext_expl_halflife);
	FIELD(frame, state.frame, sound_count);
	out["fly"] = json::array();
	for (const auto &fly : state.fly) {
		json row = { { "object", fly.obj ? fly.obj - Objects : -1 },
			         { "angles", angles(fly.angles) },
			         { "step", vector(fly.step) },
			         { "angstep", vector(fly.angstep) },
			         { "headvec", vector(fly.headvec) } };
		FIELD(row, fly, speed);
		FIELD(row, fly, first_time);
		FIELD(row, fly, transition_reached);
		FIELD(row, fly, offset_frac);
		FIELD(row, fly, offset_dist);
		out["fly"].push_back(std::move(row));
	}
	// A new outside explosion overwrites the entire old object before use
	// Its active copy lives outside Objects and needs the same union observation
	out["explosion"] = json::array();
	if (state.explosion_playing)
		out["explosion"].push_back(input_demo_object_trace_snapshot(state.explosion, -1));
	return out;
}

json player_state(const player &p)
{
	json out = json::object();
	FIELD(out, p, connected);
	FIELD(out, p, objnum);
	FIELD(out, p, flags);
	FIELD(out, p, energy);
	FIELD(out, p, shields);
	FIELD(out, p, shields_delta);
	FIELD(out, p, shields_time);
	FIELD(out, p, shields_time_hours);
	FIELD(out, p, shields_certain);
	FIELD(out, p, lives);
	FIELD(out, p, level);
	FIELD(out, p, laser_level);
	FIELD(out, p, starting_level);
	FIELD(out, p, killer_objnum);
	FIELD(out, p, primary_weapon_flags);
	FIELD(out, p, secondary_weapon_flags);
	out["primary_ammo"] = scalars(p.primary_ammo);
	out["secondary_ammo"] = scalars(p.secondary_ammo);
	FIELD(out, p, primary_weapon);
	FIELD(out, p, secondary_weapon);
	FIELD(out, p, last_score);
	FIELD(out, p, score);
	FIELD(out, p, time_level);
	FIELD(out, p, time_total);
	FIELD(out, p, cloak_time);
	FIELD(out, p, invulnerable_time);
	FIELD(out, p, KillGoalCount);
	FIELD(out, p, net_killed_total);
	FIELD(out, p, net_kills_total);
	FIELD(out, p, num_kills_level);
	FIELD(out, p, num_kills_total);
	FIELD(out, p, num_robots_level);
	FIELD(out, p, num_robots_total);
	FIELD(out, p, hostages_rescued_total);
	FIELD(out, p, hostages_total);
	FIELD(out, p, hostages_on_board);
	FIELD(out, p, hostages_level);
	FIELD(out, p, homing_object_dist);
	FIELD(out, p, hours_level);
	FIELD(out, p, hours_total);
	return out;
}

json world_state()
{
	json out = json::object();
	out["ai"] = input_demo_ai_trace_snapshot();
	out["endlevel"] = endlevel_state();
	auto &globals = out["globals"];
	GLOBAL(globals, Player_num);
	GLOBAL(globals, N_players);
	GLOBAL(globals, Current_level_num);
	GLOBAL(globals, Next_level_num);
	GLOBAL(globals, Game_mode);
	GLOBAL(globals, Game_suspended);
	GLOBAL(globals, Difficulty_level);
	GLOBAL(globals, Difficulty_level_changed);
	GLOBAL(globals, Difficulty_level_min_seen);
	GLOBAL(globals, Difficulty_level_max_seen);
	GLOBAL(globals, GameTime64);
	GLOBAL(globals, FrameTime);
	GLOBAL(globals, Endlevel_sequence);
	globals["Fusion_next_sound_time"] = game_get_fusion_next_sound_time();
	globals["Fuelcen_last_sound_time"] = fuelcen_get_last_sound_time();
	globals["Collision_delay_last_play_time"] = collide_get_collision_delay_last_play_time();
	game_d_tick_state tick = {};
	game_get_d_tick_state(&tick);
	auto &clock = out["tick"];
	FIELD(clock, tick, count);
	FIELD(clock, tick, step);
	FIELD(clock, tick, timer);
	out["players"] = json::array();
	for (int i = 0; i < N_players; ++i) out["players"].push_back(player_state(Players[i]));
	auto &weapons = out["weapons"];
	GLOBAL(weapons, Next_laser_fire_time);
	GLOBAL(weapons, Last_laser_fired_time);
	GLOBAL(weapons, Next_missile_fire_time);
	GLOBAL(weapons, Next_flare_fire_time);
	GLOBAL(weapons, Auto_fire_fusion_cannon_time);
	GLOBAL(weapons, Global_laser_firing_count);
	GLOBAL(weapons, Global_missile_firing_count);
	laser_runtime_state laser = {};
	laser_get_runtime_state(&laser);
	FIELD(weapons, laser, fusion_charge);
	FIELD(weapons, laser, spreadfire_toggle);
	FIELD(weapons, laser, missile_gun);
	FIELD(weapons, laser, proximity_dropped);
	FIELD(weapons, laser, helix_orientation);
	FIELD(weapons, laser, smartmines_dropped);
	FIELD(weapons, laser, last_omega_fire_time);
	out["walls"] = json::array();
	for (int i = 0; i < Num_walls; ++i) {
		const auto &w = Walls[i];
		json row = json::object();
		FIELD(row, w, segnum);
		FIELD(row, w, sidenum);
		FIELD(row, w, hps);
		FIELD(row, w, linked_wall);
		FIELD(row, w, type);
		FIELD(row, w, flags);
		FIELD(row, w, state);
		FIELD(row, w, trigger);
		FIELD(row, w, clip_num);
		FIELD(row, w, keys);
#ifdef DXX_BUILD_DESCENT_II
		FIELD(row, w, controlling_trigger);
		FIELD(row, w, cloak_value);
#endif
		out["walls"].push_back(std::move(row));
	}
	out["doors"] = json::array();
	for (int i = 0; i < Num_open_doors; ++i) {
		const auto &d = ActiveDoors[i];
		json row = json::object();
		FIELD(row, d, n_parts);
		FIELD(row, d, time);
		row["front"] = scalars(d.front_wallnum);
		row["back"] = scalars(d.back_wallnum);
		out["doors"].push_back(std::move(row));
	}
	out["triggers"] = json::array();
	for (int i = 0; i < Num_triggers; ++i) {
		const auto &t = Triggers[i];
		json row = json::object();
		FIELD(row, t, type);
		FIELD(row, t, flags);
		FIELD(row, t, num_links);
		FIELD(row, t, value);
		FIELD(row, t, time);
#ifdef DXX_BUILD_DESCENT_II
		FIELD(row, t, pad);
		row["d1_saved"] = { { "type", static_cast<int>(t.d1_saved.type) }, { "link_num", static_cast<int>(t.d1_saved.link_num) } };
#else
		FIELD(row, t, link_num);
#endif
		row["segments"] = scalars(t.seg);
		row["sides"] = scalars(t.side);
		out["triggers"].push_back(std::move(row));
	}
	out["segments"] = json::array();
	out["automap"] = json::array();
	for (int i = 0; i <= Highest_segment_index; ++i) {
		const auto &s = Segments[i];
		json row = json::object();
		FIELD(row, s, value);
		FIELD(row, s, special);
		FIELD(row, s, matcen_num);
		FIELD(row, s, static_light);
		row["sides"] = json::array();
		for (const auto &side : s.sides) {
			json face = { { "wall", side.wall_num }, { "texture", side.tmap_num }, { "overlay", side.tmap_num2 }, { "uvls", json::array() } };
			for (const auto &uv : side.uvls) face["uvls"].push_back({ uv.u, uv.v, uv.l });
			row["sides"].push_back(std::move(face));
		}
		out["segments"].push_back(std::move(row));
		out["automap"].push_back(static_cast<int>(Automap_visited[i]));
	}
	auto &reactor = out["reactor"];
	GLOBAL(reactor, Control_center_destroyed);
	GLOBAL(reactor, Countdown_timer);
	GLOBAL(reactor, Countdown_seconds_left);
	GLOBAL(reactor, Total_countdown_time);
	GLOBAL(reactor, Control_center_been_hit);
	GLOBAL(reactor, Control_center_player_been_seen);
	GLOBAL(reactor, Control_center_next_fire_time);
	GLOBAL(reactor, Control_center_present);
	GLOBAL(reactor, Dead_controlcen_object_num);
	GLOBAL(reactor, controlcen_death_silence);
	GLOBAL(reactor, Reactor_countdown_paused);
	FIELD(reactor, ControlCenterTriggers, num_links);
	reactor["segments"] = scalars(ControlCenterTriggers.seg);
	reactor["sides"] = scalars(ControlCenterTriggers.side);
	out["fuelcenters"] = json::array();
	for (int i = 0; i < Num_fuelcenters; ++i) {
		const auto &s = Station[i];
		json row = json::object();
		FIELD(row, s, Type);
		FIELD(row, s, segnum);
		FIELD(row, s, Flag);
		FIELD(row, s, Enabled);
		FIELD(row, s, Lives);
		FIELD(row, s, Capacity);
		FIELD(row, s, MaxCapacity);
		FIELD(row, s, Timer);
		FIELD(row, s, Disable_time);
		row["center"] = vector(s.Center);
		out["fuelcenters"].push_back(std::move(row));
	}
	out["robotcenters"] = json::array();
	for (int i = 0; i < Num_robot_centers; ++i) {
		const auto &r = RobotCenters[i];
		json row = json::object();
		FIELD(row, r, hit_points);
		FIELD(row, r, interval);
		FIELD(row, r, segnum);
		FIELD(row, r, fuelcen_num);
		row["robot_flags"] = scalars(r.robot_flags);
		out["robotcenters"].push_back(std::move(row));
	}
	out["effects"] = json::array();
	for (int i = 0; i < Num_effects; ++i) {
		const auto &e = Effects[i];
		json row = json::object();
		FIELD(row, e, time_left);
		FIELD(row, e, frame_count);
		FIELD(row, e, flags);
		FIELD(row, e, segnum);
		FIELD(row, e, sidenum);
		FIELD(row, e, dest_bm_num);
		row["wall_bitmap"] = e.changing_wall_texture >= 0 ? Textures[e.changing_wall_texture].index : -1;
		row["object_bitmap"] = e.changing_object_texture >= 0 ? ObjBitmaps[e.changing_object_texture].index : -1;
		out["effects"].push_back(std::move(row));
	}
	out["stuck"] = json::object();
	out["stuck"]["count"] = Num_stuck_objects;
	out["stuck"]["slots"] = json::object();
	for (int i = 0; i < MAX_STUCK_OBJECTS; ++i)
		if (Stuck_objects[i].wallnum != -1) out["stuck"]["slots"][std::to_string(i)] = { Stuck_objects[i].objnum, Stuck_objects[i].wallnum, Stuck_objects[i].signature };
	out["morphs"] = json::object();
	for (int i = 0; i < MAX_MORPH_OBJECTS; ++i) {
		const auto &m = morph_objects[i];
		if (!m.obj || m.obj->signature != m.Morph_sig || m.obj->type == OBJ_NONE) continue;
		json row = { { "object", m.obj - Objects }, { "points", json::array() } };
		FIELD(row, m, Morph_sig);
		FIELD(row, m, n_submodels_active);
		FIELD(row, m, morph_save_control_type);
		FIELD(row, m, morph_save_movement_type);
		row["submodel_active"] = scalars(m.submodel_active);
		row["n_morphing_points"] = scalars(m.n_morphing_points);
		row["submodel_startpoints"] = scalars(m.submodel_startpoints);
		const auto &p = m.morph_save_phys_info;
		auto &physics = row["physics"];
		FIELD(physics, p, mass);
		FIELD(physics, p, drag);
		FIELD(physics, p, brakes);
		FIELD(physics, p, turnroll);
		FIELD(physics, p, flags);
		physics["velocity"] = vector(p.velocity);
		physics["thrust"] = vector(p.thrust);
		physics["rotvel"] = vector(p.rotvel);
		physics["rotthrust"] = vector(p.rotthrust);
		for (int j = 0; j < MAX_VECS; ++j) row["points"].push_back({ vector(m.morph_vecs[j]), vector(m.morph_deltas[j]), m.morph_times[j] });
		out["morphs"][std::to_string(i)] = std::move(row);
	}
	const auto *secrets = secret_area_get_state();
	out["secrets"] = json::array();
	for (int i = 0; i < secret_area_total(secrets); ++i)
		out["secrets"].push_back({ secrets->secrets[i].identity, secrets->found[i] });
	return out;
}
#undef FIELD
#undef GLOBAL
} // namespace

nlohmann::ordered_json input_demo_endlevel_trace_snapshot()
{
	return endlevel_state();
}

int input_demo_world_trace_write(uint32_t frame, const char *phase, char *error, size_t error_size)
{
	static json previous;
	json current = world_state();
	const bool boundary = phase != nullptr;
	json record = { { "type", boundary ? "world_boundary" : "world_state" }, { "version", 8 }, { "f", frame }, { "reset", boundary || frame == 0 }, { "state", json::object() } };
	if (boundary) record["phase"] = phase;
	for (auto it = current.begin(); it != current.end(); ++it)
		if (boundary || frame == 0 || !previous.contains(it.key()) || previous[it.key()] != it.value()) record["state"][it.key()] = it.value();
	if (!boundary) previous = std::move(current);
	return input_demo_state_trace_write_json(record.dump().c_str(), error, error_size);
}
