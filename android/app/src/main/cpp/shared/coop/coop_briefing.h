#ifndef COOP_BRIEFING_H
#define COOP_BRIEFING_H

#include <stddef.h>
#include <stdint.h>
#include "coop_transition_policy.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Level entry arms before synchronization identifies a possible rejoin */
void coop_briefing_arm(void (*present)(int), int level);
unsigned coop_briefing_count_intro(void (*present)(int), int level);
/* Bit 0 is the session preference; bit 1 skips this level in host SYNC */
unsigned coop_briefing_sync_flags(int level);
void coop_briefing_apply_sync_flags(unsigned flags, int level);
void coop_briefing_disarm_for_rejoin(void);
void coop_briefing_run(void (*present)(int), int level);
int coop_briefing_active(void);
int coop_briefing_suppressed_for_restore(void);
unsigned coop_briefing_presentations_started(void);
int coop_briefing_palette_changed(void);
int coop_briefing_palette_restored(void);
unsigned coop_briefing_release_acknowledged(void);
/* Local automation fault injection; never set by a network peer */
int coop_briefing_test_delay_release(unsigned milliseconds);
unsigned coop_briefing_test_release_packets_dropped(void);
int coop_briefing_presenting(void);
int coop_briefing_planning(void);
void coop_briefing_plan_add(unsigned steps);
void coop_briefing_plan_message(const char *message);
int coop_briefing_plan_ready(void);
unsigned coop_briefing_count_message(const char *message);
void coop_briefing_step(int video);
void coop_briefing_step_complete(int viewed);
void coop_briefing_skip(void);
int coop_briefing_cancelled(void);
/* Pump from presentation event loops, including paused movie windows */
void coop_briefing_pump(void);
void coop_briefing_receive(const unsigned char *packet, int sender);
/* Keeps the final release snapshot available to a late/retrying peer */
void coop_briefing_network_frame(void);
int coop_briefing_host_disconnected(int player);
/* Main-menu idle reports a connection failure after presentation teardown */
int coop_briefing_show_failure(void);
/* Thread-safe UI bridge; action is consumed on the engine thread */
void coop_briefing_ui(char *text, size_t capacity, uint64_t *generation, int *launch);
int coop_briefing_request_launch(uint64_t generation);
/* Engine-thread diagnostic snapshot */
void coop_briefing_get_state(coop_transition_policy *state, coop_presentation_progress *local);

#ifdef __cplusplus
}
#endif
#endif
