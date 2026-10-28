#include "projectile_probe.h"
#include "projectile.h"
#include "preview.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#include "android_log.h"
#include "debug_log_categories.h"
#include "inferno.h"
#include "object.h"
#include "game.h"
#include "player.h"
#include "laser.h"
#include "weapon.h"
#include "robot.h"
#include "ai.h"
#include "segment.h"
#include "gameseg.h"
void collide_robot_and_weapon(object *, object *, vms_vector *);
}

namespace
{
struct Victim {
	int owner, signature;
	fix shields;
};
std::vector<Victim> victims;
std::vector<xl_projectile_state> earned;
std::string error;
int first = -1, second = -1;
void require(bool valid, const char *message)
{
	if (!valid) throw std::runtime_error(message);
}
int shot(int room, vms_vector position)
{
	const int owner = Laser_create_new(&ConsoleObject->orient.fvec, &position, room, ConsoleObject - Objects, FUSION_ID, 0);
	require(owner >= 0, "Cannot create fusion through production firing");
	auto &weapon = Objects[owner];
	require(weapon.mtype.phys_info.flags & PF_PERSISTENT, "Fusion shot is not persistent");
	weapon.ctype.laser_info.multiplier = F1_0;
	weapon.shields = i2f(40);
	// Keep ordinary physics serialization; the automation pauses between contacts
	weapon.lifeleft = IMMORTAL_TIME;
	return owner;
}
int robot(int room, const vms_vector &position)
{
	const int owner = obj_create(OBJ_ROBOT, 0, room, &position, &vmd_identity_matrix, F1_0, CT_AI, MT_PHYSICS, RT_POLYOBJ);
	require(owner >= 0, "Cannot create controlled robot target");
	auto &target = Objects[owner];
	target.rtype.pobj_info.model_num = Robot_info[0].model_num;
	target.mtype.phys_info.mass = Robot_info[0].mass;
	target.mtype.phys_info.drag = Robot_info[0].drag;
	target.shields = i2f(100);
	init_ai_object(owner, AIB_NORMAL, -1);
	return owner;
}

void dispatch(int owner, int target)
{
	// Position the projectile at the target for this controlled contact, then
	// return it to its clear flight room before the next automation frame
	auto &weapon = Objects[owner];
	const int segment = weapon.segnum;
	const auto position = weapon.pos, previous = weapon.last_pos;
	obj_relink(owner, Objects[target].segnum);
	weapon.pos = weapon.last_pos = Objects[target].pos;
	collide_robot_and_weapon(&Objects[target], &weapon, &Objects[target].pos);
	obj_relink(owner, segment);
	weapon.pos = position;
	weapon.last_pos = previous;
	Objects[target].mtype.phys_info.velocity = {};
}
void contact(int owner, int target)
{
	Objects[owner].shields = i2f(40);
	dispatch(owner, target);
}
std::vector<xl_projectile_state> capture()
{
	std::vector<xl_projectile_state> result;
	for (int owner : { first, second }) {
		xl_projectile_state state{};
		const auto &object = Objects[owner];
		const int found = xl_projectile_state_get(owner, &state);
		debug_log_force(DLOG_GAME, "Projectile probe owner=%d found=%d type=%d control=%d movement=%d signature=%d flags=%d physics=%d velocity=%d,%d,%d life=%d hits=%d", owner, found, object.type, object.control_type, object.movement_type, object.signature, object.flags, object.mtype.phys_info.flags, object.mtype.phys_info.velocity.x, object.mtype.phys_info.velocity.y, object.mtype.phys_info.velocity.z, object.lifeleft, state.count);
		require(found, "Missing earned projectile owner");
		result.push_back(state);
	}
	return result;
}
void verify()
{
	require(xl_level_extensions_active() && earned.size() == 2 && victims.size() == 22, "Projectile probe has no XL baseline");
	const auto current = capture();
	for (std::size_t i = 0; i < earned.size(); ++i)
		require(current[i].owner == earned[i].owner && current[i].signature == earned[i].signature && current[i].count == earned[i].count &&
		            std::equal(current[i].victims, current[i].victims + current[i].count, earned[i].victims),
		        "Earned projectile history changed");
	for (const auto &victim : victims)
		require(victim.owner <= Highest_object_index && Objects[victim.owner].type == OBJ_ROBOT && Objects[victim.owner].signature == victim.signature && Objects[victim.owner].shields == victim.shields, "Earned projectile target identity or damage changed");
	const auto shield = Objects[first].shields;
	const int count = num_objects;
	const auto &target = victims.back();
	dispatch(first, target.owner);
	require(Objects[target.owner].shields == target.shields && Objects[first].shields == shield && num_objects == count, "Restored repeat contact changed damage or effects");
}
} // namespace

const char *xl_projectile_probe(const char *operation)
{
	try {
		require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "Projectile probe requires running singleplayer");
		if (!std::strcmp(operation, "prepare")) {
			require(xl_level_extensions_active(), "Projectile preparation requires an XL level");
			Game_suspended |= SUSP_ROBOTS;
			int room = -1;
			for (int i = 0; i < Num_segments; ++i)
				if (Segments[i].objects < 0 && i != ConsoleObject->segnum) {
					room = i;
					break;
				}
			require(room >= 0, "Cannot find empty room for projectile contacts");
			vms_vector center;
			compute_segment_center(&center, &Segments[room]);
			first = shot(room, center);
			second = shot(room, center);
			victims.clear();
			earned.clear();
			for (int i = 0; i < 22; ++i) {
				const int target = robot(room, center);
				contact(first, target);
				require(Objects[target].shields == i2f(60) && Objects[first].shields == i2f(25), "Distinct XL contact damage or attrition differs");
				victims.push_back({ target, Objects[target].signature, Objects[target].shields });
			}
			xl_projectile_state full{};
			require(xl_projectile_state_get(first, &full) && full.count == 20, "XL history did not reach its boundary");
			for (int i = 0; i < full.count; ++i) require(full.victims[i] == victims[i + 2].owner, "XL history did not evict oldest victims");
			contact(first, victims.front().owner);
			contact(second, victims.back().owner);
			victims.front().shields = victims.back().shields = i2f(20);
			int flight_room = -1, clearance = 0;
			char source_file[] = __FILE__;
			vms_vector flight_center{};
			for (int i = 0; i < Num_segments; ++i) {
				if (Segments[i].objects >= 0) continue;
				vms_vector candidate;
				compute_segment_center(&candidate, &Segments[i]);
				for (int radius = clearance + 1; radius <= 100; ++radius) {
					if (get_seg_masks(&candidate, i, i2f(radius), source_file, __LINE__).facemask) break;
					flight_room = i;
					flight_center = candidate;
					clearance = radius;
				}
			}
			require(flight_room >= 0 && clearance >= 10, "No clear room for saved in-flight projectiles");
			for (int owner : { first, second }) {
				obj_relink(owner, flight_room);
				Objects[owner].pos = flight_center;
				Objects[owner].pos.x += owner == first ? -i2f(3) : i2f(3);
				Objects[owner].last_pos = Objects[owner].pos;
				Objects[owner].mtype.phys_info.velocity = { 0, 0, Weapon_info[FUSION_ID].speed[Difficulty_level] };
			}
			debug_log_force(DLOG_GAME, "Projectile probe flight room=%d clearance=%d", flight_room, clearance);
			earned = capture();
			verify();
		} else if (!std::strcmp(operation, "verify")) {
			Game_suspended |= SUSP_ROBOTS;
			verify();
		} else if (!std::strcmp(operation, "disturb")) {
			verify();
			contact(first, victims[1].owner);
			require(Objects[victims[1].owner].shields == i2f(20), "Deliberate post-capture hit did not change earned damage");
			xl_projectile_state state{};
			require(xl_projectile_state_get(first, &state) && state.victims[state.count - 1] == victims[1].owner, "Deliberate post-capture hit did not change history");
		} else if (!std::strcmp(operation, "reuse")) {
			verify();
			const int signature = Objects[second].signature;
			const auto position = Objects[second].pos;
			const int room = Objects[second].segnum;
			obj_delete(second);
			const int replacement = shot(room, position);
			require(replacement == second && Objects[replacement].signature != signature && !xl_projectile_has_hit(replacement, victims[1].owner), "Reused projectile inherited previous owner state");
			contact(replacement, victims[1].owner);
			require(Objects[victims[1].owner].shields == i2f(20), "Reused projectile failed to damage prior victim");
		} else if (!std::strcmp(operation, "stock")) {
			require(!xl_level_extensions_active(), "Stock projectile control requires Redux");
			const int owner = shot(ConsoleObject->segnum, ConsoleObject->pos);
			const int target = robot(ConsoleObject->segnum, ConsoleObject->pos);
			contact(owner, target);
			require(Objects[target].shields == i2f(60) && Objects[owner].shields == i2f(40) && Objects[owner].ctype.laser_info.hitobj_list[target] && !xl_projectile_has_hit(owner, target), "XL projectile path leaked into Redux");
			contact(owner, target);
			require(Objects[target].shields == i2f(60), "Redux projectile repeat-hit suppression changed");
			obj_delete(target);
			obj_delete(owner);
		} else require(false, "Unknown projectile probe operation");
		return nullptr;
	} catch (const std::exception &failure) {
		error = failure.what();
		return error.c_str();
	}
}
