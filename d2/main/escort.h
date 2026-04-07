/*
 *
 * Header for escort.c
 *
 */

#ifndef _ESCORT_H
#define _ESCORT_H
#define GUIDEBOT_NAME_LEN 9
extern void change_guidebot_name(void);
extern void do_escort_menu(void);
extern void detect_escort_goal_accomplished(int index);
extern void set_escort_special_goal(int key);

#ifdef NETWORK
extern int Escort_owner_player;
void multi_send_escort_owner(int owner_pnum);
void multi_do_escort_owner(const ubyte *buf);
void escort_transfer_ownership_on_disconnect(int gone_pnum);
#endif

#endif // _ESCORT_H
