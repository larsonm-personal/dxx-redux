#include <string.h>

#include "input_demo_rng_mode.h"

int input_demo_rng_mode_parse(const char *text)
{
	if (!text)
		return 0;
	if (!strcmp(text, "lcg_state"))
		return D_RAND_REPLAY_MODE_LCG_STATE;
	if (!strcmp(text, "libc_reseed"))
		return D_RAND_REPLAY_MODE_LIBC_RESEED;
	if (!strcmp(text, "output_log"))
		return D_RAND_REPLAY_MODE_OUTPUT_LOG;
	return 0;
}

const char *input_demo_rng_mode_name(int mode)
{
	switch (mode)
	{
		case D_RAND_REPLAY_MODE_LCG_STATE:
			return "lcg_state";
		case D_RAND_REPLAY_MODE_LIBC_RESEED:
			return "libc_reseed";
		case D_RAND_REPLAY_MODE_OUTPUT_LOG:
			return "output_log";
		default:
			return "invalid";
	}
}

int input_demo_rng_mode_is_compatible(int fixture_mode, int engine_mode)
{
	if (fixture_mode == D_RAND_REPLAY_MODE_OUTPUT_LOG)
		return 0;
	if (fixture_mode != D_RAND_REPLAY_MODE_LCG_STATE &&
		fixture_mode != D_RAND_REPLAY_MODE_LIBC_RESEED)
		return 0;
	return fixture_mode == engine_mode;
}