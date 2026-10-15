#ifndef INPUT_DEMO_REPLAY_H
#define INPUT_DEMO_REPLAY_H

#include <stddef.h>
#include <stdint.h>

#include "input_demo_fixture.h"
#include "input_demo_controls.h"
#include "input_demo_dispatch.h"
#include "input_demo_result.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
#pragma pack(push, 4)
#endif

typedef struct input_demo_replay_frame {
	uint32_t frame;
	int32_t frame_time;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	/* Keep the trailing RNG fields on an explicit boundary shared by C and C++ builds */
	uint32_t rng_state;
	int32_t has_rng_call_count;
	uint32_t rng_call_count;
	int32_t has_state;
	input_demo_result state_result;
} input_demo_replay_frame;

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
#pragma pack(pop)
#endif

void input_demo_replay_frame_clear(input_demo_replay_frame *frame);
void input_demo_replay_direct_command_event_clear(input_demo_replay_direct_command_event *event);
int input_demo_replay_format_version(void);
int input_demo_replay_get_input_batch_info(uint32_t frame_index, input_demo_input_batch_info *info,
                                           char *error, size_t error_size);
int input_demo_replay_get_input_operation(uint32_t frame_index, uint32_t operation_index,
                                          input_demo_input_operation *operation, char *error, size_t error_size);
int input_demo_replay_is_loaded(void);
void input_demo_replay_unload(void);
int input_demo_replay_load(const char *demo_path, char *error, size_t error_size);
int input_demo_replay_is_finished(void);
uint32_t input_demo_replay_frame_count(void);
uint32_t input_demo_replay_next_frame_index(void);
int64_t input_demo_replay_final_game_time64(void);
int input_demo_replay_game(void);
const char *input_demo_replay_mission(void);
int input_demo_replay_level(void);
int input_demo_replay_difficulty(void);
const char *input_demo_replay_start_mode(void);
const char *input_demo_replay_asset_fingerprints(void);
const char *input_demo_replay_rng_mode(void);
const char *input_demo_replay_actual_result_path(void);
void input_demo_replay_set_actual_result_path(const char *path);
const input_demo_level_start *input_demo_replay_level_start(void);
/* Last terminal write verdict survives unload, but every load attempt clears it */
int input_demo_replay_actual_result_written(void);
int input_demo_replay_write_actual_result(const input_demo_result *result,
                                          char *error, size_t error_size);
int input_demo_replay_has_player_cfg(void);
int input_demo_replay_get_player_cfg(input_demo_player_cfg *player_cfg);
int input_demo_replay_get_rear_view_baseline(input_demo_rear_view_baseline *state);
int input_demo_replay_get_view_projection(input_demo_view_projection *state);
int input_demo_replay_has_checkpoint(void);
const char *input_demo_replay_checkpoint_save_name(void);
const uint8_t *input_demo_replay_checkpoint_data(void);
size_t input_demo_replay_checkpoint_size(void);
int64_t input_demo_replay_checkpoint_start_gt(void);
int input_demo_replay_get_checkpoint_collision_delay_last_play_time(int64_t *last_play_time);
int input_demo_replay_get_legacy_fx_rng_seed(uint32_t *state,
                                             uint32_t *call_count);
int input_demo_replay_get_checkpoint_escort_state(input_demo_checkpoint_escort_state *escort_state);
int input_demo_replay_get_checkpoint_thief_state(input_demo_checkpoint_thief_state *thief_state);
int input_demo_replay_get_expected_result(input_demo_result *result,
                                          char *error, size_t error_size);
int input_demo_replay_compare_result(const input_demo_result *actual,
                                     char *error, size_t error_size);
int input_demo_replay_get_current_frame_direct_command_count(uint32_t *count,
                                                             char *error, size_t error_size);
int input_demo_replay_get_current_frame_direct_command_event(uint32_t direct_command_index,
                                                             input_demo_replay_direct_command_event *event,
                                                             char *error, size_t error_size);
int input_demo_replay_get_current_frame(input_demo_replay_frame *frame,
                                        char *error, size_t error_size);
int input_demo_replay_get_next_frame(input_demo_replay_frame *frame,
                                     char *error, size_t error_size);
int input_demo_replay_advance_frame(char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
