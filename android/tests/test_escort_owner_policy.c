#include "escort_owner_policy.h"

#include <stdio.h>

#define CHECK(condition)                                                       \
	do {                                                                       \
		if (!(condition)) {                                                    \
			fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
			        #condition);                                               \
			return 0;                                                         \
		}                                                                      \
	} while (0)

static int test_packet_sender_must_match_authenticated_transport_player(void)
{
	CHECK(escort_owner_packet_sender_valid(2, 2));
	CHECK(!escort_owner_packet_sender_valid(1, 2));
	CHECK(!escort_owner_packet_sender_valid(2, -1));
	return 1;
}

static int test_host_observer_is_not_an_owner_candidate(void)
{
	unsigned char eligible[3];

	eligible[0] = (unsigned char) escort_owner_slot_eligible(0, 3, 1, 1);
	eligible[1] = (unsigned char) escort_owner_slot_eligible(1, 3, 1, 1);
	eligible[2] = (unsigned char) escort_owner_slot_eligible(2, 3, 0, 1);
	CHECK(!eligible[0]);
	CHECK(eligible[1]);
	CHECK(!eligible[2]);
	CHECK(escort_owner_first_eligible_slot(eligible, 3) == 1);
	return 1;
}

static int test_saved_owner_follows_identity_to_a_new_slot(void)
{
	const int original_slot_by_current_slot[3] = { 2, 0, 1 };
	const unsigned char eligible[3] = { 1, 1, 1 };
	const unsigned char observer_host_eligible[3] = { 0, 1, 1 };

	CHECK(escort_owner_remap_saved_slot(
	          1, original_slot_by_current_slot, eligible, 3) == 2);
	CHECK(escort_owner_remap_saved_slot(
	          0, original_slot_by_current_slot, eligible, 3) == 1);
	CHECK(escort_owner_remap_saved_slot(
	          2, original_slot_by_current_slot, observer_host_eligible, 3) == -1);
	return 1;
}

static int test_unowned_bot_can_only_be_claimed_by_requester(void)
{
	CHECK(escort_owner_request_allowed(-1, 1, 1, 1, 1, 4, 4));
	CHECK(!escort_owner_request_allowed(-1, 1, 2, 1, 1, 4, 4));
	CHECK(!escort_owner_request_allowed(-1, 1, -1, 1, 1, 4, 4));
	return 1;
}

static int test_only_current_owner_can_transfer_or_abdicate(void)
{
	CHECK(escort_owner_request_allowed(1, 1, 2, 1, 1, 4, 4));
	CHECK(escort_owner_request_allowed(1, 1, -1, 1, 1, 4, 4));
	CHECK(!escort_owner_request_allowed(1, 2, 2, 1, 1, 4, 4));
	return 1;
}

static int test_ineligible_players_and_stale_requests_are_rejected(void)
{
	CHECK(!escort_owner_request_allowed(1, 1, 2, 0, 1, 4, 4));
	CHECK(!escort_owner_request_allowed(1, 1, 2, 1, 0, 4, 4));
	CHECK(!escort_owner_request_allowed(1, 1, 2, 1, 1, 5, 4));
	return 1;
}

int main(void)
{
	if (!test_packet_sender_must_match_authenticated_transport_player() ||
	    !test_host_observer_is_not_an_owner_candidate() ||
	    !test_saved_owner_follows_identity_to_a_new_slot() ||
	    !test_unowned_bot_can_only_be_claimed_by_requester() ||
	    !test_only_current_owner_can_transfer_or_abdicate() ||
	    !test_ineligible_players_and_stale_requests_are_rejected())
		return 1;
	puts("escort owner policy tests passed");
	return 0;
}
