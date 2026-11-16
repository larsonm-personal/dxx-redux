#ifndef DXX_XL_VCLIP_FRAME_H
#define DXX_XL_VCLIP_FRAME_H
#include <stdint.h>

/* CurFrame uses the authored period even when the replacement count differs */
static inline int dxx_xl_vclip_object_frame(int total_time, int frame_time, int frames, int timeleft,
                                            int object_total, int object_period)
{
	int64_t frame;
	if (total_time <= 0 || frame_time <= 0 || frames <= 0) return -1;
	if (timeleft > total_time) timeleft %= total_time;
	if (object_period) {
		if (object_total < 0 || object_period < 0) return -1;
		total_time = object_total;
		frame_time = object_period;
	}
	frame = ((int64_t) total_time - timeleft) / frame_time;
	return frame < frames ? (int) frame : frames - 1;
}
static inline int dxx_xl_vclip_lifetime_frame(int total_time, int frame_time, int frames, int timeleft)
{
	return dxx_xl_vclip_object_frame(total_time, frame_time, frames, timeleft, 0, 0);
}
#endif
