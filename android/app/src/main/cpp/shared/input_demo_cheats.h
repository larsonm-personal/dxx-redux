#ifndef INPUT_DEMO_CHEATS_H
#define INPUT_DEMO_CHEATS_H

#include <stddef.h>

/* Stable direct-command values, independent of the engine's cheat table order */
#define INPUT_DEMO_CHEAT_D1_ENABLE           0
#define INPUT_DEMO_CHEAT_D1_STANDARD_WEAPONS 1
#define INPUT_DEMO_CHEAT_D1_ALL_WEAPONS      2
#define INPUT_DEMO_CHEAT_D2_ALL_WEAPONS      3

#ifdef __cplusplus
extern "C" {
#endif

int input_demo_recorder_stage_cheat(int cheat, char *error, size_t error_size);
void input_demo_record_cheat(int cheat);
/* Engine-owned effects, shared by live activation and playback */
int game_apply_demo_cheat(int cheat, int validate_only);

#ifdef __cplusplus
}
#endif

#endif
