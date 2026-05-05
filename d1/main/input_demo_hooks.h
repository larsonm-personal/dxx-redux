#ifndef D1X_INPUT_DEMO_HOOKS_H
#define D1X_INPUT_DEMO_HOOKS_H

#include "input_demo_state_trace.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *input_demo_current_mission_id(void);
void input_demo_record_game_frame(void);
void input_demo_update_rng_trace_context(void);
void input_demo_capture_state_trace_diag(input_demo_state_trace_diag *diag);
void input_demo_capture_current_result(input_demo_result *result);
void input_demo_log_current_replay_frame_state_mismatch(void);

#ifdef __cplusplus
}
#endif

#endif