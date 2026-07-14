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

#endif
