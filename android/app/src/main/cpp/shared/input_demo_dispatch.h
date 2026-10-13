#ifndef INPUT_DEMO_DISPATCH_H
#define INPUT_DEMO_DISPATCH_H

#include <stddef.h>
#include <stdint.h>
#include "input_demo_controls.h"

#ifdef __cplusplus
extern "C" {
#endif

#define INPUT_DEMO_INPUT_OPERATION_MAX   1024u
#define INPUT_DEMO_INPUT_BATCH_MAX_BYTES (256u * 1024u)
#define INPUT_DEMO_INPUT_RESET           1
#define INPUT_DEMO_INPUT_SAMPLE          2
#define INPUT_DEMO_INPUT_COMMAND         3
#define INPUT_DEMO_INPUT_CLEAR           4

/* Shared C/C++ ABI must not inherit engine header packing */
#pragma pack(push, 4)
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_NONE                     0
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_GOAL            1
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_MARKER              2
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_CURRENT_WEAPON      3
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_SECONDARY_WEAPON    4
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_DROP_FLAG                5
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_ESCORT_RELEASE_CONTROL   6
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_DEATH_ABORT              7
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_CHANGE_DIFFICULTY        8
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_SPAWN           9
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_FIND_SECRET     10
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_WARP_TO_ME      11
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_FIND_UNEXPLORED 12
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_SELECT_WEAPON_EXACT      13
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_TEXT_SIZE                40

typedef struct input_demo_replay_direct_command_event {
	int32_t kind;
	int32_t value0;
	int32_t value1;
	char text[INPUT_DEMO_REPLAY_DIRECT_COMMAND_TEXT_SIZE];
} input_demo_replay_direct_command_event;

typedef struct input_demo_input_operation {
	int32_t kind;
	int32_t frame_time;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
	input_demo_replay_direct_command_event command;
	int32_t state_is_residual;
} input_demo_input_operation;

typedef struct input_demo_input_batch_info {
	uint32_t operation_count;
	uint32_t pre_rng_state;
	uint32_t pre_rng_count;
	uint32_t post_rng_state;
	uint32_t post_rng_count;
	int32_t is_tail;
	input_demo_control_state residual_state;
	input_demo_control_pulse residual_pulse;
} input_demo_input_batch_info;
#pragma pack(pop)

/* Verify the engine/C++ boundary even under enclosing pack(1) headers */
typedef char input_demo_operation_command_offset[(offsetof(input_demo_input_operation, command) == 52) ? 1 : -1];
typedef char input_demo_operation_size[(sizeof(input_demo_input_operation) == 108) ? 1 : -1];
typedef char input_demo_batch_residual_offset[(offsetof(input_demo_input_batch_info, residual_state) == 24) ? 1 : -1];
typedef char input_demo_batch_size[(sizeof(input_demo_input_batch_info) == 68) ? 1 : -1];

#ifdef __cplusplus
}
#endif
#endif
