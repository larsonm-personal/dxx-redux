#ifndef WEAPON_CHEAT_SHARED_H
#define WEAPON_CHEAT_SHARED_H

#include "input_demo_dispatch.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Resolve honestbob against the installed gameplay profile */
int weapon_cheat_default_grant(void);
/* A zero grant is D1 cheat enablement, without an inventory grant */
void weapon_cheat_execute_live(int grant);
void weapon_cheat_cancel_capture(const char *reason);
int weapon_cheat_apply(const input_demo_replay_direct_command_event *event,
                       int validate_only, char *error, size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
