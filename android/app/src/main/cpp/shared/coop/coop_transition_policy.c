#include "coop_transition_policy.h"

#include <limits.h>
#include <string.h>

static unsigned player_bit(unsigned player)
{
	return player < COOP_TRANSITION_PLAYERS ? 1u << player : 0;
}

static int participating(const coop_transition_policy *p, unsigned player)
{
	return (p->participants & player_bit(player)) != 0;
}

static void update_clock(coop_transition_policy *p, uint64_t now_ms)
{
	if (now_ms > p->now_ms)
		p->now_ms = now_ms;
}

static uint64_t deadline_after(const coop_transition_policy *p, unsigned duration)
{
	return p->now_ms > UINT64_MAX - duration ? UINT64_MAX : p->now_ms + duration;
}

static void set_phase(coop_transition_policy *p, coop_transition_phase phase)
{
	p->phase = phase;
	p->acknowledged = 0;
	p->deadline_ms = 0;
}

static void finish(coop_transition_policy *p)
{
	set_phase(p, COOP_PHASE_SETTLED);
	p->operation = COOP_OP_NONE;
}

static void complete_barrier(coop_transition_policy *p)
{
	if (!p->participants || (p->acknowledged & p->participants) != p->participants)
		return;
	switch (p->phase) {
		case COOP_PHASE_NORMAL_WAIT: set_phase(p, COOP_PHASE_LOADING); break;
		case COOP_PHASE_FREEZING: set_phase(p, COOP_PHASE_CAPTURING); break;
		case COOP_PHASE_CAPTURING:
			coop_transition_source_captured(p, p->generation, p->now_ms);
			break;
		case COOP_PHASE_BRIEFING_PREPARE:
			set_phase(p, COOP_PHASE_BRIEFING);
			p->deadline_ms = deadline_after(p, COOP_BRIEFING_LIMIT_MS);
			break;
		case COOP_PHASE_CLOSING_PRESENTATION: set_phase(p, COOP_PHASE_LOADING); break;
		case COOP_PHASE_LOADING: set_phase(p, COOP_PHASE_COMMITTED); break;
		case COOP_PHASE_COMMITTED: finish(p); break;
		default: break;
	}
}

int coop_transition_init(coop_transition_policy *p, uint64_t generation,
                         unsigned host, unsigned participants, uint64_t now_ms)
{
	if (!p || !generation || participants > UINT8_MAX ||
	    !(participants & player_bit(host)))
		return 0;
	memset(p, 0, sizeof(*p));
	p->generation = generation;
	p->host = (uint8_t) host;
	p->participants = (uint8_t) participants;
	p->now_ms = now_ms;
	return 1;
}

int coop_transition_begin(coop_transition_policy *p, uint64_t generation,
                          coop_operation operation, unsigned requester, uint64_t now_ms)
{
	if (!p || p->generation != generation || p->generation == UINT64_MAX ||
	    p->phase != COOP_PHASE_SETTLED || !participating(p, requester) ||
	    operation <= COOP_OP_NONE || operation > COOP_OP_RESTART)
		return 0;
	/* Save/load/rewind/restart authority is the host, even if a client asked */
	if (operation >= COOP_OP_BRIEFING && requester != p->host)
		return 0;
	update_clock(p, now_ms);
	p->source_generation = p->generation++;
	p->operation = operation;
	p->launch_reason = COOP_LAUNCH_NONE;
	p->presentation_ready = 0;
	memset(p->progress, 0, sizeof(p->progress));
	set_phase(p, operation == COOP_OP_NORMAL_EXIT ? COOP_PHASE_NORMAL_WAIT : operation == COOP_OP_BRIEFING ? COOP_PHASE_BRIEFING_PREPARE
	                                                                                                       : COOP_PHASE_FREEZING);
	return 1;
}

int coop_transition_normal_exit_allowed(const coop_transition_policy *p,
                                        uint64_t generation, unsigned player)
{
	return p && p->phase == COOP_PHASE_NORMAL_WAIT && participating(p, player) &&
	       (generation == p->generation || generation == p->source_generation);
}

int coop_transition_ack(coop_transition_policy *p, uint64_t generation,
                        coop_transition_phase phase, unsigned player, uint64_t now_ms)
{
	if (!p || p->generation != generation || p->phase != phase ||
	    !participating(p, player))
		return 0;
	switch (phase) {
		case COOP_PHASE_NORMAL_WAIT:
		case COOP_PHASE_FREEZING:
		case COOP_PHASE_CAPTURING:
		case COOP_PHASE_BRIEFING_PREPARE:
		case COOP_PHASE_CLOSING_PRESENTATION:
		case COOP_PHASE_LOADING:
		case COOP_PHASE_COMMITTED:
		case COOP_PHASE_RECOVERING: break;
		default: return 0;
	}
	update_clock(p, now_ms);
	p->acknowledged |= (uint8_t) player_bit(player);
	complete_barrier(p);
	return 1;
}

int coop_transition_source_captured(coop_transition_policy *p, uint64_t generation,
                                    uint64_t now_ms)
{
	if (!p || p->generation != generation || p->phase != COOP_PHASE_CAPTURING)
		return 0;
	update_clock(p, now_ms);
	if (p->test_secret_warning_ms &&
	    (p->operation == COOP_OP_SECRET_ENTER || p->operation == COOP_OP_SECRET_RETURN)) {
		set_phase(p, COOP_PHASE_WARNING);
		p->deadline_ms = deadline_after(p, p->test_secret_warning_ms);
	} else {
		/* Saving has no destination-world apply, but still requires a release */
		set_phase(p, p->operation == COOP_OP_SAVE ? COOP_PHASE_COMMITTED : COOP_PHASE_LOADING);
	}
	return 1;
}

static int presentation_done(coop_presentation_state state)
{
	return state == COOP_PRESENTATION_READY || state == COOP_PRESENTATION_SKIPPED ||
	       state == COOP_PRESENTATION_UNAVAILABLE;
}

int coop_transition_progress(coop_transition_policy *p, uint64_t generation,
                             unsigned player, uint32_t revision, unsigned completed,
                             unsigned total, coop_presentation_state state,
                             uint64_t now_ms)
{
	coop_presentation_progress *progress;
	uint64_t shortened;
	if (!p || generation != p->generation || p->phase != COOP_PHASE_BRIEFING ||
	    !participating(p, player) || !revision || completed > total || total > UINT16_MAX ||
	    state < COOP_PRESENTATION_READING || state > COOP_PRESENTATION_UNAVAILABLE)
		return 0;
	progress = &p->progress[player];
	if (revision <= progress->revision || presentation_done(progress->state) ||
	    (progress->revision && (total != progress->total || completed < progress->completed)))
		return 0;
	update_clock(p, now_ms);
	progress->revision = revision;
	progress->completed = (uint16_t) completed;
	progress->total = (uint16_t) total;
	progress->state = state;
	if (presentation_done(state)) {
		p->presentation_ready |= (uint8_t) player_bit(player);
		if (player == p->host) {
			shortened = deadline_after(p, COOP_BRIEFING_HOST_WAIT_MS);
			if (shortened < p->deadline_ms)
				p->deadline_ms = shortened;
		}
	}
	return 1;
}

static void launch(coop_transition_policy *p, coop_launch_reason reason)
{
	set_phase(p, COOP_PHASE_CLOSING_PRESENTATION);
	p->launch_reason = reason;
}

int coop_transition_launch_now(coop_transition_policy *p, uint64_t generation,
                               unsigned player, uint64_t now_ms)
{
	if (!p || p->generation != generation || player != p->host ||
	    p->phase != COOP_PHASE_BRIEFING || !(p->presentation_ready & player_bit(player)))
		return 0;
	update_clock(p, now_ms);
	launch(p, COOP_LAUNCH_HOST);
	return 1;
}

void coop_transition_tick(coop_transition_policy *p, uint64_t now_ms)
{
	if (!p)
		return;
	update_clock(p, now_ms);
	if (p->phase == COOP_PHASE_WARNING && p->now_ms >= p->deadline_ms)
		set_phase(p, COOP_PHASE_LOADING);
	else if (p->phase == COOP_PHASE_BRIEFING) {
		if (p->now_ms >= p->deadline_ms)
			launch(p, COOP_LAUNCH_DEADLINE);
		else if (p->presentation_ready == p->participants)
			launch(p, COOP_LAUNCH_ALL_READY);
	}
}

int coop_transition_remove_player(coop_transition_policy *p, unsigned player, uint64_t now_ms)
{
	if (!p || !participating(p, player) || player == p->host)
		return 0;
	p->participants &= (uint8_t) ~player_bit(player);
	p->acknowledged &= p->participants;
	p->presentation_ready &= p->participants;
	update_clock(p, now_ms);
	complete_barrier(p);
	coop_transition_tick(p, now_ms);
	return 1;
}

int coop_transition_abort(coop_transition_policy *p, uint64_t generation)
{
	if (!p || p->generation != generation || p->generation == UINT64_MAX ||
	    p->phase == COOP_PHASE_SETTLED || p->phase == COOP_PHASE_COMMITTED ||
	    p->phase == COOP_PHASE_RECOVERING || p->phase == COOP_PHASE_LOBBY ||
	    p->phase == COOP_PHASE_NORMAL_WAIT)
		return 0;
	p->generation++;
	set_phase(p, COOP_PHASE_RECOVERING);
	return 1;
}

int coop_transition_recovered(coop_transition_policy *p, uint64_t generation)
{
	if (!p || p->generation != generation || p->phase != COOP_PHASE_RECOVERING ||
	    p->acknowledged != p->participants)
		return 0;
	finish(p);
	return 1;
}

void coop_transition_host_lost(coop_transition_policy *p)
{
	if (p && p->phase != COOP_PHASE_SETTLED)
		set_phase(p, COOP_PHASE_LOBBY);
}

int coop_transition_gameplay_allowed(const coop_transition_policy *p, unsigned player)
{
	return p && participating(p, player) &&
	       (p->phase == COOP_PHASE_SETTLED ||
	        (p->phase == COOP_PHASE_NORMAL_WAIT && !(p->acknowledged & player_bit(player))));
}

unsigned coop_transition_seconds_remaining(const coop_transition_policy *p)
{
	uint64_t remaining;
	if (!p || !p->deadline_ms || p->now_ms >= p->deadline_ms)
		return 0;
	remaining = p->deadline_ms - p->now_ms;
	remaining = remaining / 1000 + (remaining % 1000 != 0);
	return remaining > UINT_MAX ? UINT_MAX : (unsigned) remaining;
}

const char *coop_transition_block_reason(const coop_transition_policy *p)
{
	if (!p)
		return "Co-op session unavailable";
	switch (p->phase) {
		case COOP_PHASE_SETTLED: return NULL;
		case COOP_PHASE_NORMAL_WAIT: return "Waiting for players to exit the mine";
		case COOP_PHASE_BRIEFING_PREPARE: return "Preparing briefings";
		case COOP_PHASE_BRIEFING: return "Waiting for briefings to finish";
		case COOP_PHASE_CLOSING_PRESENTATION: return "Starting the mine";
		case COOP_PHASE_WARNING: return "Secret teleporter transition in progress";
		case COOP_PHASE_RECOVERING: return "Recovering co-op session";
		case COOP_PHASE_LOBBY: return "Return to the lobby to recover the session";
		default: return "Co-op operation in progress";
	}
}
