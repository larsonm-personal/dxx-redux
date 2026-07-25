#include "coop_player_session.h"

#include <stdio.h>

#define CHECK(condition)                                                       \
	do {                                                                       \
		if (!(condition)) {                                                    \
			fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
			        #condition);                                               \
			return 0;                                                         \
		}                                                                      \
	} while (0)

static int test_restore_preserves_live_session_identity(void)
{
	player live_player = { 0 };
	player restored_player = { 0 };

	memcpy(live_player.callsign, "touch", sizeof("touch"));
	memcpy(live_player.net_address, "live42", sizeof(live_player.net_address));
	live_player.connected = 2;
	live_player.objnum = 77;
	live_player.n_packets_got = 123;
	live_player.n_packets_sent = 456;
	live_player.energy = 1;
	live_player.score = 2;

	memcpy(restored_player.callsign, "coopsave", sizeof("coopsave"));
	memcpy(restored_player.net_address, "save42",
	       sizeof(restored_player.net_address));
	restored_player.connected = 1;
	restored_player.objnum = 9;
	restored_player.n_packets_got = 10;
	restored_player.n_packets_sent = 11;
	restored_player.energy = 100;
	restored_player.score = 83450;

	coop_restore_player_game_state(&live_player, &restored_player);

	CHECK(!strcmp(live_player.callsign, "touch"));
	CHECK(!memcmp(live_player.net_address, "live42",
	              sizeof(live_player.net_address)));
	CHECK(live_player.connected == 2);
	CHECK(live_player.objnum == 77);
	CHECK(live_player.n_packets_got == 123);
	CHECK(live_player.n_packets_sent == 456);
	CHECK(live_player.energy == 100);
	CHECK(live_player.score == 83450);
	return 1;
}

static int test_sync_prefers_stable_client_identity(void)
{
	CHECK(coop_sync_identity_matches("client-a", "client-a", 0));
	CHECK(coop_sync_identity_matches("", "", 1));
	CHECK(!coop_sync_identity_matches("client-a", "client-b", 0));
	CHECK(!coop_sync_identity_matches("", "", 0));
	return 1;
}

int main(void)
{
	if (!test_restore_preserves_live_session_identity() ||
	    !test_sync_prefers_stable_client_identity())
		return 1;
	puts("coop player session tests passed");
	return 0;
}
