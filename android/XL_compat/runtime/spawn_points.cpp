#include "spawn_points.h"
#include "preview.h"
#include "motion_math.h"
#include <algorithm>
#include <array>
#include <cstring>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "segment.h"
#include "switch.h"
#include "automap.h"
#include "hudmsg.h"
#ifdef __ANDROID__
#include "android_log.h"
#endif
extern int HighlightMarker;
}

namespace
{
std::array<xl_spawn_point, XL_SPAWN_SLOTS> points{};
bool configured = false;
int spawn_marker()
{
	// The reference searches the nine singleplayer marker slots, case sensitively
	for (int slot = 0; slot < 9; ++slot) {
		const int object = MarkerObject[slot];
		if (object >= 0 && object <= Highest_object_index && Objects[object].type == OBJ_MARKER && Objects[object].id == slot &&
		    Objects[object].segnum >= 0 && Objects[object].segnum <= Highest_segment_index && !std::strncmp(MarkerMessage[slot], "SPAWN", MARKER_MESSAGE_LEN)) return slot;
	}
	return -1;
}
xl_spawn_point destination(int segment, int side)
{
	const auto geometry = xl_compat::compile_classic_lighting_side(segment, side);
	const vms_vector direction{
		-static_cast<fix>((std::int64_t(geometry.fixed_normals[0][0]) + geometry.fixed_normals[1][0]) / 2),
		-static_cast<fix>((std::int64_t(geometry.fixed_normals[0][1]) + geometry.fixed_normals[1][1]) / 2),
		-static_cast<fix>((std::int64_t(geometry.fixed_normals[0][2]) + geometry.fixed_normals[1][2]) / 2)
	};
	auto orientation = xl_compat::teleport_matrix(xl_compat::teleport_angles(direction));
	if (xl_compat::teleport_dot(direction, orientation.fvec) < 0) {
		vm_vec_negate(&orientation.fvec);
		vm_vec_negate(&orientation.rvec);
	}
	xl_spawn_point result{};
	result.segment = segment;
	result.side = side;
	std::copy(geometry.segment_center.begin(), geometry.segment_center.end(), result.position);
	int index = 0;
	for (const auto &axis : { orientation.rvec, orientation.uvec, orientation.fvec }) {
		result.orientation[index++] = axis.x;
		result.orientation[index++] = axis.y;
		result.orientation[index++] = axis.z;
	}
	return result;
}
} // namespace

extern "C" void xl_spawn_clear(void)
{
	configured = false;
	points.fill({});
}
extern "C" int xl_spawn_action(const trigger *action)
{
	if (!xl_level_extensions_active() || (Game_mode & GM_MULTI) || !action || action->type != 28 || action->num_links <= 0 || action->num_links > MAX_WALLS_PER_LINK) return 1;
	for (int link = 0; link < action->num_links; ++link)
		if (action->seg[link] < 0 || action->seg[link] > Highest_segment_index || action->side[link] < 0 || action->side[link] >= 6) return 1;
	std::array<xl_spawn_point, XL_SPAWN_SLOTS> next;
	for (int slot = 0; slot < XL_SPAWN_SLOTS; ++slot) {
		const int link = slot % action->num_links;
		next[slot] = destination(action->seg[link], action->side[link]);
	}
	points = next;
	configured = true;
	const int marker = spawn_marker();
	if (marker >= 0) {
		// A trigger effect is not another input command. Keep other marker IDs stable
		obj_delete(MarkerObject[marker]);
		MarkerObject[marker] = -1;
		MarkerMessage[marker][0] = 0;
		HighlightMarker = -1;
	}
	if (!(action->flags & TF_NO_MESSAGE)) HUD_init_message_literal(HM_DEFAULT, "New spawn points set!");
	return 0;
}
extern "C" int xl_spawn_apply(int player, int object_index)
{
	if (!xl_level_extensions_active() || (Game_mode & GM_MULTI) || player != Player_num || player < 0 || player >= XL_SPAWN_SLOTS ||
	    object_index < 0 || object_index > Highest_object_index || object_index != Players[Player_num].objnum || Objects[object_index].type != OBJ_PLAYER) return 0;
	auto &actor = Objects[object_index];
	const int marker = spawn_marker();
	if (marker >= 0) {
		const auto &spawn = Objects[MarkerObject[marker]];
		actor.pos = spawn.pos;
		actor.orient = spawn.orient;
		obj_relink(object_index, spawn.segnum);
#ifdef __ANDROID__
		debug_log(DLOG_GAME, "XL spawn apply: player=%d marker=%d segment=%d position=%d,%d,%d", player, marker, actor.segnum, actor.pos.x, actor.pos.y, actor.pos.z);
#endif
		return 1;
	}
	if (!configured) return 0;
	const auto &point = points[player];
	actor.pos = { point.position[0], point.position[1], point.position[2] };
	actor.orient = { { point.orientation[0], point.orientation[1], point.orientation[2] }, { point.orientation[3], point.orientation[4], point.orientation[5] }, { point.orientation[6], point.orientation[7], point.orientation[8] } };
	obj_relink(object_index, point.segment);
#ifdef __ANDROID__
	debug_log(DLOG_GAME, "XL spawn apply: player=%d authored segment=%d side=%d position=%d,%d,%d", player, point.segment, point.side, actor.pos.x, actor.pos.y, actor.pos.z);
#endif
	return 1;
}
extern "C" int xl_spawn_state_count(void)
{
	return xl_level_extensions_active() && configured ? XL_SPAWN_SLOTS : 0;
}
extern "C" int xl_spawn_state_get(int index, xl_spawn_point *result)
{
	if (!result || index < 0 || index >= xl_spawn_state_count()) return 0;
	*result = points[index];
	return 1;
}
extern "C" int xl_spawn_restore(const xl_spawn_point *saved, int count, int apply)
{
	if (!xl_level_extensions_active() || (count != 0 && count != XL_SPAWN_SLOTS) || (count && !saved)) return 0;
	if (count && !xl_spawn_targets_valid(saved, count)) return 0;
	for (int slot = 0; slot < count; ++slot) {
		const auto &point = saved[slot];
		if (point.segment < 0 || point.segment > Highest_segment_index || point.side < 0 || point.side >= 6) return 0;
		const auto expected = destination(point.segment, point.side);
		if (std::memcmp(&point, &expected, sizeof(point))) return 0;
	}
	if (apply) {
		xl_spawn_clear();
		if (count) {
			std::copy(saved, saved + count, points.begin());
			configured = true;
		}
	}
	return 1;
}
