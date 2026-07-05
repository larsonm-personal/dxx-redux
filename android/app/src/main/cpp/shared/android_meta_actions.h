/*
 * android_meta_actions.h -- Meta action IDs for extra controller/touch controls.
 *
 * These IDs are shared between Kotlin (TouchBindings.kt) and C.
 * Keep both sides in sync when adding new actions.
 *
 * Meta actions are game functions that aren't part of the standard
 * kc_joystick[] control array (e.g. quicksave, guide bot commands,
 * direct weapon selection).  They are dispatched by injecting the
 * corresponding SDL key sequence into the event queue.
 */

#ifndef ANDROID_META_ACTIONS_H
#define ANDROID_META_ACTIONS_H

/* Action IDs -- must match TouchBindings.kt META_* constants */
#define META_ACTION_OFFSET 1000

#define META_QUICK_SAVE            1000
#define META_QUICK_LOAD            1001
#define META_GAME_MENU             1002
#define META_GUIDE_BOT_MENU        1003
#define META_GUIDE_FIND_ENERGY     1004
#define META_GUIDE_FIND_REACTOR    1005
#define META_GUIDE_FIND_SHIELD     1006
#define META_GUIDE_FIND_POWERUP    1007
#define META_GUIDE_FIND_ROBOT      1008
#define META_GUIDE_FIND_HOSTAGE    1009
#define META_GUIDE_SCRAM           1010
#define META_GUIDE_FIND_ITEMS      1011
#define META_GUIDE_FIND_EXIT       1012
#define META_GUIDE_CLEAR_GOAL      1013
#define META_MULTIPLAYER_HUD       1014
#define META_DROP_FLAG             1015
#define META_DROP_MARKER           1016
#define META_WEAPON_1              1020
#define META_WEAPON_2              1021
#define META_WEAPON_3              1022
#define META_WEAPON_4              1023
#define META_WEAPON_5              1024
#define META_WEAPON_6              1025
#define META_WEAPON_7              1026
#define META_WEAPON_8              1027
#define META_WEAPON_9              1028
#define META_WEAPON_10             1029
#define META_PAUSE                 1030
#define META_RETURN_TO_LAUNCHER    1031
#define META_GUIDE_RELEASE_CONTROL 1032
#define META_DEMO_RECORD_TOGGLE    1034
#define META_REWIND                1035
#define META_GUIDE_SPAWN           1037
#define META_GUIDE_FIND_SECRET     1038
#define META_CYCLE_LEFT_VIEW       1039
#define META_CYCLE_RIGHT_VIEW      1040
#define META_GUIDE_NEXT_GOAL       1041
#define META_GUIDE_WARP_TO_ME      1042

/* Flags for dispatch table entries */
#define META_FLAG_INSTANT 1 /* inject full press+release on button down, ignore up */

/* Set by META_RETURN_TO_LAUNCHER so standard_handler skips the
 * "Abort Game?" confirmation dialog during gameplay.
 * Declared volatile because it is set on the UI thread (JNI) and
 * read on the game thread. */
extern volatile int android_force_quit;

/* Set on UI thread by META_GUIDE_RELEASE_CONTROL, consumed on game thread
 * in gamecntl.c to call escort_release_control() */
extern volatile int android_escort_release_pending;

/* Set on UI thread by META_GUIDE_SPAWN, consumed on the game thread in
 * gamecntl.c to spawn or release the guide-bot at the local player. */
extern volatile int android_escort_spawn_pending;

/* Set on UI thread by META_GUIDE_FIND_SECRET, consumed on the game thread
 * in gamecntl.c to send the guide-bot to a generated secret entrance. */
extern volatile int android_escort_find_secret_pending;

/* Set on UI thread by META_GUIDE_NEXT_GOAL, consumed on the game thread
 * in gamecntl.c to resume the guide-bot's default objective progression. */
extern volatile int android_escort_next_goal_pending;

/* Set on UI thread by META_GUIDE_WARP_TO_ME, consumed on the game thread
 * in gamecntl.c to recall the released guide-bot to the local player. */
extern volatile int android_escort_warp_to_me_pending;

/* Set on UI thread by META_DEMO_RECORD_TOGGLE, consumed on the game thread
 * in gamecntl.c to start or stop Android quick input-demo recording. */
extern volatile int android_demo_record_toggle_pending;

/* Set on UI thread by META_REWIND, consumed on the game thread
 * in gamecntl.c to trigger Android rewind handling. */
extern volatile int android_rewind_pending;

/*
 * Dispatch a meta action.  Called from JNI (UI thread).
 *   action_id: one of the META_* constants above
 *   pressed:   1 = button down, 0 = button up
 * Returns 0 on success, -1 if action_id is unknown.
 */
int meta_action_dispatch(int action_id, int pressed);

#endif /* ANDROID_META_ACTIONS_H */
