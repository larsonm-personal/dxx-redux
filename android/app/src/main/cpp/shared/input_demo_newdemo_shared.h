#ifndef INPUT_DEMO_NEWDEMO_SHARED_H
#define INPUT_DEMO_NEWDEMO_SHARED_H

/*
 * Shared helper bodies for d1/main/newdemo.c and d2/main/newdemo.c.
 * Including files provide the game headers, constants, and any per-game
 * adapter macros.
 */

#ifndef INPUT_DEMO_QUICK_RECORD_MISSION_EXPR
#error INPUT_DEMO_QUICK_RECORD_MISSION_EXPR must be defined before including input_demo_newdemo_shared.h
#endif

#ifndef INPUT_DEMO_QUICK_RECORD_BUILTIN_MISSION_ID
#error INPUT_DEMO_QUICK_RECORD_BUILTIN_MISSION_ID must be defined before including input_demo_newdemo_shared.h
#endif

#ifndef INPUT_DEMO_QUICK_RECORD_FALLBACK_MISSION_NAME
#error INPUT_DEMO_QUICK_RECORD_FALLBACK_MISSION_NAME must be defined before including input_demo_newdemo_shared.h
#endif

#ifndef INPUT_DEMO_QUICK_RECORD_NAME_PREFIX
#error INPUT_DEMO_QUICK_RECORD_NAME_PREFIX must be defined before including input_demo_newdemo_shared.h
#endif

#ifndef INPUT_DEMO_PRIMARY_ORDER_COUNT
#error INPUT_DEMO_PRIMARY_ORDER_COUNT must be defined before including input_demo_newdemo_shared.h
#endif

#ifndef INPUT_DEMO_RECORDER_SETTINGS_GAME
#error INPUT_DEMO_RECORDER_SETTINGS_GAME must be defined before including input_demo_newdemo_shared.h
#endif

#ifndef INPUT_DEMO_RECORDER_SETTINGS_MISSION
#error INPUT_DEMO_RECORDER_SETTINGS_MISSION must be defined before including input_demo_newdemo_shared.h
#endif

#ifndef INPUT_DEMO_CLEAR_QUICK_RECORDING_EXTRA
#define INPUT_DEMO_CLEAR_QUICK_RECORDING_EXTRA() ((void)0)
#endif

#ifndef INPUT_DEMO_CAPTURE_CHECKPOINT_EXTRA
#define INPUT_DEMO_CAPTURE_CHECKPOINT_EXTRA(settings) ((void)0)
#endif

#ifndef INPUT_DEMO_FILL_EXTRA_PLAYER_CFG
#define INPUT_DEMO_FILL_EXTRA_PLAYER_CFG(settings) ((void)0)
#endif

static const char *input_demo_quick_record_mission_name(void)
{
	const char *mission = INPUT_DEMO_QUICK_RECORD_MISSION_EXPR;

	if (!mission || !mission[0] ||
		!strcmp(mission, INPUT_DEMO_QUICK_RECORD_BUILTIN_MISSION_ID))
		return INPUT_DEMO_QUICK_RECORD_FALLBACK_MISSION_NAME;
	return mission;
}

static void input_demo_clear_quick_recording(void)
{
	input_demo_android_quick_recording = 0;
	input_demo_android_quick_record_level = 0;
	input_demo_android_quick_record_mission[0] = 0;
	INPUT_DEMO_CLEAR_QUICK_RECORDING_EXTRA();
}

static void input_demo_release_recorder_settings(input_demo_recorder_settings *settings)
{
	if (!settings || !settings->checkpoint_data)
		return;
	mem_free((void *) settings->checkpoint_data);
	settings->checkpoint_data = NULL;
	settings->checkpoint_size = 0;
}

static int input_demo_capture_recorder_checkpoint(input_demo_recorder_settings *settings,
	                                              char *error, size_t error_size)
{
	const char *save_name = GameArg.SysUsePlayersDir ? INPUT_DEMO_CHECKPOINT_PLAYERS_NAME : INPUT_DEMO_CHECKPOINT_NAME;
	char logical_path[PATH_MAX] = "";
	char desc[] = "Input Demo Checkpoint";
	PHYSFS_file *fp = NULL;
	PHYSFS_sint64 file_size;
	unsigned char *data = NULL;

	if (!settings)
		return 1;
	if (GameArg.SysUsePlayersDir)
		PHYSFS_mkdir("Players");
	snprintf(logical_path, SDL_arraysize(logical_path), "%s", save_name);
	PHYSFS_delete(logical_path);
	if (!state_save_all_sub(logical_path, desc)) {
		if (error && error_size)
			snprintf(error, error_size, "%s", "could not create input demo checkpoint save");
		return 0;
	}
	fp = PHYSFS_openRead(logical_path);
	if (!fp) {
		if (error && error_size)
			snprintf(error, error_size, "%s", "could not read input demo checkpoint save");
		PHYSFS_delete(logical_path);
		return 0;
	}
	file_size = PHYSFS_fileLength(fp);
	if (file_size <= 0 || file_size > UINT32_MAX) {
		if (error && error_size)
			snprintf(error, error_size, "%s", "input demo checkpoint save has invalid size");
		PHYSFS_close(fp);
		PHYSFS_delete(logical_path);
		return 0;
	}
	MALLOC(data, unsigned char, (size_t) file_size);
	if (!data) {
		if (error && error_size)
			snprintf(error, error_size, "%s", "could not allocate input demo checkpoint buffer");
		PHYSFS_close(fp);
		PHYSFS_delete(logical_path);
		return 0;
	}
	if (PHYSFS_readBytes(fp, data, file_size) != file_size) {
		if (error && error_size)
			snprintf(error, error_size, "%s", "could not read input demo checkpoint bytes");
		PHYSFS_close(fp);
		PHYSFS_delete(logical_path);
		d_free(data);
		return 0;
	}
	PHYSFS_close(fp);
	PHYSFS_delete(logical_path);
	settings->checkpoint_save_name = save_name;
	settings->checkpoint_data = data;
	settings->checkpoint_size = (size_t) file_size;
	settings->has_checkpoint_start_gt = 1;
	settings->checkpoint_start_gt = GameTime64;
	settings->has_checkpoint_collision_delay_last_play_time = 1;
	settings->checkpoint_collision_delay_last_play_time = collide_get_collision_delay_last_play_time();
	INPUT_DEMO_CAPTURE_CHECKPOINT_EXTRA(settings);
	return 1;
}

static int input_demo_is_mid_level_record_start(void)
{
	const obj_position *player_start;

	if (ThisLevelTime != 0)
		return 1;
	if (!ConsoleObject)
		return 0;
	player_start = &Player_init[Player_num];
	if (ConsoleObject->segnum != player_start->segnum)
		return 1;
	if (ConsoleObject->pos.x != player_start->pos.x ||
	    ConsoleObject->pos.y != player_start->pos.y ||
	    ConsoleObject->pos.z != player_start->pos.z)
		return 1;
	if (ConsoleObject->orient.rvec.x != player_start->orient.rvec.x ||
	    ConsoleObject->orient.rvec.y != player_start->orient.rvec.y ||
	    ConsoleObject->orient.rvec.z != player_start->orient.rvec.z ||
	    ConsoleObject->orient.uvec.x != player_start->orient.uvec.x ||
	    ConsoleObject->orient.uvec.y != player_start->orient.uvec.y ||
	    ConsoleObject->orient.uvec.z != player_start->orient.uvec.z ||
	    ConsoleObject->orient.fvec.x != player_start->orient.fvec.x ||
	    ConsoleObject->orient.fvec.y != player_start->orient.fvec.y ||
	    ConsoleObject->orient.fvec.z != player_start->orient.fvec.z)
		return 1;
	return 0;
}

static int input_demo_ascii_equal_ignore_case(char lhs, char rhs)
{
	if (lhs >= 'A' && lhs <= 'Z')
		lhs += 'a' - 'A';
	if (rhs >= 'A' && rhs <= 'Z')
		rhs += 'a' - 'A';
	return lhs == rhs;
}

static void input_demo_fill_recorder_player_cfg(input_demo_recorder_settings *settings)
{
	int i;

	if (!settings)
		return;
	settings->has_player_cfg = 1;
	settings->player_cfg.auto_leveling = PlayerCfg.AutoLeveling;
	settings->player_cfg.persistent_debris = PlayerCfg.PersistentDebris;
	INPUT_DEMO_FILL_EXTRA_PLAYER_CFG(settings);
	settings->player_cfg.no_fire_autoselect = PlayerCfg.NoFireAutoselect;
	settings->player_cfg.cycle_autoselect_only = PlayerCfg.CycleAutoselectOnly;
	settings->player_cfg.select_after_fire = PlayerCfg.SelectAfterFire;
	settings->player_cfg.classic_autoselect_weapon = PlayerCfg.ClassicAutoselectWeapon;
	settings->player_cfg.primary_order_count = INPUT_DEMO_PRIMARY_ORDER_COUNT;
	settings->player_cfg.secondary_order_count = MAX_SECONDARY_WEAPONS + 1;
	for (i = 0; i < INPUT_DEMO_PRIMARY_ORDER_COUNT; i++)
		settings->player_cfg.primary_order[i] = PlayerCfg.PrimaryOrder[i];
	for (i = 0; i < MAX_SECONDARY_WEAPONS + 1; i++)
		settings->player_cfg.secondary_order[i] = PlayerCfg.SecondaryOrder[i];
}

static void input_demo_strip_hog_extension(char *value)
{
	size_t len = strlen(value);

	if (len > 4 &&
	    value[len - 4] == '.' &&
	    input_demo_ascii_equal_ignore_case(value[len - 3], 'h') &&
	    input_demo_ascii_equal_ignore_case(value[len - 2], 'o') &&
	    input_demo_ascii_equal_ignore_case(value[len - 1], 'g'))
		value[len - 4] = 0;
}

static void input_demo_sanitize_slug(char *result, unsigned int result_size, const char *source)
{
	char working[PATH_MAX] = "";
	const char *basename = source ? source : "";
	const char *slash;
	unsigned int out = 0;
	int wrote_separator = 0;
	size_t i;

	if (!result_size)
		return;
	result[0] = 0;
	slash = strrchr(basename, '/');
	if (slash)
		basename = slash + 1;
	slash = strrchr(basename, '\\');
	if (slash)
		basename = slash + 1;
	snprintf(working, SDL_arraysize(working), "%s", basename);
	input_demo_strip_hog_extension(working);
	for (i = 0; working[i] && out + 1 < result_size; ++i) {
		unsigned char ch = (unsigned char) working[i];

		if ((ch >= 'A' && ch <= 'Z') ||
		    (ch >= 'a' && ch <= 'z') ||
		    (ch >= '0' && ch <= '9') ||
		    ch == '_' || ch == '-' || ch == '.') {
			result[out++] = (char) ch;
			wrote_separator = 0;
		} else if (out && !wrote_separator) {
			result[out++] = '_';
			wrote_separator = 1;
		}
	}
	while (out && result[out - 1] == '_')
		out--;
	if (!out)
		snprintf(result, result_size, "%s", "mission");
	else
		result[out] = 0;
}

static int input_demo_prepare_recorder_settings(input_demo_recorder_settings *settings,
	                                            char *error, size_t error_size)
{
	const char *rng_mode;
	int replay_mode;

	if (settings)
		input_demo_recorder_settings_clear(settings);
	if (Game_mode & GM_MULTI) {
		if (error && error_size)
			snprintf(error, error_size, "%s", "multiplayer input demo recording is not supported");
		return 0;
	}
	if (Current_level_num == 0) {
		if (error && error_size)
			snprintf(error, error_size, "%s", "input demo recording requires a real level");
		return 0;
	}
	replay_mode = d_rand_get_replay_mode();
	if (replay_mode != D_RAND_REPLAY_MODE_LCG_STATE) {
		rng_mode = input_demo_rng_mode_name(replay_mode);
		if (error && error_size)
			snprintf(error, error_size, "live recording does not support rng_mode %s yet", rng_mode ? rng_mode : "unknown");
		return 0;
	}
	if (settings) {
		settings->game = INPUT_DEMO_RECORDER_SETTINGS_GAME;
		settings->mission = INPUT_DEMO_RECORDER_SETTINGS_MISSION;
		settings->level = Current_level_num;
		settings->difficulty = Difficulty_level;
		settings->rng_mode = input_demo_rng_mode_name(replay_mode);
		settings->record_per_frame_state = 0;
#if defined(__ANDROID__)
		settings->record_per_frame_state = g_demo_record_per_frame_state ? 1 : 0;
#endif
		input_demo_fill_recorder_player_cfg(settings);
	}
	if (input_demo_is_mid_level_record_start() && !input_demo_capture_recorder_checkpoint(settings, error, error_size))
		return 0;
	return 1;
}

static void input_demo_build_quick_record_name(char *demo_name, unsigned int demo_name_size)
{
	char mission_slug[PATH_MAX] = "";
	char base_name[PATH_MAX] = "";
	char relative_path[PATH_MAX] = "";
	time_t now = time(NULL);
	struct tm *current_time = localtime(&now);
	int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
	int attempt;

	if (current_time) {
		year = current_time->tm_year + 1900;
		month = current_time->tm_mon + 1;
		day = current_time->tm_mday;
		hour = current_time->tm_hour;
		minute = current_time->tm_min;
		second = current_time->tm_sec;
	}
	input_demo_sanitize_slug(mission_slug, SDL_arraysize(mission_slug),
	                         input_demo_android_quick_record_mission[0] ? input_demo_android_quick_record_mission : input_demo_quick_record_mission_name());
	sprintf_s(base_name, SDL_arraysize(base_name), INPUT_DEMO_QUICK_RECORD_NAME_PREFIX "_%s_level%d_%04d%02d%02d_%02d%02d%02d",
	          mission_slug,
	          input_demo_android_quick_record_level,
	          year, month, day, hour, minute, second);
	for (attempt = 0; attempt < INPUT_DEMO_QUICK_NAME_ATTEMPTS; ++attempt) {
		if (!attempt)
			sprintf_s(demo_name, demo_name_size, "%s", base_name);
		else
			sprintf_s(demo_name, demo_name_size, "%s_%d", base_name, attempt + 1);
		sprintf_s(relative_path, SDL_arraysize(relative_path), INPUT_DEMO_NEW_DIR "/%s" INPUT_DEMO_EXTENSION, demo_name);
		if (!PHYSFSX_exists(relative_path, 0))
			return;
	}
	sprintf_s(demo_name, demo_name_size, "%s_%u", base_name, (unsigned int) timer_query());
}

static void input_demo_build_classic_demo_path(char *relative_path,
	                                           unsigned int relative_path_size,
	                                           const char *demo_dir,
	                                           const char *demo_name)
{
	sprintf_s(relative_path, relative_path_size, "%s/%s" DEMO_EXT, demo_dir, demo_name);
}

static void input_demo_delete_classic_demo_sidecar(const char *input_demo_relative_path)
{
	char classic_demo_path[PATH_MAX] = "";
	size_t input_demo_path_len;
	size_t input_demo_extension_len;

	if (!input_demo_relative_path || !input_demo_relative_path[0])
		return;
	input_demo_path_len = strlen(input_demo_relative_path);
	input_demo_extension_len = strlen(INPUT_DEMO_EXTENSION);
	if (input_demo_path_len <= input_demo_extension_len)
		return;
	if (strcmp(input_demo_relative_path + input_demo_path_len - input_demo_extension_len, INPUT_DEMO_EXTENSION))
		return;
	if (input_demo_path_len - input_demo_extension_len + strlen(DEMO_EXT) >= SDL_arraysize(classic_demo_path))
		return;
	memcpy(classic_demo_path, input_demo_relative_path, input_demo_path_len - input_demo_extension_len);
	memcpy(classic_demo_path + input_demo_path_len - input_demo_extension_len, DEMO_EXT, strlen(DEMO_EXT) + 1);
	PHYSFS_delete(classic_demo_path);
}

static void input_demo_trim_new_recordings(void)
{
	char **find, **i;
	static const char *const types[] = { INPUT_DEMO_EXTENSION, NULL };

	for (;;) {
		char oldest_path[PATH_MAX] = "";
		PHYSFS_sint64 oldest_modtime = 0;
		int file_count = 0;
		int found_oldest = 0;

		find = PHYSFSX_findFiles(INPUT_DEMO_NEW_DIR, types);
		if (!find)
			return;
		for (i = find; *i != NULL; i++) {
			PHYSFS_Stat statbuf;
			char relative_path[PATH_MAX] = "";

			sprintf_s(relative_path, SDL_arraysize(relative_path), INPUT_DEMO_NEW_DIR "/%s", *i);
			if (!PHYSFS_stat(relative_path, &statbuf) || statbuf.filetype != PHYSFS_FILETYPE_REGULAR)
				continue;
			file_count++;
			if (!found_oldest || statbuf.modtime < oldest_modtime ||
			    (statbuf.modtime == oldest_modtime && strcmp(relative_path, oldest_path) < 0)) {
				oldest_modtime = statbuf.modtime;
				sprintf_s(oldest_path, SDL_arraysize(oldest_path), "%s", relative_path);
				found_oldest = 1;
			}
		}
		PHYSFS_freeList(find);
		if (file_count <= INPUT_DEMO_NEW_LIMIT || !found_oldest)
			return;
		if (!PHYSFS_delete(oldest_path))
			return;
		input_demo_delete_classic_demo_sidecar(oldest_path);
		{
			char trace_path[PATH_MAX] = "";

			snprintf(trace_path, SDL_arraysize(trace_path), "%s%s", oldest_path, INPUT_DEMO_RNG_TRACE_SUFFIX);
			PHYSFS_delete(trace_path);
		}
	}
}

static int maybe_start_input_demo_recording(int is_autorecord)
{
	input_demo_recorder_settings settings;
	char error[256] = "";
	int started;

	if (!input_demo_prepare_recorder_settings(&settings, error, sizeof(error))) {
		if (!is_autorecord)
			con_printf(CON_NORMAL, "Input demo recording skipped: %s\n", error);
		return 0;
	}
	started = input_demo_recorder_start(&settings, error, sizeof(error));
	input_demo_release_recorder_settings(&settings);
	if (!started) {
		con_printf(CON_NORMAL, "Input demo recording did not start: %s\n", error);
		return 0;
	}
	con_printf(CON_NORMAL, "Input demo recording started for %s level %d\n",
		INPUT_DEMO_RECORDER_SETTINGS_MISSION, Current_level_num);
	return 1;
}

static void maybe_flush_input_demo_recording(const char *demo_name, int use_new_record_dir)
{
	char relative_path[PATH_MAX] = "";
	char absolute_path[PATH_MAX] = "";
	char trace_path[PATH_MAX + sizeof(INPUT_DEMO_RNG_TRACE_SUFFIX)] = "";
	char error[256] = "";
	input_demo_result result;
	const char *demo_dir = use_new_record_dir ? INPUT_DEMO_NEW_DIR : INPUT_DEMO_RECORD_DIR;

	if (!input_demo_recorder_is_active())
		return;
	PHYSFS_mkdir(INPUT_DEMO_RECORD_DIR);
	if (use_new_record_dir)
		PHYSFS_mkdir(INPUT_DEMO_NEW_DIR);
	sprintf_s(relative_path, SDL_arraysize(relative_path), "%s/%s" INPUT_DEMO_EXTENSION, demo_dir, demo_name);
	if (!PHYSFSX_getRealPath(relative_path, absolute_path)) {
		con_printf(CON_NORMAL, "Input demo recording stopped: could not resolve file path %s\n", relative_path);
		input_demo_recorder_cancel();
		return;
	}
	input_demo_capture_current_result(&result);
	if (!input_demo_recorder_flush_with_result(absolute_path, &result, error, sizeof(error))) {
		con_printf(CON_NORMAL, "Input demo recording stopped: %s\n", error);
		input_demo_recorder_cancel();
		return;
	}
	if (use_new_record_dir)
		input_demo_trim_new_recordings();
	snprintf(trace_path, SDL_arraysize(trace_path), "%s%s", absolute_path, INPUT_DEMO_RNG_TRACE_SUFFIX);
	con_printf(CON_NORMAL, "Input demo file saved to %s\n", absolute_path);
	con_printf(CON_NORMAL, "Input demo RNG trace saved to %s\n", trace_path);
}

#undef INPUT_DEMO_QUICK_RECORD_MISSION_EXPR
#undef INPUT_DEMO_QUICK_RECORD_BUILTIN_MISSION_ID
#undef INPUT_DEMO_QUICK_RECORD_FALLBACK_MISSION_NAME
#undef INPUT_DEMO_QUICK_RECORD_NAME_PREFIX
#undef INPUT_DEMO_PRIMARY_ORDER_COUNT
#undef INPUT_DEMO_RECORDER_SETTINGS_GAME
#undef INPUT_DEMO_RECORDER_SETTINGS_MISSION
#undef INPUT_DEMO_CLEAR_QUICK_RECORDING_EXTRA
#undef INPUT_DEMO_CAPTURE_CHECKPOINT_EXTRA
#undef INPUT_DEMO_FILL_EXTRA_PLAYER_CFG

#endif