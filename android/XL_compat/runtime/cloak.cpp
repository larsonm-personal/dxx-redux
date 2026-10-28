#include "cloak.h"
#include "preview.h"
#include <cstring>
extern "C" {
#include "inferno.h"
#include "wall.h"
#include "segment.h"
#include "gameseg.h"
#include "digi.h"
#include "sounds.h"
#include "newdemo.h"
#ifdef __ANDROID__
#include "guidebot_extensions.h"
#endif
}

namespace
{
bool valid_wall(int index)
{
	if (index < 0 || index >= Num_walls) return false;
	const auto &w = Walls[index];
	return w.segnum >= 0 && w.segnum < Num_segments && w.sidenum >= 0 && w.sidenum < 6 &&
	       Segments[w.segnum].sides[w.sidenum].wall_num == index;
}
int connected_wall(int segment_index, int side)
{
	auto &room = Segments[segment_index];
	const int child = room.children[side];
	if (child < 0 || child >= Num_segments) return -1;
	const int connected = find_connect_side(&room, &Segments[child]);
	if (connected < 0 || connected >= 6) return -1;
	const int back = Segments[child].sides[connected].wall_num;
	return valid_wall(back) ? back : -1;
}
void notify_wall(int index)
{
#ifdef __ANDROID__
	if (index >= 0) escort_route_notify_wall_changed(index);
#else
	(void) index;
#endif
}
void restore_light(const cloaking_wall &entry)
{
	for (int corner = 0; corner < 4; ++corner) {
		const auto &front = Walls[entry.front_wallnum];
		Segments[front.segnum].sides[front.sidenum].uvls[corner].l = entry.front_ls[corner];
		if (entry.back_wallnum >= 0) {
			const auto &back = Walls[entry.back_wallnum];
			Segments[back.segnum].sides[back.sidenum].uvls[corner].l = entry.back_ls[corner];
		}
	}
}
void scale_light(const cloaking_wall &entry, fix scale)
{
	for (int corner = 0; corner < 4; ++corner) {
		const auto &front = Walls[entry.front_wallnum];
		Segments[front.segnum].sides[front.sidenum].uvls[corner].l = fixmul(entry.front_ls[corner], scale);
		if (entry.back_wallnum >= 0) {
			const auto &back = Walls[entry.back_wallnum];
			Segments[back.segnum].sides[back.sidenum].uvls[corner].l = fixmul(entry.back_ls[corner], scale);
		}
	}
}
void erase_entry(int index)
{
	--Num_cloaking_walls;
	if (index < Num_cloaking_walls)
		std::memmove(&CloakingWalls[index], &CloakingWalls[index + 1], (Num_cloaking_walls - index) * sizeof(cloaking_wall));
}
} // namespace

extern "C" int xl_start_wall_cloak(int segment_index, int side, int opening)
{
	if (!xl_level_extensions_active()) return 0;
	if (Newdemo_state == ND_STATE_PLAYBACK || segment_index < 0 || segment_index >= Num_segments || side < 0 || side >= 6) return 1;
	const int index = Segments[segment_index].sides[side].wall_num;
	if (!valid_wall(index)) return 1;
	auto &wall = Walls[index];
	const int desired = opening ? WALL_DOOR_CLOAKING : WALL_DOOR_DECLOAKING;
	if (wall.type == (opening ? WALL_OPEN : WALL_CLOSED) || wall.state == desired) return 1;
	const int back = connected_wall(segment_index, side);
	cloaking_wall *entry = nullptr;
	if (wall.state == (opening ? WALL_DOOR_DECLOAKING : WALL_DOOR_CLOAKING)) {
		for (int i = 0; i < Num_cloaking_walls; ++i)
			if (CloakingWalls[i].front_wallnum == index || CloakingWalls[i].back_wallnum == index) {
				entry = &CloakingWalls[i];
				entry->time = F1_0 - entry->time;
				break;
			}
	} else if (wall.state == WALL_DOOR_CLOSED && Num_cloaking_walls < DXX_XL_MAX_CLOAKING_WALLS) {
		entry = &CloakingWalls[Num_cloaking_walls++];
		*entry = {};
	}
	if (!entry) {
		// Source open fallback; correct the decloak caller's type/state mix-up
		wall.type = opening ? WALL_OPEN : WALL_CLOSED;
		wall.state = WALL_DOOR_CLOSED;
		if (back >= 0) {
			Walls[back].type = wall.type;
			Walls[back].state = WALL_DOOR_CLOSED;
		}
		notify_wall(index);
		notify_wall(back);
		return 1;
	}
	wall.state = desired;
	if (back >= 0) Walls[back].state = desired;
	entry->front_wallnum = index;
	entry->back_wallnum = back;
	for (int corner = 0; corner < 4; ++corner) {
		entry->front_ls[corner] = Segments[segment_index].sides[side].uvls[corner].l;
		entry->back_ls[corner] = back >= 0 ? Segments[Walls[back].segnum].sides[Walls[back].sidenum].uvls[corner].l : 0;
	}
	vms_vector center;
	compute_center_point_on_side(&center, &Segments[segment_index], side);
	digi_link_sound_to_pos(opening ? SOUND_WALL_CLOAK_ON : SOUND_WALL_CLOAK_OFF, segment_index, side, &center, 0, F1_0);
	notify_wall(index);
	notify_wall(back);
	return 1;
}

extern "C" int xl_cloak_frame_process(void)
{
	if (!xl_level_extensions_active()) return 0;
	if (Newdemo_state == ND_STATE_PLAYBACK) return 1;
	for (int i = 0; i < Num_cloaking_walls;) {
		auto &entry = CloakingWalls[i];
		if (!valid_wall(entry.front_wallnum) || (entry.back_wallnum != -1 && !valid_wall(entry.back_wallnum))) {
			erase_entry(i);
			continue;
		}
		auto &front = Walls[entry.front_wallnum];
		auto *back = entry.back_wallnum >= 0 ? &Walls[entry.back_wallnum] : nullptr;
		const bool opening = front.state == WALL_DOOR_CLOAKING;
		if (!opening && front.state != WALL_DOOR_DECLOAKING) {
			erase_entry(i);
			continue;
		}
		const int64_t elapsed = int64_t(entry.time) + FrameTime;
		const bool completed = elapsed > F1_0;
		entry.time = completed ? F1_0 : static_cast<fix>(elapsed);
		if (completed) {
			front.type = opening ? WALL_OPEN : WALL_CLOSED;
			front.state = WALL_DOOR_CLOSED;
			restore_light(entry);
		} else if (opening && entry.time > F1_0 / 2) {
			const int old_type = front.type;
			front.type = WALL_CLOAKED;
			front.cloak_value = static_cast<int8_t>(31.0f * (float(entry.time) / F1_0));
			if (old_type != WALL_CLOAKED) restore_light(entry);
		} else if (opening) {
			scale_light(entry, fixdiv(F1_0 / 2 - entry.time, F1_0 / 2));
		} else if (entry.time > F1_0 / 2) {
			front.type = WALL_CLOSED;
			scale_light(entry, fixdiv(entry.time - F1_0 / 2, F1_0 / 2));
		} else {
			front.type = WALL_CLOAKED;
			front.cloak_value = ((F1_0 / 2 - entry.time) * (31 - 2)) / (F1_0 / 2);
		}
		if (back) {
			if (completed || !opening || entry.time > F1_0 / 2) back->type = front.type;
			if (completed) back->state = front.state;
			if (!completed && (opening ? entry.time > F1_0 / 2 : entry.time <= F1_0 / 2)) back->cloak_value = front.cloak_value;
		}
		notify_wall(entry.front_wallnum);
		notify_wall(entry.back_wallnum);
		if (Newdemo_state == ND_STATE_RECORDING) {
			const auto &face = Segments[front.segnum].sides[front.sidenum];
			newdemo_record_cloaking_wall(entry.front_wallnum, entry.back_wallnum, front.type, front.state, front.cloak_value,
			                             face.uvls[0].l, face.uvls[1].l, face.uvls[2].l, face.uvls[3].l);
		}
		// Record the owner before compaction and process its replacement this frame
		if (completed) erase_entry(i);
		else ++i;
	}
	return 1;
}

extern "C" int xl_cloak_saved_valid(const cloaking_wall *entries, int count)
{
	if (count < 0 || count > DXX_XL_MAX_CLOAKING_WALLS) return 0;
	bool seen[DXX_XL_MAX_WALLS]{};
	for (int i = 0; i < count; ++i) {
		const auto &entry = entries[i];
		if (!valid_wall(entry.front_wallnum) || (entry.back_wallnum != -1 && !valid_wall(entry.back_wallnum)) || entry.time < 0 || entry.time > F1_0) return 0;
		const auto &front = Walls[entry.front_wallnum];
		if (front.state != WALL_DOOR_CLOAKING && front.state != WALL_DOOR_DECLOAKING) return 0;
		if (connected_wall(front.segnum, front.sidenum) != entry.back_wallnum) return 0;
		if (seen[entry.front_wallnum]) return 0;
		seen[entry.front_wallnum] = true;
		if (entry.back_wallnum >= 0) {
			if (seen[entry.back_wallnum] || Walls[entry.back_wallnum].state != front.state) return 0;
			seen[entry.back_wallnum] = true;
		}
	}
	return 1;
}
