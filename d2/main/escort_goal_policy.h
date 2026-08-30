#ifndef D2_ESCORT_GOAL_POLICY_H
#define D2_ESCORT_GOAL_POLICY_H

#define ESCORT_PATH_RECALC_LIMIT_PER_SECOND 4

typedef struct escort_path_recalc_limiter {
	long long timestamps[ESCORT_PATH_RECALC_LIMIT_PER_SECOND];
	unsigned int count;
} escort_path_recalc_limiter;

static inline void escort_path_recalc_limiter_reset(escort_path_recalc_limiter *limiter)
{
	limiter->count = 0;
}

static inline int escort_path_recalc_limiter_allow(escort_path_recalc_limiter *limiter,
	long long now, long long interval, long long *next_allowed)
{
	unsigned int expired = 0;
	unsigned int i;

	if (limiter->count && now < limiter->timestamps[0])
		escort_path_recalc_limiter_reset(limiter);
	while (expired < limiter->count &&
	       now - limiter->timestamps[expired] >= interval)
		expired++;
	if (expired) {
		for (i = expired; i < limiter->count; i++)
			limiter->timestamps[i - expired] = limiter->timestamps[i];
		limiter->count -= expired;
	}
	if (limiter->count >= ESCORT_PATH_RECALC_LIMIT_PER_SECOND) {
		if (next_allowed)
			*next_allowed = limiter->timestamps[0] + interval;
		return 0;
	}
	limiter->timestamps[limiter->count++] = now;
	if (next_allowed)
		*next_allowed = now;
	return 1;
}

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

static inline int escort_audit_replan_should_defer(
	int audit_only, int last_valid, long long now, long long last,
	long long interval)
{
	return audit_only && last_valid && now >= last && now - last < interval;
}

#endif
