#include <stdio.h>
#include <stdlib.h>
#include "coop/coop_robot_door.h"

#define CHECK(condition)                                            \
	do {                                                            \
		if (!(condition)) {                                         \
			fprintf(stderr, "line %d: %s\n", __LINE__, #condition); \
			exit(1);                                                \
		}                                                           \
	} while (0)

int Game_mode, Player_num;
object Objects[MAX_OBJECTS];
segment Segments[MAX_SEGMENTS];
wall Walls[MAX_WALLS];
robot_info Robot_info[MAX_ROBOT_TYPES];
#ifdef DXX_BUILD_DESCENT_II
int Escort_owner_player;
#endif
static int sends;
int multi_i_am_master(void)
{
	return Player_num == 0;
}
void wall_open_door(segment *seg, int side)
{
	CHECK(seg == &Segments[0] && side == 1);
	if (Walls[0].state == WALL_DOOR_CLOSED || Walls[0].state == WALL_DOOR_CLOSING)
		Walls[0].state = WALL_DOOR_OPENING;
}
void multi_send_door_open(int segnum, int side, ubyte flags)
{
	CHECK(segnum == 0 && side == 1 && flags == Walls[0].flags);
	++sends;
}

int main(void)
{
	Segments[0].sides[1].wall_num = 0;
	for (int companion = 0; companion <= 1; ++companion) {
#ifdef DXX_BUILD_DESCENT_II
		Robot_info[0].companion = companion;
#else
		if (companion) continue;
#endif
		for (int owner = -1; owner < 2; ++owner) {
			Objects[0].ctype.ai_info.REMOTE_OWNER = (sbyte) owner;
#ifdef DXX_BUILD_DESCENT_II
			Escort_owner_player = owner;
			/* Companion ownership is independent of ordinary robot authority */
			if (companion) Objects[0].ctype.ai_info.REMOTE_OWNER = (sbyte) (owner == 0 ? 1 : 0);
#endif
			for (Player_num = 0; Player_num < 2; ++Player_num) {
				Game_mode = GM_MULTI_COOP;
				const int authority = owner == Player_num || (owner == -1 && Player_num == 0);
				for (int closing = 0; closing <= 1; ++closing) {
					const int initial = closing ? WALL_DOOR_CLOSING : WALL_DOOR_CLOSED;
					sends = 0;
					Walls[0].state = initial;
					coop_robot_open_door(&Objects[0], 0, 1);
					CHECK(sends == authority);
					CHECK(Walls[0].state == (authority ? WALL_DOOR_OPENING : initial));
					coop_robot_open_door(&Objects[0], 0, 1);
					CHECK(sends == authority);
				}
				Game_mode = 0;
				sends = 0;
				Walls[0].state = WALL_DOOR_CLOSED;
				coop_robot_open_door(&Objects[0], 0, 1);
				CHECK(Walls[0].state == WALL_DOOR_OPENING && !sends);
			}
		}
	}
	puts("PASS: robot door ownership, host fallback, closing reversal and single broadcast");
	return 0;
}
