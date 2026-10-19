#ifndef CLASSIC_DEMO_WEAPON_H
#define CLASSIC_DEMO_WEAPON_H

static inline int classic_demo_weapon_index_valid(int weapon_index, int weapon_count)
{
	return weapon_index >= 0 && weapon_index < weapon_count;
}

static inline int classic_demo_weapon_event_valid(int weapon_type, int weapon_index,
                                                  int old_weapon_index, int has_old_weapon, int primary_count, int secondary_count)
{
	int weapon_count = weapon_type ? secondary_count : primary_count;

	return classic_demo_weapon_index_valid(weapon_index, weapon_count) &&
	       (!has_old_weapon || classic_demo_weapon_index_valid(old_weapon_index, weapon_count));
}

#endif
