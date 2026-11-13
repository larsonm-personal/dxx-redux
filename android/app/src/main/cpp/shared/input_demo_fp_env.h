#ifndef INPUT_DEMO_FP_ENV_H
#define INPUT_DEMO_FP_ENV_H

#include <stddef.h>
#include <float.h>

/* The CMake profile supplies contraction/optimization flags to both languages
 * Check the actual translation unit too, including a later fast-math override */
#if defined(DXX_INPUT_DEMO_STRICT_FP) && DXX_INPUT_DEMO_STRICT_FP &&                                                        \
    ((defined(_MSC_VER) && defined(_M_FP_STRICT)) ||                                                                        \
     (!defined(_MSC_VER) && (defined(__GNUC__) || defined(__clang__)) &&                                                    \
      !defined(__FAST_MATH__) && (!defined(__FINITE_MATH_ONLY__) || __FINITE_MATH_ONLY__ == 0) && FLT_EVAL_METHOD == 0)) && \
    (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(__SSE2__) ||                                    \
     defined(__aarch64__) || (defined(__arm__) && defined(__ARM_FP) && (__ARM_FP & 8)))
#define INPUT_DEMO_STRICT_FP_BUILD 1
#else
#define INPUT_DEMO_STRICT_FP_BUILD 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

int input_demo_configure_startup_fp_environment(char *error, size_t error_size);
int input_demo_validate_xl_fp_environment(char *error, size_t error_size);
void input_demo_restore_replay_fp_environment(void);

#ifdef __cplusplus
}
#endif

#endif
