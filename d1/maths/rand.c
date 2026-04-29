// Descent random number stuff...
// rand has different ranges on different machines...

#define DXX_D_RAND_NO_ANNOTATION 1

#include <stdlib.h>
#include "maths.h"
#include "input_demo_rng_trace.h"

static unsigned int d_rand_call_count;

#ifndef NO_WATCOM_RAND
static unsigned int d_rand_state;
#endif

#ifdef NO_WATCOM_RAND
static void d_srand_internal(unsigned int seed)
{
	srand(seed);
}

int d_rand_get_replay_mode(void)
{
	return D_RAND_REPLAY_MODE_LIBC_RESEED;
}

static int d_rand_internal(void)
{
	d_rand_call_count++;
	return rand() & 0x7fff;
}

int d_rand_get_state(unsigned int *state)
{
	(void)state;
	return 0;
}

int d_rand_set_state(unsigned int state)
{
	(void)state;
	return 0;
}
#else
int d_rand_get_replay_mode(void)
{
	return D_RAND_REPLAY_MODE_LCG_STATE;
}

static int d_rand_internal(void)
{
	d_rand_call_count++;
	return ((d_rand_state = d_rand_state * 0x41c64e6d + 0x3039) >> 16) & 0x7fff;
}

static void d_srand_internal(unsigned int seed)
{
	d_rand_state = seed;
}

int d_rand_get_state(unsigned int *state)
{
	if (!state)
		return 0;
	*state = d_rand_state;
	return 1;
}

int d_rand_set_state(unsigned int state)
{
	d_rand_state = state;
	return 1;
}
#endif

int d_rand(void)
{
	return d_rand_internal();
}

int d_rand_annotated(const char *file, const char *func, int line)
{
	unsigned int state_before = 0;
	unsigned int state_after = 0;
	int has_state_before = d_rand_get_state(&state_before);
	int result = d_rand_internal();
	int has_state_after = d_rand_get_state(&state_after);

	input_demo_rng_trace_record_rand(file,
		func,
		line,
		d_rand_call_count,
		has_state_before,
		state_before,
		has_state_after,
		state_after,
		result);
	return result;
}

void d_srand(unsigned int seed)
{
	d_srand_internal(seed);
}

void d_srand_annotated(unsigned int seed, const char *file, const char *func, int line)
{
	unsigned int state_before = 0;
	unsigned int state_after = 0;
	int has_state_before = d_rand_get_state(&state_before);

	d_srand_internal(seed);
	input_demo_rng_trace_record_srand(file,
		func,
		line,
		d_rand_call_count,
		has_state_before,
		state_before,
		d_rand_get_state(&state_after),
		state_after,
		seed);
}

unsigned int d_rand_get_call_count(void)
{
	return d_rand_call_count;
}

void d_rand_reset_call_count(void)
{
	d_rand_call_count = 0;
}
