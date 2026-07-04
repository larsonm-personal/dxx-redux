#include <stdio.h>
#include <string.h>

#include "level_metadata_scan.h"

#define TEST_SEGMENTS 3
#define TEST_SIDES    LEVEL_METADATA_MAX_SIDES
#define TEST_WALLS    8
#define TEST_OBJECTS  4
#define TEST_FIX      65536
#define TEST_WALL_OPEN 4
#define TEST_WALL_BLASTABLE 1
#define TEST_WALL_DOOR 2
#define TEST_WALL_ILLUSION 3
#define TEST_WALL_CLOSED 5
#define TEST_KEY_NONE 1
#define TEST_KEY_BLUE 2
#define TEST_TRIGGER_OPEN_WALL 9
#define TEST_TRIGGER_EXIT 3
#define TEST_OBJ_POWERUP 1
#define TEST_POWERUP_BLUE_KEY 10

static int test_children[TEST_SEGMENTS][TEST_SIDES] = {
	{ 1, -1, -1, -1, -1, -1 },
	{ 2, 0, -1, -1, -1, -1 },
	{ -1, 1, -1, -1, -1, -1 }
};
static int test_wall_nums[TEST_SEGMENTS][TEST_SIDES];
static int test_wall_type[TEST_WALLS];
static int test_wall_key[TEST_WALLS];
static int test_wall_trigger[TEST_WALLS];
static int test_wall_seg[TEST_WALLS];
static int test_wall_sides[TEST_WALLS];
static int test_object_count_value;
static int test_object_type[TEST_OBJECTS];
static int test_object_id[TEST_OBJECTS];
static int test_object_seg[TEST_OBJECTS];
static int test_trigger_type[1];
static int test_trigger_link_count[1];
static int test_trigger_link_seg[1][LEVEL_METADATA_MAX_ROUTE_LINKS];
static int test_trigger_link_sides[1][LEVEL_METADATA_MAX_ROUTE_LINKS];

static void test_reset(void)
{
	int seg;
	int side;
	int wall;
	int object;
	int link;

	for (seg = 0; seg < TEST_SEGMENTS; ++seg)
		for (side = 0; side < TEST_SIDES; ++side)
			test_wall_nums[seg][side] = -1;
	for (wall = 0; wall < TEST_WALLS; ++wall) {
		test_wall_type[wall] = TEST_WALL_OPEN;
		test_wall_key[wall] = TEST_KEY_NONE;
		test_wall_trigger[wall] = -1;
		test_wall_seg[wall] = -1;
		test_wall_sides[wall] = -1;
	}
	test_object_count_value = 0;
	for (object = 0; object < TEST_OBJECTS; ++object) {
		test_object_type[object] = 0;
		test_object_id[object] = -1;
		test_object_seg[object] = -1;
	}
	test_trigger_type[0] = TEST_TRIGGER_OPEN_WALL;
	test_trigger_link_count[0] = 0;
	for (link = 0; link < LEVEL_METADATA_MAX_ROUTE_LINKS; ++link) {
		test_trigger_link_seg[0][link] = -1;
		test_trigger_link_sides[0][link] = -1;
	}
}

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

static int test_wall_num(void *user, int seg, int side)
{
	(void) user;
	if (seg < 0 || seg >= TEST_SEGMENTS || side < 0 || side >= TEST_SIDES)
		return -1;
	return test_wall_nums[seg][side];
}

static int test_wall_segment(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= TEST_WALLS)
		return -1;
	return test_wall_seg[wall_num];
}

static int test_wall_side(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= TEST_WALLS)
		return -1;
	return test_wall_sides[wall_num];
}

static int test_wall_type_at(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= TEST_WALLS)
		return TEST_WALL_OPEN;
	return test_wall_type[wall_num];
}

static int test_wall_keys(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= TEST_WALLS)
		return TEST_KEY_NONE;
	return test_wall_key[wall_num];
}

static int test_wall_trigger_at(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= TEST_WALLS)
		return -1;
	return test_wall_trigger[wall_num];
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

static int test_object_count(void *user)
{
	(void) user;
	return test_object_count_value;
}

static int test_object_segment(void *user, int objnum)
{
	(void) user;
	if (objnum < 0 || objnum >= TEST_OBJECTS)
		return -1;
	return test_object_seg[objnum];
}

static int test_object_type_at(void *user, int objnum)
{
	(void) user;
	if (objnum < 0 || objnum >= TEST_OBJECTS)
		return 0;
	return test_object_type[objnum];
}

static int test_object_id_at(void *user, int objnum)
{
	(void) user;
	if (objnum < 0 || objnum >= TEST_OBJECTS)
		return -1;
	return test_object_id[objnum];
}

static int test_object_position(void *user, int objnum, int xyz[3])
{
	(void) user;
	if (objnum < 0 || objnum >= TEST_OBJECTS || !xyz || test_object_seg[objnum] < 0)
		return 0;
	xyz[0] = test_object_seg[objnum] * 100 * TEST_FIX;
	xyz[1] = 0;
	xyz[2] = 0;
	return 1;
}

static int test_side_has_exit_trigger(void *user, int seg, int side)
{
	(void) user;
	return seg == 2 && side == 0;
}

static int test_triggered_side_opener_count(void *user, int seg, int side)
{
	(void) user;
	return (seg == 0 && side == 0) || (seg == 1 && side == 1) ? 1 : 0;
}

static int test_triggered_side_opener_wall_num(void *user, int seg, int side, int index)
{
	(void) user;
	if (index != 0)
		return -1;
	return (seg == 0 && side == 0) || (seg == 1 && side == 1) ? 2 : -1;
}

static int test_trigger_type_at(void *user, int trigger_num)
{
	(void) user;
	if (trigger_num != 0)
		return -1;
	return test_trigger_type[0];
}

static int test_trigger_link_count_at(void *user, int trigger_num)
{
	(void) user;
	if (trigger_num != 0)
		return 0;
	return test_trigger_link_count[0];
}

static int test_trigger_link_segment(void *user, int trigger_num, int link_index)
{
	(void) user;
	if (trigger_num != 0 || link_index < 0 || link_index >= test_trigger_link_count[0])
		return -1;
	return test_trigger_link_seg[0][link_index];
}

static int test_trigger_link_side(void *user, int trigger_num, int link_index)
{
	(void) user;
	if (trigger_num != 0 || link_index < 0 || link_index >= test_trigger_link_count[0])
		return -1;
	return test_trigger_link_sides[0][link_index];
}

static level_metadata_scan_view test_view(void)
{
	level_metadata_scan_view view;

	memset(&view, 0, sizeof(view));
	view.num_segments = TEST_SEGMENTS;
	view.num_walls = TEST_WALLS;
	view.start_segment = 0;
	view.segment_special_control_center = 3;
	view.wall_type_blastable = TEST_WALL_BLASTABLE;
	view.wall_type_door = TEST_WALL_DOOR;
	view.wall_type_illusion = TEST_WALL_ILLUSION;
	view.wall_type_open = TEST_WALL_OPEN;
	view.wall_key_none = TEST_KEY_NONE;
	view.wall_key_blue = TEST_KEY_BLUE;
	view.obj_type_powerup = TEST_OBJ_POWERUP;
	view.powerup_key_blue = TEST_POWERUP_BLUE_KEY;
	view.trigger_type_open_wall = TEST_TRIGGER_OPEN_WALL;
	view.trigger_type_exit = TEST_TRIGGER_EXIT;
	view.segment_child = test_segment_child;
	view.reverse_side = test_reverse_side;
	view.wall_num = test_wall_num;
	view.wall_segment = test_wall_segment;
	view.wall_side = test_wall_side;
	view.wall_type = test_wall_type_at;
	view.wall_keys = test_wall_keys;
	view.wall_trigger = test_wall_trigger_at;
	view.segment_special = test_segment_special;
	view.segment_center = test_segment_center;
	view.start_position = test_start_position;
	view.object_count = test_object_count;
	view.object_segment = test_object_segment;
	view.object_type = test_object_type_at;
	view.object_id = test_object_id_at;
	view.object_position = test_object_position;
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

	test_reset();
	level_metadata_scan_level(&view, &state);
	failures += expect_string("travel status", "ok", level_metadata_travel_status_name(state.travel_status));
	failures += expect_string("travel note", "no reactor, exit exists", state.travel_note);
	failures += expect_int("targets reached", 1, state.travel_targets_reached);
	failures += expect_int("targets total", 1, state.travel_targets_total);
	failures += expect_double("travel distance", 200.0, state.travel_distance);
	failures += expect_int("travel time", 4, state.travel_time_seconds);
	failures += expect_string("route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("route steps", 2, state.route_step_count);
	failures += expect_string("route step 0", "start", level_metadata_route_step_kind_name(state.route_steps[0].kind));
	failures += expect_string("route step 1", "exit", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	return failures;
}

static int test_reactorless_missing_exit(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	view.side_has_exit_trigger = NULL;
	level_metadata_scan_level(&view, &state);
	failures += expect_string("missing exit status", "failed", level_metadata_travel_status_name(state.travel_status));
	failures += expect_string("missing exit problem", "missing exit", state.travel_problem);
	failures += expect_string("missing exit note", "missing reactor", state.travel_note);
	failures += expect_string("missing exit route status", "failed", level_metadata_travel_status_name(state.route_status));
	failures += expect_string("missing exit route problem", "missing exit", state.route_problem);
	return failures;
}

static int test_route_key_step(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	test_wall_nums[0][0] = 0;
	test_wall_nums[1][1] = 1;
	test_wall_type[0] = TEST_WALL_DOOR;
	test_wall_type[1] = TEST_WALL_DOOR;
	test_wall_key[0] = TEST_KEY_BLUE;
	test_wall_key[1] = TEST_KEY_BLUE;
	test_wall_seg[0] = 0;
	test_wall_sides[0] = 0;
	test_wall_seg[1] = 1;
	test_wall_sides[1] = 1;
	test_object_count_value = 1;
	test_object_type[0] = TEST_OBJ_POWERUP;
	test_object_id[0] = TEST_POWERUP_BLUE_KEY;
	test_object_seg[0] = 0;
	level_metadata_scan_level(&view, &state);
	failures += expect_string("key route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("key route steps", 3, state.route_step_count);
	failures += expect_string("key route step", "key", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_int("key route key index", 0, state.route_steps[1].key_index);
	failures += expect_string("key route exit", "exit", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	return failures;
}

static int test_route_trigger_step(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	test_wall_nums[0][0] = 0;
	test_wall_nums[1][1] = 1;
	test_wall_nums[0][2] = 2;
	test_wall_type[0] = TEST_WALL_CLOSED;
	test_wall_type[1] = TEST_WALL_CLOSED;
	test_wall_type[2] = TEST_WALL_OPEN;
	test_wall_trigger[2] = 0;
	test_wall_seg[0] = 0;
	test_wall_sides[0] = 0;
	test_wall_seg[1] = 1;
	test_wall_sides[1] = 1;
	test_wall_seg[2] = 0;
	test_wall_sides[2] = 2;
	test_trigger_link_count[0] = 2;
	test_trigger_link_seg[0][0] = 0;
	test_trigger_link_sides[0][0] = 0;
	test_trigger_link_seg[0][1] = 1;
	test_trigger_link_sides[0][1] = 1;
	view.triggered_side_opener_count = test_triggered_side_opener_count;
	view.triggered_side_opener_wall_num = test_triggered_side_opener_wall_num;
	view.trigger_type = test_trigger_type_at;
	view.trigger_link_count = test_trigger_link_count_at;
	view.trigger_link_segment = test_trigger_link_segment;
	view.trigger_link_side = test_trigger_link_side;
	level_metadata_scan_level(&view, &state);
	failures += expect_string("trigger route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("trigger route steps", 3, state.route_step_count);
	failures += expect_string("trigger route step", "trigger", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_int("trigger route trigger", 0, state.route_steps[1].trigger_num);
	failures += expect_string("trigger route type", "open_wall", state.route_steps[1].trigger_type_name);
	failures += expect_int("trigger route link count", 2, state.route_steps[1].opened_link_count);
	failures += expect_string("trigger route exit", "exit", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	return failures;
}

int main(void)
{
	int failures = 0;

	failures += test_reactorless_reachable_exit();
	failures += test_reactorless_missing_exit();
	failures += test_route_key_step();
	failures += test_route_trigger_step();
	if (failures)
		return 1;

	puts("PASS: level metadata scan");
	return 0;
}
