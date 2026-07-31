#include <stdio.h>

#include "coop_warp.h"

static int expect_int(const char *label, int expected, int actual)
{
	if (expected == actual)
		return 0;
	fprintf(stderr, "%s expected %d got %d\n", label, expected, actual);
	return 1;
}

int main(void)
{
	int failures = 0;

	failures += expect_int("below minimum distance blocked", 0,
		coop_warp_distance_allows(COOP_WARP_DISTANCE_THRESHOLD - 1));
	failures += expect_int("minimum distance allowed", 1,
		coop_warp_distance_allows(COOP_WARP_DISTANCE_THRESHOLD));
	failures += expect_int("above minimum distance allowed", 1,
		coop_warp_distance_allows(COOP_WARP_DISTANCE_THRESHOLD + F1_0));

	if (failures)
		return 1;

	puts("PASS: coop warp distance policy");
	return 0;
}
