#ifndef D2_ESCORT_GOAL_POLICY_H
#define D2_ESCORT_GOAL_POLICY_H

/* ESCORT_GOAL_UNSPECIFIED is negative; all pathable Guide-Bot goals are positive. */
static inline int escort_goal_is_pathable(int goal_object)
{
	return goal_object > 0;
}

#endif
