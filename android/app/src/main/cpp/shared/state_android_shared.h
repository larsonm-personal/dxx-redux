#ifndef STATE_ANDROID_SHARED_H
#define STATE_ANDROID_SHARED_H

/*
 * Shared Android helper bodies for d1/main/state.c and d2/main/state.c.
 * Including files provide the game headers, state prototypes, and per-game
 * adapter macros.
 */

#ifndef STATE_ANDROID_MEMORY_FILE_NAME
#error STATE_ANDROID_MEMORY_FILE_NAME must be defined before including state_android_shared.h
#endif

#ifndef STATE_ANDROID_SAVE_META_GAME_ID
#error STATE_ANDROID_SAVE_META_GAME_ID must be defined before including state_android_shared.h
#endif

#ifndef STATE_ANDROID_GAME_LABEL
#error STATE_ANDROID_GAME_LABEL must be defined before including state_android_shared.h
#endif

#ifndef STATE_ANDROID_RESTORE_FROM_MEMORY_CALL
#error STATE_ANDROID_RESTORE_FROM_MEMORY_CALL must be defined before including state_android_shared.h
#endif

#ifndef STATE_ANDROID_AUTOSAVE_PRECHECK
#define STATE_ANDROID_AUTOSAVE_PRECHECK(slotnum) ((void) 0)
#endif

#ifndef STATE_ANDROID_AUTOSAVE_PREPARE_SLOT
#define STATE_ANDROID_AUTOSAVE_PREPARE_SLOT(slotnum) ((void) 0)
#endif

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
static int g_android_save_blank_thumbnail = 0;

static void state_android_memory_filename(char *filename, size_t filename_size)
{
	if (!filename || !filename_size)
		return;
	snprintf(filename, filename_size,
	         GameArg.SysUsePlayersDir ? "Players/%s" : "%s", STATE_ANDROID_MEMORY_FILE_NAME);
}

static int state_android_is_memory_filename(const char *filename)
{
	char expected[PATH_MAX];

	state_android_memory_filename(expected, sizeof(expected));
	return filename && !strcmp(filename, expected);
}

static rewind_file *state_android_open_read_buffered(const char *filename)
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

static rewind_file *state_android_open_write_buffered(const char *filename)
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

static int state_android_close_file(rewind_file *file)
{
	int result = rewind_file_close(file);
	d_free(file);
	return result;
}

static void state_android_write_save_metadata(rewind_file *fp,
                                              const char *desc,
                                              const char *mission_filename)
{
	struct PHYSFS_File *physfs_fp;
	android_save_meta_write_params android_params;
	char android_desc[DESC_LENGTH + 1];

	if (rewind_file_is_memory(fp))
		return;
	physfs_fp = rewind_file_physfs_handle(fp);
	coop_write_save_metadata(physfs_fp);
	memset(&android_params, 0, sizeof(android_params));
	memcpy(android_desc, desc, DESC_LENGTH);
	android_desc[DESC_LENGTH] = '\0';
	android_params.game_id = STATE_ANDROID_SAVE_META_GAME_ID;
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

int state_android_save_to_path(const char *filename, const char *desc, int save_kind, int blank_thumbnail)
{
	int result;
	char save_filename[PATH_MAX];
	char save_desc[DESC_LENGTH + 1];
	int prev_kind = g_android_save_meta_kind;
	int prev_blank = g_android_save_blank_thumbnail;

	if (!filename || !filename[0] || !desc)
		return 0;
	memset(save_filename, 0, sizeof(save_filename));
	memset(save_desc, 0, sizeof(save_desc));
	strncpy(save_filename, filename, PATH_MAX - 1);
	strncpy(save_desc, desc, DESC_LENGTH);
	g_android_save_meta_kind = save_kind;
	g_android_save_blank_thumbnail = blank_thumbnail ? 1 : 0;
	result = state_save_all_sub(save_filename, save_desc);
	g_android_save_meta_kind = prev_kind;
	g_android_save_blank_thumbnail = prev_blank;
	return result;
}

int state_save_to_memory(rewind_memory_buffer *buffer, const char *desc, int save_kind, int blank_thumbnail)
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
	result = STATE_ANDROID_RESTORE_FROM_MEMORY_CALL(filename);
	g_state_android_memory_read_buffer = NULL;
	return result;
}

int state_android_save_to_slot(int slotnum, const char *desc, int save_kind)
{
	int result;
	char filename[PATH_MAX];

	if (!desc || slotnum < 0 || slotnum >= NUM_SAVES) {
		debug_log(DLOG_GAME, "autosave skipped: invalid %s slot request", STATE_ANDROID_GAME_LABEL);
		return 0;
	}
	STATE_ANDROID_AUTOSAVE_PRECHECK(slotnum);
	if (Game_mode & GM_MULTI) {
		debug_log(DLOG_GAME, "autosave skipped: %s multiplayer is active", STATE_ANDROID_GAME_LABEL);
		return 0;
	}
	android_repair_player_callsign_for_autosave(STATE_ANDROID_GAME_LABEL);

	stop_time();
	memset(filename, 0, sizeof(filename));
	snprintf(filename, PATH_MAX, GameArg.SysUsePlayersDir ? "Players/%s.sg%x" : "%s.sg%x",
	         Players[Player_num].callsign, slotnum);
	STATE_ANDROID_AUTOSAVE_PREPARE_SLOT(slotnum);
	result = state_android_save_to_path(filename, desc, save_kind, 1);
	if (!result)
		debug_log(DLOG_GAME, "autosave failed: %s slot %d", STATE_ANDROID_GAME_LABEL, slotnum);
	return result;
}

static void state_android_restore_player_flight_state(void)
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
		          STATE_ANDROID_GAME_LABEL, objnum, obj->control_type, obj->movement_type,
		          obj->mtype.phys_info.flags, Player_is_dead);

	Player_is_dead = 0;
	obj->control_type = CT_FLYING;
	obj->movement_type = MT_PHYSICS;
	obj->mtype.phys_info.flags |= PF_TURNROLL | PF_LEVELLING | PF_WIGGLE | PF_USES_THRUST;
}

#undef STATE_ANDROID_MEMORY_FILE_NAME
#undef STATE_ANDROID_SAVE_META_GAME_ID
#undef STATE_ANDROID_GAME_LABEL
#undef STATE_ANDROID_RESTORE_FROM_MEMORY_CALL
#undef STATE_ANDROID_AUTOSAVE_PRECHECK
#undef STATE_ANDROID_AUTOSAVE_PREPARE_SLOT

#endif