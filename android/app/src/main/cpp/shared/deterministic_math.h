#ifndef DXX_DETERMINISTIC_MATH_H
#define DXX_DETERMINISTIC_MATH_H

#include "maths.h"

#define DXX_AI_PATH_SMOOTHING_BASE_FRAME_TIME (F1_0 / 30)

static inline fix dxx_ai_path_smoothing_delta(fix component, fix frame_time)
{
	return fixmuldiv(component / 2, frame_time, DXX_AI_PATH_SMOOTHING_BASE_FRAME_TIME);
}

#endif