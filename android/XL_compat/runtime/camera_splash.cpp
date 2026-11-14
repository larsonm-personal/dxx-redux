#include "camera_splash.h"
#include "boss_spawn.h"
#include "camera_collision.h"
#include "camera_flash.h"
#include "camera_children.h"
#include "seismic_gameplay.h"
#include "../camera_flash.h"
#include "../camera_splash.h"
#include "../camera_contact.h"
#include "../robot_damage.h"
#include "object_birth.h"
#include "explosion_motion.h"
#include "motion_math.h"
#include "speed_boost.h"
#include "gameplay_audio.h"
#include <algorithm>
#include <cstring>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "weapon.h"
#include "laser.h"
#include "robot.h"
#include "fireball.h"
#include "game.h"
#include "ai.h"
#include "collide.h"
#include "fvi.h"
#include "physics.h"
#include "automap.h"
#include "multi.h"
#include "rng_shared.h"
#include "gauges.h"
#include "dxxerror.h"
}
#undef min
#undef max

namespace
{
using xl_compat::FixedLightPoint;
using xl_compat::splash_divide;
using xl_compat::splash_multiply;
FixedLightPoint point(const vms_vector &v)
{
	return { v.x, v.y, v.z };
}
vms_vector native(const FixedLightPoint &v)
{
	return { v[0], v[1], v[2] };
}
void apply_force(object &actor, const FixedLightPoint &force)
{
	if (actor.movement_type != MT_PHYSICS) return;
	const bool blocked = object_is_observer(&actor) || (&actor == ConsoleObject && (Automap_active || xl_speed_boost_player_locked()));
	actor.mtype.phys_info.velocity = native(xl_compat::camera_force_velocity(point(actor.mtype.phys_info.velocity), force, actor.mtype.phys_info.mass, blocked));
}
} // namespace

// Pinned ApplyRotForce/TurnTowardsVector with the already qualified XL angle conversion
void xl_camera_splash_rotation(object &actor, const vms_vector &input)
{
	const auto force = point(input);
	if (actor.movement_type != MT_PHYSICS) return;
	const int magnitude = xl_compat::geometry_default_magnitude(force) / 8;
	int rate = 4 * F1_0;
	if (magnitude >= F1_0 / 256 && magnitude >= (actor.mtype.phys_info.mass >> 14)) {
		rate = splash_divide(actor.mtype.phys_info.mass, magnitude);
		if (actor.type == OBJ_ROBOT) {
			rate = std::max(rate, F1_0 / 4);
			const auto &definition = Robot_info[actor.id];
			if (!definition.thief && !definition.attack_type && actor.ctype.ai_info.SKIP_AI_COUNT * FrameTime < 3 * F1_0 / 4) {
				const int time = splash_divide(F1_0, 8 * FrameTime);
				actor.ctype.ai_info.SKIP_AI_COUNT += time / F1_0 + (d_rand() * 2 < (time & 65535));
			}
		} else rate = std::max(rate, F1_0 / 2);
	}
	if ((&actor == ConsoleObject && Automap_active) || force == FixedLightPoint{}) return;
	if (actor.control_type == CT_MORPH) rate *= 2;
	const auto goal = xl_compat::teleport_angles(native(force));
	const auto current = xl_compat::teleport_angles(actor.orient.fvec);
	const auto turn = [&](int destination, int present, int previous) {
		int delta = destination - present;
		if (delta > F1_0 / 2) delta -= F1_0;
		if (delta < -F1_0 / 2) delta += F1_0;
		delta = splash_divide(delta, rate);
		if (std::abs(delta) < F1_0 / 16) delta *= 4;
		return ((delta ^ previous) >= 0 || std::abs(delta) >= F1_0 / 8) ? delta : delta / 4;
	};
	auto &rotation = actor.mtype.phys_info.rotvel;
	rotation = { turn(goal.p, current.p, rotation.x), turn(goal.h, current.h, rotation.y), 0 };
}

bool xl_camera_splash_profile(int id)
{
	return xl_camera_splash_profile(id, Weapon_info, N_weapon_types);
}
bool xl_camera_splash_profile(int id, const weapon_info *weapons, int count)
{
	if (!weapons || count < 0 || count > MAX_WEAPON_TYPES || id < 0 || id >= count) return false;
	const auto &definition = weapons[id];
	const bool mine = is_proximity_bomb_or_smart_mine_or_placed_mine(static_cast<weapon_type_t>(id)) || id == ROBOT_SUPERPROX_ID;
	// Source flash processing is driven by the weapon data, including custom missiles
	if ((!xl_camera_contact_projectile(id, weapons, count) && !mine) || definition.flash < 0 || definition.children < -1 || definition.children >= count) return false;
	const int radius = definition.damage_radius;
	for (int difficulty = 0; difficulty < NDL; ++difficulty) {
		const int strength = definition.strength[difficulty];
		if (double(strength) * definition.flash > INT32_MAX) return false;
		// The source mine-chain test multiplies distance by force in signed fixed point
		if (strength <= 0 || radius <= 0 || double(strength) / F1_0 * double(radius) / 4.0 + double(radius) / F1_0 > INT32_MAX) return false;
	}
	return true;
}
static void camera_radial_explosion(object &weapon, vms_vector impact)
{
	const auto &definition = Weapon_info[weapon.id];
	if (!xl_camera_splash_profile(weapon.id)) Error("Camera splash weapon profile requires further qualification");
	const int strength = definition.strength[Difficulty_level], radius = definition.damage_radius;
	try {
		if (weapon.id == EARTHSHAKER_ID) xl_seismic_detonation(&impact);
		xl_gameplay_weapon_explosion(int(&weapon - Objects), 0);
		auto *explosion = object_create_explosion(weapon.segnum, &impact, definition.impact_size, definition.robot_hit_vclip);
		if (!explosion) return;
		xl_explosion_motion_begin(explosion);
		const int parent = weapon.ctype.laser_info.parent_num;
		object *killer = parent >= 0 && parent <= Highest_object_index ? &Objects[parent] : nullptr;
		std::vector<int> order;
		for (int index = 0; index <= Highest_object_index; ++index)
			if (Objects[index].type != OBJ_NONE) order.push_back(index);
		std::stable_sort(order.begin(), order.end(), [](int a, int b) { return xl_object_birth_order(a) < xl_object_birth_order(b); });
		for (int index : order) {
			auto &actor = Objects[index];
			if (&actor == &weapon || actor.flags & OF_SHOULD_BE_DEAD) continue;
			if (actor.type == OBJ_WEAPON) {
				if (!is_proximity_bomb_or_smart_mine_or_placed_mine(static_cast<weapon_type_t>(actor.id)) && actor.id != ROBOT_SUPERPROX_ID) continue;
			} else if (actor.type == OBJ_ROBOT) {
				if (parent < 0 || (killer && killer->type == OBJ_ROBOT && killer->id == actor.id)) continue;
			} else if (actor.type != OBJ_CNTRLCEN && actor.type != OBJ_PLAYER) continue;
			const auto sample = xl_compat::camera_splash_sample(point(actor.pos), point(impact), strength, radius, strength);
			if (!sample.inside || !object_to_object_visibility(explosion, &actor, FQ_TRANSWALL)) continue;
			int damage = sample.damage;
			if (actor.type == OBJ_WEAPON) {
				apply_force(actor, sample.impulse);
				if (splash_multiply(sample.distance, sample.force) > 8000 * F1_0) {
					// Mark before recursion so overlapping mine blasts cannot detonate it twice
					actor.flags |= OF_SHOULD_BE_DEAD;
					camera_radial_explosion(actor, actor.pos);
				}
			} else if (actor.type == OBJ_ROBOT) {
				apply_force(actor, sample.impulse);
				int skip = actor.ctype.ai_info.SKIP_AI_COUNT;
				auto thrust = point(actor.mtype.phys_info.rotthrust);
				if (xl_compat::camera_flash_robot(skip, thrust, sample.impulse, definition.flash,
				                                  Robot_info[actor.id].boss_flag != 0, explosion->ctype.ai_info.SKIP_AI_COUNT, FrameTime, [] { return d_rand(); })) {
					actor.mtype.phys_info.rotthrust = native(thrust);
					actor.mtype.phys_info.flags |= PF_USES_THRUST;
				}
				actor.ctype.ai_info.SKIP_AI_COUNT = skip;
				auto rotation = sample.impulse;
				for (auto &axis : rotation) axis = splash_multiply(axis, -2 * F1_0 * (7 - Difficulty_level) / 8);
				xl_camera_splash_rotation(actor, native(rotation));
				const auto &robot = Robot_info[actor.id];
				const int boss = xl_boss_effective_profile(robot.boss_flag) - BOSS_D2;
				if (boss >= 0 && boss < NUM_D2_BOSSES && Boss_invulnerable_matter[boss]) damage /= 4;
				const bool killer_robot = killer && killer->type == OBJ_ROBOT;
				const bool killer_guidebot = killer_robot && Robot_info[killer->id].companion;
				if (xl_compat::robot_damage_allowed(true, actor.flags & OF_EXPLODING, actor.shields, robot.strength,
				                                    GameTime64, xl_object_birth_time(index), false, killer_robot, killer_guidebot) &&
				    apply_damage_to_robot(&actor, damage, parent) &&
				    !(Game_suspended & SUSP_ROBOTS) && parent == Players[Player_num].objnum)
					add_points_to_score(robot.score_value);
				if (!definition.flash && robot.companion) {
					char message[27]{};
					const int count = std::max(1, std::min(4, damage / 8 / F1_0));
					for (int i = 0; i < count; ++i) std::strcat(message, "ouch! ");
					buddy_message(message);
				}
			} else if (actor.type == OBJ_CNTRLCEN) {
				if (actor.shields >= 0) apply_damage_to_controlcen(&actor, damage, parent);
			} else {
				if (definition.flash && actor.id == Player_num)
					xl_camera_flash_blast(sample.force, definition.flash, weapon.ctype.laser_info.parent_signature == ConsoleObject->signature);
				auto rotation = sample.impulse;
				if (parent >= 0 && killer != ConsoleObject)
					for (auto &axis : rotation) axis /= 2;
				for (auto &axis : rotation) axis /= 2;
				apply_force(actor, sample.impulse);
				xl_camera_splash_rotation(actor, native(rotation));
				if (!Difficulty_level) damage /= 4;
				if (actor.shields >= 0) apply_damage_to_player(&actor, killer, damage, 0);
			}
		}
		xl_camera_smart_children(weapon);
	} catch (const std::exception &error) {
		Error("Cannot apply camera splash: %s", error.what());
	}
}

void xl_camera_splash(object &weapon, object &camera, vms_vector &impact)
{
	impact = native(xl_compat::camera_splash_impact(point(impact), point(camera.pos), camera.size, weapon.size));
	camera_radial_explosion(weapon, impact);
}
