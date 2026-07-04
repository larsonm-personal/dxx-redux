#include <stdio.h>
#include <string.h>

#include "level_metadata_scan.h"

#define TEST_SEGMENTS 3
#define TEST_SIDES    LEVEL_METADATA_MAX_SIDES
#define TEST_FIX      65536

static int test_children[TEST_SEGMENTS][TEST_SIDES] = {
	{ 1, -1, -1, -1, -1, -1 },
	{ 2, 0, -1, -1, -1, -1 },
	{ -1, 1, -1, -1, -1, -1 }
};

static int test_reverse_side(void *user, int seg, int child)
{
	int side;

	(void) user;
	if (seg < 0 || seg >= TEST_SEGMENTS || child < 0 || child >= TEST_SEGMENTS)
		return -1;
	for (side = 0; side < TEST_SIDES; side++)
		if (test_children[child][side] == seg)
			return side;
	return -1;
}

static int test_segment_child(void *user, int seg, int side)
{
	(void) user;
	if (seg < 0 || seg >= TEST_SEGMENTS || side < 0 || side >= TEST_SIDES)
		return -1;
	return test_children[seg][side];
}

static int test_segment_special(void *user, int seg)
{
	(void) user;
	(void) seg;
	return 0;
}

static int test_segment_center(void *user, int seg, int xyz[3])
{
	(void) user;
	if (seg < 0 || seg >= TEST_SEGMENTS || !xyz)
		return 0;
	xyz[0] = seg * 100 * TEST_FIX;
	xyz[1] = 0;
	xyz[2] = 0;
	return 1;
}

static int test_start_position(void *user, int xyz[3])
{
	(void) user;
	if (!xyz)
		return 0;
	xyz[0] = 0;
	xyz[1] = 0;
	xyz[2] = 0;
	return 1;
}

static int test_side_has_exit_trigger(void *user, int seg, int side)
{
	(void) user;
	return seg == 2 && side == 0;
}

static level_metadata_scan_view test_view(void)
{
	level_metadata_scan_view view;

	memset(&view, 0, sizeof(view));
	view.num_segments = TEST_SEGMENTS;
	view.start_segment = 0;
	view.segment_special_control_center = 3;
	view.wall_key_none = 0;
	view.segment_child = test_segment_child;
	view.reverse_side = test_reverse_side;
	view.segment_special = test_segment_special;
	view.segment_center = test_segment_center;
	view.start_position = test_start_position;
	view.side_has_exit_trigger = test_side_has_exit_trigger;
	return view;
}

static int expect_int(const char *label, int expected, int actual)
{
	if (expected == actual)
		return 0;
	fprintf(stderr, "%s expected %d got %d\n", label, expected, actual);
	return 1;
}

static int expect_double(const char *label, double expected, double actual)
{
	double delta = expected - actual;

	if (delta < 0.0)
		delta = -delta;
	if (delta < 0.0001)
		return 0;
	fprintf(stderr, "%s expected %.4f got %.4f\n", label, expected, actual);
	return 1;
}

static int expect_string(const char *label, const char *expected, const char *actual)
{
	if (!strcmp(expected, actual))
		return 0;
	fprintf(stderr, "%s expected \"%s\" got \"%s\"\n", label, expected, actual);
	return 1;
}

static int test_reactorless_reachable_exit(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	level_metadata_scan_level(&view, &state);
	failures += expect_string("travel status", "ok", level_metadata_travel_status_name(state.travel_status));
	failures += expect_string("travel note", "no reactor, exit exists", state.travel_note);
	failures += expect_int("targets reached", 1, state.travel_targets_reached);
	failures += expect_int("targets total", 1, state.travel_targets_total);
	failures += expect_double("travel distance", 200.0, state.travel_distance);
	failures += expect_int("travel time", 4, state.travel_time_seconds);
	return failures;
}

static int test_reactorless_missing_exit(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	view.side_has_exit_trigger = NULL;
	level_metadata_scan_level(&view, &state);
	failures += expect_string("missing exit status", "failed", level_metadata_travel_status_name(state.travel_status));
	failures += expect_string("missing exit problem", "missing exit", state.travel_problem);
	failures += expect_string("missing exit note", "missing reactor", state.travel_note);
	return failures;
}

int main(void)
{
	int failures = 0;

	failures += test_reactorless_reachable_exit();
	failures += test_reactorless_missing_exit();
	if (failures)
		return 1;

	puts("PASS: level metadata scan");
	return 0;
}
