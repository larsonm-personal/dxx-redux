#ifndef DXX_RNG_SHARED_H
#define DXX_RNG_SHARED_H

#define D_RAND_REPLAY_MODE_LCG_STATE   1
#define D_RAND_REPLAY_MODE_LIBC_RESEED 2
#define D_RAND_REPLAY_MODE_OUTPUT_LOG  3

typedef enum d_rng_stream {
	D_RNG_SIM = 0,
	D_RNG_FX = 1,
	D_RNG_STREAM_COUNT
} d_rng_stream;

void d_srand_annotated(d_rng_stream stream, unsigned int seed, const char *file, const char *func, int line);
int d_rand_annotated(d_rng_stream stream, const char *file, const char *func, int line);
int d_rand_get_replay_mode(void);
int d_rand_get_replay_mode_for_stream(d_rng_stream stream);
int d_rand_get_state(unsigned int *state);
int d_rand_set_state(unsigned int state);
unsigned int d_rand_get_call_count(void);
void d_rand_reset_call_count(void);
void d_srand_stream(d_rng_stream stream, unsigned int seed);
int d_rand_stream(d_rng_stream stream);
int d_rand_get_stream_state(d_rng_stream stream, unsigned int *state);
int d_rand_set_stream_state(d_rng_stream stream, unsigned int state);
unsigned int d_rand_get_stream_call_count(d_rng_stream stream);
void d_rand_set_call_count(unsigned int count);
void d_rand_set_stream_call_count(d_rng_stream stream, unsigned int count);
void d_rand_reset_stream_call_count(d_rng_stream stream);

#if defined(_MSC_VER) && !defined(__clang__)
#define DXX_RAND_CALLER_FUNCTION __FUNCTION__
#else
#define DXX_RAND_CALLER_FUNCTION __func__
#endif

#ifndef DXX_RNG_DEFAULT_STREAM
#define DXX_RNG_DEFAULT_STREAM D_RNG_SIM
#endif

#ifndef DXX_D_RAND_NO_ANNOTATION
#define d_rand()         d_rand_annotated(DXX_RNG_DEFAULT_STREAM, __FILE__, DXX_RAND_CALLER_FUNCTION, __LINE__)
#define d_srand(seed)    d_srand_annotated(DXX_RNG_DEFAULT_STREAM, (seed), __FILE__, DXX_RAND_CALLER_FUNCTION, __LINE__)
#define d_rand_fx()      d_rand_annotated(D_RNG_FX, __FILE__, DXX_RAND_CALLER_FUNCTION, __LINE__)
#define d_srand_fx(seed) d_srand_annotated(D_RNG_FX, (seed), __FILE__, DXX_RAND_CALLER_FUNCTION, __LINE__)
#endif

#endif
