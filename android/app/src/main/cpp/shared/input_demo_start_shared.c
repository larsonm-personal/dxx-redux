#include <stdio.h>
#include <string.h>

#include "args.h"
#include "game.h"
#include "gameseq.h"
#include "input_demo_debug_logging.h"
#include "input_demo_replay.h"
#include "input_demo_rng_mode.h"
#include "input_demo_rng_trace.h"
#include "input_demo_start.h"
#include "input_demo_state_trace.h"
#include "mission.h"
#include "newmenu.h"
#include "object.h"
#include "physfsx.h"
#include "playsave.h"
#include "player.h"
#include "state.h"
#include "text.h"
#ifdef DXX_BUILD_DESCENT_II
#include "replay_debug_overlay.h"
#endif
#ifdef __ANDROID__
#include "android_crash_handler.h"
#define INPUT_DEMO_CRUMB(msg) crash_breadcrumb(msg)
#define INPUT_DEMO_CRUMB_V    crash_breadcrumb_v
#else
#define INPUT_DEMO_CRUMB(msg)   ((void) 0)
#define INPUT_DEMO_CRUMB_V(...) ((void) 0)
#endif

#include "input_demo_start_shared.h"

#ifdef DXX_BUILD_DESCENT_II
#include "d1_save_translate.h"
extern int state_restore_all_sub(char *filename, int multi);
#else
extern int state_restore_all_sub(char *filename);
#endif

static int input_demo_d1_in_d2_enabled = 0;

#if NDL != INPUT_DEMO_DIFFICULTY_LEVELS
#error Input demo difficulty domain must match the engine difficulty domain
#endif

static int input_demo_difficulty_is_valid(int difficulty)
{
	return difficulty >= 0 && difficulty < NDL;
}

static unsigned int input_demo_primary_order_copy_count(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return MAX_PRIMARY_WEAPONS + 1;
#else
	return MAX_PRIMARY_WEAPONS + 2;
#endif
}

static void input_demo_apply_extra_player_cfg(
    const input_demo_player_cfg *player_cfg)
{
#ifdef DXX_BUILD_DESCENT_II
	if (player_cfg->has_headlight_active_default)
		PlayerCfg.HeadlightActiveDefault =
		    player_cfg->headlight_active_default;
#else
	(void) player_cfg;
#endif
}

static void input_demo_apply_extra_replay_cmdline_options(
    input_demo_replay_cmdline_options *options)
{
#ifdef DXX_BUILD_DESCENT_II
	g_replay_robot_labels_enabled = options->replay_labels_enabled;
#else
	(void) options;
#endif
}

static void input_demo_format_replay_mission_name(const char *replay_mission,
                                                  char *mission_name, size_t mission_name_size)
{
#ifdef DXX_BUILD_DESCENT_II
	if (!d_stricmp(replay_mission, "d1"))
		snprintf(mission_name, mission_name_size, "%s", D1_MISSION_FILENAME);
	else
		snprintf(mission_name, mission_name_size, "%s", replay_mission);
#else
	if (!d_stricmp(replay_mission, "d1"))
		snprintf(mission_name, mission_name_size, "%s", D1_MISSION_FILENAME);
	else
		snprintf(mission_name, mission_name_size, "%s", replay_mission);
#endif
}

static int input_demo_restore_checkpoint_save(const char *path)
{
#ifdef DXX_BUILD_DESCENT_II
	return state_restore_all_sub((char *) path, 0);
#else
	return state_restore_all_sub((char *) path);
#endif
}

static void input_demo_capture_restored_player_diag(
    const char *local_player_callsign,
    const input_demo_player_cfg *replay_player_cfg,
    int have_replay_player_cfg,
    input_demo_replay_restored_player_diag *diag);

static void input_demo_log_restored_player_diag(
    const input_demo_replay_restored_player_diag *diag);

static int input_demo_skip_level_intro = 0;

static int input_demo_find_cmd_arg(const char *name)
{
	int i;

	for (i = 1; i < Num_args; ++i)
		if (!d_stricmp(Args[i], name))
			return i;

	return 0;
}

static const char *input_demo_cmd_arg_value(int arg_index, const char *name)
{
	if (arg_index + 1 >= Num_args || !Args[arg_index + 1] || Args[arg_index + 1][0] == '-') {
		printf("Missing value for %s\n", name);
		return NULL;
	}

	return Args[arg_index + 1];
}

static unsigned int input_demo_replay_hash_u8_sequence(const ubyte *values, int count)
{
	unsigned int hash = 2166136261u;
	int i;

	for (i = 0; i < count; i++) {
		hash ^= values[i];
		hash *= 16777619u;
	}
	return hash;
}

static void input_demo_apply_replay_player_cfg(const input_demo_player_cfg *player_cfg)
{
	if (!player_cfg)
		return;
	PlayerCfg.AutoLeveling = player_cfg->auto_leveling;
	PlayerCfg.PersistentDebris = player_cfg->persistent_debris;
	input_demo_apply_extra_player_cfg(player_cfg);
	PlayerCfg.NoFireAutoselect = player_cfg->no_fire_autoselect;
	PlayerCfg.CycleAutoselectOnly = player_cfg->cycle_autoselect_only;
	PlayerCfg.SelectAfterFire = player_cfg->select_after_fire;
	PlayerCfg.ClassicAutoselectWeapon = player_cfg->classic_autoselect_weapon;
	PlayerCfg.OriginalHoming = player_cfg->original_homing;
	memcpy(PlayerCfg.PrimaryOrder, player_cfg->primary_order,
	       input_demo_primary_order_copy_count());
	memcpy(PlayerCfg.SecondaryOrder, player_cfg->secondary_order,
	       MAX_SECONDARY_WEAPONS + 1);
}

static void input_demo_apply_legacy_replay_homing_default(void)
{
	PlayerCfg.OriginalHoming = 0;
}

void input_demo_set_skip_level_intro(int skip)
{
	input_demo_skip_level_intro = skip ? 1 : 0;
}

int input_demo_consume_skip_level_intro(void)
{
	int skip = input_demo_skip_level_intro;

	input_demo_skip_level_intro = 0;
	return skip;
}

int input_demo_maybe_validate_metadata_from_cmdline(void)
{
	int arg_index = input_demo_find_cmd_arg("-inputdemo-validate");
	int engine_mode;
	int demo_mode;
	const char *demo_path;
	const char *error;

	if (!arg_index)
		return -1;
	demo_path = input_demo_cmd_arg_value(arg_index, "-inputdemo-validate");
	if (!demo_path)
		return 1;
	engine_mode = d_rand_get_replay_mode();
	error = input_demo_rng_mode_validate_metadata_file(demo_path, engine_mode,
	                                                   &demo_mode);
	if (error) {
		printf("Input demo file invalid: %s\n", demo_path);
		printf("%s\n", error);
		printf("Active RNG backend expects: %s\n",
		       input_demo_rng_mode_name(engine_mode));
		return 1;
	}
	printf("Input demo file OK: %s\n", demo_path);
	printf("rng_mode: %s\n", input_demo_rng_mode_name(demo_mode));
	return 0;
}

static int input_demo_load_replay_checked(const char *demo_path,
                                          int expected_game,
                                          int alternate_game,
                                          const char *expected_game_name,
                                          char *error,
                                          size_t error_size)
{
	if (!input_demo_replay_load(demo_path, error, error_size))
		return 0;
	if (input_demo_replay_game() != expected_game &&
	    (!alternate_game || input_demo_replay_game() != alternate_game)) {
		input_demo_replay_unload();
		snprintf(error, error_size,
		         "Input demo replay currently supports %s demos only",
		         expected_game_name ? expected_game_name : "requested");
		return 0;
	}
	return 1;
}

int input_demo_load_replay_from_path_common_with_alternate(const char *demo_path,
                                                           int expected_game,
                                                           int alternate_game,
                                                           const char *expected_game_name,
                                                           char *error,
                                                           size_t error_size)
{
	int engine_mode;
	int demo_mode;
	const char *validation_error;

	if (!demo_path || !demo_path[0]) {
		snprintf(error, error_size, "%s", "missing demo path");
		return 0;
	}
	engine_mode = d_rand_get_replay_mode();
	validation_error = input_demo_rng_mode_validate_metadata_file(demo_path,
	                                                              engine_mode, &demo_mode);
	if (validation_error) {
		snprintf(error, error_size, "%s", validation_error);
		return 0;
	}
	return input_demo_load_replay_checked(demo_path, expected_game, alternate_game,
	                                      expected_game_name, error, error_size);
}

int input_demo_load_replay_from_path_common(const char *demo_path,
                                            int expected_game,
                                            const char *expected_game_name,
                                            char *error,
                                            size_t error_size)
{
	return input_demo_load_replay_from_path_common_with_alternate(
	    demo_path, expected_game, 0, expected_game_name, error, error_size);
}

int input_demo_parse_replay_cmdline(input_demo_replay_cmdline_options *options)
{
	int arg_index;
	int actual_result_arg_index;
	int replay_labels_arg_index;
	int debug_log_arg_index = 0;
	int state_log_arg_index;
	int rng_trace_arg_index;

	if (!options)
		return 1;
	options->demo_path = NULL;
	options->actual_result_path = NULL;
	options->state_log_path = NULL;
	options->rng_trace_path = NULL;
	options->replay_labels_enabled = 0;
	options->allow_d1_in_d2 = 0;
	options->d1_in_d2_start_from_level = 0;

	arg_index = input_demo_find_cmd_arg("-inputdemo-replay");
	actual_result_arg_index = input_demo_find_cmd_arg("-inputdemo-actual-result");
	replay_labels_arg_index = input_demo_find_cmd_arg("-inputdemo-replay-labels");
#if INPUT_DEMO_DEBUG_LOGGING_AVAILABLE
	debug_log_arg_index = input_demo_find_cmd_arg("-inputdemo-debug-log");
#endif
	state_log_arg_index = input_demo_find_cmd_arg("-inputdemo-state-log");
	rng_trace_arg_index = input_demo_find_cmd_arg("-inputdemo-rng-trace");

	input_demo_debug_set_enabled(debug_log_arg_index ? 1 : 0);
	options->replay_labels_enabled = replay_labels_arg_index ? 1 : 0;
	options->allow_d1_in_d2 = input_demo_find_cmd_arg("-inputdemo-d1-in-d2") ? 1 : 0;
	options->d1_in_d2_start_from_level =
	    input_demo_find_cmd_arg("-inputdemo-d1-in-d2-start-from-level") ? 1 : 0;
	input_demo_apply_extra_replay_cmdline_options(options);

	if (!arg_index)
		return -1;
	options->demo_path = input_demo_cmd_arg_value(arg_index, "-inputdemo-replay");
	if (!options->demo_path)
		return 1;
	if (actual_result_arg_index) {
		options->actual_result_path = input_demo_cmd_arg_value(
		    actual_result_arg_index, "-inputdemo-actual-result");
		if (!options->actual_result_path)
			return 1;
	}
	if (state_log_arg_index) {
		options->state_log_path = input_demo_cmd_arg_value(
		    state_log_arg_index, "-inputdemo-state-log");
		if (!options->state_log_path)
			return 1;
	}
	if (rng_trace_arg_index) {
		options->rng_trace_path = input_demo_cmd_arg_value(
		    rng_trace_arg_index, "-inputdemo-rng-trace");
		if (!options->rng_trace_path)
			return 1;
	}
	return 0;
}

int input_demo_apply_replay_common_setup(
    const input_demo_replay_cmdline_options *options,
    char *error,
    size_t error_size)
{
	if (!options)
		return 0;
	input_demo_d1_in_d2_enabled = options->allow_d1_in_d2 ? 1 : 0;
	if (options->actual_result_path)
		input_demo_replay_set_actual_result_path(options->actual_result_path);
	if (options->rng_trace_path && !input_demo_rng_trace_start_replay(
	                                   options->rng_trace_path, error, error_size)) {
		input_demo_replay_unload();
		return 0;
	}
	return 1;
}

int input_demo_start_replay_state_trace_and_log_paths(
    const input_demo_replay_cmdline_options *options,
    char *error,
    size_t error_size)
{
	const char *state_log_path;

	if (!options)
		return 0;
	state_log_path = options->state_log_path;
	if (state_log_path && !input_demo_state_trace_start_replay(state_log_path,
	                                                           error, error_size)) {
		printf("Input demo replay state trace start failed: %s\n", error);
		input_demo_replay_unload();
		return 0;
	}
	if (options->rng_trace_path)
		input_demo_debug_printf("Input demo replay rng trace: %s\n",
		                        options->rng_trace_path);
	if (state_log_path)
		input_demo_debug_printf("Input demo replay state trace: %s\n",
		                        state_log_path);
	if (options->actual_result_path)
		input_demo_debug_printf("Input demo replay actual result: %s\n",
		                        options->actual_result_path);
	return 1;
}

static int input_demo_prepare_loaded_replay_context(
    input_demo_replay_loaded_context *context)
{
	if (!context)
		return 0;
	memset(context, 0, sizeof(*context));
	if (Player_num >= 0 && Player_num < MAX_PLAYERS) {
		strncpy(context->local_player_callsign, Players[Player_num].callsign,
		        CALLSIGN_LEN);
		context->local_player_callsign[CALLSIGN_LEN] = '\0';
	}
	context->have_replay_player_cfg = input_demo_replay_get_player_cfg(
	    &context->replay_player_cfg);
	context->start_mode = input_demo_replay_start_mode();
	if (!context->start_mode) {
		printf("Input demo replay metadata is missing start_mode\n");
		input_demo_replay_unload();
		return 0;
	}
	if (!input_demo_replay_mission()) {
		printf("Input demo replay metadata is missing mission\n");
		input_demo_replay_unload();
		return 0;
	}
	input_demo_format_replay_mission_name(input_demo_replay_mission(),
	                                      context->mission_name, sizeof(context->mission_name));
	return 1;
}

static int input_demo_restore_replay_checkpoint_data(
    const char *checkpoint_name,
    const uint8_t *checkpoint_data,
    size_t checkpoint_size)
{
	char local_checkpoint_name[PATH_MAX] = "";
	PHYSFS_file *checkpoint_file = NULL;
	const char *checkpoint_base_name;

	checkpoint_base_name = strrchr(checkpoint_name, '/');
	if (checkpoint_base_name)
		checkpoint_base_name++;
	else
		checkpoint_base_name = checkpoint_name;
	if (GameArg.SysUsePlayersDir)
		snprintf(local_checkpoint_name, sizeof(local_checkpoint_name), "Players/%s",
		         checkpoint_base_name);
	else
		snprintf(local_checkpoint_name, sizeof(local_checkpoint_name), "%s",
		         checkpoint_base_name);
	input_demo_debug_printf(
	    "Input demo replay checkpoint temp path: recorded=%s local=%s\n",
	    checkpoint_name, local_checkpoint_name);
	INPUT_DEMO_CRUMB_V("input_demo: checkpoint temp=%s size=%u",
	                   local_checkpoint_name, (unsigned int) checkpoint_size);
	if (!strncmp(local_checkpoint_name, "Players/", 8))
		PHYSFS_mkdir("Players");
	PHYSFS_delete(local_checkpoint_name);
	checkpoint_file = PHYSFS_openWrite(local_checkpoint_name);
	if (!checkpoint_file) {
		printf("Input demo replay could not write checkpoint file: %s\n",
		       local_checkpoint_name);
		input_demo_replay_unload();
		return 0;
	}
	if (PHYSFS_writeBytes(checkpoint_file, checkpoint_data, checkpoint_size) !=
	    (PHYSFS_sint64) checkpoint_size) {
		PHYSFS_close(checkpoint_file);
		PHYSFS_delete(local_checkpoint_name);
		printf("Input demo replay could not write checkpoint bytes: %s\n",
		       local_checkpoint_name);
		input_demo_replay_unload();
		return 0;
	}
	PHYSFS_close(checkpoint_file);
	INPUT_DEMO_CRUMB("input_demo: checkpoint bytes written");
	if (!input_demo_restore_checkpoint_save(local_checkpoint_name)) {
		PHYSFS_delete(local_checkpoint_name);
		printf("Input demo replay could not restore checkpoint: %s\n",
		       local_checkpoint_name);
		input_demo_replay_unload();
		return 0;
	}
	PHYSFS_delete(local_checkpoint_name);
	INPUT_DEMO_CRUMB_V(
	    "input_demo: checkpoint restored mission=%s level=%d difficulty=%d gt=%lld",
	    Current_mission_filename, Current_level_num, Difficulty_level,
	    (long long) GameTime64);
	return 1;
}

static int input_demo_start_replay_new_level(
    input_demo_replay_loaded_context *replay_context)
{
	const input_demo_player_cfg *replay_player_cfg;
	int have_replay_player_cfg;

	if (!replay_context)
		return 1;
	if (!input_demo_difficulty_is_valid(input_demo_replay_difficulty())) {
		printf("Input demo replay difficulty is out of range: %d\n",
		       input_demo_replay_difficulty());
		input_demo_replay_unload();
		return 1;
	}
	replay_player_cfg = &replay_context->replay_player_cfg;
	have_replay_player_cfg = replay_context->have_replay_player_cfg;
	INPUT_DEMO_CRUMB_V("input_demo: new_level mission=%s level=%d frames=%u",
	                   replay_context->mission_name, input_demo_replay_level(),
	                   input_demo_replay_frame_count());
	if (!load_mission_by_name(replay_context->mission_name)) {
		printf("Input demo replay could not load mission: %s\n",
		       replay_context->mission_name);
		input_demo_replay_unload();
		return 1;
	}
	Difficulty_level = input_demo_replay_difficulty();
	if (have_replay_player_cfg)
		input_demo_apply_replay_player_cfg(replay_player_cfg);
	else
		input_demo_apply_legacy_replay_homing_default();
	printf("Input demo replay starting: %s level %d, %u frames\n",
	       replay_context->mission_name, input_demo_replay_level(),
	       input_demo_replay_frame_count());
	INPUT_DEMO_CRUMB("input_demo: new_level StartNewGame");
	input_demo_set_skip_level_intro(1);
	StartNewGame(input_demo_replay_level());
	return 0;
}

int input_demo_start_loaded_replay_common(void)
{
	const char *checkpoint_name;
	const uint8_t *checkpoint_data;
	size_t checkpoint_size;
	input_demo_replay_loaded_context replay_context;
	char *mission_name;
	const char *start_mode;
	const char *local_player_callsign;
	const input_demo_player_cfg *replay_player_cfg;
	int have_replay_player_cfg;

	if (!input_demo_replay_is_loaded()) {
		printf("Input demo replay is not loaded\n");
		return 1;
	}
	INPUT_DEMO_CRUMB("input_demo: start_loaded_replay enter");
	if (!input_demo_prepare_loaded_replay_context(&replay_context))
		return 1;
	mission_name = replay_context.mission_name;
	start_mode = replay_context.start_mode;
	local_player_callsign = replay_context.local_player_callsign;
	replay_player_cfg = &replay_context.replay_player_cfg;
	have_replay_player_cfg = replay_context.have_replay_player_cfg;
	if (!strcmp(start_mode, "new_level")) {
		return input_demo_start_replay_new_level(&replay_context);
	}
	if (strcmp(start_mode, "save_checkpoint") != 0) {
		printf("Input demo replay start_mode not supported: %s\n", start_mode);
		input_demo_replay_unload();
		return 1;
	}
	checkpoint_name = input_demo_replay_checkpoint_save_name();
	checkpoint_data = input_demo_replay_checkpoint_data();
	checkpoint_size = input_demo_replay_checkpoint_size();
	if (!input_demo_replay_has_checkpoint() || !checkpoint_name ||
	    !checkpoint_name[0] || !checkpoint_data || !checkpoint_size) {
		printf("Input demo replay is missing checkpoint data\n");
		input_demo_replay_unload();
		return 1;
	}
#ifdef DXX_BUILD_DESCENT_II
	if (input_demo_d1_in_d2_enabled &&
	    input_demo_replay_game() == INPUT_DEMO_GAME_D1) {
		d1_save_translate_checkpoint_start d1_checkpoint;
		const char *d1_mission_name;
		if (!d1_save_translate_read_checkpoint_start(checkpoint_data, checkpoint_size,
		                                             &d1_checkpoint)) {
			printf("Input demo replay could not parse D1 checkpoint metadata\n");
			input_demo_replay_unload();
			return 1;
		}
		if (!input_demo_difficulty_is_valid(d1_checkpoint.difficulty)) {
			printf("Input demo replay translated checkpoint difficulty is out of range: %d\n",
			       d1_checkpoint.difficulty);
			input_demo_replay_unload();
			return 1;
		}
		d1_mission_name =
		    d1_checkpoint.mission_name[0] ? d1_checkpoint.mission_name : mission_name;
		printf("Input demo replay D1-in-D2 starting from translated D1 checkpoint metadata\n");
		INPUT_DEMO_CRUMB_V("input_demo: d1-in-d2 translated start mission=%s level=%d version=%d",
		                   d1_mission_name, d1_checkpoint.current_level,
		                   d1_checkpoint.version);
		if (!load_mission_by_name((char *) d1_mission_name)) {
			printf("Input demo replay could not load translated D1 mission: %s\n",
			       d1_mission_name);
			input_demo_replay_unload();
			return 1;
		}
		Difficulty_level = d1_checkpoint.difficulty;
		difficulty_restore_history(d1_checkpoint.difficulty_changed,
		                           d1_checkpoint.difficulty_min,
		                           d1_checkpoint.difficulty_max);
		if (have_replay_player_cfg)
			input_demo_apply_replay_player_cfg(replay_player_cfg);
		else
			input_demo_apply_legacy_replay_homing_default();
		printf("Input demo replay starting: %s level %d, %u frames\n",
		       d1_mission_name, d1_checkpoint.current_level,
		       input_demo_replay_frame_count());
		input_demo_set_skip_level_intro(1);
		StartNewGame(d1_checkpoint.current_level);
		GameTime64 = (fix64) d1_checkpoint.game_time +
		             input_demo_replay_checkpoint_start_gt();
		Difficulty_level = d1_checkpoint.difficulty;
		difficulty_restore_history(d1_checkpoint.difficulty_changed,
		                           d1_checkpoint.difficulty_min,
		                           d1_checkpoint.difficulty_max);
		if (!d1_save_translate_apply_checkpoint_objects(checkpoint_data, checkpoint_size,
		                                                &d1_checkpoint)) {
			printf("Input demo replay could not translate D1 checkpoint objects\n");
			input_demo_replay_unload();
			return 1;
		}
		d1_save_translate_apply_checkpoint_player(&d1_checkpoint,
		                                          local_player_callsign);
		return 0;
	}
#endif
	PlayerCfg.OriginalHoming = have_replay_player_cfg ? replay_player_cfg->original_homing : 0;
	if (!input_demo_restore_replay_checkpoint_data(checkpoint_name,
	                                               checkpoint_data, checkpoint_size))
		return 1;
	{
		input_demo_replay_restored_player_diag player_diag;

		input_demo_capture_restored_player_diag(local_player_callsign,
		                                        replay_player_cfg, have_replay_player_cfg, &player_diag);
		input_demo_log_restored_player_diag(&player_diag);
	}
	if (d_stricmp(Current_mission_filename, mission_name) ||
	    Current_level_num != input_demo_replay_level() ||
	    Difficulty_level != input_demo_replay_difficulty()) {
		printf("Input demo replay checkpoint restore mismatch: mission=%s level=%d difficulty=%d\n",
		       Current_mission_filename, Current_level_num, Difficulty_level);
		input_demo_replay_unload();
		return 1;
	}
	printf("Input demo replay starting: %s level %d, %u frames\n",
	       mission_name, input_demo_replay_level(),
	       input_demo_replay_frame_count());
	INPUT_DEMO_CRUMB_V("input_demo: replay armed mission=%s level=%d frames=%u",
	                   mission_name, input_demo_replay_level(),
	                   input_demo_replay_frame_count());
	return 0;
}

static void input_demo_capture_restored_player_diag(
    const char *local_player_callsign,
    const input_demo_player_cfg *replay_player_cfg,
    int have_replay_player_cfg,
    input_demo_replay_restored_player_diag *diag)
{
	if (!diag)
		return;
	memset(diag, 0, sizeof(*diag));
	diag->player_cfg_result = -1;
	diag->replay_auto_level = -1;

	if (ConsoleObject)
		diag->replay_auto_level =
		    (ConsoleObject->mtype.phys_info.flags & PF_LEVELLING) ? 1 : 0;
	if (!Players[Player_num].callsign[0] && local_player_callsign &&
	    local_player_callsign[0]) {
		strncpy(Players[Player_num].callsign, local_player_callsign, CALLSIGN_LEN);
		Players[Player_num].callsign[CALLSIGN_LEN] = '\0';
	}
	if (Players[Player_num].callsign[0]) {
		new_player_config();
		diag->player_cfg_result = read_player_file();
	}
	if (have_replay_player_cfg)
		input_demo_apply_replay_player_cfg(replay_player_cfg);
	else {
		if (diag->replay_auto_level >= 0)
			PlayerCfg.AutoLeveling = diag->replay_auto_level;
		input_demo_apply_legacy_replay_homing_default();
	}
	diag->primary_order_hash = input_demo_replay_hash_u8_sequence(
	    PlayerCfg.PrimaryOrder, input_demo_primary_order_copy_count());
	diag->secondary_order_hash = input_demo_replay_hash_u8_sequence(
	    PlayerCfg.SecondaryOrder, MAX_SECONDARY_WEAPONS + 1);
	diag->replay_callsign = Players[Player_num].callsign[0] ? Players[Player_num].callsign : "<empty>";
	if (ConsoleObject) {
		diag->player_mass = ConsoleObject->mtype.phys_info.mass;
		diag->player_drag = ConsoleObject->mtype.phys_info.drag;
		diag->player_brakes = ConsoleObject->mtype.phys_info.brakes;
		diag->player_phys_flags = ConsoleObject->mtype.phys_info.flags;
	}
	if (Player_ship) {
		diag->ship_mass = Player_ship->mass;
		diag->ship_drag = Player_ship->drag;
		diag->ship_brakes = Player_ship->brakes;
		diag->ship_max_thrust = Player_ship->max_thrust;
		diag->ship_max_rotthrust = Player_ship->max_rotthrust;
		diag->ship_wiggle = Player_ship->wiggle;
	}
}

static void input_demo_log_restored_player_diag(
    const input_demo_replay_restored_player_diag *diag)
{
	if (!diag)
		return;
#ifdef DXX_BUILD_DESCENT_II
	input_demo_debug_printf("Input demo replay player config: callsign=%s result=%d auto_level=%d debris=%d headlight_default=%d autoselect=(nofire=%d,after=%d,cycle=%d,classic=%d) order_hash=(0x%x,0x%x) player_flags=0x%x phys=(%d,%d,%d,0x%x) ship=(%d,%d,%d,%d,%d,%d)\n",
	                        diag->replay_callsign, diag->player_cfg_result, PlayerCfg.AutoLeveling,
	                        PlayerCfg.PersistentDebris, PlayerCfg.HeadlightActiveDefault,
	                        PlayerCfg.NoFireAutoselect, PlayerCfg.SelectAfterFire,
	                        PlayerCfg.CycleAutoselectOnly, PlayerCfg.ClassicAutoselectWeapon,
	                        diag->primary_order_hash, diag->secondary_order_hash,
	                        Players[Player_num].flags,
	                        diag->player_mass, diag->player_drag,
	                        diag->player_brakes, diag->player_phys_flags,
	                        diag->ship_mass, diag->ship_drag,
	                        diag->ship_brakes, diag->ship_max_thrust,
	                        diag->ship_max_rotthrust, diag->ship_wiggle);
#else
	input_demo_debug_printf("Input demo replay player config: callsign=%s result=%d auto_level=%d debris=%d autoselect=(nofire=%d,after=%d,cycle=%d,classic=%d) order_hash=(0x%x,0x%x) player_flags=0x%x phys=(%d,%d,%d,0x%x) ship=(%d,%d,%d,%d,%d,%d)\n",
	                        diag->replay_callsign, diag->player_cfg_result, PlayerCfg.AutoLeveling,
	                        PlayerCfg.PersistentDebris,
	                        PlayerCfg.NoFireAutoselect, PlayerCfg.SelectAfterFire,
	                        PlayerCfg.CycleAutoselectOnly, PlayerCfg.ClassicAutoselectWeapon,
	                        diag->primary_order_hash, diag->secondary_order_hash,
	                        Players[Player_num].flags,
	                        diag->player_mass, diag->player_drag,
	                        diag->player_brakes, diag->player_phys_flags,
	                        diag->ship_mass, diag->ship_drag,
	                        diag->ship_brakes, diag->ship_max_thrust,
	                        diag->ship_max_rotthrust, diag->ship_wiggle);
#endif
}
