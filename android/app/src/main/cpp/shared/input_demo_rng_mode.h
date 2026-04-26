#ifndef INPUT_DEMO_RNG_MODE_H
#define INPUT_DEMO_RNG_MODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "maths.h"

int input_demo_rng_mode_parse(const char *text);
const char *input_demo_rng_mode_name(int mode);
int input_demo_rng_mode_is_compatible(int fixture_mode, int engine_mode);

#ifdef __cplusplus
}
#endif

#endif