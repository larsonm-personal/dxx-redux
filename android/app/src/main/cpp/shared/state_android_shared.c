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
#include "android_file_pair_transaction.h"
#include "android_rewind.h"
#include "android_save_meta.h"
#include "android_save_set.h"
#include "coop_save.h"
#include "android_log.h"
#include "state_android_shared.h"
#ifdef DXX_BUILD_DESCENT_II
#include "escort.h"
#endif

extern int state_save_all_sub(char *filename, char *desc);
#ifdef DXX_BUILD_DESCENT_II
extern int state_restore_all_sub(char *filename, int multi);
extern int Final_boss_is_dead;
extern int copy_file(char *old_file, char *new_file);
#else
extern int state_restore_all_sub(char *filename);
#endif
extern int Player_is_dead;

static int g_state_android_coop_callsign_remap_allowed;

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

	if (!pilot || !pilot[0])
		return;
	if (!mission || !mission[0])
		mission = "default";
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

#ifdef DXX_BUILD_DESCENT_II
static int state_android_pair_exists(void *context, const char *path)
{
	(void) context;
	return PHYSFSX_exists(path, 0);
}

static int state_android_pair_rename(void *context, const char *old_path,
                                     const char *new_path)
{
	(void) context;
	return PHYSFSX_rename(old_path, new_path);
}

static int state_android_pair_delete(void *context, const char *path)
{
	(void) context;
	return !PHYSFSX_exists(path, 0) || PHYSFS_delete(path);
}
#endif

static int state_android_publish_save_slot(const char *temp_filename,
                                           const char *filename, int slotnum)
{
#ifdef DXX_BUILD_DESCENT_II
	char main_backup[PATH_MAX];
	char companion[PATH_MAX];
	char companion_temp[PATH_MAX];
	char companion_backup[PATH_MAX];
	int companion_present;
	int copy_result;
	struct android_file_pair_paths paths;
	struct android_file_pair_ops ops = {
		NULL, state_android_pair_exists, state_android_pair_rename,
		state_android_pair_delete
	};

	if (!state_android_build_secret_filename(
	        companion, sizeof(companion), slotnum) ||
	    snprintf(main_backup, sizeof(main_backup), "%s.bak", filename) >=
	        (int) sizeof(main_backup) ||
	    snprintf(companion_temp, sizeof(companion_temp), "%s.tmp", companion) >=
	        (int) sizeof(companion_temp) ||
	    snprintf(companion_backup, sizeof(companion_backup), "%s.bak", companion) >=
	        (int) sizeof(companion_backup))
		return 0;
	PHYSFS_delete(companion_temp);
	companion_present = PHYSFSX_exists(SECRETC_FILENAME, 0);
	if (companion_present) {
		state_android_ensure_parent_dirs_for_path(companion_temp);
		copy_result = copy_file(SECRETC_FILENAME, companion_temp);
		if (copy_result) {
			PHYSFS_delete(companion_temp);
			debug_log(DLOG_GAME,
			          "autosave failed: D2 slot %d secret companion stage result=%d",
			          slotnum, copy_result);
			return 0;
		}
	}
	paths.primary_temp = temp_filename;
	paths.primary_path = filename;
	paths.primary_backup = main_backup;
	paths.companion_temp = companion_temp;
	paths.companion_path = companion;
	paths.companion_backup = companion_backup;
	paths.companion_present = companion_present;
	if (!android_file_pair_publish(&paths, &ops)) {
		debug_log(DLOG_GAME,
		          "autosave failed: D2 slot %d main/secret pair publish",
		          slotnum);
		return 0;
	}
	return 1;
#else
	(void) slotnum;
	return PHYSFSX_rename(temp_filename, filename);
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

static int state_android_validate_save_path(const char *filename, int save_kind)
{
	android_save_meta_disk meta;
	PHYSFS_file *fp;
	int result;

	if (!filename)
		return 0;
	fp = PHYSFSX_openReadBuffered(filename);
	if (!fp)
		return 0;
	result = android_save_meta_read_physfs(fp, PHYSFS_fileLength(fp), &meta) &&
	         meta.game_id == state_android_save_meta_game_id() &&
	         meta.save_kind == save_kind;
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

int state_android_read_android_metadata_trailer(rewind_file *file,
                                                android_save_meta_disk *meta)
{
	struct PHYSFS_File *physfs_file;
	PHYSFS_sint64 saved_pos;
	PHYSFS_sint64 file_len;
	int have_meta;

	if (!file || !meta)
		return 0;
	file_len = rewind_file_length(file);
	if (!rewind_file_is_memory(file)) {
		physfs_file = rewind_file_physfs_handle(file);
		return physfs_file ? android_save_meta_read_physfs(physfs_file, file_len, meta) : 0;
	}

	saved_pos = rewind_file_tell(file);
	have_meta = file_len >= (PHYSFS_sint64) sizeof(*meta) &&
	            rewind_file_seek(file, file_len - (PHYSFS_sint64) sizeof(*meta)) &&
	            rewind_file_read(file, meta, sizeof(*meta), 1) == 1 &&
	            android_save_meta_is_valid(meta);
	rewind_file_seek(file, saved_pos);
	return have_meta;
}

int state_android_read_coop_metadata_trailer(rewind_file *file,
                                             coop_save_metadata *meta)
{
	struct PHYSFS_File *physfs_file;
	PHYSFS_sint64 saved_pos;
	PHYSFS_sint64 file_len;
	PHYSFS_sint64 coop_trailer_end;
	android_save_meta_disk android_meta;
	int have_android_meta;
	int have_coop_meta = 0;

	if (!file || !meta)
		return 0;

	if (rewind_file_is_memory(file)) {
		saved_pos = rewind_file_tell(file);
		file_len = rewind_file_length(file);
		have_android_meta = state_android_read_android_metadata_trailer(
		    file, &android_meta);
		coop_trailer_end = file_len;
		if (have_android_meta)
			coop_trailer_end -=
			    (PHYSFS_sint64) sizeof(android_save_meta_disk);
		have_coop_meta = coop_read_save_metadata_rewind(
		    file, coop_trailer_end, meta);
		rewind_file_seek(file, saved_pos);
		return have_coop_meta;
	}

	physfs_file = rewind_file_physfs_handle(file);
	if (!physfs_file)
		return 0;

	saved_pos = rewind_file_tell(file);
	file_len = rewind_file_length(file);
	have_android_meta = android_save_meta_read_physfs(
	    physfs_file, file_len, &android_meta);

	coop_trailer_end = file_len;
	if (have_android_meta)
		coop_trailer_end -= (PHYSFS_sint64) sizeof(android_meta);
	if (coop_trailer_end >= 0)
		have_coop_meta = coop_read_save_metadata(
		    physfs_file, coop_trailer_end, meta);

	rewind_file_seek(file, saved_pos);
	return have_coop_meta;
}

int state_android_write_save_metadata(rewind_file *fp, const char *desc,
                                      const char *mission_filename)
{
	struct PHYSFS_File *physfs_fp;
	android_save_meta_write_params android_params;
	char android_desc[STATE_ANDROID_DESC_LENGTH + 1];

	if (rewind_file_is_memory(fp)) {
		return !(Game_mode & GM_MULTI_COOP) ||
		       coop_write_save_metadata_rewind(fp);
	}
	physfs_fp = rewind_file_physfs_handle(fp);
	if ((Game_mode & GM_MULTI_COOP) &&
	    !coop_write_save_metadata(physfs_fp))
		return 0;
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
#ifdef DXX_BUILD_DESCENT_II
	android_params.guidebot_route_target_mode = (uint8_t) escort_get_route_target_mode();
#endif
	android_save_meta_apply_cached_thumbnail(&android_params);
	if (!android_save_meta_write_physfs(physfs_fp, &android_params))
		return 0;
	state_android_write_last_save_set(
	    (Game_mode & GM_MULTI_COOP) ? 1 : 0, Players[Player_num].callsign,
	    mission_filename);
	return 1;
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

static int state_android_save_to_slot_internal(int slotnum, const char *desc,
                                               int save_kind,
                                               int blank_thumbnail);

static void state_android_save_highest_progress_if_needed(int blank_thumbnail)
{
	android_save_meta_disk old_meta;
	android_save_meta_disk *old_meta_ptr = NULL;
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

	result = state_android_save_to_slot_internal(
	    ANDROID_SAVE_META_SLOT_AUTO_PROGRESS, "AUTO BEST",
	    ANDROID_SAVE_META_KIND_AUTO_PROGRESS, blank_thumbnail);
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
	buffer->error = 0;
	result = state_android_save_to_path(filename, desc, save_kind, blank_thumbnail);
	g_state_android_memory_write_buffer = NULL;
	return result && !buffer->error;
}

static int state_restore_from_memory_internal(const rewind_memory_buffer *buffer,
                                              int allow_coop_callsign_remap)
{
	int result;
	char filename[PATH_MAX];

	if (!buffer || (!buffer->data && buffer->size != 0))
		return 0;
	state_android_memory_filename(filename, sizeof(filename));
	g_state_android_memory_read_buffer = buffer;
	g_state_android_coop_callsign_remap_allowed = allow_coop_callsign_remap;
	result = state_android_restore_from_memory_call(filename);
	g_state_android_coop_callsign_remap_allowed = 0;
	g_state_android_memory_read_buffer = NULL;
	return result;
}

int state_restore_from_memory(const rewind_memory_buffer *buffer)
{
	return state_restore_from_memory_internal(buffer, 0);
}

int state_restore_coop_from_memory(const rewind_memory_buffer *buffer)
{
	return state_restore_from_memory_internal(buffer, 1);
}

int state_android_coop_callsign_remap_allowed(void)
{
	return g_state_android_coop_callsign_remap_allowed;
}

static int state_android_save_to_slot_internal(int slotnum, const char *desc,
                                               int save_kind,
                                               int blank_thumbnail)
{
	int result;
	char filename[PATH_MAX];
#ifdef ANDROID
	char temp_filename[PATH_MAX];
#endif

	android_repair_player_callsign_for_autosave(state_android_game_label());
	if (save_kind == ANDROID_SAVE_META_KIND_AUTO_EXIT ||
	    save_kind == ANDROID_SAVE_META_KIND_AUTO_MINIMIZE)
		state_android_save_highest_progress_if_needed(blank_thumbnail);

	stop_time();
	memset(filename, 0, sizeof(filename));
	state_android_build_save_filename(filename, sizeof(filename), slotnum, 0, 1);
#ifdef ANDROID
	if (snprintf(temp_filename, sizeof(temp_filename), "%s.tmp", filename) >=
	    (int) sizeof(temp_filename)) {
		debug_log(DLOG_GAME, "autosave failed: %s slot %d temporary path too long",
		          state_android_game_label(), slotnum);
		return 0;
	}
	PHYSFS_delete(temp_filename);
	result = state_android_save_to_path(temp_filename, desc, save_kind,
	                                    blank_thumbnail);
	if (result && !state_android_validate_save_path(temp_filename, save_kind)) {
		debug_log(DLOG_GAME, "autosave failed: %s slot %d temporary save validation",
		          state_android_game_label(), slotnum);
		result = 0;
	}
	if (result &&
	    !state_android_publish_save_slot(temp_filename, filename, slotnum)) {
		result = 0;
	}
	if (!result)
		PHYSFS_delete(temp_filename);
#else
	result = state_android_save_to_path(filename, desc, save_kind,
	                                    blank_thumbnail);
#endif
	if (!result)
		debug_log(DLOG_GAME, "autosave failed: %s slot %d",
		          state_android_game_label(), slotnum);
	return result;
}

int state_android_save_to_slot(int slotnum, const char *desc, int save_kind)
{
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
	return state_android_save_to_slot_internal(slotnum, desc, save_kind, 0);
}

int state_android_save_lifecycle_checkpoint(int slotnum, const char *desc,
                                            int save_kind)
{
	if (!desc || slotnum < 0 || slotnum >= STATE_ANDROID_NUM_SAVES) {
		debug_log(DLOG_GAME,
		          "lifecycle checkpoint skipped: invalid %s slot request",
		          state_android_game_label());
		return 0;
	}
	if (Game_mode & GM_MULTI) {
		debug_log(DLOG_GAME,
		          "lifecycle checkpoint skipped: %s multiplayer is active",
		          state_android_game_label());
		return 0;
	}
	if (Current_level_num <= 0 || Player_is_dead) {
		debug_log(DLOG_GAME,
		          "lifecycle checkpoint skipped: %s level=%d dead=%d",
		          state_android_game_label(), Current_level_num, Player_is_dead);
		return 0;
	}
	if (!state_android_autosave_precheck(slotnum))
		return 0;
	return state_android_save_to_slot_internal(slotnum, desc, save_kind, 1)
	           ? 1
	           : -1;
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
