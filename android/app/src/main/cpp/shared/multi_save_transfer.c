#ifdef __ANDROID__

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "android_log.h"
#include "android_rewind.h"
#include "android_rewind_policy.h"
#include "byteswap.h"
#include "coop_save.h"
#include "coop/coop_briefing.h"
#include "coop/coop_travel.h"
#include "coop/coop_world_visit.h"
#include "coop/coop_gameplay_runtime.h"
#include "coop/coop_level_restart.h"
#include "coop/coop_recovery.h"
#include "coop/coop_multi_status.h"
#include "fix.h"
#include "game.h"
#include "cntrlcen.h"
#include "hudmsg.h"
#include "multi.h"
#include "net_udp.h"
#include "newmenu.h"
#include "text.h"
#include "multi_save_transfer_policy.h"
#include "physfsx.h"
#include "player.h"
#include "object.h"

extern sbyte PKilledFlags[MAX_PLAYERS];
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
	uint32_t recovery_epoch;
	uint64_t world_visit;
	int64_t game_time64;
	int has_collision_delay_last_play_time;
	int64_t collision_delay_last_play_time;
	int rewound_seconds;
	int total_chunks;
	int chunks_received;
	int apply_pending;
	uint64_t started_ms;
	uint64_t progress_ms;
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
	uint32_t recovery_epoch;
	uint64_t world_visit;
	int64_t game_time64;
	int has_collision_delay_last_play_time;
	int64_t collision_delay_last_play_time;
	int rewound_seconds;
	int total_chunks;
	int next_chunk;
	unsigned window_waits;
	unsigned peak_pending;
	int begin_sent;
	int apply_sent;
	int coop_restore_pending;
	int level_restart_pending;
	int rewind_snapshot_index;
	ubyte coop_restore_slot;
	uint coop_restore_game_id;
	uint64_t started_ms;
	uint64_t progress_ms;
	unsigned char required_players[MAX_PLAYERS];
	multi_save_transfer_budget chunk_budget;
	unsigned char ready_players[MAX_PLAYERS];
	unsigned char applying_players[MAX_PLAYERS];
	unsigned char *data;
} multi_save_send_transfer;

#define MULTI_SAVE_TRANSFER_READY_BUFFER 1
#define MULTI_SAVE_TRANSFER_READY_APPLY  2

typedef char multi_save_transfer_chunk_count_fits_wire[(MULTI_SAVE_TRANSFER_MAX_BYTES + MULTI_REWIND_SAVE_CHUNK_PAYLOAD - 1) /
                                                                   MULTI_REWIND_SAVE_CHUNK_PAYLOAD <=
                                                               UINT16_MAX
                                                           ? 1
                                                           : -1];

static uint64_t multi_save_transfer_now_ms(void)
{
	return (uint64_t) timer_query() * 1000 / F1_0;
}

static multi_rewind_save_transfer Rewind_save_transfer;
static multi_save_send_transfer Save_send_transfer;
static ubyte Rewind_save_transfer_id = 0;
static int Save_transfer_restore_active;
static fix64 Save_transfer_timeout_grace_until;
static int Coop_restore_transfer_failed;
static uint32_t Coop_restore_status_revision;
static int Coop_restore_status_sender = -1;
static int Test_rollback_waiting, Test_rollback_dropped, Test_rollback_waiting_players;
static int Test_delay_world_apply;
static uint64_t Test_world_apply_after;

void multi_save_transfer_test_delay_world_apply(void)
{
	if (!multi_i_am_master()) return;
	Test_delay_world_apply = 1;
	Test_world_apply_after = 0;
}

static int restore_barrier_waiting_peer(const ubyte *data, int len, int pnum);

/* Only isolated packets from the current restore/rollback may bypass the playing roster */
int multi_save_transfer_waiting_peer(const ubyte *data, int len, int pnum)
{
	if (!data || len < 2 || pnum < 0 || pnum >= N_players ||
	    Players[pnum].connected != CONNECT_WAITING) return 0;
	if (restore_barrier_waiting_peer(data, len, pnum)) return 1;
	if (!coop_travel_rollback_allowed()) return 0;

	if (multi_i_am_master()) {
		if (!Save_send_transfer.active || Save_send_transfer.transfer_kind != MULTI_SAVE_TRANSFER_KIND_ROLLBACK ||
		    !Save_send_transfer.required_players[pnum] || data[1] != Save_send_transfer.transfer_id) return 0;
		return (data[0] == MULTI_REWIND_SAVE_BEGIN && len == MULTI_REWIND_SAVE_BEGIN_LEN &&
		        data[29] == MULTI_SAVE_TRANSFER_KIND_ROLLBACK) ||
		       (data[0] == MULTI_REWIND_SAVE_CHUNK && len == MULTI_REWIND_SAVE_CHUNK_LEN) ||
		       (data[0] == MULTI_REWIND_SAVE_APPLY && len == MULTI_REWIND_SAVE_APPLY_LEN);
	}
	return pnum == multi_who_is_master() && Rewind_save_transfer.active &&
	       Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_ROLLBACK &&
	       data[1] == Rewind_save_transfer.transfer_id && data[0] == MULTI_REWIND_SAVE_READY &&
	       len == MULTI_REWIND_SAVE_READY_LEN;
}

/* Android automation: require recovery delivery to a waiting peer, including a retry */
int multi_save_transfer_test_rollback_waiting(int verify)
{
	if (verify) return Test_rollback_waiting_players > 0 && Test_rollback_dropped == 1;
	if (!multi_i_am_master() || multi_save_transfer_busy() || coop_travel_active()) return 0;
	Test_rollback_waiting = 1;
	Test_rollback_dropped = Test_rollback_waiting_players = 0;
	return 1;
}

int multi_save_transfer_test_drop_packet(const ubyte *data, int len, int pnum)
{
	if (!Test_rollback_waiting || Test_rollback_dropped || !Netgame.PacketLossPrevention ||
	    !multi_save_transfer_waiting_peer(data, len, pnum) || data[0] != MULTI_REWIND_SAVE_CHUNK) return 0;
	Test_rollback_dropped = 1;
	Test_rollback_waiting = 0;
	COOPLOG("travel rollback test dropped initial chunk: player=%d id=%u", pnum, data[1]);
	return 1;
}

void multi_reset_coop_restore_status(void)
{
	Coop_restore_status_revision = 0;
	Coop_restore_status_sender = -1;
}

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

#include "multi_save_transfer_barrier.h"

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
	if (Rewind_save_transfer.active && restore_barrier_kind(Rewind_save_transfer.transfer_kind) &&
	    !Save_transfer_restore_active && !Restore_barrier.local_loaded) restore_barrier_fail("The save transfer was interrupted");
	if (Rewind_save_transfer.data)
		d_free(Rewind_save_transfer.data);
	if (Rewind_save_transfer.chunk_received)
		d_free(Rewind_save_transfer.chunk_received);
	memset(&Rewind_save_transfer, 0, sizeof(Rewind_save_transfer));
}

static void multi_save_send_reset(void)
{
	if (Save_send_transfer.active && restore_barrier_kind(Save_send_transfer.transfer_kind) &&
	    !Save_transfer_restore_active && !Restore_barrier.local_loaded) restore_barrier_fail("The save transfer was interrupted");
	if (Save_send_transfer.data)
		d_free(Save_send_transfer.data);
	memset(&Save_send_transfer, 0, sizeof(Save_send_transfer));
}

void multi_save_transfer_barrier_reset(void)
{
	restore_barrier_unpause();
	memset(&Restore_pause_test, 0, sizeof(Restore_pause_test));
	memset(&Restore_barrier, 0, sizeof(Restore_barrier));
	multi_save_send_reset();
	multi_rewind_receive_reset();
}

int multi_save_transfer_pause_frame(void)
{
	if (!multi_save_transfer_paused()) return 0;
	if (Restore_barrier_pumping || Save_transfer_restore_active) return 1;
	Restore_barrier_pumping = 1;
	multi_do_protocol_frame(0, 1);
	multi_save_transfer_frame();
	Restore_barrier_pumping = 0;
	if (multi_quit_game || Restore_barrier.phase == RESTORE_FAILED) {
		multi_save_transfer_barrier_reset();
		return -1;
	}
	return 0;
}

void multi_cancel_coop_travel_transfer(void)
{
	if (Save_transfer_restore_active) return;
	if (Save_send_transfer.active && Save_send_transfer.transfer_kind >= MULTI_SAVE_TRANSFER_KIND_WORLD &&
	    Save_send_transfer.transfer_kind <= MULTI_SAVE_TRANSFER_KIND_CHECKPOINT) multi_save_send_reset();
	if (Rewind_save_transfer.active && Rewind_save_transfer.transfer_kind >= MULTI_SAVE_TRANSFER_KIND_WORLD &&
	    Rewind_save_transfer.transfer_kind <= MULTI_SAVE_TRANSFER_KIND_CHECKPOINT) multi_rewind_receive_reset();
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

int multi_save_transfer_restoring(void)
{
	return Save_transfer_restore_active;
}

static int multi_save_transfer_begin_restore(uint32_t generation, uint64_t visit)
{
	restore_pause_test_applying();
	if (!visit) visit = coop_world_visit_reserve();
	if (!coop_world_visit_activate(visit)) {
		COOPLOG("world visit restore refused: requested=%llu active=%llu reserved=%llu",
		        (unsigned long long) visit, (unsigned long long) coop_world_visit_current(),
		        (unsigned long long) coop_world_visit_high_water());
		return 0;
	}
	COOPLOG("world visit restore begin: visit=%llu source_level=%d",
	        (unsigned long long) visit, Current_level_num);
	coop_clear_pending_restore_inventory();
	coop_recovery_begin_restore(generation);
	Save_transfer_restore_active = 1;
	if (restore_barrier_busy()) Restore_barrier.deadline = multi_save_transfer_now_ms() + 60000u;
	Save_transfer_timeout_grace_until = timer_query() + F1_0 * 60;
	return 1;
}

void coop_gameplay_restore_player_life(void)
{
	/* Level initialization marks remote ships killed until REAPPEAR. During a
	 * coordinated load that gameplay packet is fenced, so derive this state
	 * from the successfully restored world without respawn effects or gear */
	for (int i = 0; i < N_players; ++i) {
		int objnum = Players[i].objnum;
		if (Players[i].connected != CONNECT_PLAYING || objnum < 0 || objnum > Highest_object_index) continue;
		int killed = Objects[objnum].type != OBJ_PLAYER || Players[i].shields < 0 ||
		             (i == Player_num && Player_is_dead);
		COOPLOG("restore player life: player=%d visit=%llu killed=%d/%d object_type=%d shields=%d",
		        i, (unsigned long long) coop_world_visit_current(), PKilledFlags[i], killed,
		        Objects[objnum].type, Players[i].shields);
		PKilledFlags[i] = (sbyte) killed;
	}
}

static void multi_save_transfer_finish_restore(int restored)
{
	if (restored) coop_gameplay_restore_player_life();
	restore_barrier_local_done(restored);
	coop_recovery_end_restore();
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
	if (!++Coop_restore_status_revision)
		++Coop_restore_status_revision;
	PUT_INTEL_INT(multibuf + 2, Coop_restore_status_revision);
	COOPLOG("restore status send: status=%d revision=%u", status, Coop_restore_status_revision);
	multi_send_data(multibuf, 6, 2);
}

void multi_do_coop_restore_status(const ubyte *buf, int authenticated_sender)
{
	uint32_t revision = (uint32_t) GET_INTEL_INT(buf + 2);
	if (multi_i_am_master() || !(Game_mode & GM_MULTI_COOP) ||
	    authenticated_sender != multi_who_is_master() || buf[1] > 2 || !revision)
		return;
	if (Coop_restore_status_sender == authenticated_sender && Coop_restore_status_revision &&
	    (int32_t) (revision - Coop_restore_status_revision) <= 0) {
		COOPLOG("restore status ignored: status=%u revision=%u current=%u",
		        buf[1], revision, Coop_restore_status_revision);
		return;
	}
	Coop_restore_status_revision = revision;
	Coop_restore_status_sender = authenticated_sender;
	COOPLOG("restore status receive: status=%u revision=%u", buf[1], revision);
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
	coop_world_visit_write(multibuf + 4, Rewind_save_transfer.world_visit);
	multi_send_data_direct(multibuf, MULTI_REWIND_SAVE_READY_LEN,
	                       multi_who_is_master(), 2);
}

static int multi_apply_world_transfer(const rewind_memory_buffer *buffer, int kind)
{
	int result;
	if (kind == MULTI_SAVE_TRANSFER_KIND_ROLLBACK) {
		result = coop_travel_restore_source(buffer->data, buffer->size);
		if (!result) coop_travel_world_failed();
		return result;
	}
	if (kind != MULTI_SAVE_TRANSFER_KIND_WORLD && kind != MULTI_SAVE_TRANSFER_KIND_FRESH_WORLD)
		return state_restore_coop_from_memory(buffer);
	if (!coop_travel_world_apply_allowed()) return 0;
	coop_travel_destination_started();
	if (kind == MULTI_SAVE_TRANSFER_KIND_FRESH_WORLD) {
		result = buffer->size == 4 && coop_initialize_travel_world((int32_t) GET_INTEL_INT(buffer->data));
	} else result = coop_restore_world_from_memory(buffer);
	if (result) coop_travel_destination_applied();
	else coop_travel_world_failed();
	return result;
}

static void multi_rewind_apply_received_transfer(int at_frame_boundary)
{
	android_rewind_authoritative_restore restore;
	rewind_memory_buffer buffer = { 0 };
	uint checksum;
	int status;
	int restored;
	if (Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_ROLLBACK && !coop_travel_rollback_allowed()) return;

	if ((Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_CAMPAIGN ||
	     Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_CHECKPOINT) &&
	    !coop_travel_campaign_stage_allowed()) return;
	if ((Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_WORLD ||
	     Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_FRESH_WORLD) &&
	    !coop_travel_world_apply_allowed()) return;

	if (multi_save_transfer_client_apply_action_for_context(
	        at_frame_boundary, Rewind_save_transfer.active,
	        Rewind_save_transfer.apply_pending,
	        Rewind_save_transfer.chunks_received,
	        Rewind_save_transfer.total_chunks) !=
	    MULTI_SAVE_TRANSFER_CLIENT_APPLY)
		return;
	COOPLOG("save transfer applying at frame boundary: kind=%d id=%u bytes=%u chunks=%d",
	        Rewind_save_transfer.transfer_kind,
	        Rewind_save_transfer.transfer_id,
	        Rewind_save_transfer.total_size,
	        Rewind_save_transfer.total_chunks);
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
		if (Rewind_save_transfer.transfer_kind >= MULTI_SAVE_TRANSFER_KIND_WORLD)
			coop_travel_world_failed();
		multi_rewind_receive_reset();
		return;
	}

	if (Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_CAMPAIGN ||
	    Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_CHECKPOINT) {
		int staged = Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_CHECKPOINT
		                 ? coop_travel_stage_checkpoint(Rewind_save_transfer.data, Rewind_save_transfer.total_size)
		                 : coop_travel_stage_campaign(Rewind_save_transfer.data, Rewind_save_transfer.total_size);
		if (staged)
			multi_rewind_send_ready(MULTI_SAVE_TRANSFER_READY_APPLY);
		else coop_travel_world_failed();
		multi_rewind_receive_reset();
		return;
	}

	if (Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE ||
	    Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART ||
	    Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_WORLD ||
	    Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_FRESH_WORLD ||
	    Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_ROLLBACK) {
		buffer.data = Rewind_save_transfer.data;
		buffer.size = Rewind_save_transfer.total_size;
		buffer.capacity = Rewind_save_transfer.total_size;
		if (!multi_save_transfer_begin_restore(Rewind_save_transfer.recovery_epoch, Rewind_save_transfer.world_visit)) {
			coop_restore_status_failed();
			coop_travel_world_failed();
			multi_rewind_receive_reset();
			return;
		}
		multi_rewind_send_ready(MULTI_SAVE_TRANSFER_READY_APPLY);
		multi_prepare_restore_sync();
		restored = multi_apply_world_transfer(&buffer, Rewind_save_transfer.transfer_kind);
		multi_save_transfer_finish_restore(restored);
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
	if (!multi_save_transfer_begin_restore(Rewind_save_transfer.recovery_epoch, Rewind_save_transfer.world_visit)) {
		multi_rewind_receive_reset();
		return;
	}
	multi_rewind_send_ready(MULTI_SAVE_TRANSFER_READY_APPLY);
	multi_prepare_restore_sync();
	status = android_rewind_restore_authoritative(&restore);
	multi_save_transfer_finish_restore(status == ANDROID_REWIND_STATUS_RESTORED);
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
	coop_transition_policy travel;

	if (!data || total_size == 0 ||
	    total_size > MULTI_SAVE_TRANSFER_MAX_BYTES)
		return 0;
	if (transfer_kind != MULTI_SAVE_TRANSFER_KIND_REWIND &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_RESTORE &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_WORLD &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_FRESH_WORLD &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_CAMPAIGN &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_CHECKPOINT &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_ROLLBACK)
		return 0;
	total_chunks = (int) ((total_size + MULTI_REWIND_SAVE_CHUNK_PAYLOAD - 1) /
	                      MULTI_REWIND_SAVE_CHUNK_PAYLOAD);
	if (total_chunks <= 0 || total_chunks > 65535)
		return 0;
	if (Save_send_transfer.active || restore_barrier_busy()) {
		COOPLOG("save transfer refused: another send is active");
		return 0;
	}
	data_copy = (unsigned char *) d_malloc((unsigned int) total_size);
	if (!data_copy)
		return 0;
	memcpy(data_copy, data, total_size);
	uint64_t visit = transfer_kind == MULTI_SAVE_TRANSFER_KIND_CAMPAIGN || transfer_kind == MULTI_SAVE_TRANSFER_KIND_CHECKPOINT
	                     ? coop_world_visit_current()
	                     : coop_world_visit_reserve();
	if (!visit) {
		d_free(data_copy);
		COOPLOG("save transfer refused: no world visit available");
		return 0;
	}
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
	Save_send_transfer.recovery_epoch = coop_recovery_epoch() + 1;
	Save_send_transfer.world_visit = visit;
	if (!Save_send_transfer.recovery_epoch) Save_send_transfer.recovery_epoch = 1;
	Save_send_transfer.game_time64 = game_time64;
	Save_send_transfer.has_collision_delay_last_play_time =
	    has_collision_delay_last_play_time;
	Save_send_transfer.collision_delay_last_play_time =
	    collision_delay_last_play_time;
	Save_send_transfer.rewound_seconds = rewound_seconds;
	Save_send_transfer.total_chunks = total_chunks;
	Save_send_transfer.started_ms = Save_send_transfer.progress_ms = multi_save_transfer_now_ms();
	Save_send_transfer.data = data_copy;
	coop_travel_get_state(&travel, NULL, NULL, NULL);
	for (i = 0; i < MAX_PLAYERS; ++i) {
		Save_send_transfer.required_players[i] =
		    i != Player_num && (transfer_kind == MULTI_SAVE_TRANSFER_KIND_ROLLBACK
		                            ? !!(travel.participants & (1u << i))
		                            : Players[i].connected == CONNECT_PLAYING);
		Save_send_transfer.ready_players[i] =
		    Save_send_transfer.required_players[i] ? 0 : 1;
		Save_send_transfer.applying_players[i] =
		    Save_send_transfer.required_players[i] ? 0 : 1;
		if (Test_rollback_waiting && transfer_kind == MULTI_SAVE_TRANSFER_KIND_ROLLBACK &&
		    Save_send_transfer.required_players[i]) {
			Players[i].connected = Netgame.players[i].connected = CONNECT_WAITING;
			++Test_rollback_waiting_players;
			COOPLOG("travel rollback test waiting participant: player=%d", i);
		}
	}

	if (!restore_barrier_begin(transfer_kind, transfer_id, visit, total_chunks)) {
		multi_save_send_reset();
		return 0;
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

static void multi_save_transfer_send_packet(int len)
{
	if (Save_send_transfer.transfer_kind != MULTI_SAVE_TRANSFER_KIND_ROLLBACK) {
		multi_send_data(multibuf, len, 2);
		return;
	}
	/* A failed level sync can leave a frozen participant waiting */
	for (int i = 0; i < N_players; ++i)
		if (Save_send_transfer.required_players[i]) multi_send_data_direct(multibuf, len, i, 2);
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
	PUT_INTEL_INT(multibuf + 32, Save_send_transfer.recovery_epoch);
	coop_world_visit_write(multibuf + 36, Save_send_transfer.world_visit);
	multi_save_transfer_send_packet(MULTI_REWIND_SAVE_BEGIN_LEN);
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
	multi_save_transfer_send_packet(MULTI_REWIND_SAVE_CHUNK_LEN);
}

static void multi_save_transfer_send_apply(void)
{

	memset(multibuf, 0, MULTI_REWIND_SAVE_APPLY_LEN);
	multibuf[0] = MULTI_REWIND_SAVE_APPLY;
	multibuf[1] = Save_send_transfer.transfer_id;
	multibuf[2] = (ubyte) Save_send_transfer.requester;
	multibuf[3] = (ubyte) multi_rewind_clamp_u8(Save_send_transfer.rewound_seconds);
	multi_save_transfer_send_packet(MULTI_REWIND_SAVE_APPLY_LEN);
}

void multi_save_transfer_frame(void)
{
	restore_barrier_tick();
	if (Restore_barrier.phase == RESTORE_FAILED) return;
	unsigned chunks;
	static uint64_t diagnostic_ms;
	if (Save_send_transfer.active && multi_save_transfer_now_ms() - diagnostic_ms >= 5000) {
		unsigned required = 0, ready = 0, applying = 0, connected = 0;
		for (int p = 0; p < MAX_PLAYERS; ++p) {
			if (Save_send_transfer.required_players[p]) required |= 1u << p;
			if (Save_send_transfer.ready_players[p]) ready |= 1u << p;
			if (Save_send_transfer.applying_players[p]) applying |= 1u << p;
			connected |= (unsigned) (Players[p].connected & 15) << (4 * p);
		}
		diagnostic_ms = multi_save_transfer_now_ms();
		COOPLOG("transfer pump: kind=%d id=%u restoring=%d chunks=%d/%d apply_sent=%d required=%x ready=%x applying=%x connected=%x pending=%u",
		        Save_send_transfer.transfer_kind, Save_send_transfer.transfer_id, Save_transfer_restore_active,
		        Save_send_transfer.next_chunk, Save_send_transfer.total_chunks, Save_send_transfer.apply_sent,
		        required, ready, applying, connected, net_udp_reliable_pending());
	}
	if (Save_transfer_restore_active) return;

	/* A restore tears down and rebuilds the current level.  Packet handlers
	 * only mark it pending so the UDP parser can finish against the state with
	 * which it began.  Apply here, at the next game-frame boundary. */
	multi_rewind_apply_received_transfer(1);

	if (Rewind_save_transfer.active &&
	    (Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE ||
	     Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART ||
	     Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_WORLD ||
	     Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_FRESH_WORLD ||
	     Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_CAMPAIGN ||
	     Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_CHECKPOINT ||
	     Rewind_save_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_ROLLBACK) &&
	    multi_save_transfer_expired(multi_save_transfer_now_ms(), Rewind_save_transfer.started_ms,
	                                Rewind_save_transfer.progress_ms, Rewind_save_transfer.total_chunks)) {
		COOPLOG("coop restore receive timeout: id=%u chunks=%d/%d",
		        Rewind_save_transfer.transfer_id,
		        Rewind_save_transfer.chunks_received,
		        Rewind_save_transfer.total_chunks);
		coop_restore_status_failed();
		if (Rewind_save_transfer.transfer_kind >= MULTI_SAVE_TRANSFER_KIND_WORLD)
			coop_travel_world_failed();
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
	if (multi_save_transfer_expired(multi_save_transfer_now_ms(), Save_send_transfer.started_ms,
	                                Save_send_transfer.progress_ms, Save_send_transfer.total_chunks)) {
		COOPLOG("save transfer send timeout: kind=%d id=%u chunks=%d/%d",
		        Save_send_transfer.transfer_kind,
		        Save_send_transfer.transfer_id,
		        Save_send_transfer.next_chunk,
		        Save_send_transfer.total_chunks);
		if (Save_send_transfer.transfer_kind >= MULTI_SAVE_TRANSFER_KIND_WORLD)
			coop_travel_world_failed();
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
	if (restore_pause_test_hold_source()) return;

	unsigned copies = 1;
	if (Save_send_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_ROLLBACK) {
		copies = 0;
		for (int i = 0; i < N_players; ++i)
			if (Save_send_transfer.required_players[i]) ++copies;
		if (!copies) copies = 1;
	}
	unsigned pending = net_udp_reliable_pending();
	unsigned remaining = (unsigned) (Save_send_transfer.total_chunks - Save_send_transfer.next_chunk);
	unsigned window = multi_save_transfer_window_chunks(pending, copies, remaining);
	if (pending > Save_send_transfer.peak_pending) Save_send_transfer.peak_pending = pending;
	if (remaining && !window) {
		if (!Save_send_transfer.window_waits)
			COOPLOG("save transfer awaiting reliable ACKs: kind=%d id=%u pending=%u copies=%u", Save_send_transfer.transfer_kind,
			        Save_send_transfer.transfer_id, pending, copies);
		++Save_send_transfer.window_waits;
	}
	chunks = multi_save_transfer_chunk_budget(
	    &Save_send_transfer.chunk_budget, multi_save_transfer_now_ms(),
	    window);
	if (chunks) Save_send_transfer.progress_ms = multi_save_transfer_now_ms();
	while (chunks--) {
		multi_save_transfer_send_chunk(Save_send_transfer.next_chunk++);
	}
	if (Save_send_transfer.next_chunk < Save_send_transfer.total_chunks)
		return;

	if (!Save_send_transfer.apply_sent) {
		multi_save_transfer_send_apply();
		Save_send_transfer.apply_sent = 1;
		COOPLOG("save transfer payload sent: kind=%d id=%u requester=%d bytes=%u chunks=%d checksum=%u window_waits=%u peak_pending=%u",
		        Save_send_transfer.transfer_kind,
		        Save_send_transfer.transfer_id,
		        Save_send_transfer.requester,
		        Save_send_transfer.total_size,
		        Save_send_transfer.total_chunks,
		        Save_send_transfer.checksum, Save_send_transfer.window_waits, Save_send_transfer.peak_pending);
		return;
	}

	if (!Save_send_transfer.coop_restore_pending &&
	    !Save_send_transfer.level_restart_pending &&
	    Save_send_transfer.transfer_kind != MULTI_SAVE_TRANSFER_KIND_WORLD &&
	    Save_send_transfer.transfer_kind != MULTI_SAVE_TRANSFER_KIND_FRESH_WORLD &&
	    Save_send_transfer.transfer_kind != MULTI_SAVE_TRANSFER_KIND_CAMPAIGN &&
	    Save_send_transfer.transfer_kind != MULTI_SAVE_TRANSFER_KIND_CHECKPOINT &&
	    Save_send_transfer.transfer_kind != MULTI_SAVE_TRANSFER_KIND_ROLLBACK &&
	    Save_send_transfer.transfer_kind != MULTI_SAVE_TRANSFER_KIND_REWIND) {
		multi_save_send_reset();
		return;
	}
	if (!multi_save_transfer_all_players_applying())
		return;
	if (restore_pause_test_hold_host_apply()) {
		/* The fixture delays local application after complete transfer; let the
		 * peer's real loader deadline expire before the artificial hold ends */
		Save_send_transfer.progress_ms = multi_save_transfer_now_ms();
		/* Match local-load liveness grace while this fixture waits outside the reader */
		Save_transfer_timeout_grace_until = timer_query() + F1_0 * 5;
		return;
	}
	/* Automation exercises a peer requesting destination sync while the host
	 * still owns a destroyed source. Continue networking throughout the hold */
	if (Test_delay_world_apply && (Save_send_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_WORLD ||
	                               Save_send_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_FRESH_WORLD)) {
		if (!Test_world_apply_after) {
			Test_world_apply_after = multi_save_transfer_now_ms() + 5000;
			COOPLOG("test delaying host world apply: kind=%d id=%u", Save_send_transfer.transfer_kind, Save_send_transfer.transfer_id);
		}
		if (multi_save_transfer_now_ms() < Test_world_apply_after) return;
		Test_delay_world_apply = 0;
	}
	if (Save_send_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_CAMPAIGN ||
	    Save_send_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_CHECKPOINT) {
		int staged = Save_send_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_CHECKPOINT
		                 ? coop_travel_stage_checkpoint(Save_send_transfer.data, Save_send_transfer.total_size)
		                 : coop_travel_stage_campaign(Save_send_transfer.data, Save_send_transfer.total_size);
		multi_save_send_reset();
		if (!staged) coop_travel_world_failed();
		return;
	}

	if (Save_send_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_REWIND) {
		android_rewind_authoritative_restore restore;
		int status;

		memset(&restore, 0, sizeof(restore));
		restore.buffer.data = Save_send_transfer.data;
		restore.buffer.size = Save_send_transfer.total_size;
		restore.buffer.capacity = Save_send_transfer.total_size;
		restore.snapshot_index = Save_send_transfer.rewind_snapshot_index;
		restore.rewound_seconds = Save_send_transfer.rewound_seconds;
		restore.game_time64 = Save_send_transfer.game_time64;
		restore.has_collision_delay_last_play_time =
		    Save_send_transfer.has_collision_delay_last_play_time;
		restore.collision_delay_last_play_time =
		    Save_send_transfer.collision_delay_last_play_time;
		Save_send_transfer.data = NULL;
		if (!multi_save_transfer_begin_restore(Save_send_transfer.recovery_epoch, Save_send_transfer.world_visit)) {
			multi_save_send_reset();
			d_free(restore.buffer.data);
			return;
		}
		multi_save_send_reset();
		multi_prepare_restore_sync();
		status = android_rewind_restore_authoritative(&restore);
		multi_save_transfer_finish_restore(status == ANDROID_REWIND_STATUS_RESTORED);
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

		if (!multi_save_transfer_begin_restore(Save_send_transfer.recovery_epoch, Save_send_transfer.world_visit)) {
			multi_save_send_reset();
			coop_level_restart_transfer_finished(0);
			return;
		}
		multi_save_send_reset();
		restored = coop_level_restart_apply_host();
		multi_save_transfer_finish_restore(restored);
		coop_level_restart_transfer_finished(restored);
		HUD_init_message_literal(HM_DEFAULT,
		                         restored ? "Level restarted" : "Level restart failed");
		return;
	}

	{
		rewind_memory_buffer buffer = { 0 };
		int transfer_kind = Save_send_transfer.transfer_kind;
		int restored;

		COOPLOG("coop restore transfer synchronized: id=%u slot=%u game_id=%u",
		        Save_send_transfer.transfer_id,
		        (uint) Save_send_transfer.coop_restore_slot,
		        Save_send_transfer.coop_restore_game_id);
		/* Apply exactly the snapshot sent to peers, even if its disk slot changes */
		buffer.data = Save_send_transfer.data;
		buffer.size = Save_send_transfer.total_size;
		buffer.capacity = Save_send_transfer.total_size;
		Save_send_transfer.data = NULL;
		if (!multi_save_transfer_begin_restore(Save_send_transfer.recovery_epoch, Save_send_transfer.world_visit)) {
			multi_save_send_reset();
			d_free(buffer.data);
			coop_restore_status_failed();
			coop_travel_world_failed();
			return;
		}
		multi_save_send_reset();
		multi_prepare_restore_sync();
		restored = multi_apply_world_transfer(&buffer, transfer_kind);
		multi_save_transfer_finish_restore(restored);
		d_free(buffer.data);
		if (restored) {
			coop_restore_status_complete();
			multi_send_score();
		} else {
			coop_restore_status_failed();
			HUD_init_message_literal(HM_DEFAULT, "Host save failed");
		}
		COOPLOG("coop restore transfer host apply: status=%d bytes=%u",
		        restored, (uint) buffer.size);
	}
}

static int multi_send_rewind_save_transfer(
    const android_rewind_authoritative_restore *restore, int requester)
{
	int sent = multi_send_save_transfer_buffer(
	    restore->buffer.data, restore->buffer.size,
	    MULTI_SAVE_TRANSFER_KIND_REWIND, requester,
	    restore->rewound_seconds, restore->game_time64,
	    restore->has_collision_delay_last_play_time,
	    restore->collision_delay_last_play_time);
	/* The host owns this history; captures stay blocked until the shared release */
	if (sent) Save_send_transfer.rewind_snapshot_index = restore->snapshot_index;
	return sent;
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
	return Save_send_transfer.active || Rewind_save_transfer.active ||
	       Save_transfer_restore_active || restore_barrier_busy();
}

int multi_send_coop_world_restore_transfer(const rewind_memory_buffer *buffer)
{
#ifdef DXX_BUILD_DESCENT_II
	if (!buffer || buffer->error || !(Game_mode & GM_MULTI_COOP) ||
	    !multi_i_am_master() || multi_save_transfer_busy() || coop_briefing_active() ||
	    !coop_travel_world_apply_allowed()) return 0;
	if (!multi_rewind_has_connected_clients()) {
		int restored;
		if (!multi_save_transfer_begin_restore(0, 0)) return 0;
		restored = multi_apply_world_transfer(buffer, MULTI_SAVE_TRANSFER_KIND_WORLD);
		multi_save_transfer_finish_restore(restored);
		return restored;
	}
	if (!Netgame.PacketLossPrevention) return 0;
	return multi_send_save_transfer_buffer(buffer->data, buffer->size,
	                                       MULTI_SAVE_TRANSFER_KIND_WORLD,
	                                       Player_num, 0, GameTime64, 0, 0);
#else
	(void) buffer;
	return 0;
#endif
}

int multi_send_coop_campaign_transfer(const rewind_memory_buffer *buffer)
{
	if (!buffer || buffer->error || !(Game_mode & GM_MULTI_COOP) || !multi_i_am_master() ||
	    multi_save_transfer_busy() || !coop_travel_campaign_stage_allowed()) return 0;
	if (!multi_rewind_has_connected_clients()) return coop_travel_stage_campaign(buffer->data, buffer->size);
	if (!Netgame.PacketLossPrevention) return 0;
	return multi_send_save_transfer_buffer(buffer->data, buffer->size, MULTI_SAVE_TRANSFER_KIND_CAMPAIGN,
	                                       Player_num, 0, GameTime64, 0, 0);
}

int multi_send_coop_checkpoint_transfer(const rewind_memory_buffer *buffer)
{
	if (!buffer || buffer->error || !(Game_mode & GM_MULTI_COOP) || !multi_i_am_master() ||
	    multi_save_transfer_busy() || !coop_travel_campaign_stage_allowed()) return 0;
	if (!multi_rewind_has_connected_clients()) return coop_travel_stage_checkpoint(buffer->data, buffer->size);
	if (!Netgame.PacketLossPrevention) return 0;
	return multi_send_save_transfer_buffer(buffer->data, buffer->size, MULTI_SAVE_TRANSFER_KIND_CHECKPOINT,
	                                       Player_num, 0, GameTime64, 0, 0);
}

int multi_send_coop_rollback_transfer(const rewind_memory_buffer *buffer)
{
	coop_transition_policy travel;
	if (!buffer || buffer->error || !(Game_mode & GM_MULTI_COOP) || !multi_i_am_master() ||
	    multi_save_transfer_busy() || !coop_travel_rollback_allowed()) return 0;
	coop_travel_get_state(&travel, NULL, NULL, NULL);
	if (travel.participants == (1u << Player_num)) {
		int result;
		if (!multi_save_transfer_begin_restore(0, 0)) return 0;
		result = multi_apply_world_transfer(buffer, MULTI_SAVE_TRANSFER_KIND_ROLLBACK);
		multi_save_transfer_finish_restore(result);
		return result;
	}
	if (!Netgame.PacketLossPrevention) return 0;
	return multi_send_save_transfer_buffer(buffer->data, buffer->size, MULTI_SAVE_TRANSFER_KIND_ROLLBACK,
	                                       Player_num, 0, GameTime64, 0, 0);
}

int multi_send_coop_world_initialize_transfer(int level)
{
#ifdef DXX_BUILD_DESCENT_II
	unsigned char data[4];
	rewind_memory_buffer buffer = { 0 };
	if (!level || level < -127 || level > 127 || !coop_travel_fresh_destination_allowed(level) || !(Game_mode & GM_MULTI_COOP) ||
	    !Netgame.AllowSecretWarps || !multi_i_am_master() ||
	    multi_save_transfer_busy() || coop_briefing_active() || !coop_travel_world_apply_allowed()) return 0;
	PUT_INTEL_INT(data, level);
	if (!multi_rewind_has_connected_clients()) {
		int result;
		buffer.data = data;
		buffer.size = buffer.capacity = sizeof(data);
		if (!multi_save_transfer_begin_restore(0, 0)) return 0;
		result = multi_apply_world_transfer(&buffer, MULTI_SAVE_TRANSFER_KIND_FRESH_WORLD);
		multi_save_transfer_finish_restore(result);
		return result;
	}
	if (!Netgame.PacketLossPrevention) return 0;
	return multi_send_save_transfer_buffer(data, sizeof(data), MULTI_SAVE_TRANSFER_KIND_FRESH_WORLD,
	                                       Player_num, 0, GameTime64, 0, 0);
#else
	(void) level;
	return 0;
#endif
}

int multi_send_level_restart_transfer(const rewind_memory_buffer *buffer)
{
	int sent;

	if (!buffer || !buffer->data || !buffer->size ||
	    !(Game_mode & GM_MULTI_COOP) || !multi_i_am_master())
		return 0;
	if (!multi_rewind_has_connected_clients()) {
		int restored;

		if (!multi_save_transfer_begin_restore(0, 0)) return 0;
		restored = coop_level_restart_apply_host();
		multi_save_transfer_finish_restore(restored);
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
		if (!multi_save_transfer_begin_restore(0, 0)) return ANDROID_REWIND_STATUS_FAILED;
		status = android_rewind_restore_authoritative(&restore);
		multi_save_transfer_finish_restore(status == ANDROID_REWIND_STATUS_RESTORED);
	}
	if (rewound_seconds)
		*rewound_seconds = restore.rewound_seconds;
	return status;
}

void multi_do_rewind_request(const ubyte *buf, int authenticated_sender)
{
	static fix64 next_request_time[MAX_PLAYERS] = { 0 };
	int requester = buf[1];
	int rewound_seconds = 0;
	int status;
	int requester_valid = multi_rewind_requester_valid(requester);
	COOPLOG("client rewind received: player=%d sender=%d valid=%d enabled=%d", requester,
	        authenticated_sender, requester_valid, android_rewind_clients_can_request());
	if (requester != authenticated_sender) return;

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
	/* Simulation time moves backwards when the request succeeds */
	fix64 now = timer_query();
	if (now < next_request_time[requester]) {
		multi_send_rewind_result(requester, ANDROID_REWIND_STATUS_BLOCKED_MULTIPLAYER, 0);
		return;
	}
	next_request_time[requester] = now + F1_0;
	COOPLOG("client rewind accepted: player=%d real_time=%lld game_time=%lld", requester,
	        (long long) now, (long long) GameTime64);
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

	if (coop_briefing_active() || multi_i_am_master() || !(Game_mode & GM_MULTI_COOP))
		return;
	total_size = GET_INTEL_INT(buf + 4);
	checksum = GET_INTEL_INT(buf + 8);
	transfer_kind = buf[29];
	if (!coop_travel_transfer_buffer_allowed(transfer_kind)) return;
	total_chunks = GET_INTEL_SHORT(buf + 30);
	if (!GET_INTEL_INT(buf + 32) || total_size == 0 || total_size > MULTI_SAVE_TRANSFER_MAX_BYTES ||
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
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_WORLD &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_FRESH_WORLD &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_CAMPAIGN &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_CHECKPOINT &&
	    transfer_kind != MULTI_SAVE_TRANSFER_KIND_ROLLBACK) {
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
	uint64_t visit = coop_world_visit_read(buf + 36);
	int stages_only = transfer_kind == MULTI_SAVE_TRANSFER_KIND_CAMPAIGN || transfer_kind == MULTI_SAVE_TRANSFER_KIND_CHECKPOINT;
	if (!visit || (stages_only ? visit != coop_world_visit_current() : visit <= coop_world_visit_current())) {
		COOPLOG("save transfer begin rejected world visit: kind=%d received=%llu active=%llu",
		        transfer_kind, (unsigned long long) visit, (unsigned long long) coop_world_visit_current());
		return;
	}
	coop_world_visit_observe(visit);
	if (restore_barrier_busy()) {
		if (Rewind_save_transfer.active && Rewind_save_transfer.transfer_id == buf[1] && Rewind_save_transfer.world_visit == visit)
			multi_rewind_send_ready(MULTI_SAVE_TRANSFER_READY_BUFFER);
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
	Rewind_save_transfer.recovery_epoch = (uint32_t) GET_INTEL_INT(buf + 32);
	Rewind_save_transfer.world_visit = visit;
	Rewind_save_transfer.game_time64 = multi_rewind_get_i64(buf + 12);
	Rewind_save_transfer.collision_delay_last_play_time =
	    multi_rewind_get_i64(buf + 20);
	Rewind_save_transfer.has_collision_delay_last_play_time = buf[28] ? 1 : 0;
	Rewind_save_transfer.total_chunks = total_chunks;
	Rewind_save_transfer.started_ms = Rewind_save_transfer.progress_ms = multi_save_transfer_now_ms();
	HUD_init_message_literal(HM_DEFAULT,
	                         transfer_kind == MULTI_SAVE_TRANSFER_KIND_RESTORE
	                             ? "Receiving host save"
	                         : transfer_kind == MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART
	                             ? "Receiving level restart"
	                             : "Receiving host rewind");
	if (!restore_barrier_begin(transfer_kind, buf[1], visit, total_chunks)) {
		multi_rewind_receive_reset();
		return;
	}
	multi_rewind_send_ready(MULTI_SAVE_TRANSFER_READY_BUFFER);
	COOPLOG("save transfer begin: kind=%d id=%u requester=%d bytes=%u chunks=%d checksum=%u",
	        transfer_kind, Rewind_save_transfer.transfer_id,
	        Rewind_save_transfer.requester, total_size, total_chunks, checksum);
}

void multi_do_rewind_save_ready(const ubyte *buf, int authenticated_sender)
{
	int pnum = buf[2];
	int phase = buf[3];
	if (pnum != authenticated_sender || pnum >= N_players) return;
	if (phase >= RESTORE_LOADED) {
		restore_barrier_receive(buf, authenticated_sender);
		return;
	}
	COOPLOG("transfer READY received: player=%d phase=%d id=%u host=%d active=%d expected_id=%u connected=%d required=%d apply_sent=%d",
	        pnum, phase, buf[1], multi_i_am_master(), Save_send_transfer.active, Save_send_transfer.transfer_id,
	        pnum < N_players ? Players[pnum].connected : -1,
	        pnum < MAX_PLAYERS ? Save_send_transfer.required_players[pnum] : 0, Save_send_transfer.apply_sent);

	if (!multi_i_am_master() || !Save_send_transfer.active ||
	    buf[1] != Save_send_transfer.transfer_id ||
	    coop_world_visit_read(buf + 4) != Save_send_transfer.world_visit ||
	    pnum < 0 || pnum >= N_players || pnum == Player_num ||
	    (Players[pnum].connected != CONNECT_PLAYING &&
	     !(Save_send_transfer.transfer_kind == MULTI_SAVE_TRANSFER_KIND_ROLLBACK && Players[pnum].connected == CONNECT_WAITING)) ||
	    !Save_send_transfer.required_players[pnum])
		return;
	if (phase == MULTI_SAVE_TRANSFER_READY_BUFFER) {
		if (!Save_send_transfer.ready_players[pnum]) Save_send_transfer.progress_ms = multi_save_transfer_now_ms();
		Save_send_transfer.ready_players[pnum] = 1;
		COOPLOG("save transfer buffer ready: id=%u player=%d",
		        Save_send_transfer.transfer_id, pnum);
	} else if (phase == MULTI_SAVE_TRANSFER_READY_APPLY &&
	           Save_send_transfer.apply_sent &&
	           Save_send_transfer.ready_players[pnum]) {
		if (!Save_send_transfer.applying_players[pnum]) Save_send_transfer.progress_ms = multi_save_transfer_now_ms();
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
		Rewind_save_transfer.progress_ms = multi_save_transfer_now_ms();
	}
	multi_rewind_apply_received_transfer(0);
}

void multi_do_rewind_save_apply(const ubyte *buf)
{
	if (multi_i_am_master() || !Rewind_save_transfer.active ||
	    buf[1] != Rewind_save_transfer.transfer_id)
		return;
	Rewind_save_transfer.apply_pending = 1;
	Rewind_save_transfer.rewound_seconds = buf[3];
	multi_rewind_apply_received_transfer(0);
}

#endif /* __ANDROID__ */
