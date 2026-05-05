#ifndef D2X_INPUT_DEMO_HOOKS_H
#define D2X_INPUT_DEMO_HOOKS_H

typedef struct input_demo_result input_demo_result;
typedef struct input_demo_state_trace_diag input_demo_state_trace_diag;

#ifdef __cplusplus
extern "C" {
#endif

void input_demo_record_game_frame(void);
void input_demo_update_rng_trace_context(void);
void input_demo_capture_state_trace_diag(input_demo_state_trace_diag *diag);
void input_demo_capture_current_result(input_demo_result *result);
void input_demo_set_awareness_source(const char *source_tag, int source_objnum, int aux_objnum);
void input_demo_update_result_kills_baseline(void);
void input_demo_log_current_replay_frame_state_mismatch(void);
void input_demo_log_replay_energy_stage(const char *label);
int input_demo_prepare_replay_frame(void);
int input_demo_step_replay_frame(void);
void input_demo_advance_replay_frame(void);
void input_demo_finish_replay_without_close(void);
int input_demo_finish_replay_from_level_exit(void);
int input_demo_finish_replay_from_mine_exit(void);

#ifdef __cplusplus
}
#endif

#endif