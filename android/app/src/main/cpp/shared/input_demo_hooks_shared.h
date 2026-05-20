#ifndef INPUT_DEMO_HOOKS_SHARED_H
#define INPUT_DEMO_HOOKS_SHARED_H

#include "input_demo_replay.h"
#include "input_demo_result.h"
#include "input_demo_state_trace.h"
#include "object.h"

void input_demo_capture_runtime_state_diag(input_demo_state_trace_diag *diag);
void input_demo_capture_player_weapon_diag(input_demo_state_trace_diag *diag);
void input_demo_capture_object_state_diag(input_demo_state_trace_diag *diag);
void input_demo_capture_current_result(input_demo_result *result);
void input_demo_delay_replay_frame_shared(fix frame_time,
                                          fix64 *replay_last_timer_value);
int input_demo_prepare_replay_frame_shared(
    int *logged_state_mismatch,
    int *logged_state_trace_error,
    fix64 *replay_last_timer_value,
    void (*before_prepare_replay_frame)(void),
    void (*stop_replay)(int));
int input_demo_step_replay_frame_shared(
    int (*prepare_replay_frame)(void),
    int (*sync_replay_rng_to_current_frame)(void));
void input_demo_advance_replay_frame_shared(
    int (*before_advance_replay_frame)(void),
    void (*stop_replay)(int));
void input_demo_write_replay_frame_state_trace_shared(
    const input_demo_replay_frame *replay_frame,
    int *logged_state_trace_error);
void input_demo_log_replay_frame_state_mismatch_shared(
    const input_demo_replay_frame *replay_frame,
    int *logged_state_mismatch);
int input_demo_trace_collision_pose_active(void);
unsigned int input_demo_trace_collision_frame_index(void);
const char *input_demo_trace_collision_mode_name(void);
void input_demo_log_player_bump_probe(const char *step, object *obj0,
                                      object *obj1, const vms_vector *relative_velocity,
                                      const vms_vector *float_force, fix scale_num, fix scale_den,
                                      int damage_flag);

#endif
