#ifndef BOSS_HEALTH_SHARED_H
#define BOSS_HEALTH_SHARED_H

#include <stdint.h>

int32_t boss_health_maximum_for_difficulty(int32_t strength, int difficulty);
void boss_health_rescale_live_bosses(int old_difficulty, int new_difficulty);

static inline int32_t boss_health_rescale_value(int32_t shields,
                                                int32_t old_maximum,
                                                int32_t new_maximum)
{
	int64_t scaled;

	if (shields <= 0 || old_maximum <= 0 || new_maximum <= 0 ||
	    old_maximum == new_maximum)
		return shields;
	scaled = ((int64_t) shields * new_maximum + old_maximum / 2) /
	         old_maximum;
	if (scaled <= 0)
		return 1;
	if (scaled > INT32_MAX)
		return INT32_MAX;
	return (int32_t) scaled;
}

#endif
