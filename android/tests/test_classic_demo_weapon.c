#include <stdio.h>

#include "classic_demo_weapon.h"

#define D1_PRIMARY_WEAPONS   5
#define D1_SECONDARY_WEAPONS 5
#define D2_PRIMARY_WEAPONS   10
#define D2_SECONDARY_WEAPONS 10

static int check(int condition, const char *message)
{
	if (condition)
		return 1;
	fprintf(stderr, "classic demo weapon test failed: %s\n", message);
	return 0;
}

int main(void)
{
	if (!check(!classic_demo_weapon_index_valid(-1, D1_PRIMARY_WEAPONS), "negative index accepted") ||
	    !check(classic_demo_weapon_index_valid(0, D1_PRIMARY_WEAPONS), "zero index rejected") ||
	    !check(classic_demo_weapon_index_valid(D1_PRIMARY_WEAPONS - 1, D1_PRIMARY_WEAPONS),
	           "maximum D1 index rejected") ||
	    !check(!classic_demo_weapon_index_valid(D1_PRIMARY_WEAPONS, D1_PRIMARY_WEAPONS),
	           "oversized D1 index accepted") ||
	    !check(classic_demo_weapon_event_valid(0, 4, 0, 0,
	                                           D1_PRIMARY_WEAPONS, D1_SECONDARY_WEAPONS),
	           "valid shareware-layout primary event rejected") ||
	    !check(!classic_demo_weapon_event_valid(0, 5, 0, 0,
	                                            D1_PRIMARY_WEAPONS, D1_SECONDARY_WEAPONS),
	           "invalid shareware-layout primary event accepted") ||
	    !check(classic_demo_weapon_event_valid(1, 4, 4, 1,
	                                           D1_PRIMARY_WEAPONS, D1_SECONDARY_WEAPONS),
	           "valid registered secondary event rejected") ||
	    !check(!classic_demo_weapon_event_valid(1, 4, 5, 1,
	                                            D1_PRIMARY_WEAPONS, D1_SECONDARY_WEAPONS),
	           "invalid old secondary weapon accepted") ||
	    !check(classic_demo_weapon_event_valid(255, 9, 9, 1,
	                                           D2_PRIMARY_WEAPONS, D2_SECONDARY_WEAPONS),
	           "legacy nonzero secondary type rejected") ||
	    !check(!classic_demo_weapon_event_valid(255, 10, 9, 1,
	                                            D2_PRIMARY_WEAPONS, D2_SECONDARY_WEAPONS),
	           "oversized D2 secondary weapon accepted"))
		return 1;

	puts("classic demo weapon tests passed");
	return 0;
}
