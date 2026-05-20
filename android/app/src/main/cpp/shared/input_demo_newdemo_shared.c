#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "args.h"
#include "collide.h"
#include "console.h"
#include "game.h"
#include "gameseq.h"
#include "gameseg.h"
#include "gr.h"
#include "input_demo_hooks.h"
#include "input_demo_hooks_shared.h"
#include "input_demo_recorder.h"
#include "input_demo_result.h"
#include "input_demo_rng_mode.h"
#include "input_demo_rng_trace.h"
#include "menu.h"
#include "mission.h"
#include "newdemo.h"
#include "object.h"
#include "palette.h"
#include "physfsx.h"
#include "player.h"
#include "segment.h"
#include "state.h"
#include "text.h"
#include "timer.h"
#include "u_mem.h"
#ifdef DXX_BUILD_DESCENT_II
#include "escort.h"
#endif

#include "input_demo_newdemo_shared.h"

#if defined(__ANDROID__)
extern volatile int g_demo_record_per_frame_state;
#endif

extern fix ThisLevelTime;
extern PHYSFS_file *outfile;
extern void newdemo_write_end(void);
extern void newdemo_get_default_filename(char *filename_buffer,
                                         unsigned int filename_buffer_length);
extern int newdemo_prompt_filename(char *filename_buffer,
                                   unsigned int filename_buffer_length);
extern int input_demo_newdemo_record_no_space(void);
#ifdef DXX_BUILD_DESCENT_II
extern void newdemo_record_oneframeevent_update(void);
#else
extern void newdemo_record_oneframeevent_update(int wallupdate);
#endif

#define INPUT_DEMO_RECORD_DIR              "input_demo_recordings"
#define INPUT_DEMO_NEW_DIR                 INPUT_DEMO_RECORD_DIR "/new"
#define INPUT_DEMO_EXTENSION               ".dximdemo"
#define INPUT_DEMO_NEW_LIMIT               10
#define INPUT_DEMO_QUICK_NAME_ATTEMPTS     255
#define INPUT_DEMO_TEMP_NAME               "tmpdemo"
#define INPUT_DEMO_CHECKPOINT_NAME         "inputdemo_start.dgss"
#define INPUT_DEMO_CHECKPOINT_PLAYERS_NAME "Players/inputdemo_start.dgss"
#define INPUT_DEMO_TEMP_DEMO_FILENAME      DEMO_DIR "tmpdemo.dem"

static int input_demo_android_quick_recording = 0;
static int input_demo_android_quick_record_level = 0;
static char input_demo_android_quick_record_mission[PATH_MAX] = "";

static const char *input_demo_quick_record_mission_expr(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return Current_mission_filename;
#else
	return input_demo_current_mission_id();
#endif
}

static const char *input_demo_quick_record_builtin_mission_id(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return "d2";
#else
	return "d1";
#endif
}

static const char *input_demo_quick_record_fallback_mission_name(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return "descent2";
#else
	return "descent";
#endif
}

static const char *input_demo_quick_record_name_prefix(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return "d2";
#else
	return "d1";
#endif
}

static unsigned int input_demo_primary_order_count(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return MAX_PRIMARY_WEAPONS + 1;
#else
	return MAX_PRIMARY_WEAPONS + 2;
#endif
}

static int input_demo_recorder_settings_game(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return INPUT_DEMO_GAME_D2;
#else
	return INPUT_DEMO_GAME_D1;
#endif
}

static const char *input_demo_recorder_settings_mission(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return Current_mission_filename;
#else
	return input_demo_current_mission_id();
#endif
}

static void input_demo_record_oneframeevent_update_common(void)
{
#ifdef DXX_BUILD_DESCENT_II
	newdemo_record_oneframeevent_update();
#else
	newdemo_record_oneframeevent_update(0);
#endif
}

static void input_demo_clear_quick_recording_extra(void)
{
#ifdef DXX_BUILD_DESCENT_II
	input_demo_clear_recording_terminal_exit();
#endif
}

static void input_demo_quick_recording_start_prep(void)
{
#ifdef DXX_BUILD_DESCENT_II
	input_demo_clear_recording_terminal_exit();
#endif
}

static void input_demo_capture_checkpoint_extra(
    input_demo_recorder_settings *settings)
{
#ifdef DXX_BUILD_DESCENT_II
	escort_get_input_demo_checkpoint_state(
	    &settings->checkpoint_escort_state);
	escort_get_input_demo_checkpoint_thief_state(
	    &settings->checkpoint_thief_state);
#else
	(void) settings;
#endif
}

static void input_demo_fill_extra_player_cfg(
    input_demo_recorder_settings *settings)
{
#ifdef DXX_BUILD_DESCENT_II
	settings->player_cfg.has_headlight_active_default = 1;
	settings->player_cfg.headlight_active_default =
	    PlayerCfg.HeadlightActiveDefault;
#else
	(void) settings;
#endif
}

static const char *input_demo_quick_record_mission_name(void)
{
	const char *mission = input_demo_quick_record_mission_expr();

	if (!mission || !mission[0] ||
	    !strcmp(mission, input_demo_quick_record_builtin_mission_id()))
		return input_demo_quick_record_fallback_mission_name();
	return mission;
}

static void input_demo_clear_quick_recording(void)
{
	input_demo_android_quick_recording = 0;
	input_demo_android_quick_record_level = 0;
	input_demo_android_quick_record_mission[0] = 0;
	input_demo_clear_quick_recording_extra();
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
	input_demo_capture_checkpoint_extra(settings);
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
	input_demo_fill_extra_player_cfg(settings);
	settings->player_cfg.no_fire_autoselect = PlayerCfg.NoFireAutoselect;
	settings->player_cfg.cycle_autoselect_only = PlayerCfg.CycleAutoselectOnly;
	settings->player_cfg.select_after_fire = PlayerCfg.SelectAfterFire;
	settings->player_cfg.classic_autoselect_weapon = PlayerCfg.ClassicAutoselectWeapon;
	settings->player_cfg.primary_order_count =
	    input_demo_primary_order_count();
	settings->player_cfg.secondary_order_count = MAX_SECONDARY_WEAPONS + 1;
	for (i = 0; i < (int) input_demo_primary_order_count(); i++)
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
		settings->game = input_demo_recorder_settings_game();
		settings->mission = input_demo_recorder_settings_mission();
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
	sprintf_s(base_name, SDL_arraysize(base_name), "%s_%s_level%d_%04d%02d%02d_%02d%02d%02d",
	          input_demo_quick_record_name_prefix(),
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

int maybe_start_input_demo_recording(int is_autorecord)
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
	           input_demo_recorder_settings_mission(), Current_level_num);
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

void input_demo_stop_recording_common(int is_manual)
{
	char demo_name[PATH_MAX] = "";
	char filename[PATH_MAX] = "";
	const char *input_demo_name = demo_name;
	int was_android_quick_recording = input_demo_android_quick_recording;
	int was_autorecord = Newdemo_is_autorecord;

	if (!input_demo_newdemo_record_no_space()) {
		input_demo_record_oneframeevent_update_common();
		newdemo_write_end();
	}

	PHYSFS_close(outfile);
	outfile = NULL;
	Newdemo_state = ND_STATE_NORMAL;
	Newdemo_is_autorecord = 0;
	gr_palette_load(gr_palette);

	if (was_android_quick_recording) {
		char input_demo_path[PATH_MAX] = "";

		input_demo_build_quick_record_name(demo_name, SDL_arraysize(demo_name));
		input_demo_build_classic_demo_path(filename, SDL_arraysize(filename), INPUT_DEMO_NEW_DIR, demo_name);
		sprintf_s(input_demo_path, SDL_arraysize(input_demo_path), "%s/%s" INPUT_DEMO_EXTENSION, INPUT_DEMO_NEW_DIR, demo_name);
		input_demo_clear_quick_recording();
		maybe_flush_input_demo_recording(demo_name, 1);
		if (!PHYSFSX_exists(input_demo_path, 0)) {
			PHYSFS_delete(INPUT_DEMO_TEMP_DEMO_FILENAME);
			return;
		}
		PHYSFS_delete(filename);
		if (!PHYSFSX_rename(INPUT_DEMO_TEMP_DEMO_FILENAME, filename)) {
			con_printf(CON_NORMAL, "Input demo classic demo sidecar save failed for %s\n", filename);
			PHYSFS_delete(INPUT_DEMO_TEMP_DEMO_FILENAME);
		} else {
			con_printf(CON_NORMAL, "Input demo classic demo saved to %s\n", filename);
		}
		return;
	}
	input_demo_clear_quick_recording();

	newdemo_get_default_filename(demo_name, SDL_arraysize(demo_name));
	if (is_manual || !was_autorecord || !PlayerCfg.AutoDemoHideUi)
		if (!newdemo_prompt_filename(demo_name, SDL_arraysize(demo_name))) {
			input_demo_name = INPUT_DEMO_TEMP_NAME;
			maybe_flush_input_demo_recording(input_demo_name, 0);
			return;
		}

	sprintf_s(filename, SDL_arraysize(filename), DEMO_DIR "%s" DEMO_EXT, demo_name);

	PHYSFS_delete(filename);
	PHYSFSX_rename(INPUT_DEMO_TEMP_DEMO_FILENAME, filename);
	maybe_flush_input_demo_recording(input_demo_name, 0);
}

int input_demo_stop_quick_recording_common(void)
{
	if (Newdemo_state != ND_STATE_RECORDING || !input_demo_android_quick_recording)
		return 0;
	input_demo_stop_recording_common(0);
	return 1;
}

int input_demo_toggle_quick_recording_common(void)
{
	char error[256] = "";

	if (Newdemo_state == ND_STATE_RECORDING) {
		if (!input_demo_stop_quick_recording_common()) {
			con_printf(CON_NORMAL, "Input demo quick toggle ignored: classic demo recording is already active\n");
			return 0;
		}
		return 1;
	}
	if (Newdemo_state != ND_STATE_NORMAL)
		return 0;
	if (!input_demo_prepare_recorder_settings(NULL, error, sizeof(error))) {
		con_printf(CON_NORMAL, "Input demo recording skipped: %s\n", error);
		return 0;
	}
	input_demo_quick_recording_start_prep();
	input_demo_android_quick_recording = 1;
	input_demo_android_quick_record_level = Current_level_num;
	snprintf(input_demo_android_quick_record_mission,
	         SDL_arraysize(input_demo_android_quick_record_mission),
	         "%s",
	         input_demo_quick_record_mission_name());
	newdemo_start_recording(1);
	if (Newdemo_state != ND_STATE_RECORDING || !input_demo_recorder_is_active()) {
		input_demo_clear_quick_recording();
		if (Newdemo_state == ND_STATE_RECORDING) {
			PHYSFS_close(outfile);
			outfile = NULL;
			Newdemo_state = ND_STATE_NORMAL;
			Newdemo_is_autorecord = 0;
			PHYSFS_delete(INPUT_DEMO_TEMP_DEMO_FILENAME);
			gr_palette_load(gr_palette);
		}
		if (Newdemo_state == ND_STATE_NORMAL)
			con_printf(CON_NORMAL, "Input demo recording did not start\n");
		return 0;
	}
	return 1;
}