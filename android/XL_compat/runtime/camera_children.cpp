#include "camera_children.h"
#include "camera_glass.h"
#include <algorithm>
#include "../camera_children.h"
#include "preview.h"
#include "object_birth.h"
extern "C" {
#include "inferno.h"
#include "object.h"
#include "laser.h"
#include "weapon.h"
#include "player.h"
#include "robot.h"
#include "fvi.h"
#include "rng_shared.h"
}
void xl_camera_smart_children(object &weapon)
{
	if (!xl_level_extensions_active() || weapon.type != OBJ_WEAPON || weapon.id >= N_weapon_types)
		throw std::runtime_error("Camera children require an active XL weapon");
	const auto &definition = Weapon_info[weapon.id];
	if (definition.children < 1) return;
	if (definition.children >= N_weapon_types) throw std::runtime_error("Invalid camera child weapon definition");
	if (weapon.id == EARTHSHAKER_ID) {
		const auto generation = xl_wall_trigger_generation();
		xl_camera_blast_glass(weapon, definition.strength[Difficulty_level]);
		if (xl_wall_trigger_generation() != generation) return;
	}
	const auto point = [](const vms_vector &v) { return xl_compat::FixedLightPoint{ v.x, v.y, v.z }; };
	std::vector<xl_compat::CameraSmartTarget> candidates;
	for (int i = 0; i <= Highest_object_index; ++i) {
		const auto &actor = Objects[i];
		if (actor.type == OBJ_PLAYER)
			candidates.push_back({ i, true, false, bool(Players[actor.id].flags & PLAYER_FLAGS_CLOAKED), false, point(actor.pos) });
		else if (actor.type == OBJ_ROBOT)
			candidates.push_back({ i, false, true, bool(actor.ctype.ai_info.CLOAKED), bool(Robot_info[actor.id].companion), point(actor.pos) });
	}
	std::stable_sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) { return xl_object_birth_order(a.owner) < xl_object_birth_order(b.owner); });
	const auto targets = xl_compat::camera_smart_targets(point(weapon.pos), weapon.ctype.laser_info.parent_num,
	                                                     weapon.ctype.laser_info.parent_type == OBJ_PLAYER, weapon.ctype.laser_info.parent_type == OBJ_ROBOT, candidates,
	                                                     [&](int owner) { return object_to_object_visibility(&weapon, &Objects[owner], FQ_TRANSWALL) != 0; });
	for (int child = 0; child < NUM_SMART_CHILDREN; ++child) {
		const int target = xl_compat::camera_smart_choice(targets, [] { return d_rand(); });
		const auto goal = target < 0 ? xl_compat::FixedLightPoint{} : point(Objects[target].pos);
		const auto direction = xl_compat::camera_smart_direction(point(weapon.pos), target < 0 ? nullptr : &goal, [] { return d_rand(); });
		vms_vector native{ direction[0], direction[1], direction[2] };
		const int owner = Laser_create_new(&native, &weapon.pos, weapon.segnum, int(&weapon - Objects), definition.children, child == 0);
		if (owner >= 0) Objects[owner].ctype.laser_info.track_goal = target;
	}
}
