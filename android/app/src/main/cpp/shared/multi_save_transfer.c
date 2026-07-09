#ifdef __ANDROID__

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "android_log.h"
#include "android_rewind.h"
#include "android_rewind_policy.h"
#include "byteswap.h"
#include "fix.h"
#include "game.h"
#include "hudmsg.h"
#include "multi.h"
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
	unsigned char *data;
	unsigned char *chunk_received;
} multi_rewind_save_transfer;

#define MULTI_SAVE_TRANSFER_KIND_REWIND  0
#define MULTI_SAVE_TRANSFER_KIND_RESTORE 1
#define MULTI_SAVE_TRANSFER_MAX_BYTES    (2u * 1024u * 1024u)

static multi_rewind_save_transfer Rewind_save_transfer;
static ubyte Rewind_save_transfer_id = 0;

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
		multi_rewind_receive_reset();
		return;
	}

	if (Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE) {
		buffer.data = Rewind_save_transfer.data;
		buffer.size = Rewind_save_transfer.total_size;
		buffer.capacity = Rewind_save_transfer.total_size;
		restored = state_restore_from_memory(&buffer);
		if (restored) {
			HUD_init_message_literal(HM_DEFAULT, "Host save restored");
			multi_send_score();
		} else {
			HUD_init_message_literal(HM_DEFAULT, "Host save failed");
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
	status = android_rewind_restore_authoritative(&restore);
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
	int chunk_index;
	uint checksum;
	ubyte transfer_id;

	if (!data || total_size == 0 ||
	    total_size > MULTI_SAVE_TRANSFER_MAX_BYTES)
		return 0;
	if (transfer_kind != MULTI_SAVE_TRANSFER_KIND_REWIND &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_RESTORE)
		return 0;
	total_chunks = (int) ((total_size + MULTI_REWIND_SAVE_CHUNK_PAYLOAD - 1) /
	                      MULTI_REWIND_SAVE_CHUNK_PAYLOAD);
	if (total_chunks <= 0 || total_chunks > 65535)
		return 0;
	checksum = multi_rewind_checksum(data, total_size);
	transfer_id = ++Rewind_save_transfer_id;
	if (!transfer_id)
		transfer_id = ++Rewind_save_transfer_id;

	memset(multibuf, 0, MULTI_REWIND_SAVE_BEGIN_LEN);
	multibuf[0] = MULTI_REWIND_SAVE_BEGIN;
	multibuf[1] = transfer_id;
	multibuf[2] = (ubyte) requester;
	multibuf[3] = (ubyte) multi_rewind_clamp_u8(rewound_seconds);
	PUT_INTEL_INT(multibuf + 4, (uint) total_size);
	PUT_INTEL_INT(multibuf + 8, checksum);
	multi_rewind_put_i64(multibuf + 12, game_time64);
	multi_rewind_put_i64(multibuf + 20, collision_delay_last_play_time);
	multibuf[28] = (ubyte) (has_collision_delay_last_play_time ? 1 : 0);
	multibuf[29] = (ubyte) transfer_kind;
	PUT_INTEL_SHORT(multibuf + 30, (ushort) total_chunks);
	multi_send_data(multibuf, MULTI_REWIND_SAVE_BEGIN_LEN, 2);

	for (chunk_index = 0; chunk_index < total_chunks; chunk_index++) {
		size_t offset = (size_t) chunk_index * MULTI_REWIND_SAVE_CHUNK_PAYLOAD;
		size_t data_len = total_size - offset;
		if (data_len > MULTI_REWIND_SAVE_CHUNK_PAYLOAD)
			data_len = MULTI_REWIND_SAVE_CHUNK_PAYLOAD;
		memset(multibuf, 0, MULTI_REWIND_SAVE_CHUNK_LEN);
		multibuf[0] = MULTI_REWIND_SAVE_CHUNK;
		multibuf[1] = transfer_id;
		PUT_INTEL_SHORT(multibuf + 2, (ushort) chunk_index);
		PUT_INTEL_SHORT(multibuf + 4, (ushort) data_len);
		memcpy(multibuf + 8, data + offset, data_len);
		multi_send_data(multibuf, MULTI_REWIND_SAVE_CHUNK_LEN, 2);
	}

	memset(multibuf, 0, MULTI_REWIND_SAVE_APPLY_LEN);
	multibuf[0] = MULTI_REWIND_SAVE_APPLY;
	multibuf[1] = transfer_id;
	multibuf[2] = (ubyte) requester;
	multibuf[3] = (ubyte) multi_rewind_clamp_u8(rewound_seconds);
	multi_send_data(multibuf, MULTI_REWIND_SAVE_APPLY_LEN, 2);

	COOPLOG("save transfer sent: kind=%d id=%u requester=%d bytes=%u chunks=%d checksum=%u",
	        transfer_kind, transfer_id, requester, (uint) total_size,
	        total_chunks, checksum);
	return 1;
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
		return 0;
	}

	fp = PHYSFSX_openReadBuffered(filename);
	if (!fp) {
		COOPLOG("coop restore transfer open failed: slot=%u id=%u file='%s'",
		        (uint) slot, id, filename);
		return 0;
	}
	file_len = PHYSFS_fileLength(fp);
	if (file_len <= 0 || (uint64_t) file_len > MULTI_SAVE_TRANSFER_MAX_BYTES) {
		COOPLOG("coop restore transfer refused: slot=%u id=%u bytes=%u max=%u",
		        (uint) slot, id, (uint) file_len,
		        (uint) MULTI_SAVE_TRANSFER_MAX_BYTES);
		PHYSFS_close(fp);
		return 0;
	}
	data = (unsigned char *) d_malloc((unsigned int) file_len);
	if (!data) {
		COOPLOG("coop restore transfer alloc failed: slot=%u id=%u bytes=%u",
		        (uint) slot, id, (uint) file_len);
		PHYSFS_close(fp);
		return 0;
	}
	if (PHYSFS_read(fp, data, 1, (PHYSFS_uint32) file_len) != file_len) {
		COOPLOG("coop restore transfer read failed: slot=%u id=%u bytes=%u file='%s'",
		        (uint) slot, id, (uint) file_len, filename);
		d_free(data);
		PHYSFS_close(fp);
		return 0;
	}
	PHYSFS_close(fp);

	sent = multi_send_save_transfer_buffer(data, (size_t) file_len,
	                                       MULTI_SAVE_TRANSFER_KIND_RESTORE,
	                                       Player_num, 0, GameTime64, 0, 0);
	COOPLOG("coop restore transfer send: sent=%d slot=%u id=%u bytes=%u file='%s'",
	        sent, (uint) slot, id, (uint) file_len, filename);
	d_free(data);
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

	status = android_rewind_restore_authoritative(&restore);
	if (status == ANDROID_REWIND_STATUS_RESTORED && has_clients &&
	    !multi_send_rewind_save_transfer(&restore, requester))
		COOPLOG("rewind transfer send failed after host restore");
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
		multi_rewind_receive_reset();
		return;
	}
	if (transfer_kind != MULTI_SAVE_TRANSFER_KIND_REWIND &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_RESTORE) {
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
		multi_rewind_receive_reset();
		return;
	}

	multi_rewind_receive_reset();
	Rewind_save_transfer.data = (unsigned char *) d_malloc(total_size);
	Rewind_save_transfer.chunk_received =
	    (unsigned char *) d_malloc((unsigned int) total_chunks);
	if (!Rewind_save_transfer.data || !Rewind_save_transfer.chunk_received) {
		HUD_init_message_literal(HM_DEFAULT,
		                         transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE
		                             ? "Host save sync failed"
		                             : "Host rewind sync failed");
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
	HUD_init_message_literal(HM_DEFAULT,
	                         transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE
	                             ? "Receiving host save"
	                             : "Receiving host rewind");
	COOPLOG("save transfer begin: kind=%d id=%u requester=%d bytes=%u chunks=%d checksum=%u",
	        transfer_kind, Rewind_save_transfer.transfer_id,
	        Rewind_save_transfer.requester, total_size, total_chunks, checksum);
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
