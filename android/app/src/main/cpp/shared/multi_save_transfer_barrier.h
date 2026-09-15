/* Shared D1/D2 restore coordination; included by multi_save_transfer.c */
#ifndef MULTI_SAVE_TRANSFER_BARRIER_H
#define MULTI_SAVE_TRANSFER_BARRIER_H

enum { RESTORE_LOADING = 1,
	   RESTORE_RELEASING,
	   RESTORE_RUNNING,
	   RESTORE_DONE,
	   RESTORE_FAILED };
enum { RESTORE_LOADED = 3,
	   RESTORE_RELEASE,
	   RESTORE_RELEASE_ACK,
	   RESTORE_RUN,
	   RESTORE_RUN_ACK,
	   RESTORE_ERROR };
static struct {
	uint64_t visit, deadline, next_send;
	unsigned required, loaded, acknowledged, running;
	int host, id, phase, pause_owned, local_loaded;
} Restore_barrier;
static int Restore_barrier_pumping;
/* Survive session teardown until the stable main menu can explain the failure */
static const char *Restore_failure_reason;

#include "multi_save_transfer_barrier_test.h"

const char *multi_save_transfer_barrier_status(int *local_loaded, uint64_t *visit)
{
	static const char *const names[] = { "idle", "loading", "releasing", "running", "done", "failed" };
	*local_loaded = Restore_barrier.local_loaded;
	*visit = Restore_barrier.visit;
	return names[Restore_barrier.phase];
}

static int restore_barrier_kind(int kind)
{
	return kind == MULTI_SAVE_TRANSFER_KIND_RESTORE || kind == MULTI_SAVE_TRANSFER_KIND_REWIND ||
	       kind == MULTI_SAVE_TRANSFER_KIND_LEVEL_RESTART;
}

int multi_save_transfer_paused(void)
{
	return Restore_barrier.phase == RESTORE_LOADING || Restore_barrier.phase == RESTORE_RELEASING ||
	       Restore_barrier.phase == RESTORE_FAILED;
}

static int restore_barrier_busy(void)
{
	return Restore_barrier.phase && Restore_barrier.phase != RESTORE_DONE;
}

static int restore_barrier_waiting_peer(const ubyte *data, int len, int pnum)
{
	return restore_barrier_busy() && (Restore_barrier.required & (1u << pnum)) &&
	       data[0] == MULTI_REWIND_SAVE_READY && len == MULTI_REWIND_SAVE_READY_LEN &&
	       data[1] == Restore_barrier.id && data[2] == Player_num &&
	       coop_world_visit_read(data + 4) == Restore_barrier.visit;
}

static void restore_barrier_unpause(void)
{
	if (Restore_barrier.pause_owned) {
		restore_pause_test_release();
		Restore_barrier.pause_owned = 0;
		game_flush_inputs();
		start_time();
		reset_time();
	}
}

static void restore_barrier_packet(int phase, int target)
{
	ubyte packet[MULTI_REWIND_SAVE_READY_LEN] = { 0 };
	packet[0] = MULTI_REWIND_SAVE_READY;
	packet[1] = (ubyte) Restore_barrier.id;
	packet[2] = (ubyte) Player_num;
	packet[3] = (ubyte) phase;
	coop_world_visit_write(packet + 4, Restore_barrier.visit);
	multi_send_data_direct(packet, sizeof(packet), target, 2);
}

static void restore_barrier_broadcast(int phase)
{
	for (int i = 0; i < N_players; ++i)
		if (i != Player_num && (Restore_barrier.required & (1u << i)) && Players[i].connected != CONNECT_DISCONNECTED)
			restore_barrier_packet(phase, i);
}

int multi_save_transfer_show_failure(void)
{
	if (Game_wind || !Restore_failure_reason) return 0;
	const char *reason = Restore_failure_reason;
	Restore_failure_reason = NULL;
	game_flush_inputs();
	nm_messagebox("Co-op restore interrupted", 1, TXT_OK,
	              "%s\n\nThe mine has been closed\nHost or join a saved co-op game to continue", reason);
	return 1;
}

static void restore_barrier_fail(const char *reason)
{
	if (!Restore_barrier.phase || Restore_barrier.phase == RESTORE_FAILED) return;
	Restore_failure_reason = reason;
	restore_pause_test_check();
	debug_log_force(DLOG_COOP_DESYNC, "restore barrier failed: visit=%llu phase=%d local_loaded=%d paused=%d clock_drift=%d",
	                (unsigned long long) Restore_barrier.visit, Restore_barrier.phase, Restore_barrier.local_loaded,
	                game_is_time_paused(), Restore_pause_test.failed);
	if (Player_num == Restore_barrier.host) restore_barrier_broadcast(RESTORE_ERROR);
	else restore_barrier_packet(RESTORE_ERROR, Restore_barrier.host);
	Restore_barrier.phase = RESTORE_FAILED;
	if (!Restore_barrier.pause_owned) {
		stop_time();
		Restore_barrier.pause_owned = 1;
	}
	multi_quit_game = 1;
}

int multi_save_transfer_host_disconnected(int player)
{
	if (!restore_barrier_busy() || player != Restore_barrier.host) return 0;
	/* Do not publish a replacement host for an unfinished restored world */
	restore_barrier_fail("The host disconnected during restore");
	return 1;
}

/* The synchronous level loader cannot run the normal transfer frame. Check
 * cancellation here without applying another save or releasing gameplay */
int multi_save_transfer_sync_poll(int sync_failed)
{
	if (!Save_transfer_restore_active || !restore_barrier_busy()) return 0;
	restore_pause_test_sync_wait();
	if (sync_failed) restore_barrier_fail("This player could not load the saved mine");
	else if (multi_save_transfer_now_ms() >= Restore_barrier.deadline) {
		if (Restore_barrier.phase != RESTORE_FAILED)
			debug_log_force(DLOG_COOP_DESYNC, "restore synchronous loader deadline expired: visit=%llu local_loaded=%d paused=%d",
			                (unsigned long long) Restore_barrier.visit, Restore_barrier.local_loaded, game_is_time_paused());
		restore_barrier_fail("A player did not finish restoring in time");
	} else if (multi_quit_game)
		restore_barrier_fail("The team changed during restore");
	return Restore_barrier.phase == RESTORE_FAILED;
}

static int restore_barrier_begin(int kind, int id, uint64_t visit, unsigned chunks)
{
	if (!restore_barrier_kind(kind)) return 1;
	if (restore_barrier_busy()) return 0;
	memset(&Restore_barrier, 0, sizeof(Restore_barrier));
	Restore_barrier.visit = visit;
	Restore_barrier.id = id;
	Restore_barrier.host = multi_who_is_master();
	Restore_barrier.phase = RESTORE_LOADING;
	Restore_barrier.deadline = multi_save_transfer_now_ms() + multi_save_transfer_limit_ms(chunks) + 60000u;
	for (int i = 0; i < N_players; ++i)
		if (Players[i].connected == CONNECT_PLAYING) Restore_barrier.required |= 1u << i;
	Restore_barrier.required |= (1u << Player_num) | (1u << Restore_barrier.host);
	stop_time();
	Restore_barrier.pause_owned = 1;
	game_flush_inputs();
	restore_pause_test_begin();
	COOPLOG("restore barrier freeze: visit=%llu host=%d participants=%x", (unsigned long long) visit,
	        Restore_barrier.host, Restore_barrier.required);
	return 1;
}

static void restore_barrier_local_done(int restored)
{
	if (Restore_barrier.phase != RESTORE_LOADING) return;
	if (!restored) {
		restore_barrier_fail("This player could not load the saved mine");
		return;
	}
	Restore_barrier.local_loaded = 1;
	if (Restore_pause_test.armed) restore_pause_test_capture(2);
	Restore_barrier.loaded |= 1u << Player_num;
	Restore_barrier.next_send = 0;
	Restore_barrier.deadline = multi_save_transfer_now_ms() + 60000u;
	COOPLOG("restore barrier loaded: visit=%llu player=%d", (unsigned long long) Restore_barrier.visit, Player_num);
}

static void restore_barrier_receive(const ubyte *buf, int sender)
{
	if (!Restore_barrier.phase || buf[1] != Restore_barrier.id ||
	    coop_world_visit_read(buf + 4) != Restore_barrier.visit || sender != buf[2] ||
	    sender < 0 || sender >= N_players || !(Restore_barrier.required & (1u << sender))) return;
	int phase = buf[3];
	if ((Player_num == Restore_barrier.host ? sender != Player_num : sender == Restore_barrier.host) &&
	    restore_pause_test_drop(phase)) return;
	if (phase == RESTORE_ERROR) {
		if (Player_num == Restore_barrier.host || sender == Restore_barrier.host)
			restore_barrier_fail("Another player could not finish restoring");
		return;
	}
	if (Player_num == Restore_barrier.host) {
		if (sender == Player_num) return;
		if (phase == RESTORE_LOADED && Restore_barrier.phase == RESTORE_LOADING)
			Restore_barrier.loaded |= 1u << sender;
		else if (phase == RESTORE_RELEASE_ACK && Restore_barrier.phase == RESTORE_RELEASING)
			Restore_barrier.acknowledged |= 1u << sender;
		else if (phase == RESTORE_RUN_ACK && Restore_barrier.phase == RESTORE_RUNNING)
			Restore_barrier.running |= 1u << sender;
	} else if (sender == Restore_barrier.host && Restore_barrier.local_loaded) {
		if (phase == RESTORE_RELEASE && (Restore_barrier.phase == RESTORE_LOADING || Restore_barrier.phase == RESTORE_RELEASING)) {
			Restore_barrier.phase = RESTORE_RELEASING;
			restore_barrier_packet(RESTORE_RELEASE_ACK, sender);
		} else if (phase == RESTORE_RUN && (Restore_barrier.phase == RESTORE_RELEASING || Restore_barrier.phase == RESTORE_DONE)) {
			Restore_barrier.phase = RESTORE_DONE;
			restore_barrier_packet(RESTORE_RUN_ACK, sender);
			restore_barrier_unpause();
		}
	}
}

static void restore_barrier_tick(void)
{
	if (!restore_barrier_busy() || Save_transfer_restore_active) return;
	uint64_t now = multi_save_transfer_now_ms();
	if (!(Game_mode & GM_MULTI_COOP) || multi_who_is_master() != Restore_barrier.host || multi_quit_game ||
	    now >= Restore_barrier.deadline) {
		restore_barrier_fail(now >= Restore_barrier.deadline ? "A player did not finish restoring in time" : "The team changed during restore");
		return;
	}
	for (int i = 0; i < N_players; ++i)
		if ((Restore_barrier.required & (1u << i)) && Players[i].connected == CONNECT_DISCONNECTED) {
			restore_barrier_fail("A player disconnected during restore");
			return;
		}
	if (restore_pause_test_hold_loaded()) return;
	if (Player_num == Restore_barrier.host) {
		if (Restore_barrier.phase == RESTORE_LOADING && Restore_barrier.loaded == Restore_barrier.required) {
			Restore_barrier.phase = RESTORE_RELEASING;
			Restore_barrier.acknowledged = 1u << Player_num;
			Restore_barrier.next_send = 0;
		}
		if (Restore_barrier.phase == RESTORE_RELEASING && Restore_barrier.acknowledged == Restore_barrier.required) {
			Restore_barrier.phase = RESTORE_RUNNING;
			Restore_barrier.running = 1u << Player_num;
			Restore_barrier.next_send = 0;
		}
		if (Restore_barrier.phase == RESTORE_RUNNING && Restore_barrier.running == Restore_barrier.required) {
			Restore_barrier.phase = RESTORE_DONE;
			restore_barrier_unpause();
			COOPLOG("restore barrier complete: visit=%llu", (unsigned long long) Restore_barrier.visit);
			return;
		}
	}
	if (now < Restore_barrier.next_send) return;
	Restore_barrier.next_send = now + 500;
	if (Player_num != Restore_barrier.host) {
		if (Restore_barrier.local_loaded)
			restore_barrier_packet(Restore_barrier.phase == RESTORE_LOADING ? RESTORE_LOADED : RESTORE_RELEASE_ACK, Restore_barrier.host);
	} else if (Restore_barrier.phase == RESTORE_RELEASING) restore_barrier_broadcast(RESTORE_RELEASE);
	else if (Restore_barrier.phase == RESTORE_RUNNING) {
		restore_barrier_broadcast(RESTORE_RUN);
		restore_barrier_unpause();
	}
}

#endif
