#ifndef INPUT_DEMO_RNG_MODE_H
#define INPUT_DEMO_RNG_MODE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "maths.h"

int input_demo_rng_mode_parse(const char *text);
const char *input_demo_rng_mode_name(int mode);
int input_demo_rng_mode_is_compatible(int fixture_mode, int engine_mode);
const char *input_demo_rng_mode_parse_metadata_text(const char *text, int *mode);
const char *input_demo_rng_mode_validate_metadata_text(const char *text, int engine_mode,
                                                       int *mode);
const char *input_demo_rng_mode_validate_metadata_file(const char *path, int engine_mode,
                                                       int *mode);

#ifdef __cplusplus
}
#endif

#endif