#ifdef __ANDROID__

#include "coop_level_restart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "android_log.h"
#include "android_save_meta.h"
#include "android_save_set.h"
#include "game.h"
#include "mission.h"
#include "multi.h"
#include "physfsx.h"
#include "player.h"
#include "state_android_shared.h"

enum { COOP_LEVEL_RESTART_MISSION_LEN = 32 };

typedef struct coop_level_restart_session {
	int pending_capture;
	int valid;
	int busy;
	int restore_suppressed;
	int level_num;
	char mission[COOP_LEVEL_RESTART_MISSION_LEN];
	rewind_memory_buffer buffer;
} coop_level_restart_session;

static coop_level_restart_session g_level_restart;

static unsigned int coop_level_restart_checksum(const unsigned char *data,
                                                size_t size)
{
	unsigned int hash = 2166136261u;
	size_t i;

	for (i = 0; i < size; ++i) {
		hash ^= data[i];
		hash *= 16777619u;
	}
	return hash ? hash : 1;
}

static int coop_level_restart_identity_matches(void)
{
	return g_level_restart.valid &&
	       g_level_restart.level_num == Current_level_num &&
	       !strcmp(g_level_restart.mission, Current_mission_filename);
}

static int coop_level_restart_has_duplicate_callsigns(void)
{
	int i;
	int j;

	for (i = 0; i < N_players; ++i) {
		if (Players[i].connected != CONNECT_PLAYING)
			continue;
		for (j = i + 1; j < N_players; ++j)
			if (Players[j].connected == CONNECT_PLAYING &&
			    !strcmp(Players[i].callsign, Players[j].callsign))
				return 1;
	}
	return 0;
}

static int coop_level_restart_build_manifest_filename(char *filename,
                                                      size_t filename_size)
{
	char mission[32];

	android_save_set_sanitize_component(mission, sizeof(mission),
	                                    Current_mission_filename, "default");
	return snprintf(filename, filename_size,
	                "coop_level_start_%s.json", mission) < (int) filename_size;
}

static int coop_level_restart_read_retained_level(const char *manifest)
{
	PHYSFS_file *fp;
	char data[1024];
	PHYSFS_sint64 size;
	char *level;

	fp = PHYSFS_openRead(manifest);
	if (!fp)
		return 0;
	size = PHYSFS_fileLength(fp);
	if (size <= 0 || size >= (PHYSFS_sint64) sizeof(data)) {
		PHYSFS_close(fp);
		return 0;
	}
	if (PHYSFS_read(fp, data, 1, (PHYSFS_uint32) size) != size) {
		PHYSFS_close(fp);
		return 0;
	}
	PHYSFS_close(fp);
	data[size] = '\0';
	level = strstr(data, "\"level\":");
	return level ? atoi(level + 8) : 0;
}

static int coop_level_restart_write_buffer(const char *filename,
                                           const rewind_memory_buffer *buffer)
{
	PHYSFS_file *fp;

	state_android_ensure_parent_dirs_for_path(filename);
	fp = PHYSFS_openWrite(filename);
	if (!fp)
		return 0;
	if (PHYSFS_write(fp, buffer->data, 1, (PHYSFS_uint32) buffer->size) !=
	    (PHYSFS_sint64) buffer->size) {
		PHYSFS_close(fp);
		return 0;
	}
	return PHYSFS_close(fp);
}

static int coop_level_restart_write_manifest(const char *filename,
                                             const char *save_path,
                                             unsigned int checksum)
{
	PHYSFS_file *fp;
	char json[2048];
	char callsigns[256];
	char client_ids[512];
	int callsign_offset = 0;
	int client_id_offset = 0;
	int count = 0;
	int i;
	int length;

	for (i = 0; i < N_players; ++i) {
		if (Players[i].connected != CONNECT_PLAYING)
			continue;
		if (count) {
			callsign_offset += snprintf(callsigns + callsign_offset,
			                            sizeof(callsigns) - callsign_offset, ", ");
			client_id_offset += snprintf(client_ids + client_id_offset,
			                             sizeof(client_ids) - client_id_offset, ", ");
		}
		callsign_offset += snprintf(callsigns + callsign_offset,
		                            sizeof(callsigns) - callsign_offset,
		                            "\"%.8s\"", Players[i].callsign);
		client_id_offset += snprintf(client_ids + client_id_offset,
		                             sizeof(client_ids) - client_id_offset,
		                             "\"%.36s\"", Netgame.players[i].client_id);
		count++;
	}
	length = snprintf(json, sizeof(json),
	                  "{\n"
	                  "  \"version\": 1,\n"
	                  "  \"checkpoint_id\": \"%.31s\",\n"
	                  "  \"type\": \"level_start_highest\",\n"
	                  "  \"mission\": \"%.31s\",\n"
	                  "  \"level\": %d,\n"
	                  "  \"level_name\": \"%.63s\",\n"
	                  "  \"timestamp\": %u,\n"
	                  "  \"num_players\": %d,\n"
	                  "  \"total_score\": %d,\n"
	                  "  \"callsigns\": [%s],\n"
	                  "  \"client_ids\": [%s],\n"
	                  "  \"save_path\": \"%s\",\n"
	                  "  \"size\": %u,\n"
	                  "  \"checksum\": %u\n"
	                  "}\n",
	                  Current_mission_filename, Current_mission_filename,
	                  Current_level_num, Current_level_name, (unsigned int) time(NULL),
	                  count, Players[Player_num].score, callsigns, client_ids, save_path,
	                  (unsigned int) g_level_restart.buffer.size, checksum);
	if (length <= 0 || length >= (int) sizeof(json))
		return 0;
	fp = PHYSFS_openWrite(filename);
	if (!fp)
		return 0;
	if (PHYSFS_write(fp, json, 1, (PHYSFS_uint32) length) != length) {
		PHYSFS_close(fp);
		return 0;
	}
	return PHYSFS_close(fp);
}

static void coop_level_restart_persist_if_highest(void)
{
	char save_path[PATH_MAX];
	char save_temp[PATH_MAX];
	char manifest[PATH_MAX];
	char manifest_temp[PATH_MAX];
	unsigned int checksum;
	int retained_level;

	if (Current_level_num <= 0 || !g_level_restart.valid ||
	    !state_android_build_coop_sidecar_filename(
	        save_path, sizeof(save_path), "level_start_highest.sav") ||
	    !coop_level_restart_build_manifest_filename(manifest, sizeof(manifest)))
		return;
	retained_level = coop_level_restart_read_retained_level(manifest);
	if (retained_level >= Current_level_num && PHYSFSX_exists(save_path, 0))
		return;
	snprintf(save_temp, sizeof(save_temp), "%s.tmp", save_path);
	snprintf(manifest_temp, sizeof(manifest_temp), "%s.tmp", manifest);
	checksum = coop_level_restart_checksum(g_level_restart.buffer.data,
	                                       g_level_restart.buffer.size);
	if (!coop_level_restart_write_buffer(save_temp, &g_level_restart.buffer) ||
	    !coop_level_restart_write_manifest(manifest_temp, save_path, checksum) ||
	    !PHYSFSX_rename(save_temp, save_path) ||
	    !PHYSFSX_rename(manifest_temp, manifest)) {
		PHYSFS_delete(save_temp);
		PHYSFS_delete(manifest_temp);
		COOPLOG("level-start retained checkpoint write failed: mission='%s' level=%d",
		        Current_mission_filename, Current_level_num);
		return;
	}
	COOPLOG("level-start retained checkpoint saved: mission='%s' level=%d bytes=%u checksum=%u",
	        Current_mission_filename, Current_level_num,
	        (unsigned int) g_level_restart.buffer.size, checksum);
}

void coop_level_restart_note_natural_level(int level_num)
{
	if (!(Game_mode & GM_MULTI_COOP))
		return;
	if (g_level_restart.valid && level_num == g_level_restart.level_num &&
	    !strcmp(g_level_restart.mission, Current_mission_filename))
		return;
	g_level_restart.pending_capture = 1;
	g_level_restart.restore_suppressed = 0;
}

void coop_level_restart_maybe_capture_ready(void)
{
	rewind_memory_buffer next = { NULL, 0, 0 };
	unsigned char *old_data;

	if (!g_level_restart.pending_capture || g_level_restart.restore_suppressed ||
	    !(Game_mode & GM_MULTI_COOP) || !multi_i_am_master() || is_observer() ||
	    Current_level_num == 0 || !multi_all_players_alive() ||
	    coop_level_restart_has_duplicate_callsigns() || multi_save_transfer_busy())
		return;
	if (!state_save_to_memory(&next, "LEVEL START",
	                          ANDROID_SAVE_META_KIND_MANUAL, 1)) {
		free(next.data);
		return;
	}
	old_data = g_level_restart.buffer.data;
	g_level_restart.buffer = next;
	g_level_restart.level_num = Current_level_num;
	snprintf(g_level_restart.mission, sizeof(g_level_restart.mission), "%s",
	         Current_mission_filename);
	g_level_restart.valid = 1;
	g_level_restart.pending_capture = 0;
	free(old_data);
	coop_level_restart_persist_if_highest();
	COOPLOG("level-start checkpoint captured: mission='%s' level=%d bytes=%u",
	        g_level_restart.mission, g_level_restart.level_num,
	        (unsigned int) g_level_restart.buffer.size);
}

void coop_level_restart_note_restore_begin(void)
{
	g_level_restart.restore_suppressed = 1;
	g_level_restart.pending_capture = 0;
}

void coop_level_restart_note_restore_end(int restored)
{
	if (!restored)
		return;
	if (!coop_level_restart_identity_matches())
		g_level_restart.valid = 0;
}

int coop_level_restart_get_state(void)
{
	if (!(Game_mode & GM_MULTI_COOP) || !multi_i_am_master())
		return COOP_LEVEL_RESTART_HIDDEN;
	if (g_level_restart.busy)
		return COOP_LEVEL_RESTART_BUSY;
	if (coop_level_restart_identity_matches())
		return COOP_LEVEL_RESTART_READY;
	return g_level_restart.pending_capture ? COOP_LEVEL_RESTART_CAPTURING
	                                       : COOP_LEVEL_RESTART_HIDDEN;
}

int coop_level_restart_request(void)
{
	if (coop_level_restart_get_state() != COOP_LEVEL_RESTART_READY ||
	    coop_level_restart_has_duplicate_callsigns())
		return 0;
	g_level_restart.busy = 1;
	if (!multi_send_level_restart_transfer(&g_level_restart.buffer)) {
		g_level_restart.busy = 0;
		return 0;
	}
	return 1;
}

int coop_level_restart_load_retained_and_request(void)
{
	char filename[PATH_MAX];
	PHYSFS_file *fp;
	PHYSFS_sint64 size;
	unsigned char *data;
	unsigned char *old_data;

	if (!(Game_mode & GM_MULTI_COOP) || !multi_i_am_master() ||
	    !state_android_build_coop_sidecar_filename(
	        filename, sizeof(filename), "level_start_highest.sav"))
		return 0;
	fp = PHYSFS_openRead(filename);
	if (!fp)
		return 0;
	size = PHYSFS_fileLength(fp);
	if (size <= 0 || size > 2 * 1024 * 1024) {
		PHYSFS_close(fp);
		return 0;
	}
	data = (unsigned char *) malloc((size_t) size);
	if (!data || PHYSFS_read(fp, data, 1, (PHYSFS_uint32) size) != size) {
		free(data);
		PHYSFS_close(fp);
		return 0;
	}
	PHYSFS_close(fp);
	old_data = g_level_restart.buffer.data;
	g_level_restart.buffer.data = data;
	g_level_restart.buffer.size = (size_t) size;
	g_level_restart.buffer.capacity = (size_t) size;
	g_level_restart.level_num = Current_level_num;
	snprintf(g_level_restart.mission, sizeof(g_level_restart.mission), "%s",
	         Current_mission_filename);
	g_level_restart.valid = 1;
	g_level_restart.pending_capture = 0;
	g_level_restart.restore_suppressed = 0;
	free(old_data);
	return coop_level_restart_request();
}

int coop_level_restart_apply_host(void)
{
	int restored;

	if (!g_level_restart.valid)
		return 0;
	restored = state_restore_coop_from_memory(&g_level_restart.buffer);
	if (restored)
		multi_send_score();
	return restored;
}

const rewind_memory_buffer *coop_level_restart_buffer(void)
{
	return g_level_restart.valid ? &g_level_restart.buffer : NULL;
}

void coop_level_restart_transfer_finished(int restored)
{
	g_level_restart.busy = 0;
	if (!restored)
		COOPLOG("level-start restart transfer failed");
}

void coop_level_restart_clear(void)
{
	free(g_level_restart.buffer.data);
	memset(&g_level_restart, 0, sizeof(g_level_restart));
}

#endif
