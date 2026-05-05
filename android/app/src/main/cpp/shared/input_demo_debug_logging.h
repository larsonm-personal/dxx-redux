#ifndef INPUT_DEMO_DEBUG_LOGGING_H
#define INPUT_DEMO_DEBUG_LOGGING_H

#if defined(__ANDROID__) && defined(NDEBUG)
#define INPUT_DEMO_DEBUG_LOGGING_AVAILABLE 0
#else
#define INPUT_DEMO_DEBUG_LOGGING_AVAILABLE 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if INPUT_DEMO_DEBUG_LOGGING_AVAILABLE

int input_demo_debug_is_enabled(void);
void input_demo_debug_set_enabled(int enabled);
void input_demo_debug_printf(const char *fmt, ...);
int input_demo_debug_record_probe_active(void);
int input_demo_debug_activity_probe_active(void);
int input_demo_debug_replay_probe_active(void);
int input_demo_debug_frame_in_range(unsigned int start_frame, unsigned int end_frame);
int input_demo_debug_activity_frame_in_range(unsigned int start_frame, unsigned int end_frame);
int input_demo_debug_replay_frame_in_range(unsigned int start_frame, unsigned int end_frame);
const char *input_demo_debug_activity_mode_name(void);
unsigned int input_demo_debug_frame_index(void);

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

#else

#define input_demo_debug_is_enabled()                                    0
#define input_demo_debug_set_enabled(enabled)                            ((void) (enabled))
#define input_demo_debug_printf(...)                                     ((void) 0)
#define input_demo_debug_record_probe_active()                           0
#define input_demo_debug_activity_probe_active()                         0
#define input_demo_debug_replay_probe_active()                           0
#define input_demo_debug_frame_in_range(start_frame, end_frame)          0
#define input_demo_debug_activity_frame_in_range(start_frame, end_frame) 0
#define input_demo_debug_replay_frame_in_range(start_frame, end_frame)   0
#define input_demo_debug_activity_mode_name()                            "none"
#define input_demo_debug_frame_index()                                   0u

static inline void input_demo_debug_log_player_motion_state(const char *stage)
{
	(void) stage;
}
static inline void input_demo_debug_log_warning_probe(const char *label, void *obj, int view_x, int view_y, int view_z, int near_center, int prev_danger_obj, int prev_danger_sig)
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
static inline void input_demo_debug_log_replay_frame_state_mismatch(const void *replay_frame)
{
	(void) replay_frame;
}
static inline void input_demo_debug_log_result_state(const char *label)
{
	(void) label;
}
static inline void input_demo_debug_log_replay_fire_state(const char *label, int can_fire_laser)
{
	(void) label;
	(void) can_fire_laser;
}
static inline void input_demo_debug_log_replay_energy_stage(const char *label)
{
	(void) label;
}
static inline void input_demo_debug_write_replay_frame_state_trace(const void *replay_frame)
{
	(void) replay_frame;
}
static inline void input_demo_debug_log_player_bump_probe(const char *step, void *obj0, void *obj1, const void *relative_velocity, const void *force, int scale_num, int scale_den, int damage_flag)
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
static inline void input_demo_debug_log_player_robot_contact_probe(const char *step, void *playerobj, void *robot, const void *collision_point, int damage)
{
	(void) step;
	(void) playerobj;
	(void) robot;
	(void) collision_point;
	(void) damage;
}
static inline void input_demo_debug_log_weapon_robot_accept_seq(void *weapon, void *robot)
{
	(void) weapon;
	(void) robot;
}
static inline void input_demo_debug_log_weapon_robot_path_probe(const char *step, void *weapon, void *robot, const void *collision_point)
{
	(void) step;
	(void) weapon;
	(void) robot;
	(void) collision_point;
}
static inline void input_demo_debug_log_weapon_robot_reason_probe(const char *step, void *weapon, void *robot, const void *collision_point)
{
	(void) step;
	(void) weapon;
	(void) robot;
	(void) collision_point;
}
static inline void input_demo_debug_log_weapon_robot_collision_pose(const char *step, void *weapon, void *robot, void *collision_point, int damage, int robot_old_shields)
{
	(void) step;
	(void) weapon;
	(void) robot;
	(void) collision_point;
	(void) damage;
	(void) robot_old_shields;
}
static inline void input_demo_debug_log_weapon_robot_dispatch_probe(void *weapon, void *robot, const void *collision_point)
{
	(void) weapon;
	(void) robot;
	(void) collision_point;
}
static inline void input_demo_debug_log_ai_robot_state(const char *label, void *objp)
{
	(void) label;
	(void) objp;
}
static inline void input_demo_debug_log_fvi_weapon_robot_check(int p0, int p1, int thisobjnum, int objnum, int fudged_rad, int d)
{
	(void) p0;
	(void) p1;
	(void) thisobjnum;
	(void) objnum;
	(void) fudged_rad;
	(void) d;
}
static inline void input_demo_debug_log_exploding_object_probe(const char *step, void *obj, int delay_time, int spawned_objnum)
{
	(void) step;
	(void) obj;
	(void) delay_time;
	(void) spawned_objnum;
}

#endif

#ifdef __cplusplus
}
#endif

#endif
