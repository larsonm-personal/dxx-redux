#include <stdio.h>

#include "classic_demo_player.h"

#define TEST_MAX_PLAYERS 8

static int check(int condition, const char *message)
{
	if (condition)
		return 1;
	fprintf(stderr, "classic demo player test failed: %s\n", message);
	return 0;
}

int main(void)
{
	if (!check(!classic_demo_player_count_valid(-1, TEST_MAX_PLAYERS), "negative count accepted") ||
	    !check(!classic_demo_player_count_valid(0, TEST_MAX_PLAYERS), "zero count accepted") ||
	    !check(classic_demo_player_count_valid(1, TEST_MAX_PLAYERS), "minimum count rejected") ||
	    !check(classic_demo_player_count_valid(TEST_MAX_PLAYERS, TEST_MAX_PLAYERS), "maximum count rejected") ||
	    !check(!classic_demo_player_count_valid(TEST_MAX_PLAYERS + 1, TEST_MAX_PLAYERS), "oversized count accepted") ||
	    !check(!classic_demo_player_index_valid(-1, TEST_MAX_PLAYERS), "negative index accepted") ||
	    !check(classic_demo_player_index_valid(0, TEST_MAX_PLAYERS), "zero index rejected") ||
	    !check(classic_demo_player_index_valid(TEST_MAX_PLAYERS - 1, TEST_MAX_PLAYERS), "maximum index rejected") ||
	    !check(!classic_demo_player_index_valid(TEST_MAX_PLAYERS, TEST_MAX_PLAYERS), "oversized index accepted") ||
	    !check(!classic_demo_player_count_can_add(TEST_MAX_PLAYERS, TEST_MAX_PLAYERS), "full count can grow") ||
	    !check(classic_demo_player_count_can_add(TEST_MAX_PLAYERS - 1, TEST_MAX_PLAYERS), "available count cannot grow") ||
	    !check(!classic_demo_player_count_can_remove(1, TEST_MAX_PLAYERS), "minimum count can shrink") ||
	    !check(classic_demo_player_count_can_remove(2, TEST_MAX_PLAYERS), "nonminimum count cannot shrink"))
		return 1;

	puts("classic demo player tests passed");
	return 0;
}
