#ifndef INPUT_DEMO_DEBUG_LOGGING_H
#define INPUT_DEMO_DEBUG_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

// Stub function declarations - implementations use void* to avoid d1/d2 header includes
// These are designed to be called from d1/d2 code

void input_demo_debug_log_player_motion_state(const char *stage);
void input_demo_debug_log_warning_probe(const char *label, void *obj, int view_x, int view_y, int view_z, int near_center, int prev_danger_obj, int prev_danger_sig);
void input_demo_debug_log_replay_frame_state_mismatch(const void *replay_frame);
void input_demo_debug_log_result_state(const char *label);
void input_demo_debug_log_replay_fire_state(const char *label, int can_fire_laser);
void input_demo_debug_log_replay_energy_stage(const char *label);
void input_demo_debug_write_replay_frame_state_trace(const void *replay_frame);
void input_demo_debug_log_player_bump_probe(const char *step, void *obj0, void *obj1, const void *relative_velocity, const void *force, int scale_num, int scale_den, int damage_flag);
void input_demo_debug_log_player_robot_contact_probe(const char *step, void *playerobj, void *robot, const void *collision_point, int damage);
void input_demo_debug_log_weapon_robot_accept_seq(void *weapon, void *robot);
void input_demo_debug_log_weapon_robot_path_probe(const char *step, void *weapon, void *robot, const void *collision_point);
void input_demo_debug_log_weapon_robot_reason_probe(const char *step, void *weapon, void *robot, const void *collision_point);
void input_demo_debug_log_weapon_robot_collision_pose(const char *step, void *weapon, void *robot, void *collision_point, int damage, int robot_old_shields);
void input_demo_debug_log_weapon_robot_dispatch_probe(void *weapon, void *robot, const void *collision_point);
void input_demo_debug_log_ai_robot_state(const char *label, void *objp);
void input_demo_debug_log_fvi_weapon_robot_check(int p0, int p1, int thisobjnum, int objnum, int fudged_rad, int d);
void input_demo_debug_log_exploding_object_probe(const char *step, void *obj, int delay_time, int spawned_objnum);

#ifdef __cplusplus
}
#endif

#endif
