#include "input_demo_state_trace.h"

#include <stdio.h>
#include <string.h>

#include <string>
#include <zlib.h>

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
	gzFile compressed_file;
	char source[INPUT_DEMO_STATE_TRACE_MAX_SOURCE];
} input_demo_state_trace_session;

static input_demo_state_trace_session g_input_demo_state_trace_session;

static int copy_error(const char *message, char *error, size_t error_size)
{
	if (error && error_size)
		snprintf(error, error_size, "%s", message ? message : "unknown error");
	return 0;
}

static void reset_session(void)
{
	if (g_input_demo_state_trace_session.file) {
		fclose(g_input_demo_state_trace_session.file);
		g_input_demo_state_trace_session.file = NULL;
	}
	if (g_input_demo_state_trace_session.compressed_file)
		gzclose(g_input_demo_state_trace_session.compressed_file);
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
	// These fixed schema keys are unique and emitted once in their canonical order
	// Append directly: ordered_json operator[] otherwise scans all preceding keys
	ordered_json::object_t root;
	root.reserve(384);
	root.emplace_back("awareness_events", diag.awareness_events);
	root.emplace_back("camera_awake_robots", diag.camera_awake_robots);
	root.emplace_back("danger_laser_robots", diag.danger_laser_robots);
	root.emplace_back("d_tick_count", diag.d_tick_count);
	root.emplace_back("runtime_state_hash", diag.runtime_state_hash);
	root.emplace_back("object_allocator_num_objects", diag.object_allocator_num_objects);
	root.emplace_back("object_signature_seed", diag.object_signature_seed);
	root.emplace_back("object_free_list_count", diag.object_free_list_count);
	root.emplace_back("object_free_list_hash", diag.object_free_list_hash);
	root.emplace_back("object_free_head0", diag.object_free_head0);
	root.emplace_back("object_free_head1", diag.object_free_head1);
	root.emplace_back("object_free_head2", diag.object_free_head2);
	root.emplace_back("object_free_head3", diag.object_free_head3);
	root.emplace_back("object_homer_frame_count", diag.object_homer_frame_count);
	root.emplace_back("object_current_homer_frame_time", diag.object_current_homer_frame_time);
	root.emplace_back("object_do_homer_frame", diag.object_do_homer_frame);
	root.emplace_back("weapon_next_laser_delta", diag.weapon_next_laser_delta);
	root.emplace_back("weapon_next_missile_delta", diag.weapon_next_missile_delta);
	root.emplace_back("weapon_last_laser_delta", diag.weapon_last_laser_delta);
	root.emplace_back("weapon_next_flare_delta", diag.weapon_next_flare_delta);
	root.emplace_back("weapon_auto_fusion_delta", diag.weapon_auto_fusion_delta);
	root.emplace_back("weapon_last_omega_delta", diag.weapon_last_omega_delta);
	root.emplace_back("weapon_global_laser_firing_count", diag.weapon_global_laser_firing_count);
	root.emplace_back("weapon_global_missile_firing_count", diag.weapon_global_missile_firing_count);
	root.emplace_back("weapon_fusion_charge", diag.weapon_fusion_charge);
	root.emplace_back("weapon_spreadfire_toggle", diag.weapon_spreadfire_toggle);
	root.emplace_back("weapon_missile_gun", diag.weapon_missile_gun);
	root.emplace_back("weapon_proximity_dropped", diag.weapon_proximity_dropped);
	root.emplace_back("weapon_helix_orientation", diag.weapon_helix_orientation);
	root.emplace_back("weapon_smartmines_dropped", diag.weapon_smartmines_dropped);
	root.emplace_back("primary_weapon_picked_up", diag.primary_weapon_picked_up);
	root.emplace_back("secondary_weapon_picked_up", diag.secondary_weapon_picked_up);
	root.emplace_back("player_vel_x", diag.player_vel_x);
	root.emplace_back("player_vel_y", diag.player_vel_y);
	root.emplace_back("player_vel_z", diag.player_vel_z);
	root.emplace_back("player_last_x", diag.player_last_x);
	root.emplace_back("player_last_y", diag.player_last_y);
	root.emplace_back("player_last_z", diag.player_last_z);
	root.emplace_back("player_bump_frame", diag.player_bump_frame);
	root.emplace_back("player_bump_count", diag.player_bump_count);
	root.emplace_back("player_bump_step_hash", diag.player_bump_step_hash);
	root.emplace_back("player_bump_other_obj", diag.player_bump_other_obj);
	root.emplace_back("player_bump_other_sig", diag.player_bump_other_sig);
	root.emplace_back("player_bump_other_type", diag.player_bump_other_type);
	root.emplace_back("player_bump_other_id", diag.player_bump_other_id);
	root.emplace_back("player_bump_damage_flag", diag.player_bump_damage_flag);
	root.emplace_back("player_bump_force_mag", diag.player_bump_force_mag);
	root.emplace_back("player_bump_damage_raw", diag.player_bump_damage_raw);
	root.emplace_back("player_bump_damage_scaled", diag.player_bump_damage_scaled);
	root.emplace_back("player_bump_other_attack_type", diag.player_bump_other_attack_type);
	root.emplace_back("player_bump_rel_vel_x", diag.player_bump_rel_vel_x);
	root.emplace_back("player_bump_rel_vel_y", diag.player_bump_rel_vel_y);
	root.emplace_back("player_bump_rel_vel_z", diag.player_bump_rel_vel_z);
	root.emplace_back("player_bump_force_x", diag.player_bump_force_x);
	root.emplace_back("player_bump_force_y", diag.player_bump_force_y);
	root.emplace_back("player_bump_force_z", diag.player_bump_force_z);
	root.emplace_back("player_bump_player_mass", diag.player_bump_player_mass);
	root.emplace_back("player_bump_other_mass", diag.player_bump_other_mass);
	root.emplace_back("player_weapon_count", diag.player_weapon_count);
	root.emplace_back("player_weapon_hash", diag.player_weapon_hash);
	root.emplace_back("highest_object_index", diag.highest_object_index);
	root.emplace_back("live_object_count", diag.live_object_count);
	root.emplace_back("live_object_hash", diag.live_object_hash);
	root.emplace_back("object_slot_bucket_size", diag.object_slot_bucket_size);
	root.emplace_back("object_slot_counts", diag.object_slot_counts);
	root.emplace_back("object_slot_hashes", diag.object_slot_hashes);
	root.emplace_back("object_focus_slot_base", diag.object_focus_slot_base);
	root.emplace_back("object_focus_slot_hashes", diag.object_focus_slot_hashes);
	root.emplace_back("robot_object_count", diag.robot_object_count);
	root.emplace_back("robot_state_hash", diag.robot_state_hash);
	root.emplace_back("robot_object_bucket_hashes", diag.robot_object_bucket_hashes);
	root.emplace_back("robot_changed_obj", diag.robot_changed_obj);
	root.emplace_back("robot_changed_sig", diag.robot_changed_sig);
	root.emplace_back("robot_changed_id", diag.robot_changed_id);
	root.emplace_back("robot_changed_bucket", diag.robot_changed_bucket);
	root.emplace_back("robot_changed_prev_hash", diag.robot_changed_prev_hash);
	root.emplace_back("robot_changed_hash", diag.robot_changed_hash);
	root.emplace_back("robot_changed_type", diag.robot_changed_type);
	root.emplace_back("robot_changed_seg", diag.robot_changed_seg);
	root.emplace_back("robot_changed_control", diag.robot_changed_control);
	root.emplace_back("robot_changed_movement", diag.robot_changed_movement);
	root.emplace_back("robot_changed_render", diag.robot_changed_render);
	root.emplace_back("robot_changed_flags", diag.robot_changed_flags);
	root.emplace_back("robot_changed_x", diag.robot_changed_x);
	root.emplace_back("robot_changed_y", diag.robot_changed_y);
	root.emplace_back("robot_changed_z", diag.robot_changed_z);
	root.emplace_back("robot_changed_last_x", diag.robot_changed_last_x);
	root.emplace_back("robot_changed_last_y", diag.robot_changed_last_y);
	root.emplace_back("robot_changed_last_z", diag.robot_changed_last_z);
	root.emplace_back("robot_changed_vel_x", diag.robot_changed_vel_x);
	root.emplace_back("robot_changed_vel_y", diag.robot_changed_vel_y);
	root.emplace_back("robot_changed_vel_z", diag.robot_changed_vel_z);
	root.emplace_back("robot_changed_rotvel_x", diag.robot_changed_rotvel_x);
	root.emplace_back("robot_changed_rotvel_y", diag.robot_changed_rotvel_y);
	root.emplace_back("robot_changed_rotvel_z", diag.robot_changed_rotvel_z);
	root.emplace_back("robot_changed_model", diag.robot_changed_model);
	root.emplace_back("robot_changed_subobj_flags", diag.robot_changed_subobj_flags);
	root.emplace_back("robot_sample_obj", diag.robot_sample_obj);
	root.emplace_back("robot_sample_sig", diag.robot_sample_sig);
	root.emplace_back("robot_sample_id", diag.robot_sample_id);
	root.emplace_back("robot_sample_seg", diag.robot_sample_seg);
	root.emplace_back("robot_sample_model", diag.robot_sample_model);
	root.emplace_back("robot_sample_subobj_flags", diag.robot_sample_subobj_flags);
	root.emplace_back("robot_sample_behavior", diag.robot_sample_behavior);
	root.emplace_back("robot_sample_mode", diag.robot_sample_mode);
	root.emplace_back("robot_sample_cur_state", diag.robot_sample_cur_state);
	root.emplace_back("robot_sample_goal_state", diag.robot_sample_goal_state);
	root.emplace_back("robot_sample_anim_at_goal", diag.robot_sample_anim_at_goal);
	root.emplace_back("robot_sample_anim_angles_hash", diag.robot_sample_anim_angles_hash);
	root.emplace_back("robot_sample_goal_angles_hash", diag.robot_sample_goal_angles_hash);
	root.emplace_back("robot_sample_delta_angles_hash", diag.robot_sample_delta_angles_hash);
	root.emplace_back("robot_sample_goal_state_hash", diag.robot_sample_goal_state_hash);
	root.emplace_back("robot_sample_achieved_state_hash", diag.robot_sample_achieved_state_hash);
	root.emplace_back("robot_sample_goal_seg", diag.robot_sample_goal_seg);
	root.emplace_back("robot_sample_hide_index", diag.robot_sample_hide_index);
	root.emplace_back("robot_sample_path_dir", diag.robot_sample_path_dir);
	root.emplace_back("robot_sample_prev_vis", diag.robot_sample_prev_vis);
	root.emplace_back("robot_sample_aware", diag.robot_sample_aware);
	root.emplace_back("robot_sample_aware_time", diag.robot_sample_aware_time);
	root.emplace_back("robot_sample_since", diag.robot_sample_since);
	root.emplace_back("robot_sample_next_action", diag.robot_sample_next_action);
	root.emplace_back("robot_sample_retry", diag.robot_sample_retry);
	root.emplace_back("robot_sample_retry_chain", diag.robot_sample_retry_chain);
	root.emplace_back("robot_sample_path_index", diag.robot_sample_path_index);
	root.emplace_back("robot_sample_path_length", diag.robot_sample_path_length);
	root.emplace_back("robot_sample_phys_flags", diag.robot_sample_phys_flags);
	root.emplace_back("robot_sample_vel_x", diag.robot_sample_vel_x);
	root.emplace_back("robot_sample_vel_y", diag.robot_sample_vel_y);
	root.emplace_back("robot_sample_vel_z", diag.robot_sample_vel_z);
	root.emplace_back("robot_sample_pos_x", diag.robot_sample_pos_x);
	root.emplace_back("robot_sample_pos_y", diag.robot_sample_pos_y);
	root.emplace_back("robot_sample_pos_z", diag.robot_sample_pos_z);
	root.emplace_back("robot_sample_goal_x", diag.robot_sample_goal_x);
	root.emplace_back("robot_sample_goal_y", diag.robot_sample_goal_y);
	root.emplace_back("robot_sample_goal_z", diag.robot_sample_goal_z);
	root.emplace_back("robot_sample_next_goal_x", diag.robot_sample_next_goal_x);
	root.emplace_back("robot_sample_next_goal_y", diag.robot_sample_next_goal_y);
	root.emplace_back("robot_sample_next_goal_z", diag.robot_sample_next_goal_z);
	root.emplace_back("robot_sample_mass", diag.robot_sample_mass);
	root.emplace_back("robot_sample_drag", diag.robot_sample_drag);
	root.emplace_back("robot_sample_brakes", diag.robot_sample_brakes);
	root.emplace_back("robot_sample_fvec_x", diag.robot_sample_fvec_x);
	root.emplace_back("robot_sample_fvec_y", diag.robot_sample_fvec_y);
	root.emplace_back("robot_sample_fvec_z", diag.robot_sample_fvec_z);
	root.emplace_back("robot_sample_rvec_x", diag.robot_sample_rvec_x);
	root.emplace_back("robot_sample_rvec_y", diag.robot_sample_rvec_y);
	root.emplace_back("robot_sample_rvec_z", diag.robot_sample_rvec_z);
	root.emplace_back("robot_sample_uvec_x", diag.robot_sample_uvec_x);
	root.emplace_back("robot_sample_uvec_y", diag.robot_sample_uvec_y);
	root.emplace_back("robot_sample_uvec_z", diag.robot_sample_uvec_z);
	root.emplace_back("robot_sample_orient_hash", diag.robot_sample_orient_hash);
	root.emplace_back("robot_sample_rotthrust_x", diag.robot_sample_rotthrust_x);
	root.emplace_back("robot_sample_rotthrust_y", diag.robot_sample_rotthrust_y);
	root.emplace_back("robot_sample_rotthrust_z", diag.robot_sample_rotthrust_z);
	root.emplace_back("robot_sample_rotvel_x", diag.robot_sample_rotvel_x);
	root.emplace_back("robot_sample_rotvel_y", diag.robot_sample_rotvel_y);
	root.emplace_back("robot_sample_rotvel_z", diag.robot_sample_rotvel_z);
	root.emplace_back("robot_ai_static_state_hash", diag.robot_ai_static_state_hash);
	root.emplace_back("robot_ai_static_bucket_hashes", diag.robot_ai_static_bucket_hashes);
	root.emplace_back("robot_ai_static_without_changed_hash", diag.robot_ai_static_without_changed_hash);
	root.emplace_back("robot_ai_static_changed_obj", diag.robot_ai_static_changed_obj);
	root.emplace_back("robot_ai_static_changed_sig", diag.robot_ai_static_changed_sig);
	root.emplace_back("robot_ai_static_changed_id", diag.robot_ai_static_changed_id);
	root.emplace_back("robot_ai_static_changed_prev_hash", diag.robot_ai_static_changed_prev_hash);
	root.emplace_back("robot_ai_static_changed_hash", diag.robot_ai_static_changed_hash);
	root.emplace_back("robot_ai_static_changed_behavior", diag.robot_ai_static_changed_behavior);
	root.emplace_back("robot_ai_static_changed_flags_hash", diag.robot_ai_static_changed_flags_hash);
	root.emplace_back("robot_ai_static_changed_current_gun", diag.robot_ai_static_changed_current_gun);
	root.emplace_back("robot_ai_static_changed_current_state", diag.robot_ai_static_changed_current_state);
	root.emplace_back("robot_ai_static_changed_goal_state", diag.robot_ai_static_changed_goal_state);
	root.emplace_back("robot_ai_static_changed_path_dir", diag.robot_ai_static_changed_path_dir);
	root.emplace_back("robot_ai_static_changed_submode", diag.robot_ai_static_changed_submode);
	root.emplace_back("robot_ai_static_changed_goalside", diag.robot_ai_static_changed_goalside);
	root.emplace_back("robot_ai_static_changed_skip_ai_count", diag.robot_ai_static_changed_skip_ai_count);
	root.emplace_back("robot_ai_static_changed_hide_segment", diag.robot_ai_static_changed_hide_segment);
	root.emplace_back("robot_ai_static_changed_hide_index", diag.robot_ai_static_changed_hide_index);
	root.emplace_back("robot_ai_static_changed_path_length", diag.robot_ai_static_changed_path_length);
	root.emplace_back("robot_ai_static_changed_cur_path_index", diag.robot_ai_static_changed_cur_path_index);
	root.emplace_back("robot_ai_static_changed_follow_start", diag.robot_ai_static_changed_follow_start);
	root.emplace_back("robot_ai_static_changed_follow_end", diag.robot_ai_static_changed_follow_end);
	root.emplace_back("robot_ai_static_changed_danger_laser_num", diag.robot_ai_static_changed_danger_laser_num);
	root.emplace_back("robot_ai_static_changed_danger_laser_sig", diag.robot_ai_static_changed_danger_laser_sig);
	root.emplace_back("robot_ai_static_trace_slots", diag.robot_ai_static_trace_slots);
	root.emplace_back("robot_ai_static_trace_sigs", diag.robot_ai_static_trace_sigs);
	root.emplace_back("robot_ai_static_trace_ids", diag.robot_ai_static_trace_ids);
	root.emplace_back("robot_ai_static_trace_hashes", diag.robot_ai_static_trace_hashes);
	root.emplace_back("robot_ai_static_trace_flags_hashes", diag.robot_ai_static_trace_flags_hashes);
	root.emplace_back("robot_ai_static_trace_current_guns", diag.robot_ai_static_trace_current_guns);
	root.emplace_back("robot_ai_static_trace_current_states", diag.robot_ai_static_trace_current_states);
	root.emplace_back("robot_ai_static_trace_goal_states", diag.robot_ai_static_trace_goal_states);
	root.emplace_back("robot_ai_static_trace_path_dirs", diag.robot_ai_static_trace_path_dirs);
	root.emplace_back("robot_ai_static_trace_submodes", diag.robot_ai_static_trace_submodes);
	root.emplace_back("robot_ai_static_trace_goalsides", diag.robot_ai_static_trace_goalsides);
	root.emplace_back("robot_ai_static_trace_skip_ai_counts", diag.robot_ai_static_trace_skip_ai_counts);
	root.emplace_back("robot_ai_static_trace_hide_segments", diag.robot_ai_static_trace_hide_segments);
	root.emplace_back("robot_ai_static_trace_hide_indexes", diag.robot_ai_static_trace_hide_indexes);
	root.emplace_back("robot_ai_static_trace_path_lengths", diag.robot_ai_static_trace_path_lengths);
	root.emplace_back("robot_ai_static_trace_cur_path_indexes", diag.robot_ai_static_trace_cur_path_indexes);
	root.emplace_back("robot_ai_static_trace_follow_starts", diag.robot_ai_static_trace_follow_starts);
	root.emplace_back("robot_ai_static_trace_follow_ends", diag.robot_ai_static_trace_follow_ends);
	root.emplace_back("robot_ai_static_trace_danger_nums", diag.robot_ai_static_trace_danger_nums);
	root.emplace_back("robot_ai_static_trace_danger_sigs", diag.robot_ai_static_trace_danger_sigs);
	root.emplace_back("robot_ai_local_state_hash", diag.robot_ai_local_state_hash);
	root.emplace_back("robot_ai_local_bucket_hashes", diag.robot_ai_local_bucket_hashes);
	root.emplace_back("robot_anim_pose_state_hash", diag.robot_anim_pose_state_hash);
	root.emplace_back("robot_anim_pose_bucket_hashes", diag.robot_anim_pose_bucket_hashes);
	root.emplace_back("robot_anim_pose_changed_obj", diag.robot_anim_pose_changed_obj);
	root.emplace_back("robot_anim_pose_changed_sig", diag.robot_anim_pose_changed_sig);
	root.emplace_back("robot_anim_pose_changed_id", diag.robot_anim_pose_changed_id);
	root.emplace_back("robot_anim_pose_changed_prev_hash", diag.robot_anim_pose_changed_prev_hash);
	root.emplace_back("robot_anim_pose_changed_hash", diag.robot_anim_pose_changed_hash);
	root.emplace_back("robot_anim_pose_changed_model", diag.robot_anim_pose_changed_model);
	root.emplace_back("robot_anim_pose_changed_subobj_flags", diag.robot_anim_pose_changed_subobj_flags);
	root.emplace_back("robot_anim_pose_changed_anim_angles_hash", diag.robot_anim_pose_changed_anim_angles_hash);
	root.emplace_back("robot_anim_pose_changed_goal_angles_hash", diag.robot_anim_pose_changed_goal_angles_hash);
	root.emplace_back("robot_anim_pose_changed_delta_angles_hash", diag.robot_anim_pose_changed_delta_angles_hash);
	root.emplace_back("robot_anim_pose_changed_goal_state_hash", diag.robot_anim_pose_changed_goal_state_hash);
	root.emplace_back("robot_anim_pose_changed_achieved_state_hash", diag.robot_anim_pose_changed_achieved_state_hash);
	root.emplace_back("robot_anim_pose_changed_current_gun", diag.robot_anim_pose_changed_current_gun);
	root.emplace_back("robot_anim_pose_changed_current_state", diag.robot_anim_pose_changed_current_state);
	root.emplace_back("robot_anim_pose_changed_goal_state", diag.robot_anim_pose_changed_goal_state);
	root.emplace_back("weapon_object_count", diag.weapon_object_count);
	root.emplace_back("weapon_state_hash", diag.weapon_state_hash);
	root.emplace_back("weapon_sample_obj", diag.weapon_sample_obj);
	root.emplace_back("weapon_sample_sig", diag.weapon_sample_sig);
	root.emplace_back("weapon_sample_id", diag.weapon_sample_id);
	root.emplace_back("weapon_sample_seg", diag.weapon_sample_seg);
	root.emplace_back("weapon_sample_control", diag.weapon_sample_control);
	root.emplace_back("weapon_sample_movement", diag.weapon_sample_movement);
	root.emplace_back("weapon_sample_render", diag.weapon_sample_render);
	root.emplace_back("weapon_sample_flags", diag.weapon_sample_flags);
	root.emplace_back("weapon_sample_phys_flags", diag.weapon_sample_phys_flags);
	root.emplace_back("weapon_sample_x", diag.weapon_sample_x);
	root.emplace_back("weapon_sample_y", diag.weapon_sample_y);
	root.emplace_back("weapon_sample_z", diag.weapon_sample_z);
	root.emplace_back("weapon_sample_last_x", diag.weapon_sample_last_x);
	root.emplace_back("weapon_sample_last_y", diag.weapon_sample_last_y);
	root.emplace_back("weapon_sample_last_z", diag.weapon_sample_last_z);
	root.emplace_back("weapon_sample_vel_x", diag.weapon_sample_vel_x);
	root.emplace_back("weapon_sample_vel_y", diag.weapon_sample_vel_y);
	root.emplace_back("weapon_sample_vel_z", diag.weapon_sample_vel_z);
	root.emplace_back("weapon_sample_size", diag.weapon_sample_size);
	root.emplace_back("weapon_sample_shields", diag.weapon_sample_shields);
	root.emplace_back("weapon_sample_lifeleft", diag.weapon_sample_lifeleft);
	root.emplace_back("weapon_sample_parent_type", diag.weapon_sample_parent_type);
	root.emplace_back("weapon_sample_parent_num", diag.weapon_sample_parent_num);
	root.emplace_back("weapon_sample_parent_sig", diag.weapon_sample_parent_sig);
	root.emplace_back("weapon_trace_slots", diag.weapon_trace_slots);
	root.emplace_back("weapon_trace_sigs", diag.weapon_trace_sigs);
	root.emplace_back("weapon_trace_ids", diag.weapon_trace_ids);
	root.emplace_back("weapon_trace_hashes", diag.weapon_trace_hashes);
	root.emplace_back("weapon_trace_segs", diag.weapon_trace_segs);
	root.emplace_back("weapon_trace_lifeleft", diag.weapon_trace_lifeleft);
	root.emplace_back("weapon_trace_track_goals", diag.weapon_trace_track_goals);
	root.emplace_back("weapon_trace_fvec_x", diag.weapon_trace_fvec_x);
	root.emplace_back("weapon_trace_fvec_y", diag.weapon_trace_fvec_y);
	root.emplace_back("weapon_trace_fvec_z", diag.weapon_trace_fvec_z);
	root.emplace_back("weapon_trace_vel_x", diag.weapon_trace_vel_x);
	root.emplace_back("weapon_trace_vel_y", diag.weapon_trace_vel_y);
	root.emplace_back("weapon_trace_vel_z", diag.weapon_trace_vel_z);
	root.emplace_back("fireball_object_count", diag.fireball_object_count);
	root.emplace_back("fireball_state_hash", diag.fireball_state_hash);
	root.emplace_back("fireball_changed_obj", diag.fireball_changed_obj);
	root.emplace_back("fireball_changed_sig", diag.fireball_changed_sig);
	root.emplace_back("fireball_changed_id", diag.fireball_changed_id);
	root.emplace_back("fireball_changed_bucket", diag.fireball_changed_bucket);
	root.emplace_back("fireball_changed_prev_hash", diag.fireball_changed_prev_hash);
	root.emplace_back("fireball_changed_hash", diag.fireball_changed_hash);
	root.emplace_back("fireball_changed_seg", diag.fireball_changed_seg);
	root.emplace_back("fireball_changed_control", diag.fireball_changed_control);
	root.emplace_back("fireball_changed_movement", diag.fireball_changed_movement);
	root.emplace_back("fireball_changed_render", diag.fireball_changed_render);
	root.emplace_back("fireball_changed_flags", diag.fireball_changed_flags);
	root.emplace_back("fireball_changed_x", diag.fireball_changed_x);
	root.emplace_back("fireball_changed_y", diag.fireball_changed_y);
	root.emplace_back("fireball_changed_z", diag.fireball_changed_z);
	root.emplace_back("fireball_changed_last_x", diag.fireball_changed_last_x);
	root.emplace_back("fireball_changed_last_y", diag.fireball_changed_last_y);
	root.emplace_back("fireball_changed_last_z", diag.fireball_changed_last_z);
	root.emplace_back("fireball_changed_size", diag.fireball_changed_size);
	root.emplace_back("fireball_changed_shields", diag.fireball_changed_shields);
	root.emplace_back("fireball_changed_lifeleft", diag.fireball_changed_lifeleft);
	root.emplace_back("fireball_sample_obj", diag.fireball_sample_obj);
	root.emplace_back("fireball_sample_sig", diag.fireball_sample_sig);
	root.emplace_back("fireball_sample_id", diag.fireball_sample_id);
	root.emplace_back("fireball_sample_hash", diag.fireball_sample_hash);
	root.emplace_back("fireball_sample_seg", diag.fireball_sample_seg);
	root.emplace_back("fireball_sample_control", diag.fireball_sample_control);
	root.emplace_back("fireball_sample_movement", diag.fireball_sample_movement);
	root.emplace_back("fireball_sample_render", diag.fireball_sample_render);
	root.emplace_back("fireball_sample_flags", diag.fireball_sample_flags);
	root.emplace_back("fireball_sample_x", diag.fireball_sample_x);
	root.emplace_back("fireball_sample_y", diag.fireball_sample_y);
	root.emplace_back("fireball_sample_z", diag.fireball_sample_z);
	root.emplace_back("fireball_sample_last_x", diag.fireball_sample_last_x);
	root.emplace_back("fireball_sample_last_y", diag.fireball_sample_last_y);
	root.emplace_back("fireball_sample_last_z", diag.fireball_sample_last_z);
	root.emplace_back("fireball_sample_size", diag.fireball_sample_size);
	root.emplace_back("fireball_sample_shields", diag.fireball_sample_shields);
	root.emplace_back("fireball_sample_lifeleft", diag.fireball_sample_lifeleft);
	root.emplace_back("fireball_sample_attached_obj", diag.fireball_sample_attached_obj);
	root.emplace_back("fireball_sample_spawn_time", diag.fireball_sample_spawn_time);
	root.emplace_back("fireball_sample_delete_time", diag.fireball_sample_delete_time);
	root.emplace_back("fireball_sample_delete_objnum", diag.fireball_sample_delete_objnum);
	root.emplace_back("fireball_sample_attach_parent", diag.fireball_sample_attach_parent);
	root.emplace_back("fireball_sample_prev_attach", diag.fireball_sample_prev_attach);
	root.emplace_back("fireball_sample_next_attach", diag.fireball_sample_next_attach);
	root.emplace_back("fireball_trace_slots", diag.fireball_trace_slots);
	root.emplace_back("fireball_trace_sigs", diag.fireball_trace_sigs);
	root.emplace_back("fireball_trace_ids", diag.fireball_trace_ids);
	root.emplace_back("fireball_trace_hashes", diag.fireball_trace_hashes);
	root.emplace_back("fireball_trace_segs", diag.fireball_trace_segs);
	root.emplace_back("fireball_trace_lifeleft", diag.fireball_trace_lifeleft);
	root.emplace_back("fireball_trace_delete_objnums", diag.fireball_trace_delete_objnums);
	root.emplace_back("fireball_trace_attached_objs", diag.fireball_trace_attached_objs);
	root.emplace_back("debris_object_count", diag.debris_object_count);
	root.emplace_back("debris_state_hash", diag.debris_state_hash);
	root.emplace_back("segment_object_list_count", diag.segment_object_list_count);
	root.emplace_back("segment_object_list_hash", diag.segment_object_list_hash);
	root.emplace_back("segment_object_link_error_count", diag.segment_object_link_error_count);
	root.emplace_back("segment_trace_segs", diag.segment_trace_segs);
	root.emplace_back("segment_trace_counts", diag.segment_trace_counts);
	root.emplace_back("segment_trace_hashes", diag.segment_trace_hashes);
	root.emplace_back("segment_trace_heads", diag.segment_trace_heads);
	root.emplace_back("segment_trace_objs", diag.segment_trace_objs);
	root.emplace_back("segment_trace_sigs", diag.segment_trace_sigs);
	root.emplace_back("segment_trace_types", diag.segment_trace_types);
	root.emplace_back("segment_trace_ids", diag.segment_trace_ids);
	root.emplace_back("segment_trace_prevs", diag.segment_trace_prevs);
	root.emplace_back("segment_trace_nexts", diag.segment_trace_nexts);
	root.emplace_back("player_weapon_obj0", diag.player_weapon_obj0);
	root.emplace_back("player_weapon_sig0", diag.player_weapon_sig0);
	root.emplace_back("player_weapon_id0", diag.player_weapon_id0);
	root.emplace_back("player_weapon_obj1", diag.player_weapon_obj1);
	root.emplace_back("player_weapon_sig1", diag.player_weapon_sig1);
	root.emplace_back("player_weapon_id1", diag.player_weapon_id1);
	root.emplace_back("player_weapon_obj2", diag.player_weapon_obj2);
	root.emplace_back("player_weapon_sig2", diag.player_weapon_sig2);
	root.emplace_back("player_weapon_id2", diag.player_weapon_id2);
	root.emplace_back("player_weapon_obj3", diag.player_weapon_obj3);
	root.emplace_back("player_weapon_sig3", diag.player_weapon_sig3);
	root.emplace_back("player_weapon_id3", diag.player_weapon_id3);
	root.emplace_back("ai_probe_skip_count", diag.ai_probe_skip_count);
	root.emplace_back("ai_probe_skip_obj", diag.ai_probe_skip_obj);
	root.emplace_back("ai_probe_skip_sig", diag.ai_probe_skip_sig);
	root.emplace_back("ai_probe_skip_id", diag.ai_probe_skip_id);
	root.emplace_back("ai_probe_timeslice_count", diag.ai_probe_timeslice_count);
	root.emplace_back("ai_probe_timeslice_obj", diag.ai_probe_timeslice_obj);
	root.emplace_back("ai_probe_timeslice_sig", diag.ai_probe_timeslice_sig);
	root.emplace_back("ai_probe_timeslice_id", diag.ai_probe_timeslice_id);
	root.emplace_back("ai_probe_process_count", diag.ai_probe_process_count);
	root.emplace_back("ai_probe_process_obj", diag.ai_probe_process_obj);
	root.emplace_back("ai_probe_process_sig", diag.ai_probe_process_sig);
	root.emplace_back("ai_probe_process_id", diag.ai_probe_process_id);
	root.emplace_back("ai_probe_phys_skip_count", diag.ai_probe_phys_skip_count);
	root.emplace_back("ai_probe_phys_skip_obj", diag.ai_probe_phys_skip_obj);
	root.emplace_back("ai_probe_phys_skip_sig", diag.ai_probe_phys_skip_sig);
	root.emplace_back("ai_probe_phys_skip_id", diag.ai_probe_phys_skip_id);
	root.emplace_back("ai_probe_phys_skip_before", diag.ai_probe_phys_skip_before);
	root.emplace_back("ai_probe_phys_skip_after", diag.ai_probe_phys_skip_after);
	return ordered_json(std::move(root));
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
	const size_t length = strlen(path);
	if (length >= 3 && strcmp(path + length - 3, ".gz") == 0)
		g_input_demo_state_trace_session.compressed_file = gzopen(path, "wb1");
	else
		g_input_demo_state_trace_session.file = fopen(path, "wb");
	if (!g_input_demo_state_trace_session.file && !g_input_demo_state_trace_session.compressed_file)
		return copy_error("could not open input demo state trace file", error, error_size);
	g_input_demo_state_trace_session.active = 1;
	snprintf(g_input_demo_state_trace_session.source,
	         sizeof(g_input_demo_state_trace_session.source),
	         "%s",
	         source);
	const ordered_json meta = {
		{ "type", "meta" }, { "version", 1 }, { "source", source }, { "game", game }, { "mission", mission }, { "level", level }, { "difficulty", difficulty }, { "start_mode", start_mode }, { "frame_count", frame_count }
	};
	if (!input_demo_state_trace_write_json(meta.dump().c_str(), error, error_size)) {
		reset_session();
		return 0;
	}
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
	if (!state)
		return copy_error("missing input demo state trace frame state", error, error_size);
	if (!input_demo_result_snapshot_to_json_buffer(state, state_json, sizeof(state_json)))
		return copy_error("could not encode input demo state trace frame state", error, error_size);
	ordered_json frame_json = {
		{ "type", "frame_state" }, { "source", g_input_demo_state_trace_session.source }, { "f", frame }, { "ft", frame_time }, { "rng", { { "s", rng_state } } }
	};
	if (has_rng_call_count)
		frame_json["rng"]["c"] = rng_call_count;
	if (diag)
		frame_json["diag"] = input_demo_state_trace_build_diag_json(*diag);
	frame_json["state"] = ordered_json::parse(state_json);
	return input_demo_state_trace_write_json(frame_json.dump().c_str(), error, error_size);
}

int input_demo_state_trace_write_json(const char *json, char *error, size_t error_size)
{
	auto &session = g_input_demo_state_trace_session;
	if (!session.active || !json)
		return copy_error("input demo state trace is not active or record is missing", error, error_size);
	std::string line(json);
	line += '\n';
	if (session.compressed_file) {
		if (gzwrite(session.compressed_file, line.data(), static_cast<unsigned>(line.size())) != static_cast<int>(line.size()) ||
		    gzflush(session.compressed_file, Z_SYNC_FLUSH) != Z_OK)
			return copy_error("could not write compressed input demo state trace", error, error_size);
	} else if (!session.file || fwrite(line.data(), 1, line.size(), session.file) != line.size() || fflush(session.file) != 0)
		return copy_error("could not write input demo state trace", error, error_size);
	return 1;
}
}
