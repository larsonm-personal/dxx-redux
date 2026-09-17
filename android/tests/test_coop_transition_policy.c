#include "coop_transition_policy.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
		return 0; \
	} \
} while (0)

static int acknowledge_team(coop_transition_policy *p, coop_transition_phase phase, uint64_t now)
{
	unsigned player;
	for (player = 0; player < COOP_TRANSITION_PLAYERS; ++player)
		if (p->participants & (1u << player))
			CHECK(coop_transition_ack(p, p->generation, phase, player, now));
	return 1;
}

static int briefing(coop_transition_policy *p)
{
	CHECK(coop_transition_init(p, 40, 0, 3, 1000));
	CHECK(coop_transition_begin(p, 40, COOP_OP_BRIEFING, 0, 1000));
	CHECK(acknowledge_team(p, COOP_PHASE_BRIEFING_PREPARE, 2000));
	CHECK(coop_transition_seconds_remaining(p) == 120);
	return 1;
}

static int test_normal_exit_race_keeps_other_player_in_mine(void)
{
	coop_transition_policy p;
	CHECK(coop_transition_init(&p, 7, 0, 3, 0));
	CHECK(coop_transition_begin(&p, 7, COOP_OP_NORMAL_EXIT, 1, 50));
	CHECK(p.phase == COOP_PHASE_NORMAL_WAIT);
	CHECK(coop_transition_gameplay_allowed(&p, 0));
	CHECK(coop_transition_gameplay_allowed(&p, 1));
	CHECK(!coop_transition_begin(&p, 7, COOP_OP_SECRET_ENTER, 0, 50));
	CHECK(!coop_transition_begin(&p, p.generation, COOP_OP_SECRET_ENTER, 0, 50));
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_NORMAL_WAIT, 1, 100));
	CHECK(!coop_transition_gameplay_allowed(&p, 1));
	coop_transition_tick(&p, 999999);
	CHECK(coop_transition_gameplay_allowed(&p, 0));
	CHECK(coop_transition_seconds_remaining(&p) == 0);
	CHECK(coop_transition_normal_exit_allowed(&p, 7, 0));
	CHECK(!coop_transition_abort(&p, p.generation));
	CHECK(!coop_transition_begin(&p, p.generation, COOP_OP_REWIND, 0, 999999));
	/* Remaining player's normal escape or countdown death resolves the wait */
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_NORMAL_WAIT, 0, 999999));
	CHECK(p.phase == COOP_PHASE_LOADING);
	CHECK(!coop_transition_gameplay_allowed(&p, 0));
	CHECK(acknowledge_team(&p, COOP_PHASE_LOADING, 1000000));
	CHECK(p.phase == COOP_PHASE_COMMITTED);
	CHECK(!coop_transition_abort(&p, p.generation));
	CHECK(acknowledge_team(&p, COOP_PHASE_COMMITTED, 1000001));
	CHECK(coop_transition_gameplay_allowed(&p, 0));
	return 1;
}

static int test_secret_winner_waits_for_freeze_and_snapshot(void)
{
	coop_transition_policy p;
	CHECK(coop_transition_init(&p, 7, 0, 3, 0));
	CHECK(coop_transition_begin(&p, 7, COOP_OP_SECRET_ENTER, 1, 50));
	CHECK(!coop_transition_normal_exit_allowed(&p, 7, 0));
	CHECK(!coop_transition_source_captured(&p, p.generation, 50));
	CHECK(!coop_transition_gameplay_allowed(&p, 0));
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_FREEZING, 0, 100));
	coop_transition_tick(&p, 200000);
	CHECK(p.phase == COOP_PHASE_FREEZING);
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_FREEZING, 1, 200000));
	CHECK(p.phase == COOP_PHASE_CAPTURING);
	CHECK(coop_transition_source_captured(&p, p.generation, 201000));
	CHECK(coop_transition_seconds_remaining(&p) == 0);
	CHECK(p.phase == COOP_PHASE_LOADING);
	CHECK(!coop_transition_source_captured(&p, p.generation, 201000));
	CHECK(!coop_transition_ack(&p, p.generation, COOP_PHASE_FREEZING, 1, 201000));
	return 1;
}

static int test_briefing_deadlines(void)
{
	coop_transition_policy p;
	CHECK(briefing(&p));
	CHECK(coop_transition_progress(&p, p.generation, 0, 1, 1, 5,
	                               COOP_PRESENTATION_SKIPPED, 12000));
	CHECK(p.phase == COOP_PHASE_BRIEFING);
	CHECK(coop_transition_seconds_remaining(&p) == 20);
	/* A later duplicate, including a new revision, must not extend the wait */
	CHECK(!coop_transition_progress(&p, p.generation, 0, 2, 1, 5,
	                                COOP_PRESENTATION_SKIPPED, 25000));
	coop_transition_tick(&p, 31999);
	CHECK(p.phase == COOP_PHASE_BRIEFING);
	CHECK(coop_transition_seconds_remaining(&p) == 1);
	coop_transition_tick(&p, 32000);
	CHECK(p.phase == COOP_PHASE_CLOSING_PRESENTATION);
	CHECK(p.launch_reason == COOP_LAUNCH_DEADLINE);
	CHECK(briefing(&p));
	CHECK(coop_transition_progress(&p, p.generation, 0, 1, 5, 5,
	                               COOP_PRESENTATION_READY, 117000));
	CHECK(coop_transition_seconds_remaining(&p) == 5);
	coop_transition_tick(&p, 122000);
	CHECK(p.phase == COOP_PHASE_CLOSING_PRESENTATION);
	CHECK(briefing(&p));
	coop_transition_tick(&p, 122000);
	CHECK(p.phase == COOP_PHASE_CLOSING_PRESENTATION);
	CHECK(p.launch_reason == COOP_LAUNCH_DEADLINE);
	return 1;
}

static int test_load_starts_when_every_peer_is_prepared(void)
{
	coop_transition_policy p;
	CHECK(coop_transition_init(&p, 10, 0, 3, 0));
	CHECK(coop_transition_begin(&p, 10, COOP_OP_SECRET_RETURN, 1, 0));
	CHECK(acknowledge_team(&p, COOP_PHASE_FREEZING, 100));
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_CAPTURING, 0, 200));
	coop_transition_tick(&p, 10000);
	CHECK(p.phase == COOP_PHASE_CAPTURING && !p.deadline_ms);
	CHECK(!coop_transition_ack(&p, 10, COOP_PHASE_CAPTURING, 1, 10000));
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_CAPTURING, 1, 10000));
	CHECK(p.phase == COOP_PHASE_LOADING && !p.deadline_ms);
	CHECK(!coop_transition_ack(&p, p.generation, COOP_PHASE_CAPTURING, 0, 11000));
	/* Immediate loading must still wait for destination and release acknowledgments */
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_LOADING, 0, 11000));
	coop_transition_tick(&p, 90000);
	CHECK(p.phase == COOP_PHASE_LOADING);
	CHECK(!coop_transition_gameplay_allowed(&p, 0));
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_LOADING, 1, 90000));
	CHECK(p.phase == COOP_PHASE_COMMITTED);
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_COMMITTED, 0, 90000));
	CHECK(!coop_transition_gameplay_allowed(&p, 0));
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_COMMITTED, 1, 90000));
	CHECK(p.phase == COOP_PHASE_SETTLED);
	return 1;
}

static int test_force_launch_does_not_bypass_loading(void)
{
	coop_transition_policy p;
	CHECK(briefing(&p));
	CHECK(!coop_transition_launch_now(&p, p.generation, 0, 3000));
	CHECK(coop_transition_progress(&p, p.generation, 0, 1, 1, 5,
	                               COOP_PRESENTATION_SKIPPED, 3000));
	CHECK(!coop_transition_launch_now(&p, p.generation, 1, 3000));
	CHECK(!coop_transition_launch_now(&p, 40, 0, 3000));
	CHECK(coop_transition_launch_now(&p, p.generation, 0, 3000));
	CHECK(p.launch_reason == COOP_LAUNCH_HOST);
	CHECK(!coop_transition_launch_now(&p, p.generation, 0, 3000));
	CHECK(!coop_transition_ack(&p, p.generation, COOP_PHASE_LOADING, 0, 3000));
	CHECK(acknowledge_team(&p, COOP_PHASE_CLOSING_PRESENTATION, 4000));
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_LOADING, 0, 5000));
	coop_transition_tick(&p, 90000);
	CHECK(!coop_transition_gameplay_allowed(&p, 0));
	CHECK(!coop_transition_gameplay_allowed(&p, 1));
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_LOADING, 1, 90000));
	CHECK(p.phase == COOP_PHASE_COMMITTED);
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_COMMITTED, 1, 91000));
	CHECK(!coop_transition_gameplay_allowed(&p, 1));
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_COMMITTED, 0, 91000));
	CHECK(coop_transition_gameplay_allowed(&p, 0));
	CHECK(coop_transition_gameplay_allowed(&p, 1));
	return 1;
}

static int test_progress_and_authoritative_removal(void)
{
	coop_transition_policy p, before;
	CHECK(briefing(&p));
	CHECK(coop_transition_progress(&p, p.generation, 1, 2, 2, 5,
	                               COOP_PRESENTATION_VIDEO, 3000));
	before = p;
	CHECK(!coop_transition_progress(&p, p.generation, 1, 1, 1, 5,
	                                COOP_PRESENTATION_READING, 4000));
	CHECK(!coop_transition_progress(&p, p.generation, 1, 3, 2, 6,
	                                COOP_PRESENTATION_READING, 4000));
	CHECK(!coop_transition_progress(&p, p.generation, 8, 3, 2, 5,
	                                COOP_PRESENTATION_READING, 4000));
	CHECK(!memcmp(&before, &p, sizeof(p)));
	CHECK(coop_transition_progress(&p, p.generation, 0, 1, 0, 0,
	                               COOP_PRESENTATION_READY, 3000));
	CHECK(!coop_transition_remove_player(&p, 0, 4000));
	CHECK(coop_transition_remove_player(&p, 1, 4000));
	CHECK(p.phase == COOP_PHASE_CLOSING_PRESENTATION);
	CHECK(p.launch_reason == COOP_LAUNCH_ALL_READY);
	return 1;
}

static int test_page_counts_do_not_decide_readiness(void)
{
	coop_transition_policy p;
	CHECK(briefing(&p));
	CHECK(coop_transition_progress(&p, p.generation, 0, 1, 3, 3,
	                               COOP_PRESENTATION_READING, 3000));
	CHECK(coop_transition_progress(&p, p.generation, 1, 1, 4, 8,
	                               COOP_PRESENTATION_READY, 3000));
	CHECK(p.presentation_ready == 2);
	coop_transition_tick(&p, 3000);
	CHECK(p.phase == COOP_PHASE_BRIEFING);
	CHECK(p.progress[0].total == 3 && p.progress[1].total == 8);
	CHECK(coop_transition_progress(&p, p.generation, 0, 2, 3, 3,
	                               COOP_PRESENTATION_READY, 4000));
	coop_transition_tick(&p, 4000);
	CHECK(p.phase == COOP_PHASE_CLOSING_PRESENTATION);
	CHECK(p.launch_reason == COOP_LAUNCH_ALL_READY);
	return 1;
}

static int test_operation_gate_recovery_and_stale_ack(void)
{
	coop_transition_policy p;
	uint64_t old_generation;
	CHECK(coop_transition_init(&p, 80, 0, 3, 0));
	CHECK(!coop_transition_begin(&p, 80, COOP_OP_LOAD, 1, 0));
	CHECK(coop_transition_begin(&p, 80, COOP_OP_LOAD, 0, 0));
	CHECK(!coop_transition_begin(&p, 80, COOP_OP_NORMAL_EXIT, 1, 0));
	CHECK(acknowledge_team(&p, COOP_PHASE_FREEZING, 1000));
	CHECK(coop_transition_source_captured(&p, p.generation, 1000));
	old_generation = p.generation;
	CHECK(coop_transition_abort(&p, old_generation));
	CHECK(p.generation > old_generation);
	CHECK(!coop_transition_ack(&p, old_generation, COOP_PHASE_LOADING, 1, 2000));
	CHECK(!coop_transition_recovered(&p, old_generation));
	CHECK(!coop_transition_recovered(&p, p.generation));
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_RECOVERING, 0, 2000));
	CHECK(!coop_transition_recovered(&p, p.generation));
	CHECK(coop_transition_ack(&p, p.generation, COOP_PHASE_RECOVERING, 1, 2000));
	CHECK(coop_transition_recovered(&p, p.generation));
	CHECK(coop_transition_block_reason(&p) == NULL);
	CHECK(coop_transition_begin(&p, p.generation, COOP_OP_SECRET_RETURN, 1, 3000));
	coop_transition_host_lost(&p);
	CHECK(p.phase == COOP_PHASE_LOBBY);
	CHECK(!coop_transition_recovered(&p, p.generation));
	CHECK(!coop_transition_gameplay_allowed(&p, 1));
	return 1;
}

static int test_full_roster_duplicate_and_out_of_order_barriers(void)
{
	coop_transition_policy p;
	unsigned player;
	CHECK(coop_transition_init(&p, 12, 3, 255, 0));
	CHECK(coop_transition_begin(&p, 12, COOP_OP_BRIEFING, 3, 0));
	for (player = 0; player < 7; ++player) {
		CHECK(coop_transition_ack(&p, 13, COOP_PHASE_BRIEFING_PREPARE, player, 100));
		CHECK(coop_transition_ack(&p, 13, COOP_PHASE_BRIEFING_PREPARE, player, 100));
	}
	CHECK(p.phase == COOP_PHASE_BRIEFING_PREPARE);
	CHECK(coop_transition_ack(&p, 13, COOP_PHASE_BRIEFING_PREPARE, 7, 100));
	for (player = 0; player < 8; ++player)
		CHECK(coop_transition_progress(&p, 13, player, 1, 0, 0,
		                               COOP_PRESENTATION_READY, 100));
	/* Completion is handled on the next host tick, not in the Skip callback */
	CHECK(p.phase == COOP_PHASE_BRIEFING);
	coop_transition_tick(&p, 100);
	CHECK(p.phase == COOP_PHASE_CLOSING_PRESENTATION);
	CHECK(acknowledge_team(&p, COOP_PHASE_CLOSING_PRESENTATION, 200));
	for (player = 0; player < 8; ++player) {
		CHECK(!coop_transition_ack(&p, 12, COOP_PHASE_LOADING, player, 200));
		CHECK(!coop_transition_ack(&p, 13, COOP_PHASE_COMMITTED, player, 200));
	}
	CHECK(p.acknowledged == 0);
	CHECK(acknowledge_team(&p, COOP_PHASE_LOADING, 300));
	for (player = 0; player < 8; ++player)
		CHECK(!coop_transition_gameplay_allowed(&p, player));
	CHECK(acknowledge_team(&p, COOP_PHASE_COMMITTED, 400));
	for (player = 0; player < 8; ++player)
		CHECK(coop_transition_gameplay_allowed(&p, player));
	return 1;
}

static int test_secret_disconnect_releases_each_barrier(void)
{
	const coop_transition_phase phases[] = { COOP_PHASE_FREEZING, COOP_PHASE_CAPTURING, COOP_PHASE_LOADING, COOP_PHASE_COMMITTED };
	for (unsigned removed_at = 0; removed_at < sizeof(phases) / sizeof(phases[0]); ++removed_at) {
		coop_transition_policy p;
		CHECK(coop_transition_init(&p, 1, 0, 7, 0));
		CHECK(coop_transition_begin(&p, 1, COOP_OP_SECRET_ENTER, 2, 1));
		for (unsigned stage = 0; stage < sizeof(phases) / sizeof(phases[0]); ++stage) {
			CHECK(p.phase == phases[stage]);
			CHECK(coop_transition_ack(&p, p.generation, p.phase, 0, 10));
			if (stage == removed_at) {
				CHECK(!coop_transition_remove_player(&p, 0, 10));
				CHECK(coop_transition_remove_player(&p, 2, 10));
				CHECK(!coop_transition_ack(&p, p.generation, p.phase, 2, 10));
			}
			CHECK(p.phase == phases[stage]); /* Remaining client is still required */
			if (stage < removed_at) CHECK(coop_transition_ack(&p, p.generation, p.phase, 2, 10));
			CHECK(coop_transition_ack(&p, p.generation, p.phase, 1, 10));
		}
		CHECK(p.phase == COOP_PHASE_SETTLED && p.participants == 3);
	}
	return 1;
}

int main(void)
{
	if (!test_secret_disconnect_releases_each_barrier() ||
	    !test_normal_exit_race_keeps_other_player_in_mine() ||
	    !test_secret_winner_waits_for_freeze_and_snapshot() ||
	    !test_load_starts_when_every_peer_is_prepared() ||
	    !test_briefing_deadlines() || !test_force_launch_does_not_bypass_loading() ||
	    !test_progress_and_authoritative_removal() || !test_operation_gate_recovery_and_stale_ack() ||
	    !test_page_counts_do_not_decide_readiness() ||
	    !test_full_roster_duplicate_and_out_of_order_barriers())
		return 1;
	puts("PASS co-op transition policy: exit arbitration, deadlines, readiness, and recovery");
	return 0;
}
