#ifndef INPUT_DEMO_STATE_TRACE_H
#define INPUT_DEMO_STATE_TRACE_H

#include <stddef.h>
#include <stdint.h>

#include "input_demo_result.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	INPUT_DEMO_OBJECT_SLOT_BUCKET_BITS = 5,
	INPUT_DEMO_OBJECT_SLOT_BUCKET_SIZE = 1 << INPUT_DEMO_OBJECT_SLOT_BUCKET_BITS,
	INPUT_DEMO_OBJECT_SLOT_BUCKET_COUNT = 32
};

typedef struct input_demo_state_trace_diag {
	int32_t awareness_events;
	int32_t camera_awake_robots;
	int32_t danger_laser_robots;
	int32_t d_tick_count;
	uint32_t runtime_state_hash;
	int32_t object_allocator_num_objects;
	int32_t object_signature_seed;
	int32_t object_free_list_count;
	uint32_t object_free_list_hash;
	int32_t object_free_head0;
	int32_t object_free_head1;
	int32_t object_free_head2;
	int32_t object_free_head3;
	uint32_t object_homer_frame_count;
	int32_t object_current_homer_frame_time;
	int32_t object_do_homer_frame;
	int64_t weapon_next_laser_delta;
	int64_t weapon_next_missile_delta;
	int64_t weapon_last_laser_delta;
	int64_t weapon_next_flare_delta;
	int64_t weapon_auto_fusion_delta;
	int64_t weapon_last_omega_delta;
	int32_t weapon_global_laser_firing_count;
	int32_t weapon_global_missile_firing_count;
	int32_t weapon_fusion_charge;
	int32_t weapon_spreadfire_toggle;
	int32_t weapon_missile_gun;
	int32_t weapon_proximity_dropped;
	int32_t weapon_helix_orientation;
	int32_t weapon_smartmines_dropped;
	int32_t player_vel_x;
	int32_t player_vel_y;
	int32_t player_vel_z;
	int32_t player_last_x;
	int32_t player_last_y;
	int32_t player_last_z;
	int32_t player_weapon_count;
	uint32_t player_weapon_hash;
	int32_t highest_object_index;
	int32_t live_object_count;
	uint32_t live_object_hash;
	int32_t object_slot_bucket_size;
	int32_t object_slot_counts[INPUT_DEMO_OBJECT_SLOT_BUCKET_COUNT];
	uint32_t object_slot_hashes[INPUT_DEMO_OBJECT_SLOT_BUCKET_COUNT];
	int32_t object_focus_slot_base;
	uint32_t object_focus_slot_hashes[INPUT_DEMO_OBJECT_SLOT_BUCKET_SIZE];
	int32_t robot_object_count;
	uint32_t robot_state_hash;
	int32_t robot_changed_obj;
	int32_t robot_changed_sig;
	int32_t robot_changed_id;
	int32_t robot_changed_bucket;
	uint32_t robot_changed_prev_hash;
	uint32_t robot_changed_hash;
	int32_t robot_changed_type;
	int32_t robot_changed_seg;
	int32_t robot_changed_control;
	int32_t robot_changed_movement;
	int32_t robot_changed_render;
	int32_t robot_changed_flags;
	int32_t robot_changed_x;
	int32_t robot_changed_y;
	int32_t robot_changed_z;
	int32_t robot_changed_last_x;
	int32_t robot_changed_last_y;
	int32_t robot_changed_last_z;
	int32_t robot_changed_vel_x;
	int32_t robot_changed_vel_y;
	int32_t robot_changed_vel_z;
	int32_t robot_changed_rotvel_x;
	int32_t robot_changed_rotvel_y;
	int32_t robot_changed_rotvel_z;
	int32_t robot_changed_model;
	int32_t robot_changed_subobj_flags;
	int32_t robot_sample_obj;
	int32_t robot_sample_sig;
	int32_t robot_sample_id;
	int32_t robot_sample_seg;
	int32_t robot_sample_behavior;
	int32_t robot_sample_mode;
	int32_t robot_sample_cur_state;
	int32_t robot_sample_goal_state;
	int32_t robot_sample_goal_seg;
	int32_t robot_sample_hide_index;
	int32_t robot_sample_path_dir;
	int32_t robot_sample_prev_vis;
	int32_t robot_sample_aware;
	int32_t robot_sample_aware_time;
	int32_t robot_sample_since;
	int32_t robot_sample_next_action;
	int32_t robot_sample_retry;
	int32_t robot_sample_retry_chain;
	int32_t robot_sample_path_index;
	int32_t robot_sample_path_length;
	int32_t robot_sample_phys_flags;
	int32_t robot_sample_vel_x;
	int32_t robot_sample_vel_y;
	int32_t robot_sample_vel_z;
	int32_t robot_sample_pos_x;
	int32_t robot_sample_pos_y;
	int32_t robot_sample_pos_z;
	int32_t robot_sample_goal_x;
	int32_t robot_sample_goal_y;
	int32_t robot_sample_goal_z;
	int32_t robot_sample_next_goal_x;
	int32_t robot_sample_next_goal_y;
	int32_t robot_sample_next_goal_z;
	int32_t robot_sample_mass;
	int32_t robot_sample_drag;
	int32_t robot_sample_brakes;
	int32_t robot_sample_fvec_x;
	int32_t robot_sample_fvec_y;
	int32_t robot_sample_fvec_z;
	int32_t robot_sample_rotthrust_x;
	int32_t robot_sample_rotthrust_y;
	int32_t robot_sample_rotthrust_z;
	int32_t robot_sample_rotvel_x;
	int32_t robot_sample_rotvel_y;
	int32_t robot_sample_rotvel_z;
	uint32_t robot_ai_static_state_hash;
	uint32_t robot_ai_local_state_hash;
	int32_t weapon_object_count;
	uint32_t weapon_state_hash;
	int32_t weapon_sample_obj;
	int32_t weapon_sample_sig;
	int32_t weapon_sample_id;
	int32_t weapon_sample_seg;
	int32_t weapon_sample_control;
	int32_t weapon_sample_movement;
	int32_t weapon_sample_render;
	int32_t weapon_sample_flags;
	int32_t weapon_sample_phys_flags;
	int32_t weapon_sample_x;
	int32_t weapon_sample_y;
	int32_t weapon_sample_z;
	int32_t weapon_sample_last_x;
	int32_t weapon_sample_last_y;
	int32_t weapon_sample_last_z;
	int32_t weapon_sample_vel_x;
	int32_t weapon_sample_vel_y;
	int32_t weapon_sample_vel_z;
	int32_t weapon_sample_size;
	int32_t weapon_sample_shields;
	int32_t weapon_sample_lifeleft;
	int32_t weapon_sample_parent_type;
	int32_t weapon_sample_parent_num;
	int32_t weapon_sample_parent_sig;
	int32_t fireball_object_count;
	uint32_t fireball_state_hash;
	int32_t debris_object_count;
	uint32_t debris_state_hash;
	int32_t segment_object_list_count;
	uint32_t segment_object_list_hash;
	int32_t segment_object_link_error_count;
	int32_t player_weapon_obj0;
	int32_t player_weapon_sig0;
	int32_t player_weapon_id0;
	int32_t player_weapon_obj1;
	int32_t player_weapon_sig1;
	int32_t player_weapon_id1;
	int32_t player_weapon_obj2;
	int32_t player_weapon_sig2;
	int32_t player_weapon_id2;
	int32_t player_weapon_obj3;
	int32_t player_weapon_sig3;
	int32_t player_weapon_id3;
	int32_t ai_probe_skip_count;
	int32_t ai_probe_skip_obj;
	int32_t ai_probe_skip_sig;
	int32_t ai_probe_skip_id;
	int32_t ai_probe_timeslice_count;
	int32_t ai_probe_timeslice_obj;
	int32_t ai_probe_timeslice_sig;
	int32_t ai_probe_timeslice_id;
	int32_t ai_probe_process_count;
	int32_t ai_probe_process_obj;
	int32_t ai_probe_process_sig;
	int32_t ai_probe_process_id;
	int32_t ai_probe_phys_skip_count;
	int32_t ai_probe_phys_skip_obj;
	int32_t ai_probe_phys_skip_sig;
	int32_t ai_probe_phys_skip_id;
	int32_t ai_probe_phys_skip_before;
	int32_t ai_probe_phys_skip_after;
} input_demo_state_trace_diag;

int input_demo_state_trace_is_active(void);
void input_demo_state_trace_stop(void);
int input_demo_state_trace_start(const char *path,
                                 const char *source,
                                 const char *game,
                                 const char *mission,
                                 int level,
                                 int difficulty,
                                 const char *start_mode,
                                 uint32_t frame_count,
                                 char *error,
                                 size_t error_size);
int input_demo_state_trace_start_replay(const char *path,
                                        char *error,
                                        size_t error_size);
int input_demo_state_trace_write_frame(uint32_t frame,
                                       int32_t frame_time,
                                       uint32_t rng_state,
                                       int has_rng_call_count,
                                       uint32_t rng_call_count,
                                       const input_demo_state_trace_diag *diag,
                                       const input_demo_result *state,
                                       char *error,
                                       size_t error_size);

#ifdef __cplusplus
}

#include <string>

bool input_demo_state_trace_diag_to_json_text(const input_demo_state_trace_diag *diag,
                                              std::string *json_text,
                                              std::string *error);
#endif

#endif
