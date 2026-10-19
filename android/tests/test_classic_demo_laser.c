#include <stdio.h>

#include "classic_demo_laser.h"

#define D1_MAX_LASER_LEVEL 3
#define D2_MAX_LASER_LEVEL 5

static int check(int condition, const char *message)
{
	if (condition)
		return 1;
	fprintf(stderr, "classic demo laser test failed: %s\n", message);
	return 0;
}

int main(void)
{
	if (!check(!classic_demo_laser_level_valid(-1, D1_MAX_LASER_LEVEL),
	           "negative D1 laser level accepted") ||
	    !check(classic_demo_laser_level_valid(0, D1_MAX_LASER_LEVEL),
	           "zero D1 laser level rejected") ||
	    !check(classic_demo_laser_level_valid(D1_MAX_LASER_LEVEL, D1_MAX_LASER_LEVEL),
	           "maximum D1 laser level rejected") ||
	    !check(!classic_demo_laser_level_valid(D1_MAX_LASER_LEVEL + 1, D1_MAX_LASER_LEVEL),
	           "oversized D1 laser level accepted") ||
	    !check(classic_demo_laser_level_valid(D2_MAX_LASER_LEVEL, D2_MAX_LASER_LEVEL),
	           "maximum D2 laser level rejected") ||
	    !check(!classic_demo_laser_level_valid(D2_MAX_LASER_LEVEL + 1, D2_MAX_LASER_LEVEL),
	           "oversized D2 laser level accepted") ||
	    !check(classic_demo_laser_event_valid(0, D1_MAX_LASER_LEVEL, D1_MAX_LASER_LEVEL),
	           "valid D1 laser event rejected") ||
	    !check(!classic_demo_laser_event_valid(-1, 0, D1_MAX_LASER_LEVEL),
	           "negative old D1 laser level accepted") ||
	    !check(!classic_demo_laser_event_valid(0, D1_MAX_LASER_LEVEL + 1,
	                                           D1_MAX_LASER_LEVEL),
	           "oversized new D1 laser level accepted"))
		return 1;

	puts("classic demo laser tests passed");
	return 0;
}
