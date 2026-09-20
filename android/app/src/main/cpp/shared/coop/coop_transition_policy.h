#ifndef COOP_TRANSITION_POLICY_H
#define COOP_TRANSITION_POLICY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Host-owned policy, independent of rendering and transport. The caller
 * authenticates senders and supplies monotonic milliseconds, never GameTime */
enum {
	COOP_TRANSITION_PLAYERS = 8,
	COOP_BRIEFING_LIMIT_MS = 120000,
	COOP_BRIEFING_HOST_WAIT_MS = 20000,
	COOP_SECRET_TEST_WARNING_MS = 7000
};

typedef enum coop_operation {
	COOP_OP_NONE,
	COOP_OP_NORMAL_EXIT,
	COOP_OP_SECRET_ENTER,
	COOP_OP_SECRET_RETURN,
	COOP_OP_BRIEFING,
	COOP_OP_SAVE,
	COOP_OP_LOAD,
	COOP_OP_REWIND,
	COOP_OP_RESTART,
	COOP_OP_FLYOUT
} coop_operation;

typedef enum coop_transition_phase {
	COOP_PHASE_SETTLED,
	COOP_PHASE_NORMAL_WAIT,
	COOP_PHASE_FREEZING,
	COOP_PHASE_CAPTURING,
	COOP_PHASE_WARNING,
	COOP_PHASE_BRIEFING_PREPARE,
	COOP_PHASE_BRIEFING,
	COOP_PHASE_CLOSING_PRESENTATION,
	COOP_PHASE_LOADING,
	COOP_PHASE_COMMITTED,
	COOP_PHASE_RECOVERING,
	COOP_PHASE_LOBBY
} coop_transition_phase;

typedef enum coop_presentation_state {
	COOP_PRESENTATION_PENDING,
	COOP_PRESENTATION_READING,
	COOP_PRESENTATION_VIDEO,
	COOP_PRESENTATION_READY,
	COOP_PRESENTATION_SKIPPED,
	COOP_PRESENTATION_UNAVAILABLE,
	COOP_PRESENTATION_FLYOUT
} coop_presentation_state;

typedef enum coop_launch_reason {
	COOP_LAUNCH_NONE,
	COOP_LAUNCH_ALL_READY,
	COOP_LAUNCH_HOST,
	COOP_LAUNCH_DEADLINE
} coop_launch_reason;

typedef struct coop_presentation_progress {
	uint32_t revision;
	uint16_t completed;
	uint16_t total;
	uint16_t remaining_ms;
	coop_presentation_state state;
} coop_presentation_progress;

typedef struct coop_transition_policy {
	uint64_t generation;
	uint64_t source_generation;
	uint64_t deadline_ms;
	uint64_t now_ms;
	/* Explicit automation inspection window; zero in normal gameplay */
	unsigned test_secret_warning_ms;
	coop_operation operation;
	coop_transition_phase phase;
	coop_launch_reason launch_reason;
	uint8_t host;
	uint8_t participants;
	uint8_t acknowledged;
	uint8_t presentation_ready;
	coop_presentation_progress progress[COOP_TRANSITION_PLAYERS];
} coop_transition_policy;

/* Fly-out allowance: content remaining + 5s, between 20s and 60s */
unsigned coop_flyout_allowance_ms(unsigned remaining_ms);

int coop_transition_init(coop_transition_policy *policy, uint64_t generation,
                         unsigned host, unsigned participants, uint64_t now_ms);
/* A normal exit locks the mode, but does not freeze or mark anyone escaped */
int coop_transition_begin(coop_transition_policy *policy, uint64_t generation,
                          coop_operation operation, unsigned requester, uint64_t now_ms);
/* A normal exit allowed after the mode is locked still follows individual escape */
int coop_transition_normal_exit_allowed(const coop_transition_policy *policy,
                                        uint64_t generation, unsigned player);
/* Ack means normal outcome resolved, frozen, presentation prepared/closed,
 * destination applied, or release received, according to the expected phase */
int coop_transition_ack(coop_transition_policy *policy, uint64_t generation,
                        coop_transition_phase phase, unsigned player, uint64_t now_ms);
int coop_transition_source_captured(coop_transition_policy *policy, uint64_t generation,
                                    uint64_t now_ms);
int coop_transition_progress(coop_transition_policy *policy, uint64_t generation,
                             unsigned player, uint32_t revision, unsigned completed,
                             unsigned total, coop_presentation_state state,
                             uint64_t now_ms);
int coop_transition_launch_now(coop_transition_policy *policy, uint64_t generation,
                               unsigned player, uint64_t now_ms);
void coop_transition_tick(coop_transition_policy *policy, uint64_t now_ms);
/* Only an authoritative disconnect/removal calls this, not a local timeout */
int coop_transition_remove_player(coop_transition_policy *policy, unsigned player,
                                  uint64_t now_ms);
/* Abort is prohibited after commit. Recovery needs a fresh generation */
int coop_transition_abort(coop_transition_policy *policy, uint64_t generation);
int coop_transition_recovered(coop_transition_policy *policy, uint64_t generation);
void coop_transition_host_lost(coop_transition_policy *policy);
int coop_transition_gameplay_allowed(const coop_transition_policy *policy, unsigned player);
unsigned coop_transition_seconds_remaining(const coop_transition_policy *policy);
const char *coop_transition_block_reason(const coop_transition_policy *policy);

#ifdef __cplusplus
}
#endif
#endif
