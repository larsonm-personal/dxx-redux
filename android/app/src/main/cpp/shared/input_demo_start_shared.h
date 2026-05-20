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

#ifndef INPUT_DEMO_RESTORED_PLAYER_DIAG_EXTRA_FMT
#define INPUT_DEMO_RESTORED_PLAYER_DIAG_EXTRA_FMT ""
#endif

#ifndef INPUT_DEMO_RESTORED_PLAYER_DIAG_EXTRA_ARGS
#define INPUT_DEMO_RESTORED_PLAYER_DIAG_EXTRA_ARGS
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

static int input_demo_load_replay_from_path_common(const char *demo_path,
	int expected_game,
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
	return input_demo_load_replay_checked(demo_path, expected_game,
		expected_game_name, error, error_size);
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

static int input_demo_start_replay_state_trace_and_log_paths(
	const input_demo_replay_cmdline_options *options,
	char *error,
	size_t error_size)
{
	const char *state_log_path;

	if (!options)
		return 0;
	state_log_path = options->state_log_path;
	if (state_log_path && !input_demo_state_trace_start_replay(state_log_path,
		error, error_size))
	{
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

static int input_demo_start_loaded_replay_common(void)
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

	if (!input_demo_replay_is_loaded())
	{
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
		INPUT_DEMO_CRUMB_V("input_demo: new_level mission=%s level=%d frames=%u",
			mission_name, input_demo_replay_level(),
			input_demo_replay_frame_count());
		if (!load_mission_by_name(mission_name))
		{
			printf("Input demo replay could not load mission: %s\n",
				mission_name);
			input_demo_replay_unload();
			return 1;
		}
		Difficulty_level = input_demo_replay_difficulty();
		if (have_replay_player_cfg)
			input_demo_apply_replay_player_cfg(replay_player_cfg);
		printf("Input demo replay starting: %s level %d, %u frames\n",
			mission_name, input_demo_replay_level(),
			input_demo_replay_frame_count());
		INPUT_DEMO_CRUMB("input_demo: new_level StartNewGame");
		input_demo_set_skip_level_intro(1);
		StartNewGame(input_demo_replay_level());
		return 0;
	}
	if (strcmp(start_mode, "save_checkpoint") != 0)
	{
		printf("Input demo replay start_mode not supported: %s\n", start_mode);
		input_demo_replay_unload();
		return 1;
	}
	checkpoint_name = input_demo_replay_checkpoint_save_name();
	checkpoint_data = input_demo_replay_checkpoint_data();
	checkpoint_size = input_demo_replay_checkpoint_size();
	if (!input_demo_replay_has_checkpoint() || !checkpoint_name ||
		!checkpoint_name[0] || !checkpoint_data || !checkpoint_size)
	{
		printf("Input demo replay is missing checkpoint data\n");
		input_demo_replay_unload();
		return 1;
	}
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
		Difficulty_level != input_demo_replay_difficulty())
	{
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
		local_player_callsign[0])
	{
		strncpy(Players[Player_num].callsign, local_player_callsign, CALLSIGN_LEN);
		Players[Player_num].callsign[CALLSIGN_LEN] = '\0';
	}
	if (Players[Player_num].callsign[0]) {
		new_player_config();
		diag->player_cfg_result = read_player_file();
	}
	if (have_replay_player_cfg)
		input_demo_apply_replay_player_cfg(replay_player_cfg);
	else if (diag->replay_auto_level >= 0)
		PlayerCfg.AutoLeveling = diag->replay_auto_level;
	diag->primary_order_hash = input_demo_replay_hash_u8_sequence(
		PlayerCfg.PrimaryOrder, INPUT_DEMO_PRIMARY_ORDER_COPY_COUNT);
	diag->secondary_order_hash = input_demo_replay_hash_u8_sequence(
		PlayerCfg.SecondaryOrder, MAX_SECONDARY_WEAPONS + 1);
	diag->replay_callsign = Players[Player_num].callsign[0] ?
		Players[Player_num].callsign : "<empty>";
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
	input_demo_debug_printf("Input demo replay player config: callsign=%s result=%d auto_level=%d debris=%d"
		INPUT_DEMO_RESTORED_PLAYER_DIAG_EXTRA_FMT
		" autoselect=(nofire=%d,after=%d,cycle=%d,classic=%d) order_hash=(0x%x,0x%x) player_flags=0x%x phys=(%d,%d,%d,0x%x) ship=(%d,%d,%d,%d,%d,%d)\n",
		diag->replay_callsign, diag->player_cfg_result, PlayerCfg.AutoLeveling,
		PlayerCfg.PersistentDebris
		INPUT_DEMO_RESTORED_PLAYER_DIAG_EXTRA_ARGS,
		PlayerCfg.NoFireAutoselect, PlayerCfg.SelectAfterFire,
		PlayerCfg.CycleAutoselectOnly, PlayerCfg.ClassicAutoselectWeapon,
		diag->primary_order_hash, diag->secondary_order_hash,
		Players[Player_num].flags,
		diag->player_mass, diag->player_drag,
		diag->player_brakes, diag->player_phys_flags,
		diag->ship_mass, diag->ship_drag,
		diag->ship_brakes, diag->ship_max_thrust,
		diag->ship_max_rotthrust, diag->ship_wiggle);
}

#undef INPUT_DEMO_APPLY_EXTRA_PLAYER_CFG
#undef INPUT_DEMO_APPLY_EXTRA_REPLAY_CMDLINE_OPTIONS
#undef INPUT_DEMO_FORMAT_REPLAY_MISSION_NAME
#undef INPUT_DEMO_RESTORE_CHECKPOINT_SAVE
#undef INPUT_DEMO_RESTORED_PLAYER_DIAG_EXTRA_FMT
#undef INPUT_DEMO_RESTORED_PLAYER_DIAG_EXTRA_ARGS

#endif