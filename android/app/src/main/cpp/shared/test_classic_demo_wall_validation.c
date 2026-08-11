#include "classic_demo_wall_validation.h"

#include <limits.h>
#include <stdio.h>

#define TEST_MAX_WALLS 254

static int fail(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

int main(void)
{
	int count;

	if (classic_demo_wall_records_fit(-1, TEST_MAX_WALLS, 0) ||
	    !classic_demo_wall_records_fit(0, TEST_MAX_WALLS, 0) ||
	    classic_demo_wall_records_fit(TEST_MAX_WALLS + 1, TEST_MAX_WALLS, INT64_MAX) ||
	    classic_demo_wall_records_fit(INT_MAX, TEST_MAX_WALLS, INT64_MAX))
		return fail("wall count domain validation failed");
	for (count = 1; count <= TEST_MAX_WALLS; count++) {
		int64_t required = (int64_t) count * CLASSIC_DEMO_WALL_RECORD_BYTES;

		if (classic_demo_wall_records_fit(count, TEST_MAX_WALLS, required - 1) ||
		    !classic_demo_wall_records_fit(count, TEST_MAX_WALLS, required))
			return fail("wall record truncation boundary failed");
	}
	if (classic_demo_wall_records_fit(1, TEST_MAX_WALLS, -1))
		return fail("negative remaining length was accepted");
	puts("PASS");
	return 0;
}
