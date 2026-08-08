#include <stdio.h>
#include <string.h>

#include "merged_wall_geometry_hit.h"

static int check_outcome(const char *profile, const char *case_name,
                         int fate, int hit_wall, int hit_bad_p0,
                         int hit_side_seg, int hit_seg, int hit_side,
                         enum merged_wall_geometry_outcome expected_outcome,
                         int expected_seg, int expected_side, const char *expected_status)
{
	int segnum = 99;
	int sidenum = 99;
	enum merged_wall_geometry_outcome outcome =
	    merged_wall_classify_geometry_hit(fate, hit_wall, hit_bad_p0,
	                                      hit_side_seg, hit_seg, hit_side,
	                                      9, 6, &segnum, &sidenum);
	const char *status = merged_wall_geometry_outcome_status(outcome);

	if (outcome != expected_outcome || segnum != expected_seg ||
	    sidenum != expected_side || strcmp(status, expected_status) != 0) {
		fprintf(stderr,
		        "%s %s failed: outcome=%d seg=%d side=%d status=%s\n",
		        profile, case_name, (int) outcome, segnum, sidenum, status);
		return 0;
	}
	return 1;
}

static int check_profile(const char *profile, int hit_none, int hit_wall, int hit_bad_p0)
{
	return check_outcome(profile, "wall", hit_wall, hit_wall, hit_bad_p0,
	                     4, 2, 3, MERGED_WALL_GEOMETRY_WALL_HIT, 4, 3, "hit") &&
	       check_outcome(profile, "wall-hit-segment", hit_wall, hit_wall, hit_bad_p0,
	                     -1, 2, 5, MERGED_WALL_GEOMETRY_WALL_HIT, 2, 5, "hit") &&
	       check_outcome(profile, "none", hit_none, hit_wall, hit_bad_p0,
	                     4, 2, 3, MERGED_WALL_GEOMETRY_NO_WALL, -1, -1, "no_wall") &&
	       check_outcome(profile, "bad-start", hit_bad_p0, hit_wall, hit_bad_p0,
	                     4, 2, 3, MERGED_WALL_GEOMETRY_BAD_STARTSEG, -1, -1, "bad_startseg") &&
	       check_outcome(profile, "invalid-segment", hit_wall, hit_wall, hit_bad_p0,
	                     10, 2, 3, MERGED_WALL_GEOMETRY_INVALID_WALL_HIT, -1, -1,
	                     "invalid_wall_hit") &&
	       check_outcome(profile, "invalid-side", hit_wall, hit_wall, hit_bad_p0,
	                     4, 2, 6, MERGED_WALL_GEOMETRY_INVALID_WALL_HIT, -1, -1,
	                     "invalid_wall_hit");
}

int main(void)
{
	if (!check_profile("d1", 0, 1, 3) || !check_profile("d2", 0, 1, 3))
		return 1;
	puts("merged wall geometry hit tests passed");
	return 0;
}
