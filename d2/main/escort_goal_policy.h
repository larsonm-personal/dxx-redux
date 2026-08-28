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

/* A short path can be the complete route to a nearby door or objective. */
static inline int escort_path_needs_fallback(int path_length, int semantic_route_active)
{
	return path_length < (semantic_route_active ? 1 : 2);
}

/* Semantic routes own their physical path until the player is genuinely lost. */
static inline int escort_semantic_route_suppresses_midpoint_visit(
	int semantic_route_active, int going_to_object, int lost_player_timeout)
{
	return semantic_route_active && going_to_object && !lost_player_timeout;
}

static inline int escort_path_has_remaining_point(int path_length,
	int current_path_index, int path_direction)
{
	int next_path_index = current_path_index + path_direction;

	return path_length > 0 && current_path_index >= 0 &&
	       current_path_index < path_length && next_path_index >= 0 &&
	       next_path_index < path_length;
}

/* Unlike classic patrol paths, a completed semantic route must not reverse. */
static inline int escort_semantic_route_holds_endpoint(
	int semantic_route_active, int going_to_object, int path_remaining)
{
	return semantic_route_active && going_to_object && !path_remaining;
}

static inline int escort_audit_replan_should_defer(
	int audit_only, int last_valid, long long now, long long last,
	long long interval)
{
	return audit_only && last_valid && now >= last && now - last < interval;
}

#endif
