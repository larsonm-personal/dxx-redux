#include <assert.h>

#include "homing_compat.h"

enum {
	GM_NETWORK = 2,
	GM_MULTI_ROBOTS = 8,
	GM_MULTI_COOP = 16,
	GM_MULTI = 38,
};

int main(void)
{
	const int32_t frame_time_25 = HOMING_COMPAT_F1_0 / HOMING_COMPAT_REFERENCE_FPS;
	const int32_t d1_min_dot = 3 * HOMING_COMPAT_F1_0 / 4;
	const int32_t d2_min_dot = 7 * HOMING_COMPAT_F1_0 / 8;

	assert(homing_compat_original_enabled(1, 0, 0, GM_MULTI, GM_MULTI_COOP));
	assert(!homing_compat_original_enabled(0, 1, 0, GM_MULTI, GM_MULTI_COOP));
	assert(homing_compat_original_enabled(0, 1,
		GM_NETWORK | GM_MULTI_ROBOTS | GM_MULTI_COOP, GM_MULTI, GM_MULTI_COOP));
	assert(!homing_compat_original_enabled(1, 1,
		GM_NETWORK, GM_MULTI, GM_MULTI_COOP));

	assert(homing_compat_d1_retention_dot(frame_time_25, d1_min_dot) == 62914);
	assert(homing_compat_d2_original_retention_dot(frame_time_25, d2_min_dot) == 51610);
	assert(homing_compat_d1_retention_dot(frame_time_25, d2_min_dot) == 64225);
	return 0;
}
