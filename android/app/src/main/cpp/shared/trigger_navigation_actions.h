#ifndef DXX_TRIGGER_NAVIGATION_ACTIONS_H
#define DXX_TRIGGER_NAVIGATION_ACTIONS_H

#include "switch.h"
#include "level_metadata_scan.h"
#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2/d1_in_d2_levels.h"
#define TRIGGER_NAVIGATION_TOGGLE_DOOR (-7)
#else
#define TRIGGER_NAVIGATION_TOGGLE_DOOR TRIGGER_CONTROL_DOORS
#endif

/* Native D1 clears ON for one-shots but still admits subsequent crossings */
static inline int trigger_navigation_flags(int trigger_num)
{
	if (trigger_num < 0 || trigger_num >= Num_triggers) return 0;
#ifdef DXX_BUILD_DESCENT_II
	short source_flags;
	return d1_in_d2_trigger_source_flags(&Triggers[trigger_num], &source_flags)
	           ? Triggers[trigger_num].flags & ~TF_ONE_SHOT
	           : Triggers[trigger_num].flags;
#else
	return Triggers[trigger_num].flags & ~TRIGGER_ONE_SHOT;
#endif
}

/* Ordered navigation actions, independent of activation admission flags */
static inline int trigger_navigation_action_types(int trigger_num, int types[LEVEL_METADATA_MAX_TRIGGER_ACTIONS])
{
	int count = 0;
	short flags;
	if (trigger_num < 0 || trigger_num >= Num_triggers)
		return 0;
#ifdef DXX_BUILD_DESCENT_II
	if (!d1_in_d2_trigger_source_flags(&Triggers[trigger_num], &flags)) {
		types[0] = Triggers[trigger_num].type;
		return 1;
	}
	if (flags & TRIGGER_EXIT) types[count++] = TT_EXIT;
	if (flags & TRIGGER_SECRET_EXIT) types[count++] = TT_SECRET_EXIT;
	if (flags & TRIGGER_CONTROL_DOORS) types[count++] = TRIGGER_NAVIGATION_TOGGLE_DOOR;
	if (flags & TRIGGER_ILLUSION_ON) types[count++] = TT_ILLUSION_ON;
	if (flags & TRIGGER_ILLUSION_OFF) types[count++] = TT_ILLUSION_OFF;
#else
	flags = Triggers[trigger_num].flags;
	if (flags & TRIGGER_EXIT) types[count++] = TRIGGER_EXIT;
	if (flags & TRIGGER_SECRET_EXIT) types[count++] = TRIGGER_SECRET_EXIT;
	if (flags & TRIGGER_CONTROL_DOORS) types[count++] = TRIGGER_CONTROL_DOORS;
	if (flags & TRIGGER_ILLUSION_ON) types[count++] = TRIGGER_ILLUSION_ON;
	if (flags & TRIGGER_ILLUSION_OFF) types[count++] = TRIGGER_ILLUSION_OFF;
#endif
	return count;
}

static inline int trigger_navigation_opens_links(int trigger_num)
{
	short flags;
	if (trigger_num < 0 || trigger_num >= Num_triggers) return 0;
#ifdef DXX_BUILD_DESCENT_II
	if (!d1_in_d2_trigger_source_flags(&Triggers[trigger_num], &flags)) {
		const int type = Triggers[trigger_num].type;
		return type == TT_OPEN_DOOR || type == TT_ILLUSION_OFF || type == TT_UNLOCK_DOOR ||
		       type == TT_OPEN_WALL || type == TT_ILLUSORY_WALL;
	}
#else
	flags = Triggers[trigger_num].flags;
#endif
	if (flags & (TRIGGER_EXIT | TRIGGER_SECRET_EXIT)) return 0;
	if (flags & TRIGGER_ILLUSION_OFF) return 1;
	if (flags & TRIGGER_ILLUSION_ON) return 0;
	return (flags & TRIGGER_CONTROL_DOORS) != 0;
}

#endif
