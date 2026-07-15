#ifndef HOMING_COMPAT_H
#define HOMING_COMPAT_H

#include <stdint.h>

#define HOMING_COMPAT_F1_0               65536
#define HOMING_COMPAT_REFERENCE_FPS      25
#define HOMING_COMPAT_D1_ACQUISITION_DOT (3 * HOMING_COMPAT_F1_0 / 4)
#define HOMING_COMPAT_D2_ACQUISITION_DOT (7 * HOMING_COMPAT_F1_0 / 8)

static inline int homing_compat_original_enabled(int player_setting,
                                                 int netgame_setting, int game_mode, int multi_mask, int coop_mask)
{
	if (!(game_mode & multi_mask))
		return player_setting != 0;
	return (game_mode & coop_mask) && netgame_setting;
}

static inline int32_t homing_compat_acquisition_dot(int original_homing,
                                                    int d1_gameplay, int32_t redux_dot)
{
	return original_homing && d1_gameplay ? HOMING_COMPAT_D1_ACQUISITION_DOT : redux_dot;
}

static inline int32_t homing_compat_d1_retention_dot(int32_t frame_time,
                                                     int32_t min_trackable_dot)
{
	if (frame_time <= HOMING_COMPAT_F1_0 / 16)
		return 3 * (HOMING_COMPAT_F1_0 - min_trackable_dot) / 4 +
		       min_trackable_dot;
	if (frame_time < HOMING_COMPAT_F1_0 / 4)
		return (int32_t) (((int64_t) (HOMING_COMPAT_F1_0 - min_trackable_dot) *
		                   (HOMING_COMPAT_F1_0 - 4 * frame_time)) >>
		                  16) +
		       min_trackable_dot;
	return min_trackable_dot;
}

static inline int32_t homing_compat_d2_original_retention_dot(int32_t frame_time,
                                                              int32_t min_trackable_dot)
{
	if (frame_time <= HOMING_COMPAT_F1_0 / 64)
		return min_trackable_dot;
	if (frame_time < HOMING_COMPAT_F1_0 / 32)
		return min_trackable_dot + HOMING_COMPAT_F1_0 / 64 - 2 * frame_time;
	if (frame_time < HOMING_COMPAT_F1_0 / 4)
		return min_trackable_dot + HOMING_COMPAT_F1_0 / 64 -
		       HOMING_COMPAT_F1_0 / 16 - frame_time;
	return min_trackable_dot + HOMING_COMPAT_F1_0 / 64 - HOMING_COMPAT_F1_0 / 8;
}

#endif
