#include "camera_collision.h"
#include "../camera_contact.h"
#include "../static_contact.h"
#include <map>
#include "speed_boost.h"
#include "projectile.h"
#include "preview.h"
#include "camera_splash.h"
#include "seismic_gameplay.h"
#include "explosion_motion.h"
#include "camera_controller.h"
#include "../camera_sphere_unstick.h"
extern "C" {
#include "inferno.h"
#include "object.h"
#include "robot.h"
#include "weapon.h"
#include "fireball.h"
#include "gameseg.h"
#include "ai.h"
#include "automap.h"
#include "digi.h"
#include "sounds.h"
#include "dxxerror.h"
#include "multi.h"
#include "laser.h"
#include "game.h"
#include "rng_shared.h"
}

namespace
{
struct ActorHitState {
	int signature = -1, type = OBJ_NONE;
	xl_compat::CameraHitState hits;
};
std::array<ActorHitState, MAX_OBJECTS> actor_hits{};
unsigned player_weapon_contacts = 0;
std::map<int, object> static_restore_slots;
const robot_info &camera_definition(const object &camera)
{
	if (camera.control_type != 18 || camera.movement_type != MT_NONE ||
	    (camera.ctype.ai_info.behavior != 128 && camera.ctype.ai_info.behavior != 129) ||
	    camera.id >= N_robot_types || (Game_mode & GM_MULTI))
		Error("Camera contact exceeds qualified object profile");
	const auto &definition = Robot_info[camera.id];
	if (definition.strength != -1 || definition.attack_type || definition.kamikaze || definition.thief || definition.companion)
		Error("Camera contact requires the original normalized camera definition");
	return definition;
}
void reflect_actor(object &actor)
{
	// Source ApplyForce leaves nonphysical Omega blobs unchanged
	if (actor.movement_type != MT_PHYSICS) return;
	const auto &velocity = actor.mtype.phys_info.velocity;
	const bool blocked = object_is_observer(&actor) ||
	                     (&actor == ConsoleObject && (Automap_active || xl_speed_boost_player_locked()));
	const auto result = xl_compat::camera_reflect_velocity({ velocity.x, velocity.y, velocity.z }, actor.mtype.phys_info.mass, blocked);
	actor.mtype.phys_info.velocity = { result[0], result[1], result[2] };
}
bool contact_projectile(int id)
{
	switch (id) {
		case LASER_ID_L1:
		case LASER_ID_L2:
		case LASER_ID_L3:
		case LASER_ID_L4:
		case LASER_ID_L5:
		case LASER_ID_L6:
		case FLARE_ID:
		case VULCAN_ID:
		case SPREADFIRE_ID:
		case PLASMA_ID:
		case FUSION_ID:
		case GAUSS_ID:
		case HELIX_ID:
		case PHOENIX_ID:
		case OMEGA_ID:
		case CONCUSSION_ID:
		case HOMING_ID:
		case GUIDEDMISS_ID:
		case PROXIMITY_ID:
		case SMART_ID:
		case MEGA_ID:
		case SUPERPROX_ID:
		case EARTHSHAKER_ID:
		case PLAYER_SMART_HOMING_ID:
		case SMART_MINE_HOMING_ID:
		case EARTHSHAKER_MEGA_ID:
		case PMINE_ID:
		case FLASH_ID:
		case MERCURY_ID: return true;
		default: return false;
	}
}
} // namespace

void xl_camera_validate_weapon_definitions()
{
	for (int id = 0; id < N_weapon_types; ++id) {
		if (!contact_projectile(id)) continue;
		const auto &definition = Weapon_info[id];
		if (definition.damage_radius ? !xl_camera_splash_profile(id) : definition.children != -1 || definition.flash)
			throw std::runtime_error("Camera weapon definition exceeds the qualified gameplay profile: " + std::to_string(id));
	}
}

extern "C" void xl_camera_hit_state_clear(void)
{
	actor_hits.fill({});
	player_weapon_contacts = 0;
}
extern "C" unsigned xl_camera_player_weapon_contacts(void)
{
	return player_weapon_contacts;
}
extern "C" void xl_camera_hit_state_created(int owner)
{
	if (owner >= 0 && owner < object_capacity()) actor_hits[owner] = {};
}
bool xl_camera_hit_state_get(int owner, xl_compat::CameraHitState &state)
{
	if (!xl_level_extensions_active() || owner < 0 || owner > Highest_object_index || Objects[owner].type == OBJ_NONE) return false;
	const auto &entry = actor_hits[owner];
	if (entry.signature != Objects[owner].signature || entry.type != Objects[owner].type) return false;
	state = entry.hits;
	return true;
}
std::vector<XlCameraHitSave> xl_camera_hit_snapshot()
{
	std::vector<XlCameraHitSave> result;
	if (!xl_level_extensions_active()) return result;
	const auto ticks = xl_camera_controller_ticks();
	if (ticks > INT32_MAX) throw std::runtime_error("Camera hit save clock exceeds qualified signed range");
	for (int owner = 0; owner <= Highest_object_index; ++owner) {
		const auto &actor = Objects[owner];
		if (actor.type != OBJ_PLAYER && actor.type != OBJ_WEAPON && actor.type != OBJ_ROBOT) continue;
		xl_compat::CameraHitState state;
		xl_camera_hit_state_get(owner, state);
		result.push_back({ owner, actor.signature, actor.type, state.repaired_elapsed(int32_t(ticks)) });
	}
	return result;
}
bool xl_camera_hit_restore(const std::vector<XlCameraHitSave> &saved, bool apply)
{
	if (!xl_level_extensions_active() || saved.size() > MAX_OBJECTS || xl_camera_controller_ticks() > INT32_MAX) return false;
	try {
		std::vector<ActorHitState> prepared(MAX_OBJECTS);
		std::size_t row = 0;
		for (int owner = 0; owner <= Highest_object_index; ++owner) {
			const auto &actor = Objects[owner];
			if (actor.type != OBJ_PLAYER && actor.type != OBJ_WEAPON && actor.type != OBJ_ROBOT) continue;
			if (row == saved.size()) return false;
			const auto &entry = saved[row++];
			if (entry.owner != owner || entry.signature < 0 || entry.signature != actor.signature || entry.type != actor.type) return false;
			auto &state = prepared[owner];
			state.signature = entry.signature;
			state.type = entry.type;
			state.hits.restore_repaired(int32_t(xl_camera_controller_ticks()), entry.elapsed);
		}
		if (row != saved.size()) return false;
		if (apply) std::copy(prepared.begin(), prepared.end(), actor_hits.begin());
		return true;
	} catch (const std::runtime_error &) {
		return false;
	}
}
extern "C" void xl_camera_physics_unstick(object *actor, const object *camera,
                                          const vms_vector *old_position, const vms_vector *attempted_position,
                                          const vms_vector *offset, const vms_vector *old_velocity)
{
	try {
		if (!xl_level_extensions_active() || !actor || !camera || !old_position || !attempted_position || !offset || !old_velocity ||
		    (actor->type != OBJ_PLAYER && actor->type != OBJ_WEAPON && actor->type != OBJ_ROBOT && actor->type != OBJ_FIREBALL) || actor->movement_type != MT_PHYSICS || (camera->type != 16 && !xl_static_robot(camera)))
			throw std::runtime_error("Camera unstick exceeds the qualified physical actor profile");
		if (camera->type == 16) camera_definition(*camera);
		const int owner = int(actor - Objects);
		if (owner < 0 || owner > Highest_object_index) throw std::runtime_error("Camera unstick actor is not installed");
		auto &entry = actor_hits[owner];
		if (entry.signature != actor->signature || entry.type != actor->type) entry = { actor->signature, actor->type, {} };
		const auto point = [](const vms_vector &v) { return xl_compat::FixedLightPoint{ v.x, v.y, v.z }; };
		xl_compat::CameraUnstickState state{ point(*old_position), point(*attempted_position), point(*offset) };
		auto position = point(actor->pos);
		// Qualified shader-enabled source profile; renderer policy remains an admission gate
		xl_compat::camera_sphere_unstick(state, entry.hits, position, point(camera->pos), point(*old_velocity), actor->size, actor->type,
		                                 int32_t(xl_camera_controller_ticks()), 2000);
		actor->pos = { position[0], position[1], position[2] };
	} catch (const std::exception &error) {
		Error("Cannot unstick XL camera contact: %s", error.what());
	}
}

extern "C" int xl_camera_pair_collision(int a, int b)
{
	if (a != 16 && b != 16) return -1;
	const int other = a == 16 ? b : a;
	return other == OBJ_PLAYER || other == OBJ_WEAPON || other == OBJ_ROBOT || other == OBJ_FIREBALL;
}

extern "C" int xl_camera_passive_contact(object *a, object *b)
{
	object *camera = a->type == 16 ? a : b->type == 16 ? b
	                                                   : nullptr;
	if (!camera) return 0;
	const object *actor = camera == a ? b : a;
	if (actor->type != OBJ_ROBOT && actor->type != OBJ_FIREBALL) return 0;
	camera_definition(*camera);
	// Source enables both pairs but its dispatcher applies no response
	// The caller still performs reverse-query unsticking and movement retry
	return 1;
}

extern "C" int xl_camera_player_contact(object *a, object *b, vms_vector *point)
{
	object *camera = a->type == 16 ? a : b->type == 16 ? b
	                                                   : nullptr;
	if (!camera) return 0;
	object *player = camera == a ? b : a;
	if (player->type != OBJ_PLAYER) return 0;
	// Static behavior uses XL's separate geometry collision model, still unqualified
	const auto &definition = camera_definition(*camera);
	if (camera->flags & OF_EXPLODING) return 1;
	const int segment = find_point_seg(point, player->segnum);
	if (segment != -1)
		xl_explosion_motion_begin(object_create_explosion(segment, point, Weapon_info[0].impact_size, Weapon_info[0].wall_hit_vclip));
	if (player->id == Player_num) {
		create_awareness_event(player, PA_PLAYER_COLLISION);
		// Original attackType is zero and DoAIRobotHit returns for control != CT_AI
	}
	if (!definition.energy_drain)
		digi_link_sound_to_pos(SOUND_ROBOT_HIT_PLAYER, player->segnum, 0, point, 0, F1_0);
	try {
		reflect_actor(*player);
	} catch (const std::exception &error) {
		Error("Cannot apply camera contact force: %s", error.what());
	}
	return 1;
}

extern "C" int xl_camera_weapon_contact(object *a, object *b, vms_vector *point)
{
	object *camera = a->type == 16 ? a : b->type == 16 ? b
	                                                   : nullptr;
	if (!camera) return 0;
	object *weapon = camera == a ? b : a;
	if (weapon->type != OBJ_WEAPON) return 0;
	camera_definition(*camera);
	if (!xl_level_extensions_active() || weapon->id >= N_weapon_types)
		Error("Camera weapon contact requires active XL definitions");
	// Reprogrammed robot shooters and the robot-hit-robot cheat need separate qualification
	if (cheats.robotskillrobots) Error("Camera weapon contact with robot-hit-robot cheat is unqualified");
	if (weapon->ctype.laser_info.parent_type == OBJ_ROBOT) return 1;
	const auto &definition = Weapon_info[weapon->id];
	// Missile lightning rods use the source-supported explosion-decoration-off profile
	if (!contact_projectile(weapon->id))
		Error("Camera missile blast and splash contact is not yet qualified (weapon=%d radius=%d)", weapon->id, definition.damage_radius);
	// Optional missile blast decoration is deferred; impact and radial effects remain active
	if (weapon->id == EARTHSHAKER_ID) xl_seismic_detonation(&weapon->pos);
	// Single-player Omega passes the source distance gate
	if ((weapon->mtype.phys_info.flags & PF_PERSISTENT) && xl_projectile_add_hit(int(weapon - Objects), int(camera - Objects)) < 0) return 1;
	if (weapon->ctype.laser_info.parent_signature == camera->signature) return 1;
	if (weapon->ctype.laser_info.parent_type == OBJ_PLAYER && weapon->ctype.laser_info.parent_num == Players[Player_num].objnum) ++player_weapon_contacts;
	if (definition.damage_radius) {
		const auto generation = xl_wall_trigger_generation();
		xl_camera_splash(*weapon, *camera, *point);
		if (xl_wall_trigger_generation() != generation) return 1;
	}
	if (weapon->ctype.laser_info.parent_type == OBJ_PLAYER && !(camera->flags & OF_EXPLODING)) {
		if (weapon->ctype.laser_info.parent_num == Players[Player_num].objnum)
			create_awareness_event(weapon, PA_WEAPON_ROBOT_COLLISION);
		// ROBOTINFO(object pointer) is null for cameras: use weapon impact, no robot impact sound
		if (definition.robot_hit_vclip > -1) {
			auto *flash = object_create_explosion(weapon->segnum, point, definition.impact_size, definition.robot_hit_vclip);
			if (flash) {
				xl_explosion_motion_begin(flash);
				obj_attach(camera, flash);
			}
		}
		try {
			// Source damage returns immediately for the normalized strength -1 definition
			if (!(weapon->flags & OF_HARMLESS)) reflect_actor(*weapon);
			if (weapon->id == GAUSS_ID) {
				int skip = camera->ctype.ai_info.SKIP_AI_COUNT;
				std::array<std::int32_t, 3> thrust{};
				if (xl_compat::camera_gauss_spin(skip, FrameTime, thrust, [] { return d_rand(); })) {
					camera->ctype.ai_info.SKIP_AI_COUNT = skip;
					camera->mtype.phys_info.rotthrust = { thrust[0], thrust[1], thrust[2] };
					camera->mtype.phys_info.flags |= PF_USES_THRUST;
				}
			}
		} catch (const std::exception &error) {
			Error("Cannot apply camera weapon contact: %s", error.what());
		}
	}
	xl_projectile_impact(int(weapon - Objects), int(camera - Objects));
	return 1;
}

extern "C" int xl_static_robot(const object *actor)
{
	return xl_level_extensions_active() && actor && actor->type == OBJ_ROBOT && actor->ctype.ai_info.behavior == 135;
}
extern "C" int xl_static_robot_bump(object *a, object *b)
{
	object *fixed = xl_static_robot(a) ? a : xl_static_robot(b) ? b
	                                                            : nullptr;
	if (!fixed) return 0;
	object *actor = fixed == a ? b : a;
	if (xl_static_robot(actor)) return 1;
	try {
		if (fixed->movement_type != MT_NONE || fixed->control_type != CT_NONE ||
		    fixed->mtype.phys_info.velocity.x || fixed->mtype.phys_info.velocity.y || fixed->mtype.phys_info.velocity.z)
			throw std::runtime_error("Static contact exceeds the stationary authored profile");
		// Source nonphysical-other branch computes force from the zero-velocity static owner
		if (actor->movement_type != MT_PHYSICS) return 1;
		const auto point = [](const vms_vector &v) { return xl_compat::FixedLightPoint{ v.x, v.y, v.z }; };
		const bool player = actor->type == OBJ_PLAYER;
		// Default sphere physics passes a zero normal to player contact; weapon bump passes null
		const auto normal = player ? std::optional<xl_compat::FixedLightPoint>({ 0, 0, 0 }) : std::nullopt;
		const auto result = xl_compat::static_contact(point(fixed->pos), fixed->mtype.phys_info.mass,
		                                              point(actor->pos), point(actor->mtype.phys_info.velocity), actor->mtype.phys_info.mass,
		                                              player, actor->mtype.phys_info.flags & PF_PERSISTENT,
		                                              player && actor == ConsoleObject && xl_speed_boost_player_locked(), normal);
		if (result.apply) {
			actor->mtype.phys_info.velocity = { result.velocity[0], result.velocity[1], result.velocity[2] };
			const vms_vector force{ result.rotation_force[0], result.rotation_force[1], result.rotation_force[2] };
			xl_camera_splash_rotation(*actor, force);
		}
	} catch (const std::exception &error) {
		Error("Cannot apply static object contact: %s", error.what());
	}
	return 1;
}
extern "C" int xl_static_robot_player_contact(object *robot, object *player)
{
	if (!xl_static_robot(robot)) return 0;
	// Source static player contact skips impact effects, sound, awareness and attack AI
	return xl_static_robot_bump(robot, player);
}
extern "C" int xl_static_robot_gauss(object *robot)
{
	if (!xl_static_robot(robot)) return 0;
	try {
		int skip = robot->ctype.ai_info.SKIP_AI_COUNT;
		std::array<std::int32_t, 3> thrust{};
		if (xl_compat::camera_gauss_spin(skip, FrameTime, thrust, [] { return d_rand(); })) {
			robot->ctype.ai_info.SKIP_AI_COUNT = skip;
			robot->mtype.phys_info.rotthrust = { thrust[0], thrust[1], thrust[2] };
			robot->mtype.phys_info.flags |= PF_USES_THRUST;
		}
	} catch (const std::exception &error) {
		Error("Cannot apply static Gauss contact: %s", error.what());
	}
	return 1;
}
extern "C" int xl_static_robot_query(const object *target, const object *moving,
                                     const vms_vector *start, const vms_vector *end, int radius,
                                     vms_vector *hit, vms_vector *normal)
{
	try {
		if (!xl_static_robot(target) || !moving || !start || !end || !hit || !normal)
			throw std::runtime_error("Static query has no qualified owner");
		const int owner = int(target - Objects);
		if (owner < 0 || owner > Highest_object_index) throw std::runtime_error("Static query owner is not installed");
		const auto point = [](const vms_vector &v) { return xl_compat::FixedLightPoint{ v.x, v.y, v.z }; };
		xl_compat::FixedLightPoint intersection{};
		const int size = radius < 0 ? 0 : target->size;
		const int result = xl_compat::model_sphere_query(intersection, point(*start), point(*end), point(target->pos), xl_compat::hitbox_fixed(double(size) + radius));
		if (result < 0) return 0;
		*hit = { intersection[0], intersection[1], intersection[2] };
		*normal = {};
		if (moving->type != OBJ_POWERUP) {
			auto &entry = actor_hits[owner];
			if (entry.signature != target->signature || entry.type != target->type) entry = { target->signature, target->type, {} };
			entry.hits.register_hit(intersection, point(target->pos), target->size, target->type, int32_t(xl_camera_controller_ticks()), 2000);
		}
		return result;
	} catch (const std::exception &error) {
		Error("Cannot query static robot geometry: %s", error.what());
	}
	return 0;
}

extern "C" void xl_static_robot_restore_clear(void)
{
	static_restore_slots.clear();
}
extern "C" void xl_static_robot_restore_prepare(void)
{
	static_restore_slots.clear();
	if (!xl_level_extensions_active()) return;
	// Source LoadState leaves omitted unions in freshly initialized level slots
	for (int i = 0; i <= Highest_object_index; ++i) {
		const auto &actor = Objects[i];
		if (actor.type == OBJ_ROBOT && actor.control_type == CT_NONE && actor.movement_type == MT_NONE)
			static_restore_slots.emplace(i, actor);
	}
}
extern "C" void xl_static_robot_restore_payloads(void)
{
	if (!xl_level_extensions_active()) return;
	for (const auto &[owner, initial] : static_restore_slots) {
		if (owner > Highest_object_index) continue;
		auto &actor = Objects[owner];
		if (actor.type != OBJ_ROBOT || actor.id != initial.id) continue;
		if (actor.movement_type == MT_NONE) actor.mtype = initial.mtype;
		if (actor.control_type == CT_NONE) actor.ctype = initial.ctype;
	}
}
