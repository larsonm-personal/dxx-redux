#include <stdio.h>

#include "level_statistics.hpp"

struct test_side
{
	short wall_num;
	short tmap_num;
	short tmap_num2;
};

struct test_segment
{
	short children[6];
	test_side sides[6];
};

static int check_equal(int actual, int expected, const char *label)
{
	if (actual == expected)
		return 1;
	fprintf(stderr, "FAIL: %s expected %d, got %d\n", label, expected, actual);
	return 0;
}

int main(void)
{
	test_segment segments[2] = {};
	for (test_segment &segment : segments)
		for (int side = 0; side < 6; ++side) {
			segment.children[side] = 1;
			segment.sides[side].wall_num = -1;
		}

	segments[0].children[0] = -1;
	segments[0].sides[0].tmap_num = 4;
	segments[0].sides[0].tmap_num2 = static_cast<short>((2 << 14) | 7);
	segments[0].children[1] = -1;
	segments[0].sides[1].tmap_num = 4;
	segments[0].sides[1].tmap_num2 = 0;
	segments[0].sides[2].tmap_num = 8;
	segments[0].sides[2].tmap_num2 = 9;
	segments[0].sides[3].wall_num = 1;
	segments[0].sides[3].tmap_num = 10;
	segments[0].sides[3].tmap_num2 = 7;
	segments[0].sides[4].wall_num = 3;
	segments[0].sides[4].tmap_num = 11;
	segments[1].children[0] = -1;
	segments[1].sides[0].tmap_num = 99;
	segments[1].sides[0].tmap_num2 = 100;

	const level_statistics statistics =
		collect_level_statistics(segments, 2, 3, 5, 12, 100);
	if (!check_equal(statistics.segment_count, 2, "segments") ||
	    !check_equal(statistics.wall_count, 3, "walls") ||
	    !check_equal(statistics.trigger_count, 5, "triggers") ||
	    !check_equal(statistics.object_count, 12, "objects") ||
	    !check_equal(statistics.texture_count, 4, "textures"))
		return 1;

	const level_statistics empty =
		collect_level_statistics<test_segment>(NULL, -1, -2, -3, -4, 0);
	if (!check_equal(empty.segment_count, 0, "empty segments") ||
	    !check_equal(empty.wall_count, 0, "empty walls") ||
	    !check_equal(empty.trigger_count, 0, "empty triggers") ||
	    !check_equal(empty.object_count, 0, "empty objects") ||
	    !check_equal(empty.texture_count, 0, "empty textures"))
		return 1;

	puts("PASS: level statistics");
	return 0;
}
