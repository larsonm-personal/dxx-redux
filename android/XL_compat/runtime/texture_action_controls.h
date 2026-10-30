#ifndef DXX_XL_TEXTURE_ACTION_CONTROLS_H
#define DXX_XL_TEXTURE_ACTION_CONTROLS_H
// Controlled consumers shared by native and Android integration diagnostics
#include "preview.h"
#include "texture_pixels.h"
#include "object_trigger.h"
#include "trigger_state.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#include "inferno.h"
#include "game.h"
#include "object.h"
#include "player.h"
#include "segment.h"
#include "gameseg.h"
#include "switch.h"
#include "wall.h"
#include "textures.h"
#include "piggy.h"
#include "fvi.h"
#include "collide.h"
#include "laser.h"
void obj_delete_all_that_should_be_dead(void);
void flush_fcd_cache(void);
}

namespace xl_texture_controls
{
inline void require(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
inline side &face(int offset = 0)
{
	const auto &wall = Walls[Num_walls - 2 + offset];
	return Segments[wall.segnum].sides[wall.sidenum];
}
inline void fixture()
{
	require(xl_level_extensions_active() && Num_triggers == 12 && xl_object_trigger_count() == 2 &&
	            Triggers[0].type == 19 && Triggers[0].value == 0x00020001 && Triggers[4].time == 6000 &&
	            Triggers[0].seg[0] == Walls[Num_walls - 2].segnum && Triggers[0].side[0] == Walls[Num_walls - 2].sidenum,
	        "Texture controls require the labeled derived portal fixture");
	for (int id = 1; id <= 4; ++id)
		require(GameBitmaps[Textures[id].index].bm_type == BM_RGBA, "Texture action lost its prepared RGBA material");
}
inline std::vector<int> snapshot()
{
	std::vector<int> result;
	for (int s = 0; s < Num_segments; ++s)
		for (const auto &side : Segments[s].sides) {
			result.push_back(side.tmap_num);
			result.push_back(side.tmap_num2);
			for (const auto &uvl : side.uvls) {
				result.push_back(uvl.u);
				result.push_back(uvl.v);
				result.push_back(uvl.l);
			}
		}
	return result;
}
inline std::vector<int> uvs()
{
	std::vector<int> result;
	for (int s = 0; s < Num_segments; ++s)
		for (const auto &side : Segments[s].sides)
			for (const auto &uvl : side.uvls) {
				result.push_back(uvl.u);
				result.push_back(uvl.v);
				result.push_back(uvl.l);
			}
	return result;
}
inline int fire(int index)
{
	xl_wall_trigger_reset();
	return check_trigger_sub(index, Player_num, 1);
}
inline void expect(int base, int overlay, int offset = 0)
{
	if (face(offset).tmap_num != base || std::uint16_t(face(offset).tmap_num2) != unsigned(overlay))
		throw std::runtime_error("Texture action side " + std::to_string(offset) + " expected " + std::to_string(base) + "/" + std::to_string(overlay) +
		                         ", found " + std::to_string(face(offset).tmap_num) + "/" + std::to_string(std::uint16_t(face(offset).tmap_num2)));
}
inline void ray(bool transparent)
{
	const auto &wall = Walls[Num_walls - 2];
	auto &room = Segments[wall.segnum];
	const auto &surface = room.sides[wall.sidenum];
	vms_vector middle, begin, end;
	compute_center_point_on_side(&middle, &room, wall.sidenum);
	vm_vec_scale_add(&begin, &middle, &surface.normals[0], F1_0 / 2);
	vm_vec_scale_add(&end, &middle, &surface.normals[0], -F1_0 / 2);
	fvi_query query{};
	query.p0 = &begin;
	query.p1 = &end;
	query.startseg = wall.segnum;
	query.thisobjnum = -1;
	query.flags = FQ_TRANSPOINT;
	fvi_info hit{};
	require((find_vector_intersection(&query, &hit) == HIT_NONE) == transparent, "Texture-trigger change did not reach actual collision transparency");
}
inline void sound_route()
{
	const auto &front_wall = Walls[Num_walls - 2];
	const auto &back_wall = Walls[Num_walls - 1];
	const int first = front_wall.segnum, middle = back_wall.segnum;
	int last = -1, outward = -1, inward = -1;
	for (int side = 0; side < 6 && last < 0; ++side) {
		const int child = Segments[middle].children[side];
		if (child < 0 || child == first) continue;
		const int back = find_connect_side(&Segments[middle], &Segments[child]);
		if (back >= 0) {
			last = child;
			outward = side;
			inward = back;
		}
	}
	require(last >= 0, "Texture cache control requires a three-segment route");
	std::vector<std::array<short, 6>> children(Num_segments);
	for (int s = 0; s < Num_segments; ++s) std::copy_n(Segments[s].children, 6, children[s].begin());
	const auto old_front = face();
	const auto old_outward = Segments[middle].sides[outward].wall_num;
	const auto old_inward = Segments[last].sides[inward].wall_num;
	const auto restore = [&] {
		for (int s = 0; s < Num_segments; ++s) std::copy(children[s].begin(), children[s].end(), Segments[s].children);
		face() = old_front;
		Segments[middle].sides[outward].wall_num = old_outward;
		Segments[last].sides[inward].wall_num = old_inward;
		flush_fcd_cache();
	};
	try {
		// Restrict the graph for this cache observation; no actor is moved through it
		for (int s = 0; s < Num_segments; ++s) std::fill_n(Segments[s].children, 6, short(-1));
		Segments[first].children[front_wall.sidenum] = short(middle);
		Segments[middle].children[back_wall.sidenum] = short(first);
		Segments[middle].children[outward] = short(last);
		Segments[last].children[inward] = short(middle);
		Segments[middle].sides[outward].wall_num = Segments[last].sides[inward].wall_num = -1;
		require(fire(0) == 0, "Cannot close controlled sound route");
		flush_fcd_cache();
		vms_vector begin, end;
		compute_segment_center(&begin, &Segments[first]);
		compute_segment_center(&end, &Segments[last]);
		const auto sound = [&] { return find_connected_distance_for_sound(&begin, first, &end, last, 8, WID_RENDPAST_FLAG); };
		const auto gameplay = [&] { return find_connected_distance(&begin, first, &end, last, 8, WID_RENDPAST_FLAG); };
		require(sound() == -1 && sound() == 1000 * F1_0, "Sound route did not retain an observed blocked-cache entry");
		require(gameplay() == -1 && gameplay() == 1000 * F1_0, "Gameplay cache control was not established");
		require(fire(3) == 0, "Cannot open controlled sound route");
		const auto opened = sound();
		require(opened > 0 && opened < 1000 * F1_0 && gameplay() == 1000 * F1_0,
		        "Texture operation retained a stale sound path or changed the gameplay cache");
		restore();
	} catch (...) {
		restore();
		throw;
	}
}
inline void material_contacts()
{
	const auto surface = face();
	const auto material0 = TmapInfo[0], material1 = TmapInfo[1], material2 = TmapInfo[2];
	const auto shields = Players[Player_num].shields;
	const auto flags = Players[Player_num].flags;
	std::vector<int> signatures(MAX_OBJECTS, -1);
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type != OBJ_NONE) signatures[i] = Objects[i].signature;
	const auto restore = [&] {
		face() = surface;
		TmapInfo[0] = material0;
		TmapInfo[1] = material1;
		TmapInfo[2] = material2;
		Players[Player_num].shields = shields;
		Players[Player_num].flags = flags;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type != OBJ_NONE && signatures[i] != Objects[i].signature) Objects[i].flags |= OF_SHOULD_BE_DEAD;
		obj_delete_all_that_should_be_dead();
	};
	try {
		TmapInfo[1].damage = 0;
		TmapInfo[1].flags = 0;
		TmapInfo[0].flags = TmapInfo[2].flags = TMI_WATER;
		Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
		const auto &wall = Walls[Num_walls - 2];
		vms_vector point, center;
		compute_center_point_on_side(&point, &Segments[wall.segnum], wall.sidenum);
		compute_segment_center(&center, &Segments[wall.segnum]);
		for (const int overlay : { 0, 0x4000, 0x8000, 0xc000, 0xc002 }) {
			face().tmap_num = 1;
			face().tmap_num2 = short(overlay);
			Players[Player_num].shields = 100 * F1_0;
			collide_object_with_wall(ConsoleObject, 128 * F1_0, short(wall.segnum), short(wall.sidenum), &point);
			require(Players[Player_num].shields == (overlay == 0xc002 ? 100 : 99) * F1_0,
			        "Cleared overlay acquired phantom water contact or real water lost its effect");
			const int flare = Laser_create_new(&ConsoleObject->orient.fvec, &center, wall.segnum, Players[Player_num].objnum, FLARE_ID, 0);
			require(flare >= 0, "Cannot create controlled material-contact flare");
			collide_object_with_wall(&Objects[flare], F1_0, short(wall.segnum), short(wall.sidenum), &point);
			require(bool(Objects[flare].flags & OF_SHOULD_BE_DEAD) == (overlay == 0xc002),
			        "Cleared overlay extinguished a flare or real water retained it");
		}
		restore();
	} catch (...) {
		restore();
		throw;
	}
}
inline void events()
{
	fixture();
	const auto before_uvs = uvs();
	xl_wall_trigger_frame();
	expect(4, 0, 1);
	expect(4, 0xc002);
	require((Triggers[6].flags & TF_DISABLED) != 0, "Wall texture autoplay was not consumed");
	const auto untouched = std::array<int, 2>{ face(1).tmap_num, face(1).tmap_num2 };
	xl_wall_trigger_reset();
	const auto &front = Walls[Num_walls - 2];
	check_trigger(&Segments[front.segnum], short(front.sidenum), short(Players[Player_num].objnum), 0);
	expect(1, 0xc002);
	require(face(1).tmap_num == untouched[0] && face(1).tmap_num2 == untouched[1], "Single-sided texture action changed the connected side");
	require(fire(1) == 0, "Overlay clear action did not dispatch");
	expect(1, 0xc000);
	ray(false);
	require(fire(2) == 0, "Base-only texture action did not dispatch");
	expect(4, 0xc000);
	require(fire(0) == 0 && fire(2) == 0, "Retained-overlay texture action did not dispatch");
	expect(4, 0xc002);
	for (int orientation = 0; orientation < 4; ++orientation) {
		face().tmap_num2 = short((orientation << 14) | 2);
		require(fire(3) == 0, "Super-transparent texture action did not dispatch");
		expect(1, (orientation << 14) | 3);
		ray(true);
		require(fire(1) == 0, "Oriented overlay clear failed");
		expect(1, orientation << 14);
		ray(false);
		// A cleared rotated overlay must not become a destructible texture-zero surface
		const auto original = TmapInfo[0];
		TmapInfo[0].eclip_num = -1;
		TmapInfo[0].destroyed = 4;
		object blower{};
		blower.ctype.laser_info.parent_type = OBJ_PLAYER;
		vms_vector point;
		compute_center_point_on_side(&point, &Segments[front.segnum], front.sidenum);
		const int blew = check_effect_blowup(&Segments[front.segnum], front.sidenum, &point, &blower, 1);
		TmapInfo[0] = original;
		require(blew == 0, "Cleared overlay exposed a phantom destructible texture");
		require(fire(0) == 0, "Overlay restoration failed");
		expect(1, (orientation << 14) | 2);
		ray(false);
	}
	require(fire(5) == 0, "Master texture dispatch failed");
	expect(1, 0xc002);
	require(fire(7) == 0, "Multiple texture targets failed");
	expect(1, 0xc002);
	expect(1, 2, 1);
	require(fire(8) == 0, "14-bit overlay assignment failed");
	expect(1, 0xc002);
	const auto before = snapshot();
	require(fire(9) == 1 && fire(10) == 0 && snapshot() == before, "Disabled or empty texture operation changed geometry");
	for (int variant = 0; variant < 5; ++variant) {
		auto bad = Triggers[0];
		if (variant == 0) bad.value = 65535;
		if (variant == 1) bad.value = (910 << 16) | 1;
		if (variant == 2) bad.seg[0] = -1;
		if (variant == 3) bad.side[0] = 6;
		if (variant == 4) {
			bad.num_links = 2;
			bad.seg[1] = short(Num_segments);
		}
		require(xl_trigger_texture_action(&bad) == 1 && snapshot() == before, "Invalid texture request partially applied");
	}
	require(fire(11) == 0 && fire(11) == 1, "One-shot texture action repeated");
	expect(4, 0xc000);
	int owner = -1;
	for (int i = 0; i < xl_object_trigger_count(); ++i) {
		xl_object_trigger_state action{};
		require(xl_object_trigger_state_get(i, &action), "Texture object binding unavailable");
		if (!(action.flags & 128)) owner = action.owner;
	}
	require(owner >= 0 && Objects[owner].type == OBJ_POWERUP, "Texture removal owner missing");
	Objects[owner].flags |= OF_SHOULD_BE_DEAD;
	obj_delete_all_that_should_be_dead();
	require(Objects[owner].type == OBJ_NONE, "Texture owner removal did not complete");
	expect(4, 0xc000);
	require(uvs() == before_uvs, "Texture actions changed UVs or authored light records");
	sound_route();
	material_contacts();
}
inline xl_trigger_state prepare()
{
	fixture();
	require(fire(1) == 0, "Cannot earn cleared texture state");
	require(fire(4) == 1, "Delayed texture operation did not arm");
	GameTime64 += F1_0;
	xl_trigger_state pending{};
	require(xl_wall_trigger_state(4, &pending) && pending.operated && pending.delay_ms == 6000 && pending.elapsed == F1_0,
	        "Texture pending delay lost its earned phase");
	expect(1, 0xc000);
	return pending;
}
inline void continuation(const xl_trigger_state &saved)
{
	xl_trigger_state current{};
	require(xl_wall_trigger_state(4, &current) && current.elapsed == saved.elapsed && current.delay_ms == saved.delay_ms && current.operated,
	        "Texture restoration lost the pending event");
	GameTime64 += 6 * F1_0 - current.elapsed - 1;
	xl_wall_trigger_frame();
	expect(1, 0xc000);
	++GameTime64;
	xl_wall_trigger_frame();
	expect(4, 0xc002);
}
} // namespace xl_texture_controls
#endif
