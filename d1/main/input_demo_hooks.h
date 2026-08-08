#ifndef D1X_INPUT_DEMO_HOOKS_H
#define D1X_INPUT_DEMO_HOOKS_H

#include <stdint.h>

#include "input_demo_direct_command_policy.h"

struct object;
struct vms_vector;

typedef struct fvi_info fvi_info;
typedef struct input_demo_result input_demo_result;
typedef struct input_demo_state_trace_diag input_demo_state_trace_diag;

#ifdef __cplusplus
extern "C" {
#endif

const char *input_demo_current_mission_id(void);
void input_demo_record_death_abort_direct_command(void);
void input_demo_record_direct_command_select_weapon_exact(int weapon_class,
	int weapon_index);
int input_demo_apply_replay_direct_commands(input_demo_direct_command_phase phase);
void input_demo_record_game_frame(void);
void input_demo_update_rng_trace_context(void);
void input_demo_capture_state_trace_diag(input_demo_state_trace_diag *diag);
void input_demo_capture_current_result(input_demo_result *result);
int input_demo_trace_collision_pose_active(void);
unsigned int input_demo_trace_collision_frame_index(void);
const char *input_demo_trace_collision_mode_name(void);
void input_demo_log_player_robot_contact_probe(const char *step, struct object *player, struct object *robot,
	const struct vms_vector *collision_point, int32_t damage);
void input_demo_log_weapon_robot_accept_seq(struct object *weapon, struct object *robot);
void input_demo_log_fvi_weapon_robot_check(const struct vms_vector *p0,
	const struct vms_vector *p1, int weapon_objnum, int robot_objnum,
	int32_t fudged_rad, int32_t d);
void input_demo_log_weapon_lifetime(const char *step, struct object *obj);
void input_demo_record_homing_state(const char *step, struct object *obj,
	int straight_time_active, int do_homer_frame, int track_goal_before,
	int track_goal_after, int dot, int32_t ideal_homer_frame_time,
	unsigned int homer_frame_count);
void input_demo_log_player_shot_create_probe(struct object *shooter,
	struct object *weapon, int laser_type, int gun_num, int harmless,
	int make_sound, const struct vms_vector *direction);
void input_demo_log_replay_collision_pair(const char *kind, struct object *obj0,
	struct object *obj1, const struct vms_vector *collision_point);
void input_demo_log_weapon_robot_path_probe(const char *step,
	struct object *weapon, struct object *robot,
	const struct vms_vector *collision_point);
void input_demo_log_robot_fire_probe(struct object *objp,
	const struct vms_vector *fire_vec, int weapon_type);
int input_demo_trace_ai_visibility_active(struct object *objp);
void input_demo_log_ai_visibility_probe(struct object *objp,
	const char *step_label, int previous_visibility,
	int raw_player_visibility, int final_player_visibility,
	int sight_sound_gate, int attack_sound_gate, int misc_sound_gate,
	const struct vms_vector *pos,
	const struct vms_vector *believed_player_pos);
void input_demo_log_ai_visibility_fvi_probe(struct object *objp,
	const char *step_label, int visibility_result, int hit_type,
	int hit_seg, int hit_object, int startseg, int flags, int32_t dot,
	int32_t field_of_view, const struct vms_vector *hit_pos,
	const struct vms_vector *pos,
	const struct vms_vector *believed_player_pos);
void input_demo_log_player_bump_probe(const char *step, struct object *obj0, struct object *obj1,
	const struct vms_vector *relative_velocity, const struct vms_vector *float_force,
	int32_t scale_num, int32_t scale_den, int damage_flag);
void input_demo_log_powerup_spawn_probe(struct object *source, struct object *created, int created_objnum);
void input_demo_log_replay_physics_fvi_fate(struct object *obj, int fate,
	const fvi_info *hit_info, const struct vms_vector *frame_vec,
	const struct vms_vector *new_pos, int32_t sim_time, int ignore_count);
void input_demo_log_player_robot_hit_object_probe(const char *step,
	struct object *moving_obj, int hit_object,
	const struct vms_vector *collision_point,
	const struct vms_vector *old_velocity, int ignore_count,
	int will_retry, int ignored_hit);
void input_demo_log_current_replay_frame_state_mismatch(void);
int input_demo_prepare_replay_frame(void);
int input_demo_step_replay_frame(void);
void input_demo_advance_replay_frame(void);
int input_demo_restore_checkpoint_object_links(void);
void input_demo_finish_replay_without_close(void);
int input_demo_finish_replay_from_game_over(void);
int input_demo_finish_replay_from_mine_exit(void);

#ifdef __cplusplus
}
#endif

#endif
