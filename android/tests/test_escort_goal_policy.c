#include "escort_goal_policy.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
	escort_path_recalc_limiter limiter = {{0}, 0};
	long long next_allowed = -1;

	assert(!escort_goal_is_pathable(-1));
	assert(!escort_goal_is_pathable(0));
	assert(escort_goal_is_pathable(1));
	assert(escort_goal_is_pathable(25));
	assert(!escort_goal_request_is_pathable(-1, -1));
	assert(escort_goal_request_is_pathable(-1, 7));
	assert(escort_goal_request_is_pathable(5, -1));
	assert(escort_path_needs_fallback(-1, 0));
	assert(escort_path_needs_fallback(0, 0));
	assert(escort_path_needs_fallback(1, 0));
	assert(!escort_path_needs_fallback(2, 0));
	assert(!escort_path_needs_fallback(3, 0));
	assert(escort_path_needs_fallback(0, 1));
	assert(!escort_path_needs_fallback(1, 1));
	assert(escort_semantic_route_suppresses_midpoint_visit(1, 1, 0));
	assert(!escort_semantic_route_suppresses_midpoint_visit(1, 1, 1));
	assert(!escort_semantic_route_suppresses_midpoint_visit(1, 0, 0));
	assert(!escort_semantic_route_suppresses_midpoint_visit(0, 1, 0));
	assert(escort_path_has_remaining_point(2, 0, 1));
	assert(!escort_path_has_remaining_point(2, 1, 1));
	assert(!escort_path_has_remaining_point(1, 0, 1));
	assert(escort_path_has_remaining_point(2, 1, -1));
	assert(!escort_path_has_remaining_point(2, 0, -1));
	assert(!escort_path_has_remaining_point(0, 0, 1));
	assert(!escort_audit_replan_should_defer(0, 1, 120, 100, 50));
	assert(!escort_audit_replan_should_defer(1, 0, 120, 100, 50));
	assert(escort_audit_replan_should_defer(1, 1, 120, 100, 50));
	assert(!escort_audit_replan_should_defer(1, 1, 150, 100, 50));
	assert(!escort_audit_replan_should_defer(1, 1, 90, 100, 50));
	assert(escort_path_recalc_limiter_allow(&limiter, 0, 1000, &next_allowed));
	assert(escort_path_recalc_limiter_allow(&limiter, 100, 1000, &next_allowed));
	assert(escort_path_recalc_limiter_allow(&limiter, 200, 1000, &next_allowed));
	assert(escort_path_recalc_limiter_allow(&limiter, 300, 1000, &next_allowed));
	assert(!escort_path_recalc_limiter_allow(&limiter, 999, 1000, &next_allowed));
	assert(next_allowed == 1000);
	assert(escort_path_recalc_limiter_allow(&limiter, 1000, 1000, &next_allowed));
	assert(!escort_path_recalc_limiter_allow(&limiter, 1099, 1000, &next_allowed));
	assert(next_allowed == 1100);
	escort_path_recalc_limiter_reset(&limiter);
	assert(escort_path_recalc_limiter_allow(&limiter, 500, 1000, &next_allowed));
	assert(escort_path_recalc_limiter_allow(&limiter, 100, 1000, &next_allowed));
	assert(limiter.count == 1);
	puts("escort goal policy tests passed");
	return 0;
}
