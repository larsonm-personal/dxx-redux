#include <stdio.h>
#include <string.h>

#include "input_demo_rng_mode.h"

static int report_failure(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

int main(void)
{
	int engine_mode = d_rand_get_replay_mode();

	if (input_demo_rng_mode_parse("lcg_state") != D_RAND_REPLAY_MODE_LCG_STATE)
		return report_failure("failed to parse lcg_state rng_mode");
	if (input_demo_rng_mode_parse("libc_reseed") != D_RAND_REPLAY_MODE_LIBC_RESEED)
		return report_failure("failed to parse libc_reseed rng_mode");
	if (input_demo_rng_mode_parse("output_log") != D_RAND_REPLAY_MODE_OUTPUT_LOG)
		return report_failure("failed to parse output_log rng_mode");
	if (input_demo_rng_mode_parse("per_frame_seed") != 0)
		return report_failure("unexpectedly accepted legacy per_frame_seed rng_mode");
	if (strcmp(input_demo_rng_mode_name(D_RAND_REPLAY_MODE_LCG_STATE), "lcg_state"))
		return report_failure("wrong lcg_state name");
	if (strcmp(input_demo_rng_mode_name(D_RAND_REPLAY_MODE_LIBC_RESEED), "libc_reseed"))
		return report_failure("wrong libc_reseed name");
	if (strcmp(input_demo_rng_mode_name(D_RAND_REPLAY_MODE_OUTPUT_LOG), "output_log"))
		return report_failure("wrong output_log name");
	if (strcmp(input_demo_rng_mode_name(0), "invalid"))
		return report_failure("wrong invalid rng_mode name");
	if (input_demo_rng_mode_is_compatible(D_RAND_REPLAY_MODE_OUTPUT_LOG, engine_mode))
		return report_failure("output_log rng_mode unexpectedly marked compatible");

#ifdef NO_WATCOM_RAND
	if (engine_mode != D_RAND_REPLAY_MODE_LIBC_RESEED)
		return report_failure("NO_WATCOM_RAND reported the wrong engine replay mode");
	if (!input_demo_rng_mode_is_compatible(D_RAND_REPLAY_MODE_LIBC_RESEED, engine_mode))
		return report_failure("libc_reseed rng_mode should match NO_WATCOM_RAND builds");
	if (input_demo_rng_mode_is_compatible(D_RAND_REPLAY_MODE_LCG_STATE, engine_mode))
		return report_failure("lcg_state rng_mode should not match NO_WATCOM_RAND builds");

	puts("PASS: input demo rng_mode matches libc reseed backend");
#else
	if (engine_mode != D_RAND_REPLAY_MODE_LCG_STATE)
		return report_failure("LCG build reported the wrong engine replay mode");
	if (!input_demo_rng_mode_is_compatible(D_RAND_REPLAY_MODE_LCG_STATE, engine_mode))
		return report_failure("lcg_state rng_mode should match LCG builds");
	if (input_demo_rng_mode_is_compatible(D_RAND_REPLAY_MODE_LIBC_RESEED, engine_mode))
		return report_failure("libc_reseed rng_mode should not match LCG builds");

	puts("PASS: input demo rng_mode matches LCG backend");
#endif

	return 0;
}