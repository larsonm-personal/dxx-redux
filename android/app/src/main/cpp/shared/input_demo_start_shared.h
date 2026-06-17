#ifndef INPUT_DEMO_START_SHARED_H
#define INPUT_DEMO_START_SHARED_H

#include <stddef.h>

#include "input_demo_replay.h"
#include "player.h"

typedef struct input_demo_replay_cmdline_options {
	const char *demo_path;
	const char *actual_result_path;
	const char *state_log_path;
	const char *rng_trace_path;
	int replay_labels_enabled;
	int allow_d1_in_d2;
} input_demo_replay_cmdline_options;

typedef struct input_demo_replay_loaded_context {
	input_demo_player_cfg replay_player_cfg;
	char local_player_callsign[CALLSIGN_LEN + 1];
	char mission_name[PATH_MAX];
	const char *start_mode;
	int have_replay_player_cfg;
} input_demo_replay_loaded_context;

typedef struct input_demo_replay_restored_player_diag {
	int player_cfg_result;
	int replay_auto_level;
	const char *replay_callsign;
	unsigned int primary_order_hash;
	unsigned int secondary_order_hash;
	fix player_mass;
	fix player_drag;
	fix player_brakes;
	unsigned int player_phys_flags;
	fix ship_mass;
	fix ship_drag;
	fix ship_brakes;
	fix ship_max_thrust;
	fix ship_max_rotthrust;
	fix ship_wiggle;
} input_demo_replay_restored_player_diag;

void input_demo_set_skip_level_intro(int skip);
int input_demo_consume_skip_level_intro(void);
int input_demo_maybe_validate_metadata_from_cmdline(void);
int input_demo_load_replay_from_path_common(const char *demo_path,
                                            int expected_game, const char *expected_game_name, char *error,
                                            size_t error_size);
int input_demo_load_replay_from_path_common_with_alternate(const char *demo_path,
                                                           int expected_game, int alternate_game,
                                                           const char *expected_game_name, char *error,
                                                           size_t error_size);
int input_demo_parse_replay_cmdline(input_demo_replay_cmdline_options *options);
int input_demo_apply_replay_common_setup(
    const input_demo_replay_cmdline_options *options, char *error,
    size_t error_size);
int input_demo_start_replay_state_trace_and_log_paths(
    const input_demo_replay_cmdline_options *options, char *error,
    size_t error_size);
int input_demo_start_loaded_replay_common(void);

#endif
