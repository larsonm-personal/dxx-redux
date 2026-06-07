/*
 *
 * Header for escort.c
 *
 */

#ifndef _ESCORT_H
#define _ESCORT_H

struct input_demo_checkpoint_escort_state;
struct input_demo_checkpoint_thief_state;

#define GUIDEBOT_NAME_LEN 9
extern void change_guidebot_name(void);
extern void do_escort_menu(void);
extern void detect_escort_goal_accomplished(int index);
extern void set_escort_special_goal(int key);
extern void escort_find_secret_goal(void);
extern int escort_get_secret_goal_display_index(void);
extern int escort_get_secret_goal_seg(void);
extern int escort_get_secret_goal_side(void);
extern void input_demo_apply_recorded_guidebot_goal(int special_key, int from_menu);
extern void input_demo_apply_recorded_guidebot_find_secret(void);
extern void escort_rebuild_runtime_state_after_restore(void);
extern void escort_spawn_at_player(void);
extern void escort_get_input_demo_checkpoint_state(struct input_demo_checkpoint_escort_state *escort_state);
extern void escort_get_input_demo_checkpoint_thief_state(struct input_demo_checkpoint_thief_state *thief_state);
extern int Buddy_objnum, Buddy_allowed_to_talk;

#ifdef NETWORK
extern int Escort_owner_player;
void multi_send_escort_owner(int owner_pnum);
void multi_do_escort_owner(const ubyte *buf);
void escort_transfer_ownership_on_disconnect(int gone_pnum);
void escort_release_control(void);
#endif

#endif // _ESCORT_H
