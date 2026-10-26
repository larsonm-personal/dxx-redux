#include "preview.h"
#include "geometry.h"
#include <cstdint>
#include <cstdlib>
#include <cmath>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "segment.h"
#include "switch.h"
#include "wall.h"
#include "gameseg.h"
#include "vecmat.h"
#include "rng_shared.h"
#include "digi.h"
#include "sounds.h"
#include "hudmsg.h"
extern int Do_appearance_effect;
}

static vms_angvec teleport_angles(const vms_vector &vector)
{
	const auto direction = xl_compat::geometry_fixed_direction({ vector.x, vector.y, vector.z });
	vms_angvec angles{};
	// XL retains the positive endpoint for both signs at exactly unit magnitude
	angles.p = std::abs(direction[1]) >= F1_0 ? 0x4000 : fix_asin(-direction[1]);
	angles.h = direction[0] || direction[2] ? fix_atan2(direction[2], direction[0]) : 0;
	return angles;
}

static fix teleport_dot(const vms_vector &a, const vms_vector &b)
{
	return static_cast<fix>((double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z) / 65536.0);
}

static vms_matrix teleport_matrix(const vms_angvec &angles)
{
	const auto sine = [](fixang a) { return static_cast<fix>(std::sin(double(a) * 2.0 * 3.14159265358979323846 / 65536.0) * 65536.0); };
	const auto cosine = [](fixang a) { return static_cast<fix>(std::cos(double(a) * 2.0 * 3.14159265358979323846 / 65536.0) * 65536.0); };
	const auto multiply = [](fix a, fix b) { return static_cast<fix>(double(b) / 65536.0 * a); };
	const fix sp = sine(angles.p), cp = cosine(angles.p), sb = sine(angles.b), cb = cosine(angles.b), sh = sine(angles.h), ch = cosine(angles.h);
	const fix sbsh = multiply(sb, sh), cbch = multiply(cb, ch), cbsh = multiply(cb, sh), sbch = multiply(sb, ch);
	vms_matrix result;
	result.rvec = { cbch + multiply(sp, sbsh), multiply(sb, cp), multiply(sp, sbch) - cbsh };
	result.uvec = { multiply(sp, cbsh) - sbch, multiply(cb, cp), sbsh + multiply(sp, cbch) };
	result.fvec = { multiply(sh, cp), -sp, multiply(ch, cp) };
	return result;
}

extern "C" void xl_wall_teleport_crossings(int object_index, const int *segments, int count)
{
	if (!xl_level_extensions_active()) return;
	for (int i = 0; i + 1 < count; ++i) {
		const int from = segments[i], to = segments[i + 1];
		if (from < 0 || from > Highest_segment_index || to < 0 || to > Highest_segment_index) continue;
		const int side = find_connect_side(&Segments[to], &Segments[from]);
		if (side < 0) continue;
		const int wall = Segments[from].sides[side].wall_num;
		if (wall < 0 || wall >= Num_walls) continue;
		const int trigger = Walls[wall].trigger;
		const int result = xl_wall_teleport(trigger, object_index, 0);
		if (result >= 0) xl_preview_trigger_result(trigger, 0, result);
	}
}

extern "C" int xl_wall_teleport_actor(int object_index)
{
	if (!xl_level_extensions_active() || object_index < 0 || object_index > Highest_object_index) return 0;
	const int type = Objects[object_index].type;
	return type == OBJ_ROBOT || type == OBJ_DEBRIS || type == OBJ_CNTRLCEN || type == OBJ_HOSTAGE || type == OBJ_POWERUP;
}

// Return -1 for stock dispatch, otherwise the ordinary trigger result convention
extern "C" int xl_wall_teleport(int trigger_index, int object_index, int shot)
{
	if (!xl_level_extensions_active() || trigger_index < 0 || trigger_index >= Num_triggers || Triggers[trigger_index].type != 14) return -1;
	auto &trigger = Triggers[trigger_index];
	if ((Game_mode & GM_MULTI) || (trigger.flags & TF_DISABLED) || object_index < 0 || object_index > Highest_object_index) return 1;
	auto &object = Objects[object_index];
	const bool player = object.type == OBJ_PLAYER;
	if (player) {
		if (object_index != Players[Player_num].objnum || Players[Player_num].shields < 0 || Player_is_dead) return 1;
	} else if (object.type != OBJ_ROBOT && object.type != OBJ_DEBRIS && object.type != OBJ_CNTRLCEN && object.type != OBJ_HOSTAGE && object.type != OBJ_POWERUP) return 1;
	const auto delay = std::abs(std::int64_t(trigger.time));
	if ((trigger.flags & ~7) || (delay >= 100 && delay <= 900000) || trigger.num_links < 0 || trigger.num_links > MAX_WALLS_PER_LINK) return 1;
	for (int i = 0; i < trigger.num_links; ++i)
		if (trigger.seg[i] < 0 || trigger.seg[i] > Highest_segment_index || trigger.side[i] < 0 || trigger.side[i] >= 6) return 1;
	if (trigger.num_links) {
		// XL selects rand() % links; use the engine's simulation stream for replay
		const int destination = d_rand() % trigger.num_links;
		const int segment = trigger.seg[destination], side = trigger.side[destination];
		const auto geometry = xl_compat::compile_classic_lighting_side(segment, side);
		vms_vector direction;
		direction.x = -static_cast<fix>((std::int64_t(geometry.fixed_normals[0][0]) + geometry.fixed_normals[1][0]) / 2);
		direction.y = -static_cast<fix>((std::int64_t(geometry.fixed_normals[0][1]) + geometry.fixed_normals[1][1]) / 2);
		direction.z = -static_cast<fix>((std::int64_t(geometry.fixed_normals[0][2]) + geometry.fixed_normals[1][2]) / 2);
		const auto target = teleport_angles(direction);
		object.orient = teleport_matrix(target);
		if (teleport_dot(direction, object.orient.fvec) < 0) {
			vm_vec_negate(&object.orient.fvec);
			vm_vec_negate(&object.orient.rvec);
		}
		auto velocity_angles = teleport_angles(object.mtype.phys_info.velocity);
		velocity_angles.p -= target.p;
		velocity_angles.b -= target.b;
		velocity_angles.h -= target.h;
		const auto rotation = teleport_matrix(velocity_angles);
		vms_vector velocity;
		velocity.x = teleport_dot(object.mtype.phys_info.velocity, rotation.rvec);
		velocity.y = teleport_dot(object.mtype.phys_info.velocity, rotation.uvec);
		velocity.z = teleport_dot(object.mtype.phys_info.velocity, rotation.fvec);
		object.mtype.phys_info.velocity = velocity;
		object.pos = { geometry.segment_center[0], geometry.segment_center[1], geometry.segment_center[2] };
		obj_relink(object_index, segment);
		Do_appearance_effect = 1;
	}
	digi_play_sample(SOUND_SECRET_EXIT, F1_0);
	if (player && shot && !(trigger.flags & TF_NO_MESSAGE)) HUD_init_message_literal(HM_DEFAULT, "Teleport!");
	if (trigger.flags & TF_ONE_SHOT) trigger.flags |= TF_DISABLED;
	return 0;
}
