#ifndef DXX_INPUT_DEMO_START_H
#define DXX_INPUT_DEMO_START_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int input_demo_maybe_validate_metadata_from_cmdline(void);
int input_demo_maybe_start_replay_from_cmdline(void);
void input_demo_set_skip_level_intro(int skip);
void input_demo_apply_level_player_cfg(void);
int input_demo_consume_skip_level_intro(void);
int input_demo_load_replay_from_path(const char *demo_path, char *error, size_t error_size);
int input_demo_start_loaded_replay(void);

#ifdef __cplusplus
}
#endif

#endif
