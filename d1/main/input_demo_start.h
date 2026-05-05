#ifndef DXX_INPUT_DEMO_START_H
#define DXX_INPUT_DEMO_START_H

#ifdef __cplusplus
extern "C" {
#endif

int input_demo_maybe_validate_metadata_from_cmdline(void);
int input_demo_maybe_start_replay_from_cmdline(void);
void input_demo_set_skip_level_intro(int skip);
int input_demo_consume_skip_level_intro(void);

#ifdef __cplusplus
}
#endif

#endif