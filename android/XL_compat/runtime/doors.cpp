#include "doors.h"
#include "linked_audio.h"
#include "preview.h"
#include <algorithm>
#include <array>
#include <cstdint>
extern "C" {
#include "inferno.h"
#include "segment.h"
#include "object.h"
#include "wall.h"
#include "gameseg.h"
#include "digi.h"
#include "newdemo.h"
int check_poke(int, int, int);
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
	return w.segnum >= 0 && w.segnum < Num_segments &&
	       w.sidenum >= 0 && w.sidenum < 6 && Segments[w.segnum].sides[w.sidenum].wall_num == index &&
	       w.clip_num >= 0 && w.clip_num < Num_wall_anims && WallAnims[w.clip_num].num_frames > 0 &&
	       WallAnims[w.clip_num].num_frames <= MAX_CLIP_FRAMES &&
	       WallAnims[w.clip_num].play_time >= WallAnims[w.clip_num].num_frames;
}
int opposite(int index)
{
	const auto &w = Walls[index];
	auto &seg = Segments[w.segnum];
	const int child = seg.children[w.sidenum];
	if (child < 0 || child >= Num_segments) return -1;
	const int side = find_connect_side(&seg, &Segments[child]);
	return side >= 0 && side < 6 ? Segments[child].sides[side].wall_num : -1;
}
int wall_at(int segment_index, int side)
{
	if (segment_index < 0 || segment_index >= Num_segments || side < 0 || side >= 6) return -1;
	const int index = Segments[segment_index].sides[side].wall_num;
	return valid_wall(index) ? index : -1;
}
bool blocked(int index)
{
	const auto &w = Walls[index];
	const int child = Segments[w.segnum].children[w.sidenum];
	// Ordinary obstruction checks require a reciprocal connection
	if (child < 0 || child >= Num_segments) return false;
	const int back_side = find_connect_side(&Segments[w.segnum], &Segments[child]);
	if (back_side < 0) return false;
	// Keep the two-sided geometry correction, with XL's nonblocking effect types
	for (int segment : { w.segnum, child })
		for (int object = Segments[segment].objects; object >= 0; object = Objects[object].next) {
			const int type = Objects[object].type;
			if (type == OBJ_WEAPON || type == OBJ_FIREBALL || type == 19 || type == 20) continue;
			if (check_poke(object, segment, segment == w.segnum ? w.sidenum : back_side)) return true;
		}
	return false;
}
void notify(int index)
{
#ifdef __ANDROID__
	escort_route_notify_wall_changed(index);
#else
	(void) index;
#endif
}
void sound(int index, int id)
{
	if (id < 0 || Newdemo_state == ND_STATE_PLAYBACK) return;
	const auto &w = Walls[index];
	vms_vector center;
	compute_center_point_on_side(&center, &Segments[w.segnum], w.sidenum);
	digi_link_sound_to_pos(id, w.segnum, w.sidenum, &center, 0, F1_0);
}
void texture(int index, int frame)
{
	const auto &w = Walls[index];
	// XL animates each face with its own clip, including asymmetric pairs
	const auto &clip = WallAnims[w.clip_num];
	auto &side = Segments[w.segnum].sides[w.sidenum];
	const int value = clip.frames[frame];
	if ((clip.flags & WCF_TMAP1) || !(side.tmap_num2 & 0x3fff)) {
		side.tmap_num = value;
		if (Newdemo_state == ND_STATE_RECORDING) newdemo_record_wall_set_tmap_num1(w.segnum, w.sidenum, w.segnum, w.sidenum, value);
	} else {
		side.tmap_num2 = (side.tmap_num2 & 0xc000) | value;
		if (Newdemo_state == ND_STATE_RECORDING) newdemo_record_wall_set_tmap_num2(w.segnum, w.sidenum, w.segnum, w.sidenum, side.tmap_num2);
	}
}
active_door *find(int index)
{
	for (int i = 0; i < Num_open_doors; ++i)
		for (int part = 0; part < ActiveDoors[i].n_parts && part < 2; ++part)
			if (ActiveDoors[i].front_wallnum[part] == index || ActiveDoors[i].back_wallnum[part] == index) return &ActiveDoors[i];
	return nullptr;
}
void erase(int index)
{
	for (int i = index + 1; i < Num_open_doors; ++i) ActiveDoors[i - 1] = ActiveDoors[i];
	ActiveDoors[--Num_open_doors] = {};
	xl_audio_update_routes();
}
void advance(active_door &door)
{
	const auto elapsed = static_cast<int64_t>(door.time) + FrameTime;
	door.time = static_cast<fix>((std::min) (elapsed, static_cast<int64_t>(INT32_MAX)));
}
int animate(int index, fix elapsed, bool opening)
{
	auto &w = Walls[index];
	const auto &clip = WallAnims[w.clip_num];
	const int n = clip.num_frames;
	const int phase = elapsed / (clip.play_time / n);
	const int frame = opening ? phase : n - phase - 1;
	if (opening) {
		if (frame < n) texture(index, frame);
		if (frame > n / 2) w.flags |= WALL_DOOR_OPENED;
		if (frame < n - 1) return 0;
		texture(index, n - 1);
		w.state = w.flags & WALL_DOOR_AUTO ? WALL_DOOR_WAITING : WALL_DOOR_OPEN;
		notify(index);
		return w.flags & WALL_DOOR_AUTO ? 2 : 1;
	}
	if (frame < n / 2) w.flags &= ~WALL_DOOR_OPENED;
	if (frame <= 0) return 1;
	texture(index, frame);
	w.state = WALL_DOOR_CLOSING;
	return 0;
}
int transition(int segment_index, int side, bool opening, bool force)
{
	if (!xl_level_extensions_active()) return 0;
	const int index = wall_at(segment_index, side);
	if (index < 0 || Num_open_doors < 0 || Num_open_doors > MAX_DOORS) return 1;
	auto &w = Walls[index];
	const int back = opposite(index);
	if (back >= 0 && !valid_wall(back)) return 1;
	bool reuse;
	if (opening) {
		if (w.state != WALL_DOOR_CLOSED && w.state != WALL_DOOR_CLOSING) return 1;
		reuse = w.state == WALL_DOOR_CLOSING;
	} else {
		if (w.state != WALL_DOOR_OPEN && w.state != WALL_DOOR_OPENING && !(force && w.state == WALL_DOOR_WAITING)) return 1;
		if (blocked(index)) return 1;
		reuse = w.state == WALL_DOOR_OPENING || w.state == WALL_DOOR_WAITING;
	}
	auto *door = reuse ? find(index) : nullptr;
	if (door) {
		const auto remaining = static_cast<int64_t>(WallAnims[w.clip_num].play_time) - door->time;
		if (remaining > INT32_MAX) return 1;
		door->time = static_cast<fix>((std::max) (INT64_C(0), remaining));
	} else {
		if (Num_open_doors == MAX_DOORS) return 1;
		door = &ActiveDoors[Num_open_doors++];
		*door = {};
		xl_audio_update_routes();
	}
	door->n_parts = 1;
	door->front_wallnum[0] = index;
	door->back_wallnum[0] = back;
	door->front_wallnum[1] = door->back_wallnum[1] = -1;
	w.state = opening ? WALL_DOOR_OPENING : WALL_DOOR_CLOSING;
	if (opening) xl_audio_update_routes();
	if (back >= 0) Walls[back].state = w.state;
	if (opening && back >= 0 && w.linked_wall == back) {
		door->n_parts = 2;
		door->front_wallnum[1] = back;
		// The reference mistakenly stores a side number as a wall identity here
		door->back_wallnum[1] = index;
	}
	notify(index);
	if (back >= 0) notify(back);
	if (Newdemo_state == ND_STATE_RECORDING) newdemo_record_door_opening(segment_index, side);
	sound(index, WallAnims[w.clip_num].open_sound);
	return 1;
}
} // namespace

extern "C" int xl_door_open(int segment_index, int side)
{
	return transition(segment_index, side, true, false);
}
extern "C" int xl_door_close(int segment_index, int side, int force)
{
	return transition(segment_index, side, false, force != 0);
}
extern "C" int xl_authored_close_door(int segment_index, int side)
{
	return xl_door_close(segment_index, side, 1);
}

extern "C" int xl_door_state_valid(const active_door *doors, int count)
{
	if (!xl_level_extensions_active()) return 1;
	if (count < 0 || count > MAX_DOORS || (count && !doors)) return 0;
	std::array<int, MAX_WALLS> owners;
	owners.fill(-1);
	for (int i = 0; i < count; ++i) {
		const auto &door = doors[i];
		if (door.n_parts < 1 || door.n_parts > 2 || door.time < 0) return 0;
		bool opening = false, closing = false, waiting = true, all_closing = true;
		for (int part = 0; part < door.n_parts; ++part) {
			const int front = door.front_wallnum[part], back = door.back_wallnum[part];
			if (!valid_wall(front) || back < -1 || (back >= 0 && !valid_wall(back)) || back != opposite(front)) return 0;
			for (int index : { front, back })
				if (index >= 0) {
					if (owners[index] >= 0 && owners[index] != i) return 0;
					owners[index] = i;
					const int state = Walls[index].state;
					if (state != WALL_DOOR_OPENING && state != WALL_DOOR_CLOSING && state != WALL_DOOR_WAITING && state != WALL_DOOR_OPEN) return 0;
					opening |= state == WALL_DOOR_OPENING;
					closing |= state == WALL_DOOR_CLOSING;
					waiting &= state == WALL_DOOR_WAITING;
					all_closing &= state == WALL_DOOR_CLOSING;
				}
		}
		if (door.n_parts == 2 && (door.front_wallnum[1] != door.back_wallnum[0] || door.back_wallnum[1] != door.front_wallnum[0] || Walls[door.front_wallnum[0]].linked_wall != door.front_wallnum[1])) return 0;
		if ((!opening && !closing && !waiting) || (closing && !all_closing) ||
		    (waiting && !(Walls[door.front_wallnum[0]].flags & WALL_DOOR_AUTO))) return 0;
	}
	for (int index = 0; index < Num_walls; ++index)
		if (Walls[index].type == WALL_DOOR && (Walls[index].state == WALL_DOOR_OPENING || Walls[index].state == WALL_DOOR_CLOSING || Walls[index].state == WALL_DOOR_WAITING) && owners[index] < 0) return 0;
	return 1;
}

extern "C" int xl_door_frame_process(void)
{
	if (!xl_level_extensions_active()) return 0;
	for (int i = 0; i < Num_open_doors;) {
		auto &door = ActiveDoors[i];
		const int front = door.front_wallnum[0];
		if (!valid_wall(front)) {
			erase(i);
			continue;
		}
		auto &w = Walls[front];
		bool opening = false;
		for (int part = 0; part < door.n_parts; ++part)
			for (int index : { door.front_wallnum[part], door.back_wallnum[part] })
				if (index >= 0) opening |= Walls[index].state == WALL_DOOR_OPENING;
		// A faster face must not retire its still-opening partner
		if (!opening && (w.state == WALL_DOOR_CLOSED || w.state == WALL_DOOR_OPEN)) {
			erase(i);
			continue;
		}
		if (!opening && w.state == WALL_DOOR_WAITING) {
			advance(door);
			if (door.time > DOOR_WAIT_TIME && !blocked(front)) {
				for (int part = 0; part < door.n_parts; ++part)
					for (int index : { door.front_wallnum[part], door.back_wallnum[part] })
						if (index >= 0) Walls[index].state = WALL_DOOR_CLOSING;
				door.time = 0;
			} else {
				// Keep both faces waiting until the deadline, correcting the source's premature back-side close
				for (int part = 0; part < door.n_parts; ++part)
					for (int index : { door.front_wallnum[part], door.back_wallnum[part] })
						if (index >= 0) Walls[index].flags |= WALL_DOOR_OPENED;
			}
			++i;
			continue;
		}
		if (!opening && w.state != WALL_DOOR_CLOSING) {
			++i;
			continue;
		}
		if (!opening && (w.flags & WALL_DOOR_AUTO) && blocked(front)) {
			digi_kill_sound_linked_to_segment(w.segnum, w.sidenum, -1);
			xl_door_open(w.segnum, w.sidenum);
			++i;
			continue;
		}
		int flags = opening ? 3 : 1;
		for (int part = 0; part < door.n_parts; ++part) {
			const int a = door.front_wallnum[part], b = door.back_wallnum[part];
			if (opening) {
				kill_stuck_objects(a);
				if (b >= 0) kill_stuck_objects(b);
			} else if (!part && !door.time) sound(a, WallAnims[Walls[a].clip_num].close_sound);
			advance(door);
			flags &= animate(a, door.time, opening);
			if (b >= 0) flags &= animate(b, door.time, opening);
		}
		bool finished_opening = opening;
		for (int part = 0; opening && part < door.n_parts; ++part)
			for (int index : { door.front_wallnum[part], door.back_wallnum[part] })
				if (index >= 0) finished_opening &= Walls[index].state != WALL_DOOR_OPENING;
		if (finished_opening) {
			// The initiating face owns automatic closing; mixed face flags must not strand a waiting partner
			flags = w.flags & WALL_DOOR_AUTO ? 2 : 1;
			for (int part = 0; part < door.n_parts; ++part)
				for (int index : { door.front_wallnum[part], door.back_wallnum[part] })
					if (index >= 0) Walls[index].state = flags == 2 ? WALL_DOOR_WAITING : WALL_DOOR_OPEN;
		}
		if (flags & 1) {
			if (!opening)
				for (int part = 0; part < door.n_parts; ++part)
					for (int index : { door.front_wallnum[part], door.back_wallnum[part] })
						if (index >= 0) {
							Walls[index].state = WALL_DOOR_CLOSED;
							Walls[index].flags &= ~WALL_DOOR_OPENED;
							texture(index, 0);
							notify(index);
						}
			erase(i);
		} else {
			if (opening && (flags & 2)) door.time = 0;
			++i;
		}
	}
	return 1;
}
