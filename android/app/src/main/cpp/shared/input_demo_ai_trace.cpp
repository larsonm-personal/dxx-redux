#include "input_demo_ai_trace.h"

#include <cstring>
#include <string>
#include <nlohmann/json.hpp>

extern "C" {
#include "ai.h"
#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2/d1_in_d2_ai.h"
#endif
}

namespace
{
using json = nlohmann::ordered_json;

json vector(const vms_vector &v)
{
	return { v.x, v.y, v.z };
}
json angles(const vms_angvec &v)
{
	return { v.p, v.b, v.h };
}

// Copy packed engine scalars instead of binding unaligned references
#define FIELD(out, value, name) (out)[#name] = static_cast<int64_t>((value).name)
#define GLOBAL(out, name)       (out)[#name] = static_cast<int64_t>(name)

json local_state(const ai_local &a)
{
	json out = json::object();
	FIELD(out, a, player_awareness_type);
	FIELD(out, a, retry_count);
	FIELD(out, a, consecutive_retries);
	FIELD(out, a, mode);
	FIELD(out, a, previous_visibility);
	FIELD(out, a, rapidfire_count);
	FIELD(out, a, goal_segment);
	FIELD(out, a, next_fire);
	FIELD(out, a, player_awareness_time);
	FIELD(out, a, time_player_seen);
	FIELD(out, a, time_player_sound_attacked);
	FIELD(out, a, next_misc_sound_time);
	FIELD(out, a, time_since_processed);
#ifdef DXX_BUILD_DESCENT_II
	FIELD(out, a, next_action_time);
	FIELD(out, a, next_fire2);
	FIELD(out["d1_saved"], a.d1_saved, last_see_time);
	FIELD(out["d1_saved"], a.d1_saved, last_attack_time);
	FIELD(out["d1_saved"], a.d1_saved, wait_time);
#else
	FIELD(out, a, last_see_time);
	FIELD(out, a, last_attack_time);
	FIELD(out, a, wait_time);
#endif
	for (const char *key : { "goal_angles", "delta_angles", "goal_state", "achieved_state" }) out[key] = json::array();
	for (int i = 0; i < MAX_SUBMODELS; ++i) {
		out["goal_angles"].push_back(angles(a.goal_angles[i]));
		out["delta_angles"].push_back(angles(a.delta_angles[i]));
		out["goal_state"].push_back(static_cast<int>(a.goal_state[i]));
		out["achieved_state"].push_back(static_cast<int>(a.achieved_state[i]));
	}
	return out;
}

template <typename T, size_t N>
json scalars(const T (&values)[N])
{
	json out = json::array();
	for (const auto value : values) out.push_back(static_cast<int64_t>(value));
	return out;
}
} // namespace

nlohmann::ordered_json input_demo_ai_local_trace_snapshot(int slot)
{
	return local_state(Ai_local_info[slot]);
}

nlohmann::ordered_json input_demo_ai_trace_snapshot()
{
	json out = json::object();
	GLOBAL(out, Ai_initialized);
	GLOBAL(out, Overall_agitation);
	out["Believed_player_pos"] = vector(Believed_player_pos);
#ifdef DXX_BUILD_DESCENT_II
	GLOBAL(out, Believed_player_seg);
	GLOBAL(out, Ai_last_missile_camera);
#endif
	// Slot zero defines the encoding default, including its nonzero saved clocks
	// Every other slot is either field-identical to it or emitted in full
	// This retains inactive storage without repeating the usual player-slot
	// defaults in every unused slot after checkpoint clock rebasing
	const ai_local &default_source = Ai_local_info[0];
	const json local_default = local_state(default_source);
	out["local_capacity"] = MAX_OBJECTS;
	out["local_default"] = local_default;
	out["locals"] = json::object();
	for (int i = 1; i < MAX_OBJECTS; ++i) {
		// Unequal padding still takes the full field comparison below
		if (std::memcmp(&Ai_local_info[i], &default_source, sizeof(default_source)) == 0) continue;
		json row = local_state(Ai_local_info[i]);
		if (row != local_default) out["locals"][std::to_string(i)] = std::move(row);
	}
	ai_path_runtime_state path = {};
	ai_path_get_runtime_state(&path);
	auto &runtime = out["path_runtime"];
	FIELD(runtime, path, last_tick_garbage_collected);
	FIELD(runtime, path, player_path_length);
	FIELD(runtime, path, player_hide_index);
	FIELD(runtime, path, player_cur_path_index);
	FIELD(runtime, path, player_following_path_flag);
	FIELD(runtime, path, player_goal_segment);
#ifdef DXX_BUILD_DESCENT_II
	FIELD(runtime, path, last_buddy_polish_path_tick);
#endif
	out["path_free_index"] = Point_segs_free_ptr - Point_segs;
	out["paths"] = json::array();
	for (const auto &p : Point_segs) out["paths"].push_back({ { "segment", p.segnum }, { "point", vector(p.point) } });
	out["cloak"] = json::array();
	for (const auto &c : Ai_cloak_info) {
		json row = { { "last_time", c.last_time }, { "last_position", vector(c.last_position) } };
#ifdef DXX_BUILD_DESCENT_II
		FIELD(row, c, last_segment);
#endif
		out["cloak"].push_back(std::move(row));
	}
	GLOBAL(out, Num_awareness_events);
	out["awareness"] = json::array();
	// Preserve the complete queue storage plus its active count
	for (const auto &e : Awareness_events)
		out["awareness"].push_back({ { "segment", e.segnum }, { "type", e.type }, { "position", vector(e.pos) } });
	auto &boss = out["boss"];
	GLOBAL(boss, Boss_cloak_start_time);
	GLOBAL(boss, Boss_cloak_end_time);
	GLOBAL(boss, Last_teleport_time);
	GLOBAL(boss, Boss_teleport_interval);
	GLOBAL(boss, Boss_cloak_interval);
	GLOBAL(boss, Boss_cloak_duration);
	GLOBAL(boss, Last_gate_time);
	GLOBAL(boss, Gate_interval);
	GLOBAL(boss, Boss_dying_start_time);
	GLOBAL(boss, Boss_dying);
	GLOBAL(boss, Boss_dying_sound_playing);
	GLOBAL(boss, Num_boss_teleport_segs);
	boss["teleport_segments"] = scalars(Boss_teleport_segs);
#if defined(DXX_BUILD_DESCENT_II) || !defined(SHAREWARE)
	GLOBAL(boss, Num_boss_gate_segs);
	boss["gate_segments"] = scalars(Boss_gate_segs);
#endif
#ifdef DXX_BUILD_DESCENT_II
	GLOBAL(boss, Boss_hit_time);
	boss["d1_hit_pending"] = d1_in_d2_ai_boss_hit_pending();
	boss["d1_been_hit"] = d1_in_d2_ai_boss_been_hit();
#else
	GLOBAL(boss, Boss_hit_this_frame);
	GLOBAL(boss, Boss_been_hit);
#endif
	return out;
}
#undef FIELD
#undef GLOBAL
