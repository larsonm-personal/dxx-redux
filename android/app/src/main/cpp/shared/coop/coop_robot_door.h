#ifndef COOP_ROBOT_DOOR_H
#define COOP_ROBOT_DOOR_H

#include "game.h"
#include "object.h"
#include "robot.h"
#include "segment.h"
#include "wall.h"
#include "android_log.h"
#ifdef NETWORK
#include "multi.h"
#ifdef DXX_BUILD_DESCENT_II
#include "escort.h"
#endif
#endif

/* Robot body contacts need the same replication as weapon/player contacts
 * Receive-side wall_open_door calls must not broadcast another door event */
static inline void coop_robot_open_door(object *robot, int segnum, int side)
{
	int wallnum = Segments[segnum].sides[side].wall_num;
	int previous_state = Walls[wallnum].state;
#ifdef NETWORK
	if (Game_mode & GM_MULTI_COOP) {
		int owner = robot->ctype.ai_info.REMOTE_OWNER;
#ifdef DXX_BUILD_DESCENT_II
		if (Robot_info[robot->id].companion) owner = Escort_owner_player;
#endif
		if (owner != Player_num && (owner != -1 || !multi_i_am_master())) return;
	}
#endif
	wall_open_door(&Segments[segnum], side);
#ifdef NETWORK
	if ((Game_mode & GM_MULTI_COOP) && Walls[wallnum].state != previous_state) {
		debug_log(DLOG_NETWORK, "[DOOR] robot body player=%d robot=%d segment=%d side=%d wall=%d state=%d->%d",
		          Player_num, (int) (robot - Objects), segnum, side, wallnum, previous_state, Walls[wallnum].state);
		multi_send_door_open(segnum, side, Walls[wallnum].flags);
	}
#else
	(void) robot;
	(void) previous_state;
#endif
}

#endif
