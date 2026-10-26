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

/* Wclips are driven by the applied wall mapping, including saved/reversed state.
 * Restrict source counts so every authored frame is represented without a new clock. */
static inline int dxx_xl_wclip_frame(int stock_frames, int stock_frame, int source_frames)
{
	if (stock_frames < 1 || stock_frames > 50 || stock_frame < 0 || stock_frame >= stock_frames ||
	    source_frames < 1 || source_frames > stock_frames) return -1;
	return stock_frames == 1 ? 0 : stock_frame * (source_frames - 1) / (stock_frames - 1);
}

#if defined(ANDROID) && defined(DXX_BUILD_DESCENT_II) && defined(DXX_XL_RUNTIME) && defined(OGL_MERGE)
/* Renderer-only observation of the clock actually applied to a wall mapping */
void dxx_xl_wall_mapped_phase(int effect, int bitmap, int frames, int frame_time,
                              int frame, int time_left, int stopped);
#endif
