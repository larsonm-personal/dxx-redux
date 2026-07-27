#ifdef __ANDROID__

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "android_log.h"
#include "android_rewind.h"
#include "android_rewind_policy.h"
#include "byteswap.h"
#include "coop_save.h"
#include "coop/coop_level_restart.h"
#include "fix.h"
#include "game.h"
#include "hudmsg.h"
#include "multi.h"
#include "multi_save_transfer_policy.h"
#include "physfsx.h"
#include "player.h"
#include "pstypes.h"
#include "state_android_shared.h"
#include "timer.h"
#include "u_mem.h"

typedef struct multi_rewind_save_transfer {
	int active;
	int transfer_kind;
	ubyte transfer_id;
	int requester;
	uint total_size;
	uint checksum;
	int64_t game_time64;
	int has_collision_delay_last_play_time;
	int64_t collision_delay_last_play_time;
	int rewound_seconds;
	int total_chunks;
	int chunks_received;
	int apply_pending;
	fix64 started_at;
	unsigned char *data;
	unsigned char *chunk_received;
} multi_rewind_save_transfer;

typedef struct multi_save_send_transfer {
	int active;
	int transfer_kind;
	ubyte transfer_id;
	int requester;
	uint total_size;
	uint checksum;
	int64_t game_time64;
	int has_collision_delay_last_play_time;
	int64_t collision_delay_last_play_time;
	int rewound_seconds;
	int total_chunks;
	int next_chunk;
	int begin_sent;
	int apply_sent;
	int coop_restore_pending;
	int level_restart_pending;
	ubyte coop_restore_slot;
	uint coop_restore_game_id;
	fix64 started_at;
	unsigned char required_players[MAX_PLAYERS];
	unsigned char ready_players[MAX_PLAYERS];
	unsigned char applying_players[MAX_PLAYERS];
	unsigned char *data;
} multi_save_send_transfer;

#define MULTI_SAVE_TRANSFER_KIND_REWIND        0
#define MULTI_SAVE_TRANSFER_KIND_RESTORE       1
#define MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART 2
#define MULTI_SAVE_TRANSFER_READY_BUFFER       1
#define MULTI_SAVE_TRANSFER_READY_APPLY        2
#define MULTI_SAVE_TRANSFER_MAX_BYTES          (2u * 1024u * 1024u)
#define MULTI_SAVE_TRANSFER_CHUNKS_FRAME       8
#define MULTI_SAVE_TRANSFER_TIMEOUT            (F1_0 * 60)

static multi_rewind_save_transfer Rewind_save_transfer;
static multi_save_send_transfer Save_send_transfer;
static ubyte Rewind_save_transfer_id = 0;
static int Save_transfer_restore_active;
static fix64 Save_transfer_timeout_grace_until;
static int Coop_restore_transfer_failed;

static int multi_rewind_requester_valid(int pnum)
{
	return pnum >= 0 && pnum < N_players &&
	       (Players[pnum].connected == CONNECT_PLAYING ||
	        Players[pnum].connected == CONNECT_WAITING);
}

static int multi_rewind_clamp_u8(int value)
{
	if (value < 0)
		return 0;
	if (value > 255)
		return 255;
	return value;
}

static void multi_rewind_put_i64(ubyte *dst, int64_t value)
{
	u_int64_t raw = (u_int64_t) value;
	PUT_INTEL_INT(dst, (uint) (raw & 0xffffffffu));
	PUT_INTEL_INT(dst + 4, (uint) (raw >> 32));
}

static int64_t multi_rewind_get_i64(const ubyte *src)
{
	u_int64_t low = (u_int64_t) GET_INTEL_INT(src);
	u_int64_t high = (u_int64_t) GET_INTEL_INT(src + 4);
	return (int64_t) (low | (high << 32));
}

static uint multi_rewind_checksum(const unsigned char *data, size_t size)
{
	uint hash = 2166136261u;
	size_t i;

	for (i = 0; i < size; i++) {
		hash ^= data[i];
		hash *= 16777619u;
	}
	return hash ? hash : 1;
}

static int multi_rewind_has_connected_clients(void)
{
	int i;

	for (i = 0; i < N_players; i++)
		if (i != Player_num && Players[i].connected == CONNECT_PLAYING)
			return 1;
	return 0;
}

static void multi_rewind_receive_reset(void)
{
	if (Rewind_save_transfer.data)
		d_free(Rewind_save_transfer.data);
	if (Rewind_save_transfer.chunk_received)
		d_free(Rewind_save_transfer.chunk_received);
	memset(&Rewind_save_transfer, 0, sizeof(Rewind_save_transfer));
}

static void multi_save_send_reset(void)
{
	if (Save_send_transfer.data)
		d_free(Save_send_transfer.data);
	memset(&Save_send_transfer, 0, sizeof(Save_send_transfer));
}

static void multi_save_transfer_refresh_peer_times(void)
{
	fix64 now = timer_query();
	int i;

	for (i = 0; i < N_players; ++i)
		if (i != Player_num &&
		    Players[i].connected != CONNECT_DISCONNECTED)
			Netgame.players[i].LastPacketTime = now;
}

static void multi_save_transfer_begin_restore(void)
{
	Save_transfer_restore_active = 1;
	Save_transfer_timeout_grace_until = timer_query() + F1_0 * 60;
}

static void multi_save_transfer_finish_restore(void)
{
	Save_transfer_restore_active = 0;
	Save_transfer_timeout_grace_until = timer_query() + F1_0 * 30;
	multi_save_transfer_refresh_peer_times();
}

void multi_send_coop_restore_status(int status)
{
	if (!(Game_mode & GM_MULTI_COOP) || !multi_i_am_master() || status < 0 || status > 2)
		return;
	multibuf[0] = MULTI_COOP_RESTORE_STATUS;
	multibuf[1] = (ubyte) status;
	multi_send_data(multibuf, 2, 2);
}

void multi_do_coop_restore_status(const ubyte *buf)
{
	if (multi_i_am_master() || !(Game_mode & GM_MULTI_COOP))
		return;
	if (buf[1] == 0)
		coop_restore_status_complete();
	else if (buf[1] == 1)
		coop_restore_status_waiting();
	else if (buf[1] == 2)
		coop_restore_status_failed();
}

int multi_save_transfer_timeout_suspended(void)
{
	return Save_transfer_restore_active ||
	       timer_query() < Save_transfer_timeout_grace_until;
}

static void multi_rewind_send_ready(int phase)
{
	memset(multibuf, 0, MULTI_REWIND_SAVE_READY_LEN);
	multibuf[0] = MULTI_REWIND_SAVE_READY;
	multibuf[1] = Rewind_save_transfer.transfer_id;
	multibuf[2] = (ubyte) Player_num;
	multibuf[3] = (ubyte) phase;
	multi_send_data_direct(multibuf, MULTI_REWIND_SAVE_READY_LEN,
	                       multi_who_is_master(), 2);
}

static void multi_rewind_apply_received_transfer(void)
{
	android_rewind_authoritative_restore restore;
	rewind_memory_buffer buffer;
	uint checksum;
	int status;
	int restored;

	if (!Rewind_save_transfer.active ||
	    !Rewind_save_transfer.apply_pending ||
	    Rewind_save_transfer.chunks_received != Rewind_save_transfer.total_chunks)
		return;
	checksum = multi_rewind_checksum(Rewind_save_transfer.data,
	                                 Rewind_save_transfer.total_size);
	if (checksum != Rewind_save_transfer.checksum) {
		COOPLOG("save transfer checksum mismatch: kind=%d got=%u expected=%u bytes=%u",
		        Rewind_save_transfer.transfer_kind, checksum,
		        Rewind_save_transfer.checksum,
		        Rewind_save_transfer.total_size);
		HUD_init_message_literal(HM_DEFAULT,
		                         Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE
		                             ? "Host save sync failed"
		                             : "Host rewind sync failed");
		if (Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE)
			coop_restore_status_failed();
		multi_rewind_receive_reset();
		return;
	}

	if (Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE ||
	    Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART) {
		buffer.data = Rewind_save_transfer.data;
		buffer.size = Rewind_save_transfer.total_size;
		buffer.capacity = Rewind_save_transfer.total_size;
		multi_save_transfer_begin_restore();
		multi_rewind_send_ready(MULTI_SAVE_TRANSFER_READY_APPLY);
		multi_prepare_restore_sync();
		restored = state_restore_coop_from_memory(&buffer);
		multi_save_transfer_finish_restore();
		if (restored) {
			coop_restore_status_complete();
			HUD_init_message_literal(HM_DEFAULT,
			                         Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART
			                             ? "Level restarted"
			                             : "Host save restored");
			multi_send_score();
		} else {
			coop_restore_status_failed();
			HUD_init_message_literal(HM_DEFAULT,
			                         Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART
			                             ? "Level restart failed"
			                             : "Host save failed");
		}
		COOPLOG("coop restore transfer apply: status=%d bytes=%u chunks=%d",
		        restored, Rewind_save_transfer.total_size,
		        Rewind_save_transfer.total_chunks);
		multi_rewind_receive_reset();
		return;
	}

	memset(&restore, 0, sizeof(restore));
	restore.buffer.data = Rewind_save_transfer.data;
	restore.buffer.size = Rewind_save_transfer.total_size;
	restore.buffer.capacity = Rewind_save_transfer.total_size;
	restore.snapshot_index = -1;
	restore.rewound_seconds = Rewind_save_transfer.rewound_seconds;
	restore.game_time64 = Rewind_save_transfer.game_time64;
	restore.has_collision_delay_last_play_time =
	    Rewind_save_transfer.has_collision_delay_last_play_time;
	restore.collision_delay_last_play_time =
	    Rewind_save_transfer.collision_delay_last_play_time;
	multi_save_transfer_begin_restore();
	multi_rewind_send_ready(MULTI_SAVE_TRANSFER_READY_APPLY);
	multi_prepare_restore_sync();
	status = android_rewind_restore_authoritative(&restore);
	multi_save_transfer_finish_restore();
	if (status == ANDROID_REWIND_STATUS_RESTORED)
		HUD_init_message(HM_DEFAULT, "Host rewound %d seconds",
		                 Rewind_save_transfer.rewound_seconds);
	else
		HUD_init_message_literal(HM_DEFAULT, "Host rewind failed");
	COOPLOG("rewind transfer apply: status=%d bytes=%u chunks=%d",
	        status, Rewind_save_transfer.total_size,
	        Rewind_save_transfer.total_chunks);
	multi_rewind_receive_reset();
}

static int multi_send_save_transfer_buffer(const unsigned char *data,
                                           size_t total_size,
                                           int transfer_kind,
                                           int requester,
                                           int rewound_seconds,
                                           int64_t game_time64,
                                           int has_collision_delay_last_play_time,
                                           int64_t collision_delay_last_play_time)
{
	int total_chunks;
	uint checksum;
	ubyte transfer_id;
	unsigned char *data_copy;
	int i;

	if (!data || total_size == 0 ||
	    total_size > MULTI_SAVE_TRANSFER_MAX_BYTES)
		return 0;
	if (transfer_kind != MULTI_SAVE_TRANSFER_KIND_REWIND &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_RESTORE &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART)
		return 0;
	total_chunks = (int) ((total_size + MULTI_REWIND_SAVE_CHUNK_PAYLOAD - 1) /
	                      MULTI_REWIND_SAVE_CHUNK_PAYLOAD);
	if (total_chunks <= 0 || total_chunks > 65535)
		return 0;
	if (Save_send_transfer.active) {
		COOPLOG("save transfer refused: another send is active");
		return 0;
	}
	data_copy = (unsigned char *) d_malloc((unsigned int) total_size);
	if (!data_copy)
		return 0;
	memcpy(data_copy, data, total_size);
	checksum = multi_rewind_checksum(data, total_size);
	transfer_id = ++Rewind_save_transfer_id;
	if (!transfer_id)
		transfer_id = ++Rewind_save_transfer_id;

	memset(&Save_send_transfer, 0, sizeof(Save_send_transfer));
	Save_send_transfer.active = 1;
	Save_send_transfer.transfer_kind = transfer_kind;
	Save_send_transfer.transfer_id = transfer_id;
	Save_send_transfer.requester = requester;
	Save_send_transfer.total_size = (uint) total_size;
	Save_send_transfer.checksum = checksum;
	Save_send_transfer.game_time64 = game_time64;
	Save_send_transfer.has_collision_delay_last_play_time =
	    has_collision_delay_last_play_time;
	Save_send_transfer.collision_delay_last_play_time =
	    collision_delay_last_play_time;
	Save_send_transfer.rewound_seconds = rewound_seconds;
	Save_send_transfer.total_chunks = total_chunks;
	Save_send_transfer.started_at = timer_query();
	Save_send_transfer.data = data_copy;
	for (i = 0; i < MAX_PLAYERS; ++i) {
		Save_send_transfer.required_players[i] =
		    (i != Player_num && Players[i].connected == CONNECT_PLAYING) ? 1 : 0;
		Save_send_transfer.ready_players[i] =
		    Save_send_transfer.required_players[i] ? 0 : 1;
		Save_send_transfer.applying_players[i] =
		    Save_send_transfer.required_players[i] ? 0 : 1;
	}

	COOPLOG("save transfer queued: kind=%d id=%u requester=%d bytes=%u chunks=%d checksum=%u",
	        transfer_kind, transfer_id, requester, (uint) total_size,
	        total_chunks, checksum);
	return 1;
}

static int multi_save_transfer_all_players_ready(void)
{
	int i;

	for (i = 0; i < MAX_PLAYERS; ++i)
		if (Save_send_transfer.required_players[i] &&
		    !Save_send_transfer.ready_players[i])
			return 0;
	return 1;
}

static int multi_save_transfer_all_players_applying(void)
{
	int i;

	for (i = 0; i < MAX_PLAYERS; ++i)
		if (Save_send_transfer.required_players[i] &&
		    !Save_send_transfer.applying_players[i])
			return 0;
	return 1;
}

static void multi_save_transfer_send_begin(void)
{
	memset(multibuf, 0, MULTI_REWIND_SAVE_BEGIN_LEN);
	multibuf[0] = MULTI_REWIND_SAVE_BEGIN;
	multibuf[1] = Save_send_transfer.transfer_id;
	multibuf[2] = (ubyte) Save_send_transfer.requester;
	multibuf[3] = (ubyte) multi_rewind_clamp_u8(Save_send_transfer.rewound_seconds);
	PUT_INTEL_INT(multibuf + 4, Save_send_transfer.total_size);
	PUT_INTEL_INT(multibuf + 8, Save_send_transfer.checksum);
	multi_rewind_put_i64(multibuf + 12, Save_send_transfer.game_time64);
	multi_rewind_put_i64(multibuf + 20,
	                     Save_send_transfer.collision_delay_last_play_time);
	multibuf[28] =
	    (ubyte) (Save_send_transfer.has_collision_delay_last_play_time ? 1 : 0);
	multibuf[29] = (ubyte) Save_send_transfer.transfer_kind;
	PUT_INTEL_SHORT(multibuf + 30, (ushort) Save_send_transfer.total_chunks);
	multi_send_data(multibuf, MULTI_REWIND_SAVE_BEGIN_LEN, 2);
}

static void multi_save_transfer_send_chunk(int chunk_index)
{
	size_t offset = (size_t) chunk_index * MULTI_REWIND_SAVE_CHUNK_PAYLOAD;
	size_t data_len = Save_send_transfer.total_size - offset;

	if (data_len > MULTI_REWIND_SAVE_CHUNK_PAYLOAD)
		data_len = MULTI_REWIND_SAVE_CHUNK_PAYLOAD;
	memset(multibuf, 0, MULTI_REWIND_SAVE_CHUNK_LEN);
	multibuf[0] = MULTI_REWIND_SAVE_CHUNK;
	multibuf[1] = Save_send_transfer.transfer_id;
	PUT_INTEL_SHORT(multibuf + 2, (ushort) chunk_index);
	PUT_INTEL_SHORT(multibuf + 4, (ushort) data_len);
	memcpy(multibuf + 8, Save_send_transfer.data + offset, data_len);
	multi_send_data(multibuf, MULTI_REWIND_SAVE_CHUNK_LEN, 2);
}

static void multi_save_transfer_send_apply(void)
{

	memset(multibuf, 0, MULTI_REWIND_SAVE_APPLY_LEN);
	multibuf[0] = MULTI_REWIND_SAVE_APPLY;
	multibuf[1] = Save_send_transfer.transfer_id;
	multibuf[2] = (ubyte) Save_send_transfer.requester;
	multibuf[3] = (ubyte) multi_rewind_clamp_u8(Save_send_transfer.rewound_seconds);
	multi_send_data(multibuf, MULTI_REWIND_SAVE_APPLY_LEN, 2);
}

void multi_save_transfer_frame(void)
{
	int chunks_sent = 0;
	ubyte restore_slot;
	uint restore_game_id;

	if (Rewind_save_transfer.active &&
	    (Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE ||
	     Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART) &&
	    timer_query() > Rewind_save_transfer.started_at + MULTI_SAVE_TRANSFER_TIMEOUT) {
		COOPLOG("coop restore receive timeout: id=%u chunks=%d/%d",
		        Rewind_save_transfer.transfer_id,
		        Rewind_save_transfer.chunks_received,
		        Rewind_save_transfer.total_chunks);
		coop_restore_status_failed();
		multi_rewind_receive_reset();
	}
	if (!Save_send_transfer.active)
		return;
	if (!(Game_mode & GM_MULTI_COOP) || !multi_i_am_master()) {
		if (Save_send_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE)
			coop_restore_status_failed();
		if (Save_send_transfer.level_restart_pending)
			coop_level_restart_transfer_finished(0);
		multi_save_send_reset();
		return;
	}
	if (timer_query() > Save_send_transfer.started_at +
	                        MULTI_SAVE_TRANSFER_TIMEOUT) {
		COOPLOG("save transfer send timeout: kind=%d id=%u chunks=%d/%d",
		        Save_send_transfer.transfer_kind,
		        Save_send_transfer.transfer_id,
		        Save_send_transfer.next_chunk,
		        Save_send_transfer.total_chunks);
		if (Save_send_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE)
			coop_restore_status_failed();
		if (Save_send_transfer.level_restart_pending)
			coop_level_restart_transfer_finished(0);
		multi_save_send_reset();
		return;
	}
	if (!Save_send_transfer.begin_sent) {
		multi_save_transfer_send_begin();
		Save_send_transfer.begin_sent = 1;
		return;
	}
	if (!multi_save_transfer_all_players_ready())
		return;

	while (Save_send_transfer.next_chunk < Save_send_transfer.total_chunks &&
	       chunks_sent < MULTI_SAVE_TRANSFER_CHUNKS_FRAME) {
		multi_save_transfer_send_chunk(Save_send_transfer.next_chunk++);
		chunks_sent++;
	}
	if (Save_send_transfer.next_chunk < Save_send_transfer.total_chunks)
		return;

	if (!Save_send_transfer.apply_sent) {
		multi_save_transfer_send_apply();
		Save_send_transfer.apply_sent = 1;
		COOPLOG("save transfer payload sent: kind=%d id=%u requester=%d bytes=%u chunks=%d checksum=%u",
		        Save_send_transfer.transfer_kind,
		        Save_send_transfer.transfer_id,
		        Save_send_transfer.requester,
		        Save_send_transfer.total_size,
		        Save_send_transfer.total_chunks,
		        Save_send_transfer.checksum);
		return;
	}

	if (!Save_send_transfer.coop_restore_pending &&
	    !Save_send_transfer.level_restart_pending &&
	    Save_send_transfer.transfer_kind != MULTI_SAVE_TRANSFER_KIND_REWIND) {
		multi_save_send_reset();
		return;
	}
	if (!multi_save_transfer_all_players_applying())
		return;

	if (Save_send_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_REWIND) {
		android_rewind_authoritative_restore restore;
		int status;

		memset(&restore, 0, sizeof(restore));
		restore.buffer.data = Save_send_transfer.data;
		restore.buffer.size = Save_send_transfer.total_size;
		restore.buffer.capacity = Save_send_transfer.total_size;
		restore.snapshot_index = -1;
		restore.rewound_seconds = Save_send_transfer.rewound_seconds;
		restore.game_time64 = Save_send_transfer.game_time64;
		restore.has_collision_delay_last_play_time =
		    Save_send_transfer.has_collision_delay_last_play_time;
		restore.collision_delay_last_play_time =
		    Save_send_transfer.collision_delay_last_play_time;
		Save_send_transfer.data = NULL;
		multi_save_send_reset();
		multi_save_transfer_begin_restore();
		multi_prepare_restore_sync();
		status = android_rewind_restore_authoritative(&restore);
		multi_save_transfer_finish_restore();
		d_free(restore.buffer.data);
		HUD_init_message_literal(
		    HM_DEFAULT, status == ANDROID_REWIND_STATUS_RESTORED
		                    ? "Rewind complete"
		                    : "Rewind failed");
		COOPLOG("rewind transfer host apply: status=%d bytes=%u",
		        status, (uint) restore.buffer.size);
		return;
	}

	if (Save_send_transfer.level_restart_pending) {
		int restored;

		multi_save_send_reset();
		multi_save_transfer_begin_restore();
		restored = coop_level_restart_apply_host();
		multi_save_transfer_finish_restore();
		coop_level_restart_transfer_finished(restored);
		HUD_init_message_literal(HM_DEFAULT,
		                         restored ? "Level restarted" : "Level restart failed");
		return;
	}

	restore_slot = Save_send_transfer.coop_restore_slot;
	restore_game_id = Save_send_transfer.coop_restore_game_id;
	COOPLOG("coop restore transfer synchronized: id=%u slot=%u game_id=%u",
	        Save_send_transfer.transfer_id, (uint) restore_slot,
	        restore_game_id);
	multi_save_send_reset();
	multi_save_transfer_begin_restore();
	multi_restore_game(restore_slot, restore_game_id);
	multi_save_transfer_finish_restore();
}

static int multi_send_rewind_save_transfer(
    const android_rewind_authoritative_restore *restore, int requester)
{
	return multi_send_save_transfer_buffer(
	    restore->buffer.data, restore->buffer.size,
	    MULTI_SAVE_TRANSFER_KIND_REWIND, requester,
	    restore->rewound_seconds, restore->game_time64,
	    restore->has_collision_delay_last_play_time,
	    restore->collision_delay_last_play_time);
}

int multi_send_coop_restore_save_transfer(const char *filename, ubyte slot, uint id)
{
	PHYSFS_file *fp;
	PHYSFS_sint64 file_len;
	unsigned char *data;
	int sent;

	Coop_restore_transfer_failed = 0;
	if (!filename || !(Game_mode & GM_MULTI_COOP) || !multi_i_am_master())
		return 0;
	if (!multi_rewind_has_connected_clients()) {
		COOPLOG("coop restore transfer skipped: no connected clients slot=%u id=%u",
		        (uint) slot, id);
		return 1;
	}
	if (!Netgame.PacketLossPrevention) {
		COOPLOG("coop restore transfer refused: packet loss prevention disabled slot=%u id=%u",
		        (uint) slot, id);
		goto failed;
	}

	fp = PHYSFSX_openReadBuffered(filename);
	if (!fp) {
		COOPLOG("coop restore transfer open failed: slot=%u id=%u file='%s'",
		        (uint) slot, id, filename);
		goto failed;
	}
	file_len = PHYSFS_fileLength(fp);
	if (file_len <= 0 || (uint64_t) file_len > MULTI_SAVE_TRANSFER_MAX_BYTES) {
		COOPLOG("coop restore transfer refused: slot=%u id=%u bytes=%u max=%u",
		        (uint) slot, id, (uint) file_len,
		        (uint) MULTI_SAVE_TRANSFER_MAX_BYTES);
		PHYSFS_close(fp);
		goto failed;
	}
	data = (unsigned char *) d_malloc((unsigned int) file_len);
	if (!data) {
		COOPLOG("coop restore transfer alloc failed: slot=%u id=%u bytes=%u",
		        (uint) slot, id, (uint) file_len);
		PHYSFS_close(fp);
		goto failed;
	}
	if (PHYSFS_read(fp, data, 1, (PHYSFS_uint32) file_len) != file_len) {
		COOPLOG("coop restore transfer read failed: slot=%u id=%u bytes=%u file='%s'",
		        (uint) slot, id, (uint) file_len, filename);
		d_free(data);
		PHYSFS_close(fp);
		goto failed;
	}
	PHYSFS_close(fp);

	sent = multi_send_save_transfer_buffer(data, (size_t) file_len,
	                                       MULTI_SAVE_TRANSFER_KIND_RESTORE,
	                                       Player_num, 0, GameTime64, 0, 0);
	if (sent && Save_send_transfer.active) {
		Save_send_transfer.coop_restore_pending = 1;
		Save_send_transfer.coop_restore_slot = slot;
		Save_send_transfer.coop_restore_game_id = id;
	}
	COOPLOG("coop restore transfer send: sent=%d slot=%u id=%u bytes=%u file='%s'",
	        sent, (uint) slot, id, (uint) file_len, filename);
	d_free(data);
	if (!sent)
		goto failed;
	return sent;

failed:
	Coop_restore_transfer_failed = 1;
	coop_restore_status_failed();
	return -1;
}

int multi_coop_restore_transfer_pending(void)
{
	return (Save_send_transfer.active &&
	        Save_send_transfer.coop_restore_pending) ||
	       Coop_restore_transfer_failed;
}

int multi_save_transfer_busy(void)
{
	return Save_send_transfer.active || Rewind_save_transfer.active;
}

int multi_send_level_restart_transfer(const rewind_memory_buffer *buffer)
{
	int sent;

	if (!buffer || !buffer->data || !buffer->size ||
	    !(Game_mode & GM_MULTI_COOP) || !multi_i_am_master())
		return 0;
	if (!multi_rewind_has_connected_clients()) {
		int restored;

		multi_save_transfer_begin_restore();
		restored = coop_level_restart_apply_host();
		multi_save_transfer_finish_restore();
		coop_level_restart_transfer_finished(restored);
		return restored;
	}
	if (!Netgame.PacketLossPrevention)
		return 0;
	sent = multi_send_save_transfer_buffer(
	    buffer->data, buffer->size, MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART,
	    Player_num, 0, GameTime64, 0, 0);
	if (sent)
		Save_send_transfer.level_restart_pending = 1;
	return sent;
}

static void multi_send_rewind_result(int requester, int status, int rewound_seconds)
{
	if (requester < 0 || requester >= N_players)
		return;
	multibuf[0] = MULTI_REWIND_RESULT;
	multibuf[1] = (ubyte) requester;
	multibuf[2] = (ubyte) status;
	multibuf[3] = (ubyte) multi_rewind_clamp_u8(rewound_seconds);
	multi_send_data_direct(multibuf, 4, requester, 2);
}

void multi_send_rewind_request(void)
{
	static ubyte request_id = 0;

	if (!(Game_mode & GM_MULTI_COOP))
		return;
	if (multi_i_am_master())
		return;
	multibuf[0] = MULTI_REWIND_REQUEST;
	multibuf[1] = (ubyte) Player_num;
	multibuf[2] = ++request_id;
	multi_send_data_direct(multibuf, 3, multi_who_is_master(), 2);
	HUD_init_message_literal(HM_DEFAULT, "Rewind requested");
}

int multi_perform_rewind_request(int requester, int *rewound_seconds)
{
	android_rewind_authoritative_restore restore;
	int status;
	int has_clients;

	if (rewound_seconds)
		*rewound_seconds = 0;
	if (!(Game_mode & GM_MULTI_COOP))
		return ANDROID_REWIND_STATUS_BLOCKED_MULTIPLAYER;
	if (!multi_i_am_master())
		return ANDROID_REWIND_STATUS_NOT_HOST;

	status = android_rewind_select_restore(&restore);
	if (rewound_seconds)
		*rewound_seconds = restore.rewound_seconds;
	if (status != ANDROID_REWIND_STATUS_RESTORED)
		return status;
	if (!restore.buffer.data || restore.buffer.size == 0 ||
	    restore.buffer.size > MULTI_SAVE_TRANSFER_MAX_BYTES) {
		COOPLOG("rewind transfer refused: bytes=%u max=%u",
		        (uint) restore.buffer.size, (uint) MULTI_SAVE_TRANSFER_MAX_BYTES);
		return ANDROID_REWIND_STATUS_FAILED;
	}

	has_clients = multi_rewind_has_connected_clients();
	if (has_clients && !Netgame.PacketLossPrevention) {
		COOPLOG("rewind transfer refused: packet loss prevention disabled");
		return ANDROID_REWIND_STATUS_FAILED;
	}

	if (multi_save_transfer_host_action_for_rewind(has_clients) ==
	    MULTI_SAVE_TRANSFER_HOST_WAIT_FOR_CLIENTS) {
		if (!multi_send_rewind_save_transfer(&restore, requester)) {
			COOPLOG("rewind transfer queue failed before host restore");
			status = ANDROID_REWIND_STATUS_FAILED;
		} else
			HUD_init_message_literal(HM_DEFAULT, "Waiting to rewind");
	} else {
		multi_save_transfer_begin_restore();
		status = android_rewind_restore_authoritative(&restore);
		multi_save_transfer_finish_restore();
	}
	if (rewound_seconds)
		*rewound_seconds = restore.rewound_seconds;
	return status;
}

void multi_do_rewind_request(const ubyte *buf)
{
	static fix64 next_request_time[MAX_PLAYERS] = { 0 };
	int requester = buf[1];
	int rewound_seconds = 0;
	int status;
	int requester_valid = multi_rewind_requester_valid(requester);

	if (!multi_i_am_master())
		return;
	if (!(Game_mode & GM_MULTI_COOP)) {
		if (requester_valid)
			multi_send_rewind_result(requester, ANDROID_REWIND_STATUS_BLOCKED_MULTIPLAYER, 0);
		return;
	}
	if (!android_rewind_is_client_request_allowed(1, 1, 1,
	                                              android_rewind_clients_can_request(),
	                                              requester_valid)) {
		if (requester_valid)
			multi_send_rewind_result(requester,
			                         android_rewind_clients_can_request() ? ANDROID_REWIND_STATUS_BLOCKED_MULTIPLAYER : ANDROID_REWIND_STATUS_DISABLED,
			                         0);
		return;
	}
	if (GameTime64 < next_request_time[requester]) {
		multi_send_rewind_result(requester, ANDROID_REWIND_STATUS_BLOCKED_MULTIPLAYER, 0);
		return;
	}
	next_request_time[requester] = GameTime64 + F1_0;
	status = multi_perform_rewind_request(requester, &rewound_seconds);
	if (status != ANDROID_REWIND_STATUS_RESTORED)
		multi_send_rewind_result(requester, status, rewound_seconds);
}

void multi_do_rewind_result(const ubyte *buf)
{
	int requester = buf[1];
	int status = buf[2];
	int rewound_seconds = buf[3];

	if (requester != Player_num)
		return;
	switch (status) {
		case ANDROID_REWIND_STATUS_RESTORED:
			HUD_init_message(HM_DEFAULT, "Host rewound %d seconds", rewound_seconds);
			break;
		case ANDROID_REWIND_STATUS_DISABLED:
			HUD_init_message_literal(HM_DEFAULT, "Rewind requests disabled");
			break;
		case ANDROID_REWIND_STATUS_NO_POINT:
			HUD_init_message_literal(HM_DEFAULT, "Host has no rewind point yet");
			break;
		case ANDROID_REWIND_STATUS_BLOCKED_MULTIPLAYER:
			HUD_init_message_literal(HM_DEFAULT, "Rewind request denied");
			break;
		default:
			HUD_init_message_literal(HM_DEFAULT, "Host rewind failed");
			break;
	}
}

void multi_do_rewind_save_begin(const ubyte *buf)
{
	uint total_size;
	uint checksum;
	int transfer_kind;
	int total_chunks;
	int expected_chunks;

	if (multi_i_am_master() || !(Game_mode & GM_MULTI_COOP))
		return;
	total_size = GET_INTEL_INT(buf + 4);
	checksum = GET_INTEL_INT(buf + 8);
	transfer_kind = buf[29];
	total_chunks = GET_INTEL_SHORT(buf + 30);
	if (total_size == 0 || total_size > MULTI_SAVE_TRANSFER_MAX_BYTES ||
	    total_chunks <= 0) {
		COOPLOG("save transfer begin rejected: bytes=%u chunks=%d",
		        total_size, total_chunks);
		if (transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE)
			coop_restore_status_failed();
		multi_rewind_receive_reset();
		return;
	}
	if (transfer_kind != MULTI_SAVE_TRANSFER_KIND_REWIND &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_RESTORE &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART) {
		COOPLOG("save transfer begin rejected: unknown kind=%d",
		        transfer_kind);
		multi_rewind_receive_reset();
		return;
	}
	expected_chunks = (int) ((total_size + MULTI_REWIND_SAVE_CHUNK_PAYLOAD - 1) /
	                         MULTI_REWIND_SAVE_CHUNK_PAYLOAD);
	if (total_chunks != expected_chunks) {
		COOPLOG("save transfer begin rejected: bytes=%u chunks=%d expected=%d",
		        total_size, total_chunks, expected_chunks);
		if (transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE)
			coop_restore_status_failed();
		multi_rewind_receive_reset();
		return;
	}

	multi_rewind_receive_reset();
	if (transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE)
		coop_restore_status_waiting();
	Rewind_save_transfer.data = (unsigned char *) d_malloc(total_size);
	Rewind_save_transfer.chunk_received =
	    (unsigned char *) d_malloc((unsigned int) total_chunks);
	if (!Rewind_save_transfer.data || !Rewind_save_transfer.chunk_received) {
		HUD_init_message_literal(HM_DEFAULT,
		                         transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE
		                             ? "Host save sync failed"
		                             : "Host rewind sync failed");
		if (transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE)
			coop_restore_status_failed();
		multi_rewind_receive_reset();
		return;
	}
	memset(Rewind_save_transfer.chunk_received, 0, (size_t) total_chunks);
	Rewind_save_transfer.active = 1;
	Rewind_save_transfer.transfer_kind = transfer_kind;
	Rewind_save_transfer.transfer_id = buf[1];
	Rewind_save_transfer.requester = buf[2];
	Rewind_save_transfer.rewound_seconds = buf[3];
	Rewind_save_transfer.total_size = total_size;
	Rewind_save_transfer.checksum = checksum;
	Rewind_save_transfer.game_time64 = multi_rewind_get_i64(buf + 12);
	Rewind_save_transfer.collision_delay_last_play_time =
	    multi_rewind_get_i64(buf + 20);
	Rewind_save_transfer.has_collision_delay_last_play_time = buf[28] ? 1 : 0;
	Rewind_save_transfer.total_chunks = total_chunks;
	Rewind_save_transfer.started_at = timer_query();
	HUD_init_message_literal(HM_DEFAULT,
	                         transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE
	                             ? "Receiving host save"
	                         : transfer_kind == MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART
	                             ? "Receiving level restart"
	                             : "Receiving host rewind");
	multi_rewind_send_ready(MULTI_SAVE_TRANSFER_READY_BUFFER);
	COOPLOG("save transfer begin: kind=%d id=%u requester=%d bytes=%u chunks=%d checksum=%u",
	        transfer_kind, Rewind_save_transfer.transfer_id,
	        Rewind_save_transfer.requester, total_size, total_chunks, checksum);
}

void multi_do_rewind_save_ready(const ubyte *buf)
{
	int pnum = buf[2];
	int phase = buf[3];

	if (!multi_i_am_master() || !Save_send_transfer.active ||
	    buf[1] != Save_send_transfer.transfer_id ||
	    pnum < 0 || pnum >= N_players || pnum == Player_num ||
	    Players[pnum].connected != CONNECT_PLAYING ||
	    !Save_send_transfer.required_players[pnum])
		return;
	if (phase == MULTI_SAVE_TRANSFER_READY_BUFFER) {
		Save_send_transfer.ready_players[pnum] = 1;
		COOPLOG("save transfer buffer ready: id=%u player=%d",
		        Save_send_transfer.transfer_id, pnum);
	} else if (phase == MULTI_SAVE_TRANSFER_READY_APPLY &&
	           Save_send_transfer.apply_sent &&
	           Save_send_transfer.ready_players[pnum]) {
		Save_send_transfer.applying_players[pnum] = 1;
		COOPLOG("save transfer apply ready: id=%u player=%d",
		        Save_send_transfer.transfer_id, pnum);
	}
}

void multi_do_rewind_save_chunk(const ubyte *buf)
{
	int chunk_index;
	int data_len;
	size_t offset;
	size_t expected_len;

	if (multi_i_am_master() || !Rewind_save_transfer.active ||
	    buf[1] != Rewind_save_transfer.transfer_id)
		return;
	chunk_index = GET_INTEL_SHORT(buf + 2);
	data_len = GET_INTEL_SHORT(buf + 4);
	if (chunk_index < 0 || chunk_index >= Rewind_save_transfer.total_chunks ||
	    data_len <= 0 || data_len > MULTI_REWIND_SAVE_CHUNK_PAYLOAD)
		return;
	offset = (size_t) chunk_index * MULTI_REWIND_SAVE_CHUNK_PAYLOAD;
	if (offset >= Rewind_save_transfer.total_size)
		return;
	expected_len = Rewind_save_transfer.total_size - offset;
	if (expected_len > MULTI_REWIND_SAVE_CHUNK_PAYLOAD)
		expected_len = MULTI_REWIND_SAVE_CHUNK_PAYLOAD;
	if ((size_t) data_len != expected_len ||
	    offset + (size_t) data_len > Rewind_save_transfer.total_size)
		return;
	if (!Rewind_save_transfer.chunk_received[chunk_index]) {
		memcpy(Rewind_save_transfer.data + offset, buf + 8, (size_t) data_len);
		Rewind_save_transfer.chunk_received[chunk_index] = 1;
		Rewind_save_transfer.chunks_received++;
	}
	multi_rewind_apply_received_transfer();
}

void multi_do_rewind_save_apply(const ubyte *buf)
{
	if (multi_i_am_master() || !Rewind_save_transfer.active ||
	    buf[1] != Rewind_save_transfer.transfer_id)
		return;
	Rewind_save_transfer.apply_pending = 1;
	Rewind_save_transfer.rewound_seconds = buf[3];
	multi_rewind_apply_received_transfer();
}

#endif /* __ANDROID__ */
