/* Android automation: measure both frozen worlds across deliberately slow restore */
#ifndef MULTI_SAVE_TRANSFER_BARRIER_TEST_H
#define MULTI_SAVE_TRANSFER_BARRIER_TEST_H

static struct {
	int armed, stage, failed, sync_stall;
	unsigned verified;
	unsigned drop_mask, dropped, received[RESTORE_ERROR + 1];
	uint64_t started, source_ms, loaded_ms, waiting_since, waiting_ms;
	uint64_t host_apply_after;
	unsigned loaded_hold_ms;
	fix countdown;
	fix64 game_time;
} Restore_pause_test;

void multi_save_transfer_test_pause_arm(void)
{
	memset(&Restore_pause_test, 0, sizeof(Restore_pause_test));
	Restore_pause_test.armed = 1;
	Restore_pause_test.loaded_hold_ms = multi_i_am_master() ? 3000 : 20000;
	Restore_pause_test.drop_mask = multi_i_am_master()
	                                   ? (1u << RESTORE_LOADED) | (1u << RESTORE_RELEASE_ACK) | (1u << RESTORE_RUN_ACK)
	                                   : (1u << RESTORE_RELEASE) | (1u << RESTORE_RUN);
}

void multi_save_transfer_test_loss_arm(void)
{
	multi_save_transfer_test_pause_arm();
	Restore_pause_test.loaded_hold_ms = 60000;
	Restore_pause_test.drop_mask = 0;
}

void multi_save_transfer_test_sync_stall_arm(void)
{
	multi_save_transfer_test_loss_arm();
	Restore_pause_test.sync_stall = 1;
}

int multi_save_transfer_test_pause_verified(void)
{
	debug_log_force(DLOG_COOP_DESYNC, "restore pause/retry result: player=%d stages=%u failed=%d source_ms=%llu loaded_ms=%llu peer_wait_ms=%llu drops=%x/%x",
	                Player_num, Restore_pause_test.verified, Restore_pause_test.failed,
	                (unsigned long long) Restore_pause_test.source_ms, (unsigned long long) Restore_pause_test.loaded_ms,
	                (unsigned long long) Restore_pause_test.waiting_ms, Restore_pause_test.dropped, Restore_pause_test.drop_mask);
	if (Restore_pause_test.verified != 3 || Restore_pause_test.failed || Restore_pause_test.stage ||
	    !Restore_pause_test.drop_mask || Restore_pause_test.dropped != Restore_pause_test.drop_mask ||
	    (multi_i_am_master() && Restore_pause_test.waiting_ms < 5000)) return 0;
	for (int phase = RESTORE_LOADED; phase <= RESTORE_RUN_ACK; ++phase)
		if ((Restore_pause_test.drop_mask & (1u << phase)) && Restore_pause_test.received[phase] < 2) return 0;
	return 1;
}

static void restore_pause_test_check(void)
{
	if (!Restore_pause_test.stage) return;
	if (!game_is_time_paused() || Countdown_timer != Restore_pause_test.countdown ||
	    GameTime64 != Restore_pause_test.game_time) {
		COOPLOG("restore pause test drift: stage=%d paused=%d countdown=%d/%d game=%lld/%lld",
		        Restore_pause_test.stage, game_is_time_paused(), Countdown_timer, Restore_pause_test.countdown,
		        (long long) GameTime64, (long long) Restore_pause_test.game_time);
		Restore_pause_test.failed = 1;
	}
}

int64_t multi_save_transfer_test_loaded_game_time(void)
{
	return Restore_pause_test.game_time;
}

static void restore_pause_test_capture(int stage)
{
	Restore_pause_test.stage = stage;
	Restore_pause_test.started = multi_save_transfer_now_ms();
	Restore_pause_test.countdown = Countdown_timer;
	Restore_pause_test.game_time = GameTime64;
}

static void restore_pause_test_sync_wait(void)
{
	if (!Restore_pause_test.sync_stall) return;
	if (!Restore_pause_test.stage) {
		restore_pause_test_capture(3);
		debug_log_force(DLOG_COOP_DESYNC, "restore test entered synchronous loader wait: visit=%llu player=%d",
		                (unsigned long long) Restore_barrier.visit, Player_num);
	}
	restore_pause_test_check();
}

static int restore_pause_test_hold_host_apply(void)
{
	if (!Restore_pause_test.sync_stall || !multi_i_am_master()) return 0;
	if (!Restore_pause_test.host_apply_after) {
		Restore_pause_test.host_apply_after = multi_save_transfer_now_ms() + 90000;
		debug_log_force(DLOG_COOP_DESYNC, "restore test holding host apply after peer acknowledgement: visit=%llu",
		                (unsigned long long) Restore_barrier.visit);
	}
	restore_pause_test_check();
	return multi_save_transfer_now_ms() < Restore_pause_test.host_apply_after;
}

static void restore_pause_test_begin(void)
{
	if (!Restore_pause_test.armed) return;
	int count, level, after_count, after_level;
	uint64_t generation, after_generation;
	android_rewind_authoritative_restore selected;
	android_rewind_get_history(&count, &level, &generation);
	android_rewind_maybe_capture_frame();
	int rewind_status = android_rewind_select_restore(&selected);
	android_rewind_get_history(&after_count, &after_level, &after_generation);
	if (rewind_status != ANDROID_REWIND_STATUS_BLOCKED_MULTIPLAYER || selected.buffer.data ||
	    count != after_count || level != after_level || generation != after_generation)
		Restore_pause_test.failed = 1;
	if (multi_i_am_master()) {
		const rewind_memory_buffer *checkpoint = coop_level_restart_buffer();
		rewind_memory_buffer before = { NULL, 0, 0 };
		if (checkpoint) before = *checkpoint;
		uint32_t checksum = before.data ? coop_save_checksum(before.data, before.size, 2166136261u) : 0;
		int state = coop_level_restart_get_state();
		int restart = coop_level_restart_request();
		int retained = coop_level_restart_load_retained_and_request();
		checkpoint = coop_level_restart_buffer();
		if (state != COOP_LEVEL_RESTART_BUSY || restart || retained ||
		    (checkpoint ? checkpoint->data : NULL) != before.data ||
		    (checkpoint ? checkpoint->size : 0) != before.size ||
		    (checkpoint && coop_save_checksum(checkpoint->data, checkpoint->size, 2166136261u) != checksum))
			Restore_pause_test.failed = 1;
	}
	/* Start the near-expired source fixture only once this peer is frozen */
	Countdown_timer = i2f(5);
	Countdown_seconds_left = 5;
	restore_pause_test_capture(1);
}

static int restore_pause_test_hold_source(void)
{
	return Restore_pause_test.stage == 1 && multi_save_transfer_now_ms() < Restore_pause_test.started + 7000;
}

static void restore_pause_test_applying(void)
{
	if (Restore_pause_test.stage != 1) return;
	restore_pause_test_check();
	Restore_pause_test.source_ms = multi_save_transfer_now_ms() - Restore_pause_test.started;
	if (Restore_pause_test.source_ms >= 6000) Restore_pause_test.verified |= 1;
	Restore_pause_test.stage = 0;
}

static int restore_pause_test_hold_loaded(void)
{
	restore_pause_test_check();
	if (Restore_pause_test.stage != 2) return 0;
	uint64_t now = multi_save_transfer_now_ms();
	if (now < Restore_pause_test.started + Restore_pause_test.loaded_hold_ms) return 1;
	if (multi_i_am_master() && Restore_barrier.loaded != Restore_barrier.required) {
		if (!Restore_pause_test.waiting_since) Restore_pause_test.waiting_since = now;
		Restore_pause_test.waiting_ms = now - Restore_pause_test.waiting_since;
	}
	Restore_pause_test.verified |= 2;
	return 0;
}

static void restore_pause_test_release(void)
{
	if (!Restore_pause_test.stage) return;
	restore_pause_test_check();
	if (Restore_pause_test.stage == 2) Restore_pause_test.loaded_ms = multi_save_transfer_now_ms() - Restore_pause_test.started;
	Restore_pause_test.stage = 0;
	Restore_pause_test.armed = 0;
}

/* Drop after authenticated delivery, exercising application retries even when
 * the reliable transport has already acknowledged the datagram */
static int restore_pause_test_drop(int phase)
{
	if (phase < RESTORE_LOADED || phase > RESTORE_RUN_ACK || !(Restore_pause_test.drop_mask & (1u << phase))) return 0;
	++Restore_pause_test.received[phase];
	if (Restore_pause_test.dropped & (1u << phase)) return 0;
	Restore_pause_test.dropped |= 1u << phase;
	COOPLOG("restore pause test discarded delivered phase: player=%d phase=%d", Player_num, phase);
	return 1;
}

#endif
