#ifndef INPUT_DEMO_STATE_TRACE_H
#define INPUT_DEMO_STATE_TRACE_H

#include <stddef.h>
#include <stdint.h>

#include "input_demo_result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct input_demo_state_trace_diag {
	int32_t awareness_events;
	int32_t camera_awake_robots;
	int32_t danger_laser_robots;
	int32_t d_tick_count;
	int32_t player_vel_x;
	int32_t player_vel_y;
	int32_t player_vel_z;
	int32_t player_last_x;
	int32_t player_last_y;
	int32_t player_last_z;
	int32_t ai_probe_skip_count;
	int32_t ai_probe_skip_obj;
	int32_t ai_probe_skip_sig;
	int32_t ai_probe_skip_id;
	int32_t ai_probe_timeslice_count;
	int32_t ai_probe_timeslice_obj;
	int32_t ai_probe_timeslice_sig;
	int32_t ai_probe_timeslice_id;
	int32_t ai_probe_process_count;
	int32_t ai_probe_process_obj;
	int32_t ai_probe_process_sig;
	int32_t ai_probe_process_id;
	int32_t ai_probe_phys_skip_count;
	int32_t ai_probe_phys_skip_obj;
	int32_t ai_probe_phys_skip_sig;
	int32_t ai_probe_phys_skip_id;
	int32_t ai_probe_phys_skip_before;
	int32_t ai_probe_phys_skip_after;
} input_demo_state_trace_diag;

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
                                       const input_demo_state_trace_diag *diag,
                                       const input_demo_result *state,
                                       char *error,
                                       size_t error_size);

#ifdef __cplusplus
}
#endif

#endif