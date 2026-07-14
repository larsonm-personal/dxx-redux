#include "escort_owner_policy.h"

#include <stdint.h>

int escort_owner_packet_sender_valid(int claimed_sender, int authenticated_sender)
{
	return authenticated_sender >= 0 && claimed_sender == authenticated_sender;
}

int escort_owner_slot_eligible(int slot, int player_count, int connected_playing, int host_is_observer)
{
	if (slot < 0 || slot >= player_count)
		return 0;
	if (!connected_playing)
		return 0;
	return !(host_is_observer && slot == 0);
}

int escort_owner_first_eligible_slot(const unsigned char *eligible, int player_count)
{
	int slot;

	if (!eligible || player_count <= 0)
		return -1;
	for (slot = 0; slot < player_count; ++slot)
		if (eligible[slot])
			return slot;
	return -1;
}

int escort_owner_remap_saved_slot(
    int saved_owner,
    const int *original_slot_by_current_slot,
    const unsigned char *eligible,
    int player_count)
{
	int current_slot;

	if (saved_owner < 0 ||
	    !original_slot_by_current_slot ||
	    !eligible ||
	    player_count <= 0)
		return -1;
	for (current_slot = 0; current_slot < player_count; ++current_slot)
		if (eligible[current_slot] &&
		    original_slot_by_current_slot[current_slot] == saved_owner)
			return current_slot;
	return -1;
}

int escort_owner_request_allowed(int current_owner,
                                 int sender,
                                 int requested_owner,
                                 int sender_eligible,
                                 int requested_owner_eligible,
                                 unsigned int current_generation,
                                 unsigned int request_generation)
{
	if (!sender_eligible)
		return 0;
	if (requested_owner != -1 && !requested_owner_eligible)
		return 0;
	if (request_generation != current_generation)
		return 0;
	if (current_owner == -1)
		return requested_owner == sender;
	return sender == current_owner;
}

int escort_owner_generation_is_newer(unsigned int candidate, unsigned int current)
{
	return candidate != 0 && candidate != current &&
	       (int32_t)(candidate - current) > 0;
}

int escort_owner_key_change_relevant(int changed_player, int effective_owner)
{
	return changed_player >= 0 && changed_player == effective_owner;
}
