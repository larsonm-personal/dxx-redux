/* Shared Android helper bodies for d1/main/state.c and d2/main/state.c. */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "pstypes.h"
#include "args.h"
#include "dxxerror.h"
#include "game.h"
#include "gameseq.h"
#include "object.h"
#include "player.h"
#include "render.h"
#include "state.h"
#include "u_mem.h"
#include "physfsx.h"
#include "android_resume_pilot.h"
#include "android_rewind.h"
#include "android_save_meta.h"
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
	STATE_ANDROID_NUM_SAVES = 10
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
	char state_android_fc;

	if (slotnum >= 10)
		state_android_fc = (slotnum - 10) + 'a';
	else
		state_android_fc = '0' + slotnum;
	sprintf(state_android_temp_fname,
	        GameArg.SysUsePlayersDir ? "Players/%csecret.sgc" : "%csecret.sgc",
	        state_android_fc);
	if (PHYSFSX_exists(state_android_temp_fname, 0)) {
		if (!PHYSFS_delete(state_android_temp_fname))
			Error("Cannot delete file <%s>: %s", state_android_temp_fname,
			      PHYSFS_getLastError());
	}
	if (PHYSFSX_exists(SECRETC_FILENAME, 0)) {
		int copy_result = copy_file(SECRETC_FILENAME, state_android_temp_fname);
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
	android_params.callsign = Players[Player_num].callsign;
	android_params.description = android_desc;
	android_params.mission_name = mission_filename;
	android_params.level_num = Current_level_num;
	android_params.level_name = Current_level_name;
	android_params.level_seconds = state_time_to_seconds(
	    Players[Player_num].time_level, Players[Player_num].hours_level);
	android_params.total_seconds = state_time_to_seconds(
	    Players[Player_num].time_total, Players[Player_num].hours_total);
	android_save_meta_write_physfs(physfs_fp, &android_params);
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
	g_android_save_meta_kind = save_kind;
	g_android_save_blank_thumbnail = blank_thumbnail ? 1 : 0;
	result = state_save_all_sub(save_filename, save_desc);
	g_android_save_meta_kind = prev_kind;
	g_android_save_blank_thumbnail = prev_blank;
	return result;
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

	stop_time();
	memset(filename, 0, sizeof(filename));
	snprintf(filename, PATH_MAX,
	         GameArg.SysUsePlayersDir ? "Players/%s.sg%x" : "%s.sg%x",
	         Players[Player_num].callsign, slotnum);
	state_android_autosave_prepare_slot(slotnum);
	result = state_android_save_to_path(filename, desc, save_kind, 1);
	if (!result)
		debug_log(DLOG_GAME, "autosave failed: %s slot %d",
		          state_android_game_label(), slotnum);
	return result;
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
