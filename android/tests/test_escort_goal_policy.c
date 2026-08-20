#include "escort_goal_policy.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
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
	puts("escort goal policy tests passed");
	return 0;
}
