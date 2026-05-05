#ifndef D1X_INPUT_DEMO_HOOKS_H
#define D1X_INPUT_DEMO_HOOKS_H

#include <stdint.h>

struct object;
struct vms_vector;

typedef struct input_demo_result input_demo_result;
typedef struct input_demo_state_trace_diag input_demo_state_trace_diag;

#ifdef __cplusplus
extern "C" {
#endif

const char *input_demo_current_mission_id(void);
void input_demo_record_game_frame(void);
void input_demo_update_rng_trace_context(void);
void input_demo_capture_state_trace_diag(input_demo_state_trace_diag *diag);
void input_demo_capture_current_result(input_demo_result *result);
void input_demo_log_player_bump_probe(const char *step, struct object *obj0, struct object *obj1,
	const struct vms_vector *relative_velocity, const struct vms_vector *float_force,
	int32_t scale_num, int32_t scale_den, int damage_flag);
void input_demo_log_current_replay_frame_state_mismatch(void);
int input_demo_prepare_replay_frame(void);
int input_demo_step_replay_frame(void);
void input_demo_advance_replay_frame(void);
void input_demo_finish_replay_without_close(void);
int input_demo_finish_replay_from_mine_exit(void);

#ifdef __cplusplus
}
#endif

#endif