#ifndef INPUT_DEMO_DIRECT_COMMAND_POLICY_H
#define INPUT_DEMO_DIRECT_COMMAND_POLICY_H

#include <stddef.h>

#include "input_demo_replay.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum input_demo_direct_command_phase {
	INPUT_DEMO_DIRECT_COMMAND_PHASE_DEAD = 0,
	INPUT_DEMO_DIRECT_COMMAND_PHASE_GAMEPLAY = 1
} input_demo_direct_command_phase;

typedef int (*input_demo_direct_command_death_callback)(void *context,
	int validate_only, char *error, size_t error_size);
typedef int (*input_demo_direct_command_difficulty_callback)(void *context,
	int difficulty, int validate_only, char *error, size_t error_size);
typedef int (*input_demo_direct_command_game_callback)(void *context,
	const input_demo_replay_direct_command_event *event,
	int validate_only, char *error, size_t error_size);

typedef struct input_demo_direct_command_policy {
	void *context;
	int unload_replay_on_failure;
	input_demo_direct_command_death_callback apply_death_abort;
	input_demo_direct_command_difficulty_callback change_difficulty;
	input_demo_direct_command_game_callback apply_game_specific;
} input_demo_direct_command_policy;

int input_demo_direct_command_apply_current_frame(
	const input_demo_direct_command_policy *policy,
	input_demo_direct_command_phase phase,
	char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
