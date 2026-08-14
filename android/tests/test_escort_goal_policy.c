#include "escort_goal_policy.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
	assert(!escort_goal_is_pathable(-1));
	assert(!escort_goal_is_pathable(0));
	assert(escort_goal_is_pathable(1));
	assert(escort_goal_is_pathable(25));
	puts("escort goal policy tests passed");
	return 0;
}
