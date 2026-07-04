/* android port: small math helpers for coop indicator line visibility */
#ifndef COOP_INDICATOR_LINES_MATH_H
#define COOP_INDICATOR_LINES_MATH_H

#include "gr.h"
#include "maths.h"

#define COOP_INDICATOR_LINE_FADE_LEVEL          14
#define COOP_INDICATOR_TARGET_SCREEN_MARGIN_DIV 4

static inline fix coop_indicator_line_advance_alpha(fix alpha, int should_show, fix frame_time)
{
	fix step;

	if (alpha < 0)
		alpha = 0;
	if (alpha > F1_0)
		alpha = F1_0;
	if (frame_time <= 0)
		step = 0;
	else if (frame_time >= F1_0)
		step = F1_0;
	else
		step = frame_time;

	if (should_show) {
		if (alpha >= F1_0 - step)
			return F1_0;
		return alpha + step;
	}
	if (alpha <= step)
		return 0;
	return alpha - step;
}

static inline int coop_indicator_line_fade_level(fix alpha)
{
	const int full_alpha_units =
	    (GR_FADE_LEVELS - 1) - COOP_INDICATOR_LINE_FADE_LEVEL;
	int alpha_units;

	if (alpha <= 0)
		return GR_FADE_OFF;
	if (alpha > F1_0)
		alpha = F1_0;

	alpha_units = (int) (((fix64) full_alpha_units * alpha + F1_0 / 2) / F1_0);
	if (alpha_units < 1)
		alpha_units = 1;
	return (GR_FADE_LEVELS - 1) - alpha_units;
}

static inline int coop_indicator_target_in_inner_screen(fix sx, fix sy,
                                                        int canvas_width,
                                                        int canvas_height)
{
	fix width, height, margin_x, margin_y;

	if (canvas_width <= 0 || canvas_height <= 0)
		return 0;

	width = i2f(canvas_width);
	height = i2f(canvas_height);
	margin_x = width / COOP_INDICATOR_TARGET_SCREEN_MARGIN_DIV;
	margin_y = height / COOP_INDICATOR_TARGET_SCREEN_MARGIN_DIV;
	return sx >= margin_x && sx <= width - margin_x &&
	       sy >= margin_y && sy <= height - margin_y;
}

#endif /* COOP_INDICATOR_LINES_MATH_H */
