#include "escort_owner_policy.h"

#include <assert.h>
#include <stdio.h>

static void test_unowned_bot_can_only_be_claimed_by_requester(void)
{
	assert(escort_owner_request_allowed(-1, 1, 1, 1, 1, 4, 4));
	assert(!escort_owner_request_allowed(-1, 1, 2, 1, 1, 4, 4));
	assert(!escort_owner_request_allowed(-1, 1, -1, 1, 1, 4, 4));
}

static void test_only_current_owner_can_transfer_or_abdicate(void)
{
	assert(escort_owner_request_allowed(1, 1, 2, 1, 1, 4, 4));
	assert(escort_owner_request_allowed(1, 1, -1, 1, 1, 4, 4));
	assert(!escort_owner_request_allowed(1, 2, 2, 1, 1, 4, 4));
}

static void test_ineligible_players_and_stale_requests_are_rejected(void)
{
	assert(!escort_owner_request_allowed(1, 1, 2, 0, 1, 4, 4));
	assert(!escort_owner_request_allowed(1, 1, 2, 1, 0, 4, 4));
	assert(!escort_owner_request_allowed(1, 1, 2, 1, 1, 5, 4));
}

int main(void)
{
	test_unowned_bot_can_only_be_claimed_by_requester();
	test_only_current_owner_can_transfer_or_abdicate();
	test_ineligible_players_and_stale_requests_are_rejected();
	puts("escort owner policy tests passed");
	return 0;
}
