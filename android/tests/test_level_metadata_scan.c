#include <stdio.h>
#include <string.h>

#include "level_metadata_scan.h"

#define TEST_SEGMENTS              4
#define TEST_SIDES                 LEVEL_METADATA_MAX_SIDES
#define TEST_WALLS                 8
#define TEST_OBJECTS               4
#define TEST_TRIGGERS              2
#define TEST_FIX                   65536
#define TEST_WALL_OPEN             4
#define TEST_WALL_BLASTABLE        1
#define TEST_WALL_DOOR             2
#define TEST_WALL_ILLUSION         3
#define TEST_WALL_CLOSED           5
#define TEST_WALL_FLAG_DOOR_OPENED 2
#define TEST_WALL_FLAG_DOOR_LOCKED 8
#define TEST_WALL_CLIP_HIDDEN      8
#define TEST_KEY_NONE              1
#define TEST_KEY_BLUE              2
#define TEST_KEY_RED               4
#define TEST_KEY_GOLD              8
#define TEST_TRIGGER_OPEN_WALL     9
#define TEST_TRIGGER_EXIT          3
#define TEST_TRIGGER_FLAG_DISABLED 4
#define TEST_OBJ_POWERUP           1
#define TEST_OBJ_CONTROL_CENTER    2
#define TEST_OBJ_ROBOT             3
#define TEST_POWERUP_BLUE_KEY      10
#define TEST_POWERUP_RED_KEY       11
#define TEST_POWERUP_GOLD_KEY      12
#define TEST_ROBOT_BOSS            20
#define TEST_ROBOT_GUIDEBOT        21

static const int test_default_children[TEST_SEGMENTS][TEST_SIDES] = {
	{ 1, -1, -1, -1, -1, -1 },
	{ 2, 0, -1, -1, -1, -1 },
	{ -1, 1, -1, -1, -1, -1 },
	{ -1, -1, -1, -1, -1, -1 }
};
static int test_children[TEST_SEGMENTS][TEST_SIDES];
static int test_wall_nums[TEST_SEGMENTS][TEST_SIDES];
static int test_wall_type[TEST_WALLS];
static int test_wall_flags[TEST_WALLS];
static int test_wall_key[TEST_WALLS];
static int test_wall_clip_flags[TEST_WALLS];
static int test_wall_trigger[TEST_WALLS];
static int test_wall_shootable[TEST_WALLS];
static int test_wall_seg[TEST_WALLS];
static int test_wall_sides[TEST_WALLS];
static int test_object_count_value;
static int test_object_type[TEST_OBJECTS];
static int test_object_id[TEST_OBJECTS];
static int test_object_seg[TEST_OBJECTS];
static int test_segment_special_values[TEST_SEGMENTS];
static int test_trigger_type[TEST_TRIGGERS];
static int test_trigger_flags[TEST_TRIGGERS];
static int test_trigger_link_count[TEST_TRIGGERS];
static int test_trigger_link_seg[TEST_TRIGGERS][LEVEL_METADATA_MAX_ROUTE_LINKS];
static int test_trigger_link_sides[TEST_TRIGGERS][LEVEL_METADATA_MAX_ROUTE_LINKS];
static int test_side_flyable[TEST_SEGMENTS][TEST_SIDES];
static int test_control_center_link[TEST_SEGMENTS][TEST_SIDES];
static int test_segment_explored[TEST_SEGMENTS];

static void test_reset(void)
{
	int seg;
	int side;
	int wall;
	int object;
	int trigger;
	int link;

	for (seg = 0; seg < TEST_SEGMENTS; ++seg)
		for (side = 0; side < TEST_SIDES; ++side) {
			test_children[seg][side] = test_default_children[seg][side];
			test_side_flyable[seg][side] = 0;
			test_control_center_link[seg][side] = 0;
		}
	for (seg = 0; seg < TEST_SEGMENTS; ++seg)
		for (side = 0; side < TEST_SIDES; ++side)
			test_wall_nums[seg][side] = -1;
	for (wall = 0; wall < TEST_WALLS; ++wall) {
		test_wall_type[wall] = TEST_WALL_OPEN;
		test_wall_flags[wall] = 0;
		test_wall_key[wall] = TEST_KEY_NONE;
		test_wall_clip_flags[wall] = 0;
		test_wall_trigger[wall] = -1;
		test_wall_shootable[wall] = 0;
		test_wall_seg[wall] = -1;
		test_wall_sides[wall] = -1;
	}
	test_object_count_value = 0;
	for (object = 0; object < TEST_OBJECTS; ++object) {
		test_object_type[object] = 0;
		test_object_id[object] = -1;
		test_object_seg[object] = -1;
	}
	for (seg = 0; seg < TEST_SEGMENTS; ++seg)
	{
		test_segment_special_values[seg] = 0;
		test_segment_explored[seg] = 0;
	}
	for (trigger = 0; trigger < TEST_TRIGGERS; ++trigger) {
		test_trigger_type[trigger] = TEST_TRIGGER_OPEN_WALL;
		test_trigger_flags[trigger] = 0;
		test_trigger_link_count[trigger] = 0;
		for (link = 0; link < LEVEL_METADATA_MAX_ROUTE_LINKS; ++link) {
			test_trigger_link_seg[trigger][link] = -1;
			test_trigger_link_sides[trigger][link] = -1;
		}
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

static int test_segment_is_explored(void *user, int seg)
{
	(void) user;
	return seg >= 0 && seg < TEST_SEGMENTS && test_segment_explored[seg];
}

static int test_side_is_flyable(void *user, int seg, int side)
{
	(void) user;
	return seg >= 0 && seg < TEST_SEGMENTS &&
	       side >= 0 && side < TEST_SIDES &&
	       test_side_flyable[seg][side];
}

static int test_side_is_control_center_link(void *user, int seg, int side)
{
	(void) user;
	return seg >= 0 && seg < TEST_SEGMENTS &&
	       side >= 0 && side < TEST_SIDES &&
	       test_control_center_link[seg][side];
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

static int test_wall_flags_at(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= TEST_WALLS)
		return 0;
	return test_wall_flags[wall_num];
}

static int test_wall_keys(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= TEST_WALLS)
		return TEST_KEY_NONE;
	return test_wall_key[wall_num];
}

static int test_wall_clip_flags_at(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= TEST_WALLS)
		return 0;
	return test_wall_clip_flags[wall_num];
}

static int test_wall_trigger_at(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= TEST_WALLS)
		return -1;
	return test_wall_trigger[wall_num];
}

static int test_wall_is_shootable_trigger(void *user, int wall_num)
{
	(void) user;
	if (wall_num < 0 || wall_num >= TEST_WALLS)
		return 0;
	return test_wall_shootable[wall_num];
}

static int test_segment_special(void *user, int seg)
{
	(void) user;
	if (seg < 0 || seg >= TEST_SEGMENTS)
		return 0;
	return test_segment_special_values[seg];
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

static int test_object_is_boss(void *user, int objnum)
{
	(void) user;
	return objnum >= 0 &&
	       objnum < TEST_OBJECTS &&
	       test_object_type[objnum] == TEST_OBJ_ROBOT &&
	       test_object_id[objnum] == TEST_ROBOT_BOSS;
}

static int test_object_is_companion(void *user, int objnum)
{
	(void) user;
	return objnum >= 0 &&
	       objnum < TEST_OBJECTS &&
	       test_object_type[objnum] == TEST_OBJ_ROBOT &&
	       test_object_id[objnum] == TEST_ROBOT_GUIDEBOT;
}

static int test_side_has_exit_trigger(void *user, int seg, int side)
{
	(void) user;
	return seg == 2 && side == 0;
}

static int test_start_side_has_exit_trigger(void *user, int seg, int side)
{
	(void) user;
	return seg == 0 && side == 5;
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

static int test_multi_opener_side_opener_count(void *user, int seg, int side)
{
	(void) user;
	return (seg == 1 && side == 0) || (seg == 2 && side == 1) ? 2 : 0;
}

static int test_multi_opener_side_opener_wall_num(void *user, int seg, int side, int index)
{
	(void) user;
	if (!((seg == 1 && side == 0) || (seg == 2 && side == 1)))
		return -1;
	if (index == 0)
		return 2;
	if (index == 1)
		return 3;
	return -1;
}

static int test_trigger_type_at(void *user, int trigger_num)
{
	(void) user;
	if (trigger_num < 0 || trigger_num >= TEST_TRIGGERS)
		return -1;
	return test_trigger_type[trigger_num];
}

static int test_trigger_flags_at(void *user, int trigger_num)
{
	(void) user;
	if (trigger_num < 0 || trigger_num >= TEST_TRIGGERS)
		return 0;
	return test_trigger_flags[trigger_num];
}

static int test_trigger_link_count_at(void *user, int trigger_num)
{
	(void) user;
	if (trigger_num < 0 || trigger_num >= TEST_TRIGGERS)
		return 0;
	return test_trigger_link_count[trigger_num];
}

static int test_trigger_link_segment(void *user, int trigger_num, int link_index)
{
	(void) user;
	if (trigger_num < 0 || trigger_num >= TEST_TRIGGERS ||
	    link_index < 0 || link_index >= test_trigger_link_count[trigger_num])
		return -1;
	return test_trigger_link_seg[trigger_num][link_index];
}

static int test_trigger_link_side(void *user, int trigger_num, int link_index)
{
	(void) user;
	if (trigger_num < 0 || trigger_num >= TEST_TRIGGERS ||
	    link_index < 0 || link_index >= test_trigger_link_count[trigger_num])
		return -1;
	return test_trigger_link_sides[trigger_num][link_index];
}

static int test_target_visible_from_segment(void *user, int seg, const int from_pos[3], int target_seg, const int target_pos[3])
{
	(void) user;
	(void) from_pos;
	(void) target_pos;
	return seg == 0 && target_seg == 1;
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
	view.wall_flag_door_locked = TEST_WALL_FLAG_DOOR_LOCKED;
	view.wall_flag_door_opened = TEST_WALL_FLAG_DOOR_OPENED;
	view.wall_key_none = TEST_KEY_NONE;
	view.wall_key_blue = TEST_KEY_BLUE;
	view.wall_key_red = TEST_KEY_RED;
	view.wall_key_gold = TEST_KEY_GOLD;
	view.wall_clip_hidden = TEST_WALL_CLIP_HIDDEN;
	view.obj_type_powerup = TEST_OBJ_POWERUP;
	view.obj_type_control_center = TEST_OBJ_CONTROL_CENTER;
	view.obj_type_robot = TEST_OBJ_ROBOT;
	view.powerup_key_blue = TEST_POWERUP_BLUE_KEY;
	view.powerup_key_red = TEST_POWERUP_RED_KEY;
	view.powerup_key_gold = TEST_POWERUP_GOLD_KEY;
	view.trigger_type_open_wall = TEST_TRIGGER_OPEN_WALL;
	view.trigger_type_exit = TEST_TRIGGER_EXIT;
	view.trigger_flag_disabled = TEST_TRIGGER_FLAG_DISABLED;
	view.segment_child = test_segment_child;
	view.segment_is_explored = test_segment_is_explored;
	view.reverse_side = test_reverse_side;
	view.side_is_flyable = test_side_is_flyable;
	view.side_is_control_center_link = test_side_is_control_center_link;
	view.wall_num = test_wall_num;
	view.wall_segment = test_wall_segment;
	view.wall_side = test_wall_side;
	view.wall_type = test_wall_type_at;
	view.wall_flags = test_wall_flags_at;
	view.wall_keys = test_wall_keys;
	view.wall_clip_flags = test_wall_clip_flags_at;
	view.wall_trigger = test_wall_trigger_at;
	view.trigger_flags = test_trigger_flags_at;
	view.segment_special = test_segment_special;
	view.segment_center = test_segment_center;
	view.start_position = test_start_position;
	view.object_count = test_object_count;
	view.object_segment = test_object_segment;
	view.object_type = test_object_type_at;
	view.object_id = test_object_id_at;
	view.object_position = test_object_position;
	view.object_is_boss = test_object_is_boss;
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
	failures += expect_string("route exit activation", "enter_exit", level_metadata_route_activation_kind_name(state.route_steps[1].activation_kind));
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

static int test_end_route_refresh_preserves_static_metadata(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	level_metadata_scan_level(&view, &state);
	state.energy_center_count = 37;
	state.travel_distance = 1234.0;
	level_metadata_scan_end_route(&view, &state);
	failures += expect_int("route-only energy centers", 37, state.energy_center_count);
	failures += expect_double("route-only travel distance", 1234.0, state.travel_distance);
	failures += expect_string("route-only status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("route-only steps", 2, state.route_step_count);
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
	failures += expect_string("key route activation", "pickup_key", level_metadata_route_activation_kind_name(state.route_steps[1].activation_kind));
	failures += expect_string("key route exit", "exit", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	return failures;
}

static int test_route_uses_initial_key_without_powerup(void)
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
	view.initial_key_mask = LEVEL_METADATA_KEY_MASK_BLUE;
	level_metadata_scan_level(&view, &state);
	failures += expect_string("initial key route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("initial key route steps", 2, state.route_step_count);
	failures += expect_string("initial key route exit", "exit", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	return failures;
}

static int test_route_key_uses_longer_open_path(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	test_children[0][2] = 3;
	test_children[3][3] = 0;
	test_children[3][0] = 2;
	test_children[2][3] = 3;
	test_wall_nums[0][0] = 0;
	test_wall_nums[1][1] = 1;
	test_wall_nums[1][0] = 2;
	test_wall_nums[2][1] = 3;
	test_wall_type[0] = TEST_WALL_DOOR;
	test_wall_type[1] = TEST_WALL_DOOR;
	test_wall_type[2] = TEST_WALL_DOOR;
	test_wall_type[3] = TEST_WALL_DOOR;
	test_wall_key[0] = TEST_KEY_BLUE;
	test_wall_key[1] = TEST_KEY_BLUE;
	test_wall_key[2] = TEST_KEY_BLUE;
	test_wall_key[3] = TEST_KEY_BLUE;
	test_wall_seg[0] = 0;
	test_wall_sides[0] = 0;
	test_wall_seg[1] = 1;
	test_wall_sides[1] = 1;
	test_wall_seg[2] = 1;
	test_wall_sides[2] = 0;
	test_wall_seg[3] = 2;
	test_wall_sides[3] = 1;
	test_object_count_value = 2;
	test_object_type[0] = TEST_OBJ_POWERUP;
	test_object_id[0] = TEST_POWERUP_BLUE_KEY;
	test_object_seg[0] = 2;
	test_object_type[1] = TEST_OBJ_CONTROL_CENTER;
	test_object_seg[1] = 1;
	level_metadata_scan_level(&view, &state);
	failures += expect_string("longer key route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("longer key route steps", 4, state.route_step_count);
	failures += expect_string("longer key route key", "key", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_int("longer key route key index", 0, state.route_steps[1].key_index);
	failures += expect_string("longer key route reactor", "reactor", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	failures += expect_string("longer key route exit", "exit", level_metadata_route_step_kind_name(state.route_steps[3].kind));
	return failures;
}

static int test_route_prefers_ordered_key_chain(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	test_children[0][2] = 3;
	test_children[3][3] = 0;
	test_wall_nums[1][0] = 0;
	test_wall_nums[2][1] = 1;
	test_wall_type[0] = TEST_WALL_DOOR;
	test_wall_type[1] = TEST_WALL_DOOR;
	test_wall_key[0] = TEST_KEY_GOLD;
	test_wall_key[1] = TEST_KEY_GOLD;
	test_wall_seg[0] = 1;
	test_wall_sides[0] = 0;
	test_wall_seg[1] = 2;
	test_wall_sides[1] = 1;
	test_object_count_value = 4;
	test_object_type[0] = TEST_OBJ_POWERUP;
	test_object_id[0] = TEST_POWERUP_BLUE_KEY;
	test_object_seg[0] = 3;
	test_object_type[1] = TEST_OBJ_POWERUP;
	test_object_id[1] = TEST_POWERUP_GOLD_KEY;
	test_object_seg[1] = 1;
	test_object_type[2] = TEST_OBJ_POWERUP;
	test_object_id[2] = TEST_POWERUP_RED_KEY;
	test_object_seg[2] = 2;
	test_object_type[3] = TEST_OBJ_CONTROL_CENTER;
	test_object_seg[3] = 2;
	level_metadata_scan_level(&view, &state);
	failures += expect_string("ordered key route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("ordered key route steps", 6, state.route_step_count);
	failures += expect_string("ordered key route blue", "key", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_int("ordered key route blue index", 0, state.route_steps[1].key_index);
	failures += expect_string("ordered key route gold", "key", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	failures += expect_int("ordered key route gold index", 2, state.route_steps[2].key_index);
	failures += expect_string("ordered key route red", "key", level_metadata_route_step_kind_name(state.route_steps[3].kind));
	failures += expect_int("ordered key route red index", 1, state.route_steps[3].key_index);
	failures += expect_string("ordered key route reactor", "reactor", level_metadata_route_step_kind_name(state.route_steps[4].kind));
	failures += expect_string("ordered key route exit", "exit", level_metadata_route_step_kind_name(state.route_steps[5].kind));
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
	failures += expect_string("trigger route label", "Fly-through trigger 0", state.route_steps[1].label);
	failures += expect_string("trigger route activation", "fly_through_trigger", level_metadata_route_activation_kind_name(state.route_steps[1].activation_kind));
	failures += expect_int("trigger route trigger", 0, state.route_steps[1].trigger_num);
	failures += expect_string("trigger route type", "open_wall", state.route_steps[1].trigger_type_name);
	failures += expect_int("trigger route link count", 2, state.route_steps[1].opened_link_count);
	failures += expect_string("trigger route exit", "exit", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	return failures;
}

static int test_route_shootable_trigger_step(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	test_wall_nums[0][0] = 0;
	test_wall_nums[1][1] = 1;
	test_wall_nums[1][2] = 2;
	test_wall_type[0] = TEST_WALL_CLOSED;
	test_wall_type[1] = TEST_WALL_CLOSED;
	test_wall_type[2] = TEST_WALL_CLOSED;
	test_wall_trigger[2] = 0;
	test_wall_shootable[2] = 1;
	test_wall_seg[0] = 0;
	test_wall_sides[0] = 0;
	test_wall_seg[1] = 1;
	test_wall_sides[1] = 1;
	test_wall_seg[2] = 1;
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
	view.target_visible_from_segment = test_target_visible_from_segment;
	view.wall_is_shootable_trigger = test_wall_is_shootable_trigger;
	level_metadata_scan_level(&view, &state);
	failures += expect_string("shootable trigger route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("shootable trigger route steps", 3, state.route_step_count);
	failures += expect_string("shootable trigger route step", "trigger", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_string("shootable trigger route label", "Shoot switch trigger 0", state.route_steps[1].label);
	failures += expect_string("shootable trigger route activation", "shoot_switch", level_metadata_route_activation_kind_name(state.route_steps[1].activation_kind));
	failures += expect_int("shootable trigger route segment", 0, state.route_steps[1].seg);
	failures += expect_int("shootable trigger route wall", 2, state.route_steps[1].wall_num);
	failures += expect_int("shootable trigger route trigger", 0, state.route_steps[1].trigger_num);
	failures += expect_string("shootable trigger route exit", "exit", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	return failures;
}

static int test_route_skips_already_opened_trigger_door(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	test_wall_nums[0][0] = 0;
	test_wall_nums[1][1] = 1;
	test_wall_nums[0][2] = 2;
	test_wall_type[0] = TEST_WALL_DOOR;
	test_wall_type[1] = TEST_WALL_DOOR;
	test_wall_type[2] = TEST_WALL_OPEN;
	test_wall_flags[0] = TEST_WALL_FLAG_DOOR_OPENED;
	test_wall_flags[1] = TEST_WALL_FLAG_DOOR_OPENED;
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
	failures += expect_string("opened trigger door route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("opened trigger door route steps", 2, state.route_step_count);
	failures += expect_string("opened trigger door route exit", "exit", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	return failures;
}

static int test_route_does_not_reoffer_disabled_trigger(void)
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
	test_trigger_flags[0] = TEST_TRIGGER_FLAG_DISABLED;
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
	failures += expect_string("disabled trigger route status", "failed", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("disabled trigger route steps", 1, state.route_step_count);
	failures += expect_string("disabled trigger route problem", "exit unreachable", state.route_problem);
	return failures;
}

static int test_route_promotes_unreachable_trigger_blocker(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	test_wall_nums[0][0] = 0;
	test_wall_nums[1][1] = 1;
	test_wall_nums[3][0] = 2;
	test_wall_type[0] = TEST_WALL_CLOSED;
	test_wall_type[1] = TEST_WALL_CLOSED;
	test_wall_type[2] = TEST_WALL_CLOSED;
	test_wall_trigger[2] = 0;
	test_wall_seg[0] = 0;
	test_wall_sides[0] = 0;
	test_wall_seg[1] = 1;
	test_wall_sides[1] = 1;
	test_wall_seg[2] = 3;
	test_wall_sides[2] = 0;
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
	failures += expect_string("unreachable trigger route status", "partial", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("unreachable trigger route steps", 2, state.route_step_count);
	failures += expect_string("unreachable trigger route step", "trigger", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_int("unreachable trigger route wall", 2, state.route_steps[1].wall_num);
	failures += expect_int("unreachable trigger route trigger", 0, state.route_steps[1].trigger_num);
	return failures;
}

static int test_segment_route_reuses_trigger_dependencies(void)
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
	level_metadata_scan_route_to_segment(&view, 2, &state);
	failures += expect_string("segment route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("segment route steps", 3, state.route_step_count);
	failures += expect_string("segment route trigger", "trigger", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_string("segment route terminal", "unexplored", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	failures += expect_int("segment route terminal seg", 2, state.route_steps[2].seg);
	return failures;
}

static int test_unexplored_route_acquires_key_for_largest_component(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	level_metadata_unexplored_route result;
	int failures = 0;

	test_reset();
	test_segment_explored[0] = 1;
	test_children[0][2] = 3;
	test_children[3][1] = 0;
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
	level_metadata_scan_unexplored_route(&view, &state, &result);
	failures += expect_string("keyed unexplored status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("keyed unexplored component", 2, result.component_size);
	failures += expect_int("keyed unexplored target", 1, result.target_seg);
	failures += expect_int("keyed unexplored waypoint", 0, result.waypoint_seg);
	failures += expect_int("keyed unexplored direct", 0, result.direct_reachable);
	failures += expect_int("keyed unexplored steps", 3, state.route_step_count);
	failures += expect_string("keyed unexplored key step", "key", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_string("keyed unexplored terminal", "unexplored", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	return failures;
}

static int test_unexplored_route_keeps_hidden_wall_dependency(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	level_metadata_unexplored_route result;
	int failures = 0;

	test_reset();
	test_segment_explored[0] = 1;
	test_children[0][2] = 3;
	test_children[3][1] = 0;
	test_wall_nums[0][0] = 0;
	test_wall_nums[1][1] = 1;
	test_wall_type[0] = TEST_WALL_DOOR;
	test_wall_type[1] = TEST_WALL_DOOR;
	test_wall_key[0] = TEST_KEY_NONE;
	test_wall_key[1] = TEST_KEY_NONE;
	test_wall_clip_flags[0] = TEST_WALL_CLIP_HIDDEN;
	test_wall_clip_flags[1] = TEST_WALL_CLIP_HIDDEN;
	test_wall_seg[0] = 0;
	test_wall_sides[0] = 0;
	test_wall_seg[1] = 1;
	test_wall_sides[1] = 1;
	level_metadata_scan_unexplored_route(&view, &state, &result);
	failures += expect_string("hidden unexplored status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("hidden unexplored component", 2, result.component_size);
	failures += expect_int("hidden unexplored target", 1, result.target_seg);
	failures += expect_int("hidden unexplored waypoint", 0, result.waypoint_seg);
	failures += expect_int("hidden unexplored direct", 0, result.direct_reachable);
	failures += expect_int("hidden unexplored steps", 3, state.route_step_count);
	failures += expect_string("hidden unexplored blocker", "hidden_door", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_int("hidden unexplored wall", 0, state.route_steps[1].wall_num);
	failures += expect_string("hidden unexplored terminal", "unexplored", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	return failures;
}

static int test_unexplored_route_clears_target_when_fully_explored(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	level_metadata_unexplored_route result;
	int failures = 0;
	int seg;

	test_reset();
	for (seg = 0; seg < TEST_SEGMENTS; ++seg)
		test_segment_explored[seg] = 1;
	failures += expect_int("fully explored result", 0, level_metadata_scan_unexplored_route(&view, &state, &result));
	failures += expect_int("fully explored target", -1, result.target_seg);
	failures += expect_string("fully explored status", "failed", level_metadata_travel_status_name(state.route_status));
	failures += expect_string("fully explored problem", "no unexplored area", state.route_problem);
	return failures;
}

static int test_route_opens_control_center_links_after_reactor(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	test_wall_nums[1][0] = 0;
	test_wall_nums[2][1] = 1;
	test_wall_type[0] = TEST_WALL_CLOSED;
	test_wall_type[1] = TEST_WALL_CLOSED;
	test_wall_seg[0] = 1;
	test_wall_sides[0] = 0;
	test_wall_seg[1] = 2;
	test_wall_sides[1] = 1;
	test_control_center_link[1][0] = 1;
	test_control_center_link[2][1] = 1;
	test_object_count_value = 1;
	test_object_type[0] = TEST_OBJ_CONTROL_CENTER;
	test_object_seg[0] = 1;
	level_metadata_scan_level(&view, &state);
	failures += expect_string("control center link route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("control center link route steps", 3, state.route_step_count);
	failures += expect_string("control center link reactor", "reactor", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_string("control center link exit", "exit", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	level_metadata_scan_route_to_segment(&view, 2, &state);
	failures += expect_string("control center unexplored status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("control center unexplored steps", 3, state.route_step_count);
	failures += expect_string("control center unexplored reactor", "reactor", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_string("control center unexplored terminal", "unexplored", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	return failures;
}

static int test_route_accepts_any_fired_opener_for_side(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	test_wall_nums[1][0] = 0;
	test_wall_nums[2][1] = 1;
	test_wall_nums[2][2] = 2;
	test_wall_nums[0][2] = 3;
	test_wall_type[0] = TEST_WALL_CLOSED;
	test_wall_type[1] = TEST_WALL_CLOSED;
	test_wall_type[2] = TEST_WALL_OPEN;
	test_wall_type[3] = TEST_WALL_OPEN;
	test_wall_trigger[2] = 0;
	test_wall_trigger[3] = 1;
	test_wall_seg[0] = 1;
	test_wall_sides[0] = 0;
	test_wall_seg[1] = 2;
	test_wall_sides[1] = 1;
	test_wall_seg[2] = 2;
	test_wall_sides[2] = 2;
	test_wall_seg[3] = 0;
	test_wall_sides[3] = 2;
	test_trigger_link_count[0] = 2;
	test_trigger_link_seg[0][0] = 1;
	test_trigger_link_sides[0][0] = 0;
	test_trigger_link_seg[0][1] = 2;
	test_trigger_link_sides[0][1] = 1;
	test_trigger_link_count[1] = 2;
	test_trigger_link_seg[1][0] = 1;
	test_trigger_link_sides[1][0] = 0;
	test_trigger_link_seg[1][1] = 2;
	test_trigger_link_sides[1][1] = 1;
	view.triggered_side_opener_count = test_multi_opener_side_opener_count;
	view.triggered_side_opener_wall_num = test_multi_opener_side_opener_wall_num;
	view.trigger_type = test_trigger_type_at;
	view.trigger_link_count = test_trigger_link_count_at;
	view.trigger_link_segment = test_trigger_link_segment;
	view.trigger_link_side = test_trigger_link_side;
	level_metadata_scan_level(&view, &state);
	failures += expect_string("multi-opener route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("multi-opener route steps", 3, state.route_step_count);
	failures += expect_string("multi-opener route step", "trigger", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_string("multi-opener route activation", "fly_through_trigger", level_metadata_route_activation_kind_name(state.route_steps[1].activation_kind));
	failures += expect_int("multi-opener route trigger", 1, state.route_steps[1].trigger_num);
	failures += expect_string("multi-opener route exit", "exit", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	return failures;
}

static int test_route_hidden_door_step(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	test_wall_nums[0][0] = 0;
	test_wall_nums[1][1] = 1;
	test_wall_type[0] = TEST_WALL_DOOR;
	test_wall_type[1] = TEST_WALL_DOOR;
	test_wall_clip_flags[0] = TEST_WALL_CLIP_HIDDEN;
	test_wall_clip_flags[1] = TEST_WALL_CLIP_HIDDEN;
	test_wall_seg[0] = 0;
	test_wall_sides[0] = 0;
	test_wall_seg[1] = 1;
	test_wall_sides[1] = 1;
	level_metadata_scan_level(&view, &state);
	failures += expect_string("hidden door route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("hidden door route steps", 3, state.route_step_count);
	failures += expect_string("hidden door route step", "hidden_door", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_int("hidden door route segment", 0, state.route_steps[1].seg);
	failures += expect_int("hidden door route side", 0, state.route_steps[1].side);
	failures += expect_int("hidden door route wall", 0, state.route_steps[1].wall_num);
	failures += expect_string("hidden door route activation", "open_hidden_door", level_metadata_route_activation_kind_name(state.route_steps[1].activation_kind));
	failures += expect_int("hidden door route link count", 2, state.route_steps[1].opened_link_count);
	failures += expect_string("hidden door route exit", "exit", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	return failures;
}

static int test_route_visible_reactor_step(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	test_wall_nums[0][0] = 0;
	test_wall_nums[1][1] = 1;
	test_wall_type[0] = TEST_WALL_CLOSED;
	test_wall_type[1] = TEST_WALL_CLOSED;
	test_wall_seg[0] = 0;
	test_wall_sides[0] = 0;
	test_wall_seg[1] = 1;
	test_wall_sides[1] = 1;
	test_object_count_value = 1;
	test_object_type[0] = TEST_OBJ_CONTROL_CENTER;
	test_object_seg[0] = 1;
	view.side_has_exit_trigger = test_start_side_has_exit_trigger;
	view.target_visible_from_segment = test_target_visible_from_segment;
	level_metadata_scan_level(&view, &state);
	failures += expect_string("visible reactor route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("visible reactor route steps", 3, state.route_step_count);
	failures += expect_string("visible reactor route step", "reactor", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_string("visible reactor route activation", "destroy_reactor", level_metadata_route_activation_kind_name(state.route_steps[1].activation_kind));
	failures += expect_string("visible reactor route exit", "exit", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	return failures;
}

static int test_route_prefers_boss_over_control_center_segment(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	test_segment_special_values[1] = view.segment_special_control_center;
	test_object_count_value = 1;
	test_object_type[0] = TEST_OBJ_ROBOT;
	test_object_id[0] = TEST_ROBOT_BOSS;
	test_object_seg[0] = 1;
	level_metadata_scan_level(&view, &state);
	failures += expect_string("boss route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("boss route steps", 3, state.route_step_count);
	failures += expect_string("boss route step", "boss", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_string("boss route label", "Boss robot", state.route_steps[1].label);
	failures += expect_string("boss route activation", "destroy_boss", level_metadata_route_activation_kind_name(state.route_steps[1].activation_kind));
	failures += expect_string("boss route exit", "exit", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	return failures;
}

static int test_route_prefers_boss_over_reactor_object(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	test_object_count_value = 2;
	test_object_type[0] = TEST_OBJ_CONTROL_CENTER;
	test_object_seg[0] = 2;
	test_object_type[1] = TEST_OBJ_ROBOT;
	test_object_id[1] = TEST_ROBOT_BOSS;
	test_object_seg[1] = 1;
	level_metadata_scan_level(&view, &state);
	failures += expect_string("boss over reactor route status", "ok", level_metadata_travel_status_name(state.route_status));
	failures += expect_int("boss over reactor route steps", 3, state.route_step_count);
	failures += expect_string("boss over reactor route step", "boss", level_metadata_route_step_kind_name(state.route_steps[1].kind));
	failures += expect_string("boss over reactor route label", "Boss robot", state.route_steps[1].label);
	failures += expect_string("boss over reactor route activation", "destroy_boss", level_metadata_route_activation_kind_name(state.route_steps[1].activation_kind));
	failures += expect_string("boss over reactor route exit", "exit", level_metadata_route_step_kind_name(state.route_steps[2].kind));
	return failures;
}

static int test_guidebot_missing_note(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	view.object_is_companion = test_object_is_companion;
	level_metadata_scan_level(&view, &state);
	failures += expect_int("missing guidebot count", 0, state.guidebot_count);
	failures += expect_int("missing guidebot placed", 0, state.guidebot_placed);
	failures += expect_int("missing guidebot accessible", 0, state.guidebot_accessible);
	failures += expect_string(
	    "missing guidebot placement note",
	    "no guidebot or guidebot start cage placed in this level",
	    state.guidebot_placement_note);
	failures += expect_string("missing guidebot note", "", state.guidebot_note);
	return failures;
}

static int test_guidebot_accessible(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	view.object_is_companion = test_object_is_companion;
	test_object_count_value = 1;
	test_object_type[0] = TEST_OBJ_ROBOT;
	test_object_id[0] = TEST_ROBOT_GUIDEBOT;
	test_object_seg[0] = 1;
	level_metadata_scan_level(&view, &state);
	failures += expect_int("accessible guidebot count", 1, state.guidebot_count);
	failures += expect_int("accessible guidebot placed", 1, state.guidebot_placed);
	failures += expect_int("accessible guidebot flag", 1, state.guidebot_accessible);
	failures += expect_string(
	    "accessible guidebot placement note",
	    "guidebot or guidebot start cage placed in this level",
	    state.guidebot_placement_note);
	failures += expect_string("accessible guidebot note", "", state.guidebot_note);
	return failures;
}

static int test_guidebot_unreachable_note(void)
{
	level_metadata_scan_view view = test_view();
	level_metadata_state state;
	int failures = 0;

	test_reset();
	view.object_is_companion = test_object_is_companion;
	test_wall_nums[0][0] = 0;
	test_wall_nums[1][1] = 1;
	test_wall_type[0] = TEST_WALL_CLOSED;
	test_wall_type[1] = TEST_WALL_CLOSED;
	test_wall_seg[0] = 0;
	test_wall_sides[0] = 0;
	test_wall_seg[1] = 1;
	test_wall_sides[1] = 1;
	test_object_count_value = 1;
	test_object_type[0] = TEST_OBJ_ROBOT;
	test_object_id[0] = TEST_ROBOT_GUIDEBOT;
	test_object_seg[0] = 1;
	level_metadata_scan_level(&view, &state);
	failures += expect_int("unreachable guidebot count", 1, state.guidebot_count);
	failures += expect_int("unreachable guidebot placed", 1, state.guidebot_placed);
	failures += expect_int("unreachable guidebot accessible", 0, state.guidebot_accessible);
	failures += expect_string(
	    "unreachable guidebot placement note",
	    "guidebot or guidebot start cage placed in this level",
	    state.guidebot_placement_note);
	failures += expect_string("unreachable guidebot note", "guidebot is present but not reachable from the start", state.guidebot_note);
	return failures;
}

int main(void)
{
	int failures = 0;

	failures += test_reactorless_reachable_exit();
	failures += test_reactorless_missing_exit();
	failures += test_end_route_refresh_preserves_static_metadata();
	failures += test_route_key_step();
	failures += test_route_uses_initial_key_without_powerup();
	failures += test_route_key_uses_longer_open_path();
	failures += test_route_prefers_ordered_key_chain();
	failures += test_route_trigger_step();
	failures += test_route_shootable_trigger_step();
	failures += test_route_skips_already_opened_trigger_door();
	failures += test_route_does_not_reoffer_disabled_trigger();
	failures += test_route_promotes_unreachable_trigger_blocker();
	failures += test_segment_route_reuses_trigger_dependencies();
	failures += test_unexplored_route_acquires_key_for_largest_component();
	failures += test_unexplored_route_keeps_hidden_wall_dependency();
	failures += test_unexplored_route_clears_target_when_fully_explored();
	failures += test_route_opens_control_center_links_after_reactor();
	failures += test_route_accepts_any_fired_opener_for_side();
	failures += test_route_hidden_door_step();
	failures += test_route_visible_reactor_step();
	failures += test_route_prefers_boss_over_control_center_segment();
	failures += test_route_prefers_boss_over_reactor_object();
	failures += test_guidebot_missing_note();
	failures += test_guidebot_accessible();
	failures += test_guidebot_unreachable_note();
	if (failures)
		return 1;

	puts("PASS: level metadata scan");
	return 0;
}
