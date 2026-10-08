#ifndef HUD_SCORE_SHARED_H
#define HUD_SCORE_SHARED_H

#include <stdio.h>

/* Try these only after the original labeled score fails to fit */
static inline int hud_score_format_fallback(char *text, size_t size, int score, int format)
{
	if (format == 1)
		snprintf(text, size, "%d", score);
	else if (format == 2 && (score >= 1000000 || score <= -1000000)) {
		int tenth = (score % 1000000) / 100000;
		if (tenth < 0)
			tenth = -tenth;
		snprintf(text, size, "%d.%dM", score / 1000000, tenth);
	} else if (format == 2 && (score >= 1000 || score <= -1000))
		snprintf(text, size, "%dk", score / 1000);
	else
		return 0;
	return 1;
}

#endif
