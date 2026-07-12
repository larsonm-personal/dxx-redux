/*
 * Coop save-file metadata extension -- shared implementation.
 * See coop_save.h for design rationale.
 */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include <physfs.h>

#include "coop_save.h"
#include "coop_restore_remap.h"

#include "player.h"
#include "multi.h"
#include "config.h"
#include "console.h"
#include "../android_log.h"
#include "../state_android_shared.h"
#include "weapon.h"
#include "state.h"
#include "args.h"
#include "endlevel.h"
#include "cntrlcen.h"
#include "game.h"
#include "gameseq.h"
#include "gameseg.h"
#include "hudmsg.h"
#include "mission.h"

#ifdef DXX_BUILD_DESCENT_II
#include "escort.h"
#endif

#ifdef DXX_BUILD_DESCENT_II
extern sbyte PKilledFlags[MAX_PLAYERS];
#endif

extern fix ThisLevelTime;

#ifdef ANDROID
int coop_remap_restored_players(rewind_file *file,
                                const player restore_players[MAX_PLAYERS],
                                const object restore_objects[MAX_PLAYERS],
                                int coop_player_got[MAX_PLAYERS])
{
	coop_save_metadata meta_early;
	int have_meta = state_android_read_coop_metadata_trailer(file, &meta_early);
	int got_players = 0;
	int i;

	for (i = 0; i < MAX_PLAYERS; i++) {
		object *obj;
		int saved_slot = -1;
		int saved_objnum;

		if (!(Players[i].connected == CONNECT_PLAYING ||
		      Players[i].connected == CONNECT_WAITING))
			continue;

		if (have_meta) {
			int meta_idx = coop_find_player_in_metadata(Players[i].callsign,
			                                            Netgame.players[i].client_id, &meta_early);
			if (meta_idx >= 0 && meta_idx < meta_early.num_active_players)
				saved_slot = meta_early.active_players[meta_idx].original_slot;
		}

		if (saved_slot < 0 || saved_slot >= MAX_PLAYERS ||
		    restore_players[saved_slot].connected != CONNECT_PLAYING) {
			COOPLOG("P%d '%s' not found in save -- spawning fresh", i,
			        Players[i].callsign);
			HUD_init_message(HM_MULTI, "'%s' not in save -- spawning fresh",
			                 Players[i].callsign);
			continue;
		}

		saved_objnum = Players[i].objnum;
		memcpy(&Players[i], &restore_players[saved_slot], sizeof(player));
		Players[i].objnum = saved_objnum;
		coop_player_got[i] = 1;
		got_players++;
		COOPLOG("mapped P%d '%s' -> save slot %d, objnum=%d", i,
		        Players[i].callsign, saved_slot, saved_objnum);

		obj = &Objects[Players[i].objnum];
		obj->id = i;
		obj->control_type = restore_objects[saved_slot].control_type;
		obj->movement_type = restore_objects[saved_slot].movement_type;
		obj->render_type = restore_objects[saved_slot].render_type;
		obj->flags = restore_objects[saved_slot].flags;
		obj->pos = restore_objects[saved_slot].pos;
		obj->orient = restore_objects[saved_slot].orient;
		obj->size = restore_objects[saved_slot].size;
		obj->shields = restore_objects[saved_slot].shields;
		obj->lifeleft = restore_objects[saved_slot].lifeleft;
		obj->mtype.phys_info = restore_objects[saved_slot].mtype.phys_info;
		obj->rtype.pobj_info = restore_objects[saved_slot].rtype.pobj_info;
		obj->type = OBJ_PLAYER;
		multi_reset_player_object(obj);
		update_object_seg(obj);
		COOPLOG("P%d post-reset: ct=%d mt=%d phys_flags=0x%x", i,
		        obj->control_type, obj->movement_type, obj->mtype.phys_info.flags);
	}

	return got_players;
}

void coop_normalize_restored_netgame_players(const char *game_name)
{
	int live_count = 0;
	int i;

	for (i = 0; i < MAX_PLAYERS; i++)
		if (Players[i].connected == CONNECT_PLAYING ||
		    Players[i].connected == CONNECT_WAITING)
			live_count++;
	if (live_count > Netgame.numplayers)
		Netgame.numplayers = live_count;
	if (live_count > Netgame.max_numplayers)
		Netgame.max_numplayers = live_count;
	Netgame.numconnected = live_count;
	if (Game_mode & GM_MULTI_COOP)
		COOPLOG("restore netgame live count: game=%s live=%d net_num=%d max=%d connected=%d",
		        game_name, live_count, Netgame.numplayers,
		        Netgame.max_numplayers, Netgame.numconnected);
}
#endif

#ifdef ANDROID
#define COOP_SAVE_LOG(level, fmt, ...) COOPLOG(fmt, ##__VA_ARGS__)
#else
#define COOP_SAVE_LOG(level, fmt, ...) con_printf(level, fmt, ##__VA_ARGS__)
#endif

static uint32_t coop_restore_flags_durable(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return PLAYER_FLAGS_QUAD_LASERS | PLAYER_FLAGS_MAP_ALL |
	       PLAYER_FLAGS_AMMO_RACK | PLAYER_FLAGS_CONVERTER |
	       PLAYER_FLAGS_AFTERBURNER | PLAYER_FLAGS_HEADLIGHT;
#else
	return PLAYER_FLAGS_QUAD_LASERS | PLAYER_FLAGS_MAP_ALL;
#endif
}

static int coop_auto_restore_timeout_frames(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return 300;
#else
	return 150;
#endif
}

static int coop_auto_restore_timeout_before_alive_check(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return 1;
#else
	return 0;
#endif
}

static void coop_write_metadata_extra(coop_save_metadata *meta)
{
#ifdef DXX_BUILD_DESCENT_II
	meta->escort_owner_player = (int8_t) Escort_owner_player;
	meta->buddy_allowed_to_talk = Buddy_allowed_to_talk ? 1 : 0;
#else
	(void) meta;
#endif
}

static void coop_auto_restore_trace(const char *fmt, ...)
{
#ifdef DXX_BUILD_DESCENT_II
	char buf[256];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	COOPLOG("%s", buf);
#else
	(void) fmt;
#endif
}

static void coop_log_player_status(void)
{
#ifdef DXX_BUILD_DESCENT_II
	int i;

	for (i = 0; i < N_players; i++)
		COOPLOG("  P%d '%s' connected=%d PKilled=%d",
		        i, Players[i].callsign, Players[i].connected, PKilledFlags[i]);
#endif
}

static void coop_auto_restore_log_no_slot(void)
{
#ifdef DXX_BUILD_DESCENT_II
	COOPLOG("no restore slot file from lobby");
#else
	COOP_SAVE_LOG(CON_NORMAL, "coop_save: no restore slot selected from lobby\n");
#endif
}

static void coop_auto_restore_log_slot_not_viable(int slot)
{
#ifdef DXX_BUILD_DESCENT_II
	COOPLOG("selected slot %d not viable", slot);
#else
	COOP_SAVE_LOG(CON_NORMAL, "coop_save: selected slot %d not viable\n", slot);
#endif
}

static void coop_auto_restore_log_armed(int slot, uint32_t gid)
{
#ifdef DXX_BUILD_DESCENT_II
	COOPLOG("auto-restore armed from lobby-selected slot %d (game_id=%u)", slot, gid);
#else
	COOP_SAVE_LOG(CON_NORMAL,
	              "coop_save: auto-restore armed from lobby-selected slot %d (game_id=%u)\n",
	              slot, gid);
#endif
}

static void coop_auto_restore_log_trigger(int slot, uint32_t gid, int frame)
{
#ifdef DXX_BUILD_DESCENT_II
	COOPLOG("triggering auto-restore from slot %d (game_id=%u) at frame %d",
	        slot, gid, frame);
#else
	(void) frame;
	COOP_SAVE_LOG(CON_NORMAL,
	              "coop_save: triggering auto-restore from slot %d (game_id=%u)\n",
	              slot, gid);
#endif
}

/* --- forward declarations for static helpers --- */
static void coop_write_autosave_history(int slot, int n_connected);
static void coop_write_text_file(const char *filename, const char *buf);
static void coop_append_other_slots(char *buf, int *off, int buf_size,
                                    const char *old_json, int exclude_slot);
static void coop_write_progress_inventory_file(const char *filename);

/* --- absent player tracking --- */

static coop_player_record coop_absent_list[COOP_MAX_REMEMBERED_PLAYERS];
static int16_t coop_absent_source_levels[COOP_MAX_REMEMBERED_PLAYERS];
static int coop_num_absent = 0;

void coop_snapshot_player(int pnum, coop_player_record *rec)
{
	const player *p = &Players[pnum];
	int i;

	memset(rec, 0, sizeof(*rec));
	strncpy(rec->callsign, p->callsign, COOP_CALLSIGN_LEN);
	rec->callsign[COOP_CALLSIGN_LEN] = '\0';
	strncpy(rec->client_id, Netgame.players[pnum].client_id, COOP_CLIENT_ID_LEN);
	rec->client_id[COOP_CLIENT_ID_LEN] = '\0';
	rec->score = p->score;
	rec->was_connected = 1;
	rec->energy = p->energy;
	rec->shields = p->shields;
	rec->laser_level = p->laser_level;
	rec->primary_weapon_flags = p->primary_weapon_flags;
	rec->secondary_weapon_flags = p->secondary_weapon_flags;
	for (i = 0; i < MAX_PRIMARY_WEAPONS && i < COOP_SAVE_MAX_WEAPONS; i++)
		rec->primary_ammo[i] = p->primary_ammo[i];
	for (i = 0; i < MAX_SECONDARY_WEAPONS && i < COOP_SAVE_MAX_WEAPONS; i++)
		rec->secondary_ammo[i] = p->secondary_ammo[i];
	rec->flags = p->flags;
	rec->net_kills_total = p->net_kills_total;
	rec->net_killed_total = p->net_killed_total;
	rec->num_kills_total = p->num_kills_total;
	rec->hostages_rescued_total = p->hostages_rescued_total;
	rec->time_total = p->time_total;
	rec->hours_total = p->hours_total;
	rec->original_slot = (uint8_t) pnum;
}

void coop_write_save_metadata(void *fp)
{
	coop_save_metadata meta;
	int i;

	if (!(Game_mode & GM_MULTI_COOP))
		return;

	memset(&meta, 0, sizeof(meta));
	meta.tag = COOP_SAVE_META_TAG;
	meta.version = COOP_SAVE_META_VER;
	meta.wall_clock_timestamp = (uint32_t) time(NULL);
	meta.level_num = Current_level_num;
	strncpy(meta.mission_name, Netgame.mission_name, 8);
	meta.mission_name[8] = '\0';
	meta.difficulty = Netgame.difficulty;
	meta.difficulty_changed = Difficulty_level_changed ? 1 : 0;
	meta.difficulty_min = (uint8_t) Difficulty_level_min_seen;
	meta.difficulty_max = (uint8_t) Difficulty_level_max_seen;
	coop_write_metadata_extra(&meta);

	meta.num_active_players = 0;
	for (i = 0; i < MAX_PLAYERS; i++) {
		if (Players[i].connected == CONNECT_PLAYING) {
			coop_snapshot_player(i, &meta.active_players[meta.num_active_players]);
			meta.num_active_players++;
		}
	}

	/* Copy absent player records into the metadata trailer */
	meta.num_absent_players = 0;
	for (i = 0; i < coop_num_absent && i < COOP_MAX_REMEMBERED_PLAYERS; i++) {
		memcpy(&meta.absent_players[i], &coop_absent_list[i], sizeof(coop_player_record));
		meta.num_absent_players++;
	}

	PHYSFS_write((PHYSFS_file *) fp, &meta, sizeof(meta), 1);
	COOP_SAVE_LOG(CON_DEBUG, "coop_save: wrote metadata trailer (%d active, %d absent)\n",
	              meta.num_active_players, meta.num_absent_players);
}

int coop_read_save_metadata(void *fp, PHYSFS_sint64 expected_end,
                            coop_save_metadata *meta)
{
	uint32_t tag;
	PHYSFS_sint64 file_len, bytes_to_read, rest_len;

	/* The metadata trailer starts right after the existing save data */
	if (PHYSFS_seek((PHYSFS_file *) fp, expected_end) == 0)
		return 0;

	/* Check if there's enough data for at least the tag */
	file_len = PHYSFS_fileLength((PHYSFS_file *) fp);
	if (file_len < expected_end + (PHYSFS_sint64) sizeof(uint32_t))
		return 0;

	if (PHYSFS_read((PHYSFS_file *) fp, &tag, sizeof(tag), 1) != 1)
		return 0;
	if (tag != COOP_SAVE_META_TAG)
		return 0;

	/* Read the rest of the struct (skip the tag we already read) */
	memset(meta, 0, sizeof(*meta));
	meta->tag = tag;
	rest_len = file_len - expected_end - (PHYSFS_sint64) sizeof(uint32_t);
	bytes_to_read = (PHYSFS_sint64) sizeof(*meta) - (PHYSFS_sint64) sizeof(uint32_t);
	if (rest_len < bytes_to_read)
		bytes_to_read = rest_len;
	if (bytes_to_read <= 0 ||
	    PHYSFS_read((PHYSFS_file *) fp, ((char *) meta) + sizeof(uint32_t),
	                (uint) bytes_to_read, 1) != 1) {
		con_printf(CON_URGENT, "coop_save: truncated metadata trailer\n");
		return 0;
	}

	/* Basic sanity */
	if (meta->version < 1 || meta->num_active_players > 8 ||
	    meta->num_absent_players > COOP_MAX_REMEMBERED_PLAYERS) {
		con_printf(CON_URGENT, "coop_save: invalid metadata (ver=%d, active=%d, absent=%d)\n",
		           meta->version, meta->num_active_players, meta->num_absent_players);
		return 0;
	}
	if (meta->version < 4) {
		meta->difficulty_changed = 0;
		meta->difficulty_min = meta->difficulty;
		meta->difficulty_max = meta->difficulty;
	}
	if (meta->difficulty_changed > 1 || meta->difficulty > 4 ||
	    meta->difficulty_min > 4 ||
	    meta->difficulty_max > 4 || meta->difficulty_min > meta->difficulty_max) {
		con_printf(CON_URGENT, "coop_save: invalid difficulty metadata (difficulty=%d, changed=%d, min=%d, max=%d)\n",
		           meta->difficulty, meta->difficulty_changed,
		           meta->difficulty_min, meta->difficulty_max);
		return 0;
	}

	COOP_SAVE_LOG(CON_DEBUG, "coop_save: read metadata trailer (ver=%d, %d active, %d absent)\n",
	              meta->version, meta->num_active_players, meta->num_absent_players);
	return 1;
}

int coop_find_player_in_metadata(const char *callsign,
                                 const char *client_id,
                                 const coop_save_metadata *meta)
{
	int i;

	/* Prefer client_id match (active first, then absent) */
	if (client_id && client_id[0]) {
		for (i = 0; i < meta->num_active_players; i++)
			if (strncmp(meta->active_players[i].client_id, client_id, COOP_CLIENT_ID_LEN) == 0)
				return i;
		for (i = 0; i < meta->num_absent_players; i++)
			if (strncmp(meta->absent_players[i].client_id, client_id, COOP_CLIENT_ID_LEN) == 0)
				return 8 + i;
	}

	/* Fallback: callsign match (case-insensitive) */
	if (callsign && callsign[0]) {
		for (i = 0; i < meta->num_active_players; i++)
			if (strncasecmp(meta->active_players[i].callsign, callsign, COOP_CALLSIGN_LEN) == 0)
				return i;
		for (i = 0; i < meta->num_absent_players; i++)
			if (strncasecmp(meta->absent_players[i].callsign, callsign, COOP_CALLSIGN_LEN) == 0)
				return 8 + i;
	}

	return -1;
}

void coop_track_absent_player(int pnum)
{
	int i;
	coop_player_record rec;

	if (!(Game_mode & GM_MULTI_COOP))
		return;
	if (pnum < 0 || pnum >= MAX_PLAYERS)
		return;
	if (!Players[pnum].callsign[0])
		return;

	coop_snapshot_player(pnum, &rec);
	rec.was_connected = 0;

	for (i = 0; i < coop_num_absent; i++) {
		if ((rec.client_id[0] && strncmp(coop_absent_list[i].client_id, rec.client_id, COOP_CLIENT_ID_LEN) == 0) ||
		    strncasecmp(coop_absent_list[i].callsign, rec.callsign, COOP_CALLSIGN_LEN) == 0) {
			memcpy(&coop_absent_list[i], &rec, sizeof(rec));
			coop_absent_source_levels[i] = (int16_t) Current_level_num;
			COOP_SAVE_LOG(CON_NORMAL, "coop_save: updated absent player '%s' (slot %d)\n",
			              rec.callsign, i);
			return;
		}
	}

	if (coop_num_absent >= COOP_MAX_REMEMBERED_PLAYERS) {
		memmove(&coop_absent_list[0], &coop_absent_list[1],
		        sizeof(coop_player_record) * (COOP_MAX_REMEMBERED_PLAYERS - 1));
		memmove(&coop_absent_source_levels[0], &coop_absent_source_levels[1],
		        sizeof(int16_t) * (COOP_MAX_REMEMBERED_PLAYERS - 1));
		coop_num_absent = COOP_MAX_REMEMBERED_PLAYERS - 1;
	}
	memcpy(&coop_absent_list[coop_num_absent], &rec, sizeof(rec));
	coop_absent_source_levels[coop_num_absent] = (int16_t) Current_level_num;
	coop_num_absent++;
	COOP_SAVE_LOG(CON_NORMAL, "coop_save: tracked absent player '%s' (%d total absent)\n",
	              rec.callsign, coop_num_absent);
}

void coop_clear_absent_players(void)
{
	coop_num_absent = 0;
	memset(coop_absent_list, 0, sizeof(coop_absent_list));
	memset(coop_absent_source_levels, 0, sizeof(coop_absent_source_levels));
}

int coop_get_num_absent_players(void)
{
	return coop_num_absent;
}

const coop_player_record *coop_get_absent_players(void)
{
	return coop_absent_list;
}

const coop_player_record *coop_find_absent_player(const char *callsign,
                                                  const char *client_id)
{
	return coop_find_absent_player_with_level(callsign, client_id, NULL);
}

const coop_player_record *coop_find_absent_player_with_level(const char *callsign,
                                                             const char *client_id,
                                                             int *source_level)
{
	int i;

	if (client_id && client_id[0]) {
		for (i = 0; i < coop_num_absent; i++) {
			if (strncmp(coop_absent_list[i].client_id, client_id, COOP_CLIENT_ID_LEN) == 0) {
				if (source_level)
					*source_level = coop_absent_source_levels[i];
				return &coop_absent_list[i];
			}
		}
	}

	if (callsign && callsign[0]) {
		for (i = 0; i < coop_num_absent; i++) {
			if (strncasecmp(coop_absent_list[i].callsign, callsign, COOP_CALLSIGN_LEN) == 0) {
				if (source_level)
					*source_level = coop_absent_source_levels[i];
				return &coop_absent_list[i];
			}
		}
	}

	return NULL;
}

void coop_load_absent_from_metadata(const coop_save_metadata *meta)
{
	int i, n;

	coop_clear_absent_players();
	n = meta->num_absent_players;
	if (n > COOP_MAX_REMEMBERED_PLAYERS)
		n = COOP_MAX_REMEMBERED_PLAYERS;
	for (i = 0; i < n; i++) {
		memcpy(&coop_absent_list[i], &meta->absent_players[i], sizeof(coop_player_record));
		coop_absent_source_levels[i] = meta->level_num;
	}
	coop_num_absent = n;
	COOP_SAVE_LOG(CON_NORMAL, "coop_save: loaded %d absent players from save metadata\n", n);
}

#define COOP_RESTORE_FLAGS_KEYS ( \
	PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_RED_KEY | PLAYER_FLAGS_GOLD_KEY)

void coop_apply_record_to_player(int pnum, const coop_player_record *rec,
                                 int same_level)
{
	int i;
	player *p = &Players[pnum];

	p->energy = rec->energy;
	p->shields = rec->shields;
	p->score = rec->score;
	p->laser_level = rec->laser_level;
	p->primary_weapon_flags = rec->primary_weapon_flags;
	p->secondary_weapon_flags = rec->secondary_weapon_flags;
	for (i = 0; i < MAX_PRIMARY_WEAPONS && i < COOP_SAVE_MAX_WEAPONS; i++)
		p->primary_ammo[i] = rec->primary_ammo[i];
	for (i = 0; i < MAX_SECONDARY_WEAPONS && i < COOP_SAVE_MAX_WEAPONS; i++)
		p->secondary_ammo[i] = rec->secondary_ammo[i];

	p->flags |= (rec->flags & coop_restore_flags_durable());
	if (same_level)
		p->flags |= (rec->flags & COOP_RESTORE_FLAGS_KEYS);

	p->net_kills_total = rec->net_kills_total;
	p->net_killed_total = rec->net_killed_total;
	p->num_kills_total = rec->num_kills_total;
	p->hostages_rescued_total = rec->hostages_rescued_total;
	p->time_total = rec->time_total;
	p->hours_total = rec->hours_total;

	if (pnum == Player_num)
		Objects[p->objnum].shields = p->shields;

	COOP_SAVE_LOG(CON_NORMAL, "coop_save: applied record to P%d '%s' (shields=%d energy=%d laser=%d score=%d)\n",
	              pnum, p->callsign, f2i(p->shields), f2i(p->energy),
	              p->laser_level, p->score);
}

static int coop_autosave_next_slot = 0;
static uint32_t coop_autosave_game_id_sequence = 0;

static uint32_t coop_make_autosave_game_id(void)
{
	uint32_t id = ((uint32_t) time(NULL) << 8) ^
	              (++coop_autosave_game_id_sequence) ^
	              ((uint32_t) N_players << 16) ^
	              ((uint32_t) (Current_level_num & 0xff) << 24);
	if (!id || id == COOP_AUTOSAVE_GAME_ID)
		id ^= 0x13579bdfu;
	return id;
}

int coop_autosave(void)
{
	char filename[PATH_MAX];
	char desc[20];
	int slot;
	uint32_t autosave_game_id;

	if (!(Game_mode & GM_MULTI_COOP))
		return 0;
	if (Endlevel_sequence || Control_center_destroyed)
		return 0;

	slot = COOP_AUTOSAVE_SLOT_FIRST +
	       (coop_autosave_next_slot % COOP_AUTOSAVE_SLOT_COUNT);
	coop_autosave_next_slot++;

	state_android_build_coop_autosave_filename(filename, PATH_MAX, slot);
	memset(desc, 0, sizeof(desc));
	snprintf(desc, sizeof(desc), "Auto L%d %dp %dpts",
	         Current_level_num, N_players, Players[Player_num].score);
	autosave_game_id = coop_make_autosave_game_id();

	COOP_SAVE_LOG(CON_NORMAL, "coop_save: auto-saving to slot %d: %s\n",
	              slot, desc);

	if (multi_i_am_master())
		multi_send_save_game(slot, autosave_game_id, desc);

	{
		uint saved_game_id = state_game_id;
		char saved_callsign[CALLSIGN_LEN + 1];
		memcpy(saved_callsign, Players[Player_num].callsign, CALLSIGN_LEN + 1);

		state_game_id = autosave_game_id;
		strncpy(Players[Player_num].callsign, COOP_AUTOSAVE_CALLSIGN, CALLSIGN_LEN + 1);

		stop_time();
		state_save_all_sub(filename, desc);

		state_game_id = saved_game_id;
		memcpy(Players[Player_num].callsign, saved_callsign, CALLSIGN_LEN + 1);
	}

	coop_write_autosave_history(slot, N_players);

	return 1;
}

static void coop_write_autosave_history(int slot, int n_connected)
{
	char buf[2048];
	char scoped_history[PATH_MAX];
	char scoped_info[PATH_MAX];
	int off = 0;
	int i;
	unsigned now = (unsigned) time(NULL);
	char callsigns_json[512];
	char client_ids_json[512];
	int cs_off = 0, ci_off = 0;
	int n = 0;

	for (i = 0; i < N_players; i++) {
		if (Players[i].connected != CONNECT_PLAYING)
			continue;
		if (n > 0) {
			cs_off += snprintf(callsigns_json + cs_off,
			                   sizeof(callsigns_json) - cs_off, ", ");
			ci_off += snprintf(client_ids_json + ci_off,
			                   sizeof(client_ids_json) - ci_off, ", ");
		}
		cs_off += snprintf(callsigns_json + cs_off,
		                   sizeof(callsigns_json) - cs_off,
		                   "\"%s\"", Players[i].callsign);
		ci_off += snprintf(client_ids_json + ci_off,
		                   sizeof(client_ids_json) - ci_off,
		                   "\"%.36s\"", Netgame.players[i].client_id);
		n++;
	}

	{
		int total_score = 0;
		for (i = 0; i < N_players; i++)
			if (Players[i].connected == CONNECT_PLAYING)
				total_score += Players[i].score;

		off += snprintf(buf + off, sizeof(buf) - off,
		                "[\n  {\n"
		                "    \"slot\": %d,\n"
		                "    \"type\": \"full_save\",\n"
		                "    \"mission\": \"%s\",\n"
		                "    \"level\": %d,\n"
		                "    \"timestamp\": %u,\n"
		                "    \"level_time_seconds\": %d,\n"
		                "    \"num_players\": %d,\n"
		                "    \"difficulty_changed\": %d,\n"
		                "    \"difficulty_min\": %d,\n"
		                "    \"difficulty_max\": %d,\n"
		                "    \"total_score\": %d,\n"
		                "    \"callsigns\": [%s],\n"
		                "    \"client_ids\": [%s]\n"
		                "  }\n",
		                slot, Current_mission_filename, Current_level_num,
		                now, f2i(ThisLevelTime), n,
		                Difficulty_level_changed ? 1 : 0,
		                Difficulty_level_min_seen, Difficulty_level_max_seen,
		                total_score, callsigns_json, client_ids_json);
	}

	{
		PHYSFS_file *old_fp;
		char old_buf[2048];
		PHYSFS_sint64 old_len;

		if (!state_android_build_coop_sidecar_filename(
		        scoped_history, sizeof(scoped_history),
		        "coop_autosave_history.json"))
			scoped_history[0] = '\0';
		old_fp = scoped_history[0] ? PHYSFS_openRead(scoped_history) : NULL;
		if (old_fp) {
			old_len = PHYSFS_fileLength(old_fp);
			if (old_len > 0 && old_len < (PHYSFS_sint64) sizeof(old_buf) - 1) {
				PHYSFS_read(old_fp, old_buf, old_len, 1);
				old_buf[old_len] = '\0';
				coop_append_other_slots(buf, &off, sizeof(buf), old_buf, slot);
			}
			PHYSFS_close(old_fp);
		}
	}

	off += snprintf(buf + off, sizeof(buf) - off, "]\n");

	if (scoped_history[0])
		coop_write_text_file(scoped_history, buf);
	coop_write_text_file("coop_autosave_history.json", buf);

	{
		char jbuf[256];

		snprintf(jbuf, sizeof(jbuf),
		         "{\n"
		         "  \"mission\": \"%s\",\n"
		         "  \"level\": %d,\n"
		         "  \"timestamp\": %u,\n"
		         "  \"num_players\": %d\n"
		         "}\n",
		         Current_mission_filename,
		         Current_level_num, now, n_connected);

		if (state_android_build_coop_sidecar_filename(
		        scoped_info, sizeof(scoped_info), "coop_autosave_info.json"))
			coop_write_text_file(scoped_info, jbuf);
		coop_write_text_file("coop_autosave_info.json", jbuf);
	}
}

static void coop_write_text_file(const char *filename, const char *buf)
{
	PHYSFS_file *fp;

	if (!filename || !filename[0] || !buf)
		return;
	state_android_ensure_parent_dirs_for_path(filename);
	fp = PHYSFS_openWrite(filename);
	if (!fp)
		return;
	PHYSFS_write(fp, buf, strlen(buf), 1);
	PHYSFS_close(fp);
}

static void coop_append_other_slots(char *buf, int *off, int buf_size,
                                    const char *old_json, int exclude_slot)
{
	const char *p = old_json;
	int count = 0;

	while (*p && count < COOP_AUTOSAVE_SLOT_COUNT - 1) {
		const char *entry_start = strstr(p, "{");
		const char *slot_key;
		const char *num_start;
		int entry_slot;
		const char *entry_end;
		int entry_len;

		if (!entry_start)
			break;

		slot_key = strstr(entry_start, "\"slot\":");
		if (!slot_key || slot_key > entry_start + 20)
			break;

		num_start = slot_key + 7;
		while (*num_start == ' ')
			num_start++;
		entry_slot = atoi(num_start);

		entry_end = strstr(entry_start, "}");
		if (!entry_end)
			break;
		entry_end++;

		if (entry_slot != exclude_slot) {
			entry_len = (int) (entry_end - entry_start);
			if (*off + entry_len + 10 < buf_size) {
				*off += snprintf(buf + *off, buf_size - *off, ",\n  ");
				memcpy(buf + *off, entry_start, entry_len);
				*off += entry_len;
				buf[*off] = '\0';
				count++;
			}
		}

		p = entry_end;
	}
}

#define COOP_PROGRESS_INV_TAG 0x43505249
#define COOP_PROGRESS_INV_VER 1
#define COOP_PROGRESS_INV_HDR 18

static void coop_write_progress_inventory(void)
{
	char scoped_inventory[PATH_MAX];

	if (state_android_build_coop_sidecar_filename(
	        scoped_inventory, sizeof(scoped_inventory),
	        "coop_progress_inventory.bin"))
		coop_write_progress_inventory_file(scoped_inventory);
	coop_write_progress_inventory_file("coop_progress_inventory.bin");
}

static void coop_write_progress_inventory_file(const char *filename)
{
	PHYSFS_file *fp;
	coop_player_record rec;
	uint32_t tag = COOP_PROGRESS_INV_TAG;
	uint16_t ver = COOP_PROGRESS_INV_VER;
	int16_t level = (int16_t) Current_level_num;
	uint8_t num = 0;
	char mission[9];
	int i;

	state_android_ensure_parent_dirs_for_path(filename);
	fp = PHYSFS_openWrite(filename);
	if (!fp) {
		con_printf(CON_URGENT, "coop_save: failed to write progress inventory\n");
		return;
	}

	memset(mission, 0, sizeof(mission));
	strncpy(mission, Current_mission_filename, 8);

	for (i = 0; i < N_players; i++)
		if (Players[i].connected == CONNECT_PLAYING)
			num++;

	PHYSFS_write(fp, &tag, 4, 1);
	PHYSFS_write(fp, &ver, 2, 1);
	PHYSFS_write(fp, mission, 9, 1);
	PHYSFS_write(fp, &level, 2, 1);
	PHYSFS_write(fp, &num, 1, 1);

	for (i = 0; i < N_players; i++) {
		if (Players[i].connected != CONNECT_PLAYING)
			continue;
		coop_snapshot_player(i, &rec);
		PHYSFS_write(fp, &rec, sizeof(rec), 1);
	}

	PHYSFS_close(fp);
	COOP_SAVE_LOG(CON_NORMAL, "coop_save: wrote progress inventory (L%d, %d players)\n",
	              Current_level_num, num);
}

static int coop_progress_restore_attempted_level = 0;

int coop_load_progress_inventory(void)
{
	PHYSFS_file *fp;
	char scoped_inventory[PATH_MAX];
	uint32_t tag;
	uint16_t ver;
	char mission[9];
	int16_t level;
	uint8_t num;
	coop_player_record rec;
	int i;
	const char *host_callsign;
	const char *host_client_id;
	int host_restored = 0;

	if (coop_progress_restore_attempted_level == Current_level_num)
		return 0;
	coop_progress_restore_attempted_level = Current_level_num;

	if (!(Game_mode & GM_MULTI_COOP))
		return 0;
	if (!multi_i_am_master())
		return 0;

	fp = NULL;
	if (state_android_build_coop_sidecar_filename(
	        scoped_inventory, sizeof(scoped_inventory),
	        "coop_progress_inventory.bin"))
		fp = PHYSFS_openRead(scoped_inventory);
	if (!fp)
		fp = PHYSFS_openRead("coop_progress_inventory.bin");
	if (!fp)
		return 0;

	if (PHYSFS_read(fp, &tag, 4, 1) != 1 || tag != COOP_PROGRESS_INV_TAG) {
		PHYSFS_close(fp);
		return 0;
	}
	if (PHYSFS_read(fp, &ver, 2, 1) != 1 || ver != COOP_PROGRESS_INV_VER) {
		PHYSFS_close(fp);
		return 0;
	}
	if (PHYSFS_read(fp, mission, 9, 1) != 1) {
		PHYSFS_close(fp);
		return 0;
	}
	if (PHYSFS_read(fp, &level, 2, 1) != 1) {
		PHYSFS_close(fp);
		return 0;
	}
	if (PHYSFS_read(fp, &num, 1, 1) != 1 || num == 0 || num > MAX_PLAYERS) {
		PHYSFS_close(fp);
		return 0;
	}

	if (strncasecmp(mission, Current_mission_filename, 8) != 0) {
		COOP_SAVE_LOG(CON_NORMAL, "coop_save: progress inventory mission mismatch ('%s' vs '%s')\n",
		              mission, Current_mission_filename);
		PHYSFS_close(fp);
		return 0;
	}
	if (level != Current_level_num - 1) {
		COOP_SAVE_LOG(CON_NORMAL, "coop_save: progress inventory level mismatch (L%d vs current L%d)\n",
		              level, Current_level_num);
		PHYSFS_close(fp);
		return 0;
	}

	host_callsign = Players[Player_num].callsign;
	host_client_id = Netgame.players[Player_num].client_id;

	for (i = 0; i < num; i++) {
		if (PHYSFS_read(fp, &rec, sizeof(rec), 1) != 1)
			break;

		if (!host_restored &&
		    ((host_client_id[0] && strncmp(rec.client_id, host_client_id, COOP_CLIENT_ID_LEN) == 0) ||
		     strncasecmp(rec.callsign, host_callsign, COOP_CALLSIGN_LEN) == 0)) {
			coop_apply_record_to_player(Player_num, &rec, 0);
			host_restored = 1;
			continue;
		}

		if (coop_num_absent < COOP_MAX_REMEMBERED_PLAYERS) {
			rec.was_connected = 0;
			memcpy(&coop_absent_list[coop_num_absent], &rec, sizeof(rec));
			coop_absent_source_levels[coop_num_absent] = level;
			coop_num_absent++;
		}
	}

	PHYSFS_close(fp);
	COOP_SAVE_LOG(CON_NORMAL, "coop_save: loaded progress inventory (L%d, %d records, host_restored=%d, %d absent)\n",
	              level, (int) num, host_restored, coop_num_absent);
	return 1;
}

void coop_write_progress_json(void)
{
	char buf[1024];
	char scoped_progress[PATH_MAX];
	char players[256];
	char client_ids[512];
	int i, n, p_off, c_off;

	if (!(Game_mode & GM_MULTI_COOP))
		return;

	p_off = 0;
	c_off = 0;
	n = 0;
	for (i = 0; i < N_players; i++) {
		if (Players[i].connected != CONNECT_PLAYING)
			continue;
		if (n > 0) {
			p_off += snprintf(players + p_off, sizeof(players) - p_off, ", ");
			c_off += snprintf(client_ids + c_off, sizeof(client_ids) - c_off, ", ");
		}
		p_off += snprintf(players + p_off, sizeof(players) - p_off,
		                  "\"%s\"", Players[i].callsign);
		c_off += snprintf(client_ids + c_off, sizeof(client_ids) - c_off,
		                  "\"%.36s\"", Netgame.players[i].client_id);
		n++;
	}

	snprintf(buf, sizeof(buf),
	         "{\n"
	         "  \"mission\": \"%s\",\n"
	         "  \"last_completed_level\": %d,\n"
	         "  \"timestamp\": %u,\n"
	         "  \"difficulty\": %d,\n"
	         "  \"difficulty_changed\": %d,\n"
	         "  \"difficulty_min\": %d,\n"
	         "  \"difficulty_max\": %d,\n"
	         "  \"num_players\": %d,\n"
	         "  \"players\": [%s],\n"
	         "  \"client_ids\": [%s]\n"
	         "}\n",
	         Current_mission_filename,
	         Current_level_num,
	         (unsigned) time(NULL),
	         Difficulty_level,
	         Difficulty_level_changed ? 1 : 0,
	         Difficulty_level_min_seen,
	         Difficulty_level_max_seen,
	         n,
	         players,
	         client_ids);

	if (state_android_build_coop_sidecar_filename(
	        scoped_progress, sizeof(scoped_progress), "coop_progress.json"))
		coop_write_text_file(scoped_progress, buf);
	coop_write_text_file("coop_progress.json", buf);

	COOP_SAVE_LOG(CON_NORMAL, "coop_save: wrote coop_progress.json (L%d, %d players)\n",
	              Current_level_num, n);

	coop_write_progress_inventory();
}

static int coop_auto_restore_armed = 0;
static uint coop_auto_restore_game_id = 0;
static int coop_auto_restore_slot = COOP_AUTOSAVE_SLOT;
static int coop_auto_restore_frames_waited = 0;
static int coop_auto_restore_attempted = 0;

static int coop_read_restore_slot_file(void)
{
	PHYSFS_file *fp;
	char buf[16];
	PHYSFS_sint64 len;
	int slot;
	char *end;
	long parsed;

	fp = PHYSFS_openRead("coop_restore_slot.txt");
	if (!fp)
		return -1;

	len = PHYSFS_fileLength(fp);
	if (len <= 0 || len >= (PHYSFS_sint64) sizeof(buf)) {
		PHYSFS_close(fp);
		PHYSFS_delete("coop_restore_slot.txt");
		return -1;
	}

	PHYSFS_read(fp, buf, len, 1);
	PHYSFS_close(fp);
	PHYSFS_delete("coop_restore_slot.txt");

	buf[len] = '\0';
	parsed = strtol(buf, &end, 10);
	if (end == buf || *end != '\0')
		return -1;
	slot = (int) parsed;
	if (slot < 0 || slot > 9)
		return -1;
	return slot;
}

void coop_arm_auto_restore(void)
{
	char filename[PATH_MAX];
	uint gid;
	int slot;

	if (coop_auto_restore_attempted)
		return;
	coop_auto_restore_attempted = 1;

	coop_auto_restore_armed = 0;
	coop_auto_restore_game_id = 0;
	coop_auto_restore_slot = COOP_AUTOSAVE_SLOT;
	coop_auto_restore_frames_waited = 0;

	if (!(Game_mode & GM_MULTI_COOP)) {
		coop_auto_restore_trace("arm_auto_restore skipped, not coop (mode=0x%x)", Game_mode);
		return;
	}
	if (!multi_i_am_master()) {
		coop_auto_restore_trace("arm_auto_restore skipped, not master");
		return;
	}

	slot = coop_read_restore_slot_file();
	if (slot < 0) {
		coop_auto_restore_log_no_slot();
		return;
	}

	state_android_build_save_filename(filename, PATH_MAX, slot, 1, 0);
	if (slot >= COOP_AUTOSAVE_SLOT_FIRST && slot < COOP_AUTOSAVE_SLOT_FIRST + COOP_AUTOSAVE_SLOT_COUNT) {
		state_android_build_coop_autosave_filename(filename, PATH_MAX, slot);
		gid = state_get_game_id(filename);
		coop_auto_restore_trace("try autosave file '%s' game_id=%u", filename, gid);
	} else
		gid = 0;
	if (!gid) {
		state_android_build_save_filename(filename, PATH_MAX, slot, 1, 0);
		gid = state_get_game_id(filename);
		coop_auto_restore_trace("try callsign file '%s' game_id=%u", filename, gid);
	}
	if (!gid) {
		coop_auto_restore_log_slot_not_viable(slot);
		return;
	}

	coop_auto_restore_slot = slot;
	coop_auto_restore_game_id = gid;
	coop_auto_restore_armed = 1;
	coop_auto_restore_log_armed(slot, gid);
}

void coop_try_auto_restore(void)
{
	const int timeout_before_alive_check = coop_auto_restore_timeout_before_alive_check();
	const int timeout_frames = coop_auto_restore_timeout_frames();

	if (!coop_auto_restore_armed)
		return;

	coop_auto_restore_frames_waited++;
	if (coop_auto_restore_frames_waited < 30)
		return;

	if (!multi_i_am_master()) {
		coop_auto_restore_trace("auto-restore disarm: not master");
		goto disarm;
	}
	if (!(Game_mode & GM_MULTI_COOP)) {
		coop_auto_restore_trace("auto-restore disarm: not coop (game_mode=0x%x)", Game_mode);
		goto disarm;
	}
	if (Endlevel_sequence || Control_center_destroyed) {
		coop_auto_restore_trace("auto-restore disarm: endlevel=%d CC=%d",
		                        Endlevel_sequence, Control_center_destroyed);
		goto disarm;
	}
	if (timeout_before_alive_check && coop_auto_restore_frames_waited > timeout_frames) {
		coop_auto_restore_trace("auto-restore disarm: timeout at %d frames",
		                        coop_auto_restore_frames_waited);
		coop_log_player_status();
		goto disarm;
	}
	if (!multi_all_players_alive()) {
		if (timeout_before_alive_check &&
		    (coop_auto_restore_frames_waited == 30 ||
		     coop_auto_restore_frames_waited % 60 == 0)) {
			coop_auto_restore_trace("auto-restore waiting: not all alive (frame %d)",
			                        coop_auto_restore_frames_waited);
			coop_log_player_status();
		}
		return;
	}
	if (!timeout_before_alive_check && coop_auto_restore_frames_waited > timeout_frames)
		goto disarm;

	coop_auto_restore_log_trigger(coop_auto_restore_slot,
	                              coop_auto_restore_game_id, coop_auto_restore_frames_waited);

	coop_auto_restore_armed = 0;

	multi_send_restore_game(coop_auto_restore_slot, coop_auto_restore_game_id);
	if (!multi_coop_restore_transfer_pending())
		multi_restore_game(coop_auto_restore_slot, coop_auto_restore_game_id);
	return;

disarm:
	coop_auto_restore_armed = 0;
}

void coop_disarm_auto_restore(void)
{
	coop_auto_restore_armed = 0;
	coop_auto_restore_attempted = 0;
	coop_progress_restore_attempted_level = 0;
}
