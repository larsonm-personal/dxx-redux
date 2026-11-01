#include "camera_glass.h"
#include "../camera_glass.h"
#include "geometry.h"
#include "preview.h"
#include "explosion_motion.h"
#include <functional>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "segment.h"
#include "wall.h"
#include "switch.h"
#include "textures.h"
#include "effects.h"
#include "robot.h"
#include "fireball.h"
#include "newdemo.h"
#include "lighting.h"
#include "digi.h"
#include "sounds.h"
}
namespace
{
bool destructible(int texture)
{
	if (texture <= 0 || texture >= NumTextures) return false;
	const int effect = TmapInfo[texture].eclip_num;
	return effect < 0 ? TmapInfo[texture].destroyed >= 0 : effect < Num_effects && Effects[effect].dest_bm_num >= 0 && !(Effects[effect].flags & EF_ONE_SHOT);
}
bool blowup(int segment, int side, const xl_compat::FixedLightPoint &point, object *blower)
{
	auto &surface = Segments[segment].sides[side];
	const int texture = surface.tmap_num2 & 0x3fff;
	const int wall = surface.wall_num;
	const int trigger = wall >= 0 && wall < Num_walls ? wall_trigger_index(Walls[wall].trigger) : -1;
	const int parent = blower ? blower->ctype.laser_info.parent_num : -1;
	if (parent >= 0 && parent <= Highest_object_index) {
		const auto &actor = Objects[parent];
		if (actor.type != OBJ_PLAYER && (actor.type != OBJ_ROBOT || !Robot_info[actor.id].companion) && trigger >= 0 && trigger < Num_triggers) return false;
	}
	if (!destructible(texture)) return false;
	const bool permanent = trigger >= 0 && trigger < Num_triggers && (Triggers[trigger].flags & 8);
	if (!permanent) subtract_light(segment, side);
	vms_vector position{ point[0], point[1], point[2] };
	if (Newdemo_state == ND_STATE_RECORDING) newdemo_record_effect_blowup(segment, side, &position);
	const int effect = TmapInfo[texture].eclip_num;
	const int clip = effect < 0 ? 3 : Effects[effect].dest_vclip;
	const int size = effect < 0 ? 20 * F1_0 : Effects[effect].dest_size;
	xl_explosion_motion_begin(object_create_explosion(segment, &position, size, clip));
	const int orientation = surface.tmap_num2 & 0xc000;
	if (effect >= 0) {
		auto &source = Effects[effect];
		if (Vclip[clip].sound_num >= 0) digi_link_sound_to_pos(Vclip[clip].sound_num, segment, 0, &position, 0, F1_0);
		if (source.sound_num >= 0) digi_kill_sound_linked_to_segment(segment, side, source.sound_num);
		if (!permanent && source.dest_eclip >= 0 && Effects[source.dest_eclip].segnum == -1) {
			auto &next = Effects[source.dest_eclip];
			next.time_left = next.vc.frame_time;
			next.frame_count = 0;
			next.segnum = segment;
			next.sidenum = side;
			// Default native textures have no alternate frame count or slow motion
			next.flags = (next.flags | EF_ONE_SHOT | source.flags) & ~64;
			next.dest_bm_num = source.dest_bm_num;
			surface.tmap_num2 = short(next.changing_wall_texture | orientation);
		} else if (!permanent) surface.tmap_num2 = short(source.dest_bm_num | orientation);
	} else {
		if (!permanent) surface.tmap_num2 = short(TmapInfo[texture].destroyed | orientation);
		digi_link_sound_to_pos(SOUND_LIGHT_BLOWNUP, segment, 0, &position, 0, F1_0);
	}
	return true;
}
} // namespace
void xl_camera_blast_glass(object &weapon, int damage)
{
	if (!xl_level_extensions_active() || weapon.type != OBJ_WEAPON || weapon.segnum < 0 || weapon.segnum > Highest_segment_index)
		throw std::runtime_error("Glass blast requires a loaded XL weapon");
	const xl_compat::FixedLightPoint position{ weapon.pos.x, weapon.pos.y, weapon.pos.z };
	const auto generation = xl_wall_trigger_generation();
	std::vector<xl_compat::GlassRouteRoom> rooms(Highest_segment_index + 1);
	for (int segment = 0; segment <= Highest_segment_index; ++segment) {
		rooms[segment].center = xl_compat::compile_classic_lighting_side(segment, 0).segment_center;
		for (int side = 0; side < 6; ++side) rooms[segment].children[side] = Segments[segment].children[side];
	}
	std::vector<bool> visited(rooms.size());
	visited[weapon.segnum] = true;
	std::function<void(int, int)> visit = [&](int segment, int depth) {
		if (depth > 5) return;
		++depth;
		for (int side = 0; side < 6; ++side) {
			if (!destructible(Segments[segment].sides[side].tmap_num2 & 0x3fff)) continue;
			const auto point = xl_compat::compile_classic_lighting_side(segment, side).center;
			if (xl_compat::glass_distance(point, position) >= damage / 2) continue;
			// Earlier glass triggers may have changed the current door topology
			for (int room = 0; room <= Highest_segment_index; ++room)
				for (int edge = 0; edge < 6; ++edge)
					rooms[room].transparent[edge] = (WALL_IS_DOORWAY(&Segments[room], edge) & WID_RENDPAST_FLAG) != 0;
			const int distance = xl_compat::camera_glass_route(rooms, segment, weapon.segnum, point, position);
			if (distance <= 0 || distance >= damage / 2) continue;
			const int parent = weapon.ctype.laser_info.parent_num;
			object *actor = parent >= 0 && parent <= Highest_object_index ? &Objects[parent] : nullptr;
			if (blowup(segment, side, point, actor)) {
				const int wall = Segments[segment].sides[side].wall_num;
				const int trigger = wall >= 0 && wall < Num_walls ? wall_trigger_index(Walls[wall].trigger) : -1;
				if (trigger >= 0 && trigger < Num_triggers) {
					const int result = xl_wall_trigger(trigger, actor ? parent : -1, 1);
					if (xl_wall_trigger_generation() != generation) return;
					if (!result && actor && Newdemo_state == ND_STATE_RECORDING) newdemo_record_trigger(segment, side, parent, 1);
				}
			}
		}
		for (int side = 0; side < 6; ++side) {
			const int child = Segments[segment].children[side];
			if (child >= 0 && !visited[child] && (WALL_IS_DOORWAY(&Segments[segment], side) & WID_FLY_FLAG)) {
				visited[child] = true;
				visit(child, depth);
				if (xl_wall_trigger_generation() != generation) return;
			}
		}
	};
	visit(weapon.segnum, 0);
}
