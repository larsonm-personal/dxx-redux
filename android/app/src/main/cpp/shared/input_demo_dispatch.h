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
#define INPUT_DEMO_INPUT_CONTROLS        5
#define INPUT_DEMO_INPUT_AUTOMAP         6
#define INPUT_DEMO_INPUT_REAR_VIEW       7
#define INPUT_DEMO_INPUT_COCKPIT         8
#define INPUT_DEMO_REAR_VIEW_ENTER       1
#define INPUT_DEMO_REAR_VIEW_TOGGLE_OFF  2
#define INPUT_DEMO_REAR_VIEW_ARM         3
#define INPUT_DEMO_REAR_VIEW_RELEASE_OFF 4
#define INPUT_DEMO_REAR_VIEW_RESET       5
#define INPUT_DEMO_AUTOMAP_BEGIN         1
#define INPUT_DEMO_AUTOMAP_DEPTHS        2
#define INPUT_DEMO_AUTOMAP_ACTIVE        3
#define INPUT_DEMO_AUTOMAP_CLOSE         4

/* Closed is zero; failed partial openings may close before activation */
static inline int input_demo_automap_next_phase(int current, int phase)
{
	if (phase == INPUT_DEMO_AUTOMAP_BEGIN && current == 0) return phase;
	if (phase == INPUT_DEMO_AUTOMAP_DEPTHS && current == INPUT_DEMO_AUTOMAP_BEGIN) return phase;
	if (phase == INPUT_DEMO_AUTOMAP_ACTIVE && current == INPUT_DEMO_AUTOMAP_DEPTHS) return phase;
	if (phase == INPUT_DEMO_AUTOMAP_CLOSE && current >= INPUT_DEMO_AUTOMAP_BEGIN && current <= INPUT_DEMO_AUTOMAP_ACTIVE) return 0;
	return -1;
}

/* Wire key codes match d1/d2 arch/include/key.h; only digits accept Shift */
#define INPUT_DEMO_GUIDEBOT_KEY_FIRST   0x02
#define INPUT_DEMO_GUIDEBOT_KEY_LAST    0x0b
#define INPUT_DEMO_GUIDEBOT_KEY_RECALL  0x13
#define INPUT_DEMO_GUIDEBOT_KEY_SHIFTED 0x100
#define INPUT_DEMO_PLAYER_MARKER_COUNT  9

static inline int input_demo_guidebot_key_supported(int32_t key)
{
	const int32_t digit = key & ~INPUT_DEMO_GUIDEBOT_KEY_SHIFTED;
	return key == INPUT_DEMO_GUIDEBOT_KEY_RECALL ||
	       (digit >= INPUT_DEMO_GUIDEBOT_KEY_FIRST && digit <= INPUT_DEMO_GUIDEBOT_KEY_LAST);
}

static inline int input_demo_player_marker_supported(int32_t marker)
{
	return marker >= 0 && marker < INPUT_DEMO_PLAYER_MARKER_COUNT;
}

/* Ordinary F3/reverse cycles full, status-bar and full-screen cockpit modes */
static inline int input_demo_cockpit_mode_supported(int32_t mode)
{
	return mode == 0 || mode == 2 || mode == 3;
}

/* Shared C/C++ ABI must not inherit engine header packing */
#pragma pack(push, 4)
typedef struct input_demo_rear_view_baseline {
	int32_t active;
	int32_t leave_mode;
	int32_t current_cockpit;
	int32_t preferred_cockpit;
} input_demo_rear_view_baseline;

#define INPUT_DEMO_VIEW_DIMENSION_MAX 16384
#define INPUT_DEMO_VIEW_PIXELS_MAX    UINT32_C(67108864)
#define INPUT_DEMO_VIEW_SCALE_MAX     (16 * 65536)
/* Both native render.h files define MAX_RENDER_SEGS as 9000 */
#define INPUT_DEMO_VIEW_DEPTH_MAX 9000
typedef struct input_demo_view_projection {
	int32_t screen_width;
	int32_t screen_height;
	int32_t aspect;
	int32_t zoom;
	int32_t depth;
	int32_t classic_depth;
	int32_t missile_view_enabled;
} input_demo_view_projection;

static inline int input_demo_view_projection_supported(const input_demo_view_projection *state)
{
	return state && state->screen_width > 0 && state->screen_width <= INPUT_DEMO_VIEW_DIMENSION_MAX &&
	       state->screen_height > 0 && state->screen_height <= INPUT_DEMO_VIEW_DIMENSION_MAX &&
	       (int64_t) state->screen_width * state->screen_height <= INPUT_DEMO_VIEW_PIXELS_MAX &&
	       state->aspect > 0 && state->aspect <= INPUT_DEMO_VIEW_SCALE_MAX &&
	       state->zoom > 0 && state->zoom <= INPUT_DEMO_VIEW_SCALE_MAX &&
	       state->depth > 0 && state->depth <= INPUT_DEMO_VIEW_DEPTH_MAX &&
	       state->classic_depth >= 0 && state->classic_depth <= 1 &&
	       state->missile_view_enabled >= 0 && state->missile_view_enabled <= 1;
}

static inline int input_demo_rear_view_baseline_supported(const input_demo_rear_view_baseline *state)
{
	return state && state->active >= 0 && state->active <= 1 && state->leave_mode >= 0 && state->leave_mode <= 1 &&
	       state->current_cockpit >= 0 && state->current_cockpit <= 4 &&
	       state->preferred_cockpit >= 0 && state->preferred_cockpit <= 4;
}
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
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_DELETE_MARKER            14
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_ENABLE_CHEATS            15
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_GRANT_ALL_WEAPONS        16
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_CHEAT                    17
#define INPUT_DEMO_REPLAY_DIRECT_COMMAND_TEXT_SIZE                40

#define INPUT_DEMO_GRANT_ALL_WEAPONS_D1_SCOURGE   1
#define INPUT_DEMO_GRANT_ALL_WEAPONS_D1_BIGRED    2
#define INPUT_DEMO_GRANT_ALL_WEAPONS_D2_HONESTBOB 3

static inline int input_demo_all_weapons_grant_supported(int32_t grant)
{
	return grant == INPUT_DEMO_GRANT_ALL_WEAPONS_D1_SCOURGE ||
	       grant == INPUT_DEMO_GRANT_ALL_WEAPONS_D1_BIGRED ||
	       grant == INPUT_DEMO_GRANT_ALL_WEAPONS_D2_HONESTBOB;
}

/* D2 records D1-profile play too; native replay validates the active profile */
static inline int input_demo_all_weapons_grant_supported_for_game(int32_t grant, int game)
{
	return input_demo_all_weapons_grant_supported(grant) &&
	       (game == INPUT_DEMO_GAME_D2 ||
	        (game == INPUT_DEMO_GAME_D1 && grant != INPUT_DEMO_GRANT_ALL_WEAPONS_D2_HONESTBOB));
}

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
	int32_t automap_phase;
	int32_t rear_view_action;
	int32_t current_cockpit;
	int32_t preferred_cockpit;
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
typedef char input_demo_projection_size[(sizeof(input_demo_view_projection) == 28) ? 1 : -1];
typedef char input_demo_operation_size[(sizeof(input_demo_input_operation) == 124) ? 1 : -1];
typedef char input_demo_batch_residual_offset[(offsetof(input_demo_input_batch_info, residual_state) == 24) ? 1 : -1];
typedef char input_demo_batch_size[(sizeof(input_demo_input_batch_info) == 68) ? 1 : -1];

#ifdef __cplusplus
}
#endif
#endif
