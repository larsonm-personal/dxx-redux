#include "keyed_wall.h"
#include "preview.h"
extern "C" {
#include "inferno.h"
#include "object.h"
#include "player.h"
#include "segment.h"
#include "wall.h"
int check_transparency(segment *, int);
}
extern "C" int xl_closed_wall_doorway(int index, int segment, int side)
{
	if (!xl_level_extensions_active() || segment < 0 || segment >= Num_segments || side < 0 || side >= 6) return -1;
	auto &room = Segments[segment];
	if (room.children[side] < 0 || room.children[side] >= Num_segments) return -1;
	const int wall_index = room.sides[side].wall_num;
	if (wall_index < 0 || wall_index >= Num_walls) return -1;
	const auto &wall = Walls[wall_index];
	if (wall.type != WALL_CLOSED) return -1;
	if (index >= 0 && index <= Highest_object_index) {
		const auto &actor = Objects[index];
		if (actor.type == OBJ_PLAYER && actor.id < MAX_PLAYERS) {
			const auto keys = Players[actor.id].flags;
			if ((wall.keys == KEY_BLUE && (keys & PLAYER_FLAGS_BLUE_KEY)) || (wall.keys == KEY_RED && (keys & PLAYER_FLAGS_RED_KEY))) return WID_ILLUSORY_WALL;
		}
	}
	return (wall.cloak_value > 0 && wall.cloak_value < 31) || check_transparency(&room, side) ? WID_TRANSPARENT_WALL : WID_WALL;
}
