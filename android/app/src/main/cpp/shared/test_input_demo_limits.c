#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include "input_demo_limits.h"

static int fail(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

int main(void)
{
	if (!input_demo_checkpoint_size_supported(0) ||
	    !input_demo_checkpoint_size_supported(INPUT_DEMO_CHECKPOINT_MAX_BYTES) ||
	    input_demo_checkpoint_size_supported(INPUT_DEMO_CHECKPOINT_MAX_BYTES + 1u) ||
	    input_demo_checkpoint_size_supported(UINT32_MAX))
		return fail("checkpoint size boundary mismatch");
	if (!input_demo_checkpoint_encoded_size_supported(INPUT_DEMO_CHECKPOINT_MAX_ENCODED_BYTES) ||
	    input_demo_checkpoint_encoded_size_supported(INPUT_DEMO_CHECKPOINT_MAX_ENCODED_BYTES + 1u))
		return fail("checkpoint encoded size boundary mismatch");
	if (!input_demo_checkpoint_expansion_supported(1024, 1) ||
	    !input_demo_checkpoint_expansion_supported(2048, 2) ||
	    input_demo_checkpoint_expansion_supported(1025, 1) ||
	    input_demo_checkpoint_expansion_supported(1, 0))
		return fail("checkpoint expansion boundary mismatch");
	if (!input_demo_file_size_supported(INPUT_DEMO_FILE_MAX_BYTES) ||
	    input_demo_file_size_supported((uint64_t) INPUT_DEMO_FILE_MAX_BYTES + 1u))
		return fail("demo file size boundary mismatch");
	if (!input_demo_level_in_mission(1, 24, -3) ||
	    !input_demo_level_in_mission(24, 24, -3) ||
	    !input_demo_level_in_mission(-1, 24, -3) ||
	    !input_demo_level_in_mission(-3, 24, -3) ||
	    input_demo_level_in_mission(0, 24, -3) ||
	    input_demo_level_in_mission(25, 24, -3) ||
	    input_demo_level_in_mission(-4, 24, -3) ||
	    input_demo_level_in_mission(INT_MIN, 24, -3) ||
	    input_demo_level_in_mission(INT_MAX, 24, -3))
		return fail("mission level boundary mismatch");
	puts("PASS");
	return 0;
}
