/*
 * Coop save-file metadata extension -- implementation.
 * See coop_save.h for design rationale.
 */

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <physfs.h>
#include "coop_save.h"
#include "player.h"
#include "multi.h"
#include "config.h"
#include "console.h"
#include "weapon.h"
#include "state.h"
#include "args.h"
#include "endlevel.h"
#include "cntrlcen.h"
#include "game.h"
#include "gameseq.h"
#include "mission.h"

/* --- absent player tracking --- */

static coop_player_record coop_absent_list[COOP_MAX_REMEMBERED_PLAYERS];
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
	meta.wall_clock_timestamp = (uint32_t)time(NULL);
	meta.level_num = Current_level_num;
	strncpy(meta.mission_name, Netgame.mission_name, 8);
	meta.mission_name[8] = '\0';
	meta.difficulty = Netgame.difficulty;

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

	PHYSFS_write((PHYSFS_file *)fp, &meta, sizeof(meta), 1);
	con_printf(CON_DEBUG, "coop_save: wrote metadata trailer (%d active, %d absent)\n",
		meta.num_active_players, meta.num_absent_players);
}

int coop_read_save_metadata(void *fp, PHYSFS_sint64 expected_end,
                            coop_save_metadata *meta)
{
	uint32_t tag;
	PHYSFS_sint64 cur;

	/* The metadata trailer starts right after the existing save data */
	if (PHYSFS_seek((PHYSFS_file *)fp, expected_end) == 0)
		return 0;

	/* Check if there's enough data for at least the tag */
	cur = PHYSFS_fileLength((PHYSFS_file *)fp);
	if (cur < expected_end + (PHYSFS_sint64)sizeof(uint32_t))
		return 0;

	if (PHYSFS_read((PHYSFS_file *)fp, &tag, sizeof(tag), 1) != 1)
		return 0;
	if (tag != COOP_SAVE_META_TAG)
		return 0;

	/* Read the rest of the struct (skip the tag we already read) */
	memset(meta, 0, sizeof(*meta));
	meta->tag = tag;
	if (PHYSFS_read((PHYSFS_file *)fp, ((char *)meta) + sizeof(uint32_t),
	                sizeof(*meta) - sizeof(uint32_t), 1) != 1) {
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

	con_printf(CON_DEBUG, "coop_save: read metadata trailer (ver=%d, %d active, %d absent)\n",
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

/* --- absent player tracking --- */

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

	/* Snapshot before the player slot is cleared */
	coop_snapshot_player(pnum, &rec);
	rec.was_connected = 0; /* mark as absent */

	/* Check if this player is already in the absent list (by client_id or callsign) */
	for (i = 0; i < coop_num_absent; i++) {
		if ((rec.client_id[0] && strncmp(coop_absent_list[i].client_id, rec.client_id, COOP_CLIENT_ID_LEN) == 0) ||
		    strncasecmp(coop_absent_list[i].callsign, rec.callsign, COOP_CALLSIGN_LEN) == 0) {
			/* Update existing entry */
			memcpy(&coop_absent_list[i], &rec, sizeof(rec));
			con_printf(CON_NORMAL, "coop_save: updated absent player '%s' (slot %d)\n",
				rec.callsign, i);
			return;
		}
	}

	/* Add new entry, evicting oldest if full */
	if (coop_num_absent >= COOP_MAX_REMEMBERED_PLAYERS) {
		memmove(&coop_absent_list[0], &coop_absent_list[1],
			sizeof(coop_player_record) * (COOP_MAX_REMEMBERED_PLAYERS - 1));
		coop_num_absent = COOP_MAX_REMEMBERED_PLAYERS - 1;
	}
	memcpy(&coop_absent_list[coop_num_absent], &rec, sizeof(rec));
	coop_num_absent++;
	con_printf(CON_NORMAL, "coop_save: tracked absent player '%s' (%d total absent)\n",
		rec.callsign, coop_num_absent);
}

void coop_clear_absent_players(void)
{
	coop_num_absent = 0;
	memset(coop_absent_list, 0, sizeof(coop_absent_list));
}

int coop_get_num_absent_players(void)
{
	return coop_num_absent;
}

const coop_player_record *coop_get_absent_players(void)
{
	return coop_absent_list;
}

/* --- auto-save --- */

int coop_autosave(void)
{
	char filename[PATH_MAX];
	char desc[20];
	int n_connected = 0;
	int i;

	if (!(Game_mode & GM_MULTI_COOP))
		return 0;
	if (Endlevel_sequence || Control_center_destroyed)
		return 0;

	for (i = 0; i < N_players; i++)
		if (Players[i].connected == CONNECT_PLAYING)
			n_connected++;

	snprintf(filename, PATH_MAX,
		GameArg.SysUsePlayersDir ? "Players/%s.mg%d" : "%s.mg%d",
		Players[Player_num].callsign, COOP_AUTOSAVE_SLOT);
	snprintf(desc, 20, "Auto L%d %dp %dpts",
		Current_level_num, n_connected, Players[Player_num].score);

	con_printf(CON_NORMAL, "coop_save: auto-saving to slot %d: %s\n",
		COOP_AUTOSAVE_SLOT, desc);

	stop_time();
	state_save_all_sub(filename, desc);
	/* start_time() is called inside state_save_all_sub */

	/* Write a JSON sidecar so the Kotlin lobby can detect this save */
	{
		PHYSFS_file *jfp;
		char jbuf[256];

		snprintf(jbuf, sizeof(jbuf),
			"{\n"
			"  \"mission\": \"%s\",\n"
			"  \"level\": %d,\n"
			"  \"timestamp\": %u,\n"
			"  \"num_players\": %d\n"
			"}\n",
			Current_mission_filename,
			Current_level_num,
			(unsigned)time(NULL),
			n_connected);

		jfp = PHYSFS_openWrite("coop_autosave_info.json");
		if (jfp) {
			PHYSFS_write(jfp, jbuf, strlen(jbuf), 1);
			PHYSFS_close(jfp);
		}
	}

	return 1;
}

/* --- progress tracking --- */

void coop_write_progress_json(void)
{
	PHYSFS_file *fp;
	char buf[512];
	char players[256];
	int i, n, off;

	if (!(Game_mode & GM_MULTI_COOP))
		return;

	/* Build JSON array of connected player callsigns */
	off = 0;
	n = 0;
	for (i = 0; i < N_players; i++) {
		if (Players[i].connected != CONNECT_PLAYING)
			continue;
		if (n > 0)
			off += snprintf(players + off, sizeof(players) - off, ", ");
		off += snprintf(players + off, sizeof(players) - off,
			"\"%s\"", Players[i].callsign);
		n++;
	}

	snprintf(buf, sizeof(buf),
		"{\n"
		"  \"mission\": \"%s\",\n"
		"  \"last_completed_level\": %d,\n"
		"  \"timestamp\": %u,\n"
		"  \"difficulty\": %d,\n"
		"  \"num_players\": %d,\n"
		"  \"players\": [%s]\n"
		"}\n",
		Current_mission_filename,
		Current_level_num,
		(unsigned)time(NULL),
		Difficulty_level,
		n,
		players);

	fp = PHYSFS_openWrite("coop_progress.json");
	if (!fp) {
		con_printf(CON_URGENT, "coop_save: failed to write coop_progress.json\n");
		return;
	}
	PHYSFS_write(fp, buf, strlen(buf), 1);
	PHYSFS_close(fp);

	con_printf(CON_NORMAL, "coop_save: wrote coop_progress.json (L%d, %d players)\n",
		Current_level_num, n);
}

/* --- auto-restore --- */

static int coop_auto_restore_armed = 0;
static uint coop_auto_restore_game_id = 0;
static int coop_auto_restore_frames_waited = 0;
static int coop_auto_restore_attempted = 0;

void coop_arm_auto_restore(void)
{
	char filename[PATH_MAX];
	uint gid;

	coop_auto_restore_armed = 0;
	coop_auto_restore_game_id = 0;
	coop_auto_restore_frames_waited = 0;

	if (coop_auto_restore_attempted)
		return;
	coop_auto_restore_attempted = 1;

	if (!(Game_mode & GM_MULTI_COOP))
		return;
	if (!multi_i_am_master())
		return;

	snprintf(filename, PATH_MAX,
		GameArg.SysUsePlayersDir ? "Players/%s.mg%d" : "%s.mg%d",
		Players[Player_num].callsign, COOP_AUTOSAVE_SLOT);

	gid = state_get_game_id(filename);
	if (!gid) {
		con_printf(CON_NORMAL, "coop_save: no viable auto-save for restore\n");
		return;
	}

	coop_auto_restore_game_id = gid;
	coop_auto_restore_armed = 1;
	con_printf(CON_NORMAL, "coop_save: auto-restore armed (game_id=%u)\n", gid);
}

void coop_try_auto_restore(void)
{
	if (!coop_auto_restore_armed)
		return;

	/* Wait a few frames for all players to fully connect */
	coop_auto_restore_frames_waited++;
	if (coop_auto_restore_frames_waited < 30)
		return;

	if (!multi_i_am_master())
		goto disarm;
	if (!(Game_mode & GM_MULTI_COOP))
		goto disarm;
	if (!multi_all_players_alive())
		return; /* keep waiting, up to ~5 seconds */
	if (coop_auto_restore_frames_waited > 150)
		goto disarm; /* give up after ~5 seconds */
	if (Endlevel_sequence || Control_center_destroyed)
		goto disarm;

	con_printf(CON_NORMAL, "coop_save: triggering auto-restore from slot %d (game_id=%u)\n",
		COOP_AUTOSAVE_SLOT, coop_auto_restore_game_id);

	multi_send_restore_game(COOP_AUTOSAVE_SLOT, coop_auto_restore_game_id);
	multi_restore_game(COOP_AUTOSAVE_SLOT, coop_auto_restore_game_id);

disarm:
	coop_auto_restore_armed = 0;
}

void coop_disarm_auto_restore(void)
{
	coop_auto_restore_armed = 0;
	coop_auto_restore_attempted = 0;
}
