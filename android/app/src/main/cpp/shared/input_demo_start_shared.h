#ifndef INPUT_DEMO_START_SHARED_H
#define INPUT_DEMO_START_SHARED_H

/*
 * Shared helper bodies for d1/main/input_demo_start.c and d2/main/input_demo_start.c.
 * Including files provide the game headers and any per-game adapter macros.
 */

#ifndef INPUT_DEMO_PRIMARY_ORDER_COPY_COUNT
#error INPUT_DEMO_PRIMARY_ORDER_COPY_COUNT must be defined before including input_demo_start_shared.h
#endif

#ifndef INPUT_DEMO_APPLY_EXTRA_PLAYER_CFG
#define INPUT_DEMO_APPLY_EXTRA_PLAYER_CFG(player_cfg) ((void)0)
#endif

#ifndef INPUT_DEMO_APPLY_EXTRA_REPLAY_CMDLINE_OPTIONS
#define INPUT_DEMO_APPLY_EXTRA_REPLAY_CMDLINE_OPTIONS(options) ((void)0)
#endif

#ifndef INPUT_DEMO_FORMAT_REPLAY_MISSION_NAME
#define INPUT_DEMO_FORMAT_REPLAY_MISSION_NAME(replay_mission, mission_name, mission_name_size) \
	snprintf((mission_name), (mission_name_size), "%s", (replay_mission))
#endif

#ifndef INPUT_DEMO_RESTORE_CHECKPOINT_SAVE
#define INPUT_DEMO_RESTORE_CHECKPOINT_SAVE(path) state_restore_all_sub(path)
#endif

typedef struct input_demo_replay_cmdline_options {
	const char *demo_path;
	const char *actual_result_path;
	const char *state_log_path;
	const char *rng_trace_path;
	int replay_labels_enabled;
} input_demo_replay_cmdline_options;

typedef struct input_demo_replay_loaded_context {
	input_demo_player_cfg replay_player_cfg;
	char local_player_callsign[CALLSIGN_LEN + 1];
	char mission_name[PATH_MAX];
	const char *start_mode;
	int have_replay_player_cfg;
} input_demo_replay_loaded_context;

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
	if (arg_index + 1 >= Num_args || !Args[arg_index + 1] || Args[arg_index + 1][0] == '-')
	{
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
	INPUT_DEMO_APPLY_EXTRA_PLAYER_CFG(player_cfg);
	PlayerCfg.NoFireAutoselect = player_cfg->no_fire_autoselect;
	PlayerCfg.CycleAutoselectOnly = player_cfg->cycle_autoselect_only;
	PlayerCfg.SelectAfterFire = player_cfg->select_after_fire;
	PlayerCfg.ClassicAutoselectWeapon = player_cfg->classic_autoselect_weapon;
	memcpy(PlayerCfg.PrimaryOrder, player_cfg->primary_order,
		INPUT_DEMO_PRIMARY_ORDER_COPY_COUNT);
	memcpy(PlayerCfg.SecondaryOrder, player_cfg->secondary_order,
		MAX_SECONDARY_WEAPONS + 1);
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
	if (error)
	{
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
	const char *expected_game_name,
	char *error,
	size_t error_size)
{
	if (!input_demo_replay_load(demo_path, error, error_size))
		return 0;
	if (input_demo_replay_game() != expected_game)
	{
		input_demo_replay_unload();
		snprintf(error, error_size,
			"Input demo replay currently supports %s demos only",
			expected_game_name ? expected_game_name : "requested");
		return 0;
	}
	return 1;
}

static int input_demo_parse_replay_cmdline(input_demo_replay_cmdline_options *options)
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
	INPUT_DEMO_APPLY_EXTRA_REPLAY_CMDLINE_OPTIONS(options);

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

static int input_demo_apply_replay_common_setup(
	const input_demo_replay_cmdline_options *options,
	char *error,
	size_t error_size)
{
	if (!options)
		return 0;
	if (options->actual_result_path)
		input_demo_replay_set_actual_result_path(options->actual_result_path);
	if (options->rng_trace_path && !input_demo_rng_trace_start_replay(
		options->rng_trace_path, error, error_size))
	{
		input_demo_replay_unload();
		return 0;
	}
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
	if (!context->start_mode)
	{
		printf("Input demo replay metadata is missing start_mode\n");
		input_demo_replay_unload();
		return 0;
	}
	if (!input_demo_replay_mission())
	{
		printf("Input demo replay metadata is missing mission\n");
		input_demo_replay_unload();
		return 0;
	}
	INPUT_DEMO_FORMAT_REPLAY_MISSION_NAME(input_demo_replay_mission(),
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
		local_checkpoint_name, (unsigned int)checkpoint_size);
	if (!strncmp(local_checkpoint_name, "Players/", 8))
		PHYSFS_mkdir("Players");
	PHYSFS_delete(local_checkpoint_name);
	checkpoint_file = PHYSFS_openWrite(local_checkpoint_name);
	if (!checkpoint_file)
	{
		printf("Input demo replay could not write checkpoint file: %s\n",
			local_checkpoint_name);
		input_demo_replay_unload();
		return 0;
	}
	if (PHYSFS_writeBytes(checkpoint_file, checkpoint_data, checkpoint_size) !=
		(PHYSFS_sint64)checkpoint_size)
	{
		PHYSFS_close(checkpoint_file);
		PHYSFS_delete(local_checkpoint_name);
		printf("Input demo replay could not write checkpoint bytes: %s\n",
			local_checkpoint_name);
		input_demo_replay_unload();
		return 0;
	}
	PHYSFS_close(checkpoint_file);
	INPUT_DEMO_CRUMB("input_demo: checkpoint bytes written");
	if (!INPUT_DEMO_RESTORE_CHECKPOINT_SAVE(local_checkpoint_name))
	{
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
		(long long)GameTime64);
	return 1;
}

#undef INPUT_DEMO_APPLY_EXTRA_PLAYER_CFG
#undef INPUT_DEMO_APPLY_EXTRA_REPLAY_CMDLINE_OPTIONS
#undef INPUT_DEMO_FORMAT_REPLAY_MISSION_NAME
#undef INPUT_DEMO_RESTORE_CHECKPOINT_SAVE

#endif