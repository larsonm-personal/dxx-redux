#ifndef DXX_GAME_CHEAT_H
#define DXX_GAME_CHEAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Engine-thread only, complete code lookup; returns whether a code was accepted */
int game_execute_cheat(const char *code);

#ifdef __cplusplus
}
#endif
#endif
