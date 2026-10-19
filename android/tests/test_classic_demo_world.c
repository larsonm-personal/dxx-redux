#include <limits.h>
#include <stdio.h>

#include "classic_demo_world.h"

static int fail(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

int main(void)
{
	int64_t target = -1;

	if (classic_demo_index_valid(-1, 8) || !classic_demo_index_valid(0, 8) ||
	    !classic_demo_index_valid(7, 8) || classic_demo_index_valid(8, 8) ||
	    classic_demo_index_valid(0, 0) || classic_demo_index_valid(INT_MAX, 8))
		return fail("bounded index validation failed");

	if (classic_demo_segment_valid(-1, 9) || !classic_demo_segment_valid(0, 9) ||
	    !classic_demo_segment_valid(9, 9) || classic_demo_segment_valid(10, 9))
		return fail("segment validation failed");

	if (!classic_demo_level_number_valid(-128, 0) ||
	    classic_demo_level_number_valid(0, 0) ||
	    !classic_demo_level_number_valid(0, 1) ||
	    !classic_demo_level_number_valid(127, 0))
		return fail("encoded level validation failed");

	if (!classic_demo_level_valid(-3, -3, 27, 0) ||
	    !classic_demo_level_valid(1, -3, 27, 0) ||
	    !classic_demo_level_valid(27, -3, 27, 0) ||
	    classic_demo_level_valid(-4, -3, 27, 0) ||
	    classic_demo_level_valid(0, -3, 27, 0) ||
	    !classic_demo_level_valid(0, -3, 27, 1) ||
	    classic_demo_level_valid(28, -3, 27, 0))
		return fail("mission level validation failed");

	if (!classic_demo_level_event_valid(-3, 27, -3, 27, 0) ||
	    classic_demo_level_event_valid(0, 1, -3, 27, 0) ||
	    !classic_demo_level_event_valid(0, 1, -3, 27, 1) ||
	    classic_demo_level_event_valid(1, 28, -3, 27, 0))
		return fail("level event validation failed");

	if (classic_demo_backward_link_target(100, 10, 0, &target) ||
	    !classic_demo_backward_link_target(100, 11, 0, &target) || target != 89 ||
	    !classic_demo_backward_link_target(100, 100, 0, &target) || target != 0 ||
	    classic_demo_backward_link_target(100, 101, 0, &target) ||
	    classic_demo_backward_link_target(9, 11, 10, &target) ||
	    classic_demo_backward_link_target(100, 11, 0, NULL))
		return fail("backward frame link validation failed");

	if (classic_demo_time_advance(100, -1) != 100 ||
	    classic_demo_time_advance(100, 0) != 100 ||
	    classic_demo_time_advance(100, 1) != 101 ||
	    classic_demo_time_advance(INT32_MAX - 1, 2) != INT32_MAX ||
	    classic_demo_frame_count_previous(1) != 0 ||
	    classic_demo_frame_count_previous(INT32_MIN) != INT32_MIN)
		return fail("frame timing arithmetic failed");

	if (classic_demo_int32_wrap_add(100, -2) != 98 ||
	    classic_demo_int32_wrap_add(INT32_MAX, 1) != INT32_MIN ||
	    classic_demo_int32_wrap_add(INT32_MIN, -1) != INT32_MAX ||
	    classic_demo_int32_wrap_subtract(100, -2) != 102 ||
	    classic_demo_int32_wrap_subtract(INT32_MIN, 1) != INT32_MAX ||
	    classic_demo_int32_wrap_subtract(INT32_MAX, -1) != INT32_MIN ||
	    classic_demo_int16_wrap_add(100, -2) != 98 ||
	    classic_demo_int16_wrap_add(INT16_MAX, 1) != INT16_MIN ||
	    classic_demo_int16_wrap_add(INT16_MIN, -1) != INT16_MAX ||
	    classic_demo_int16_wrap_subtract(100, -2) != 102 ||
	    classic_demo_int16_wrap_subtract(INT16_MIN, 1) != INT16_MAX ||
	    classic_demo_int16_wrap_subtract(INT16_MAX, -1) != INT16_MIN)
		return fail("classic counter arithmetic failed");

	{
		int32_t scaled = 0;

		if (!classic_demo_scaled_short_add(100, -2, 16, &scaled) || scaled != 68 ||
		    !classic_demo_scaled_short_add(INT32_MIN, 0, 16, &scaled) ||
		    scaled != INT32_MIN ||
		    classic_demo_scaled_short_add(INT32_MAX, 1, 1, &scaled) ||
		    classic_demo_scaled_short_add(INT32_MIN, -1, 1, &scaled) ||
		    classic_demo_scaled_short_add(0, 1, -1, &scaled) ||
		    classic_demo_scaled_short_add(0, 1, 1, NULL))
			return fail("scaled short coordinate validation failed");
	}

	if (!classic_demo_segment_side_valid(9, 9, 5, 6) ||
	    classic_demo_segment_side_valid(10, 9, 5, 6) ||
	    classic_demo_segment_side_valid(9, 9, -1, 6) ||
	    classic_demo_segment_side_valid(9, 9, 6, 6))
		return fail("segment-side validation failed");

	if (!classic_demo_wall_reference_valid(-1, 4) ||
	    !classic_demo_wall_reference_valid(3, 4) ||
	    classic_demo_wall_reference_valid(-2, 4) ||
	    classic_demo_wall_reference_valid(4, 4))
		return fail("wall reference validation failed");

	if (classic_demo_primary_texture_valid(-1, 3) ||
	    !classic_demo_primary_texture_valid(2, 3) ||
	    classic_demo_primary_texture_valid(3, 3))
		return fail("primary texture validation failed");

	if (!classic_demo_overlay_texture_valid(0, 3) ||
	    !classic_demo_overlay_texture_valid((short) 0x4002, 3) ||
	    classic_demo_overlay_texture_valid((short) 0x4003, 3) ||
	    classic_demo_overlay_texture_valid((short) 0xffff, 3))
		return fail("overlay texture validation failed");

	puts("PASS");
	return 0;
}
