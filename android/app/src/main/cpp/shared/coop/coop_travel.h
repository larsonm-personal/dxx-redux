#ifndef COOP_TRAVEL_H
#define COOP_TRAVEL_H

#include "coop_transition_policy.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Engine-thread gate. The travel owner supplies capture, load and recovery;
 * packet callbacks only record requests/acknowledgments, never replace a mine */
int coop_travel_arm(void);
/* Arm with host-owned campaign preparation, loading and commit */
int coop_travel_arm_campaign(void);
/* Returns nonzero when host arbitration consumes or blocks a physical trigger */
int coop_travel_handle_exit_trigger(int trigger_num, int player, int shot);
int coop_travel_exit_side_blocked(int segnum, int side);
void coop_travel_get_physical_state(int *mode, int *pending, int *accepted);
void coop_travel_get_arrival_state(int *placed, uint32_t *checksum, int *blocked);
/* Automation holds the existing load acknowledgment for frozen-state inspection */
int coop_travel_test_hold_arrival(int hold);
/* Automation fault: delay a pending physical request across a competing exit decision */
int coop_travel_test_delay_request(int verify);
int coop_travel_campaign_stage_allowed(void);
int coop_travel_stage_campaign(const void *data, size_t size);
int coop_travel_stage_checkpoint(const void *data, size_t size);
void coop_travel_get_checkpoint_state(int *ready, uint32_t *checksum, size_t *size);
/* Verify the retained file against the staged source, without changing engine state */
int coop_travel_verify_checkpoint_file(void);
int coop_travel_rollback_allowed(void);
int coop_travel_restore_source(const void *data, size_t size);
int coop_travel_transfer_buffer_allowed(int kind);
void coop_travel_get_rollback_state(int *required, int *complete, int *failed_level);
/* Automation fault: fail only after the destination engine load succeeded */
void coop_travel_test_fail_after_load(void);
void coop_travel_get_campaign_state(int *ready, uint32_t *checksum, int *destination);
int coop_travel_apply_portable(void);
void coop_travel_get_portable_state(unsigned *received, uint32_t *checksum);
void coop_travel_reset(void);
int coop_travel_request(coop_operation operation);
int coop_travel_take_normal_exit(void);
int coop_travel_source_captured(void);
void coop_travel_destination_started(void);
void coop_travel_destination_applied(void);
/* World transfers can arrive before the matching phase snapshot. Buffer them,
 * but never apply until the local gate has entered LOADING. Once armed, an
 * aborted/settled operation cannot admit a delayed world application */
int coop_travel_world_apply_allowed(void);
/* Fresh loads must match the validated detached destination of this operation */
int coop_travel_fresh_destination_allowed(int level);
void coop_travel_world_failed(void);
void coop_travel_campaign_committed(void);
int coop_travel_abort(void);
void coop_travel_source_restored(void);
int coop_travel_active(void);
/* Pending exit arbitration blocks state replacement without freezing gameplay */
int coop_travel_blocks_state_actions(void);
int coop_travel_blocks_gameplay(void);
/* Terminal scores retain a closed world while their network wait runs */
int coop_travel_ending_campaign(void);
/* Death/drop settlement accepts source updates until the reliable drain barrier */
int coop_travel_blocks_world_updates(void);
int coop_travel_settling_players(void);
/* Reactor deaths keep participating until the host chooses a shared departure */
int coop_travel_defer_player_death(int player);
int coop_travel_defer_death_screen(void);
void coop_travel_player_reappeared(int player);
int coop_travel_waiting_after_death(void);
void coop_travel_get_freeze_state(unsigned *frozen, int *ready, unsigned *deaths);
int coop_travel_host_disconnected(int player);
/* Returns -1 to close the game window, 1 after a physical normal exit, or 0 */
int coop_travel_frame(void);
void coop_travel_network_frame(void);
void coop_travel_receive(const unsigned char *packet, int sender);
const char *coop_travel_status_message(void);
void coop_travel_get_state(coop_transition_policy *state, unsigned *normal_granted,
                           unsigned *release_acked, int *known);

#ifdef __cplusplus
}
#endif
#endif
