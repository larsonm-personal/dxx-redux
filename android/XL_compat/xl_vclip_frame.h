#ifndef DXX_XL_VCLIP_FRAME_H
#define DXX_XL_VCLIP_FRAME_H
#include <stdint.h>

/* CurFrame uses the authored period even when the replacement count differs */
static inline int dxx_xl_vclip_lifetime_frame(int total_time, int frame_time, int frames, int timeleft)
{
	int64_t frame;
	if (total_time <= 0 || frame_time <= 0 || frames <= 0) return -1;
	if (timeleft > total_time) timeleft %= total_time;
	frame = ((int64_t) total_time - timeleft) / frame_time;
	return frame < frames ? (int) frame : frames - 1;
}
#endif
