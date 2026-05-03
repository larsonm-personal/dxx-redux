#ifndef INPUT_DEMO_FP_ENV_H
#define INPUT_DEMO_FP_ENV_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int input_demo_configure_startup_fp_environment(char *error, size_t error_size);
void input_demo_restore_replay_fp_environment(void);

#ifdef __cplusplus
}
#endif

#endif
