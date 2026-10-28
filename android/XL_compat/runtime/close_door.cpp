#include "preview.h"
extern "C" {
#include "inferno.h"
#include "segment.h"
#include "wall.h"
#include "gameseg.h"
#include "digi.h"
#include "newdemo.h"
int is_door_free(segment *, int);
#ifdef __ANDROID__
#include "guidebot_extensions.h"
#endif
}

extern "C" int xl_authored_close_door(int segment_index, int side)
{
	if (!xl_level_extensions_active()) return 0;
	if (segment_index < 0 || segment_index >= Num_segments || side < 0 || side >= 6) return 1;
	auto &room = Segments[segment_index];
	const int index = room.sides[side].wall_num, child = room.children[side];
	if (index < 0 || index >= Num_walls || child < 0 || child >= Num_segments) return 1;
	auto &wall = Walls[index];
	const int connected = find_connect_side(&room, &Segments[child]);
	if (connected < 0 || connected >= 6 || wall.type != WALL_DOOR || wall.clip_num < 0 || wall.clip_num >= Num_wall_anims) return 1;
	if (wall.state != WALL_DOOR_OPEN && wall.state != WALL_DOOR_OPENING && wall.state != WALL_DOOR_WAITING) return 1;
	// Keep obstruction checks on both sides; the reference accidentally walks only one
	if (!is_door_free(&room, side)) return 1;
	if (Num_open_doors < 0 || Num_open_doors >= MAX_DOORS) return 1;
	active_door *door = nullptr;
	if (wall.state == WALL_DOOR_OPENING || wall.state == WALL_DOOR_WAITING) {
		for (int i = 0; i < Num_open_doors && !door; ++i)
			for (int part = 0; part < ActiveDoors[i].n_parts && part < 2; ++part)
				if (ActiveDoors[i].front_wallnum[part] == index || ActiveDoors[i].back_wallnum[part] == index) {
					door = &ActiveDoors[i];
					break;
				}
	}
	if (door) {
		const auto remaining = static_cast<int64_t>(WallAnims[wall.clip_num].play_time) - door->time;
		// Default-speed source arithmetic, with checked bounds for damaged live state
		if (remaining > INT32_MAX) return 1;
		door->time = remaining > 0 ? static_cast<fix>(remaining) : 0;
	} else {
		// The ordinary animation compactor reads a spare entry after the live tail
		if (Num_open_doors >= MAX_DOORS - 1) return 1;
		door = &ActiveDoors[Num_open_doors++];
		*door = {};
	}
	const int back = Segments[child].sides[connected].wall_num;
	wall.state = WALL_DOOR_CLOSING;
	if (back >= 0 && back < Num_walls) Walls[back].state = WALL_DOOR_CLOSING;
	door->front_wallnum[0] = index;
	door->back_wallnum[0] = back >= 0 && back < Num_walls ? back : -1;
	door->n_parts = 1;
#ifdef __ANDROID__
	escort_route_notify_wall_changed(index);
	if (back >= 0 && back < Num_walls) escort_route_notify_wall_changed(back);
#endif
	if (Newdemo_state == ND_STATE_RECORDING) newdemo_record_door_opening(segment_index, side);
	if (Newdemo_state != ND_STATE_PLAYBACK && WallAnims[wall.clip_num].open_sound > -1) {
		vms_vector center;
		compute_center_point_on_side(&center, &room, side);
		digi_link_sound_to_pos(WallAnims[wall.clip_num].open_sound, segment_index, side, &center, 0, F1_0);
	}
	return 1;
}
