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
	ordered_json robot_object_bucket_hashes = ordered_json::array();
	ordered_json robot_ai_static_bucket_hashes = ordered_json::array();
	ordered_json robot_ai_local_bucket_hashes = ordered_json::array();
	ordered_json robot_anim_pose_bucket_hashes = ordered_json::array();
	ordered_json weapon_trace_slots = ordered_json::array();
	ordered_json weapon_trace_sigs = ordered_json::array();
	ordered_json weapon_trace_ids = ordered_json::array();
	ordered_json weapon_trace_hashes = ordered_json::array();
	ordered_json weapon_trace_segs = ordered_json::array();
	ordered_json weapon_trace_lifeleft = ordered_json::array();
	ordered_json weapon_trace_track_goals = ordered_json::array();
	ordered_json weapon_trace_fvec_x = ordered_json::array();
	ordered_json weapon_trace_fvec_y = ordered_json::array();
	ordered_json weapon_trace_fvec_z = ordered_json::array();
	ordered_json weapon_trace_vel_x = ordered_json::array();
	ordered_json weapon_trace_vel_y = ordered_json::array();
	ordered_json weapon_trace_vel_z = ordered_json::array();
	ordered_json fireball_trace_slots = ordered_json::array();
	ordered_json fireball_trace_sigs = ordered_json::array();
	ordered_json fireball_trace_ids = ordered_json::array();
	ordered_json fireball_trace_hashes = ordered_json::array();
	ordered_json fireball_trace_segs = ordered_json::array();
	ordered_json fireball_trace_lifeleft = ordered_json::array();
	ordered_json fireball_trace_delete_objnums = ordered_json::array();
	ordered_json fireball_trace_attached_objs = ordered_json::array();
	ordered_json segment_trace_segs = ordered_json::array();
	ordered_json segment_trace_counts = ordered_json::array();
	ordered_json segment_trace_hashes = ordered_json::array();
	ordered_json segment_trace_heads = ordered_json::array();
	ordered_json segment_trace_objs = ordered_json::array();
	ordered_json segment_trace_sigs = ordered_json::array();
	ordered_json segment_trace_types = ordered_json::array();
	ordered_json segment_trace_ids = ordered_json::array();
	ordered_json segment_trace_prevs = ordered_json::array();
	ordered_json segment_trace_nexts = ordered_json::array();

	for (int index = 0; index < INPUT_DEMO_OBJECT_SLOT_BUCKET_COUNT; ++index) {
		object_slot_counts.push_back(diag.object_slot_counts[index]);
		object_slot_hashes.push_back(diag.object_slot_hashes[index]);
		robot_object_bucket_hashes.push_back(diag.robot_object_bucket_hashes[index]);
		robot_ai_static_bucket_hashes.push_back(
		    diag.robot_ai_static_bucket_hashes[index]);
		robot_ai_local_bucket_hashes.push_back(
		    diag.robot_ai_local_bucket_hashes[index]);
		robot_anim_pose_bucket_hashes.push_back(
		    diag.robot_anim_pose_bucket_hashes[index]);
	}
	for (int index = 0; index < INPUT_DEMO_OBJECT_SLOT_BUCKET_SIZE; ++index)
		object_focus_slot_hashes.push_back(diag.object_focus_slot_hashes[index]);
	for (int index = 0; index < INPUT_DEMO_WEAPON_TRACE_COUNT; ++index) {
		weapon_trace_slots.push_back(diag.weapon_trace_slots[index]);
		weapon_trace_sigs.push_back(diag.weapon_trace_sigs[index]);
		weapon_trace_ids.push_back(diag.weapon_trace_ids[index]);
		weapon_trace_hashes.push_back(diag.weapon_trace_hashes[index]);
		weapon_trace_segs.push_back(diag.weapon_trace_segs[index]);
		weapon_trace_lifeleft.push_back(diag.weapon_trace_lifeleft[index]);
		weapon_trace_track_goals.push_back(diag.weapon_trace_track_goals[index]);
		weapon_trace_fvec_x.push_back(diag.weapon_trace_fvec_x[index]);
		weapon_trace_fvec_y.push_back(diag.weapon_trace_fvec_y[index]);
		weapon_trace_fvec_z.push_back(diag.weapon_trace_fvec_z[index]);
		weapon_trace_vel_x.push_back(diag.weapon_trace_vel_x[index]);
		weapon_trace_vel_y.push_back(diag.weapon_trace_vel_y[index]);
		weapon_trace_vel_z.push_back(diag.weapon_trace_vel_z[index]);
	}
	for (int index = 0; index < INPUT_DEMO_FIREBALL_TRACE_COUNT; ++index) {
		fireball_trace_slots.push_back(diag.fireball_trace_slots[index]);
		fireball_trace_sigs.push_back(diag.fireball_trace_sigs[index]);
		fireball_trace_ids.push_back(diag.fireball_trace_ids[index]);
		fireball_trace_hashes.push_back(diag.fireball_trace_hashes[index]);
		fireball_trace_segs.push_back(diag.fireball_trace_segs[index]);
		fireball_trace_lifeleft.push_back(diag.fireball_trace_lifeleft[index]);
		fireball_trace_delete_objnums.push_back(
		    diag.fireball_trace_delete_objnums[index]);
		fireball_trace_attached_objs.push_back(diag.fireball_trace_attached_objs[index]);
	}
	for (int index = 0; index < INPUT_DEMO_SEGMENT_TRACE_COUNT; ++index) {
		segment_trace_segs.push_back(diag.segment_trace_segs[index]);
		segment_trace_counts.push_back(diag.segment_trace_counts[index]);
		segment_trace_hashes.push_back(diag.segment_trace_hashes[index]);
		segment_trace_heads.push_back(diag.segment_trace_heads[index]);
	}
	for (int index = 0; index < INPUT_DEMO_SEGMENT_TRACE_CHAIN_TOTAL; ++index) {
		segment_trace_objs.push_back(diag.segment_trace_objs[index]);
		segment_trace_sigs.push_back(diag.segment_trace_sigs[index]);
		segment_trace_types.push_back(diag.segment_trace_types[index]);
		segment_trace_ids.push_back(diag.segment_trace_ids[index]);
		segment_trace_prevs.push_back(diag.segment_trace_prevs[index]);
		segment_trace_nexts.push_back(diag.segment_trace_nexts[index]);
	}

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
	root["player_bump_frame"] = diag.player_bump_frame;
	root["player_bump_count"] = diag.player_bump_count;
	root["player_bump_step_hash"] = diag.player_bump_step_hash;
	root["player_bump_other_obj"] = diag.player_bump_other_obj;
	root["player_bump_other_sig"] = diag.player_bump_other_sig;
	root["player_bump_other_type"] = diag.player_bump_other_type;
	root["player_bump_other_id"] = diag.player_bump_other_id;
	root["player_bump_damage_flag"] = diag.player_bump_damage_flag;
	root["player_bump_force_mag"] = diag.player_bump_force_mag;
	root["player_bump_damage_raw"] = diag.player_bump_damage_raw;
	root["player_bump_damage_scaled"] = diag.player_bump_damage_scaled;
	root["player_bump_other_attack_type"] = diag.player_bump_other_attack_type;
	root["player_bump_rel_vel_x"] = diag.player_bump_rel_vel_x;
	root["player_bump_rel_vel_y"] = diag.player_bump_rel_vel_y;
	root["player_bump_rel_vel_z"] = diag.player_bump_rel_vel_z;
	root["player_bump_force_x"] = diag.player_bump_force_x;
	root["player_bump_force_y"] = diag.player_bump_force_y;
	root["player_bump_force_z"] = diag.player_bump_force_z;
	root["player_bump_player_mass"] = diag.player_bump_player_mass;
	root["player_bump_other_mass"] = diag.player_bump_other_mass;
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
	root["robot_object_bucket_hashes"] = robot_object_bucket_hashes;
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
	root["robot_sample_model"] = diag.robot_sample_model;
	root["robot_sample_subobj_flags"] = diag.robot_sample_subobj_flags;
	root["robot_sample_behavior"] = diag.robot_sample_behavior;
	root["robot_sample_mode"] = diag.robot_sample_mode;
	root["robot_sample_cur_state"] = diag.robot_sample_cur_state;
	root["robot_sample_goal_state"] = diag.robot_sample_goal_state;
	root["robot_sample_anim_at_goal"] = diag.robot_sample_anim_at_goal;
	root["robot_sample_anim_angles_hash"] = diag.robot_sample_anim_angles_hash;
	root["robot_sample_goal_angles_hash"] = diag.robot_sample_goal_angles_hash;
	root["robot_sample_delta_angles_hash"] = diag.robot_sample_delta_angles_hash;
	root["robot_sample_goal_state_hash"] = diag.robot_sample_goal_state_hash;
	root["robot_sample_achieved_state_hash"] = diag.robot_sample_achieved_state_hash;
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
	root["robot_sample_rvec_x"] = diag.robot_sample_rvec_x;
	root["robot_sample_rvec_y"] = diag.robot_sample_rvec_y;
	root["robot_sample_rvec_z"] = diag.robot_sample_rvec_z;
	root["robot_sample_uvec_x"] = diag.robot_sample_uvec_x;
	root["robot_sample_uvec_y"] = diag.robot_sample_uvec_y;
	root["robot_sample_uvec_z"] = diag.robot_sample_uvec_z;
	root["robot_sample_orient_hash"] = diag.robot_sample_orient_hash;
	root["robot_sample_rotthrust_x"] = diag.robot_sample_rotthrust_x;
	root["robot_sample_rotthrust_y"] = diag.robot_sample_rotthrust_y;
	root["robot_sample_rotthrust_z"] = diag.robot_sample_rotthrust_z;
	root["robot_sample_rotvel_x"] = diag.robot_sample_rotvel_x;
	root["robot_sample_rotvel_y"] = diag.robot_sample_rotvel_y;
	root["robot_sample_rotvel_z"] = diag.robot_sample_rotvel_z;
	root["robot_ai_static_state_hash"] = diag.robot_ai_static_state_hash;
	root["robot_ai_static_bucket_hashes"] = robot_ai_static_bucket_hashes;
	root["robot_ai_static_without_changed_hash"] = diag.robot_ai_static_without_changed_hash;
	root["robot_ai_static_changed_obj"] = diag.robot_ai_static_changed_obj;
	root["robot_ai_static_changed_sig"] = diag.robot_ai_static_changed_sig;
	root["robot_ai_static_changed_id"] = diag.robot_ai_static_changed_id;
	root["robot_ai_static_changed_prev_hash"] = diag.robot_ai_static_changed_prev_hash;
	root["robot_ai_static_changed_hash"] = diag.robot_ai_static_changed_hash;
	root["robot_ai_static_changed_behavior"] = diag.robot_ai_static_changed_behavior;
	root["robot_ai_static_changed_flags_hash"] = diag.robot_ai_static_changed_flags_hash;
	root["robot_ai_static_changed_current_gun"] = diag.robot_ai_static_changed_current_gun;
	root["robot_ai_static_changed_current_state"] = diag.robot_ai_static_changed_current_state;
	root["robot_ai_static_changed_goal_state"] = diag.robot_ai_static_changed_goal_state;
	root["robot_ai_static_changed_path_dir"] = diag.robot_ai_static_changed_path_dir;
	root["robot_ai_static_changed_submode"] = diag.robot_ai_static_changed_submode;
	root["robot_ai_static_changed_goalside"] = diag.robot_ai_static_changed_goalside;
	root["robot_ai_static_changed_skip_ai_count"] = diag.robot_ai_static_changed_skip_ai_count;
	root["robot_ai_static_changed_hide_segment"] = diag.robot_ai_static_changed_hide_segment;
	root["robot_ai_static_changed_hide_index"] = diag.robot_ai_static_changed_hide_index;
	root["robot_ai_static_changed_path_length"] = diag.robot_ai_static_changed_path_length;
	root["robot_ai_static_changed_cur_path_index"] = diag.robot_ai_static_changed_cur_path_index;
	root["robot_ai_static_changed_follow_start"] = diag.robot_ai_static_changed_follow_start;
	root["robot_ai_static_changed_follow_end"] = diag.robot_ai_static_changed_follow_end;
	root["robot_ai_static_changed_danger_laser_num"] = diag.robot_ai_static_changed_danger_laser_num;
	root["robot_ai_static_changed_danger_laser_sig"] = diag.robot_ai_static_changed_danger_laser_sig;
	root["robot_ai_local_state_hash"] = diag.robot_ai_local_state_hash;
	root["robot_ai_local_bucket_hashes"] = robot_ai_local_bucket_hashes;
	root["robot_anim_pose_state_hash"] = diag.robot_anim_pose_state_hash;
	root["robot_anim_pose_bucket_hashes"] = robot_anim_pose_bucket_hashes;
	root["robot_anim_pose_changed_obj"] = diag.robot_anim_pose_changed_obj;
	root["robot_anim_pose_changed_sig"] = diag.robot_anim_pose_changed_sig;
	root["robot_anim_pose_changed_id"] = diag.robot_anim_pose_changed_id;
	root["robot_anim_pose_changed_prev_hash"] =
	    diag.robot_anim_pose_changed_prev_hash;
	root["robot_anim_pose_changed_hash"] = diag.robot_anim_pose_changed_hash;
	root["robot_anim_pose_changed_model"] = diag.robot_anim_pose_changed_model;
	root["robot_anim_pose_changed_subobj_flags"] =
	    diag.robot_anim_pose_changed_subobj_flags;
	root["robot_anim_pose_changed_anim_angles_hash"] =
	    diag.robot_anim_pose_changed_anim_angles_hash;
	root["robot_anim_pose_changed_goal_angles_hash"] =
	    diag.robot_anim_pose_changed_goal_angles_hash;
	root["robot_anim_pose_changed_delta_angles_hash"] =
	    diag.robot_anim_pose_changed_delta_angles_hash;
	root["robot_anim_pose_changed_goal_state_hash"] =
	    diag.robot_anim_pose_changed_goal_state_hash;
	root["robot_anim_pose_changed_achieved_state_hash"] =
	    diag.robot_anim_pose_changed_achieved_state_hash;
	root["robot_anim_pose_changed_current_gun"] =
	    diag.robot_anim_pose_changed_current_gun;
	root["robot_anim_pose_changed_current_state"] =
	    diag.robot_anim_pose_changed_current_state;
	root["robot_anim_pose_changed_goal_state"] =
	    diag.robot_anim_pose_changed_goal_state;
	root["weapon_object_count"] = diag.weapon_object_count;
	root["weapon_state_hash"] = diag.weapon_state_hash;
	root["weapon_sample_obj"] = diag.weapon_sample_obj;
	root["weapon_sample_sig"] = diag.weapon_sample_sig;
	root["weapon_sample_id"] = diag.weapon_sample_id;
	root["weapon_sample_seg"] = diag.weapon_sample_seg;
	root["weapon_sample_control"] = diag.weapon_sample_control;
	root["weapon_sample_movement"] = diag.weapon_sample_movement;
	root["weapon_sample_render"] = diag.weapon_sample_render;
	root["weapon_sample_flags"] = diag.weapon_sample_flags;
	root["weapon_sample_phys_flags"] = diag.weapon_sample_phys_flags;
	root["weapon_sample_x"] = diag.weapon_sample_x;
	root["weapon_sample_y"] = diag.weapon_sample_y;
	root["weapon_sample_z"] = diag.weapon_sample_z;
	root["weapon_sample_last_x"] = diag.weapon_sample_last_x;
	root["weapon_sample_last_y"] = diag.weapon_sample_last_y;
	root["weapon_sample_last_z"] = diag.weapon_sample_last_z;
	root["weapon_sample_vel_x"] = diag.weapon_sample_vel_x;
	root["weapon_sample_vel_y"] = diag.weapon_sample_vel_y;
	root["weapon_sample_vel_z"] = diag.weapon_sample_vel_z;
	root["weapon_sample_size"] = diag.weapon_sample_size;
	root["weapon_sample_shields"] = diag.weapon_sample_shields;
	root["weapon_sample_lifeleft"] = diag.weapon_sample_lifeleft;
	root["weapon_sample_parent_type"] = diag.weapon_sample_parent_type;
	root["weapon_sample_parent_num"] = diag.weapon_sample_parent_num;
	root["weapon_sample_parent_sig"] = diag.weapon_sample_parent_sig;
	root["weapon_trace_slots"] = weapon_trace_slots;
	root["weapon_trace_sigs"] = weapon_trace_sigs;
	root["weapon_trace_ids"] = weapon_trace_ids;
	root["weapon_trace_hashes"] = weapon_trace_hashes;
	root["weapon_trace_segs"] = weapon_trace_segs;
	root["weapon_trace_lifeleft"] = weapon_trace_lifeleft;
	root["weapon_trace_track_goals"] = weapon_trace_track_goals;
	root["weapon_trace_fvec_x"] = weapon_trace_fvec_x;
	root["weapon_trace_fvec_y"] = weapon_trace_fvec_y;
	root["weapon_trace_fvec_z"] = weapon_trace_fvec_z;
	root["weapon_trace_vel_x"] = weapon_trace_vel_x;
	root["weapon_trace_vel_y"] = weapon_trace_vel_y;
	root["weapon_trace_vel_z"] = weapon_trace_vel_z;
	root["fireball_object_count"] = diag.fireball_object_count;
	root["fireball_state_hash"] = diag.fireball_state_hash;
	root["fireball_changed_obj"] = diag.fireball_changed_obj;
	root["fireball_changed_sig"] = diag.fireball_changed_sig;
	root["fireball_changed_id"] = diag.fireball_changed_id;
	root["fireball_changed_bucket"] = diag.fireball_changed_bucket;
	root["fireball_changed_prev_hash"] = diag.fireball_changed_prev_hash;
	root["fireball_changed_hash"] = diag.fireball_changed_hash;
	root["fireball_changed_seg"] = diag.fireball_changed_seg;
	root["fireball_changed_control"] = diag.fireball_changed_control;
	root["fireball_changed_movement"] = diag.fireball_changed_movement;
	root["fireball_changed_render"] = diag.fireball_changed_render;
	root["fireball_changed_flags"] = diag.fireball_changed_flags;
	root["fireball_changed_x"] = diag.fireball_changed_x;
	root["fireball_changed_y"] = diag.fireball_changed_y;
	root["fireball_changed_z"] = diag.fireball_changed_z;
	root["fireball_changed_last_x"] = diag.fireball_changed_last_x;
	root["fireball_changed_last_y"] = diag.fireball_changed_last_y;
	root["fireball_changed_last_z"] = diag.fireball_changed_last_z;
	root["fireball_changed_size"] = diag.fireball_changed_size;
	root["fireball_changed_shields"] = diag.fireball_changed_shields;
	root["fireball_changed_lifeleft"] = diag.fireball_changed_lifeleft;
	root["fireball_sample_obj"] = diag.fireball_sample_obj;
	root["fireball_sample_sig"] = diag.fireball_sample_sig;
	root["fireball_sample_id"] = diag.fireball_sample_id;
	root["fireball_sample_hash"] = diag.fireball_sample_hash;
	root["fireball_sample_seg"] = diag.fireball_sample_seg;
	root["fireball_sample_control"] = diag.fireball_sample_control;
	root["fireball_sample_movement"] = diag.fireball_sample_movement;
	root["fireball_sample_render"] = diag.fireball_sample_render;
	root["fireball_sample_flags"] = diag.fireball_sample_flags;
	root["fireball_sample_x"] = diag.fireball_sample_x;
	root["fireball_sample_y"] = diag.fireball_sample_y;
	root["fireball_sample_z"] = diag.fireball_sample_z;
	root["fireball_sample_last_x"] = diag.fireball_sample_last_x;
	root["fireball_sample_last_y"] = diag.fireball_sample_last_y;
	root["fireball_sample_last_z"] = diag.fireball_sample_last_z;
	root["fireball_sample_size"] = diag.fireball_sample_size;
	root["fireball_sample_shields"] = diag.fireball_sample_shields;
	root["fireball_sample_lifeleft"] = diag.fireball_sample_lifeleft;
	root["fireball_sample_attached_obj"] = diag.fireball_sample_attached_obj;
	root["fireball_sample_spawn_time"] = diag.fireball_sample_spawn_time;
	root["fireball_sample_delete_time"] = diag.fireball_sample_delete_time;
	root["fireball_sample_delete_objnum"] = diag.fireball_sample_delete_objnum;
	root["fireball_sample_attach_parent"] = diag.fireball_sample_attach_parent;
	root["fireball_sample_prev_attach"] = diag.fireball_sample_prev_attach;
	root["fireball_sample_next_attach"] = diag.fireball_sample_next_attach;
	root["fireball_trace_slots"] = fireball_trace_slots;
	root["fireball_trace_sigs"] = fireball_trace_sigs;
	root["fireball_trace_ids"] = fireball_trace_ids;
	root["fireball_trace_hashes"] = fireball_trace_hashes;
	root["fireball_trace_segs"] = fireball_trace_segs;
	root["fireball_trace_lifeleft"] = fireball_trace_lifeleft;
	root["fireball_trace_delete_objnums"] = fireball_trace_delete_objnums;
	root["fireball_trace_attached_objs"] = fireball_trace_attached_objs;
	root["debris_object_count"] = diag.debris_object_count;
	root["debris_state_hash"] = diag.debris_state_hash;
	root["segment_object_list_count"] = diag.segment_object_list_count;
	root["segment_object_list_hash"] = diag.segment_object_list_hash;
	root["segment_object_link_error_count"] = diag.segment_object_link_error_count;
	root["segment_trace_segs"] = segment_trace_segs;
	root["segment_trace_counts"] = segment_trace_counts;
	root["segment_trace_hashes"] = segment_trace_hashes;
	root["segment_trace_heads"] = segment_trace_heads;
	root["segment_trace_objs"] = segment_trace_objs;
	root["segment_trace_sigs"] = segment_trace_sigs;
	root["segment_trace_types"] = segment_trace_types;
	root["segment_trace_ids"] = segment_trace_ids;
	root["segment_trace_prevs"] = segment_trace_prevs;
	root["segment_trace_nexts"] = segment_trace_nexts;
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
