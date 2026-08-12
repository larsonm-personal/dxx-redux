#ifndef D2_ESCORT_OWNER_POLICY_H
#define D2_ESCORT_OWNER_POLICY_H

int escort_owner_packet_sender_valid(int claimed_sender, int authenticated_sender);
int escort_owner_slot_eligible(int slot, int player_count, int connected_playing, int host_is_observer);
int escort_owner_first_eligible_slot(const unsigned char *eligible, int player_count);
int escort_owner_remap_saved_slot(
    int saved_owner,
    const int *original_slot_by_current_slot,
    const unsigned char *eligible,
    int player_count);
int escort_owner_request_allowed(int current_owner,
                                 int sender,
                                 int requested_owner,
                                 int sender_eligible,
                                 int requested_owner_eligible,
                                 unsigned int current_generation,
                                 unsigned int request_generation);
int escort_owner_generation_is_newer(unsigned int candidate, unsigned int current);
int escort_owner_key_change_relevant(int changed_player, int effective_owner);
int escort_route_key_change_matches_objective(
    int old_flags,
    int new_flags,
    int objective_key_flag);
int escort_retry_recovery_allowed(
    int multiplayer,
    int cooperative,
    int companion,
    int local_authority);

#define ESCORT_ROUTE_EVENT_WALL    (1u << 0)
#define ESCORT_ROUTE_EVENT_TRIGGER (1u << 1)
#define ESCORT_ROUTE_EVENT_OBJECT  (1u << 2)
#define ESCORT_ROUTE_EVENT_REACTOR (1u << 3)
#define ESCORT_ROUTE_EVENT_AUTOMAP (1u << 4)

int escort_route_event_should_dirty(
    int local_authority,
    int route_active,
    int unexplored_mode,
    unsigned int event_mask,
    int matches_objective);

#endif
