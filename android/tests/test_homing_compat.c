#include <stdio.h>

#include "homing_compat.h"

#define CHECK(expression) \
	do { \
		if (!(expression)) { \
			fprintf(stderr, "Check failed: %s\n", #expression); \
			return 1; \
		} \
	} while (0)

enum {
	GM_NETWORK = 2,
	GM_MULTI_ROBOTS = 8,
	GM_MULTI_COOP = 16,
	GM_MULTI = 38,
};

int main(void)
{
	const int32_t frame_time_25 = HOMING_COMPAT_F1_0 / HOMING_COMPAT_REFERENCE_FPS;
	const int32_t d1_min_dot = HOMING_COMPAT_D1_ACQUISITION_DOT;
	const int32_t d2_min_dot = HOMING_COMPAT_D2_ACQUISITION_DOT;

	CHECK(homing_compat_original_enabled(1, 0, 0, GM_MULTI, GM_MULTI_COOP));
	CHECK(!homing_compat_original_enabled(0, 1, 0, GM_MULTI, GM_MULTI_COOP));
	CHECK(homing_compat_original_enabled(0, 1,
		GM_NETWORK | GM_MULTI_ROBOTS | GM_MULTI_COOP, GM_MULTI, GM_MULTI_COOP));
	CHECK(!homing_compat_original_enabled(1, 1,
		GM_NETWORK, GM_MULTI, GM_MULTI_COOP));

	CHECK(homing_compat_acquisition_dot(1, 1, d2_min_dot) == d1_min_dot);
	CHECK(homing_compat_acquisition_dot(0, 1, d2_min_dot) == d2_min_dot);
	CHECK(homing_compat_acquisition_dot(1, 0, d2_min_dot) == d2_min_dot);
	CHECK(homing_compat_d1_retention_dot(frame_time_25, d1_min_dot) == 61440);
	CHECK(homing_compat_d2_original_retention_dot(frame_time_25, d2_min_dot) == 51651);
	CHECK(homing_compat_d1_retention_dot(frame_time_25, d2_min_dot) == 63488);
	return 0;
}
