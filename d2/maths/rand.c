/*
 *
 * Descent random number stuff...
 * rand has different ranges on different machines...
 *
 */

#define DXX_D_RAND_NO_ANNOTATION 1

#include <stdio.h>
#include <stdlib.h>
#include "maths.h"
#include "input_demo_rng_trace.h"

static unsigned int d_rand_call_count[D_RNG_STREAM_COUNT];

#ifndef NO_WATCOM_RAND
static unsigned int d_rand_state[D_RNG_STREAM_COUNT];
#endif

static int d_rng_stream_is_valid(d_rng_stream stream)
{
	return stream >= D_RNG_SIM && stream < D_RNG_STREAM_COUNT;
}

#ifdef NO_WATCOM_RAND

static void d_srand_internal(d_rng_stream stream, unsigned int seed)
{
	(void)stream;
	srand(seed);
}

int d_rand_get_replay_mode(void)
{
	return d_rand_get_replay_mode_for_stream(D_RNG_SIM);
}

int d_rand_get_replay_mode_for_stream(d_rng_stream stream)
{
	if (!d_rng_stream_is_valid(stream))
		return D_RAND_REPLAY_MODE_LIBC_RESEED;
	return D_RAND_REPLAY_MODE_LIBC_RESEED;
}

static int d_rand_internal(d_rng_stream stream)
{
	if (!d_rng_stream_is_valid(stream))
		stream = D_RNG_SIM;
	d_rand_call_count[stream]++;
	return rand() & 0x7fff;
}

int d_rand_get_stream_state(d_rng_stream stream, unsigned int *state)
{
	(void)stream;
	(void)state;
	return 0;
}

int d_rand_set_stream_state(d_rng_stream stream, unsigned int state)
{
	(void)stream;
	(void)state;
	#include "maths.h" // Ensure maths.h is included for the new functions
	return 0;
}

#else

int d_rand_get_replay_mode(void)
{
	return d_rand_get_replay_mode_for_stream(D_RNG_SIM);
}

int d_rand_get_replay_mode_for_stream(d_rng_stream stream)
{
	if (!d_rng_stream_is_valid(stream))
		return D_RAND_REPLAY_MODE_LCG_STATE;
	return D_RAND_REPLAY_MODE_LCG_STATE;
}

static int d_rand_internal(d_rng_stream stream)
{
	if (!d_rng_stream_is_valid(stream))
		stream = D_RNG_SIM;
	d_rand_call_count[stream]++;
	return ((d_rand_state[stream] = d_rand_state[stream] * 0x41c64e6d + 0x3039) >> 16) & 0x7fff;
}

static void d_srand_internal(d_rng_stream stream, unsigned int seed)
{
	if (!d_rng_stream_is_valid(stream))
		stream = D_RNG_SIM;
	d_rand_state[stream] = seed;
}

int d_rand_get_stream_state(d_rng_stream stream, unsigned int *state)
{
	if (!d_rng_stream_is_valid(stream) || !state)
		return 0;
	*state = d_rand_state[stream];
	return 1;
}

int d_rand_set_stream_state(d_rng_stream stream, unsigned int state)
{
	if (!d_rng_stream_is_valid(stream))
		return 0;
	d_rand_state[stream] = state;
	return 1;
}

#endif

/* android port: tracks whether we're inside d_rand_annotated to detect untraced callers */
static int g_in_d_rand_annotated;

#if defined(DXX_HEADLESS_CONSOLE) && defined(_MSC_VER)
/* MSVC intrinsic: returns address of the instruction after the call in the caller */
#pragma intrinsic(_ReturnAddress)
__declspec(noinline)
#endif
int d_rand(void)
{
	return d_rand_stream(D_RNG_SIM);
}

int d_rand_stream(d_rng_stream stream)
{
#if defined(DXX_HEADLESS_CONSOLE) && defined(_MSC_VER)
	if (stream == D_RNG_SIM && !g_in_d_rand_annotated && input_demo_rng_trace_is_active()) {
		void *caller = _ReturnAddress();
		unsigned int cur_state = 0;
		d_rand_get_stream_state(D_RNG_SIM, &cur_state);
		fprintf(stderr,
			"[rng-gap] untraced d_rand_stream(sim) call: state=%u caller=%p\n",
			cur_state,
			caller);
	}
#endif
	return d_rand_internal(stream);
}

int d_rand_annotated(d_rng_stream stream, const char *file, const char *func, int line)
{
	unsigned int state_before = 0;
	unsigned int state_after = 0;
	int has_state_before = d_rand_get_stream_state(stream, &state_before);
	g_in_d_rand_annotated = 1;
	int result = d_rand_internal(stream);
	g_in_d_rand_annotated = 0;
	int has_state_after = d_rand_get_stream_state(stream, &state_after);
	unsigned int call_count = d_rand_get_stream_call_count(stream);

	input_demo_rng_trace_record_rand((int) stream,
		file,
		func,
		line,
		call_count,
		has_state_before,
		state_before,
		has_state_after,
		state_after,
		result);
	return result;
}

void d_srand(unsigned int seed)
{
	d_srand_stream(D_RNG_SIM, seed);
}

void d_srand_stream(d_rng_stream stream, unsigned int seed)
{
	d_srand_internal(stream, seed);
}

void d_srand_annotated(d_rng_stream stream, unsigned int seed, const char *file, const char *func, int line)
{
	unsigned int state_before = 0;
	unsigned int state_after = 0;
	int has_state_before = d_rand_get_stream_state(stream, &state_before);

	d_srand_internal(stream, seed);
	input_demo_rng_trace_record_srand((int) stream,
		file,
		func,
		line,
		d_rand_get_stream_call_count(stream),
		has_state_before,
		state_before,
		d_rand_get_stream_state(stream, &state_after),
		state_after,
		seed);
}

int d_rand_get_state(unsigned int *state)
{
	return d_rand_get_stream_state(D_RNG_SIM, state);
}

int d_rand_set_state(unsigned int state)
{
	return d_rand_set_stream_state(D_RNG_SIM, state);
}

unsigned int d_rand_get_call_count(void)
{
	return d_rand_get_stream_call_count(D_RNG_SIM);
}

void d_rand_set_call_count(unsigned int count)
{
	d_rand_set_stream_call_count(D_RNG_SIM, count);
}

void d_rand_reset_call_count(void)
{
	d_rand_reset_stream_call_count(D_RNG_SIM);
}

unsigned int d_rand_get_stream_call_count(d_rng_stream stream)
{
	if (!d_rng_stream_is_valid(stream))
		return d_rand_call_count[D_RNG_SIM];
	return d_rand_call_count[stream];
}

void d_rand_set_stream_call_count(d_rng_stream stream, unsigned int count)
{
	if (!d_rng_stream_is_valid(stream))
		stream = D_RNG_SIM;
	d_rand_call_count[stream] = count;
}

void d_rand_reset_stream_call_count(d_rng_stream stream)
{
	if (!d_rng_stream_is_valid(stream))
		stream = D_RNG_SIM;
	d_rand_call_count[stream] = 0;
}
