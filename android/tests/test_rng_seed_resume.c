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
	unsigned int checkpoint_state = 0;
	int first_a;
	int second_a;
	int first_b;
	int second_b;
	int first_resume;
	int next_resume;
	int replayed_first_resume;
	int replayed_next_resume;

	d_rand_reset_call_count();
	d_srand(base_seed);
	first_a = d_rand();
	second_a = d_rand();
	if (d_rand_get_call_count() != 2)
		return report_failure("call counter mismatch after two draws");

	d_rand_reset_call_count();
	d_srand(base_seed);
	first_b = d_rand();
	second_b = d_rand();
	if (d_rand_get_call_count() != 2)
		return report_failure("call counter mismatch after reseed");
	if (first_a != first_b || second_a != second_b)
		return report_failure("d_srand did not reproduce the base sequence");

	d_rand_reset_call_count();
	d_srand(resume_seed);
	first_resume = d_rand();

#ifdef NO_WATCOM_RAND
	if (d_rand_get_state(&checkpoint_state))
		return report_failure("NO_WATCOM_RAND unexpectedly exposed replayable state");
	next_resume = d_rand();
	if (d_rand_set_state(checkpoint_state))
		return report_failure("NO_WATCOM_RAND unexpectedly restored RNG state");
	d_srand(resume_seed);
	replayed_first_resume = d_rand();
	replayed_next_resume = d_rand();
	if (first_resume != replayed_first_resume || next_resume != replayed_next_resume)
		return report_failure("reseed did not reproduce the libc sequence prefix");

	puts("PASS: RNG reseed reproduces the sequence");
#else
	if (!d_rand_get_state(&checkpoint_state))
		return report_failure("LCG path did not expose replayable state");
	next_resume = d_rand();
	if (!d_rand_set_state(checkpoint_state))
		return report_failure("LCG path rejected saved RNG state");
	replayed_next_resume = d_rand();
	if (first_resume == next_resume)
		return report_failure("resume probe failed to advance the sequence");
	if (next_resume != replayed_next_resume)
		return report_failure("restored state did not resume the sequence");

	puts("PASS: RNG state restore reproduces the sequence");
#endif

	return 0;
}
