#ifndef CLASSIC_DEMO_WORLD_H
#define CLASSIC_DEMO_WORLD_H

#include <stdint.h>

#define CLASSIC_DEMO_TMAP_NUM_MASK     0x3fff
#define CLASSIC_DEMO_FRAME_HEADER_SIZE 11

static inline int classic_demo_index_valid(int index, int count)
{
	return index >= 0 && index < count;
}

static inline int classic_demo_segment_valid(int segment, int highest_segment_index)
{
	return segment >= 0 && segment <= highest_segment_index;
}

static inline int classic_demo_level_number_valid(int level, int allow_zero)
{
	return level != 0 || allow_zero;
}

static inline int classic_demo_level_valid(int level, int last_secret_level,
                                           int last_level, int allow_zero)
{
	return classic_demo_level_number_valid(level, allow_zero) &&
	       level >= last_secret_level && level <= last_level;
}

static inline int classic_demo_level_event_valid(int old_level, int new_level,
                                                 int last_secret_level,
                                                 int last_level, int allow_zero)
{
	return classic_demo_level_valid(old_level, last_secret_level, last_level, allow_zero) &&
	       classic_demo_level_valid(new_level, last_secret_level, last_level, allow_zero);
}

static inline int classic_demo_backward_link_target(int64_t origin, int link_length,
                                                    int64_t minimum_target,
                                                    int64_t *target)
{
	if (!target || origin < minimum_target ||
	    link_length < CLASSIC_DEMO_FRAME_HEADER_SIZE ||
	    (int64_t) link_length > origin - minimum_target)
		return 0;
	*target = origin - link_length;
	return 1;
}

static inline int32_t classic_demo_time_advance(int32_t total, int32_t delta)
{
	if (delta <= 0)
		return total;
	if (total > INT32_MAX - delta)
		return INT32_MAX;
	return total + delta;
}

static inline int32_t classic_demo_frame_count_previous(int32_t frame_count)
{
	return frame_count == INT32_MIN ? INT32_MIN : frame_count - 1;
}

static inline int32_t classic_demo_int32_wrap_add(int32_t value, int32_t delta)
{
	const uint32_t wrapped = (uint32_t) value + (uint32_t) delta;

	return wrapped <= INT32_MAX
	           ? (int32_t) wrapped
	           : INT32_MIN + (int32_t) (wrapped - ((uint32_t) INT32_MAX + 1u));
}

static inline int32_t classic_demo_int32_wrap_subtract(int32_t value, int32_t delta)
{
	const uint32_t wrapped = (uint32_t) value - (uint32_t) delta;

	return wrapped <= INT32_MAX
	           ? (int32_t) wrapped
	           : INT32_MIN + (int32_t) (wrapped - ((uint32_t) INT32_MAX + 1u));
}

static inline int16_t classic_demo_int16_wrap_add(int16_t value, int32_t delta)
{
	const uint16_t wrapped = (uint16_t) ((uint16_t) value + (uint16_t) delta);

	return wrapped <= INT16_MAX
	           ? (int16_t) wrapped
	           : INT16_MIN + (int16_t) (wrapped - ((uint16_t) INT16_MAX + 1u));
}

static inline int16_t classic_demo_int16_wrap_subtract(int16_t value, int32_t delta)
{
	const uint16_t wrapped = (uint16_t) ((uint16_t) value - (uint16_t) delta);

	return wrapped <= INT16_MAX
	           ? (int16_t) wrapped
	           : INT16_MIN + (int16_t) (wrapped - ((uint16_t) INT16_MAX + 1u));
}

static inline int classic_demo_scaled_short_add(int32_t base, int16_t encoded,
                                                int32_t scale, int32_t *result)
{
	const int64_t value = (int64_t) base + (int64_t) encoded * scale;

	if (!result || scale < 0 || value < INT32_MIN || value > INT32_MAX)
		return 0;
	*result = (int32_t) value;
	return 1;
}

static inline int classic_demo_segment_side_valid(int segment, int highest_segment_index,
                                                  int side, int side_count)
{
	return classic_demo_segment_valid(segment, highest_segment_index) &&
	       classic_demo_index_valid(side, side_count);
}

static inline int classic_demo_wall_reference_valid(int wall, int wall_count)
{
	return wall == -1 || classic_demo_index_valid(wall, wall_count);
}

static inline int classic_demo_primary_texture_valid(int texture, int texture_count)
{
	return classic_demo_index_valid(texture, texture_count);
}

static inline int classic_demo_overlay_texture_valid(int texture, int texture_count)
{
	return classic_demo_index_valid((uint16_t) texture & CLASSIC_DEMO_TMAP_NUM_MASK,
	                                texture_count);
}

#endif
