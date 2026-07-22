#include "thief_network_policy.h"

#include <stdio.h>

enum {
	TEST_MODE_CHASE = 1,
	TEST_MODE_THIEF_ATTACK = 15,
	TEST_MODE_THIEF_RETREAT = 16,
	TEST_MODE_THIEF_WAIT = 17,
};

#define CHECK(condition)                                                       \
	do {                                                                       \
		if (!(condition)) {                                                    \
			fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
			        #condition);                                               \
			return 0;                                                         \
		}                                                                      \
	} while (0)

static int test_only_thief_modes_are_accepted(void)
{
	CHECK(thief_network_mode_is_valid(TEST_MODE_THIEF_ATTACK,
	                                  TEST_MODE_THIEF_ATTACK,
	                                  TEST_MODE_THIEF_RETREAT,
	                                  TEST_MODE_THIEF_WAIT));
	CHECK(thief_network_mode_is_valid(TEST_MODE_THIEF_RETREAT,
	                                  TEST_MODE_THIEF_ATTACK,
	                                  TEST_MODE_THIEF_RETREAT,
	                                  TEST_MODE_THIEF_WAIT));
	CHECK(thief_network_mode_is_valid(TEST_MODE_THIEF_WAIT,
	                                  TEST_MODE_THIEF_ATTACK,
	                                  TEST_MODE_THIEF_RETREAT,
	                                  TEST_MODE_THIEF_WAIT));
	CHECK(!thief_network_mode_is_valid(THIEF_NETWORK_NO_MODE,
	                                   TEST_MODE_THIEF_ATTACK,
	                                   TEST_MODE_THIEF_RETREAT,
	                                   TEST_MODE_THIEF_WAIT));
	CHECK(!thief_network_mode_is_valid(TEST_MODE_CHASE,
	                                   TEST_MODE_THIEF_ATTACK,
	                                   TEST_MODE_THIEF_RETREAT,
	                                   TEST_MODE_THIEF_WAIT));
	CHECK(!thief_network_mode_is_valid(-1, TEST_MODE_THIEF_ATTACK,
	                                   TEST_MODE_THIEF_RETREAT,
	                                   TEST_MODE_THIEF_WAIT));
	return 1;
}

static int test_only_contacted_owner_rebuilds_path(void)
{
	CHECK(thief_network_should_prepare_path(THIEF_STATE_CONTACT, 2, 2));
	CHECK(!thief_network_should_prepare_path(0, 2, 2));
	CHECK(!thief_network_should_prepare_path(THIEF_STATE_CONTACT, 1, 2));
	CHECK(!thief_network_should_prepare_path(THIEF_STATE_CONTACT, -1, 2));
	return 1;
}

int main(void)
{
	if (!test_only_thief_modes_are_accepted() ||
	    !test_only_contacted_owner_rebuilds_path())
		return 1;
	puts("thief network policy tests passed");
	return 0;
}
