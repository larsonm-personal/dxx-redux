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

/* Engine headers select byte packing on MSVC; keep the recorder API layout stable */
#if defined(_MSC_VER)
#pragma pack(push, 8)
#endif
typedef struct input_demo_recorder_settings {
	int game;
	const char *mission;
	int level;
	int difficulty;
	const char *rng_mode;
	const input_demo_level_start *level_start;
	int has_player_cfg;
	input_demo_player_cfg player_cfg;
	const char *checkpoint_save_name;
	const uint8_t *checkpoint_data;
	size_t checkpoint_size;
	int has_checkpoint_start_gt;
	int64_t checkpoint_start_gt;
	int has_checkpoint_collision_delay_last_play_time;
	int64_t checkpoint_collision_delay_last_play_time;
	input_demo_checkpoint_escort_state checkpoint_escort_state;
	input_demo_checkpoint_thief_state checkpoint_thief_state;
	int record_per_frame_state;
	int ordered_input;
	int has_rear_view_baseline;
	input_demo_rear_view_baseline rear_view_baseline;
	int has_view_projection;
	input_demo_view_projection view_projection;
} input_demo_recorder_settings;
#if defined(_MSC_VER)
#pragma pack(pop)
#endif

void input_demo_recorder_settings_clear(input_demo_recorder_settings *settings);
int input_demo_recorder_is_active(void);
uint32_t input_demo_recorder_frame_count(void);
uint32_t input_demo_recorder_pending_input_operation_count(void);
int input_demo_recorder_get_view_projection(input_demo_view_projection *state);
uint32_t input_demo_recorder_diagnostic_frame_index(void);
int input_demo_recorder_truncate(uint32_t frame_count);
int input_demo_recorder_start(const input_demo_recorder_settings *settings,
                              char *error, size_t error_size);
void input_demo_recorder_cancel(void);
int input_demo_recorder_cancel_for_rewind(void);
int input_demo_recorder_capture_frame(int32_t frame_time,
                                      const input_demo_control_state *state,
                                      const input_demo_control_pulse *pulse,
                                      uint32_t rng_state,
                                      int has_rng_call_count,
                                      uint32_t rng_call_count,
                                      const input_demo_result *frame_state,
                                      const input_demo_state_trace_diag *frame_diag,
                                      char *error, size_t error_size);
int input_demo_recorder_uses_ordered_input(void);
int input_demo_recorder_begin_input_batch(uint32_t rng_state, uint32_t rng_count,
                                          int32_t input_frame_time, char *error, size_t error_size);
int input_demo_recorder_stage_input_reset(int32_t frame_time, char *error, size_t error_size);
int input_demo_recorder_stage_input_clear(int32_t frame_time, char *error, size_t error_size);
int input_demo_recorder_stage_input_automap(int32_t frame_time, int32_t phase,
                                            char *error, size_t error_size);
int input_demo_recorder_stage_input_rear_view(int32_t frame_time, int32_t action,
                                              char *error, size_t error_size);
int input_demo_recorder_stage_input_cockpit(int32_t frame_time, int32_t current_cockpit, int32_t preferred_cockpit,
                                            char *error, size_t error_size);
int input_demo_recorder_stage_input_controls(int32_t frame_time,
                                             const input_demo_control_state *state,
                                             const input_demo_control_pulse *pulse,
                                             char *error, size_t error_size);
int input_demo_recorder_stage_input_sample(int32_t frame_time,
                                           const input_demo_control_state *state,
                                           const input_demo_control_pulse *pulse,
                                           char *error, size_t error_size);
int input_demo_recorder_finish_input_tail(const input_demo_control_state *state,
                                          const input_demo_control_pulse *pulse,
                                          uint32_t rng_state, uint32_t rng_count,
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
int input_demo_recorder_stage_direct_command_delete_marker(int player_marker_num, int expected_signature,
                                                           char *error, size_t error_size);
int input_demo_recorder_stage_direct_command_drop_current_weapon(char *error,
                                                                 size_t error_size);
int input_demo_recorder_stage_direct_command_drop_secondary_weapon(char *error,
                                                                   size_t error_size);
int input_demo_recorder_stage_direct_command_drop_flag(char *error,
                                                       size_t error_size);
int input_demo_recorder_stage_direct_command_escort_release_control(char *error,
                                                                    size_t error_size);
int input_demo_recorder_stage_direct_command_guidebot_spawn(char *error,
                                                            size_t error_size);
int input_demo_recorder_stage_direct_command_guidebot_find_secret(char *error,
                                                                  size_t error_size);
int input_demo_recorder_stage_direct_command_guidebot_find_unexplored(char *error,
                                                                      size_t error_size);
int input_demo_recorder_stage_direct_command_guidebot_warp_to_me(char *error,
                                                                 size_t error_size);
int input_demo_recorder_stage_direct_command_death_abort(char *error,
                                                         size_t error_size);
int input_demo_recorder_stage_direct_command_change_difficulty(int difficulty,
                                                               char *error,
                                                               size_t error_size);
int input_demo_recorder_stage_direct_command_select_weapon_exact(int weapon_class,
                                                                 int weapon_index,
                                                                 char *error,
                                                                 size_t error_size);
int input_demo_recorder_stage_direct_command_enable_cheats(char *error, size_t error_size);
int input_demo_recorder_stage_direct_command_grant_all_weapons(int grant, char *error, size_t error_size);
int input_demo_recorder_flush_with_result(const char *demo_path,
                                          const input_demo_result *result,
                                          char *error, size_t error_size);
int input_demo_recorder_flush(const char *demo_path,
                              char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
