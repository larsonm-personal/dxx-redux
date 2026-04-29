#ifndef INPUT_DEMO_RNG_TRACE_H
#define INPUT_DEMO_RNG_TRACE_H

#include <stddef.h>
#include <stdint.h>

#define INPUT_DEMO_RNG_TRACE_SUFFIX ".rngtrace.jsonl"

#ifdef __cplusplus
extern "C" {
#endif

void input_demo_rng_trace_start(void);
void input_demo_rng_trace_reset(void);
int input_demo_rng_trace_is_active(void);
void input_demo_rng_trace_set_context(uint32_t frame, int64_t game_time64);
void input_demo_rng_trace_record_rand(const char *file,
                                      const char *func,
                                      int line,
                                      uint32_t call_count,
                                      int has_state_before,
                                      uint32_t state_before,
                                      int has_state_after,
                                      uint32_t state_after,
                                      int result);
void input_demo_rng_trace_record_srand(const char *file,
                                       const char *func,
                                       int line,
                                       uint32_t call_count,
                                       int has_state_before,
                                       uint32_t state_before,
                                       int has_state_after,
                                       uint32_t state_after,
                                       uint32_t seed);
size_t input_demo_rng_trace_event_count(void);
int input_demo_rng_trace_write_sidecar_for_demo(const char *demo_path,
                                                char *error,
                                                size_t error_size);

#ifdef __cplusplus
}
#endif

#endif