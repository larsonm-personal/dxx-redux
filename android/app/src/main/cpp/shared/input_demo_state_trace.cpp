#include "input_demo_state_trace.h"

#include <stdio.h>
#include <string.h>

#include <string>

#include <nlohmann/json.hpp>

#include "input_demo_replay.h"

namespace
{

using ordered_json = nlohmann::ordered_json;

enum {
	INPUT_DEMO_STATE_TRACE_MAX_SOURCE = 32,
	INPUT_DEMO_STATE_TRACE_MAX_GAME = 8,
	INPUT_DEMO_STATE_TRACE_MAX_MISSION = 64,
	INPUT_DEMO_STATE_TRACE_MAX_START_MODE = 32,
	INPUT_DEMO_STATE_TRACE_MAX_STATE_JSON = 8192
};

typedef struct input_demo_state_trace_session {
	int active;
	FILE *file;
	char source[INPUT_DEMO_STATE_TRACE_MAX_SOURCE];
} input_demo_state_trace_session;

static input_demo_state_trace_session g_input_demo_state_trace_session;

static int copy_error(const char *message, char *error, size_t error_size)
{
	if (error && error_size)
		snprintf(error, error_size, "%s", message ? message : "unknown error");
	return 0;
}

static void write_json_string(FILE *out, const char *text)
{
	const unsigned char *cursor = (const unsigned char *) (text ? text : "");

	fputc('"', out);
	for (; *cursor; ++cursor) {
		switch (*cursor) {
			case '\\':
				fputs("\\\\", out);
				break;
			case '"':
				fputs("\\\"", out);
				break;
			case '\b':
				fputs("\\b", out);
				break;
			case '\f':
				fputs("\\f", out);
				break;
			case '\n':
				fputs("\\n", out);
				break;
			case '\r':
				fputs("\\r", out);
				break;
			case '\t':
				fputs("\\t", out);
				break;
			default:
				if (*cursor < 0x20)
					fprintf(out, "\\u%04x", (unsigned int) *cursor);
				else
					fputc(*cursor, out);
				break;
		}
	}
	fputc('"', out);
}

static void reset_session(void)
{
	if (g_input_demo_state_trace_session.file) {
		fclose(g_input_demo_state_trace_session.file);
		g_input_demo_state_trace_session.file = NULL;
	}
	memset(&g_input_demo_state_trace_session, 0, sizeof(g_input_demo_state_trace_session));
}

static int game_name_from_id(int game, char *name, size_t name_size)
{
	if (!name || !name_size)
		return 0;
	if (game == INPUT_DEMO_GAME_D1) {
		snprintf(name, name_size, "%s", "d1");
		return 1;
	}
	if (game == INPUT_DEMO_GAME_D2) {
		snprintf(name, name_size, "%s", "d2");
		return 1;
	}
	return 0;
}

static ordered_json input_demo_state_trace_build_diag_json(const input_demo_state_trace_diag &diag)
{
	ordered_json root = ordered_json::object();
	ordered_json object_slot_counts = ordered_json::array();
	ordered_json object_slot_hashes = ordered_json::array();
	ordered_json object_focus_slot_hashes = ordered_json::array();

	for (int index = 0; index < INPUT_DEMO_OBJECT_SLOT_BUCKET_COUNT; ++index) {
		object_slot_counts.push_back(diag.object_slot_counts[index]);
		object_slot_hashes.push_back(diag.object_slot_hashes[index]);
	}
	for (int index = 0; index < INPUT_DEMO_OBJECT_SLOT_BUCKET_SIZE; ++index)
		object_focus_slot_hashes.push_back(diag.object_focus_slot_hashes[index]);

	root["awareness_events"] = diag.awareness_events;
	root["camera_awake_robots"] = diag.camera_awake_robots;
	root["danger_laser_robots"] = diag.danger_laser_robots;
	root["d_tick_count"] = diag.d_tick_count;
	root["runtime_state_hash"] = diag.runtime_state_hash;
	root["object_allocator_num_objects"] = diag.object_allocator_num_objects;
	root["object_signature_seed"] = diag.object_signature_seed;
	root["object_free_list_count"] = diag.object_free_list_count;
	root["object_free_list_hash"] = diag.object_free_list_hash;
	root["object_free_head0"] = diag.object_free_head0;
	root["object_free_head1"] = diag.object_free_head1;
	root["object_free_head2"] = diag.object_free_head2;
	root["object_free_head3"] = diag.object_free_head3;
	root["object_homer_frame_count"] = diag.object_homer_frame_count;
	root["object_current_homer_frame_time"] = diag.object_current_homer_frame_time;
	root["object_do_homer_frame"] = diag.object_do_homer_frame;
	root["weapon_next_laser_delta"] = diag.weapon_next_laser_delta;
	root["weapon_next_missile_delta"] = diag.weapon_next_missile_delta;
	root["weapon_last_laser_delta"] = diag.weapon_last_laser_delta;
	root["weapon_next_flare_delta"] = diag.weapon_next_flare_delta;
	root["weapon_auto_fusion_delta"] = diag.weapon_auto_fusion_delta;
	root["weapon_last_omega_delta"] = diag.weapon_last_omega_delta;
	root["weapon_global_laser_firing_count"] = diag.weapon_global_laser_firing_count;
	root["weapon_global_missile_firing_count"] = diag.weapon_global_missile_firing_count;
	root["weapon_fusion_charge"] = diag.weapon_fusion_charge;
	root["weapon_spreadfire_toggle"] = diag.weapon_spreadfire_toggle;
	root["weapon_missile_gun"] = diag.weapon_missile_gun;
	root["weapon_proximity_dropped"] = diag.weapon_proximity_dropped;
	root["weapon_helix_orientation"] = diag.weapon_helix_orientation;
	root["weapon_smartmines_dropped"] = diag.weapon_smartmines_dropped;
	root["player_vel_x"] = diag.player_vel_x;
	root["player_vel_y"] = diag.player_vel_y;
	root["player_vel_z"] = diag.player_vel_z;
	root["player_last_x"] = diag.player_last_x;
	root["player_last_y"] = diag.player_last_y;
	root["player_last_z"] = diag.player_last_z;
	root["player_weapon_count"] = diag.player_weapon_count;
	root["player_weapon_hash"] = diag.player_weapon_hash;
	root["highest_object_index"] = diag.highest_object_index;
	root["live_object_count"] = diag.live_object_count;
	root["live_object_hash"] = diag.live_object_hash;
	root["object_slot_bucket_size"] = diag.object_slot_bucket_size;
	root["object_slot_counts"] = object_slot_counts;
	root["object_slot_hashes"] = object_slot_hashes;
	root["object_focus_slot_base"] = diag.object_focus_slot_base;
	root["object_focus_slot_hashes"] = object_focus_slot_hashes;
	root["robot_object_count"] = diag.robot_object_count;
	root["robot_state_hash"] = diag.robot_state_hash;
	root["robot_changed_obj"] = diag.robot_changed_obj;
	root["robot_changed_sig"] = diag.robot_changed_sig;
	root["robot_changed_id"] = diag.robot_changed_id;
	root["robot_changed_bucket"] = diag.robot_changed_bucket;
	root["robot_changed_prev_hash"] = diag.robot_changed_prev_hash;
	root["robot_changed_hash"] = diag.robot_changed_hash;
	root["robot_changed_type"] = diag.robot_changed_type;
	root["robot_changed_seg"] = diag.robot_changed_seg;
	root["robot_changed_control"] = diag.robot_changed_control;
	root["robot_changed_movement"] = diag.robot_changed_movement;
	root["robot_changed_render"] = diag.robot_changed_render;
	root["robot_changed_flags"] = diag.robot_changed_flags;
	root["robot_changed_x"] = diag.robot_changed_x;
	root["robot_changed_y"] = diag.robot_changed_y;
	root["robot_changed_z"] = diag.robot_changed_z;
	root["robot_changed_last_x"] = diag.robot_changed_last_x;
	root["robot_changed_last_y"] = diag.robot_changed_last_y;
	root["robot_changed_last_z"] = diag.robot_changed_last_z;
	root["robot_changed_vel_x"] = diag.robot_changed_vel_x;
	root["robot_changed_vel_y"] = diag.robot_changed_vel_y;
	root["robot_changed_vel_z"] = diag.robot_changed_vel_z;
	root["robot_changed_rotvel_x"] = diag.robot_changed_rotvel_x;
	root["robot_changed_rotvel_y"] = diag.robot_changed_rotvel_y;
	root["robot_changed_rotvel_z"] = diag.robot_changed_rotvel_z;
	root["robot_changed_model"] = diag.robot_changed_model;
	root["robot_changed_subobj_flags"] = diag.robot_changed_subobj_flags;
	root["robot_sample_obj"] = diag.robot_sample_obj;
	root["robot_sample_sig"] = diag.robot_sample_sig;
	root["robot_sample_id"] = diag.robot_sample_id;
	root["robot_sample_seg"] = diag.robot_sample_seg;
	root["robot_sample_behavior"] = diag.robot_sample_behavior;
	root["robot_sample_mode"] = diag.robot_sample_mode;
	root["robot_sample_cur_state"] = diag.robot_sample_cur_state;
	root["robot_sample_goal_state"] = diag.robot_sample_goal_state;
	root["robot_sample_goal_seg"] = diag.robot_sample_goal_seg;
	root["robot_sample_hide_index"] = diag.robot_sample_hide_index;
	root["robot_sample_path_dir"] = diag.robot_sample_path_dir;
	root["robot_sample_prev_vis"] = diag.robot_sample_prev_vis;
	root["robot_sample_aware"] = diag.robot_sample_aware;
	root["robot_sample_aware_time"] = diag.robot_sample_aware_time;
	root["robot_sample_since"] = diag.robot_sample_since;
	root["robot_sample_next_action"] = diag.robot_sample_next_action;
	root["robot_sample_retry"] = diag.robot_sample_retry;
	root["robot_sample_retry_chain"] = diag.robot_sample_retry_chain;
	root["robot_sample_path_index"] = diag.robot_sample_path_index;
	root["robot_sample_path_length"] = diag.robot_sample_path_length;
	root["robot_sample_phys_flags"] = diag.robot_sample_phys_flags;
	root["robot_sample_vel_x"] = diag.robot_sample_vel_x;
	root["robot_sample_vel_y"] = diag.robot_sample_vel_y;
	root["robot_sample_vel_z"] = diag.robot_sample_vel_z;
	root["robot_sample_pos_x"] = diag.robot_sample_pos_x;
	root["robot_sample_pos_y"] = diag.robot_sample_pos_y;
	root["robot_sample_pos_z"] = diag.robot_sample_pos_z;
	root["robot_sample_goal_x"] = diag.robot_sample_goal_x;
	root["robot_sample_goal_y"] = diag.robot_sample_goal_y;
	root["robot_sample_goal_z"] = diag.robot_sample_goal_z;
	root["robot_sample_next_goal_x"] = diag.robot_sample_next_goal_x;
	root["robot_sample_next_goal_y"] = diag.robot_sample_next_goal_y;
	root["robot_sample_next_goal_z"] = diag.robot_sample_next_goal_z;
	root["robot_sample_mass"] = diag.robot_sample_mass;
	root["robot_sample_drag"] = diag.robot_sample_drag;
	root["robot_sample_brakes"] = diag.robot_sample_brakes;
	root["robot_sample_fvec_x"] = diag.robot_sample_fvec_x;
	root["robot_sample_fvec_y"] = diag.robot_sample_fvec_y;
	root["robot_sample_fvec_z"] = diag.robot_sample_fvec_z;
	root["robot_sample_rotthrust_x"] = diag.robot_sample_rotthrust_x;
	root["robot_sample_rotthrust_y"] = diag.robot_sample_rotthrust_y;
	root["robot_sample_rotthrust_z"] = diag.robot_sample_rotthrust_z;
	root["robot_sample_rotvel_x"] = diag.robot_sample_rotvel_x;
	root["robot_sample_rotvel_y"] = diag.robot_sample_rotvel_y;
	root["robot_sample_rotvel_z"] = diag.robot_sample_rotvel_z;
	root["robot_ai_static_state_hash"] = diag.robot_ai_static_state_hash;
	root["robot_ai_local_state_hash"] = diag.robot_ai_local_state_hash;
	root["weapon_object_count"] = diag.weapon_object_count;
	root["weapon_state_hash"] = diag.weapon_state_hash;
	root["fireball_object_count"] = diag.fireball_object_count;
	root["fireball_state_hash"] = diag.fireball_state_hash;
	root["debris_object_count"] = diag.debris_object_count;
	root["debris_state_hash"] = diag.debris_state_hash;
	root["segment_object_list_count"] = diag.segment_object_list_count;
	root["segment_object_list_hash"] = diag.segment_object_list_hash;
	root["segment_object_link_error_count"] = diag.segment_object_link_error_count;
	root["player_weapon_obj0"] = diag.player_weapon_obj0;
	root["player_weapon_sig0"] = diag.player_weapon_sig0;
	root["player_weapon_id0"] = diag.player_weapon_id0;
	root["player_weapon_obj1"] = diag.player_weapon_obj1;
	root["player_weapon_sig1"] = diag.player_weapon_sig1;
	root["player_weapon_id1"] = diag.player_weapon_id1;
	root["player_weapon_obj2"] = diag.player_weapon_obj2;
	root["player_weapon_sig2"] = diag.player_weapon_sig2;
	root["player_weapon_id2"] = diag.player_weapon_id2;
	root["player_weapon_obj3"] = diag.player_weapon_obj3;
	root["player_weapon_sig3"] = diag.player_weapon_sig3;
	root["player_weapon_id3"] = diag.player_weapon_id3;
	root["ai_probe_skip_count"] = diag.ai_probe_skip_count;
	root["ai_probe_skip_obj"] = diag.ai_probe_skip_obj;
	root["ai_probe_skip_sig"] = diag.ai_probe_skip_sig;
	root["ai_probe_skip_id"] = diag.ai_probe_skip_id;
	root["ai_probe_timeslice_count"] = diag.ai_probe_timeslice_count;
	root["ai_probe_timeslice_obj"] = diag.ai_probe_timeslice_obj;
	root["ai_probe_timeslice_sig"] = diag.ai_probe_timeslice_sig;
	root["ai_probe_timeslice_id"] = diag.ai_probe_timeslice_id;
	root["ai_probe_process_count"] = diag.ai_probe_process_count;
	root["ai_probe_process_obj"] = diag.ai_probe_process_obj;
	root["ai_probe_process_sig"] = diag.ai_probe_process_sig;
	root["ai_probe_process_id"] = diag.ai_probe_process_id;
	root["ai_probe_phys_skip_count"] = diag.ai_probe_phys_skip_count;
	root["ai_probe_phys_skip_obj"] = diag.ai_probe_phys_skip_obj;
	root["ai_probe_phys_skip_sig"] = diag.ai_probe_phys_skip_sig;
	root["ai_probe_phys_skip_id"] = diag.ai_probe_phys_skip_id;
	root["ai_probe_phys_skip_before"] = diag.ai_probe_phys_skip_before;
	root["ai_probe_phys_skip_after"] = diag.ai_probe_phys_skip_after;
	return root;
}

} // namespace

bool input_demo_state_trace_diag_to_json_text(const input_demo_state_trace_diag *diag,
                                              std::string *json_text,
                                              std::string *error)
{
	if (!diag) {
		if (error)
			*error = "missing state trace diag";
		return false;
	}
	if (!json_text) {
		if (error)
			*error = "missing state trace diag output";
		return false;
	}
	*json_text = input_demo_state_trace_build_diag_json(*diag).dump();
	return true;
}

extern "C" {

int input_demo_state_trace_is_active(void)
{
	return g_input_demo_state_trace_session.active ? 1 : 0;
}

void input_demo_state_trace_stop(void)
{
	reset_session();
}

int input_demo_state_trace_start(const char *path,
                                 const char *source,
                                 const char *game,
                                 const char *mission,
                                 int level,
                                 int difficulty,
                                 const char *start_mode,
                                 uint32_t frame_count,
                                 char *error,
                                 size_t error_size)
{
	FILE *file;

	if (!path || !path[0])
		return copy_error("missing input demo state trace path", error, error_size);
	if (!source || !source[0])
		return copy_error("missing input demo state trace source", error, error_size);
	if (!game || !game[0])
		return copy_error("missing input demo state trace game", error, error_size);
	if (!mission || !mission[0])
		return copy_error("missing input demo state trace mission", error, error_size);
	if (!start_mode || !start_mode[0])
		return copy_error("missing input demo state trace start_mode", error, error_size);
	reset_session();
	file = fopen(path, "wb");
	if (!file)
		return copy_error("could not open input demo state trace file", error, error_size);
	g_input_demo_state_trace_session.file = file;
	g_input_demo_state_trace_session.active = 1;
	snprintf(g_input_demo_state_trace_session.source,
	         sizeof(g_input_demo_state_trace_session.source),
	         "%s",
	         source);
	fputs("{\"type\":\"meta\",\"version\":1,\"source\":", file);
	write_json_string(file, source);
	fputs(",\"game\":", file);
	write_json_string(file, game);
	fputs(",\"mission\":", file);
	write_json_string(file, mission);
	fprintf(file,
	        ",\"level\":%d,\"difficulty\":%d,\"start_mode\":",
	        level,
	        difficulty);
	write_json_string(file, start_mode);
	fprintf(file, ",\"frame_count\":%u}\n", frame_count);
	fflush(file);
	return 1;
}

int input_demo_state_trace_start_replay(const char *path,
                                        char *error,
                                        size_t error_size)
{
	char game[INPUT_DEMO_STATE_TRACE_MAX_GAME] = "";
	const char *mission;
	const char *start_mode;

	if (!input_demo_replay_is_loaded())
		return copy_error("input demo replay is not loaded", error, error_size);
	if (!game_name_from_id(input_demo_replay_game(), game, sizeof(game)))
		return copy_error("input demo replay game is invalid", error, error_size);
	mission = input_demo_replay_mission();
	start_mode = input_demo_replay_start_mode();
	return input_demo_state_trace_start(path,
	                                    "replay",
	                                    game,
	                                    mission,
	                                    input_demo_replay_level(),
	                                    input_demo_replay_difficulty(),
	                                    start_mode,
	                                    input_demo_replay_frame_count(),
	                                    error,
	                                    error_size);
}

int input_demo_state_trace_write_frame(uint32_t frame,
                                       int32_t frame_time,
                                       uint32_t rng_state,
                                       int has_rng_call_count,
                                       uint32_t rng_call_count,
                                       const input_demo_state_trace_diag *diag,
                                       const input_demo_result *state,
                                       char *error,
                                       size_t error_size)
{
	char state_json[INPUT_DEMO_STATE_TRACE_MAX_STATE_JSON] = "";
	FILE *file;
	std::string diag_json;
	std::string diag_error;

	if (!g_input_demo_state_trace_session.active || !g_input_demo_state_trace_session.file)
		return copy_error("input demo state trace is not active", error, error_size);
	if (!state)
		return copy_error("missing input demo state trace frame state", error, error_size);
	if (!input_demo_result_snapshot_to_json_buffer(state, state_json, sizeof(state_json)))
		return copy_error("could not encode input demo state trace frame state", error, error_size);
	if (diag && !input_demo_state_trace_diag_to_json_text(diag, &diag_json, &diag_error))
		return copy_error(diag_error.c_str(), error, error_size);
	file = g_input_demo_state_trace_session.file;
	fputs("{\"type\":\"frame_state\",\"source\":", file);
	write_json_string(file, g_input_demo_state_trace_session.source);
	fprintf(file,
	        ",\"f\":%u,\"ft\":%d,\"rng\":{\"s\":%u",
	        frame,
	        frame_time,
	        rng_state);
	if (has_rng_call_count)
		fprintf(file, ",\"c\":%u", rng_call_count);
	fputs("}", file);
	if (diag)
		fprintf(file, ",\"diag\":%s", diag_json.c_str());
	fprintf(file, ",\"state\":%s}\n", state_json);
	if (fflush(file) != 0)
		return copy_error("could not flush input demo state trace file", error, error_size);
	return 1;
}
}
