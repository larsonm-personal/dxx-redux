#include <stdio.h>

#include "android_rewind_policy.h"

static int expect_index(const char *label, int expected, int actual)
{
	if (expected == actual)
		return 0;
	fprintf(stderr, "%s: expected %d got %d\n", label, expected, actual);
	return 1;
}

int main(void)
{
	const android_rewind_selection_snapshot threshold_skip[] = {
		{500, 1},
		{1000, 1},
	};
	const android_rewind_selection_snapshot threshold_exact[] = {
		{500, 1},
		{1000, 1},
	};
	const android_rewind_selection_snapshot single_recent[] = {
		{1100, 1},
	};
	const android_rewind_selection_snapshot current_only[] = {
		{1200, 1},
	};
	const android_rewind_selection_snapshot demo_filtered[] = {
		{500, 0},
		{1000, 1},
		{1300, 0},
	};
	int failures = 0;

	failures += expect_index(
		"skip point newer than min age",
		0,
		android_rewind_select_snapshot_index_for_game_time(threshold_skip, 2, 1299, 300, 0));
	failures += expect_index(
		"pick newest point at exact min age",
		1,
		android_rewind_select_snapshot_index_for_game_time(threshold_exact, 2, 1300, 300, 0));
	failures += expect_index(
		"allow single remaining older point",
		0,
		android_rewind_select_snapshot_index_for_game_time(single_recent, 1, 1200, 300, 0));
	failures += expect_index(
		"reject current snapshot as rewind target",
		-1,
		android_rewind_select_snapshot_index_for_game_time(current_only, 1, 1200, 300, 0));
	failures += expect_index(
		"require demo timeline when recording",
		1,
		android_rewind_select_snapshot_index_for_game_time(demo_filtered, 3, 1400, 300, 1));

	if (failures)
		return 1;
	puts("PASS: rewind target selection respects age and demo timeline");
	return 0;
}