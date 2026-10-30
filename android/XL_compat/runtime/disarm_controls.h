#ifndef DXX_XL_DISARM_CONTROLS_H
#define DXX_XL_DISARM_CONTROLS_H
// Controlled engine events shared by native and Android acceptance owners
#include "robot_disarm.h"
#include "robot_disarm_state.h"
#include "preview.h"
#include "object_trigger.h"
#include "trigger_state.h"
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#include "inferno.h"
#include "game.h"
#include "gameseq.h"
#include "object.h"
#include "player.h"
#include "switch.h"
#include "wall.h"
#include "segment.h"
#include "ai.h"
#include "robot.h"
#include "weapon.h"
#include "rng_shared.h"
void ai_fire_laser_at_player(object *, vms_vector *, int, vms_vector *);
void collide_robot_and_player(object *, object *, vms_vector *);
void obj_delete_all_that_should_be_dead(void);
}
namespace xl_disarm_controls
{
inline void require(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
inline nlohmann::json snapshot()
{
	return xl_compat::disarm_snapshot();
}
inline void fixture()
{
	require(xl_level_extensions_active() && Num_triggers == 8 && xl_object_trigger_count() == 2 && Triggers[0].type == 34 && Triggers[2].time == 6000 && Triggers[3].type == 31 && Triggers[6].num_links == 2 && Triggers[6].side[1] == 42, "Disarm controls require the labeled derived fixture");
}
inline int fire(int index)
{
	xl_wall_trigger_reset();
	return check_trigger_sub(index, Player_num, 1);
}
inline void events()
{
	fixture();
	xl_wall_trigger_frame();
	xl_object_trigger_frame();
	xl_object_trigger_state autoplay{};
	require(xl_robot_disarmed(Triggers[4].seg[0]) && (Triggers[4].flags & TF_DISABLED) && xl_object_trigger_state_get(1, &autoplay) && autoplay.operated, "Wall or object disarm autoplay did not operate");
	require(snapshot().size() == 2, "Disarm autoplay failed to neutralize both distinct targets");
	xl_disarm_clear();
	xl_wall_trigger_frame();
	xl_object_trigger_frame();
	require(snapshot().empty(), "Disarm autoplay repeated after being consumed");
	xl_wall_trigger_reset();
	const auto &front = Walls[Num_walls - 2];
	check_trigger(&Segments[front.segnum], short(front.sidenum), short(Players[Player_num].objnum), 0);
	require(xl_robot_disarmed(Triggers[0].seg[0]), "Player wall contact did not disarm the target");
	require(fire(1) == 0 && xl_robot_disarmed(Triggers[1].seg[0]), "One-shot disarm did not affect target");
	xl_disarm_clear();
	require(fire(1) == 1 && snapshot().empty(), "One-shot disarm repeated");
	require(fire(3) == 0 && xl_robot_disarmed(Triggers[0].seg[0]), "Master disarm dispatch failed");
	xl_disarm_clear();
	require(fire(5) == 1 && fire(6) == 0 && snapshot().empty(), "Disabled, nonrobot or inert positive-side targets changed attack state");
	require(fire(7) == 0 && Triggers[7].type == 34 && snapshot().size() == 1 && fire(7) == 0 && snapshot().size() == 1, "Alternate or duplicate disarm targets rearmed a robot");
	xl_disarm_clear();
	auto bad = Triggers[0];
	bad.num_links = 3;
	bad.seg[1] = -1;
	bad.side[1] = -1;
	bad.seg[2] = 32767;
	bad.side[2] = -1;
	require(xl_disarm_action(&bad) && xl_robot_disarmed(bad.seg[0]) && snapshot().size() == 1, "Missing target slots prevented valid disarming or changed another object");
	const auto before = snapshot();
	bad.num_links = MAX_WALLS_PER_LINK + 1;
	require(!xl_disarm_action(&bad) && snapshot() == before, "Oversized disarm list partially applied");
	xl_disarm_clear();
	xl_object_trigger_state owner{};
	require(xl_object_trigger_state_get(0, &owner) && Objects[owner.owner].type == OBJ_POWERUP, "Disarm removal owner missing");
	Objects[owner.owner].flags |= OF_SHOULD_BE_DEAD;
	obj_delete_all_that_should_be_dead();
	require(Objects[owner.owner].type == OBJ_NONE && xl_robot_disarmed(Triggers[1].seg[0]), "Powerup removal did not dispatch object disarm");
}
inline void lifetimes()
{
	fixture();
	const auto &model = Objects[Triggers[0].seg[0]];
	const auto spawn = [&] { return obj_create(OBJ_ROBOT, model.id, ConsoleObject->segnum, &ConsoleObject->pos, &ConsoleObject->orient, model.size, CT_AI, MT_PHYSICS, RT_POLYOBJ); };
	const int first = spawn();
	require(first >= 0, "Cannot create lifetime control robot");
	const int signature = Objects[first].signature;
	trigger action{};
	action.type = 34;
	action.num_links = 1;
	action.seg[0] = short(first);
	action.side[0] = -1;
	require(xl_disarm_action(&action) && xl_robot_disarmed(first), "Created robot was not disarmed");
	obj_delete(first);
	require(!xl_robot_disarmed(first), "Deleted robot retained active disarming");
	const int second = spawn();
	require(second == first && Objects[second].signature != signature && !xl_robot_disarmed(second), "Reused object slot inherited prior lifetime disarming");
	const auto saved = snapshot();
	for (const auto &row : saved) require(row[0] != first, "Stale robot lifetime was serialized");
	require(xl_disarm_action(&action) && xl_robot_disarmed(second), "Reused live slot could not be disarmed by a new action");
	obj_delete(second);
}
inline void consumers(bool stock = false)
{
	if (!stock) fixture();
	int index = -1;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_ROBOT) {
			index = i;
			break;
		}
	require(index >= 0, "Attack controls need a robot");
	auto &robot = Objects[index];
	const auto original_robot = robot, original_ship = *ConsoleObject;
	const auto original_info = Robot_info[robot.id];
	const auto original_local = Ai_local_info[index];
	const auto original_player = Players[Player_num];
	const auto original_cheats = cheats;
	const auto old_frame = FrameTime;
	const auto neutral = snapshot();
	std::vector<int> signatures(object_capacity(), -1);
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type != OBJ_NONE) signatures[i] = Objects[i].signature;
	const auto restore = [&] {
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type != OBJ_NONE && Objects[i].signature != signatures[i]) obj_delete(i);
		const auto next = robot.next, prev = robot.prev;
		robot = original_robot;
		robot.next = next;
		robot.prev = prev;
		const auto ship_next = ConsoleObject->next, ship_prev = ConsoleObject->prev;
		*ConsoleObject = original_ship;
		ConsoleObject->next = ship_next;
		ConsoleObject->prev = ship_prev;
		Robot_info[robot.id] = original_info;
		Ai_local_info[index] = original_local;
		Players[Player_num] = original_player;
		cheats = original_cheats;
		FrameTime = old_frame;
		if (!stock) require(xl_compat::disarm_restore(neutral, true), "Cannot restore neutral control state");
	};
	try {
		auto &info = Robot_info[robot.id];
		auto &local = Ai_local_info[index];
		xl_disarm_clear();
		info.boss_flag = info.companion = info.thief = info.kamikaze = 0;
		info.attack_type = 0;
		info.weapon_type = 0;
		info.weapon_type2 = -1;
		robot.control_type = CT_AI;
		robot.ctype.ai_info.SUB_FLAGS = 0;
		robot.ctype.ai_info.dying_start_time = 0;
		Players[Player_num].flags = 0;
		Players[Player_num].energy = 100 * F1_0;
		Players[Player_num].shields = 100 * F1_0;
		cheats.robotfiringsuspended = 0;
		FrameTime = F1_0 / 60;
		const auto weapons = [] { int n=0; for(int i=0;i<=Highest_object_index;++i) n+=Objects[i].type==OBJ_WEAPON; return n; };
		const int armed = weapons();
		ai_fire_laser_at_player(&robot, &robot.pos, 1, &ConsoleObject->pos);
		require(weapons() > armed, "Armed ranged-fire control did not create a real weapon");
		int launched = -1;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_WEAPON && Objects[i].signature != signatures[i]) launched = i;
		require(launched >= 0, "Missing previously launched weapon");
		const int launched_signature = Objects[launched].signature;
		trigger action{};
		action.type = 34;
		action.num_links = 1;
		action.seg[0] = short(index);
		action.side[0] = -1;
		require(bool(xl_disarm_action(&action)) == !stock && bool(xl_robot_disarmed(index)) == !stock, "Disarm activation used the wrong runtime mode");
		const auto before_rng = d_rand_get_call_count();
		const auto before_local = local;
		const int before_weapons = weapons();
		ai_fire_laser_at_player(&robot, &robot.pos, 1, &ConsoleObject->pos);
		require(stock ? weapons() > before_weapons : weapons() == before_weapons && d_rand_get_call_count() == before_rng && !std::memcmp(&local, &before_local, sizeof(local)), "Disarming changed stock firing or failed to suppress new XL shots without side effects");
		require(Objects[launched].type == OBJ_WEAPON && Objects[launched].signature == launched_signature, "Disarming removed an existing projectile");
		const auto velocity = robot.mtype.phys_info.velocity;
		require(velocity.x == original_robot.mtype.phys_info.velocity.x && velocity.y == original_robot.mtype.phys_info.velocity.y && velocity.z == original_robot.mtype.phys_info.velocity.z, "Disarming altered robot movement");
		info.attack_type = 1;
		info.energy_drain = 3;
		robot.pos = ConsoleObject->pos;
		local.next_fire = 0;
		const auto energy = Players[Player_num].energy;
		do_ai_robot_hit_attack(&robot, ConsoleObject, &robot.pos);
		require(stock ? Players[Player_num].energy == energy - 3 * F1_0 : Players[Player_num].energy == energy && local.next_fire == 0, "Disarmed melee drained energy or suppressed Redux melee");
		xl_disarm_clear();
		local.next_fire = 0;
		const auto armed_energy = Players[Player_num].energy;
		do_ai_robot_hit_attack(&robot, ConsoleObject, &robot.pos);
		require(Players[Player_num].energy == armed_energy - 3 * F1_0 && local.next_fire > 0, "Armed melee control failed to drain energy and schedule its next attack");
		info.attack_type = 0;
		info.thief = 1;
		robot.ctype.ai_info.behavior = AIB_SNIPE;
		local.mode = AIM_THIEF_WAIT;
		local.player_awareness_type = PA_PLAYER_COLLISION;
		local.next_action_time = 0;
		vms_vector direction = robot.orient.fvec;
		if (!stock) require(xl_disarm_action(&action), "Cannot disarm thief control");
		do_thief_frame(&robot, F1_0, 1, &direction);
		require(stock ? local.mode == AIM_THIEF_ATTACK : local.mode == AIM_THIEF_WAIT && local.player_awareness_type == PA_PLAYER_COLLISION && local.next_action_time == 0, "Disarming failed to suppress thief decisions or contaminated Redux");
		xl_disarm_clear();
		local.mode = AIM_THIEF_WAIT;
		local.player_awareness_type = PA_PLAYER_COLLISION;
		local.next_action_time = 0;
		do_thief_frame(&robot, F1_0, 1, &direction);
		require(local.mode == AIM_THIEF_ATTACK && !local.player_awareness_type && local.next_action_time > 0, "Armed thief decision control did not enter attack mode");
		if (!stock) require(xl_disarm_action(&action), "Cannot disarm active thief");
		// XL does not suppress collision-time theft from a thief already attacking
		Players[Player_num].primary_weapon_flags = Players[Player_num].secondary_weapon_flags = 0;
		Players[Player_num].laser_level = 0;
		for (int i = 0; i < MAX_PRIMARY_WEAPONS; ++i) Players[Player_num].primary_ammo[i] = 0;
		for (int i = 0; i < MAX_SECONDARY_WEAPONS; ++i) Players[Player_num].secondary_ammo[i] = 0;
		collide_robot_and_player(&robot, ConsoleObject, &robot.pos);
		if (local.mode != AIM_THIEF_RETREAT) throw std::runtime_error("Thief contact control did not retreat: mode=" + std::to_string(local.mode) + " behavior=" + std::to_string(robot.ctype.ai_info.behavior) + " flags=" + std::to_string(robot.flags) + " robot=" + std::to_string(index) + " stock=" + std::to_string(stock));
		restore();
	} catch (...) {
		restore();
		throw;
	}
}
inline xl_trigger_state prepare()
{
	fixture();
	xl_disarm_clear();
	require(fire(0) == 0 && fire(2) == 1, "Cannot earn disarmed state and pending delayed action");
	GameTime64 += F1_0;
	xl_trigger_state state{};
	require(xl_wall_trigger_state(2, &state) && state.operated && state.delay_ms == 6000 && state.elapsed == F1_0 && xl_robot_disarmed(Triggers[0].seg[0]) && !xl_robot_disarmed(Triggers[2].seg[0]), "Disarm continuation lacks earned state and exact pending phase");
	return state;
}
inline void continuation(const xl_trigger_state &saved)
{
	xl_trigger_state state{};
	require(xl_wall_trigger_state(2, &state) && state.operated && state.elapsed == saved.elapsed && state.delay_ms == saved.delay_ms, "Disarm restore lost delayed action timing");
	GameTime64 += 6 * F1_0 - state.elapsed - 1;
	xl_wall_trigger_frame();
	require(!xl_robot_disarmed(Triggers[2].seg[0]), "Delayed disarm operated early");
	++GameTime64;
	xl_wall_trigger_frame();
	require(xl_robot_disarmed(Triggers[2].seg[0]), "Delayed disarm did not operate at saved deadline");
}
} // namespace xl_disarm_controls
#endif
