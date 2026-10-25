#pragma once
#include <stdint.h>

/* Sample the existing strict-negative eclip clock, without advancing simulation.
 * A zero time_left still belongs to the preceding frame in do_special_effects. */
static inline int dxx_xl_animation_frame(int stock_frames, int frame_time, int frame,
                                         int time_left, int stopped, int source_frames)
{
	if (stock_frames < 1 || stock_frames > 30 || frame_time <= 0 || frame < 0 || frame >= stock_frames ||
	    time_left < 0 || time_left > frame_time || source_frames < 1 || source_frames > 128)
		return -1;
	if (stopped) return 0;
	const int64_t period = (int64_t) stock_frames * frame_time;
	const int64_t phase = (int64_t) frame * frame_time + frame_time - time_left;
	return (int) ((phase > 0 ? phase - 1 : 0) * source_frames / period);
}
