#ifndef INPUT_DEMO_REPLAY_H
#define INPUT_DEMO_REPLAY_H

#include <stddef.h>
#include <stdint.h>

#include "input_demo_controls.h"
#include "input_demo_result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct input_demo_replay_frame {
	uint32_t frame;
	int32_t frame_time;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	uint32_t rng_state;
	uint8_t has_rng_call_count;
	uint32_t rng_call_count;
} input_demo_replay_frame;

void input_demo_replay_frame_clear(input_demo_replay_frame *frame);
int input_demo_replay_is_loaded(void);
void input_demo_replay_unload(void);
int input_demo_replay_load(const char *demo_path, char *error, size_t error_size);
int input_demo_replay_is_finished(void);
uint32_t input_demo_replay_frame_count(void);
uint32_t input_demo_replay_next_frame_index(void);
int input_demo_replay_game(void);
const char *input_demo_replay_mission(void);
int input_demo_replay_level(void);
int input_demo_replay_difficulty(void);
const char *input_demo_replay_start_mode(void);
const char *input_demo_replay_rng_mode(void);
const char *input_demo_replay_actual_result_path(void);
int input_demo_replay_compare_result(const input_demo_result *actual,
                                     char *error, size_t error_size);
int input_demo_replay_get_current_frame(input_demo_replay_frame *frame,
                                        char *error, size_t error_size);
int input_demo_replay_advance_frame(char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif