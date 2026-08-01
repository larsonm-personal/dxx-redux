#include "secret_area_scan.h"

#include <stdio.h>
#include <string.h>

typedef struct test_level {
	int candidate_count;
	int opener_count;
	unsigned long long child_calls;
	unsigned long long object_segment_calls;
	unsigned long long opener_calls;
} test_level;

static secret_area_state Test_state;

static int test_segment_child(void *user, int seg, int side)
{
	test_level *level = (test_level *) user;
	int candidates = level->candidate_count;

	level->child_calls++;
	if (seg < candidates) {
		if (side == 0 && seg + 1 < candidates)
			return seg + 1;
		if (side == 1 && seg > 0)
			return seg - 1;
		if (side == 2)
			return candidates + seg;
	} else if (seg < candidates * 2 && side == 0) {
		return seg - candidates;
	}
	return -1;
}

static int test_reverse_side(void *user, int seg, int child)
{
	test_level *level = (test_level *) user;
	int candidates = level->candidate_count;

	if (seg < candidates && child == candidates + seg)
		return 0;
	if (seg >= candidates && child == seg - candidates)
		return 2;
	if (child == seg + 1)
		return 1;
	if (child == seg - 1)
		return 0;
	return -1;
}

static int test_wall_num(void *user, int seg, int side)
{
	test_level *level = (test_level *) user;
	int candidates = level->candidate_count;

	if (seg < candidates && side == 2)
		return seg;
	if (seg >= candidates && seg < candidates * 2 && side == 0)
		return seg - candidates;
	return -1;
}

static int test_wall_type(void *user, int wall_num)
{
	test_level *level = (test_level *) user;
	return wall_num < level->candidate_count ? 1 : 2;
}

static int test_wall_zero(void *user, int wall_num)
{
	(void) user;
	(void) wall_num;
	return 0;
}

static int test_segment_zero(void *user, int seg)
{
	(void) user;
	(void) seg;
	return 0;
}

static int test_segment_center(void *user, int seg, int xyz[3])
{
	(void) user;
	xyz[0] = seg;
	xyz[1] = seg * 2;
	xyz[2] = -seg;
	return 1;
}

static int test_object_count(void *user)
{
	return ((test_level *) user)->candidate_count;
}

static int test_object_segment(void *user, int objnum)
{
	test_level *level = (test_level *) user;
	level->object_segment_calls++;
	return level->candidate_count + objnum;
}

static int test_object_powerup(void *user, int objnum)
{
	(void) user;
	(void) objnum;
	return 3;
}

static int test_object_id(void *user, int objnum)
{
	(void) user;
	(void) objnum;
	return 100;
}

static const char *test_powerup_name(void *user, int id)
{
	(void) user;
	(void) id;
	return "Energy";
}

static int test_opener_count(void *user, int seg, int side)
{
	test_level *level = (test_level *) user;
	return seg == 0 && side == 2 ? level->opener_count : 0;
}

static int test_opener_segment(void *user, int seg, int side, int index)
{
	test_level *level = (test_level *) user;
	(void) seg;
	(void) side;
	(void) index;
	level->opener_calls++;
	return 0;
}

static int test_opener_side(void *user, int seg, int side, int index)
{
	(void) user;
	(void) seg;
	(void) side;
	(void) index;
	return 0;
}

static int test_opener_wall(void *user, int seg, int side, int index)
{
	test_level *level = (test_level *) user;
	(void) seg;
	(void) side;
	return level->candidate_count + index;
}

static secret_area_scan_view test_view(test_level *level)
{
	secret_area_scan_view view;

	memset(&view, 0, sizeof(view));
	view.num_segments = level->candidate_count * 2;
	view.num_walls = level->candidate_count + level->opener_count;
	view.start_segment = 0;
	view.max_generated = SECRET_AREA_MAX_GENERATED;
	view.wall_type_door = 1;
	view.wall_type_open = 2;
	view.wall_key_none = 0;
	view.wall_clip_hidden = 1;
	view.obj_type_none = 0;
	view.obj_type_robot = 1;
	view.obj_type_hostage = 2;
	view.obj_type_powerup = 3;
	view.obj_type_control_center = 4;
	view.powerup_key_blue = 10;
	view.powerup_key_red = 11;
	view.powerup_key_gold = 12;
	view.segment_special_control_center = 1;
	view.segment_special_robotmaker = 2;
	view.user = level;
	view.segment_child = test_segment_child;
	view.reverse_side = test_reverse_side;
	view.wall_num = test_wall_num;
	view.wall_type = test_wall_type;
	view.wall_flags = test_wall_zero;
	view.wall_keys = test_wall_zero;
	view.wall_clip_flags = test_wall_type;
	view.segment_special = test_segment_zero;
	view.segment_center = test_segment_center;
	view.object_count = test_object_count;
	view.object_segment = test_object_segment;
	view.object_type = test_object_powerup;
	view.object_id = test_object_id;
	view.powerup_name = test_powerup_name;
	view.triggered_side_opener_count = test_opener_count;
	view.triggered_side_opener_segment = test_opener_segment;
	view.triggered_side_opener_side = test_opener_side;
	view.triggered_side_opener_wall_num = test_opener_wall;
	return view;
}

static int run_case(int candidate_count, int expected_result)
{
	test_level level;
	secret_area_scan_view view;
	int result;

	memset(&level, 0, sizeof(level));
	level.candidate_count = candidate_count;
	view = test_view(&level);
	result = secret_area_scan_level(&view, &Test_state);
	if (result != expected_result ||
	    Test_state.raw_candidate_count != candidate_count) {
		fprintf(stderr,
		        "%d-candidate result mismatch: result=%d raw=%d final=%d reason=%d\n",
		        candidate_count, result, Test_state.raw_candidate_count,
		        Test_state.final_candidate_count, Test_state.disabled_reason);
		return 1;
	}
	if (candidate_count <= SECRET_AREA_MAX_GENERATED) {
		if (!Test_state.enabled ||
		    Test_state.disabled_reason != SECRET_AREA_DISABLED_NONE ||
		    Test_state.final_candidate_count != candidate_count)
			return 1;
	} else if (Test_state.enabled ||
	           Test_state.disabled_reason !=
	               SECRET_AREA_DISABLED_TOO_MANY_CANDIDATES ||
	           Test_state.final_candidate_count !=
	               SECRET_AREA_MAX_GENERATED + 1) {
		return 1;
	}
	if (level.child_calls > (unsigned long long) view.num_segments * 500 ||
	    level.object_segment_calls >
	        (unsigned long long) candidate_count * 2) {
		fprintf(stderr,
		        "%d-candidate work was not linear: child=%llu object_segment=%llu\n",
		        candidate_count, level.child_calls,
		        level.object_segment_calls);
		return 1;
	}
	return 0;
}

static int run_opener_case(int opener_count)
{
	test_level level;
	secret_area_scan_view view;
	int result;

	memset(&level, 0, sizeof(level));
	level.candidate_count = SECRET_AREA_MAX_GENERATED;
	level.opener_count = opener_count;
	view = test_view(&level);
	result = secret_area_scan_level(&view, &Test_state);
	if (result != SECRET_AREA_MAX_GENERATED - 1 ||
	    level.opener_calls > (unsigned long long) opener_count * 8 + 20) {
		fprintf(stderr,
		        "%d-opener result was not linear: result=%d opener_calls=%llu\n",
		        opener_count, result, level.opener_calls);
		return 1;
	}
	return 0;
}

int main(void)
{
	if (sizeof(secret_area_state) >= 512 * 1024) {
		fprintf(stderr, "Secret-area state is too large: %llu bytes\n",
		        (unsigned long long) sizeof(secret_area_state));
		return 1;
	}
	if (run_case(SECRET_AREA_MAX_GENERATED, SECRET_AREA_MAX_GENERATED) ||
	    run_case(450, 0) || run_case(4500, 0) || run_opener_case(10) ||
	    run_opener_case(100) || run_opener_case(254))
		return 1;
	printf("Secret-area bounded scaling tests passed\n");
	return 0;
}
