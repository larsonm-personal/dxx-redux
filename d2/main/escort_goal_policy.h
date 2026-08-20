#ifndef D2_ESCORT_GOAL_POLICY_H
#define D2_ESCORT_GOAL_POLICY_H

/* ESCORT_GOAL_UNSPECIFIED is negative; all pathable Guide-Bot goals are positive. */
static inline int escort_goal_is_pathable(int goal_object)
{
	return goal_object > 0;
}

/* Special commands are applied by escort_create_path_to_goal after selection. */
static inline int escort_goal_request_is_pathable(int goal_object, int special_goal)
{
	return escort_goal_is_pathable(special_goal != -1 ? special_goal : goal_object);
}

/* A short path can be the complete route to a nearby door or objective. */
static inline int escort_path_needs_fallback(int path_length)
{
	return path_length < 2;
}

#endif
