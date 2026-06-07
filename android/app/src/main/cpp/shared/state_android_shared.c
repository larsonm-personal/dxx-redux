/* Shared Android helper bodies for d1/main/state.c and d2/main/state.c. */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "pstypes.h"
#include "args.h"
#include "config.h"
#include "digi.h"
#include "dxxerror.h"
#include "game.h"
#include "gameseq.h"
#include "gr.h"
#include "mission.h"
#include "object.h"
#include "player.h"
#include "render.h"
#include "state.h"
#include "u_mem.h"
#include "physfsx.h"
#include "android_resume_pilot.h"
#include "android_rewind.h"
#include "android_save_meta.h"
#include "android_save_set.h"
#include "coop_save.h"
#include "android_log.h"
#include "state_android_shared.h"

extern int state_save_all_sub(char *filename, char *desc);
#ifdef DXX_BUILD_DESCENT_II
extern int state_restore_all_sub(char *filename, int multi);
extern int Final_boss_is_dead;
extern int copy_file(char *old_file, char *new_file);
#else
extern int state_restore_all_sub(char *filename);
#endif
extern int Player_is_dead;

enum {
	STATE_ANDROID_DESC_LENGTH = 20,
	STATE_ANDROID_NUM_SAVES = 10,
	STATE_ANDROID_PERIODIC_AUTOSAVE_SECONDS = 300,
	STATE_ANDROID_PERIODIC_AUTOSAVE_RETRY_SECONDS = 10
};

static const char *state_android_memory_file_name(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return "__rewind__.d2sg";
#else
	return "__rewind__.d1sg";
#endif
}

static int state_android_save_meta_game_id(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return ANDROID_SAVE_META_GAME_D2;
#else
	return ANDROID_SAVE_META_GAME_D1;
#endif
}

static const char *state_android_game_label(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return "D2";
#else
	return "D1";
#endif
}

static const char *state_android_current_mission_filename_or_default(void)
{
	return (Current_mission && Current_mission_filename)
	           ? Current_mission_filename
	           : "";
}

static const char *state_android_current_scope(int coop)
{
	return coop ? "coop" : "single";
}

static int state_android_last_save_set_path(char *filename, size_t filename_size,
                                            int coop)
{
	return snprintf(filename, filename_size, "%s/last_%s.txt",
	                GameArg.SysUsePlayersDir ? ANDROID_SAVE_SET_ROOT_PLAYERS
	                                         : ANDROID_SAVE_SET_ROOT_LOCAL,
	                state_android_current_scope(coop)) < (int) filename_size;
}

void state_android_ensure_parent_dirs_for_path(const char *filename)
{
	char path[PATH_MAX];
	char *p;

	if (!filename || !filename[0])
		return;
	snprintf(path, sizeof(path), "%s", filename);
	for (p = path; *p; p++) {
		if (*p != '/')
			continue;
		*p = '\0';
		if (path[0])
			PHYSFS_mkdir(path);
		*p = '/';
	}
}

static int state_android_read_last_save_set(int coop, char *pilot,
                                            size_t pilot_size, char *mission,
                                            size_t mission_size)
{
	char filename[PATH_MAX];
	char buf[128];
	PHYSFS_file *fp;
	PHYSFS_sint64 len;
	char *newline;

	if (!pilot || !pilot_size || !mission || !mission_size)
		return 0;
	if (!state_android_last_save_set_path(filename, sizeof(filename), coop))
		return 0;
	fp = PHYSFS_openRead(filename);
	if (!fp)
		return 0;
	len = PHYSFS_fileLength(fp);
	if (len <= 0 || len >= (PHYSFS_sint64) sizeof(buf)) {
		PHYSFS_close(fp);
		return 0;
	}
	if (PHYSFS_readBytes(fp, buf, (PHYSFS_uint64) len) != len) {
		PHYSFS_close(fp);
		return 0;
	}
	PHYSFS_close(fp);
	buf[len] = '\0';
	newline = strchr(buf, '\n');
	if (!newline)
		return 0;
	*newline = '\0';
	snprintf(pilot, pilot_size, "%s", buf);
	snprintf(mission, mission_size, "%s", newline + 1);
	newline = strchr(mission, '\n');
	if (newline)
		*newline = '\0';
	return pilot[0] && mission[0];
}

static void state_android_write_last_save_set(int coop, const char *pilot,
                                              const char *mission)
{
	char filename[PATH_MAX];
	char buf[128];
	PHYSFS_file *fp;
	int len;

	if (!pilot || !pilot[0] || !mission || !mission[0])
		return;
	if (!state_android_last_save_set_path(filename, sizeof(filename), coop))
		return;
	state_android_ensure_parent_dirs_for_path(filename);
	len = snprintf(buf, sizeof(buf), "%s\n%s\n", pilot, mission);
	if (len <= 0 || len >= (int) sizeof(buf))
		return;
	fp = PHYSFS_openWrite(filename);
	if (!fp)
		return;
	PHYSFS_writeBytes(fp, buf, (PHYSFS_uint64) len);
	PHYSFS_close(fp);
}

int state_android_build_save_filename(char *filename, size_t filename_size,
                                      int slotnum, int coop, int for_save)
{
	char pilot[CALLSIGN_LEN + 1];
	char mission[16];

	if (!filename || !filename_size)
		return 0;
	memset(pilot, 0, sizeof(pilot));
	memset(mission, 0, sizeof(mission));
	if (!for_save && !coop &&
	    state_android_read_last_save_set(coop, pilot, sizeof(pilot), mission,
	                                     sizeof(mission))) {
		return android_save_set_build_slot_path(
		    filename, filename_size, GameArg.SysUsePlayersDir,
		    state_android_current_scope(coop), pilot, mission, slotnum, coop);
	}
	snprintf(pilot, sizeof(pilot), "%s", Players[Player_num].callsign);
	snprintf(mission, sizeof(mission), "%s",
	         state_android_current_mission_filename_or_default());
	return android_save_set_build_slot_path(
	    filename, filename_size, GameArg.SysUsePlayersDir,
	    state_android_current_scope(coop), pilot, mission, slotnum, coop);
}

int state_android_build_coop_autosave_filename(char *filename,
                                               size_t filename_size,
                                               int slotnum)
{
	return android_save_set_build_slot_path(
	    filename, filename_size, GameArg.SysUsePlayersDir, "coop",
	    ANDROID_SAVE_SET_COOP_CALLSIGN,
	    state_android_current_mission_filename_or_default(), slotnum, 1);
}

int state_android_build_secret_filename(char *filename, size_t filename_size,
                                        int slotnum)
{
	return android_save_set_build_secret_path(
	    filename, filename_size, GameArg.SysUsePlayersDir,
	    Players[Player_num].callsign,
	    state_android_current_mission_filename_or_default(), slotnum);
}

int state_android_build_coop_sidecar_filename(char *filename,
                                              size_t filename_size,
                                              const char *sidecar_name)
{
	return android_save_set_build_sidecar_path(
	    filename, filename_size, GameArg.SysUsePlayersDir, "coop",
	    ANDROID_SAVE_SET_COOP_CALLSIGN,
	    state_android_current_mission_filename_or_default(), sidecar_name);
}

static int state_android_restore_from_memory_call(const char *filename)
{
#ifdef DXX_BUILD_DESCENT_II
	return state_restore_all_sub((char *) filename, 0);
#else
	return state_restore_all_sub((char *) filename);
#endif
}

static int state_android_autosave_precheck(int slotnum)
{
	(void) slotnum;
#ifdef DXX_BUILD_DESCENT_II
	if (Current_level_num < 0) {
		debug_log(DLOG_GAME, "autosave skipped: D2 secret level is active");
		return 0;
	}
	if (Final_boss_is_dead) {
		debug_log(DLOG_GAME, "autosave skipped: D2 final boss death sequence is active");
		return 0;
	}
#endif
	return 1;
}

static void state_android_autosave_prepare_slot(int slotnum)
{
#ifdef DXX_BUILD_DESCENT_II
	char state_android_temp_fname[PATH_MAX];

	if (!state_android_build_secret_filename(
	        state_android_temp_fname, sizeof(state_android_temp_fname), slotnum))
		return;
	if (PHYSFSX_exists(state_android_temp_fname, 0)) {
		if (!PHYSFS_delete(state_android_temp_fname))
			Error("Cannot delete file <%s>: %s", state_android_temp_fname,
			      PHYSFS_getLastError());
	}
	if (PHYSFSX_exists(SECRETC_FILENAME, 0)) {
		int copy_result;
		state_android_ensure_parent_dirs_for_path(state_android_temp_fname);
		copy_result = copy_file(SECRETC_FILENAME, state_android_temp_fname);
		Assert(copy_result == 0);
		(void) copy_result;
	}
#else
	(void) slotnum;
#endif
}

static uint32_t state_time_to_seconds(fix time_value, sbyte hours_value)
{
	if (hours_value < 0)
		hours_value = 0;
	if (time_value < 0)
		time_value = 0;
	return (uint32_t) hours_value * 3600u + (uint32_t) f2i(time_value);
}

static int g_android_save_meta_kind = ANDROID_SAVE_META_KIND_MANUAL;
static rewind_memory_buffer *g_state_android_memory_write_buffer = NULL;
static const rewind_memory_buffer *g_state_android_memory_read_buffer = NULL;
int g_android_save_blank_thumbnail = 0;

typedef struct state_android_periodic_autosave_state {
	int initialized;
	int level_num;
	int next_slot;
	fix64 next_save_time;
	fix64 last_game_time;
	char callsign[CALLSIGN_LEN + 1];
	char mission[ANDROID_SAVE_META_MISSION_LEN + 1];
} state_android_periodic_autosave_state;

static state_android_periodic_autosave_state g_state_android_periodic_autosave = {
	0, 0, ANDROID_SAVE_META_SLOT_AUTO_PERIODIC_A, 0, 0, "", ""
};

static void state_android_current_mission_name(char *mission_name,
                                               size_t mission_name_size)
{
	if (!mission_name || !mission_name_size)
		return;
	memset(mission_name, 0, mission_name_size);
	strncpy(mission_name, state_android_current_mission_filename_or_default(),
	        mission_name_size - 1);
}

static fix64 state_android_periodic_interval(void)
{
	return (fix64) F1_0 * STATE_ANDROID_PERIODIC_AUTOSAVE_SECONDS;
}

static fix64 state_android_periodic_retry_interval(void)
{
	return (fix64) F1_0 * STATE_ANDROID_PERIODIC_AUTOSAVE_RETRY_SECONDS;
}

static void state_android_periodic_autosave_reset(fix64 now,
                                                  const char *callsign,
                                                  const char *mission)
{
	state_android_periodic_autosave_state *state =
	    &g_state_android_periodic_autosave;

	memset(state, 0, sizeof(*state));
	state->initialized = 1;
	state->level_num = Current_level_num;
	state->next_slot = ANDROID_SAVE_META_SLOT_AUTO_PERIODIC_A;
	state->next_save_time = now + state_android_periodic_interval();
	state->last_game_time = now;
	strncpy(state->callsign, callsign, sizeof(state->callsign) - 1);
	strncpy(state->mission, mission, sizeof(state->mission) - 1);
}

static int state_android_periodic_context_changed(fix64 now,
                                                  const char *callsign,
                                                  const char *mission)
{
	const state_android_periodic_autosave_state *state =
	    &g_state_android_periodic_autosave;

	if (!state->initialized)
		return 1;
	if (state->level_num != Current_level_num)
		return 1;
	if (strcmp(state->callsign, callsign))
		return 1;
	if (strcmp(state->mission, mission))
		return 1;
	return now < state->last_game_time;
}

static int state_android_read_save_meta_for_slot(int slotnum,
                                                 android_save_meta_disk *meta)
{
	char filename[PATH_MAX];
	PHYSFS_file *fp;
	int result;

	if (!meta)
		return 0;
	state_android_build_save_filename(filename, sizeof(filename), slotnum, 0, 1);
	fp = PHYSFSX_openReadBuffered(filename);
	if (!fp)
		return 0;
	result = android_save_meta_read_physfs(fp, PHYSFS_fileLength(fp), meta);
	PHYSFS_close(fp);
	return result;
}

static int state_android_meta_matches_active_level_set(
    const android_save_meta_disk *meta)
{
	char mission_name[ANDROID_SAVE_META_MISSION_LEN + 1];

	if (!meta)
		return 0;
	if (meta->game_id != state_android_save_meta_game_id())
		return 0;
	if (strcmp(meta->callsign, Players[Player_num].callsign))
		return 0;
	state_android_current_mission_name(mission_name, sizeof(mission_name));
	return !strcmp(meta->mission_name, mission_name);
}

static int state_android_current_progress_beats(
    const android_save_meta_disk *old_meta)
{
	uint32_t level_seconds;
	uint32_t total_seconds;

	if (!old_meta)
		return 1;
	if (Current_level_num > old_meta->level_num)
		return 1;
	if (Current_level_num < old_meta->level_num)
		return 0;
	level_seconds = state_time_to_seconds(Players[Player_num].time_level,
	                                      Players[Player_num].hours_level);
	total_seconds = state_time_to_seconds(Players[Player_num].time_total,
	                                      Players[Player_num].hours_total);
	if (total_seconds > old_meta->total_seconds)
		return 1;
	if (total_seconds < old_meta->total_seconds)
		return 0;
	return level_seconds > old_meta->level_seconds;
}

static void state_android_memory_filename(char *filename, size_t filename_size)
{
	if (!filename || !filename_size)
		return;
	snprintf(filename, filename_size,
	         GameArg.SysUsePlayersDir ? "Players/%s" : "%s",
	         state_android_memory_file_name());
}

static int state_android_is_memory_filename(const char *filename)
{
	char expected[PATH_MAX];

	state_android_memory_filename(expected, sizeof(expected));
	return filename && !strcmp(filename, expected);
}

rewind_file *state_android_open_read_buffered(const char *filename)
{
	rewind_file *file = (rewind_file *) d_malloc(sizeof(*file));
	PHYSFS_file *physfs_file;

	if (state_android_is_memory_filename(filename)) {
		if (!g_state_android_memory_read_buffer) {
			d_free(file);
			return NULL;
		}
		rewind_file_init_memory_read(file,
		                             g_state_android_memory_read_buffer->data,
		                             g_state_android_memory_read_buffer->size);
		return file;
	}
	physfs_file = PHYSFSX_openReadBuffered(filename);
	if (!physfs_file) {
		d_free(file);
		return NULL;
	}
	rewind_file_init_physfs(file, physfs_file);
	return file;
}

rewind_file *state_android_open_write_buffered(const char *filename)
{
	rewind_file *file = (rewind_file *) d_malloc(sizeof(*file));
	PHYSFS_file *physfs_file;

	if (state_android_is_memory_filename(filename)) {
		if (!g_state_android_memory_write_buffer) {
			d_free(file);
			return NULL;
		}
		rewind_file_init_memory_write(file, g_state_android_memory_write_buffer);
		return file;
	}
	state_android_ensure_parent_dirs_for_path(filename);
	physfs_file = PHYSFSX_openWriteBuffered(filename);
	if (!physfs_file) {
		d_free(file);
		return NULL;
	}
	rewind_file_init_physfs(file, physfs_file);
	return file;
}

int state_android_close_file(rewind_file *file)
{
	int result = rewind_file_close(file);
	d_free(file);
	return result;
}

void state_android_write_save_metadata(rewind_file *fp, const char *desc,
                                       const char *mission_filename)
{
	struct PHYSFS_File *physfs_fp;
	android_save_meta_write_params android_params;
	char android_desc[STATE_ANDROID_DESC_LENGTH + 1];

	if (rewind_file_is_memory(fp))
		return;
	physfs_fp = rewind_file_physfs_handle(fp);
	coop_write_save_metadata(physfs_fp);
	memset(&android_params, 0, sizeof(android_params));
	memcpy(android_desc, desc, STATE_ANDROID_DESC_LENGTH);
	android_desc[STATE_ANDROID_DESC_LENGTH] = '\0';
	android_params.game_id = state_android_save_meta_game_id();
	android_params.save_kind = g_android_save_meta_kind;
	android_params.music_type = GameCfg.MusicType;
	android_params.callsign = Players[Player_num].callsign;
	android_params.description = android_desc;
	android_params.mission_name = mission_filename;
	android_params.level_num = Current_level_num;
	android_params.level_name = Current_level_name;
	android_params.level_seconds = state_time_to_seconds(
	    Players[Player_num].time_level, Players[Player_num].hours_level);
	android_params.total_seconds = state_time_to_seconds(
	    Players[Player_num].time_total, Players[Player_num].hours_total);
	android_params.difficulty_changed = Difficulty_level_changed ? 1 : 0;
	android_params.difficulty_min = Difficulty_level_min_seen;
	android_params.difficulty_max = Difficulty_level_max_seen;
	android_save_meta_apply_cached_thumbnail(&android_params);
	android_save_meta_write_physfs(physfs_fp, &android_params);
	state_android_write_last_save_set(
	    (Game_mode & GM_MULTI_COOP) ? 1 : 0, Players[Player_num].callsign,
	    mission_filename);
}

void state_android_restore_music_type_from_meta(const android_save_meta_disk *meta)
{
	if (!meta || meta->music_type > MUSIC_TYPE_CUSTOM)
		return;
	if (GameCfg.MusicType == meta->music_type)
		return;
	debug_log(DLOG_GAME, "restore applying saved music type: game=%s old=%d new=%d",
	          state_android_game_label(), GameCfg.MusicType, meta->music_type);
	GameCfg.MusicType = meta->music_type;
}

void state_android_prepare_modal_error_background(const char *reason)
{
	if (Game_wind)
		return;
	debug_log(DLOG_GAME, "modal error background clear: game=%s reason='%s'",
	          state_android_game_label(), reason ? reason : "");
	gr_set_current_canvas(NULL);
	gr_clear_canvas(BM_XRGB(0, 0, 0));
}

int state_android_save_to_path(const char *filename, const char *desc,
                               int save_kind, int blank_thumbnail)
{
	int result;
	char save_filename[PATH_MAX];
	char save_desc[STATE_ANDROID_DESC_LENGTH + 1];
	int prev_kind = g_android_save_meta_kind;
	int prev_blank = g_android_save_blank_thumbnail;

	if (!filename || !filename[0] || !desc)
		return 0;
	memset(save_filename, 0, sizeof(save_filename));
	memset(save_desc, 0, sizeof(save_desc));
	strncpy(save_filename, filename, PATH_MAX - 1);
	strncpy(save_desc, desc, STATE_ANDROID_DESC_LENGTH);
	android_save_meta_clear_cached_thumbnail();
	g_android_save_meta_kind = save_kind;
	g_android_save_blank_thumbnail = blank_thumbnail ? 1 : 0;
	result = state_save_all_sub(save_filename, save_desc);
	g_android_save_meta_kind = prev_kind;
	g_android_save_blank_thumbnail = prev_blank;
	android_save_meta_clear_cached_thumbnail();
	return result;
}

static void state_android_save_highest_progress_if_needed(void)
{
	android_save_meta_disk old_meta;
	android_save_meta_disk *old_meta_ptr = NULL;
	char filename[PATH_MAX];
	int have_old_meta;
	int result;

	if (Current_level_num <= 0)
		return;
	have_old_meta = state_android_read_save_meta_for_slot(
	    ANDROID_SAVE_META_SLOT_AUTO_PROGRESS, &old_meta);
	if (have_old_meta && state_android_meta_matches_active_level_set(&old_meta))
		old_meta_ptr = &old_meta;
	if (old_meta_ptr && !state_android_current_progress_beats(old_meta_ptr)) {
		debug_log(DLOG_GAME,
		          "autosave progress skipped: %s level=%d old_level=%d",
		          state_android_game_label(), Current_level_num, old_meta.level_num);
		return;
	}

	stop_time();
	state_android_build_save_filename(filename, sizeof(filename),
	                                  ANDROID_SAVE_META_SLOT_AUTO_PROGRESS, 0,
	                                  1);
	state_android_autosave_prepare_slot(ANDROID_SAVE_META_SLOT_AUTO_PROGRESS);
	result = state_android_save_to_path(filename, "AUTO BEST",
	                                    ANDROID_SAVE_META_KIND_AUTO_PROGRESS, 0);
	if (result)
		debug_log(DLOG_GAME, "autosave progress saved: %s slot %d",
		          state_android_game_label(), ANDROID_SAVE_META_SLOT_AUTO_PROGRESS);
	else
		debug_log(DLOG_GAME, "autosave progress failed: %s slot %d",
		          state_android_game_label(), ANDROID_SAVE_META_SLOT_AUTO_PROGRESS);
}

int state_save_to_memory(rewind_memory_buffer *buffer, const char *desc,
                         int save_kind, int blank_thumbnail)
{
	int result;
	char filename[PATH_MAX];

	if (!buffer || !desc)
		return 0;
	state_android_memory_filename(filename, sizeof(filename));
	g_state_android_memory_write_buffer = buffer;
	result = state_android_save_to_path(filename, desc, save_kind, blank_thumbnail);
	g_state_android_memory_write_buffer = NULL;
	return result;
}

int state_restore_from_memory(const rewind_memory_buffer *buffer)
{
	int result;
	char filename[PATH_MAX];

	if (!buffer || (!buffer->data && buffer->size != 0))
		return 0;
	state_android_memory_filename(filename, sizeof(filename));
	g_state_android_memory_read_buffer = buffer;
	result = state_android_restore_from_memory_call(filename);
	g_state_android_memory_read_buffer = NULL;
	return result;
}

int state_android_save_to_slot(int slotnum, const char *desc, int save_kind)
{
	int result;
	char filename[PATH_MAX];

	if (!desc || slotnum < 0 || slotnum >= STATE_ANDROID_NUM_SAVES) {
		debug_log(DLOG_GAME, "autosave skipped: invalid %s slot request",
		          state_android_game_label());
		return 0;
	}
	if (!state_android_autosave_precheck(slotnum))
		return 0;
	if (Game_mode & GM_MULTI) {
		debug_log(DLOG_GAME, "autosave skipped: %s multiplayer is active",
		          state_android_game_label());
		return 0;
	}
	android_repair_player_callsign_for_autosave(state_android_game_label());
	if (save_kind == ANDROID_SAVE_META_KIND_AUTO_EXIT ||
	    save_kind == ANDROID_SAVE_META_KIND_AUTO_MINIMIZE)
		state_android_save_highest_progress_if_needed();

	stop_time();
	memset(filename, 0, sizeof(filename));
	state_android_build_save_filename(filename, sizeof(filename), slotnum, 0, 1);
	state_android_autosave_prepare_slot(slotnum);
	result = state_android_save_to_path(filename, desc, save_kind, 0);
	if (!result)
		debug_log(DLOG_GAME, "autosave failed: %s slot %d",
		          state_android_game_label(), slotnum);
	return result;
}

void state_android_maybe_periodic_autosave(void)
{
	state_android_periodic_autosave_state *state =
	    &g_state_android_periodic_autosave;
	char callsign[CALLSIGN_LEN + 1];
	char mission[ANDROID_SAVE_META_MISSION_LEN + 1];
	int slotnum;
	int result;

	if ((Game_mode & GM_MULTI) || Current_level_num <= 0) {
		state->initialized = 0;
		return;
	}
	memset(callsign, 0, sizeof(callsign));
	memset(mission, 0, sizeof(mission));
	strncpy(callsign, Players[Player_num].callsign, sizeof(callsign) - 1);
	state_android_current_mission_name(mission, sizeof(mission));
	if (state_android_periodic_context_changed(GameTime64, callsign, mission)) {
		state_android_periodic_autosave_reset(GameTime64, callsign, mission);
		return;
	}
	if (Player_is_dead) {
		state->next_save_time =
		    GameTime64 + state_android_periodic_retry_interval();
		state->last_game_time = GameTime64;
		return;
	}
	if (GameTime64 < state->next_save_time) {
		state->last_game_time = GameTime64;
		return;
	}

	slotnum = state->next_slot;
	state->next_slot =
	    slotnum == ANDROID_SAVE_META_SLOT_AUTO_PERIODIC_A
	        ? ANDROID_SAVE_META_SLOT_AUTO_PERIODIC_B
	        : ANDROID_SAVE_META_SLOT_AUTO_PERIODIC_A;
	state->next_save_time = GameTime64 + state_android_periodic_interval();
	state->last_game_time = GameTime64;
	result = state_android_save_to_slot(
	    slotnum, "AUTO 5MIN", ANDROID_SAVE_META_KIND_AUTO_PERIODIC);
	if (result)
		debug_log(DLOG_GAME, "autosave periodic saved: %s slot %d",
		          state_android_game_label(), slotnum);
	else
		debug_log(DLOG_GAME, "autosave periodic failed: %s slot %d",
		          state_android_game_label(), slotnum);
}

void state_android_restore_player_flight_state(void)
{
	object *obj;
	int objnum = Players[Player_num].objnum;

	if (objnum < 0 || objnum > Highest_object_index)
		return;

	Viewer = ConsoleObject = &Objects[objnum];
	obj = ConsoleObject;
	if (obj->type == OBJ_GHOST)
		obj->type = OBJ_PLAYER;
	if (obj->type != OBJ_PLAYER)
		return;

	if (Player_is_dead || obj->control_type != CT_FLYING || obj->movement_type != MT_PHYSICS ||
	    !(obj->mtype.phys_info.flags & PF_USES_THRUST))
		debug_log(DLOG_GAME,
		          "restore controls repaired: %s obj=%d ct=%d mt=%d flags=0x%x dead=%d",
		          state_android_game_label(), objnum, obj->control_type,
		          obj->movement_type, obj->mtype.phys_info.flags, Player_is_dead);

	Player_is_dead = 0;
	obj->control_type = CT_FLYING;
	obj->movement_type = MT_PHYSICS;
	obj->mtype.phys_info.flags |= PF_TURNROLL | PF_LEVELLING | PF_WIGGLE | PF_USES_THRUST;
}
