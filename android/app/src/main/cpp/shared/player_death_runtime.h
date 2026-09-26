#ifndef DXX_PLAYER_DEATH_RUNTIME_H
#define DXX_PLAYER_DEATH_RUNTIME_H

#include "fix.h"

// Gameplay observation only; saves reject active death
// Camera/render history is excluded, and inactive sequence caches are unused
typedef struct player_death_runtime_state {
	int active, exploded, eggs_dropped, aborted;
	fix elapsed;
	int saved_flags, saved_control;
} player_death_runtime_state;

#ifdef __cplusplus
extern "C" {
#endif
void player_death_get_runtime_state(player_death_runtime_state *state);
#ifdef __cplusplus
}
#endif

#endif
