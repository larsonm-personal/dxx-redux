#ifndef BOSS_HEALTH_SHARED_H
#define BOSS_HEALTH_SHARED_H

#include <stdint.h>

int32_t boss_health_maximum_for_difficulty(int32_t strength, int difficulty);
void difficulty_health_rescale_live_robots(int old_difficulty,
                                           int new_difficulty);

static inline int32_t d2_thief_or_companion_health_maximum(
    int32_t strength, int level_num, int difficulty, int companion)
{
	int32_t maximum;

	// Boost shield for Thief and Buddy based on level.
	if (level_num < 0)
		level_num = -level_num;
	maximum = strength * (level_num + 7) / 8;
	if (!companion)
		return maximum;
	// Now, scale guide-bot hits by skill level
	if (difficulty == 0)
		return 20000 * 65536; // Trainee, basically unkillable
	if (difficulty == 1)
		return maximum * 3; // Rookie, pretty dang hard
	if (difficulty == 2)
		return maximum * 2; // Hotshot, a bit tough
	return maximum;
}

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
