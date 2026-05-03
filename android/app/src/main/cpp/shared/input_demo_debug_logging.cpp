#include "input_demo_debug_logging.h"

#ifdef ENABLE_INPUT_DEMO_DEBUG_LOGGING

extern void con_printf(int level, const char *fmt, ...);

void input_demo_debug_log_player_motion_state(const char *stage)
{
	if (stage) con_printf(0, "Input demo player motion: stage=%s\n", stage);
}

void input_demo_debug_log_warning_probe(const char *label, void *obj, int view_x, int view_y, int view_z, int near_center, int prev_danger_obj, int prev_danger_sig)
{
	if (label) con_printf(0, "Input demo warning probe: step=%s\n", label);
}

void input_demo_debug_log_replay_frame_state_mismatch(const void *replay_frame)
{
	con_printf(0, "Input demo replay state mismatch\n");
}

void input_demo_debug_log_result_state(const char *label)
{
	if (label) con_printf(0, "Input demo result state: label=%s\n", label);
}

void input_demo_debug_log_replay_fire_state(const char *label, int can_fire_laser)
{
	if (label) con_printf(0, "Input demo fire state: label=%s\n", label);
}

void input_demo_debug_log_replay_energy_stage(const char *label)
{
	if (label) con_printf(0, "Input demo energy stage: label=%s\n", label);
}

void input_demo_debug_write_replay_frame_state_trace(const void *replay_frame)
{
	con_printf(0, "Input demo state trace\n");
}

void input_demo_debug_log_player_bump_probe(const char *step, void *obj0, void *obj1, const void *relative_velocity, const void *force, int scale_num, int scale_den, int damage_flag)
{
	if (step) con_printf(0, "Input demo bump probe: step=%s\n", step);
}

void input_demo_debug_log_player_robot_contact_probe(const char *step, void *playerobj, void *robot, const void *collision_point, int damage)
{
	if (step) con_printf(0, "Input demo robot contact: step=%s\n", step);
}

void input_demo_debug_log_weapon_robot_accept_seq(void *weapon, void *robot)
{
	con_printf(0, "Input demo weapon accept\n");
}

void input_demo_debug_log_weapon_robot_path_probe(const char *step, void *weapon, void *robot, const void *collision_point)
{
	if (step) con_printf(0, "Input demo weapon path: step=%s\n", step);
}

void input_demo_debug_log_weapon_robot_reason_probe(const char *step, void *weapon, void *robot, const void *collision_point)
{
	if (step) con_printf(0, "Input demo weapon reason: step=%s\n", step);
}

void input_demo_debug_log_weapon_robot_collision_pose(const char *step, void *weapon, void *robot, void *collision_point, int damage, int robot_old_shields)
{
	if (step) con_printf(0, "Input demo collision pose: step=%s\n", step);
}

void input_demo_debug_log_weapon_robot_dispatch_probe(void *weapon, void *robot, const void *collision_point)
{
	con_printf(0, "Input demo dispatch probe\n");
}

void input_demo_debug_log_ai_robot_state(const char *label, void *objp)
{
	if (label) con_printf(0, "Input demo ai state: label=%s\n", label);
}

void input_demo_debug_log_fvi_weapon_robot_check(int p0, int p1, int thisobjnum, int objnum, int fudged_rad, int d)
{
	con_printf(0, "Input demo fvi check\n");
}

void input_demo_debug_log_exploding_object_probe(const char *step, void *obj, int delay_time, int spawned_objnum)
{
	if (step) con_printf(0, "Input demo exploding: step=%s\n", step);
}

#else

void input_demo_debug_log_player_motion_state(const char *stage)
{
	(void) stage;
}
void input_demo_debug_log_warning_probe(const char *label, void *obj, int view_x, int view_y, int view_z, int near_center, int prev_danger_obj, int prev_danger_sig)
{
	(void) label;
	(void) obj;
	(void) view_x;
	(void) view_y;
	(void) view_z;
	(void) near_center;
	(void) prev_danger_obj;
	(void) prev_danger_sig;
}
void input_demo_debug_log_replay_frame_state_mismatch(const void *replay_frame)
{
	(void) replay_frame;
}
void input_demo_debug_log_result_state(const char *label)
{
	(void) label;
}
void input_demo_debug_log_replay_fire_state(const char *label, int can_fire_laser)
{
	(void) label;
	(void) can_fire_laser;
}
void input_demo_debug_log_replay_energy_stage(const char *label)
{
	(void) label;
}
void input_demo_debug_write_replay_frame_state_trace(const void *replay_frame)
{
	(void) replay_frame;
}
void input_demo_debug_log_player_bump_probe(const char *step, void *obj0, void *obj1, const void *relative_velocity, const void *force, int scale_num, int scale_den, int damage_flag)
{
	(void) step;
	(void) obj0;
	(void) obj1;
	(void) relative_velocity;
	(void) force;
	(void) scale_num;
	(void) scale_den;
	(void) damage_flag;
}
void input_demo_debug_log_player_robot_contact_probe(const char *step, void *playerobj, void *robot, const void *collision_point, int damage)
{
	(void) step;
	(void) playerobj;
	(void) robot;
	(void) collision_point;
	(void) damage;
}
void input_demo_debug_log_weapon_robot_accept_seq(void *weapon, void *robot)
{
	(void) weapon;
	(void) robot;
}
void input_demo_debug_log_weapon_robot_path_probe(const char *step, void *weapon, void *robot, const void *collision_point)
{
	(void) step;
	(void) weapon;
	(void) robot;
	(void) collision_point;
}
void input_demo_debug_log_weapon_robot_reason_probe(const char *step, void *weapon, void *robot, const void *collision_point)
{
	(void) step;
	(void) weapon;
	(void) robot;
	(void) collision_point;
}
void input_demo_debug_log_weapon_robot_collision_pose(const char *step, void *weapon, void *robot, void *collision_point, int damage, int robot_old_shields)
{
	(void) step;
	(void) weapon;
	(void) robot;
	(void) collision_point;
	(void) damage;
	(void) robot_old_shields;
}
void input_demo_debug_log_weapon_robot_dispatch_probe(void *weapon, void *robot, const void *collision_point)
{
	(void) weapon;
	(void) robot;
	(void) collision_point;
}
void input_demo_debug_log_ai_robot_state(const char *label, void *objp)
{
	(void) label;
	(void) objp;
}
void input_demo_debug_log_fvi_weapon_robot_check(int p0, int p1, int thisobjnum, int objnum, int fudged_rad, int d)
{
	(void) p0;
	(void) p1;
	(void) thisobjnum;
	(void) objnum;
	(void) fudged_rad;
	(void) d;
}
void input_demo_debug_log_exploding_object_probe(const char *step, void *obj, int delay_time, int spawned_objnum)
{
	(void) step;
	(void) obj;
	(void) delay_time;
	(void) spawned_objnum;
}

#endif
