#ifndef INPUT_DEMO_RECORDER_H
#define INPUT_DEMO_RECORDER_H

#include <stddef.h>
#include <stdint.h>

#include "input_demo_fixture.h"
#include "input_demo_controls.h"
#include "input_demo_result.h"
#include "input_demo_state_trace.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct input_demo_recorder_settings {
	int game;
	const char *mission;
	int level;
	int difficulty;
	const char *rng_mode;
	int has_player_cfg;
	input_demo_player_cfg player_cfg;
	const char *checkpoint_save_name;
	const uint8_t *checkpoint_data;
	size_t checkpoint_size;
	int has_checkpoint_start_gt;
	int64_t checkpoint_start_gt;
	input_demo_checkpoint_escort_state checkpoint_escort_state;
	input_demo_checkpoint_thief_state checkpoint_thief_state;
	int record_per_frame_state;
} input_demo_recorder_settings;

void input_demo_recorder_settings_clear(input_demo_recorder_settings *settings);
int input_demo_recorder_is_active(void);
uint32_t input_demo_recorder_frame_count(void);
int input_demo_recorder_start(const input_demo_recorder_settings *settings,
                              char *error, size_t error_size);
void input_demo_recorder_cancel(void);
int input_demo_recorder_capture_frame(int32_t frame_time,
                                      const input_demo_control_state *state,
                                      const input_demo_control_pulse *pulse,
                                      uint32_t rng_state,
                                      int has_rng_call_count,
                                      uint32_t rng_call_count,
                                      const input_demo_result *frame_state,
                                      const input_demo_state_trace_diag *frame_diag,
                                      char *error, size_t error_size);
void input_demo_recorder_stage_pulse(const input_demo_control_pulse *pulse);
int input_demo_recorder_stage_frame_event_json(const char *json_text,
                                               char *error, size_t error_size);
int input_demo_recorder_append_frame_event_json(const char *json_text,
                                                char *error, size_t error_size);
int input_demo_recorder_stage_direct_command_guidebot_goal(int special_key,
                                                           int from_menu,
                                                           char *error, size_t error_size);
int input_demo_recorder_stage_direct_command_drop_marker(int player_marker_num,
                                                         const char *message,
                                                         char *error, size_t error_size);
int input_demo_recorder_stage_direct_command_drop_current_weapon(char *error,
                                                                 size_t error_size);
int input_demo_recorder_stage_direct_command_drop_secondary_weapon(char *error,
                                                                   size_t error_size);
int input_demo_recorder_stage_direct_command_drop_flag(char *error,
                                                       size_t error_size);
int input_demo_recorder_stage_direct_command_escort_release_control(char *error,
                                                                    size_t error_size);
int input_demo_recorder_flush_with_result(const char *demo_path,
                                          const input_demo_result *result,
                                          char *error, size_t error_size);
int input_demo_recorder_flush(const char *demo_path,
                              char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif