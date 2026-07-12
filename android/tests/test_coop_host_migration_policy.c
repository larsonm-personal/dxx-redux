#include "coop_host_migration_policy.h"

#include <stdio.h>

enum {
	TEST_DISCONNECTED = 0,
	TEST_PLAYING = 1,
	TEST_WAITING = 2,
};

#define CHECK(condition)                                                       \
	do {                                                                       \
		if (!(condition)) {                                                    \
			fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
			        #condition);                                               \
			return 0;                                                         \
		}                                                                      \
	} while (0)

static int test_ordinary_disconnect_does_not_change_host(void)
{
	const int8_t states[] = { TEST_PLAYING, TEST_DISCONNECTED, TEST_PLAYING };
	const coop_host_migration_decision decision =
		coop_host_migration_decide(0, 1, 2, states, 3, TEST_PLAYING);

	CHECK(decision.action == COOP_HOST_MIGRATION_NOT_HOST);
	CHECK(decision.new_master == 0);
	return 1;
}

static int test_lowest_playing_survivor_becomes_local_host(void)
{
	const int8_t states[] = { TEST_DISCONNECTED, TEST_PLAYING, TEST_PLAYING };
	const coop_host_migration_decision decision =
		coop_host_migration_decide(0, 0, 1, states, 3, TEST_PLAYING);

	CHECK(decision.action == COOP_HOST_MIGRATION_LOCAL_HOST);
	CHECK(decision.new_master == 1);
	return 1;
}

static int test_all_peers_choose_the_same_remote_host(void)
{
	const int8_t states[] = { TEST_DISCONNECTED, TEST_PLAYING, TEST_PLAYING, TEST_PLAYING };
	const coop_host_migration_decision decision =
		coop_host_migration_decide(0, 0, 3, states, 4, TEST_PLAYING);

	CHECK(decision.action == COOP_HOST_MIGRATION_REMOTE_HOST);
	CHECK(decision.new_master == 1);
	return 1;
}

static int test_waiting_and_disconnected_slots_are_ineligible(void)
{
	const int8_t states[] = { TEST_DISCONNECTED, TEST_WAITING, TEST_DISCONNECTED, TEST_PLAYING };
	const coop_host_migration_decision decision =
		coop_host_migration_decide(0, 0, 3, states, 4, TEST_PLAYING);

	CHECK(decision.action == COOP_HOST_MIGRATION_LOCAL_HOST);
	CHECK(decision.new_master == 3);
	return 1;
}

static int test_no_playing_survivor_preserves_legacy_host_loss(void)
{
	const int8_t states[] = { TEST_DISCONNECTED, TEST_WAITING, TEST_DISCONNECTED };
	const coop_host_migration_decision decision =
		coop_host_migration_decide(0, 0, 1, states, 3, TEST_PLAYING);

	CHECK(decision.action == COOP_HOST_MIGRATION_NO_REPLACEMENT);
	CHECK(decision.new_master == -1);
	return 1;
}

static int test_disconnected_host_is_never_re_elected(void)
{
	const int8_t stale_states[] = { TEST_PLAYING, TEST_PLAYING };
	const coop_host_migration_decision decision =
		coop_host_migration_decide(0, 0, 1, stale_states, 2, TEST_PLAYING);

	CHECK(decision.action == COOP_HOST_MIGRATION_LOCAL_HOST);
	CHECK(decision.new_master == 1);
	return 1;
}

static int test_local_host_bootstrap_resets_all_object_owners(void)
{
	int8_t owners[] = { 0, 1, -1, 3, 2 };
	int i;

	coop_host_migration_reset_object_owners(owners, 5);
	for (i = 0; i < 5; i++)
		CHECK(owners[i] == -1);
	coop_host_migration_reset_object_owners(NULL, 5);
	coop_host_migration_reset_object_owners(owners, 0);
	return 1;
}

int main(void)
{
	if (!test_ordinary_disconnect_does_not_change_host() ||
	    !test_lowest_playing_survivor_becomes_local_host() ||
	    !test_all_peers_choose_the_same_remote_host() ||
	    !test_waiting_and_disconnected_slots_are_ineligible() ||
	    !test_no_playing_survivor_preserves_legacy_host_loss() ||
	    !test_disconnected_host_is_never_re_elected() ||
	    !test_local_host_bootstrap_resets_all_object_owners())
		return 1;
	puts("coop host migration policy tests passed");
	return 0;
}
