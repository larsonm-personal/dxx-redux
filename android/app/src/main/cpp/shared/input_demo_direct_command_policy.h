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

/* This type crosses shared/engine translation-unit boundaries.  Engine
 * headers leave an outer pack(1) active, so define and restore its ABI
 * locally instead of inheriting the includer's packing state. */
#pragma pack(push, 8)
typedef struct input_demo_direct_command_policy {
	void *context;
	int unload_replay_on_failure;
	input_demo_direct_command_death_callback apply_death_abort;
	input_demo_direct_command_difficulty_callback change_difficulty;
	input_demo_direct_command_game_callback apply_game_specific;
} input_demo_direct_command_policy;
#pragma pack(pop)

#define INPUT_DEMO_POLICY_ASSERT(name, condition) \
	typedef char input_demo_policy_assert_##name[(condition) ? 1 : -1]
INPUT_DEMO_POLICY_ASSERT(function_pointer_size,
                         sizeof(input_demo_direct_command_death_callback) == sizeof(void *));
INPUT_DEMO_POLICY_ASSERT(context_offset,
                         offsetof(input_demo_direct_command_policy, context) == 0);
INPUT_DEMO_POLICY_ASSERT(flag_offset,
                         offsetof(input_demo_direct_command_policy, unload_replay_on_failure) == sizeof(void *));
INPUT_DEMO_POLICY_ASSERT(death_offset,
                         offsetof(input_demo_direct_command_policy, apply_death_abort) ==
                             (sizeof(void *) == 8 ? 16 : 8));
INPUT_DEMO_POLICY_ASSERT(difficulty_offset,
                         offsetof(input_demo_direct_command_policy, change_difficulty) ==
                             (sizeof(void *) == 8 ? 24 : 12));
INPUT_DEMO_POLICY_ASSERT(game_offset,
                         offsetof(input_demo_direct_command_policy, apply_game_specific) ==
                             (sizeof(void *) == 8 ? 32 : 16));
INPUT_DEMO_POLICY_ASSERT(total_size,
                         sizeof(input_demo_direct_command_policy) ==
                             (sizeof(void *) == 8 ? 40 : 20));
#undef INPUT_DEMO_POLICY_ASSERT

int input_demo_direct_command_apply_current_frame(
    const input_demo_direct_command_policy *policy,
    input_demo_direct_command_phase phase,
    char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
