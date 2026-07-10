#include "escort_owner_policy.h"

int escort_owner_packet_sender_valid(int claimed_sender, int authenticated_sender)
{
	return authenticated_sender >= 0 && claimed_sender == authenticated_sender;
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
