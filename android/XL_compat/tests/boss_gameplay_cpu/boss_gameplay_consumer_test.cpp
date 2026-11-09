// Asset-free consumer check. All boss, trigger and generator decisions are linked
// from production translation units; only external engine effects are recorded.
#include "object_trigger.h"
#include "boss_spawn.h"
#include "object_birth.h"
#include "robot_generator.h"
#include "inventory.h"
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "robot.h"
#include "polyobj.h"
#include "player.h"
#include "game.h"
#include "gameseq.h"
#include "gameseg.h"
#include "segment.h"
#include "fuelcen.h"
#include "switch.h"
#include "boss_health_shared.h"
#include "collide.h"
#include "fireball.h"
#include "digi.h"
#include "ai.h"
#include "morph.h"
#include "vclip.h"
#include "mission.h"
#include "wall.h"
#include "newdemo.h"
#include "input_demo_hooks.h"
#include "input_demo_rng_trace.h"
}

namespace
{
constexpr int owner_a = 3, owner_b = 5, player_slot = 1;
constexpr int boss_type = 12, child_type = 7;
constexpr fix strength = 400 * F1_0;
int births, ai_calls, morph_calls, effect_calls, sound_calls, death_calls;
int requested_type, requested_segment;
bool allocation_failure, extensions = true;
Mission fixture_mission{};

void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}

xl_compat::Inventory authored_level(bool permanent)
{
	xl_compat::Inventory level;
	level.segments.resize(2);
	level.objects.resize(6);
	for (int owner : { owner_a, owner_b }) {
		xl_compat::ObjectTrigger record;
		record.object = owner;
		record.action.type = 26;
		record.action.flags = permanent ? 8 : 0;
		record.action.value = 1;
		record.action.time = -1;
		record.action.targets.push_back({ 1, 0 });
		level.object_trigger_records.push_back(record);
	}
	return level;
}

fix interval()
{
	return i2f(4) - Difficulty_level * i2f(2) / 3;
}

fix part(fix whole, int numerator, int denominator)
{
	return fix(std::int64_t(whole) * numerator / denominator);
}

void reset(bool permanent = true)
{
	xl_object_trigger_clear();
	xl_object_birth_clear();
	xl_boss_spawn_clear();
	xl_robot_generator_clear();
	births = ai_calls = morph_calls = effect_calls = sound_calls = death_calls = 0;
	allocation_failure = false;
	cheats = {};
	extensions = true;
	GameTime64 = 0;
	FrameTime = F1_0 / 60;
	Game_mode = Game_suspended = 0;
	Player_num = 0;
	fixture_mission = {};
	fixture_mission.descent_version = 2;
	Current_mission = &fixture_mission;
	Current_level_num = 2;
	Highest_object_index = owner_b;
	Highest_segment_index = 1;
	Num_segments = 2;
	Num_robot_centers = 1;
	N_robot_types = 13;
	N_polygon_models = 2;
	for (int i = 0; i <= Highest_object_index; ++i) Objects[i] = {};
	for (int i = 0; i < 2; ++i) {
		Segments[i] = {};
		Segments[i].objects = -1;
		for (int side = 0; side < 6; ++side) {
			Segments[i].children[side] = -1;
			Segments[i].sides[side].wall_num = -1;
		}
	}
	Players[0] = {};
	Players[0].objnum = player_slot;
	Objects[player_slot].type = OBJ_PLAYER;
	Objects[player_slot].segnum = 0;
	Objects[player_slot].signature = 101;
	Robot_info[boss_type] = {};
	Robot_info[boss_type].model_num = 0;
	Robot_info[boss_type].boss_flag = 21;
	Robot_info[boss_type].strength = strength;
	Robot_info[child_type] = {};
	Robot_info[child_type].model_num = 1;
	Robot_info[child_type].strength = 10 * F1_0;
	Robot_info[child_type].mass = 3 * F1_0;
	Robot_info[child_type].drag = F1_0 / 4;
	Polygon_models[0].rad = 4 * F1_0;
	Polygon_models[1].rad = F1_0;
	RobotCenters[0] = {};
	RobotCenters[0].robot_flags[0] = 1 << child_type;
	Segment2s[1].matcen_num = 0;
	for (int owner : { owner_a, owner_b }) {
		Objects[owner].type = OBJ_ROBOT;
		Objects[owner].id = boss_type;
		Objects[owner].signature = 200 + owner;
		Objects[owner].segnum = 0;
		Objects[owner].shields = boss_health_maximum_for_difficulty(strength, Difficulty_level);
	}
	xl_robot_generator_load();
	xl_boss_spawn_load();
	xl_object_trigger_load(authored_level(permanent));
	require(xl_boss_spawn_count() == 2, "boss load");
	require(xl_object_trigger_count() == 2, "trigger load");
}

xl_object_trigger_state trigger_state(int owner)
{
	for (int i = 0; i < xl_object_trigger_count(); ++i) {
		xl_object_trigger_state state{};
		require(xl_object_trigger_state_get(i, &state), "trigger state get");
		if (state.owner == owner) return state;
	}
	throw std::runtime_error("missing trigger owner");
}

xl_boss_spawn_state boss_state(int owner)
{
	for (int i = 0; i < xl_boss_spawn_count(); ++i) {
		xl_boss_spawn_state state{};
		require(xl_boss_spawn_state_get(i, &state), "boss state get");
		if (state.owner == owner) return state;
	}
	throw std::runtime_error("missing boss owner");
}

void hit(int owner, fix remaining, int killer = player_slot)
{
	const fix old = Objects[owner].shields;
	require(remaining < old && remaining >= 0, "positive surviving hit fixture");
	require(apply_damage_to_robot(&Objects[owner], old - remaining, killer) == 0, "surviving result");
	require(Objects[owner].shields == remaining, "shield deduction");
}

void threshold_and_attribution()
{
	for (int difficulty = 0; difficulty < NDL; ++difficulty) {
		Difficulty_level = difficulty;
		reset();
		const fix maximum = boss_health_maximum_for_difficulty(strength, difficulty);
		GameTime64 = F1_0;
		const fix expected = difficulty == 0 ? (strength / (NDL + 3) * 4) / 2
		                                     : strength / (NDL + 3) * (difficulty + 4);
		require(maximum == expected, "difficulty maximum");
		hit(owner_a, part(maximum, 91, 100));
		require(!trigger_state(owner_a).operated && births == 0, "91 percent does not operate");
		hit(owner_a, part(maximum, 905, 1000));
		require(trigger_state(owner_a).operated && births == 0, "90.5 percent operates in cooldown");
		reset();
		GameTime64 = F1_0;
		hit(owner_a, part(maximum, 905, 1000), -1);
		require(!trigger_state(owner_a).operated, "unattributed damage");
		reset();
		GameTime64 = F1_0;
		require(!apply_damage_to_robot(&Objects[owner_a], F1_0, owner_b) && Objects[owner_a].shields == maximum &&
		            !trigger_state(owner_a).operated,
		        "hostile robot damage rejected");
		Objects[2].type = OBJ_CNTRLCEN;
		hit(owner_a, part(maximum, 905, 1000), 2);
		require(!trigger_state(owner_a).operated, "nonrobot nonplayer damage has no player event");
		// Fixed-point neighbors straddle the reference's float-to-int cast.
		reset();
		GameTime64 = F1_0;
		const fix lower = part(maximum, 91, 100) - 2;
		hit(owner_a, lower);
		require(trigger_state(owner_a).operated, "below 91 percent cast boundary");
		reset();
		GameTime64 = F1_0;
		hit(owner_a, part(maximum, 91, 100) + 1);
		require(!trigger_state(owner_a).operated, "above 91 percent cast boundary");
	}
}

void restore_retry_and_owner_isolation()
{
	Difficulty_level = 2;
	reset();
	const fix maximum = Objects[owner_a].shields;
	GameTime64 = interval() - 1;
	hit(owner_a, part(maximum, 905, 1000));
	require(trigger_state(owner_a).operated && births == 0, "operation is not birth");
	require(boss_state(owner_a).elapsed == interval() - 1, "blocked timer unchanged");
	require(!trigger_state(owner_b).operated, "other trigger independent");
	std::vector<xl_object_trigger_state> triggers(xl_object_trigger_count());
	std::vector<xl_boss_spawn_state> bosses(xl_boss_spawn_count());
	for (int i = 0; i < int(triggers.size()); ++i) require(xl_object_trigger_state_get(i, &triggers[i]), "snapshot trigger");
	for (int i = 0; i < int(bosses.size()); ++i) require(xl_boss_spawn_state_get(i, &bosses[i]), "snapshot boss");
	const fix shields = Objects[owner_a].shields;
	GameTime64 = 20 * F1_0;
	xl_object_trigger_load(authored_level(true));
	xl_boss_spawn_load();
	require(xl_object_trigger_restore(triggers.data(), int(triggers.size()), 0), "validate trigger");
	require(xl_boss_spawn_restore(bosses.data(), int(bosses.size()), 0), "validate boss");
	auto bad = bosses;
	bad[0].elapsed = -1;
	require(!xl_boss_spawn_restore(bad.data(), int(bad.size()), 1), "reject negative elapsed");
	bad = bosses;
	bad[0].signature++;
	require(!xl_boss_spawn_restore(bad.data(), int(bad.size()), 1), "reject signature");
	bad = bosses;
	bad[0].owner = owner_b;
	require(!xl_boss_spawn_restore(bad.data(), int(bad.size()), 1), "reject wrong owner");
	bad = bosses;
	bad[0].id = child_type;
	require(!xl_boss_spawn_restore(bad.data(), int(bad.size()), 1), "reject wrong id");
	bad = bosses;
	bad[0].interval++;
	require(!xl_boss_spawn_restore(bad.data(), int(bad.size()), 1), "reject wrong interval");
	auto bad_triggers = triggers;
	bad_triggers[0].signature++;
	require(!xl_object_trigger_restore(bad_triggers.data(), int(bad_triggers.size()), 1), "reject wrong trigger signature");
	bad_triggers = triggers;
	bad_triggers[0].owner = owner_b;
	require(!xl_object_trigger_restore(bad_triggers.data(), int(bad_triggers.size()), 1), "reject wrong trigger owner");
	bad_triggers = triggers;
	bad_triggers[0].value++;
	require(!xl_object_trigger_restore(bad_triggers.data(), int(bad_triggers.size()), 1), "reject wrong trigger value");
	bad_triggers = triggers;
	bad_triggers[0].flags ^= TF_ONE_SHOT;
	require(!xl_object_trigger_restore(bad_triggers.data(), int(bad_triggers.size()), 1), "reject wrong trigger flags");
	require(boss_state(owner_a).elapsed == GameTime64, "invalid boss restore is atomic");
	require(!trigger_state(owner_a).operated, "invalid trigger restore is atomic");
	require(xl_object_trigger_restore(triggers.data(), int(triggers.size()), 1), "apply trigger");
	require(xl_boss_spawn_restore(bosses.data(), int(bosses.size()), 1), "apply boss");
	for (int i = 0; i < int(triggers.size()); ++i) {
		xl_object_trigger_state restored{};
		require(xl_object_trigger_state_get(i, &restored), "restored trigger get");
		require(restored.owner == triggers[i].owner && restored.signature == triggers[i].signature &&
		            restored.type == triggers[i].type && restored.flags == triggers[i].flags &&
		            restored.value == triggers[i].value && restored.operated == triggers[i].operated &&
		            restored.destination == triggers[i].destination,
		        "complete trigger state restored");
	}
	require(boss_state(owner_a).elapsed == interval() - 1, "elapsed rebased");
	require(boss_state(owner_b).elapsed == interval() - 1, "other timer rebased");
	require(Objects[owner_a].shields == shields, "actor state retained");
	GameTime64++;
	require(births == 0, "clock alone cannot spawn");
	hit(owner_a, shields - 1);
	require(births == 1 && requested_type == child_type && requested_segment == 0, "restored hit creates selected child at boss");
	require(boss_state(owner_a).elapsed == 0, "creator timer reset");
	require(boss_state(owner_b).elapsed == interval(), "other timer unchanged");
	require(ai_calls == 1 && morph_calls == 1 && effect_calls == 1 && sound_calls == 1, "birth endpoints");
	const object &child = Objects[Highest_object_index];
	require(child.matcen_creator == BOSS_GATE_MATCEN_NUM && child.shields == Robot_info[child_type].strength,
	        "creator and strength initialized");
	require(child.mtype.phys_info.mass == Robot_info[child_type].mass &&
	            child.mtype.phys_info.drag == Robot_info[child_type].drag,
	        "physics initialized");
	require(child.lifeleft > 0 && child.segnum == 0, "lifetime and boss segment");
	require(Players[0].num_robots_level == 1 && Players[0].num_robots_total == 1, "player totals");
	++GameTime64;
	hit(owner_b, part(Objects[owner_b].shields, 905, 1000));
	require(births == 2 && sound_calls == 2 && boss_state(owner_a).elapsed == 1 && boss_state(owner_b).elapsed == 0,
	        "second owner has an independent gate timer");
}

void rejection_controls()
{
	Difficulty_level = 2;
	reset();
	const fix maximum = Objects[owner_a].shields;
	Objects[owner_a].flags |= OF_EXPLODING;
	require(apply_damage_to_robot(&Objects[owner_a], 1, player_slot) == 0, "exploding result");
	require(Objects[owner_a].shields == maximum, "exploding damage rejected");
	Objects[owner_a].flags &= ~OF_EXPLODING;
	Objects[owner_a].shields = -1;
	require(apply_damage_to_robot(&Objects[owner_a], 1, player_slot) == 0, "dead result");
	require(!trigger_state(owner_a).operated, "dead actor does not fire");
	reset();
	GameTime64 = F1_0;
	require(apply_damage_to_robot(&Objects[owner_a], maximum + 1, player_slot) == 1, "lethal result");
	require(death_calls == 1 && !trigger_state(owner_a).operated, "death endpoint, not damage trigger");
	reset();
	GameTime64 = interval();
	allocation_failure = true;
	hit(owner_a, part(maximum, 905, 1000));
	require(births == 1 && ai_calls == 0 && morph_calls == 0, "allocator rejection has no birth effects");
	require(boss_state(owner_a).elapsed == interval() * 3 / 4, "allocation backoff");
	const int cap = 2 * Difficulty_level + 6;
	reset();
	for (int i = owner_b + 1; i <= owner_b + cap; ++i) {
		Objects[i] = {};
		Objects[i].type = OBJ_ROBOT;
		Objects[i].matcen_creator = BOSS_GATE_MATCEN_NUM;
	}
	Highest_object_index = owner_b + cap;
	GameTime64 = interval();
	hit(owner_a, part(maximum, 905, 1000));
	require(births == 1 && ai_calls == 1, "population equality permits birth");
	reset();
	for (int i = owner_b + 1; i <= owner_b + cap; ++i) {
		Objects[i] = {};
		Objects[i].type = OBJ_ROBOT;
		Objects[i].matcen_creator = BOSS_GATE_MATCEN_NUM;
	}
	Highest_object_index = owner_b + cap;
	Objects[owner_b + cap + 1] = {};
	Objects[owner_b + cap + 1].type = OBJ_ROBOT;
	Objects[owner_b + cap + 1].matcen_creator = BOSS_GATE_MATCEN_NUM;
	++Highest_object_index;
	GameTime64 = interval();
	hit(owner_a, part(maximum, 905, 1000));
	require(births == 0 && ai_calls == 0, "population excess rejects birth");
	require(boss_state(owner_a).elapsed == interval() * 3 / 4, "population backoff");
}

void disabled_and_one_shot_ordering()
{
	Difficulty_level = 2;
	reset(false);
	auto state = trigger_state(owner_a);
	state.flags |= TF_DISABLED;
	require(xl_object_trigger_restore(&state, 1, 0) == 0, "single-state restore rejects wrong count");
	std::vector<xl_object_trigger_state> states(xl_object_trigger_count());
	for (int i = 0; i < int(states.size()); ++i) require(xl_object_trigger_state_get(i, &states[i]), "disabled setup snapshot");
	states[0].flags |= TF_DISABLED;
	require(xl_object_trigger_restore(states.data(), int(states.size()), 1), "disabled setup");
	GameTime64 = interval();
	hit(owner_a, part(Objects[owner_a].shields, 905, 1000));
	require(!trigger_state(owner_a).operated && trigger_state(owner_a).value == 0 && births == 0,
	        "nonpermanent value consumed before disabled operation");
	reset(false);
	auto authored = authored_level(false);
	authored.object_trigger_records[0].action.flags |= TF_ONE_SHOT;
	xl_object_trigger_load(authored);
	GameTime64 = interval() - 1;
	hit(owner_a, part(Objects[owner_a].shields, 905, 1000));
	require(trigger_state(owner_a).operated && (trigger_state(owner_a).flags & TF_DISABLED) &&
	            trigger_state(owner_a).value == 0 && births == 0,
	        "one-shot operates before disabling");
}

void nonpermanent_and_classic()
{
	Difficulty_level = 2;
	reset(false);
	const fix maximum = Objects[owner_a].shields;
	GameTime64 = interval() - 1;
	hit(owner_a, part(maximum, 905, 1000));
	require(trigger_state(owner_a).value == 0 && births == 0, "nonpermanent consumed despite cooldown");
	GameTime64++;
	hit(owner_a, Objects[owner_a].shields - 1);
	require(births == 0, "consumed cannot retry");
	reset();
	const auto before_trigger = trigger_state(owner_a);
	extensions = false;
	GameTime64 = interval();
	const fix old = Objects[owner_a].shields;
	const fix remaining = part(old, 905, 1000);
	require(apply_damage_to_robot(&Objects[owner_a], old - remaining, player_slot) == 0, "classic surviving result");
	require(Objects[owner_a].shields == remaining && births == 0, "classic no XL birth");
	extensions = true;
	const auto after_trigger = trigger_state(owner_a);
	require(after_trigger.owner == before_trigger.owner && after_trigger.signature == before_trigger.signature &&
	            after_trigger.value == before_trigger.value && after_trigger.flags == before_trigger.flags &&
	            after_trigger.operated == before_trigger.operated,
	        "classic did not mutate XL trigger");
	require(boss_state(owner_a).elapsed == interval(), "classic did not mutate boss timer");
}

void robot_attacker_damage_gate()
{
	for (const bool boss : { false, true })
		for (const bool lethal : { false, true })
			for (const int profile : { 0, 1, 2, 3, 4, 5, 6, 7 }) {
				reset();
				GameTime64 = 20 * F1_0;
				const int victim = boss ? owner_a : 4;
				if (!boss) {
					Objects[victim].type = OBJ_ROBOT;
					Objects[victim].id = child_type;
					Objects[victim].shields = 100 * F1_0;
				}
				const auto prior_hit = boss_state(owner_a).hit;
				int killer = owner_b;
				if (profile == 1) {
					Objects[owner_b].id = 6;
					Robot_info[6] = {};
					Robot_info[6].companion = 1;
				}
				if (profile == 2) killer = player_slot;
				if (profile == 3) {
					killer = 2;
					Objects[killer].type = OBJ_CNTRLCEN;
				}
				if (profile == 4) killer = -1;
				if (profile == 5) cheats.robotskillrobots = 1;
				if (profile == 6) extensions = false;
				if (profile == 7) Game_mode = GM_MULTI;
				const fix before = Objects[victim].shields;
				const fix damage = lethal ? before + 1 : F1_0;
				const int result = apply_damage_to_robot(&Objects[victim], damage, killer);
				const bool rejected = profile == 0;
				require(Objects[victim].shields == before - (rejected ? 0 : damage),
				        "hostile robot attacker shield gate");
				require(result == (!rejected && lethal) && death_calls == (!rejected && lethal),
				        "hostile robot attacker death gate");
				if (rejected) {
					require(boss_state(owner_a).hit == prior_hit && !trigger_state(owner_a).operated &&
					            births == 0 && Players[0].num_kills_level == 0,
					        "hostile robot rejection precedes bookkeeping and events");
				}
			}
}

void definition_instance_and_zero_damage()
{
	reset();
	GameTime64 = F1_0;
	Objects[owner_a].shields = 0;
	require(apply_damage_to_robot(&Objects[owner_a], 1, player_slot) == 1 && death_calls == 1,
	        "positive definition with zero instance shields remains destructible");
	reset();
	GameTime64 = F1_0;
	Objects[owner_a].shields = part(Objects[owner_a].shields, 905, 1000);
	require(!apply_damage_to_robot(&Objects[owner_a], 0, player_slot) && trigger_state(owner_a).operated,
	        "accepted zero damage retains threshold event");
	reset();
	GameTime64 = F1_0;
	Objects[4].type = OBJ_ROBOT;
	Objects[4].id = child_type;
	Objects[4].ctype.ai_info.behavior = 135;
	Objects[4].shields = 100 * F1_0;
	require(!apply_damage_to_robot(&Objects[4], F1_0, player_slot) && Objects[4].shields == 99 * F1_0,
	        "mature positive-strength behavior 135 remains damageable");
}

void spawned_child_birth_and_registry()
{
	reset();
	GameTime64 = 20 * F1_0;
	hit(owner_a, part(Objects[owner_a].shields, 905, 1000));
	require(births == 1, "damage event spawned child");
	const int child = Highest_object_index;
	const auto born = GameTime64;
	const fix original = Objects[child].shields;
	require(xl_object_birth_time(child) == born && xl_object_birth_order(child) == 1,
	        "spawn endpoint records actual birth");
	for (const int age : { -1, 0, F1_0 - 1 }) {
		GameTime64 = born + age;
		require(!apply_damage_to_robot(&Objects[child], 1, player_slot) && Objects[child].shields == original,
		        "spawned child grace interval");
	}
	GameTime64 = born + F1_0;
	require(!apply_damage_to_robot(&Objects[child], 1, player_slot) && Objects[child].shields == original - 1,
	        "spawned child exact mature boundary");
	// Reusing a slot must not inherit the old actor's private birth
	++Objects[child].signature;
	require(xl_object_birth_time(child) == 0 && xl_object_birth_order(child) == 0, "reused signature drops birth");
	xl_object_birth_created(child);
	require(xl_object_birth_time(child) == GameTime64 && xl_object_birth_order(child) == 2, "new signature records new birth");
	require(!apply_damage_to_robot(&Objects[child], 1, player_slot) && Objects[child].shields == original - 1,
	        "reused newborn rejects damage");
	// Registry reset models omitted private state, not the full save codec
	xl_object_birth_clear();
	require(xl_object_birth_time(child) == 0 && xl_object_birth_order(child) == 0, "registry reset clears private state");
	require(!apply_damage_to_robot(&Objects[child], 1, player_slot) && Objects[child].shields == original - 2,
	        "restored mature epoch does not invent grace");
	GameTime64 = F1_0 - 1;
	require(!apply_damage_to_robot(&Objects[child], 1, player_slot) && Objects[child].shields == original - 2,
	        "restored initial epoch preserves grace");
	GameTime64 = F1_0;
	require(!apply_damage_to_robot(&Objects[child], 1, player_slot) && Objects[child].shields == original - 3,
	        "restored initial epoch exact boundary");
}

void common_damage_gates(bool age_cases = true, bool strength_cases = true)
{
	Difficulty_level = 2;
	if (age_cases) {
		for (const bool dynamic : { false, true }) {
			for (const int age : { -1, 0, F1_0 - 1, F1_0, F1_0 + 1 }) {
				reset();
				const int owner = dynamic ? 4 : owner_a;
				if (dynamic) {
					Objects[owner].type = OBJ_ROBOT;
					Objects[owner].id = child_type;
					Objects[owner].signature = 404;
					Objects[owner].shields = 100 * F1_0;
					GameTime64 = 10 * F1_0;
					xl_object_birth_created(owner);
				} else {
					GameTime64 = 0;
				}
				const fix old = Objects[owner].shields;
				GameTime64 += age;
				const auto hit_before = boss_state(owner_a).hit;
				require(!apply_damage_to_robot(&Objects[owner], F1_0, player_slot), "boundary result");
				const bool eligible = age >= F1_0;
				require(Objects[owner].shields == old - (eligible ? F1_0 : 0), "age gate shield boundary");
				require(boss_state(owner_a).hit == (dynamic || !eligible ? hit_before : 0), "age gate boss bookkeeping");
				require(death_calls == 0 && Players[0].num_kills_level == 0 && !trigger_state(owner_a).operated,
				        "age gate side effect");
			}
		}
	}
	if (strength_cases) {
		for (const int owner : { owner_a, 4 })
			for (const fix definition_strength : { -F1_0, 0 }) {
				reset();
				GameTime64 = 2 * F1_0;
				Objects[4].type = OBJ_ROBOT;
				Objects[4].id = child_type;
				Objects[4].shields = 100 * F1_0;
				Robot_info[Objects[owner].id].strength = definition_strength;
				const fix old = Objects[owner].shields;
				const auto hit_before = boss_state(owner_a).hit;
				require(!apply_damage_to_robot(&Objects[owner], F1_0, player_slot) && Objects[owner].shields == old &&
				            boss_state(owner_a).hit == hit_before && !trigger_state(owner_a).operated,
				        "definition strength gate");
			}
	}
	if (age_cases) {
		reset();
		GameTime64 = INT64_C(1) << 40;
		Objects[4].type = OBJ_ROBOT;
		Objects[4].id = child_type;
		Objects[4].signature = 405;
		Objects[4].shields = 100 * F1_0;
		xl_object_birth_created(4);
		GameTime64 += F1_0 - 1;
		require(!apply_damage_to_robot(&Objects[4], F1_0, player_slot) && Objects[4].shields == 100 * F1_0,
		        "large epoch newborn gate");
		++GameTime64;
		require(!apply_damage_to_robot(&Objects[4], F1_0, player_slot) && Objects[4].shields == 99 * F1_0,
		        "large epoch mature boundary");
	}
	reset();
	Robot_info[boss_type].strength = 0;
	extensions = false;
	const fix classic_shields = Objects[owner_a].shields;
	require(!apply_damage_to_robot(&Objects[owner_a], F1_0, player_slot) && Objects[owner_a].shields == classic_shields - F1_0,
	        "classic inactive newborn and strength control");
	reset();
	fixture_mission.descent_version = 1;
	Robot_info[boss_type].strength = 0;
	extensions = false;
	const fix d1_shields = Objects[owner_a].shields;
	require(!apply_damage_to_robot(&Objects[owner_a], F1_0, player_slot) && Objects[owner_a].shields == d1_shields - F1_0,
	        "D1-in-D2 inactive newborn and strength control");
}
} // namespace

int main(int argc, char **argv)
{
	try {
		if (argc == 2 && !std::strcmp(argv[1], "attacker")) {
			robot_attacker_damage_gate();
			return 0;
		}
		if (argc == 2 && (!std::strcmp(argv[1], "age") || !std::strcmp(argv[1], "strength"))) {
			common_damage_gates(!std::strcmp(argv[1], "age"), !std::strcmp(argv[1], "strength"));
			return 0;
		}
		threshold_and_attribution();
		restore_retry_and_owner_isolation();
		nonpermanent_and_classic();
		rejection_controls();
		disabled_and_one_shot_ordering();
		common_damage_gates();
		spawned_child_birth_and_registry();
		definition_instance_and_zero_damage();
		robot_attacker_damage_gate();
		std::cout << "boss gameplay consumer: pass\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "boss gameplay consumer: " << error.what() << '\n';
		return 1;
	}
}

// Bounded engine endpoints. No gameplay predicate or state transition from the
// production damage/trigger/generator/boss units is implemented here.
extern "C" {
object Objects[MAX_OBJECTS];
int Highest_object_index;
segment Segments[MAX_SEGMENTS];
int Num_segments, Highest_segment_index;
player Players[MAX_PLAYERS + 4];
int Player_num, Current_level_num, Difficulty_level, Game_mode, Game_suspended;
fix FrameTime;
fix64 GameTime64;
polymodel Polygon_models[MAX_POLYGON_MODELS];
int N_polygon_models;
matcen_info RobotCenters[MAX_ROBOT_CENTERS];
int Num_robot_centers;
vclip Vclip[VCLIP_MAXNUM];
vms_vector Vertices[MAX_VERTICES];
wall Walls[MAX_WALLS];
int Num_walls;
trigger Triggers[MAX_TRIGGERS];
int Num_triggers;
Mission *Current_mission;
int Player_is_dead;
fix64 Boss_hit_time;
game_cheats cheats;

int xl_level_extensions_active(void)
{
	return extensions ? 1 : 0;
}
int xl_level_segment_group(int segment)
{
	return segment >= 0 && segment < Num_segments ? 1 : -1;
}
int boss_fits_in_seg(object *, int segment)
{
	return segment >= 0 && segment < Num_segments;
}
void obj_relink(int index, int segment)
{
	Objects[index].segnum = segment;
}
void compute_segment_center(vms_vector *result, segment *where)
{
	*result = { fix((where - Segments) * 100 * F1_0), 0, 0 };
}
int obj_create(enum object_type_t type, ubyte id, int segment, const vms_vector *position,
               const vms_matrix *, fix size, ubyte control, ubyte movement, ubyte render)
{
	++births;
	requested_type = id;
	requested_segment = segment;
	if (allocation_failure) return -1;
	const int index = ++Highest_object_index;
	require(index < MAX_OBJECTS, "fixture slot capacity");
	Objects[index] = {};
	Objects[index].type = type;
	Objects[index].id = id;
	Objects[index].segnum = segment;
	Objects[index].pos = *position;
	Objects[index].size = size;
	Objects[index].control_type = control;
	Objects[index].movement_type = movement;
	Objects[index].render_type = render;
	Objects[index].signature = 300 + index;
	xl_object_birth_created(index);
	return index;
}
int object_capacity(void)
{
	return MAX_OBJECTS;
}
void xl_camera_hit_state_clear(void) {}
void xl_camera_hit_state_created(int) {}
void init_ai_object(int, int, int)
{
	++ai_calls;
}
void morph_start(object *)
{
	++morph_calls;
}
object *object_create_explosion(short, vms_vector *, fix, int)
{
	++effect_calls;
	return nullptr;
}
int digi_link_sound_to_pos(int sound, short segment, short side, vms_vector *, int forever, fix volume)
{
	require(sound == Vclip[VCLIP_MORPHING_ROBOT].sound_num && segment == 0 && side == 0 &&
	            !forever && volume == F1_0,
	        "unexpected linked sound");
	++sound_calls;
	return 0;
}
void start_boss_death_sequence(object *)
{
	++death_calls;
}
void start_robot_death_sequence(object *)
{
	require(false, "unexpected robot death");
}
void special_reactor_stuff(void)
{
	require(false, "unexpected reactor effect");
}
void explode_object(object *actor, fix delay)
{
	require(actor->type == OBJ_ROBOT && actor->id == child_type && delay == F1_0 / 4, "unexpected explosion");
	++death_calls;
}
void buddy_message(char *, ...)
{
	require(false, "unexpected buddy message");
}
void newdemo_dump_note_robot_damage(object *, fix, fix) {}
void input_demo_record_robot_damage_event(object *, int32_t, int32_t) {}
void input_demo_log_replay_robot_damage(object *, int32_t, int32_t) {}
void input_demo_rng_trace_record_rand(int, const char *, const char *, int,
                                      uint32_t, int, uint32_t, int, uint32_t, int) {}
int wall_is_doorway(segment *, int)
{
	require(false, "unexpected wall geometry");
	return 0;
}
int find_point_seg(const vms_vector *, int)
{
	require(false, "unexpected point lookup");
	return -1;
}

int xl_countdown_valid(const trigger *)
{
	require(false, "unexpected countdown validation");
	return 0;
}
int xl_disarm_action_valid(const trigger *)
{
	require(false, "unexpected disarm validation");
	return 0;
}
int xl_seismic_action_valid(const trigger *)
{
	require(false, "unexpected seismic validation");
	return 0;
}
int xl_message_action(const trigger *)
{
	require(false, "unexpected message");
	return 0;
}
int xl_named_audio_action(trigger *, int)
{
	require(false, "unexpected named audio");
	return 0;
}
int xl_trigger_texture_action(const trigger *)
{
	require(false, "unexpected texture action");
	return 0;
}
int xl_countdown_action(const trigger *)
{
	require(false, "unexpected countdown action");
	return 0;
}
int xl_disarm_action(const trigger *)
{
	require(false, "unexpected disarm action");
	return 0;
}
int xl_seismic_action(const trigger *)
{
	require(false, "unexpected seismic action");
	return 0;
}
int xl_spawn_action(const trigger *)
{
	require(false, "unexpected ordinary spawn");
	return 0;
}
int xl_speed_boost_action(const trigger *, int, int, int)
{
	require(false, "unexpected speed boost");
	return 0;
}
int xl_object_master_action(const trigger *, int)
{
	require(false, "unexpected master action");
	return 0;
}
int xl_object_master_supported(const trigger *)
{
	require(false, "unexpected master validation");
	return 0;
}
int xl_ambient_object_set_enabled(int, int)
{
	require(false, "unexpected ambient action");
	return 0;
}
int xl_particle_object_set_enabled(int, int)
{
	require(false, "unexpected particle action");
	return 0;
}
int xl_lightning_object_set_enabled(int, int)
{
	require(false, "unexpected lightning action");
	return 0;
}
int xl_object_classic_action(const trigger *)
{
	require(false, "unexpected classic action");
	return 0;
}
}
