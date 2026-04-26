/*
 *
 * Descent random number stuff...
 * rand has different ranges on different machines...
 *
 */

#include <stdlib.h>
#include "maths.h"

static unsigned int d_rand_call_count;

#ifndef NO_WATCOM_RAND
static unsigned int d_rand_state;
#endif

#ifdef NO_WATCOM_RAND

void d_srand(unsigned int seed)
{
	srand(seed);
}

int d_rand_get_replay_mode(void)
{
	return D_RAND_REPLAY_MODE_LIBC_RESEED;
}

int d_rand()
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

int d_rand()
{
	d_rand_call_count++;
	return ((d_rand_state = d_rand_state * 0x41c64e6d + 0x3039) >> 16) & 0x7fff;
}

void d_srand(unsigned int seed)
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

unsigned int d_rand_get_call_count(void)
{
	return d_rand_call_count;
}

void d_rand_reset_call_count(void)
{
	d_rand_call_count = 0;
}
