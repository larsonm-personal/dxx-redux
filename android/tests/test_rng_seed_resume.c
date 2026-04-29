#include <stdio.h>

#include "maths.h"

static int report_failure(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

int main(void)
{
	const unsigned int base_seed = 0x12345678u;
	const unsigned int resume_seed = 0x10203040u;
	const unsigned int fx_seed = 0x55667788u;
	unsigned int checkpoint_state = 0;
	unsigned int checkpoint_fx_state = 0;
	int replay_mode;
	int first_a;
	int second_a;
	int first_b;
	int second_b;
	int first_resume;
	int next_resume;
	int replayed_first_resume;
	int replayed_next_resume;
	int sim_before_fx;
	int fx_first;
	int fx_next;
	int fx_after_checkpoint;
	int sim_after_fx;
	int replayed_sim_after_fx;
	int replayed_fx_first;

	replay_mode = d_rand_get_replay_mode();
	if (replay_mode != D_RAND_REPLAY_MODE_LCG_STATE &&
		replay_mode != D_RAND_REPLAY_MODE_LIBC_RESEED)
		return report_failure("unexpected RNG replay mode");
	if (replay_mode == D_RAND_REPLAY_MODE_OUTPUT_LOG)
		return report_failure("output-log replay mode is not implemented yet");

	d_rand_reset_call_count();
	d_rand_reset_stream_call_count(D_RNG_FX);
	d_srand(base_seed);
	first_a = d_rand();
	second_a = d_rand();
	if (d_rand_get_call_count() != 2)
		return report_failure("call counter mismatch after two draws");

	d_rand_reset_call_count();
	d_rand_reset_stream_call_count(D_RNG_FX);
	d_srand(base_seed);
	first_b = d_rand();
	second_b = d_rand();
	if (d_rand_get_call_count() != 2)
		return report_failure("call counter mismatch after reseed");
	if (first_a != first_b || second_a != second_b)
		return report_failure("d_srand did not reproduce the base sequence");

	d_rand_reset_call_count();
	d_rand_reset_stream_call_count(D_RNG_FX);
	d_srand(resume_seed);
	d_srand_stream(D_RNG_FX, fx_seed);
	first_resume = d_rand();
	sim_before_fx = d_rand();
	fx_first = d_rand_stream(D_RNG_FX);
	fx_next = d_rand_stream(D_RNG_FX);
	sim_after_fx = d_rand();
	if (d_rand_get_call_count() != 3)
		return report_failure("sim call counter mismatch with fx draws present");
	if (d_rand_get_stream_call_count(D_RNG_FX) != 2)
		return report_failure("fx call counter mismatch after two fx draws");

#ifdef NO_WATCOM_RAND
	if (replay_mode != D_RAND_REPLAY_MODE_LIBC_RESEED)
		return report_failure("NO_WATCOM_RAND reported the wrong replay mode");
	if (d_rand_get_state(&checkpoint_state))
		return report_failure("NO_WATCOM_RAND unexpectedly exposed replayable state");
	if (d_rand_get_stream_state(D_RNG_FX, &checkpoint_fx_state))
		return report_failure("NO_WATCOM_RAND unexpectedly exposed FX replayable state");
	next_resume = d_rand();
	if (d_rand_set_state(checkpoint_state))
		return report_failure("NO_WATCOM_RAND unexpectedly restored RNG state");
	if (d_rand_set_stream_state(D_RNG_FX, checkpoint_fx_state))
		return report_failure("NO_WATCOM_RAND unexpectedly restored FX RNG state");
	d_srand(resume_seed);
	d_srand_stream(D_RNG_FX, fx_seed);
	replayed_first_resume = d_rand();
	replayed_sim_after_fx = d_rand();
	if (fx_first != d_rand_stream(D_RNG_FX))
		return report_failure("FX reseed did not reproduce the first FX draw");
	replayed_fx_first = d_rand_stream(D_RNG_FX);
	replayed_next_resume = d_rand();
	if (first_resume != replayed_first_resume || next_resume != replayed_next_resume)
		return report_failure("reseed did not reproduce the libc sequence prefix");
	if (sim_before_fx != replayed_sim_after_fx || fx_next != replayed_fx_first)
		return report_failure("FX draws changed the libc reseed reproduction order");

	puts("PASS: RNG reseed reproduces the sequence");
#else
	if (replay_mode != D_RAND_REPLAY_MODE_LCG_STATE)
		return report_failure("LCG build reported the wrong replay mode");
	if (!d_rand_get_state(&checkpoint_state))
		return report_failure("LCG path did not expose replayable state");
	if (!d_rand_get_stream_state(D_RNG_FX, &checkpoint_fx_state))
		return report_failure("LCG path did not expose FX replayable state");
	next_resume = d_rand();
	fx_after_checkpoint = d_rand_stream(D_RNG_FX);
	if (!d_rand_set_state(checkpoint_state))
		return report_failure("LCG path rejected saved RNG state");
	if (!d_rand_set_stream_state(D_RNG_FX, checkpoint_fx_state))
		return report_failure("LCG path rejected saved FX RNG state");
	replayed_next_resume = d_rand();
	replayed_fx_first = d_rand_stream(D_RNG_FX);
	if (first_resume == next_resume)
		return report_failure("resume probe failed to advance the sequence");
	if (next_resume != replayed_next_resume)
		return report_failure("restored state did not resume the sequence");
	if (d_rand_get_call_count() != 5)
		return report_failure("sim call counter changed after state restore");
	if (d_rand_get_stream_call_count(D_RNG_FX) != 4)
		return report_failure("fx call counter changed after fx state restore");
	if (fx_first == replayed_next_resume)
		return report_failure("fx stream unexpectedly matched a sim resume value");
	if (fx_after_checkpoint != replayed_fx_first)
		return report_failure("FX state restore did not resume the FX sequence");

	puts("PASS: RNG state restore reproduces both sequences");
#endif

	return 0;
}
