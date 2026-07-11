#include "escort_exit_policy.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
	assert(escort_exit_segment_preferred(252, 38) == 252);
	assert(escort_exit_segment_preferred(-1, 38) == 38);
	assert(escort_exit_segment_preferred(252, -1) == 252);
	assert(escort_exit_segment_preferred(-1, -1) == -1);
	puts("escort exit policy tests passed");
	return 0;
}
