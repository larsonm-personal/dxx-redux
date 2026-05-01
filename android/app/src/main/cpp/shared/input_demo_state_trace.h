#ifndef INPUT_DEMO_STATE_TRACE_H
#define INPUT_DEMO_STATE_TRACE_H

#include <stddef.h>
#include <stdint.h>

#include "input_demo_result.h"

#ifdef __cplusplus
extern "C" {
#endif

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
                                       const input_demo_result *state,
                                       char *error,
                                       size_t error_size);

#ifdef __cplusplus
}
#endif

#endif