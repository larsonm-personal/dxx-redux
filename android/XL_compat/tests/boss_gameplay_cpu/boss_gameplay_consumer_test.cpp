// Asset-free consumer check. All boss, trigger and generator decisions are linked
// from production translation units; only external engine effects are recorded.
#include "object_trigger.h"
#include "boss_spawn.h"
#include "object_birth.h"
#include "robot_generator.h"
#include "inventory.h"
#include "seismic_gameplay.h"
#include "robot_disarm.h"
#include "projectile.h"
#include "boss_routes.h"
#include "robot_disarm_state.h"
#include "trigger_state.h"
#include "preview.h"
#include "geometry.h"
#include "fluids.h"
#include "spawn_points.h"
#include "speed_boost.h"
#include "slow_motion_runtime.h"
#include "input_demo_replay.h"
#include <nlohmann/json.hpp>
#include "gameplay_audio.h"
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <array>
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
#include "weapon.h"
#include "laser.h"
#include "bm.h"
#include "automap.h"
#include "hudmsg.h"
#include "kconfig.h"
#include "newmenu.h"
#include "window.h"
#include "text.h"
#include "console.h"
#include "d1_in_d2/d1_in_d2_presentation.h"
#include "d1_in_d2/d1_in_d2_levels.h"
extern int HighlightMarker;
void collide_player_and_materialization_center(object *);
void collide_robot_and_materialization_center(object *);
void collide_player_and_wall(object *, fix, short, short, vms_vector *);
void scrape_player_on_wall(object *, short, short, vms_vector *);
void reset_shield_warning_damage(void);
extern fix total_ouch;
extern fix64 last_damage_time;
#include "sounds.h"
#include "cntrlcen.h"
#include "gr.h"
#include "endlevel.h"
#include "rng_shared.h"
extern int Seismic_sound, Seismic_sound_playing, Seismic_tremor_volume, Seismic_tremor_magnitude;
extern fix64 Next_seismic_sound_time;
void do_countdown_frame(void);
#include "multi.h"
void bump_this_object(object *, object *, vms_vector *, int);
void collide_robot_and_robot(object *, object *, vms_vector *);
#include "reactor_countdown.h"
#include "args.h"
int do_boss_weapon_collision(object *, object *, vms_vector *);
void collide_robot_and_weapon(object *, object *, vms_vector *);
object *object_create_explosion_sub(object *, short, vms_vector *, fix, int, fix, fix, fix, int);
void collide_weapon_and_controlcen(object *, object *, vms_vector *);
extern fix Boss_invulnerable_dot;
#include "newdemo.h"
#include "input_demo_hooks.h"
#include "input_demo_rng_trace.h"
int object_to_object_visibility(object *, object *, int);
}

namespace
{
bool self_splash_trace = false, self_splash_protected = false;
object *self_splash_killer = nullptr;
int self_splash_visibility = 0, self_splash_forces = 0, self_splash_rotations = 0, self_splash_telemetry = 0, self_splash_damage_events = 0, self_splash_palettes = 0;
vms_vector self_splash_force{}, self_splash_rotation{};
fix self_splash_damage = 15 * F1_0;
bool self_splash_visible = true;
int self_splash_console = 0, self_splash_timers = 0;
std::vector<int> self_splash_events;
bool separating_trace = false;
object *separating_first = nullptr, *separating_second = nullptr;
vms_vector separating_force{}, separating_rotation{};
std::vector<std::array<int, 5>> separating_events;
bool mine_trace = false, mine_repeat = false, mine_blocked = false, mine_chain_expected = false;
int mine_id = PMINE_ID, mine_allocations = 0, mine_audio = 0, mine_classic_audio = 0, mine_children = 0;
vms_vector mine_position{};
std::vector<std::array<int, 4>> mine_events;
bool smart_trace = false;
bool splash_trace = false;
int splash_visibility = 0, splash_forces = 0, splash_rotations = 0;
bool splash_visible = true;
vms_vector splash_force{}, splash_rotation{}, splash_origin{};
constexpr int owner_a = 3, owner_b = 5, player_slot = 1;
constexpr int boss_type = 12, child_type = 7;
constexpr fix strength = 400 * F1_0;
int births, ai_calls, morph_calls, effect_calls, sound_calls, death_calls;
int requested_type, requested_segment;
bool allocation_failure, extensions = true;
bool callback_point_lookup = false;
int callback_point_calls = 0;
bool weak_spot_collision = false;
const vms_vector *weak_spot_hit = nullptr;
int weak_spot_sound_calls = 0;
bool death_trace = false;
int death_sound_events, final_sound_events, final_explosions, reactor_notifications, notification_remaining;
int countdown_wall_calls, slow_motion_off_calls;
bool master_forcefield_trace = false;
int master_forcefield_centers = 0, master_forcefield_connections = 0;
std::array<int, 2> master_forcefield_initial{};
std::vector<std::array<int, 4>> master_forcefield_events;
bool master_trace = false, master_reverse = false;
int master_named = 0;
std::vector<std::array<int, 3>> master_results;
bool boost_trace = false, boost_fixture_restore = false;
bool checkpoint_trace = false;
int checkpoint_messages = 0, checkpoint_relinks = 0, checkpoint_deletes = 0, checkpoint_expected_segment = 1, checkpoint_marker = -1;
bool fluid_trace = false, fluid_protected = false, fluid_inactive_dry = false;
int fluid_water_queries = 0;
int fluid_boost = 0, fluid_sounds = 0, fluid_damage_records = 0, fluid_damage_traces = 0, fluid_warnings = 0;
vms_vector fluid_point{};
std::vector<std::array<int, 3>> fluid_palettes;
std::vector<int> fluid_events;
bool matcen_trace = false, matcen_locked = false, matcen_protected = false, matcen_axis = false;
object *matcen_actor = nullptr;
fix matcen_before;
int matcen_exits, matcen_sounds, matcen_explosions, matcen_damage_records, matcen_damage_traces, matcen_palettes, matcen_warnings, matcen_force_calls;
std::vector<int> matcen_center_sides, matcen_events;
vms_vector matcen_recorded_force{};
bool forcefield_trace = false, forcefield_protected = false, forcefield_observer = false;
const vms_vector *forcefield_point = nullptr;
fix forcefield_before;
int forcefield_rotations, forcefield_sounds, forcefield_awareness, forcefield_damage_records, forcefield_trace_records, forcefield_warning_queries, forcefield_protection_queries;
vms_vector forcefield_force{};
std::vector<int> forcefield_events;
std::vector<std::array<int, 3>> forcefield_palettes;
bool gauss_trace = false;
int gauss_force_calls, gauss_rot_calls, gauss_static_calls;
bool teleport_trace = false;
int teleport_relinks, teleport_centers;
std::vector<int> teleport_outputs;
bool reactor_contact_trace = false;
const vms_vector *reactor_contact_point = nullptr;
int reactor_contact_impacts, reactor_contact_hitsounds;
fix reactor_contact_size;
bool contact_trace = false;
const vms_vector *contact_point = nullptr;
int contact_hud, contact_awareness, contact_ai_hits, contact_impact_records, contact_static, contact_resolutions;
fix contact_requested, contact_before;
bool disarm_trace = false;
fix64 disarm_clock;
int disarm_results, disarm_named_frames, disarm_message_frames, disarm_named_wall_frames;
bool seismic_trace = false;
int seismic_begins, seismic_ends;
std::vector<std::pair<int, int>> seismic_audio;
bool countdown_trace = false, countdown_time_paused = false, countdown_replay_finish = false;
std::vector<std::pair<int, int>> countdown_samples;
int countdown_flashes, countdown_canvas, countdown_clears, countdown_resets, countdown_deaths, countdown_replay_calls;
int countdown_flash_value;
bool reactor_trace = false;
int reactor_audio, reactor_explosions, reactor_score, reactor_cloak, reactor_hit_logs, reactor_death_logs, reactor_named;
int reactor_backend;
std::vector<int> reactor_effect_calls;
std::vector<std::array<int, 3>> reactor_hit_samples;
bool bump_trace = false;
object *bump_victim = nullptr;
vms_vector bump_full{}, bump_rotation{};
int bump_force_calls, bump_rot_calls;
object created_snapshot{};
int created_snapshot_index = -1;
Mission fixture_mission{};

void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}

void restore_fixture_boost(int boosted)
{
	xl_speed_boost_clear();
	if (!extensions) return;
	xl_speed_boost_state state{};
	state.owner = player_slot;
	state.signature = Objects[player_slot].signature;
	state.boosted = boosted;
	state.speed = 1;
	for (int axis = 0; axis < 3; ++axis) {
		state.minimum[axis] = -60 * F1_0;
		state.maximum[axis] = 60 * F1_0;
	}
	boost_fixture_restore = true;
	const int okay = xl_speed_boost_restore(boosted ? &state : nullptr, boosted ? 1 : 0, 1, 1);
	boost_fixture_restore = false;
	require(okay, "inherited fixture installs real validated boost registry state");
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
	xl_speed_boost_clear();
	xl_object_trigger_clear();
	xl_object_birth_clear();
	xl_boss_spawn_clear();
	xl_robot_generator_clear();
	births = ai_calls = morph_calls = effect_calls = sound_calls = death_calls = 0;
	slow_motion_off_calls = 0;
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

void natural_spew_attachment_controls()
{
	for (int profile = 0; profile < 12; ++profile) {
		Difficulty_level = 2;
		reset();
		N_robot_types = 41;
		for (const int type : { 38, 40 }) Robot_info[type] = Robot_info[child_type];
		auto level = authored_level(true);
		level.object_trigger_records.clear();
		const auto add = [&](int owner, int type, int flags = 0) {
			xl_compat::ObjectTrigger record;
			record.object = owner;
			record.action.type = type;
			record.action.flags = flags;
			record.action.value = 1;
			record.action.time = -1;
			if (type == 14 || type == 26) record.action.targets.push_back({ 1, 0 });
			level.object_trigger_records.push_back(record);
		};
		if (profile != 7) {
			const int type = profile == 1 ? 14 : profile == 2 ? 26
			                                                  : 30;
			add(profile == 6 ? owner_b : owner_a, type, profile == 4 || profile == 5 ? TF_DISABLED : 0);
			if (profile == 3) add(owner_a, 26);
		}
		xl_object_trigger_load(level);
		if (profile == 5) {
			xl_object_trigger_event(owner_a, 0); // Disabled action is consumed without unrelated audio
			require(trigger_state(-1).owner == -1, "consumed attachment fixture");
		}
		if (profile == 8) {
			++Objects[owner_a].signature;
			xl_boss_spawn_load(); // Rebind the boss, not the stale trigger
		}
		if (profile == 10) extensions = false;
		if (profile == 11) Game_mode = GM_MULTI;
		GameTime64 = interval();
		d_rand_set_state(703);
		const unsigned before_calls = d_rand_get_call_count();
		unsigned before_rng = 0, after_rng = 0;
		d_rand_get_state(&before_rng);
		const bool explicit_spawn = profile == 9;
		const int created = xl_boss_spawn_robot(owner_a, explicit_spawn ? child_type : -1, nullptr, explicit_spawn);
		d_rand_get_state(&after_rng);
		const bool rejected = profile <= 5 || profile == 10 || profile == 11;
		require((created < 0) == rejected,
		        "natural boss spew must reject every attached trigger, not just spawn actions");
		if (rejected) {
			require(births == 0 && !ai_calls && !morph_calls && !effect_calls && !sound_calls &&
			            before_rng == after_rng && before_calls == d_rand_get_call_count(),
			        "attachment suppression consumed RNG or emitted spawn effects");
			if (profile != 10) require(boss_state(owner_a).elapsed == GameTime64, "attachment rejection changed cooldown");
		} else {
			require(births == 1 && ai_calls == 1 && morph_calls == 1 && effect_calls == 1 && sound_calls == 1,
			        "no-attachment/stale/other-owner/explicit spawn lost production effects");
			require(requested_type == (explicit_spawn ? child_type : Objects[created].id) &&
			            (explicit_spawn || requested_type == 38 || requested_type == 40),
			        "natural spew fixture lacks usable profile-21 child definitions");
		}
	}
}

// Test-only actual profile-21 weapon callback; geometry remains an engine endpoint
void weapon_spawn_callback_controls()
{
	for (int profile = 0; profile < 13; ++profile) {
		Difficulty_level = 2;
		reset();
		callback_point_lookup = false;
		callback_point_calls = 0;
		N_robot_types = 41;
		for (const int type : { 38, 40 }) Robot_info[type] = Robot_info[child_type];
		N_weapon_types = 1;
		Weapon_info[0] = {};
		Weapon_info[0].matter = profile == 6;
		auto level = authored_level(true);
		level.object_trigger_records.clear();
		if (profile >= 1 && profile <= 5) {
			xl_compat::ObjectTrigger record;
			record.object = profile == 4 ? owner_b : owner_a;
			record.action.type = 30;
			record.action.flags = profile == 2 || profile == 3 ? TF_DISABLED : 0;
			record.action.time = -1;
			level.object_trigger_records.push_back(record);
		}
		xl_object_trigger_load(level);
		if (profile == 3) xl_object_trigger_event(owner_a, 0);
		if (profile == 5) {
			++Objects[owner_a].signature;
			xl_boss_spawn_load();
		}
		Objects[2] = {};
		Objects[2].type = profile == 8 ? OBJ_CLUTTER : OBJ_WEAPON;
		Objects[2].id = profile == 9 ? 1 : 0;
		Objects[2].ctype.laser_info.parent_type = profile == 7 ? OBJ_ROBOT : OBJ_PLAYER;
		if (profile == 10) extensions = false;
		if (profile == 11) Game_mode = GM_MULTI;
		GameTime64 = profile == 12 ? interval() - 1 : interval();
		const bool creates = profile == 0 || profile == 4 || profile == 5;
		const unsigned draws = creates ? 4 : profile == 12 ? 1
		                                                   : 0;
		d_rand_set_state(703);
		d_rand_set_call_count(0);
		for (unsigned i = 0; i < draws; ++i) (void) d_rand();
		unsigned expected_rng = 0, after_rng = 0;
		d_rand_get_state(&expected_rng);
		d_rand_set_state(703);
		d_rand_set_call_count(0);
		callback_point_lookup = true;
		xl_boss_weapon_spawn(owner_a, 2, &Objects[owner_a].pos);
		callback_point_lookup = false;
		d_rand_get_state(&after_rng);
		require(births == int(creates) && ai_calls == int(creates) && morph_calls == int(creates) &&
		            effect_calls == int(creates) && sound_calls == int(creates),
		        "actual weapon callback attachment/profile controls emitted wrong spawn effects");
		require(d_rand_get_call_count() == draws && after_rng == expected_rng,
		        "actual weapon callback changed source RNG consumption");
		require(callback_point_calls == int(creates || profile == 12),
		        "rejected weapon callback unexpectedly entered placement geometry");
		if (creates) require(requested_type == 38 || requested_type == 40,
			                 "actual weapon callback selected outside valid profile-21 children");
		if (profile != 10) require(boss_state(owner_a).elapsed == (creates ? 0 : GameTime64),
			                       "actual weapon callback changed rejection/cooldown state");
	}
	std::cout << "Actual profile-21 weapon callback: 13 attachment, attribution, energy/kinetic, cooldown and inactive/multiplayer controls passed\n";
}

void boss_weak_spot_controls(bool range_only = false)
{
	struct Sample {
		vms_vector hit, forward;
		int difficulty, exact_dot, quick_dot;
	};
	const Sample samples[] = {
		{ { 65536, 720896, 196608 }, { 0, 0, F1_0 }, 0, 17177, 15968 },
		{ { 65536, 720896, -196608 }, { 0, 0, F1_0 }, 4, -17177, -15968 },
		{ { 65536, 1507328, 196608 }, { 0, 0, F1_0 }, 1, 8468, 8086 },
		{ { 65536, 1507328, -196608 }, { 0, 0, F1_0 }, 3, -8468, -8086 },
		{ { 65536, 720896, -293221 }, { 39321, 0, 52428 }, 4, -16383, -15177 },
		{ { 0, 0, 0 }, { 0, 0, F1_0 }, 2, 0, 0 },
		{ { F1_0, 0, 1 }, { 0, 0, F1_0 }, 2, 1, 1 },
		{ { F1_0, 0, -1 }, { 0, 0, F1_0 }, 2, -1, -1 },
		{ { 1300000000, 1300000000, 1300000000 }, { 0, 0, F1_0 }, 0, 37837, 41943 },
	};
	int cases = 0;
	for (const int role : { 26, 28 })
		for (int mode = 0; mode < 4; ++mode)
			for (const auto &sample : samples) {
				if (range_only && sample.hit.x != 1300000000) continue;
				Difficulty_level = sample.difficulty;
				reset();
				Robot_info[boss_type].boss_flag = role;
				xl_boss_spawn_load();
				Objects[owner_a].orient = vmd_identity_matrix;
				Objects[owner_a].orient.fvec = sample.forward;
				Objects[2] = {};
				Objects[2].type = OBJ_WEAPON;
				Objects[2].control_type = CT_WEAPON;
				Objects[2].signature = 612;
				Objects[2].id = 0;
				Objects[2].segnum = Objects[owner_a].segnum;
				// Classic controls isolate math without entering legacy intrinsic spew
				Objects[2].ctype.laser_info.parent_type = mode == 1 ? OBJ_ROBOT : OBJ_PLAYER;
				N_weapon_types = 1;
				Weapon_info[0] = {};
				Weapon_info[0].matter = 1; // Reflection cloning is a separate behavior
				if (mode == 1) extensions = false;
				if (mode == 2) Game_mode = GM_MULTI;
				if (mode == 3) fixture_mission.descent_version = 1;
				Boss_invulnerable_dot = F1_0 / 4 - i2f(Difficulty_level) / 8;
				const bool exact = mode == 0;
				const bool allowed = (exact ? sample.exact_dot : sample.quick_dot) <= Boss_invulnerable_dot;
				auto hit = sample.hit;
				const auto weapon_before = Objects[2];
				const auto boss_before = Objects[owner_a];
				weak_spot_hit = &hit;
				weak_spot_collision = true;
				weak_spot_sound_calls = callback_point_calls = 0;
				unsigned before_rng = 0, after_rng = 0;
				d_rand_get_state(&before_rng);
				const auto calls = d_rand_get_call_count();
				const bool actual = do_boss_weapon_collision(&Objects[owner_a], &Objects[2], &hit) != 0;
				weak_spot_collision = false;
				weak_spot_hit = nullptr;
				d_rand_get_state(&after_rng);
				if (actual != allowed)
					std::cerr << "weakspot role=" << role << " mode=" << mode << " difficulty=" << Difficulty_level
					          << " exact_dot=" << sample.exact_dot << " quick_dot=" << sample.quick_dot << " threshold=" << Boss_invulnerable_dot << '\n';
				require(actual == allowed, "actual boss weak-spot acceptance differs from pinned normalization/dot profile");
				require(weak_spot_sound_calls == int(!allowed) && callback_point_calls == int(!allowed),
				        "weak-spot acceptance changed impact sound/segment dispatch");
				require(!births && !ai_calls && !morph_calls && !effect_calls && !sound_calls &&
				            before_rng == after_rng && calls == d_rand_get_call_count(),
				        "weak-spot acceptance changed intrinsic births, reflection or simulation RNG");
				require(!std::memcmp(&Objects[2], &weapon_before, sizeof(object)) &&
				            !std::memcmp(&Objects[owner_a], &boss_before, sizeof(object)),
				        "matter weak-spot contact changed weapon or boss state");
				++cases;
			}
	std::cout << "Actual boss weak spot: " << cases << " source-boundary/scope and separate range-safety consumer cases passed\n";
}

void boss_reflected_weapon_controls()
{
	struct Sample {
		vms_vector incoming, exact, quick;
		bool xl_only = false;
	};
	const Sample samples[] = {
		{ { 458752, -131072, -196608 }, { -174640, 138510, 189524 }, { -170216, 144408, 196896 } },
		{ { -196608, 262144, -458752 }, { 158097, -51342, 329031 }, { 161371, -46977, 334490 } },
		{ { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
		{ { 1, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, true },
		{ { 3, -2, 1 }, { 0, 0, 0 }, { 0, 0, 0 }, true },
		{ { 2147483647, 2147483647, 2147483647 }, { -679225525, -547722241, -416204769 }, { 0, 0, 0 }, true },
		{ { -2147483648, 0, 0 }, { 1301512192, 303693824, 379625472 }, { 0, 0, 0 }, true },
		{ { 1300000000, -1300000000, 1300000000 }, { -411175742, 968423233, -251953583 }, { 0, 0, 0 }, true },
	};
	int cases = 0;
	for (const int role : { 26, 28 })
		for (int mode = 0; mode < 4; ++mode)
			for (const auto &sample : samples)
				for (int disposition = 0; disposition < 4; ++disposition) {
					if (sample.xl_only && mode != 0) continue;
					Difficulty_level = 0;
					reset();
					Robot_info[boss_type].boss_flag = role;
					xl_boss_spawn_load();
					Objects[owner_a].orient = vmd_identity_matrix;
					Objects[2] = {};
					auto &weapon = Objects[2];
					weapon.type = OBJ_WEAPON;
					weapon.control_type = CT_WEAPON;
					weapon.movement_type = MT_PHYSICS;
					weapon.render_type = RT_NONE;
					weapon.signature = 612;
					weapon.id = 0;
					weapon.segnum = Objects[owner_a].segnum;
					weapon.pos = { 111, 222, 333 };
					weapon.orient = vmd_identity_matrix;
					weapon.size = F1_0;
					weapon.lifeleft = i2f(17);
					weapon.ctype.laser_info.parent_type = mode == 1 ? OBJ_ROBOT : OBJ_PLAYER;
					weapon.ctype.laser_info.parent_num = player_slot;
					weapon.ctype.laser_info.last_hitobj = owner_b;
					weapon.mtype.phys_info.mass = F1_0 / 2;
					weapon.mtype.phys_info.drag = F1_0 / 16;
					weapon.mtype.phys_info.thrust = { 1, 2, 3 };
					weapon.mtype.phys_info.flags = PF_PERSISTENT | PF_BOUNCED_ONCE;
					weapon.mtype.phys_info.velocity = sample.incoming;
					N_weapon_types = 1;
					Weapon_info[0] = {};
					Weapon_info[0].matter = disposition == 1;
					Weapon_info[0].mass = 3 * F1_0;
					Weapon_info[0].drag = F1_0 / 4;
					if (mode == 1) extensions = false;
					if (mode == 2) Game_mode = GM_MULTI;
					if (mode == 3) fixture_mission.descent_version = 1;
					Boss_invulnerable_dot = F1_0 / 4;
					const bool allowed = disposition == 2;
					const bool clone_attempt = disposition == 0 || disposition == 3;
					const bool clone_created = disposition == 0;
					allocation_failure = disposition == 3;
					created_snapshot_index = -1;
					created_snapshot = {};
					const auto weapon_before = weapon, boss_before = Objects[owner_a];
					const int highest_before = Highest_object_index;
					vms_vector hit{ 196608, 262144, allowed ? -327680 : 327680 };
					weak_spot_hit = &hit;
					weak_spot_collision = true;
					weak_spot_sound_calls = callback_point_calls = 0;
					unsigned before_rng = 0, after_rng = 0;
					d_rand_get_state(&before_rng);
					const auto calls = d_rand_get_call_count();
					const bool actual = do_boss_weapon_collision(&Objects[owner_a], &weapon, &hit) != 0;
					weak_spot_collision = false;
					weak_spot_hit = nullptr;
					d_rand_get_state(&after_rng);
					require(actual == allowed && births == int(clone_attempt) &&
					            Highest_object_index == highest_before + int(clone_created),
					        "actual reflected-shot allocation/disposition differs");
					require(weak_spot_sound_calls == int(!allowed) && callback_point_calls == int(!allowed) &&
					            !ai_calls && !morph_calls && !effect_calls && !sound_calls &&
					            before_rng == after_rng && calls == d_rand_get_call_count(),
					        "reflected shot changed impact dispatch, intrinsic spawning or RNG");
					require(!std::memcmp(&weapon, &weapon_before, sizeof(object)) &&
					            !std::memcmp(&Objects[owner_a], &boss_before, sizeof(object)),
					        "reflection changed the original weapon or boss");
					if (clone_created) {
						require(created_snapshot_index == highest_before + 1 && created_snapshot.type == OBJ_WEAPON,
						        "reflected energy hit did not create exactly one weapon");
						auto expected = created_snapshot;
						expected.mtype.phys_info.mass = Weapon_info[0].mass;
						expected.mtype.phys_info.drag = Weapon_info[0].drag;
						expected.mtype.phys_info.thrust = {};
						expected.mtype.phys_info.velocity = mode == 0 ? sample.exact : sample.quick;
						if (std::memcmp(&Objects[created_snapshot_index], &expected, sizeof(object))) {
							const auto &v = Objects[created_snapshot_index].mtype.phys_info.velocity;
							std::cerr << "reflection role=" << role << " mode=" << mode << " velocity=" << v.x << ',' << v.y << ',' << v.z << '\n';
						}
						require(!std::memcmp(&Objects[created_snapshot_index], &expected, sizeof(object)),
						        "actual boss reflected clone velocity or constructor-state preservation differs");
					} else
						require(created_snapshot_index == -1, "matter/allowed/failed allocation unexpectedly made a clone");
					++cases;
				}
	std::cout << "Actual reflected boss shot: " << cases << " velocity, clone, constructor-state, zero and scoped controls passed\n";
}

void boss_death_controls()
{
	int cases = 0;
	for (int profile : { 25, 23, -23 })
		for (int remaining : { 0, 1, 2 }) {
			const int sign = profile > 0 ? 1 : -1;
			Difficulty_level = 2;
			reset();
			death_trace = true;
			death_sound_events = final_sound_events = final_explosions = reactor_notifications = 0;
			notification_remaining = -1;
			countdown_wall_calls = 0;
			Control_center_destroyed = 0;
			Control_center_present = 1;
			Base_control_center_explosion_time = DEFAULT_CONTROL_CENTER_EXPLOSION_TIME;
			Countdown_timer = 0;
			Total_countdown_time = 0;
			Reactor_countdown_paused = 1;
			Dead_controlcen_object_num = -1;
			ControlCenterTriggers = {};
			ControlCenterTriggers.num_links = 1;
			ControlCenterTriggers.seg[0] = 1;
			ControlCenterTriggers.side[0] = 2;
			Robot_info[boss_type].boss_flag = profile;
			Robot_info[boss_type].deathroll_sound = 77;
			if (remaining != 1) Objects[owner_b].type = OBJ_NONE;
			if (remaining == 2) {
				Objects[4].type = OBJ_CNTRLCEN;
				Objects[4].shields = F1_0;
			}
			xl_boss_spawn_load();
			const int objectives = (sign > 0 ? 1 + (remaining == 1) : 0) + (remaining == 2);
			require(xl_level_objectives_remaining() == objectives, "authored boss/reactor objectives");
			const fix64 start = GameTime64 = 20 * F1_0;
			FrameTime = 0; // Deterministic no-fireball branch; sound timing remains real
			require(apply_damage_to_robot(&Objects[owner_a], Objects[owner_a].shields + 1, player_slot) == 1,
			        "actual lethal damage must start boss death");
			require(death_calls == 1 && boss_state(owner_a).dying == 1 && boss_state(owner_a).death_elapsed == 0,
			        "lethal death endpoint must call actual boss death registry");
			xl_boss_start_death(owner_a);
			auto frame = [&](fix64 time, int rng_calls) {
				GameTime64 = time;
				const auto before = d_rand_get_call_count();
				xl_boss_deaths_frame();
				require(d_rand_get_call_count() - before == rng_calls, "death-roll RNG call count");
			};
			frame(start + BOSS_DEATH_DURATION - F1_0, 1);
			require(!death_sound_events, "death sound boundary is strict");
			frame(start + BOSS_DEATH_DURATION - F1_0 + 1, 0);
			require(death_sound_events == 1, "death sound must start once after threshold");
			frame(start + BOSS_DEATH_DURATION - 1, 1);
			require(boss_state(owner_a).dying == 1 && xl_level_objectives_remaining() == objectives && !final_explosions,
			        "duration-minus-one retains dying owner and objective");
			std::vector<xl_boss_spawn_state> saved(xl_boss_spawn_count());
			for (int i = 0; i < int(saved.size()); ++i) require(xl_boss_spawn_state_get(i, &saved[i]), "death snapshot");
			GameTime64 = 100 * F1_0;
			require(xl_boss_spawn_restore(saved.data(), int(saved.size()), 0), "validate mid-roll at shifted clock");
			require(xl_boss_spawn_restore(saved.data(), int(saved.size()), 1), "restore actual mid-roll");
			require(boss_state(owner_a).death_elapsed == BOSS_DEATH_DURATION - 1 && boss_state(owner_a).sound == 1,
			        "restored death epoch and one-shot sound");
			const fix64 shifted = GameTime64;
			frame(shifted + 1, 1);
			require(boss_state(owner_a).dying == 1 && xl_level_objectives_remaining() == objectives && !final_explosions,
			        "exact duration retains owner and objective");
			frame(shifted + 2, 1);
			require(final_explosions == 1 && final_sound_events == 1 && death_sound_events == 1,
			        "completion explodes and sounds once");
			const bool final_objective = sign > 0 && remaining == 0;
			require(Control_center_destroyed == final_objective && countdown_wall_calls == int(final_objective), "actual countdown waits for final objective");
			require(Countdown_timer == (final_objective ? 45 * F1_0 : 0) && Total_countdown_time == (final_objective ? 45 : 0), "real final-objective countdown initialized");
			require(Reactor_countdown_paused == !final_objective && Dead_controlcen_object_num == -1, "countdown pause reset and boss reactor identity");
			const int after_objectives = objectives - (sign > 0);
			require(xl_level_objectives_remaining() == after_objectives &&
			            true,
			        "retirement precedes notification; other boss/reactor stays live");
			xl_boss_spawn_state retired{};
			require(xl_boss_spawn_state_get(0, &retired), "retired state");
			require(retired.owner == -1 && retired.dying == 2 && !retired.death_elapsed, "retired owner state");
			frame(shifted + F1_0, 0);
			require(final_explosions == 1 && final_sound_events == 1 && countdown_wall_calls == int(final_objective),
			        "repeated death frame inert");
			death_trace = false;
			++cases;
		}
	std::cout << "Actual boss death and countdown initialization: " << cases << " signed profile, boundary, objective, shifted-restore and one-shot cases passed\n";
}

void countdown_setup()
{
	countdown_wall_calls = 0;
	Control_center_destroyed = 0;
	Control_center_present = 1;
	Countdown_timer = 12345;
	Total_countdown_time = 4321;
	Countdown_seconds_left = 91;
	Reactor_countdown_paused = 1;
	Dead_controlcen_object_num = -1;
	Base_control_center_explosion_time = DEFAULT_CONTROL_CENTER_EXPLOSION_TIME;
	ControlCenterTriggers = {};
	ControlCenterTriggers.num_links = 2;
	for (int i = 0; i < 2; ++i) {
		ControlCenterTriggers.seg[i] = 1;
		ControlCenterTriggers.side[i] = 2 + i;
	}
}
void require_countdown_pending()
{
	require(!Control_center_destroyed && Countdown_timer == 12345 && Total_countdown_time == 4321 &&
	            Countdown_seconds_left == 91 && Reactor_countdown_paused == 1 && !countdown_wall_calls && !slow_motion_off_calls,
	        "remaining objective must preserve every countdown/wall sentinel");
}
void countdown_controls()
{
	int cases = 0;
	const int defaults[] = { 90, 60, 45, 35, 30 };
	for (bool xl : { false, true })
		for (int difficulty = 0; difficulty < NDL; ++difficulty)
			for (int base : { 30, 7 }) {
				Difficulty_level = difficulty;
				reset();
				death_trace = true;
				countdown_setup();
				Objects[owner_a].type = Objects[owner_b].type = OBJ_NONE;
				xl_boss_spawn_load();
				extensions = xl;
				Base_control_center_explosion_time = base;
				do_controlcen_destroyed_stuff(nullptr);
				const int expected = base == 30 ? defaults[difficulty] : base + base * (NDL - difficulty - 1) / 2;
				require(Control_center_destroyed && Countdown_timer == expected * F1_0 && Total_countdown_time == expected &&
				            !Reactor_countdown_paused && Countdown_seconds_left == 91 && countdown_wall_calls == 2 && Dead_controlcen_object_num == -1 && slow_motion_off_calls == 1,
				        "actual countdown difficulty/custom-base, wall-order, pause or display state");
				death_trace = false;
				++cases;
			}
	for (int control : { 0, 1, 2 }) {
		Difficulty_level = 2;
		reset();
		death_trace = true;
		countdown_setup();
		Objects[owner_a].type = Objects[owner_b].type = OBJ_NONE;
		xl_boss_spawn_load();
		if (control == 0) {
			Game_mode = GM_MULTI_ROBOTS;
			Control_center_destroyed = 1;
		} else if (control == 1) ControlCenterTriggers.side[1] = 6;
		else ControlCenterTriggers.seg[1] = Highest_segment_index + 1;
		do_controlcen_destroyed_stuff(nullptr);
		if (control == 0) require(Control_center_destroyed && Countdown_timer == 12345 && Total_countdown_time == 4321 &&
			                          Reactor_countdown_paused && !countdown_wall_calls && Countdown_seconds_left == 91,
			                      "multi already-destroyed preserves countdown");
		else require(Control_center_destroyed && Countdown_timer == 45 * F1_0 && Total_countdown_time == 45 &&
			             !Reactor_countdown_paused && !countdown_wall_calls && Countdown_seconds_left == 91,
			         "invalid links suppress all walls, not countdown");
		death_trace = false;
		++cases;
	}
	for (int order : { 0, 1, 2 }) {
		Difficulty_level = 2;
		reset();
		death_trace = true;
		countdown_setup();
		death_sound_events = final_sound_events = final_explosions = 0;
		Robot_info[boss_type].boss_flag = 25;
		Robot_info[boss_type].deathroll_sound = 77;
		if (order != 0) {
			Objects[owner_b].type = OBJ_NONE;
			Objects[4].type = OBJ_CNTRLCEN;
			Objects[4].shields = F1_0;
		}
		xl_boss_spawn_load();
		GameTime64 = 20 * F1_0;
		FrameTime = 0;
		if (order == 2) {
			Objects[4].shields = -1;
			do_controlcen_destroyed_stuff(&Objects[4]);
			require_countdown_pending();
			require(Dead_controlcen_object_num == 4, "early reactor records its identity");
		}
		auto kill = [&](int owner) {
			require(apply_damage_to_robot(&Objects[owner], Objects[owner].shields + 1, player_slot) == 1, "order control actual lethal boss ingress");
			GameTime64 += BOSS_DEATH_DURATION - 1;
			xl_boss_deaths_frame();
			require_countdown_pending();
			++GameTime64;
			xl_boss_deaths_frame();
			require_countdown_pending();
			++GameTime64;
			xl_boss_deaths_frame();
		};
		kill(owner_a);
		if (order == 0) {
			require_countdown_pending();
			kill(owner_b);
		} else if (order == 1) {
			require_countdown_pending();
			Objects[4].shields = -1;
			do_controlcen_destroyed_stuff(&Objects[4]);
		}
		require(Control_center_destroyed && Countdown_timer == 45 * F1_0 && Total_countdown_time == 45 &&
		            !Reactor_countdown_paused && countdown_wall_calls == 2 && !xl_level_objectives_remaining(),
		        "last objective initializes real countdown in both destruction orders");
		Countdown_timer = 44 * F1_0 + 123;
		++GameTime64;
		xl_boss_deaths_frame();
		require(Countdown_timer == 44 * F1_0 + 123 && countdown_wall_calls == 2 && Countdown_seconds_left == 91,
		        "retired boss frame must not reset advanced countdown or retoggle");
		death_trace = false;
		++cases;
	}
	for (int control : { 0, 1, 2 }) {
		reset();
		countdown_setup();
		trigger action{};
		action.type = control == 1 ? 0 : 25;
		action.time = 10;
		extensions = control != 2;
		require(xl_countdown_action(&action) == (control == 0), "countdown trigger admission");
		require(slow_motion_off_calls == (control == 0), "only an accepted countdown trigger requests time-equipment shutdown");
		if (control == 0)
			require(Control_center_destroyed && Countdown_timer == 10 * F1_0 && !Reactor_countdown_paused,
			        "accepted countdown trigger publishes countdown before shutdown");
		else require_countdown_pending();
		++cases;
	}
	std::cout << "Actual countdown initialization: " << cases << " profile, difficulty, custom-base, order, validation and scope cases passed\n";
}

void direct_bump_controls(bool oblique_only = false, int only_difficulty = -1, bool guards_only = false)
{
	struct Sample {
		int difficulty;
		vms_vector force, scaled;
		fix robot_damage, clutter_damage;
	};
	const Sample samples[] = {
		{ 2, { 4379954, 2391887, 14563121 }, { 729992, 398647, 2427187 }, 160359, 320718 },
		{ 3, { 2647386, 15113188, 1496621 }, { 378198, 2159027, 213803 }, 137644, 275288 },
		{ 0, { 24 * F1_0, 32 * F1_0, 0 }, { 6 * F1_0, 8 * F1_0, 0 }, 40960, 81920 },
		{ 4, { 24 * F1_0, 32 * F1_0, 0 }, { 3 * F1_0, 4 * F1_0, 0 }, 20480, 40960 },
		{ 2, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0 },
	};
	int cases = 0;
	for (const auto &sample : samples)
		for (int mode : { 0, 1, 2, 3 })
			for (int type : { OBJ_ROBOT, OBJ_CLUTTER })
				for (int sign : { -1, 1 })
					for (int control : { 0, 1, 2, 3, 4, 5, 6 }) {
						if (guards_only && mode == 0) continue;
						if (only_difficulty >= 0 && sample.difficulty != only_difficulty) continue;
						if (oblique_only && (mode != 0 || type != OBJ_ROBOT || control != 0 || sample.force.x != 24 * F1_0)) continue;
						Difficulty_level = sample.difficulty;
						reset();
						GameTime64 = 20 * F1_0;
						extensions = mode != 1;
						Game_mode = mode == 2 ? GM_MULTI : 0;
						if (mode == 3) fixture_mission.descent_version = 1;
						auto &victim = Objects[4];
						victim = {};
						victim.type = type;
						victim.id = child_type;
						victim.signature = 804;
						victim.segnum = 0;
						victim.movement_type = MT_PHYSICS;
						victim.mtype.phys_info.mass = F1_0;
						victim.shields = 100 * F1_0;
						if (control == 1) victim.mtype.phys_info.flags = PF_PERSISTENT;
						if (control == 2) victim.flags = OF_EXPLODING;
						if (control == 3 && type == OBJ_ROBOT) {
							victim.id = boss_type;
							Robot_info[boss_type].boss_flag = 25;
						}
						xl_object_birth_created(4);
						if (control != 4) GameTime64 += F1_0;
						vms_vector force{ sign * sample.force.x, sign * sample.force.y, sign * sample.force.z };
						if (control == 5) force = {};
						const auto before = victim;
						const auto other_before = Objects[player_slot];
						const bool immune = control == 1 || (control == 3 && type == OBJ_ROBOT);
						const bool zero_damage = immune || control == 2 || control == 5 || control == 6 || (control == 4 && type == OBJ_ROBOT && mode != 1);
						vms_vector expected_rotation{};
						if (mode == 0) expected_rotation = { sign * sample.scaled.x, sign * sample.scaled.y, sign * sample.scaled.z };
						else expected_rotation = { force.x / (4 + Difficulty_level), force.y / (4 + Difficulty_level), force.z / (4 + Difficulty_level) };
						if (control == 5) expected_rotation = {};
						fix expected_damage = 0;
						if (!zero_damage) {
							if (mode == 0) expected_damage = type == OBJ_ROBOT ? sample.robot_damage : sample.clutter_damage;
							else {
								const vms_vector *damage_vector = mode == 3 ? &expected_rotation : &force;
								fix d = vm_vec_mag_quick(damage_vector) / 8;
								if ((mode == 1 || mode == 2) && d < F1_0 / 3) d = 0;
								expected_damage = type == OBJ_ROBOT ? d / 2 : d;
							}
						}
						bump_trace = true;
						bump_victim = &victim;
						bump_force_calls = bump_rot_calls = 0;
						const auto rng = d_rand_get_call_count();
						bump_this_object(&victim, &Objects[player_slot], &force, control == 6 ? 0 : 1);
						bump_trace = false;
						if (bump_rot_calls && std::memcmp(&bump_rotation, &expected_rotation, sizeof(vms_vector)))
							std::cerr << "bump rotation mode=" << mode << " diff=" << Difficulty_level << " actual=" << bump_rotation.x << ',' << bump_rotation.y << ',' << bump_rotation.z << '\n';
						require(bump_force_calls == !immune && bump_rot_calls == !immune, "direct bump physics boundary call policy");
						require(immune || (!std::memcmp(&bump_full, &force, sizeof(force)) && !std::memcmp(&bump_rotation, &expected_rotation, sizeof(force))),
						        "full force or source-scaled rotation vector differs");
						if (victim.shields != before.shields - expected_damage) std::cerr << "damage mode=" << mode << " diff=" << Difficulty_level << " type=" << type << " control=" << control << " actual=" << before.shields - victim.shields << " expected=" << expected_damage << '\n';
						require(victim.shields == before.shields - expected_damage, "actual surviving bump damage differs");
						victim.shields = before.shields;
						require(!std::memcmp(&victim, &before, sizeof(victim)) && !std::memcmp(&Objects[player_slot], &other_before, sizeof(other_before)),
						        "recorded physics boundaries must leave actor state unchanged except real shield damage");
						require(d_rand_get_call_count() == rng && !death_calls && !births && !sound_calls && !effect_calls && !ai_calls && !morph_calls,
						        "direct bump altered RNG or lifecycle effects");
						++cases;
					}
	std::cout << "Actual direct Bump/force damage: " << cases << " vector, surviving damage and isolated profile controls passed\n";
}

void reactor_damage_controls()
{
	int cases = 0;
	auto setup = [&](int objectives, int variant) {
		Difficulty_level = 2;
		reset();
		countdown_setup();
		reactor_trace = death_trace = true;
		reactor_audio = reactor_explosions = reactor_score = reactor_cloak = reactor_hit_logs = reactor_death_logs = reactor_named = 0;
		reactor_effect_calls.clear();
		reactor_hit_samples.clear();
		Control_center_been_hit = 7;
		Highest_object_index = 7;
		Objects[4] = {};
		Objects[4].type = OBJ_CNTRLCEN;
		Objects[4].signature = 704;
		Objects[4].shields = 20 * F1_0;
		Objects[2] = {};
		Objects[2].type = OBJ_POWERUP;
		Objects[2].signature = 702;
		Objects[owner_b].type = OBJ_NONE;
		if (objectives != 1) Objects[owner_a].type = OBJ_NONE;
		else {
			Robot_info[boss_type].boss_flag = 25;
			Robot_info[boss_type].deathroll_sound = 77;
		}
		if (objectives == 2) {
			Objects[owner_b] = {};
			Objects[owner_b].type = OBJ_CNTRLCEN;
			Objects[owner_b].signature = 705;
			Objects[owner_b].shields = 20 * F1_0;
		}
		xl_boss_spawn_load();
		xl_compat::Inventory level;
		level.segments.resize(2);
		level.objects.resize(8);
		xl_compat::ObjectTrigger sound{};
		sound.object = 4;
		sound.action.type = 30;
		sound.action.value = 17 * F1_0;
		sound.action.time = 3;
		sound.action.flags = TF_ONE_SHOT;
		if (variant == 1) sound.action.flags |= TF_DISABLED;
		level.object_trigger_records.push_back(sound);
		auto effect = sound;
		effect.action.type = 33;
		effect.action.flags = variant == 1 ? TF_DISABLED : 0;
		effect.action.value = 0;
		effect.action.time = -1;
		effect.action.targets = { { 6, -1 }, { 7, -1 } };
		level.object_trigger_records.push_back(effect);
		auto unrelated = sound;
		unrelated.object = 2;
		unrelated.action.flags = TF_DISABLED;
		level.object_trigger_records.push_back(unrelated);
		xl_object_trigger_load(level);
		if (variant == 2) ++Objects[4].signature;
		if (variant == 3) extensions = false;
		GameTime64 = 20 * F1_0;
	};
	for (int objectives : { 0, 1, 2 })
		for (int variant : { 0, 1, 2, 3 })
			for (int backend : { 0, 1, 2 }) {
				setup(objectives, variant);
				reactor_backend = backend;
				auto &reactor = Objects[4];
				const auto initial_rng = d_rand_get_call_count();
				apply_damage_to_controlcen(&reactor, 10 * F1_0, player_slot);
				require(reactor.shields == 10 * F1_0 && Control_center_been_hit == 1, "real reactor surviving local damage marks hit");
				require_countdown_pending();
				apply_damage_to_controlcen(&reactor, 10 * F1_0, player_slot);
				require(reactor.shields == 0 && Control_center_been_hit == 1 && !reactor_audio && !reactor_explosions && !reactor_named && reactor_effect_calls.empty(), "zero shields is not reactor death");
				require_countdown_pending();
				apply_damage_to_controlcen(&reactor, 1, player_slot);
				require(reactor.shields == -1 && Control_center_been_hit == 1 && reactor_audio == 1 && reactor_explosions == 1 && reactor_score == 5000 &&
				            reactor_cloak == 3 && reactor_hit_logs == 3 && reactor_death_logs == 1,
				        "actual reactor death score/audio/explosion endpoints");
				require(reactor_hit_samples == std::vector<std::array<int, 3>>{ { 4, 10 * F1_0, 20 * F1_0 }, { 4, 10 * F1_0, 10 * F1_0 }, { 4, 1, 0 } } && d_rand_get_call_count() == initial_rng, "real reactor hit fields and deterministic removal RNG");
				const bool starts = objectives == 0 || variant == 3;
				if (starts) require(Control_center_destroyed && Countdown_timer == 45 * F1_0 && Total_countdown_time == 45 && !Reactor_countdown_paused && countdown_wall_calls == 2,
					                "final reactor initializes real countdown");
				else require_countdown_pending();
				require(Dead_controlcen_object_num == 4, "actual reactor death records owner");
				const bool dispatched = variant == 0;
				if (reactor_named != int(dispatched)) std::cerr << "reactor objectives=" << objectives << " variant=" << variant << " backend=" << backend << " named=" << reactor_named << " triggercount=" << xl_object_trigger_count() << "\n";
				if (reactor_named != int(dispatched)) {
					for (int i = 0; i < xl_object_trigger_count(); ++i) {
						xl_object_trigger_state st{};
						xl_object_trigger_state_get(i, &st);
						std::cerr << "state " << st.owner << " sig=" << st.signature << " type=" << st.type << " op=" << st.operated << " flags=" << st.flags << '\n';
					}
				}
				require(reactor_named == int(dispatched), "attached named sound dispatch scope");
				std::vector<int> expected;
				if (dispatched)
					for (int target : { 6, 7 })
						for (int b = 0; b <= backend; ++b) expected.push_back(target * 10 + b);
				require(reactor_effect_calls == expected, "effect target/fallback dispatch differs");
				const auto effects_before = reactor_effect_calls;
				const int named_before = reactor_named;
				xl_object_trigger_event(4, 0);
				require(reactor_named == named_before && reactor_effect_calls == effects_before, "consumed reactor removal must not redispatch");
				if (variant != 3) {
					int consumed = 0, unrelated = 0;
					for (int i = 0; i < xl_object_trigger_count(); ++i) {
						xl_object_trigger_state state{};
						require(xl_object_trigger_state_get(i, &state), "reactor trigger state");
						if (state.owner == -1) {
							++consumed;
							require(bool(state.operated) == dispatched, "reactor attachment operation status");
							if (variant == 0 || variant == 1) {
								if (state.type == 30) require((state.flags & (TF_ONE_SHOT | TF_DISABLED)) == (TF_ONE_SHOT | TF_DISABLED), "named one-shot disable and authored disabled flags preserved");
								else require(state.flags == (variant == 1 ? TF_DISABLED : 0), "effect action does not inherit named sound one-shot flag");
							}
						} else {
							++unrelated;
							require(state.owner == 2 && !state.operated && (state.flags & TF_DISABLED), "unrelated owner action preserved");
						}
					}
					require(consumed == 2 && unrelated == 1, "both reactor removal bindings consumed only");
				}
				if (variant == 3) {
					extensions = true;
					for (int i = 0; i < xl_object_trigger_count(); ++i) {
						xl_object_trigger_state st{};
						require(xl_object_trigger_state_get(i, &st), "classic preserved action snapshot");
						require(!st.operated && (st.owner == 2 || st.owner == 4), "inactive XL must preserve attachment bindings");
					}
					extensions = false;
				}
				if (objectives && variant != 3) {
					if (objectives == 1) {
						reactor_trace = false;
						FrameTime = 0;
						require(apply_damage_to_robot(&Objects[owner_a], Objects[owner_a].shields + 1, player_slot) == 1, "last boss actual lethal ingress");
						GameTime64 += BOSS_DEATH_DURATION + 1;
						xl_boss_deaths_frame();
					} else {
						reactor_trace = true;
						apply_damage_to_controlcen(&Objects[owner_b], 20 * F1_0 + 1, player_slot);
					}
					require(Control_center_destroyed && Countdown_timer == 45 * F1_0 && !Reactor_countdown_paused && countdown_wall_calls == 2,
					        "last remaining objective starts real countdown after reactor removal");
				}
				reactor_trace = death_trace = false;
				++cases;
			}
	for (int control : { 0, 1, 2, 3, 4, 5 }) {
		setup(0, 0);
		auto &reactor = Objects[4];
		int who = player_slot;
		if (control == 0) who = -1;
		if (control == 1) who = Highest_object_index + 1;
		if (control == 2 || control == 3) {
			who = 2;
			Objects[2].type = control == 2 ? OBJ_ROBOT : OBJ_WEAPON;
		}
		if (control == 4 || control == 5) reactor.flags = control == 4 ? OF_EXPLODING : OF_DESTROYED;
		apply_damage_to_controlcen(&reactor, 20 * F1_0 + 1, who);
		require(reactor.shields == (control < 4 ? 20 * F1_0 : -1) && Control_center_been_hit == (control < 4 ? 7 : 1), "attribution/flag reactor shield and hit-marker semantics");
		require(!reactor_audio && !reactor_explosions && !reactor_score && !reactor_named && reactor_effect_calls.empty() &&
		            reactor_cloak == (control >= 4) && reactor_hit_logs == (control >= 4) && !reactor_death_logs,
		        "rejected attribution or preexisting flags must suppress reactor death effects");
		require_countdown_pending();
		reactor_trace = death_trace = false;
		++cases;
	}
	std::cout << "Actual reactor damage/removal: " << cases << " damage, zero/death, attribution, objective and attached dispatch cases passed\n";
}

void countdown_advance_controls(bool seconds_only = false, bool classic_only = false)
{
	auto start = [&] {
		Difficulty_level = 2;
		reset();
		countdown_setup();
		Objects[owner_a].type = Objects[owner_b].type = OBJ_NONE;
		Objects[4] = {};
		Objects[4].type = OBJ_CNTRLCEN;
		Objects[4].signature = 904;
		Objects[4].shields = 20 * F1_0;
		xl_boss_spawn_load();
		xl_object_trigger_clear();
		reactor_trace = death_trace = true;
		apply_damage_to_controlcen(&Objects[4], 20 * F1_0 + 1, player_slot);
		reactor_trace = death_trace = false;
		require(Control_center_destroyed && Countdown_timer == 45 * F1_0, "advancement starts from actual reactor destruction");
		Countdown_seconds_left = 45;
		ConsoleObject = &Objects[player_slot];
		ConsoleObject->mtype.phys_info.rotvel = { 0, 123, 0 };
		xl_countdown_clear();
		countdown_trace = true;
		countdown_time_paused = countdown_replay_finish = false;
		countdown_samples.clear();
		countdown_flashes = countdown_canvas = countdown_clears = countdown_resets = countdown_deaths = countdown_replay_calls = 0;
		countdown_flash_value = -1;
		PaletteRedAdd = PaletteGreenAdd = PaletteBlueAdd = -1;
		d_srand(2);
		Player_is_dead = Endlevel_sequence = 0;
	};
	auto state = [] { xl_countdown_state value{}; xl_countdown_get(&value);return value; };
	int cases = 0;
	if (!classic_only) {
		if (!seconds_only) {
			start();
			const auto rng = d_rand_get_call_count();
			FrameTime = 1638;
			do_countdown_frame();
			auto first = state();
			require(first.timer == 45 * F1_0 && first.pending == 1638 && first.phase == 24 && first.fraction == 65136 && d_rand_get_call_count() == rng,
			        "subtick phase/fraction/pending cadence");
			FrameTime = 1;
			do_countdown_frame();
			auto tick = state();
			unsigned actual_rng = 0;
			d_rand_get_state(&actual_rng);
			require(tick.timer == 2947481 && !tick.pending && !tick.phase && tick.fraction == 600 && d_rand_get_call_count() == rng + 2 && actual_rng == 1495354192u,
			        "due tick consumes exact real RNG stream and accumulated frame");
			if (ConsoleObject->mtype.phys_info.rotvel.x != -2901) std::cerr << "countdown actual rotation=" << ConsoleObject->mtype.phys_info.rotvel.x << ',' << ConsoleObject->mtype.phys_info.rotvel.z << '\n';
			require(ConsoleObject->mtype.phys_info.rotvel.x == -2901 && ConsoleObject->mtype.phys_info.rotvel.y == 123 && ConsoleObject->mtype.phys_info.rotvel.z == 1206,
			        "actual XL countdown signed multiply must truncate toward zero");
			require(countdown_samples.empty() && !countdown_flashes && !countdown_deaths, "positive cadence is silent and nonterminal");
			require(xl_countdown_restore(&tick, 0) && xl_countdown_restore(&tick, 1), "saved countdown phase/fraction round trip");
			auto roundtrip = state();
			require(!std::memcmp(&tick, &roundtrip, sizeof(tick)), "countdown clock round trip exact");
			countdown_trace = false;
			++cases;
		}
		start();
		Countdown_timer = -57343;
		Countdown_seconds_left = 0;
		FrameTime = F1_0 / 32;
		do_countdown_frame();
		if (Countdown_seconds_left != 0) std::cerr << "countdown actual negative seconds=" << Countdown_seconds_left << " timer=" << Countdown_timer << '\n';
		require(Countdown_timer == -59391 && Countdown_seconds_left == 0, "negative rounded seconds must truncate toward zero");
		require(countdown_flash_value == 14 && PaletteRedAdd == 14 && PaletteGreenAdd == 14 && PaletteBlueAdd == 14 && !countdown_deaths,
		        "negative countdown palette field dispatch");
		countdown_trace = false;
		++cases;
		if (seconds_only) {
			std::cout << "Actual countdown negative display control passed\n";
			return;
		}
		for (const auto boundary : std::array<std::array<int, 2>, 6>{ { { -57345, 0 }, { -57344, 0 }, { -57343, 0 }, { 8191, 0 }, { 8192, 1 }, { 8193, 1 } } }) {
			start();
			Countdown_timer = boundary[0] + 2048;
			FrameTime = 2048;
			do_countdown_frame();
			require(Countdown_timer == boundary[0] && Countdown_seconds_left == boundary[1] && !countdown_deaths,
			        "positive/negative displayed-second truncation boundaries");
			countdown_trace = false;
			++cases;
		}
		for (int guard : { 0, 1, 2, 3, 4 }) {
			start();
			auto before = state();
			if (guard == 2 || guard == 3) {
				before.pending = 17;
				require(xl_countdown_restore(&before, 1), "dead/inactive pending setup");
			}
			const auto rng = d_rand_get_call_count();
			FrameTime = 2048;
			if (guard == 0) Reactor_countdown_paused = 1;
			if (guard == 1) countdown_time_paused = true;
			if (guard == 2) Player_is_dead = 1;
			if (guard == 3) Control_center_destroyed = 0;
			if (guard == 4) extensions = false;
			if (guard == 4) xl_countdown_frame();
			else do_countdown_frame();
			const auto after = state();
			if (guard == 0 || guard == 4) require(!std::memcmp(&before, &after, sizeof(before)) && d_rand_get_call_count() == rng, "paused/inactive runtime preserves entire clock");
			else if (guard == 1) require(after.timer == before.timer && !after.pending && after.phase == 0 && after.fraction == 16384 && d_rand_get_call_count() == rng + 2,
				                         "time pause rocks due tick and clears pending but preserves timer");
			else require(after.timer == before.timer && !after.pending && !after.phase && after.fraction == 16384 && d_rand_get_call_count() == rng,
				         "dead/not-destroyed clears pending while clock advances without RNG");
			countdown_trace = false;
			++cases;
		}
		for (int event : { 0, 1, 2, 3 }) {
			start();
			FrameTime = 2048;
			if (event == 0) {
				Countdown_timer = 51 * F1_0 / 4 + 1;
				Countdown_seconds_left = 13;
			}
			if (event == 1) {
				Countdown_timer = 1;
				Countdown_seconds_left = 0;
			}
			if (event >= 2) {
				Countdown_timer = -4 * F1_0 + 2048;
				Countdown_seconds_left = -3;
				countdown_replay_finish = event == 3;
			}
			do_countdown_frame();
			if (event == 0) require(countdown_samples == std::vector<std::pair<int, int>>{ { 113, 3 * F1_0 } }, "13-second voice crossing exact sample and volume");
			if (event == 1) {
				require(countdown_samples == std::vector<std::pair<int, int>>{ { 33, F1_0 } } && countdown_flash_value == 0 && !countdown_deaths, "zero crossing explosion sound once");
				countdown_samples.clear();
				do_countdown_frame();
				require(countdown_samples.empty(), "subsequent negative tick does not repeat explosion");
			}
			if (event >= 2) require(Countdown_timer == -4 * F1_0 && countdown_flash_value == 64 && countdown_canvas == 1 && countdown_clears == 1 && countdown_resets == 1 && countdown_replay_calls == 1 && countdown_deaths == (event == 2), "exact minus-four expiry canvas/palette/death dispatch and replay suppression");
			countdown_trace = false;
			++cases;
		}
	}
	start();
	extensions = false;
	FrameTime = 1639;
	d_tick_step = 1;
	const auto rng = d_rand_get_call_count();
	do_countdown_frame();
	if (ConsoleObject->mtype.phys_info.rotvel.x != -2901) std::cerr << "classic actual=" << ConsoleObject->mtype.phys_info.rotvel.x << "," << ConsoleObject->mtype.phys_info.rotvel.y << "," << ConsoleObject->mtype.phys_info.rotvel.z << " timer=" << Countdown_timer << " calls=" << d_rand_get_call_count() - rng << "\n";
	require(ConsoleObject->mtype.phys_info.rotvel.x == -2901 && ConsoleObject->mtype.phys_info.rotvel.y == 123 && ConsoleObject->mtype.phys_info.rotvel.z == 1206 &&
	            Countdown_timer == 45 * F1_0 - 1639 && d_rand_get_call_count() == rng + 2,
	        "classic linked signed multiply behavior remains unchanged");
	countdown_trace = false;
	++cases;
	std::cout << "Actual countdown advancement: " << cases << " cadence, real-RNG, signed rounding, guard, sound and expiry cases passed\n";
}

void seismic_gameplay_controls()
{
	auto start = [&] {
		Difficulty_level = 2;
		reset();
		GameTime64 = 20 * F1_0;
		seismic_trace = true;
		countdown_trace = false;
		countdown_time_paused = false;
		seismic_begins = seismic_ends = 0;
		seismic_audio.clear();
		Seismic_sound = 42;
		Seismic_sound_playing = Seismic_tremor_volume = Seismic_tremor_magnitude = 0;
		Next_seismic_sound_time = 0;
		xl_seismic_clear();
		ConsoleObject = &Objects[player_slot];
		ConsoleObject->mtype.phys_info.rotvel = { 0, 123, 0 };
		Buddy_objnum = 4;
		Objects[4] = {};
		Objects[4].type = OBJ_ROBOT;
		Objects[4].id = child_type;
		Objects[4].signature = 1004;
		Objects[4].mtype.phys_info.rotvel = { 0, -321, 0 };
		d_srand(2);
		d_srand_fx(7);
	};
	auto action = [] { trigger value{};value.type=36;value.value=2*F1_0;value.time=2;return value; };
	auto rng = [] { unsigned sim=0,fx=0;d_rand_get_stream_state(D_RNG_SIM,&sim);d_rand_get_stream_state(D_RNG_FX,&fx);return std::array<unsigned,4>{sim,fx,d_rand_get_stream_call_count(D_RNG_SIM),d_rand_get_stream_call_count(D_RNG_FX)}; };
	int cases = 0;
	start();
	auto request = action();
	require(xl_seismic_owned() && xl_seismic_action_valid(&request) && xl_seismic_action(&request), "actual seismic trigger admission");
	const auto seeded = rng();
	const auto player_before = ConsoleObject->mtype.phys_info.rotvel, buddy_before = Objects[4].mtype.phys_info.rotvel;
	FrameTime = 1638;
	GameTime64 += FrameTime;
	require(xl_seismic_frame() == 1, "owned seismic subtick");
	auto subtick = xl_compat::seismic_gameplay_snapshot();
	require(subtick[5] == 24 && subtick[6] == 65136 && rng() == seeded && seismic_audio.empty() && !seismic_begins &&
	            !std::memcmp(&ConsoleObject->mtype.phys_info.rotvel, &player_before, sizeof(vms_vector)) && !std::memcmp(&Objects[4].mtype.phys_info.rotvel, &buddy_before, sizeof(vms_vector)),
	        "seismic 25ms subtick is inert");
	FrameTime = 1;
	GameTime64 += 1;
	const fix64 initial = GameTime64;
	require(xl_seismic_frame() == 1, "owned seismic first tick");
	auto first = xl_compat::seismic_gameplay_snapshot();
	auto first_rng = rng();
	require(first[5] == 0 && first[6] == 600 && first[1] == 0 && first[2] == nlohmann::json::array({ 0, 2 * F1_0 }), "actual seismic interval starts at first due tick");
	require(ConsoleObject->mtype.phys_info.rotvel.x == 5803 && ConsoleObject->mtype.phys_info.rotvel.y == 123 && ConsoleObject->mtype.phys_info.rotvel.z == -2413 &&
	            Objects[4].mtype.phys_info.rotvel.x == 23212 && Objects[4].mtype.phys_info.rotvel.y == -321 && Objects[4].mtype.phys_info.rotvel.z == -9652,
	        "real seeded seismic player/buddy rotation values");
	require(Seismic_tremor_magnitude == 5803 && Seismic_tremor_volume == 16 && Seismic_sound_playing == 1 && Next_seismic_sound_time == initial + 9782 &&
	            first_rng[0] == 1495354192u && first_rng[1] == 3429651764u && first_rng[2] == seeded[2] + 2 && first_rng[3] == seeded[3] + 1 &&
	            seismic_audio == std::vector<std::pair<int, int>>{ { 1, F1_0 } },
	        "actual seismic SIM/FX stream split and loop start");
	++cases;
	FrameTime = 2048;
	GameTime64 = initial + F1_0;
	xl_seismic_frame();
	auto middle_rng = rng();
	require(ConsoleObject->mtype.phys_info.rotvel.x == 13867 && ConsoleObject->mtype.phys_info.rotvel.z == 2140 && Objects[4].mtype.phys_info.rotvel.x == 55468 && Objects[4].mtype.phys_info.rotvel.z == 8560 &&
	            Seismic_tremor_magnitude == 8064 && Seismic_tremor_volume == 1 && Next_seismic_sound_time == GameTime64 + 10643 && middle_rng[0] == 2993822286u && middle_rng[1] == 642666333u && middle_rng[2] == first_rng[2] + 2 && middle_rng[3] == first_rng[3] + 1 &&
	            seismic_audio == std::vector<std::pair<int, int>>{ { 1, F1_0 }, { 3, 2048 } },
	        "actual midpoint rocking and looping-volume request");
	++cases;
	const auto p_mid = ConsoleObject->mtype.phys_info.rotvel, b_mid = Objects[4].mtype.phys_info.rotvel;
	GameTime64 = initial + 2 * F1_0;
	xl_seismic_frame();
	require(!Seismic_tremor_magnitude && !Seismic_tremor_volume && !Seismic_sound_playing && rng() == middle_rng &&
	            !std::memcmp(&p_mid, &ConsoleObject->mtype.phys_info.rotvel, sizeof(vms_vector)) && !std::memcmp(&b_mid, &Objects[4].mtype.phys_info.rotvel, sizeof(vms_vector)) &&
	            seismic_audio == std::vector<std::pair<int, int>>{ { 1, F1_0 }, { 3, 2048 }, { 2, 0 } } && seismic_begins == 3 && seismic_ends == 3,
	        "exact seismic duration expiry stops once without RNG/rotation");
	GameTime64 += 2048;
	xl_seismic_frame();
	require(seismic_audio.size() == 3 && rng() == middle_rng, "inactive episode does not repeat loop stop");
	++cases;
	seismic_trace = false;
	for (int guard : { 0, 1 }) {
		start();
		request = action();
		require(xl_seismic_action(&request), "guard action setup");
		auto saved = xl_compat::seismic_gameplay_snapshot();
		const auto before_rng = rng();
		const auto before_player = *ConsoleObject, before_buddy = Objects[4];
		if (guard == 0) countdown_time_paused = true;
		else extensions = false;
		FrameTime = 2048;
		require(xl_seismic_frame() == (guard == 0), "paused/inactive seismic ownership return");
		require(saved == xl_compat::seismic_gameplay_snapshot() && rng() == before_rng && !std::memcmp(&before_player, ConsoleObject, sizeof(object)) && !std::memcmp(&before_buddy, &Objects[4], sizeof(object)) &&
		            seismic_audio.empty() && !seismic_begins && !seismic_ends,
		        "pause/inactive preserves state,actors,RNG,audio");
		seismic_trace = false;
		++cases;
	}
	for (int buddy : { -1, MAX_OBJECTS, player_slot }) {
		start();
		request = action();
		require(xl_seismic_action(&request), "buddy action setup");
		Buddy_objnum = buddy;
		const auto nonrobot = Objects[player_slot];
		const auto other = Objects[4];
		FrameTime = 2048;
		xl_seismic_frame();
		require(ConsoleObject->mtype.phys_info.rotvel.x == 5803 && ConsoleObject->mtype.phys_info.rotvel.z == -2413 && ConsoleObject->mtype.phys_info.rotvel.y == 123 && !std::memcmp(&other, &Objects[4], sizeof(object)), "invalid/nonrobot buddy untouched while player rocks");
		seismic_trace = false;
		++cases;
	}
	for (int control : { 0, 1, 2, 3, 4, 5, 6 }) {
		start();
		request = action();
		const auto before = xl_compat::seismic_gameplay_snapshot();
		if (control == 1) request.type = 35;
		if (control == 2) request.time = 32768;
		if (control == 3) request.value = -1;
		if (control == 4) request.time = -1;
		if (control == 5) request.time = 0;
		if (control == 6) extensions = false;
		const bool accepted = control >= 3 && control <= 5;
		require(bool(xl_seismic_action(control == 0 ? nullptr : &request)) == accepted, "actual seismic action validation bounds");
		auto after = xl_compat::seismic_gameplay_snapshot();
		if (!accepted) require(after == before, "rejected seismic ingress preserves state");
		if (control == 3) require(after[0] == 0 && after[1] == 2 * F1_0, "negative strength clamps to zero");
		if (control == 4) require(after[0] == 2 * F1_0 && after[1] == 10 * F1_0, "negative duration defaults to ten seconds");
		if (control == 5) {
			const auto before_rng = rng();
			FrameTime = 2048;
			xl_seismic_frame();
			require(!Seismic_tremor_volume && !Seismic_tremor_magnitude && rng() == before_rng && seismic_audio.empty(), "zero duration inert");
		}
		seismic_trace = false;
		++cases;
	}
	start();
	request = action();
	xl_seismic_action(&request);
	FrameTime = 1638;
	xl_seismic_frame();
	auto saved = xl_compat::seismic_gameplay_snapshot();
	require(xl_compat::restore_seismic_gameplay(saved, false) && xl_compat::restore_seismic_gameplay(saved, true) && saved == xl_compat::seismic_gameplay_snapshot(), "actual seismic snapshot validates/applies exact clock round trip");
	auto invalid = saved;
	invalid[5] = 25;
	require(!xl_compat::restore_seismic_gameplay(invalid, true) && saved == xl_compat::seismic_gameplay_snapshot(), "invalid seismic phase restore is atomic");
	FrameTime = 1;
	xl_seismic_frame();
	auto active = xl_compat::seismic_gameplay_snapshot();
	require(!active[2].is_null(), "active seismic interval snapshot");
	const auto restore_rng = rng();
	const auto restore_audio = seismic_audio;
	GameTime64 = 100 * F1_0;
	require(xl_compat::restore_seismic_gameplay(active, false) && xl_compat::restore_seismic_gameplay(active, true) && active == xl_compat::seismic_gameplay_snapshot() && rng() == restore_rng && seismic_audio == restore_audio,
	        "active seismic shifted-clock restore retains elapsed interval without audio/RNG effects");
	seismic_trace = false;
	++cases;
	std::cout << "Actual seismic trigger module: " << cases << " seeded rotation,clock,scope,action,expiry and restore cases passed\n";
}

void autoplay_disarm_controls()
{
	auto start = [&] {
		Difficulty_level = 2;
		reset();
		GameTime64 = disarm_clock = 20 * F1_0;
		xl_object_trigger_clear();
		xl_wall_trigger_reset();
		xl_disarm_clear();
		disarm_trace = true;
		disarm_results = disarm_named_frames = disarm_message_frames = disarm_named_wall_frames = 0;
		for (int slot : { 2, 4 }) {
			Objects[slot] = {};
			Objects[slot].type = OBJ_ROBOT;
			Objects[slot].id = child_type;
			Objects[slot].signature = 1100 + slot;
			Objects[slot].shields = 15 * F1_0;
		}
		Num_triggers = 1;
		Triggers[0] = {};
		auto &action = Triggers[0];
		action.type = 34;
		action.flags = TF_NO_MESSAGE | TF_ONE_SHOT | 128;
		action.time = -1;
		action.num_links = 2;
		action.seg[0] = 2;
		action.seg[1] = 4;
		action.side[0] = action.side[1] = -1;
	};
	auto rng = [] { unsigned sim=0,fx=0;d_rand_get_stream_state(D_RNG_SIM,&sim);d_rand_get_stream_state(D_RNG_FX,&fx);return std::array<unsigned,4>{sim,fx,d_rand_get_stream_call_count(D_RNG_SIM),d_rand_get_stream_call_count(D_RNG_FX)}; };
	const auto expected = nlohmann::json::array({ nlohmann::json::array({ 2, 1102 }), nlohmann::json::array({ 4, 1104 }) });
	int cases = 0;
	start();
	require(!xl_robot_disarmed(2) && !xl_robot_disarmed(4), "robots initially hostile");
	const auto player_before = Objects[player_slot], a_before = Objects[2], b_before = Objects[4];
	const auto before_rng = rng();
	const auto before_seismic = xl_compat::seismic_gameplay_snapshot();
	xl_countdown_state before_countdown{};
	xl_countdown_get(&before_countdown);
	xl_wall_trigger_frame();
	require(xl_robot_disarmed(2) && xl_robot_disarmed(4) && xl_compat::disarm_snapshot() == expected && Triggers[0].flags == 135, "real authored autoplay dispatch disarms both robots and consumes one-shot");
	xl_trigger_state state{};
	require(xl_wall_trigger_state(0, &state) && state.operated == 1 && state.object == player_slot && state.signature == Objects[player_slot].signature && !state.shot && !state.elapsed && state.delay_ms == -1 && !state.attached, "real autoplay scheduled trigger actor state");
	xl_countdown_state after_countdown{};
	xl_countdown_get(&after_countdown);
	require(!std::memcmp(&player_before, &Objects[player_slot], sizeof(object)) && !std::memcmp(&a_before, &Objects[2], sizeof(object)) && !std::memcmp(&b_before, &Objects[4], sizeof(object)) && rng() == before_rng &&
	            before_seismic == xl_compat::seismic_gameplay_snapshot() && !std::memcmp(&before_countdown, &after_countdown, sizeof(before_countdown)),
	        "autoplay leaves actors,RNG,countdown,seismic untouched");
	require(disarm_results == 1 && disarm_named_frames == 1 && disarm_message_frames == 1 && disarm_named_wall_frames == 1 && !xl_speed_boost_state_count(), "actual frame provider boundaries and dispatch result");
	xl_disarm_clear();
	xl_wall_trigger_frame();
	require(xl_compat::disarm_snapshot().empty() && disarm_results == 1 && disarm_named_wall_frames == 1, "consumed autoplay does not redisarm after registry clear");
	disarm_trace = false;
	++cases;
	for (int guard : { 0, 1, 2 }) {
		start();
		const auto guard_player = Objects[player_slot], guard_a = Objects[2], guard_b = Objects[4];
		const auto guard_rng = rng();
		if (guard == 0) Triggers[0].flags |= TF_DISABLED;
		else extensions = false;
		if (guard == 2) Game_mode = GM_MULTI;
		xl_wall_trigger_frame();
		require(!xl_robot_disarmed(2) && !xl_robot_disarmed(4) && xl_compat::disarm_snapshot().empty() && !disarm_results && Triggers[0].flags == (guard == 0 ? 135 : 131), "disabled/inactive/actual multi profile suppresses autoplay");
		require(!std::memcmp(&guard_player, &Objects[player_slot], sizeof(object)) && !std::memcmp(&guard_a, &Objects[2], sizeof(object)) && !std::memcmp(&guard_b, &Objects[4], sizeof(object)) && rng() == guard_rng &&
		            disarm_named_frames == (guard == 0) && disarm_message_frames == (guard == 0) && !disarm_named_wall_frames && !xl_speed_boost_state_count(),
		        "disabled/inactive guard leaves actors and RNG unchanged with correct empty-provider frame scope");
		disarm_trace = false;
		++cases;
	}
	start();
	xl_wall_trigger_frame();
	auto saved = xl_compat::disarm_snapshot();
	xl_disarm_clear();
	require(xl_compat::disarm_restore(saved, false) && xl_compat::disarm_snapshot().empty() && xl_compat::disarm_restore(saved, true) && xl_compat::disarm_snapshot() == saved, "real neutral registry dry-run/apply restore");
	for (int malformed : { 0, 1, 2, 3 }) {
		auto bad = saved;
		if (malformed == 0) bad[0][1] = 999;
		if (malformed == 1) bad.push_back(bad[1]);
		if (malformed == 2) {
			auto first = bad[0];
			bad[0] = bad[1];
			bad[1] = first;
		}
		if (malformed == 3) bad[0] = nlohmann::json::array({ 2 });
		require(!xl_compat::disarm_restore(bad, true) && xl_compat::disarm_snapshot() == saved, "malformed/stale neutral restore rejects atomically");
		++cases;
	}
	++Objects[2].signature;
	require(!xl_robot_disarmed(2) && xl_robot_disarmed(4) && xl_compat::disarm_snapshot() == nlohmann::json::array({ nlohmann::json::array({ 4, 1104 }) }), "simulated reused slot does not inherit neutral signature");
	Objects[4].type = OBJ_CLUTTER;
	require(!xl_robot_disarmed(4) && xl_compat::disarm_snapshot().empty(), "simulated nonrobot slot does not remain disarmed");
	xl_wall_trigger_reset();
	Triggers[0].flags = TF_NO_MESSAGE | TF_ONE_SHOT | 128;
	disarm_clock += F1_0;
	xl_wall_trigger_frame();
	require(xl_robot_disarmed(2) && !xl_robot_disarmed(4) && xl_compat::disarm_snapshot() == nlohmann::json::array({ nlohmann::json::array({ 2, 1103 }) }) && disarm_results == 2 && Triggers[0].flags == 135,
	        "new authored autoplay action can disarm a simulated new valid robot lifetime");
	disarm_trace = false;
	++cases;
	for (int variant : { 0, 1, 2, 3 }) {
		start();
		auto &action = Triggers[0];
		if (variant == 0) {
			action.num_links = 5;
			action.seg[0] = -1;
			action.seg[1] = Highest_object_index + 1;
			action.seg[2] = player_slot;
			action.seg[3] = 2;
			action.side[3] = 0;
			action.seg[4] = 4;
			action.side[4] = -1;
		}
		if (variant == 1) action.num_links = MAX_WALLS_PER_LINK + 1;
		if (variant == 2) action.num_links = -1;
		if (variant == 3) action.type = 35;
		xl_wall_trigger_frame();
		require(!xl_robot_disarmed(2) && bool(xl_robot_disarmed(4)) == (variant == 0) && disarm_results == (variant == 0), "mixed invalid targets skip; invalid action/count rejects before neutral changes");
		disarm_trace = false;
		++cases;
	}
	std::cout << "Actual autoplay disarm chain: " << cases << " authored dispatch,registry,lifetime,guards and restore cases passed\n";
}

void boss_projectile_controls()
{
	auto start = [&](bool matter, bool harmless, bool local, bool persistent, int trigger_mode, bool newborn) {
		Difficulty_level = 2;
		reset();
		GameTime64 = 20 * F1_0;
		xl_projectile_clear();
		Buddy_objnum = -1;
		contact_trace = true;
		contact_hud = contact_awareness = contact_ai_hits = contact_impact_records = contact_static = contact_resolutions = 0;
		Robot_info[boss_type].boss_flag = 25;
		Robot_info[boss_type].energy_blobs = 0;
		Robot_info[boss_type].exp1_vclip_num = Robot_info[boss_type].exp1_sound_num = -1;
		auto &boss = Objects[owner_a];
		boss.movement_type = MT_PHYSICS;
		boss.mtype.phys_info.mass = F1_0;
		boss.mtype.phys_info.velocity = {};
		boss.ctype.ai_info.behavior = 0;
		xl_boss_spawn_load();
		auto level = authored_level(true);
		for (auto &entry : level.object_trigger_records) entry.action.value = trigger_mode ? 1 : 0;
		xl_object_trigger_load(level);
		if (trigger_mode) boss.shields = part(boss.shields, trigger_mode == 1 ? 905 : 895, 1000);
		xl_object_birth_created(owner_a);
		if (!newborn) GameTime64 += F1_0;
		Objects[2] = {};
		Objects[2].type = OBJ_PLAYER;
		Objects[2].id = 1;
		Objects[2].signature = 1202;
		ConsoleObject = &Objects[player_slot];
		auto &weapon = Objects[4];
		weapon = {};
		weapon.type = OBJ_WEAPON;
		weapon.control_type = CT_WEAPON;
		weapon.movement_type = MT_PHYSICS;
		weapon.render_type = RT_NONE;
		weapon.id = 0;
		weapon.signature = 1204;
		weapon.segnum = 0;
		weapon.shields = trigger_mode ? F1_0 : (persistent ? 80 * F1_0 : 10 * F1_0);
		weapon.mtype.phys_info.mass = F1_0;
		weapon.mtype.phys_info.flags = persistent ? PF_PERSISTENT : 0;
		weapon.flags = harmless ? OF_HARMLESS : 0;
		weapon.lifeleft = 10 * F1_0;
		weapon.ctype.laser_info.parent_type = OBJ_PLAYER;
		weapon.ctype.laser_info.parent_num = local ? player_slot : 2;
		weapon.ctype.laser_info.parent_signature = Objects[weapon.ctype.laser_info.parent_num].signature;
		weapon.ctype.laser_info.multiplier = persistent ? F1_0 / 8 : F1_0;
		weapon.ctype.laser_info.last_hitobj = -1;
		N_weapon_types = 1;
		Weapon_info[0] = {};
		Weapon_info[0].matter = matter;
		Weapon_info[0].robot_hit_vclip = -1;
		callback_point_calls = 0;
		callback_point_lookup = trigger_mode != 0;
		weak_spot_sound_calls = 0;
	};
	auto invoke = [&] {
		auto &boss = Objects[owner_a];
		auto &weapon = Objects[4];
		vms_vector point = boss.pos;
		contact_point = &point;
		require(Robot_info[boss.id].boss_flag == 25 && !Robot_info[boss.id].energy_blobs && weapon.id == 0 && !Weapon_info[0].damage_radius && Robot_info[boss.id].exp1_vclip_num == -1 && Robot_info[boss.id].exp1_sound_num == -1 && Weapon_info[0].robot_hit_vclip == -1,
		        "selected collision scope excludes classification,children,splash and impact providers");
		contact_before = boss.shields;
		contact_requested = Weapon_info[0].matter ? 0 : (weapon.mtype.phys_info.flags & PF_PERSISTENT) ? 10 * F1_0
		                                                                                               : weapon.shields;
		collide_robot_and_weapon(&boss, &weapon, &point);
		contact_point = nullptr;
	};
	auto streams = [] { unsigned sim=0,fx=0;d_rand_get_stream_state(D_RNG_SIM,&sim);d_rand_get_stream_state(D_RNG_FX,&fx);return std::array<unsigned,4>{sim,fx,d_rand_get_stream_call_count(D_RNG_SIM),d_rand_get_stream_call_count(D_RNG_FX)}; };
	int cases = 0;
	for (bool matter : { false, true })
		for (bool harmless : { false, true })
			for (bool local : { false, true }) {
				start(matter, harmless, local, false, 0, false);
				const auto old_boss = Objects[owner_a], old_weapon = Objects[4];
				const auto rng = streams();
				invoke();
				const fix damage = matter || harmless ? 0 : 10 * F1_0;
				require(Objects[owner_a].shields == old_boss.shields - damage && (Objects[4].flags & OF_SHOULD_BE_DEAD) && Objects[4].shields == old_weapon.shields,
				        "actual boss projectile shields and nonpersistent retirement");
				auto expected_boss = old_boss, expected_weapon = old_weapon;
				expected_boss.shields -= damage;
				expected_weapon.flags |= OF_SHOULD_BE_DEAD;
				require(!std::memcmp(&expected_boss, &Objects[owner_a], sizeof(object)) && !std::memcmp(&expected_weapon, &Objects[4], sizeof(object)), "isolated accepted contact changes only shield and retirement fields");
				require(contact_hud == 1 && contact_awareness == local && contact_ai_hits == local && contact_impact_records == local && contact_static == !harmless && contact_resolutions == !harmless,
				        "actual local awareness/HUD/damage/bump dispatch counts");
				require(!births && !death_calls && !ai_calls && !morph_calls && !effect_calls && !sound_calls && !trigger_state(owner_a).operated && streams() == rng && weak_spot_sound_calls == matter,
				        "isolated boss contact must not dispatch unrelated effects or RNG");
				contact_trace = false;
				callback_point_lookup = false;
				++cases;
			}
	for (bool matter : { false, true })
		for (bool harmless : { false, true }) {
			start(matter, harmless, true, true, 0, false);
			const fix old_boss = Objects[owner_a].shields;
			invoke();
			const fix accepted = matter || harmless ? 0 : 10 * F1_0;
			require(Objects[owner_a].shields == old_boss - accepted && Objects[4].shields == 80 * F1_0 - (old_boss - accepted) / 4 && !(Objects[4].flags & OF_SHOULD_BE_DEAD) && xl_projectile_has_hit(4, owner_a),
			        "actual persistent history,damage and post-hit attrition");
			xl_projectile_state history{};
			require(xl_projectile_state_get(4, &history) && history.owner == 4 && history.signature == 1204 && history.count == 1 && history.victims[0] == owner_a, "actual persistent history state");
			const auto before_weapon = Objects[4], before_boss = Objects[owner_a];
			const auto hud = contact_hud, aware = contact_awareness, ai = contact_ai_hits, impact = contact_impact_records, bumps = contact_static, resolutions = contact_resolutions;
			++GameTime64;
			invoke();
			xl_projectile_state repeated{};
			require(xl_projectile_state_get(4, &repeated) && !std::memcmp(&history, &repeated, sizeof(history)) && !std::memcmp(&before_weapon, &Objects[4], sizeof(object)) && !std::memcmp(&before_boss, &Objects[owner_a], sizeof(object)) &&
			            contact_hud == hud && contact_awareness == aware && contact_ai_hits == ai && contact_impact_records == impact && contact_static == bumps && contact_resolutions == resolutions,
			        "repeat history rejects before damage,awareness and further projectile attrition");
			require(boss_state(owner_a).hit == 0 && weak_spot_sound_calls == (matter ? 2 : 0), "boss hit and immunity callback precede persistent repeat gate");
			contact_trace = false;
			++cases;
		}
	for (int trigger_case : { 0, 1, 2 }) {
		const bool matter = trigger_case == 1, harmless = trigger_case == 2;
		start(matter, harmless, true, false, matter ? 2 : 1, false);
		const fix old = Objects[owner_a].shields;
		invoke();
		require(Objects[owner_a].shields == old - (matter || harmless ? 0 : F1_0) && births == !harmless && bool(trigger_state(owner_a).operated) == !harmless &&
		            ai_calls == !harmless && morph_calls == !harmless && effect_calls == !harmless && sound_calls == !harmless,
		        "actual contact dispatches authored surviving-damage spawn including eligible zero damage; harmless skips event");
		require(Objects[4].flags & OF_SHOULD_BE_DEAD, "threshold contact retires actual projectile");
		contact_trace = false;
		callback_point_lookup = false;
		++cases;
	}
	start(false, false, true, false, 0, true);
	const auto shields = Objects[owner_a].shields;
	invoke();
	require(Objects[owner_a].shields == shields && contact_resolutions == 1 && contact_awareness == 1 && (Objects[4].flags & OF_SHOULD_BE_DEAD) && !births, "actual newborn boss damage rejection retains awareness and projectile disposition");
	contact_trace = false;
	++cases;

	start(false, false, true, false, 1, false);
	GameTime64 = 20 * F1_0;
	xl_object_birth_clear();
	Objects[owner_a].shields = 300 * F1_0;
	Objects[4].shields = 27 * F1_0;
	require(boss_health_maximum_for_difficulty(strength, Difficulty_level) == 300 * F1_0, "source-sized profile25 maximum");
	invoke();
	require(Objects[owner_a].shields == 273 * F1_0 && !births && !trigger_state(owner_a).operated && (Objects[4].flags & OF_SHOULD_BE_DEAD), "actual first shot leaves91percent without authored spawn");
	Objects[4].flags = 0;
	Objects[4].signature = 1205;
	Objects[4].shields = 3 * F1_0 / 2;
	invoke();
	require(Objects[owner_a].shields == 543 * F1_0 / 2 && births == 1 && trigger_state(owner_a).operated && trigger_state(owner_a).value == 1 && boss_state(owner_a).elapsed == 0 &&
	            requested_type == child_type && requested_segment == 0 && ai_calls == 1 && morph_calls == 1 && effect_calls == 1 && sound_calls == 1 && (Objects[4].flags & OF_SHOULD_BE_DEAD),
	        "second fresh shot reaches source threshold and real authored generation with retained value and owner timer");
	const auto &child = Objects[Highest_object_index];
	require(child.type == OBJ_ROBOT && child.id == child_type && child.segnum == 0 && child.shields == Robot_info[child_type].strength && child.mtype.phys_info.mass == Robot_info[child_type].mass &&
	            child.mtype.phys_info.drag == Robot_info[child_type].drag && child.lifeleft > 0 && xl_object_birth_time(Highest_object_index) == GameTime64 && Players[0].num_robots_level == 1 && Players[0].num_robots_total == 1,
	        "actual generated child attributes,birth registry and population counters");
	contact_trace = false;
	callback_point_lookup = false;
	++cases;
	for (int guard : { 0, 1, 2 }) {
		start(false, false, true, false, 0, false);
		if (guard == 0) {
			Objects[2].type = OBJ_ROBOT;
			Objects[2].id = child_type;
			Objects[4].ctype.laser_info.parent_type = OBJ_ROBOT;
			Objects[4].ctype.laser_info.parent_num = 2;
			Objects[4].ctype.laser_info.parent_signature = Objects[2].signature;
		}
		if (guard == 1) Objects[4].ctype.laser_info.parent_signature = Objects[owner_a].signature;
		if (guard == 2) Objects[owner_a].flags |= OF_EXPLODING;
		const auto old = Objects[owner_a].shields;
		invoke();
		require(Objects[owner_a].shields == old && !contact_hud && !contact_awareness && !contact_ai_hits && !contact_impact_records && !contact_static && !contact_resolutions && !births &&
		            bool(Objects[4].flags & OF_SHOULD_BE_DEAD) == (guard == 2),
		        "hostile-parent/self-signature early rejects preserve projectile; exploding target skips acceptance but retires");
		contact_trace = false;
		++cases;
	}
	for (bool multi : { false, true }) {
		start(false, false, true, false, 0, false);
		extensions = false;
		Game_mode = multi ? GM_MULTI : 0;
		const auto old = Objects[owner_a].shields;
		invoke();
		require(Objects[owner_a].shields == old - 10 * F1_0 && (Objects[4].flags & OF_SHOULD_BE_DEAD) && contact_awareness == 1 && contact_static == 1 && !births,
		        "classic and genuinely inactive-multi ordinary contact controls");
		contact_trace = false;
		++cases;
	}
	start(false, false, false, false, 1, false);
	const auto nonlocal_before = Objects[owner_a].shields;
	invoke();
	require(Objects[owner_a].shields == nonlocal_before - F1_0 && (Objects[4].flags & OF_SHOULD_BE_DEAD) && !contact_awareness && !contact_ai_hits && !contact_impact_records && !births && !trigger_state(owner_a).operated && trigger_state(owner_a).value == 1,
	        "eligible nonlocal player contact retains damage and retirement without local authored damage event");
	contact_trace = false;
	callback_point_lookup = false;
	++cases;
	std::cout << "Actual boss projectile collision: " << cases << " ingress,immunity,harmless,awareness,retirement,history and authored spawn cases passed\n";
}

void reactor_parent_controls(int selector = 0)
{
	auto setup = [&](int mode, bool actions) {
		Difficulty_level = 2;
		reset();
		countdown_setup();
		GameTime64 = 20 * F1_0;
		xl_object_trigger_clear();
		xl_projectile_clear();
		reactor_contact_trace = reactor_trace = death_trace = true;
		reactor_contact_impacts = reactor_contact_hitsounds = reactor_audio = reactor_explosions = reactor_score = reactor_cloak = reactor_hit_logs = reactor_death_logs = reactor_named = 0;
		reactor_effect_calls.clear();
		reactor_hit_samples.clear();
		reactor_backend = 0;
		Control_center_been_hit = 7;
		Highest_object_index = actions ? 7 : 5;
		Objects[owner_b] = {};
		Objects[owner_b].type = OBJ_PLAYER;
		Objects[owner_b].id = 1;
		Objects[owner_b].signature = 1305;
		Objects[4] = {};
		Objects[4].type = OBJ_CNTRLCEN;
		Objects[4].signature = 1304;
		Objects[4].shields = 20 * F1_0;
		Objects[4].size = 20 * F1_0;
		Objects[3] = {};
		Objects[3].type = OBJ_WEAPON;
		Objects[3].control_type = CT_WEAPON;
		Objects[3].id = 0;
		Objects[3].signature = 1303;
		Objects[3].segnum = 0;
		Objects[3].shields = 10 * F1_0;
		Objects[3].ctype.laser_info.multiplier = F1_0;
		Objects[3].ctype.laser_info.parent_type = OBJ_PLAYER;
		Objects[3].ctype.laser_info.parent_num = player_slot;
		Objects[3].ctype.laser_info.parent_signature = Objects[player_slot].signature;
		Objects[3].ctype.laser_info.last_hitobj = -1;
		xl_boss_spawn_load();
		N_weapon_types = 1;
		Weapon_info[0] = {};
		Weapon_info[0].robot_hit_vclip = -1;
		ConsoleObject = &Objects[player_slot];
		extensions = mode != 1 && mode != 4;
		Game_mode = (mode == 3 || mode == 4) ? GM_MULTI : 0;
		if (mode == 2) fixture_mission.descent_version = 1;
		reactor_contact_size = mode == 2 ? ((20 * F1_0 / 3) * 3) / 4 : 3 * F1_0;
		if (actions) {
			for (int slot : { 6, 7 }) {
				Objects[slot] = {};
				Objects[slot].type = OBJ_FIREBALL;
				Objects[slot].signature = 1300 + slot;
			}
			xl_compat::Inventory level;
			level.objects.resize(8);
			level.segments.resize(2);
			xl_compat::ObjectTrigger sound{};
			sound.object = 4;
			sound.action.type = 30;
			sound.action.flags = TF_ONE_SHOT;
			sound.action.value = 17 * F1_0;
			sound.action.time = 3;
			level.object_trigger_records.push_back(sound);
			auto effect = sound;
			effect.action.type = 33;
			effect.action.flags = 0;
			effect.action.time = -1;
			effect.action.targets = { { 6, -1 }, { 7, -1 } };
			level.object_trigger_records.push_back(effect);
			xl_object_trigger_load(level);
		}
	};
	auto invoke = [&] { vms_vector point=Objects[4].pos;reactor_contact_point=&point;collide_weapon_and_controlcen(&Objects[3],&Objects[4],&point);reactor_contact_point=nullptr; };
	auto finish = [&] { reactor_contact_trace = reactor_trace = death_trace = false; };
	int cases = 0;
	for (int mode : { 0, 1, 2, 3, 4 }) {
		if (selector == 1 && mode != 0) continue;
		if (selector == 2 && mode == 0) continue;
		setup(mode, false);
		const int missing = Highest_object_index + 1;
		require(missing < MAX_OBJECTS, "safe defined missing-parent counterexample");
		Objects[missing] = {};
		Objects[missing].id = Player_num;
		Objects[3].ctype.laser_info.parent_num = missing;
		invoke();
		if (Control_center_been_hit != (mode == 0 ? 7 : 1)) std::cerr << "reactor parent mode=" << mode << " marker=" << Control_center_been_hit << " parent=" << missing << " last=" << Highest_object_index << " impact=" << reactor_contact_impacts << " hitsound=" << reactor_contact_hitsounds << " retired=" << bool(Objects[3].flags & OF_SHOULD_BE_DEAD) << " shields=" << Objects[4].shields << '\n';
		require(Control_center_been_hit == (mode == 0 ? 7 : 1), "missing parent premark follows pinned bounds only in XL singleplayer non-D1");
		require(Objects[4].shields == 20 * F1_0 && reactor_contact_impacts == 1 && reactor_contact_hitsounds == 1 && (Objects[3].flags & OF_SHOULD_BE_DEAD) && !reactor_cloak && !reactor_hit_logs && !reactor_audio && !reactor_explosions && !reactor_score,
		        "absent parent continues impact,sound,damage rejection and projectile retirement");
		require_countdown_pending();
		finish();
		++cases;
	}
	if (selector) {
		std::cout << "Actual reactor parent guard: " << cases << " defined-profile cases passed\n";
		return;
	}
	for (int parent : { -1, MAX_OBJECTS }) {
		setup(0, false);
		Objects[3].ctype.laser_info.parent_num = parent;
		invoke();
		require(Control_center_been_hit == 7 && Objects[4].shields == 20 * F1_0 && reactor_contact_impacts == 1 && reactor_contact_hitsounds == 1 && (Objects[3].flags & OF_SHOULD_BE_DEAD), "corrected XL parent bounds reject negative/array-end before lookup without skipping impact");
		finish();
		++cases;
	}
	setup(0, true);
	for (int shot = 0; shot < 3; ++shot) {
		Objects[3].flags = 0;
		Objects[3].signature = 1303 + shot;
		Objects[3].shields = shot == 2 ? 1 : 10 * F1_0;
		invoke();
		require(Objects[4].shields == (shot == 0 ? 10 * F1_0 : shot == 1 ? 0
		                                                                 : -1) &&
		            Control_center_been_hit == 1 && reactor_contact_impacts == shot + 1 && reactor_contact_hitsounds == shot + 1 && (Objects[3].flags & OF_SHOULD_BE_DEAD),
		        "real valid reactor weapon damage survives/zero/death and retires each fresh projectile");
		if (shot < 2) {
			require_countdown_pending();
			require(!reactor_named && reactor_effect_calls.empty(), "surviving/zero reactor collision does not dispatch removal");
		}
	}
	require(Control_center_destroyed && Countdown_timer == 45 * F1_0 && !Reactor_countdown_paused && countdown_wall_calls == 2 && reactor_score == 5000 && reactor_audio == 1 && reactor_explosions == 1 && reactor_cloak == 3 && reactor_hit_logs == 3 && reactor_death_logs == 1 &&
	            reactor_named == 1 && reactor_effect_calls == std::vector<int>{ 60, 70 },
	        "actual reactor collision death reaches countdown,removal bindings and recorded outputs");
	for (int i = 0; i < xl_object_trigger_count(); ++i) {
		xl_object_trigger_state st{};
		require(xl_object_trigger_state_get(i, &st) && st.owner == -1 && st.operated, "reactor weapon death consumes each removal binding");
	}
	finish();
	++cases;
	for (int control : { 0, 1, 2, 3 }) {
		setup(0, false);
		if (control == 0) Objects[3].ctype.laser_info.parent_num = owner_b;
		if (control == 1) {
			Objects[2] = {};
			Objects[2].type = OBJ_ROBOT;
			Objects[2].id = Player_num;
			Objects[3].ctype.laser_info.parent_num = 2;
		}
		if (control == 2) Objects[3].ctype.laser_info.parent_signature = -123;
		if (control == 3) Objects[3].ctype.laser_info.parent_type = OBJ_ROBOT;
		invoke();
		const bool damaged = control == 0 || control == 2;
		require(Objects[4].shields == (damaged ? 10 * F1_0 : 20 * F1_0) && Control_center_been_hit == ((control == 1 || control == 2) ? 1 : 7) && reactor_contact_impacts == 1 && reactor_contact_hitsounds == (control != 3) && (Objects[3].flags & OF_SHOULD_BE_DEAD),
		        "normal nonlocal,in-range nonplayer,stale signature and robot-parent source semantics preserved");
		require(reactor_cloak == (control == 2) && reactor_hit_logs == (control == 2), "actual accepted/rejected local reactor damage notifications");
		require_countdown_pending();
		finish();
		++cases;
	}
	std::cout << "Actual reactor weapon parent: " << cases << " bounds,isolation,impact,damage,retirement and removal-chain cases passed\n";
}

void boss_cycle_controls(bool axes_only = false)
{
	auto streams = [] { unsigned sim=0,fx=0;d_rand_get_stream_state(D_RNG_SIM,&sim);d_rand_get_stream_state(D_RNG_FX,&fx);return std::array<unsigned,4>{sim,fx,d_rand_get_stream_call_count(D_RNG_SIM),d_rand_get_stream_call_count(D_RNG_FX)}; };
	auto start = [&] {
		teleport_trace = false;
		Difficulty_level = 2;
		reset();
		GameTime64 = 20 * F1_0;
		fixture_mission.last_level = 9;
		Robot_info[boss_type].boss_flag = 25;
		Robot_info[boss_type].see_sound = 61;
		Vclip[VCLIP_MORPHING_ROBOT].sound_num = 57;
		Objects[owner_b].type = OBJ_NONE;
		Num_segments = 3;
		Highest_segment_index = 2;
		Segments[2] = {};
		Segments[2].objects = -1;
		for (int side = 0; side < 6; ++side) {
			Segments[2].children[side] = -1;
			Segments[2].sides[side].wall_num = -1;
		}
		Segments[0].children[0] = 1;
		Segments[1].children[0] = 0;
		Objects[owner_a].pos = {};
		Objects[owner_a].segnum = 0;
		Objects[owner_a].size = 4 * F1_0;
		Objects[owner_a].orient = vmd_identity_matrix;
		Objects[owner_a].ctype.ai_info.CLOAKED = 0;
		Objects[owner_a].mtype.phys_info.velocity = {};
		ConsoleObject = &Objects[player_slot];
		ConsoleObject->pos = { 110 * F1_0, 20 * F1_0, 30 * F1_0 };
		ConsoleObject->size = 2 * F1_0;
		Ai_local_info[owner_a] = {};
		Ai_local_info[owner_a].mode = 42;
		Ai_local_info[owner_a].next_fire = 15 * F1_0;
		Ai_local_info[owner_a].next_fire2 = 17 * F1_0;
		xl_boss_spawn_load();
		auto routes = xl_compat::boss_routes();
		require(routes.size() == 1 && routes[0].home == 0 && routes[0].segments == std::vector<int>{ 0, 1 }, "actual authored boss destination traversal");
		teleport_relinks = teleport_centers = 0;
		teleport_outputs.clear();
		teleport_trace = true;
		d_srand(2);
	};
	auto pose = [&] {
		vms_matrix expected{};
		expected.rvec = { 62173, 0, -20724 };
		expected.uvec = { -11078, 55387, -33233 };
		expected.fvec = { 17515, 35030, 52545 };
		const vms_vector position{ 100 * F1_0, 0, 0 };
		if (std::memcmp(&Objects[owner_a].orient, &expected, sizeof(expected))) std::cerr << "actual teleport up=" << Objects[owner_a].orient.uvec.x << ',' << Objects[owner_a].orient.uvec.y << ',' << Objects[owner_a].orient.uvec.z << '\n';
		require(Objects[owner_a].segnum == 1 && !std::memcmp(&Objects[owner_a].pos, &position, sizeof(position)) && !std::memcmp(&Objects[owner_a].orient, &expected, sizeof(expected)) &&
		            boss_state(owner_a).teleport == 0 && Ai_local_info[owner_a].mode == 42 && !Ai_local_info[owner_a].next_fire && !Ai_local_info[owner_a].next_fire2 &&
		            teleport_relinks == 1 && teleport_centers == 1 && teleport_outputs == std::vector<int>{ 1, 2, 3, 4 },
		        "actual teleport destination,source matrix,owned timer,fire reset and output sequence");
	};
	auto restored_cloak = [&](fix elapsed, fix remaining, fix since_teleport) {
		xl_boss_spawn_state state{};
		require(xl_boss_spawn_state_get(0, &state), "boss cloak setup snapshot");
		state.cloak_start = -elapsed;
		state.cloak_end = remaining;
		state.teleport = -since_teleport;
		require(xl_boss_spawn_restore(&state, 1, 0) && xl_boss_spawn_restore(&state, 1, 1), "valid authored cloak timer restore");
		Objects[owner_a].ctype.ai_info.CLOAKED = 1;
	};
	std::array<unsigned, 4> initial{};
	int cases = 0;
	if (!axes_only) {
		start();
		initial = streams();
		GameTime64 = 10 * F1_0;
		xl_boss_ai_frame(owner_a, 1);
		require(!Objects[owner_a].ctype.ai_info.CLOAKED && streams() == initial, "exact nonfinal cloak interval does not draw");
		++GameTime64;
		const auto cloak_start = GameTime64;
		xl_boss_ai_frame(owner_a, 1);
		auto cloaked = streams();
		require(Objects[owner_a].ctype.ai_info.CLOAKED == 1 && cloaked[0] == 2207042835u && cloaked[2] == initial[2] + 1 && cloaked[1] == initial[1] && cloaked[3] == initial[3], "first eligible visible boss frame starts cloak with one real draw");
		int64_t began = 0, ends = 0;
		require(xl_boss_cloak_times(owner_a, &began, &ends) && began == cloak_start && ends == cloak_start + BOSS_CLOAK_DURATION, "actual seven-second cloak interval");
		GameTime64 = cloak_start + 3 * F1_0;
		xl_boss_ai_frame(owner_a, 1);
		auto teleported = streams();
		pose();
		require(Objects[owner_a].ctype.ai_info.CLOAKED == 1 && boss_state(owner_a).hit == 0 && teleported[0] == 2993822286u && teleported[2] == initial[2] + 4 && teleported[1] == initial[1] && teleported[3] == initial[3], "natural cloak-to-teleport cycle preserves exact SIM/FX behavior");
		GameTime64 = ends;
		xl_boss_ai_frame(owner_a, 1);
		require(Objects[owner_a].ctype.ai_info.CLOAKED == 1 && streams()[2] == teleported[2] + 1, "exact cloak end remains cloaked with continuation draw");
		++GameTime64;
		auto at_end = streams();
		xl_boss_ai_frame(owner_a, 1);
		require(!Objects[owner_a].ctype.ai_info.CLOAKED && streams() == at_end, "after cloak end uncloaks without extra draw");
		GameTime64 = ends + 10 * F1_0;
		xl_boss_ai_frame(owner_a, 1);
		require(!Objects[owner_a].ctype.ai_info.CLOAKED && streams() == at_end, "next exact interval remains uncloaked");
		++GameTime64;
		xl_boss_ai_frame(owner_a, 1);
		require(Objects[owner_a].ctype.ai_info.CLOAKED == 1 && streams()[2] == at_end[2] + 1, "next eligible frame begins a fresh cloak");
		teleport_trace = false;
		++cases;
		start();
		initial = streams();
		xl_boss_teleport(owner_a);
		pose();
		auto direct = streams();
		require(direct[0] == 1495354192u && direct[2] == initial[2] + 2 && direct[1] == initial[1] && direct[3] == initial[3], "direct seeded teleport consumes two SIM and zero FX draws");
		teleport_trace = false;
		++cases;
		for (int age_offset : { 0, 1 }) {
			start();
			GameTime64 = 18 * F1_0;
			xl_boss_note_hit(owner_a);
			GameTime64 = 20 * F1_0 + age_offset;
			initial = streams();
			xl_boss_ai_frame(owner_a, 0);
			require(bool(Objects[owner_a].ctype.ai_info.CLOAKED) == (age_offset == 0) && streams()[2] == initial[2] + (age_offset == 0) && !teleport_relinks && teleport_outputs.empty(), "invisible hit-age exact-two/after-two boundary");
			teleport_trace = false;
			++cases;
		}
		for (bool final : { false, true }) {
			start();
			if (final) Current_level_num = 9;
			GameTime64 = (final ? 15 : 10) * F1_0;
			initial = streams();
			xl_boss_ai_frame(owner_a, 1);
			require(!Objects[owner_a].ctype.ai_info.CLOAKED && streams() == initial, "ordinary/final exact cloak interval");
			++GameTime64;
			xl_boss_ai_frame(owner_a, 1);
			require(Objects[owner_a].ctype.ai_info.CLOAKED == 1 && streams()[2] == initial[2] + 1, "ordinary/final cloak interval plus one");
			teleport_trace = false;
			++cases;
		}
		for (int boundary : { 0, 1, 2 }) {
			start();
			if (boundary < 2) restored_cloak(BOSS_CLOAK_DURATION, 0, 0);
			else restored_cloak(-1, BOSS_CLOAK_DURATION + 1, 0);
			if (boundary == 1) ++GameTime64;
			initial = streams();
			xl_boss_ai_frame(owner_a, 1);
			require(bool(Objects[owner_a].ctype.ai_info.CLOAKED) == (boundary == 0) && streams()[2] == initial[2] + (boundary == 0) && !teleport_relinks, "cloak exact-end/after-end/before-start boundaries");
			teleport_trace = false;
			++cases;
		}
		for (bool upper : { false, true })
			for (int offset : { 0, 1 }) {
				start();
				const fix third = BOSS_CLOAK_DURATION / 3;
				const fix elapsed = upper ? BOSS_CLOAK_DURATION - third - offset : third + offset;
				restored_cloak(elapsed, BOSS_CLOAK_DURATION - elapsed, 8 * F1_0);
				initial = streams();
				xl_boss_ai_frame(owner_a, 1);
				require(Objects[owner_a].ctype.ai_info.CLOAKED == 1 && teleport_relinks == offset && streams()[2] == initial[2] + (offset ? 3 : 1), "strict lower/upper teleport middle-window boundaries");
				if (offset) pose();
				teleport_trace = false;
				++cases;
			}
		for (bool final : { false, true })
			for (int offset : { 0, 1 }) {
				start();
				if (final) Current_level_num = 9;
				restored_cloak(3 * F1_0, 4 * F1_0, (final ? 10 : 7) * F1_0 + offset);
				initial = streams();
				xl_boss_ai_frame(owner_a, 1);
				require(teleport_relinks == offset && streams()[2] == initial[2] + (offset ? 3 : 1), "ordinary/final exact teleport interval and plus-one");
				if (offset) pose();
				teleport_trace = false;
				++cases;
			}
		for (bool direct_teleport : { false, true })
			for (int health : { 0, 3 * F1_0, 3 * F1_0 + 1 }) {
				start();
				Objects[owner_a].shields = health;
				initial = streams();
				if (direct_teleport) xl_boss_teleport(owner_a);
				else xl_boss_ai_frame(owner_a, 1);
				const bool admitted = health > 3 * F1_0;
				require(teleport_relinks == (direct_teleport && admitted) && bool(Objects[owner_a].ctype.ai_info.CLOAKED) == (!direct_teleport && admitted) && streams()[2] == initial[2] + (admitted ? (direct_teleport ? 2 : 1) : 0), "pinned float one-percent health quirk at nominal3 and3-plus-one");
				if (direct_teleport && admitted) pose();
				teleport_trace = false;
				++cases;
			}
		start();
		auto routes = xl_compat::boss_routes();
		routes[0].segments = { 0 };
		require(xl_compat::restore_boss_routes(routes, 1), "single-room failure route setup");
		Objects[player_slot].pos = Objects[owner_a].pos = {};
		const auto before_boss = Objects[owner_a];
		const auto before_state = boss_state(owner_a);
		initial = streams();
		xl_boss_teleport(owner_a);
		const auto after_state = boss_state(owner_a);
		require(streams()[2] == initial[2] + 6 && streams()[1] == initial[1] && streams()[3] == initial[3] && teleport_centers == 5 && !teleport_relinks && teleport_outputs.empty() &&
		            !std::memcmp(&before_boss, &Objects[owner_a], sizeof(object)) && before_state.teleport == after_state.teleport && Ai_local_info[owner_a].next_fire == 15 * F1_0 && Ai_local_info[owner_a].next_fire2 == 17 * F1_0,
		        "actual destination predicate exhausts five candidates without pose/timer/audio changes");
		teleport_trace = false;
		++cases;
		for (bool multi : { false, true }) {
			start();
			const auto before_boss = Objects[owner_a];
			initial = streams();
			extensions = false;
			Game_mode = multi ? GM_MULTI : 0;
			xl_boss_ai_frame(owner_a, 1);
			xl_boss_teleport(owner_a);
			require(!std::memcmp(&before_boss, &Objects[owner_a], sizeof(object)) && streams() == initial && !teleport_relinks && teleport_outputs.empty(), "inactive/genuine multiplayer profile does not enter owned boss cycle");
			teleport_trace = false;
			++cases;
		}
	}
	for (int axis : { 0, 1, 2 }) {
		start();
		ConsoleObject->pos = { 100 * F1_0, axis == 2 ? 10 * F1_0 : 0, axis == 0 ? 10 * F1_0 : axis == 1 ? -10 * F1_0
			                                                                                            : 0 };
		initial = streams();
		xl_boss_teleport(owner_a);
		vms_matrix expected{};
		if (axis == 0) expected = vmd_identity_matrix;
		if (axis == 1) {
			expected.rvec = { -F1_0, 0, 0 };
			expected.uvec = { 0, F1_0, 0 };
			expected.fvec = { 0, 0, -F1_0 };
		}
		if (axis == 2) {
			expected.rvec = { F1_0, 0, 0 };
			expected.uvec = { 0, 0, -F1_0 };
			expected.fvec = { 0, F1_0, 0 };
		}
		require(!std::memcmp(&Objects[owner_a].orient, &expected, sizeof(expected)) && teleport_relinks == 1 && teleport_centers == 1 && teleport_outputs == std::vector<int>{ 1, 2, 3, 4 } &&
		            streams()[2] == initial[2] + 2 && !Ai_local_info[owner_a].next_fire && !Ai_local_info[owner_a].next_fire2,
		        "exact axial and pinned vertical-special-case bases stay unchanged");
		teleport_trace = false;
		++cases;
	}
	std::cout << "Actual boss cloak/teleport: " << cases << " cycle,source-matrix,clock,health,visibility,RNG and failed-destination cases passed\n";
}

void gauss_combat_controls()
{
	auto streams = [] { unsigned sim=0,fx=0;d_rand_get_stream_state(D_RNG_SIM,&sim);d_rand_get_stream_state(D_RNG_FX,&fx);return std::array<unsigned,4>{sim,fx,d_rand_get_stream_call_count(D_RNG_SIM),d_rand_get_stream_call_count(D_RNG_FX)}; };
	auto start = [&](int mode, int skip, fix frame, int boss_role = 0) {
		Difficulty_level = 2;
		reset();
		GameTime64 = 20 * F1_0;
		xl_object_birth_clear();
		xl_projectile_clear();
		Buddy_objnum = -1;
		gauss_trace = contact_trace = true;
		gauss_force_calls = gauss_rot_calls = gauss_static_calls = 0;
		contact_hud = contact_awareness = contact_ai_hits = contact_impact_records = contact_static = contact_resolutions = weak_spot_sound_calls = 0;
		extensions = mode == 0;
		Game_mode = mode == 2 ? GM_MULTI : 0;
		FrameTime = frame;
		Objects[owner_b].type = OBJ_NONE;
		auto &robot = Objects[owner_a];
		robot = {};
		robot.type = OBJ_ROBOT;
		robot.id = boss_role ? boss_type : child_type;
		robot.signature = 1403;
		robot.segnum = 0;
		robot.control_type = CT_AI;
		robot.movement_type = MT_PHYSICS;
		robot.mtype.phys_info.mass = F1_0;
		robot.mtype.phys_info.flags = PF_BOUNCE;
		robot.mtype.phys_info.rotthrust = { 111, -222, 333 };
		robot.ctype.ai_info.SKIP_AI_COUNT = skip;
		auto &info = Robot_info[robot.id];
		info.boss_flag = boss_role;
		info.companion = 0;
		info.energy_blobs = 0;
		info.exp1_vclip_num = info.exp1_sound_num = -1;
		if (!boss_role) info.strength = 100 * F1_0;
		robot.shields = boss_role ? boss_health_maximum_for_difficulty(info.strength, Difficulty_level) : 100 * F1_0;
		xl_boss_spawn_load();
		auto level = authored_level(true);
		level.object_trigger_records.resize(1);
		level.object_trigger_records[0].action.value = 0;
		xl_object_trigger_load(level);
		auto &weapon = Objects[4];
		weapon = {};
		weapon.type = OBJ_WEAPON;
		weapon.control_type = CT_WEAPON;
		weapon.movement_type = MT_PHYSICS;
		weapon.render_type = RT_NONE;
		weapon.id = GAUSS_ID;
		weapon.signature = 1404;
		weapon.segnum = 0;
		weapon.shields = 8 * F1_0;
		weapon.mtype.phys_info.mass = F1_0;
		weapon.lifeleft = 10 * F1_0;
		weapon.ctype.laser_info.parent_type = OBJ_PLAYER;
		weapon.ctype.laser_info.parent_num = player_slot;
		weapon.ctype.laser_info.parent_signature = Objects[player_slot].signature;
		weapon.ctype.laser_info.multiplier = F1_0;
		weapon.ctype.laser_info.last_hitobj = -1;
		N_weapon_types = GAUSS_ID + 1;
		Weapon_info[GAUSS_ID] = {};
		Weapon_info[GAUSS_ID].matter = 1;
		Weapon_info[GAUSS_ID].robot_hit_vclip = -1;
		ConsoleObject = &Objects[player_slot];
		d_srand(2);
	};
	auto invoke = [&](fix expected_damage) {
		auto &robot = Objects[owner_a];
		auto &weapon = Objects[4];
		require(!Robot_info[robot.id].energy_blobs && !Weapon_info[weapon.id].damage_radius && Robot_info[robot.id].exp1_vclip_num == -1 && Robot_info[robot.id].exp1_sound_num == -1 && Weapon_info[weapon.id].robot_hit_vclip == -1, "Gauss scope excludes classification,children,splash and impact providers");
		vms_vector point = robot.pos;
		contact_point = &point;
		contact_before = robot.shields;
		contact_requested = expected_damage;
		collide_robot_and_weapon(&robot, &weapon, &point);
		contact_point = nullptr;
	};
	auto finish = [&] { gauss_trace = contact_trace = false; };
	struct Sample {
		fix frame;
		int skip;
		vms_vector xl, classic;
		bool draws;
	};
	const Sample samples[] = {
		{ 1024, 0, { 241, -100, 96 }, { -241, 100, -96 }, true },
		{ 1024, 63, { 15475, -6434, 6144 }, { -15476, 6433, -6145 }, true },
		{ 1024, 64, { 111, -222, 333 }, { 111, -222, 333 }, false },
		{ 1092, 0, { 257, -107, 102 }, { -257, 107, -102 }, true },
		{ 1092, 59, { 15471, -6432, 6142 }, { -15472, 6431, -6143 }, true },
		{ 1092, 60, { 15729, -6539, 6244 }, { -15730, 6538, -6245 }, true },
		{ 1092, 61, { 111, -222, 333 }, { 111, -222, 333 }, false },
	};
	int cases = 0;
	for (int mode : { 0, 1, 2 })
		for (const auto &sample : samples) {
			start(mode, sample.skip, sample.frame);
			const auto rng = streams();
			const auto before = Objects[owner_a];
			invoke(8 * F1_0);
			const auto after = streams();
			const auto thrust = mode == 0 ? sample.xl : sample.classic;
			require(Objects[owner_a].shields == 92 * F1_0 && Objects[owner_a].ctype.ai_info.SKIP_AI_COUNT == sample.skip + sample.draws &&
			            !std::memcmp(&Objects[owner_a].mtype.phys_info.rotthrust, &thrust, sizeof(thrust)) &&
			            Objects[owner_a].mtype.phys_info.flags == (before.mtype.phys_info.flags | (sample.draws ? PF_USES_THRUST : 0)) && (Objects[4].flags & OF_SHOULD_BE_DEAD),
			        "actual Gauss damage,strict preincrement stagger threshold,all torque axes and retirement");
			auto expected_physics = before.mtype.phys_info;
			expected_physics.rotthrust = thrust;
			expected_physics.flags |= sample.draws ? PF_USES_THRUST : 0;
			require(!std::memcmp(&expected_physics, &Objects[owner_a].mtype.phys_info, sizeof(expected_physics)), "only scoped torque/flag fields change at zero-physics boundary");
			require(after[2] == rng[2] + (sample.draws ? 3 : 0) && after[0] == (sample.draws ? 2818548041u : rng[0]) && after[1] == rng[1] && after[3] == rng[3] &&
			            contact_hud == 1 && contact_awareness == 1 && contact_ai_hits == 1 && contact_impact_records == 1 && contact_resolutions == 1 && contact_static == 1 && gauss_static_calls == 1 && gauss_force_calls == 1 && gauss_rot_calls == 1 &&
			            !births && !death_calls && !effect_calls && !sound_calls,
			        "real Gauss SIM-only draw budget,awareness and zero-physics boundaries");
			finish();
			++cases;
		}
	start(0, 0, 1024);
	auto rng = streams();
	invoke(8 * F1_0);
	Objects[4].flags = 0;
	++Objects[4].signature;
	invoke(8 * F1_0);
	const vms_vector second{ 108, -295, -334 };
	auto after = streams();
	require(Objects[owner_a].shields == 84 * F1_0 && Objects[owner_a].ctype.ai_info.SKIP_AI_COUNT == 2 && !std::memcmp(&second, &Objects[owner_a].mtype.phys_info.rotthrust, sizeof(second)) && after[0] == 1775750268u && after[2] == rng[2] + 6 &&
	            after[1] == rng[1] && after[3] == rng[3] && contact_resolutions == 2 && gauss_force_calls == 2 && gauss_rot_calls == 2,
	        "fresh repeated Gauss hits retain damage and consume next three real draws");
	finish();
	++cases;
	start(0, 63, 1024);
	invoke(8 * F1_0);
	const auto saturated = Objects[owner_a].mtype.phys_info;
	const auto saturated_rng = streams();
	Objects[4].flags = 0;
	++Objects[4].signature;
	invoke(8 * F1_0);
	require(Objects[owner_a].shields == 84 * F1_0 && Objects[owner_a].ctype.ai_info.SKIP_AI_COUNT == 64 && !std::memcmp(&saturated, &Objects[owner_a].mtype.phys_info, sizeof(saturated)) && streams() == saturated_rng, "at strict threshold fresh hit still damages but does not restagger/draw");
	finish();
	++cases;
	start(0, 0, 1024);
	Objects[4].flags |= OF_HARMLESS;
	rng = streams();
	invoke(0);
	after = streams();
	const vms_vector first{ 241, -100, 96 };
	require(Objects[owner_a].shields == 100 * F1_0 && Objects[owner_a].ctype.ai_info.SKIP_AI_COUNT == 1 && !std::memcmp(&first, &Objects[owner_a].mtype.phys_info.rotthrust, sizeof(first)) && (Objects[4].flags & OF_SHOULD_BE_DEAD) &&
	            !contact_resolutions && !contact_static && !gauss_force_calls && !gauss_rot_calls && gauss_static_calls == 1 && streams()[2] == rng[2] + 3,
	        "harmless Gauss skips damage/bump but still staggers and retires");
	finish();
	++cases;
	start(0, 0, 1024);
	Objects[4].mtype.phys_info.flags |= PF_PERSISTENT;
	Objects[4].shields = 64 * F1_0;
	Objects[4].ctype.laser_info.multiplier = F1_0 / 8;
	rng = streams();
	invoke(8 * F1_0);
	require(Objects[4].shields == 41 * F1_0 && !(Objects[4].flags & OF_SHOULD_BE_DEAD) && xl_projectile_has_hit(4, owner_a), "persistent Gauss records hit and drains real target shields");
	after = streams();
	xl_projectile_state history{};
	require(Objects[owner_a].shields == 92 * F1_0 && Objects[owner_a].ctype.ai_info.SKIP_AI_COUNT == 1 && !std::memcmp(&first, &Objects[owner_a].mtype.phys_info.rotthrust, sizeof(first)) &&
	            after[0] == 2818548041u && after[2] == rng[2] + 3 && after[1] == rng[1] && after[3] == rng[3] && xl_projectile_state_get(4, &history) && history.owner == 4 && history.signature == 1404 && history.count == 1 && history.victims[0] == owner_a,
	        "first persistent Gauss hit actually damages,staggers and records exact history with real RNG");
	const auto persistent_robot = Objects[owner_a], persistent_weapon = Objects[4];
	const auto persistent_rng = streams();
	const auto notifications = contact_awareness;
	invoke(0);
	require(!std::memcmp(&persistent_robot, &Objects[owner_a], sizeof(object)) && !std::memcmp(&persistent_weapon, &Objects[4], sizeof(object)) && streams() == persistent_rng && contact_awareness == notifications && contact_resolutions == 1 && gauss_static_calls == 1,
	        "persistent repeat returns before further Gauss damage,spin,awareness and drain");
	finish();
	++cases;
	for (int control : { 0, 1, 2 }) {
		start(0, 0, 1024);
		if (control == 0) {
			Objects[4].id = 0;
			Weapon_info[0] = {};
			Weapon_info[0].robot_hit_vclip = -1;
		}
		if (control == 1) Robot_info[child_type].companion = 1;
		if (control == 2) {
			Objects[2] = {};
			Objects[2].type = OBJ_ROBOT;
			Objects[2].id = child_type;
			Objects[2].signature = 1402;
			Objects[4].ctype.laser_info.parent_type = OBJ_ROBOT;
			Objects[4].ctype.laser_info.parent_num = 2;
			Objects[4].ctype.laser_info.parent_signature = 1402;
		}
		rng = streams();
		const auto before = Objects[owner_a];
		invoke(control == 0 ? 8 * F1_0 : 0);
		require(Objects[owner_a].shields == (control == 0 ? 92 * F1_0 : 100 * F1_0) && Objects[owner_a].ctype.ai_info.SKIP_AI_COUNT == 0 && !std::memcmp(&before.mtype.phys_info, &Objects[owner_a].mtype.phys_info, sizeof(before.mtype.phys_info)) &&
		            streams() == rng && !gauss_static_calls && bool(Objects[4].flags & OF_SHOULD_BE_DEAD) == (control == 0) && contact_awareness == (control == 0),
		        "non-Gauss does not stagger;companion/robot-parent guards return before acceptance");
		finish();
		++cases;
	}
	for (int difficulty = 0; difficulty < NDL; ++difficulty) {
		start(0, 0, 1024, 21);
		Difficulty_level = difficulty;
		Objects[owner_a].shields = boss_health_maximum_for_difficulty(strength, difficulty);
		Objects[4].shields = 10 * F1_0;
		Objects[4].ctype.laser_info.multiplier = 2 * F1_0;
		const auto before = Objects[owner_a];
		rng = streams();
		const fix damage = (20 - 2 * difficulty) * F1_0;
		invoke(damage);
		require(Objects[owner_a].shields == before.shields - damage && !Objects[owner_a].ctype.ai_info.SKIP_AI_COUNT && !std::memcmp(&before.mtype.phys_info, &Objects[owner_a].mtype.phys_info, sizeof(before.mtype.phys_info)) && streams() == rng &&
		            !gauss_static_calls && !gauss_force_calls && !gauss_rot_calls && contact_static == 1 && (Objects[4].flags & OF_SHOULD_BE_DEAD) && !births,
		        "actual boss Gauss attenuation20/18/16/14/12 without ordinary stagger or RNG");
		finish();
		++cases;
	}
	start(0, 0, 1024, 25);
	rng = streams();
	const auto immune = Objects[owner_a];
	invoke(0);
	require(Objects[owner_a].shields == immune.shields && !Objects[owner_a].ctype.ai_info.SKIP_AI_COUNT && weak_spot_sound_calls == 1 && (Objects[4].flags & OF_SHOULD_BE_DEAD) && streams() == rng && !births && !gauss_static_calls, "profile25 matter immunity excludes Gauss damage/stagger while retiring shot");
	finish();
	++cases;
	std::cout << "Actual Gauss combat: " << cases << " damage,torque,skip-boundary,history,profile and RNG cases passed\n";
}

void forcefield_controls(int selector = 0)
{
	auto streams = [] { unsigned sim=0,fx=0;d_rand_get_stream_state(D_RNG_SIM,&sim);d_rand_get_stream_state(D_RNG_FX,&fx);return std::array<unsigned,4>{sim,fx,d_rand_get_stream_call_count(D_RNG_SIM),d_rand_get_stream_call_count(D_RNG_FX)}; };
	int cases = 0;
	for (int mode : { 0, 1, 2, 3, 4 })
		for (int control = 0; control <= 8; ++control) {
			if (mode && control) continue;
			if (selector == 1 && (mode || control)) continue;
			if (selector == 2 && !mode) continue;
			Difficulty_level = 2;
			reset();
			GameTime64 = 20 * F1_0;
			Endlevel_sequence = Player_is_dead = 0;
			forcefield_trace = true;
			forcefield_protected = control == 1;
			forcefield_observer = control == 6;
			forcefield_rotations = forcefield_sounds = forcefield_awareness = forcefield_damage_records = forcefield_trace_records = forcefield_warning_queries = forcefield_protection_queries = 0;
			forcefield_events.clear();
			forcefield_palettes.clear();
			forcefield_force = {};
			extensions = mode != 1 && mode != 4;
			Game_mode = (mode == 3 || mode == 4) ? GM_MULTI : 0;
			if (mode == 2) fixture_mission.descent_version = 1;
			auto &player = Objects[player_slot];
			player.type = OBJ_PLAYER;
			player.id = Player_num;
			player.control_type = CT_FLYING;
			player.movement_type = MT_PHYSICS;
			player.mtype.phys_info.mass = F1_0;
			player.segnum = 0;
			Players[Player_num].shields = player.shields = control == 8 ? 5 * F1_0 : 100 * F1_0;
			forcefield_before = player.shields;
			if (control == 2) Players[Player_num].flags |= PLAYER_FLAGS_INVULNERABLE;
			ConsoleObject = &player;
			Objects[2] = player;
			Objects[2].id = 1;
			Objects[2].signature = 1502;
			Segments[0].sides[0].tmap_num = 432;
			Segments[0].sides[0].tmap_num2 = 0;
			TmapInfo[432] = {};
			TmapInfo[432].flags = TMI_FORCE_FIELD;
			TmapInfo[432].damage = control == 7 ? 1 : 0;
			reset_shield_warning_damage();
			d_srand(2);
			const auto rng = streams();
			fix speed = 64 * F1_0;
			if (control == 3) speed = 0;
			if (control == 4) speed = F1_0;
			auto *actor = control == 5 ? &Objects[2] : &player;
			vms_vector point = actor->pos;
			forcefield_point = &point;
			collide_player_and_wall(actor, speed, 0, 0, &point);
			forcefield_point = nullptr;
			const bool effects = control != 5 && control != 6 && control != 7;
			const bool awareness = effects && control != 1 && control != 3 && control != 4;
			const bool damaged = awareness && control != 2;
			const fix damage = damaged ? 9 * F1_0 / 2 : 0;
			const auto after = streams();
			require(Players[Player_num].shields == forcefield_before - damage && player.shields == forcefield_before - damage && !Player_is_dead && !(player.flags & OF_SHOULD_BE_DEAD), "real forcefield surviving shield loss,low-shield exception and object mirror");
			require(forcefield_rotations == effects && forcefield_sounds == effects && forcefield_awareness == awareness && forcefield_trace_records == damaged && forcefield_damage_records == damaged && forcefield_warning_queries == damaged &&
			            total_ouch == damage && last_damage_time == (damaged ? GameTime64 : 0),
			        "actual effects,awareness,damage and warning accumulator guards");
			require(after[0] == (effects ? 2818548041u : rng[0]) && after[2] == rng[2] + (effects ? 3 : 0) && after[1] == rng[1] && after[3] == rng[3], "forcefield exact three SIM draws and unchanged FX stream");
			std::vector<int> expected;
			if (effects) expected = { 1, 2, 3 };
			if (awareness) {
				expected.push_back(4);
				expected.push_back(5);
			}
			if (damaged)
				for (int event : { 6, 7, 8, 9 }) expected.push_back(event);
			require(forcefield_events == expected, "actual forcefield output order retains protection after blue/torque/sound and before awareness/damage");
			std::vector<std::array<int, 3>> palettes;
			if (effects) palettes.push_back({ 0, 0, 60 });
			if (damaged) palettes.push_back({ 16, -2, -2 });
			require(forcefield_palettes == palettes && forcefield_protection_queries == (effects ? (damaged ? 2 : 1) : 0), "exact forcefield blue/red palette requests and protection query phase");
			if (effects) {
				const vms_vector wanted = mode == 0 ? vms_vector{ 619000, -257360, 245760 } : vms_vector{ -619040, 257320, -245800 };
				if (std::memcmp(&wanted, &forcefield_force, sizeof(wanted))) std::cerr << "actual forcefield torque=" << forcefield_force.x << ',' << forcefield_force.y << ',' << forcefield_force.z << " shields=" << player.shields << " events=" << forcefield_events.size() << '\n';
				require(!std::memcmp(&wanted, &forcefield_force, sizeof(wanted)), "source signed forcefield draw direction and offset");
			}
			forcefield_trace = false;
			++cases;
		}
	std::cout << "Actual player forcefield: " << cases << " torque,damage,scope,early-return,output-order and RNG cases passed\n";
}

void master_forcefield_controls()
{
	auto streams = [] {unsigned sim=0,fx=0;d_rand_get_stream_state(D_RNG_SIM,&sim);d_rand_get_stream_state(D_RNG_FX,&fx);return std::array<unsigned,4>{sim,fx,d_rand_get_stream_call_count(D_RNG_SIM),d_rand_get_stream_call_count(D_RNG_FX)}; };
	int cases = 0;
	for (int mode = 0; mode < 7; ++mode) {
		Difficulty_level = 2;
		reset();
		GameTime64 = 20 * F1_0;
		xl_wall_trigger_reset();
		master_forcefield_trace = true;
		master_forcefield_centers = master_forcefield_connections = 0;
		master_forcefield_events.clear();
		ConsoleObject = &Objects[player_slot];
		ConsoleObject->type = OBJ_PLAYER;
		ConsoleObject->id = Player_num;
		Objects[4] = {};
		Objects[4].type = 20;
		Objects[4].id = 2;
		Objects[4].render_type = 13;
		Segments[0].children[4] = 1;
		Segments[1].children[5] = 0;
		Segments[0].sides[4].wall_num = 0;
		Segments[1].sides[5].wall_num = 1;
		Segments[0].sides[4].tmap_num = Segments[1].sides[5].tmap_num = 432;
		TmapInfo[432] = {};
		TmapInfo[432].flags = TMI_FORCE_FIELD;
		Num_walls = 4;
		for (int i = 0; i < 4; ++i) {
			Walls[i] = {};
			Walls[i].linked_wall = -1;
			Walls[i].trigger = 255;
			Walls[i].clip_num = -1;
		}
		for (int i = 0; i < 2; ++i) {
			Walls[i].segnum = i;
			Walls[i].sidenum = i ? 5 : 4;
			Walls[i].type = WALL_CLOSED;
			Walls[i].flags = 16;
			Walls[i].state = 0;
			Walls[i].keys = 1;
		}
		if (mode == 1) Walls[0].type = Walls[1].type = WALL_OPEN;
		if (mode == 2) Walls[0].type = WALL_OPEN;
		for (int i = 2; i < 4; ++i) {
			Walls[i].segnum = 0;
			Walls[i].sidenum = i;
			Walls[i].trigger = i - 1;
			Segments[0].sides[i].wall_num = i;
		}
		Num_triggers = 3;
		for (int i = 0; i < 3; ++i) Triggers[i] = {};
		auto &root = Triggers[0];
		root.type = 31;
		root.flags = TF_ONE_SHOT;
		root.time = -1;
		root.num_links = 2;
		root.seg[0] = root.seg[1] = 0;
		root.side[0] = 2;
		root.side[1] = 3;
		auto &ambient = Triggers[1];
		ambient.type = 33;
		ambient.value = 5 * F1_0;
		ambient.time = -1;
		ambient.num_links = 1;
		ambient.seg[0] = 4;
		ambient.side[0] = -1;
		auto &opening = Triggers[2];
		opening.type = TT_OPEN_WALL;
		opening.value = 5 * F1_0;
		opening.time = -1;
		opening.num_links = 2;
		opening.seg[0] = mode == 3 ? 1 : 0;
		opening.side[0] = mode == 3 ? 5 : 4;
		opening.seg[1] = mode == 3 ? 0 : 1;
		opening.side[1] = mode == 3 ? 4 : 5;
		if (mode == 4 || mode == 6) opening.flags = 64;
		if (mode == 5) {
			opening.num_links = 3;
			opening.seg[0] = 0;
			opening.side[0] = 1;
			opening.seg[1] = 0;
			opening.side[1] = 4;
			opening.seg[2] = 1;
			opening.side[2] = 5;
		}
		if (mode == 6) extensions = false;
		master_forcefield_initial = { Walls[0].type, Walls[1].type };
		const std::array<wall, 4> initial = { Walls[0], Walls[1], Walls[2], Walls[3] };
		const auto actor = Objects[player_slot];
		const auto effect = Objects[4];
		const auto controls = Controls;
		const auto rng = streams();
		auto expected_triggers = std::array<trigger, 3>{ Triggers[0], Triggers[1], Triggers[2] };
		if (mode != 6) expected_triggers[0].flags |= TF_DISABLED;
		short d1_flags = 123;
		require(!d1_in_d2_trigger_source_flags(&opening, &d1_flags) && d1_flags == 123, "real source-tag decision rejects ordinary XL child as D1 record");
		require(check_trigger_sub(mode == 6 ? 2 : 0, Player_num, 0) == 0, "actual master or inactive stock entry opens forcefield pair");
		auto expected_walls = initial;
		expected_walls[0].type = expected_walls[1].type = WALL_OPEN;
		require(!std::memcmp(expected_walls.data(), Walls, sizeof(expected_walls)) && !std::memcmp(&actor, &Objects[player_slot], sizeof(actor)) && !std::memcmp(&effect, &Objects[4], sizeof(effect)) && !std::memcmp(&controls, &Controls, sizeof(Controls)) && streams() == rng, "real stock wall writes change only both wall types while actors controls and RNG remain unchanged");
		require(Triggers[0].flags == (mode == 6 ? TF_ONE_SHOT : TF_ONE_SHOT | TF_DISABLED) && Triggers[1].flags == 0 && Triggers[2].flags == ((mode == 4 || mode == 6) ? 64 : 0), "parent one-shot consumed once while children retain source flags");
		require(!std::memcmp(expected_triggers.data(), Triggers, sizeof(expected_triggers)), "all complete trigger records preserved except expected parent disabled bit");
		std::vector<std::array<int, 4>> events;
		if (mode != 6) events.push_back({ 1, 4, initial[0].type, initial[1].type });
		const int source = mode == 3 ? 1 : 0, paired = 1 - source;
		if (mode != 1) {
			if (mode != 4) events.push_back({ 2, source, initial[0].type, initial[1].type });
			auto midway = master_forcefield_initial;
			midway[source] = WALL_OPEN;
			events.push_back({ 3, source, midway[0], midway[1] });
			events.push_back({ 3, paired, WALL_OPEN, WALL_OPEN });
			events.push_back({ 4, source, WALL_OPEN, WALL_OPEN });
			events.push_back({ 4, paired, WALL_OPEN, WALL_OPEN });
		}
		require(master_forcefield_events == events && master_forcefield_centers == (mode == 1 ? 0 : 1) && master_forcefield_connections == 2, "ambient before cue then ordered source-write/hum paired-write/hum and cleanup once despite reverse duplicate target");
		if (mode == 0) {
			GameTime64 += F1_0;
			const auto records = std::array<trigger, 3>{ Triggers[0], Triggers[1], Triggers[2] };
			const int centers = master_forcefield_centers, connections = master_forcefield_connections;
			require(check_trigger_sub(0, Player_num, 0) == 1 && master_forcefield_events == events && !std::memcmp(expected_walls.data(), Walls, sizeof(expected_walls)) && !std::memcmp(records.data(), Triggers, sizeof(records)) && !std::memcmp(&actor, &Objects[player_slot], sizeof(actor)) && !std::memcmp(&effect, &Objects[4], sizeof(effect)) && master_forcefield_centers == centers && master_forcefield_connections == connections && !std::memcmp(&controls, &Controls, sizeof(Controls)) && streams() == rng, "consumed master repeat is inert for walls actions outputs actor and streams");
		}
		master_forcefield_trace = false;
		++cases;
	}
	Num_walls = 0;
	std::cout << "Actual master stock forcefield opening: " << cases << " pair-state,ordered-output,cue,missing-wall and stock cases passed\n";
}

void master_chain_controls()
{
	auto streams = [] {unsigned sim=0,fx=0;d_rand_get_stream_state(D_RNG_SIM,&sim);d_rand_get_stream_state(D_RNG_FX,&fx);return std::array<unsigned,4>{sim,fx,d_rand_get_stream_call_count(D_RNG_SIM),d_rand_get_stream_call_count(D_RNG_FX)}; };
	auto setup = [&](int mode = 0) {
		Difficulty_level = 2;
		reset();
		GameTime64 = 20 * F1_0;
		Player_is_dead = 0;
		xl_wall_trigger_reset();
		xl_seismic_clear();
		master_trace = true;
		master_reverse = mode == 1;
		master_named = 0;
		master_results.clear();
		ConsoleObject = &Objects[player_slot];
		ConsoleObject->type = OBJ_PLAYER;
		ConsoleObject->id = Player_num;
		Objects[2] = *ConsoleObject;
		Objects[2].id = 1;
		Objects[2].signature = 2002;
		Players[1] = {};
		Players[1].objnum = 2;
		Num_triggers = 3;
		for (int i = 0; i < 3; ++i) {
			Triggers[i] = {};
			Triggers[i].flags = 3;
		}
		auto &root = Triggers[0];
		root.type = 31;
		root.time = 0;
		root.value = 0;
		root.num_links = 2;
		root.seg[0] = root.seg[1] = 0;
		root.side[0] = mode == 1 ? 2 : 1;
		root.side[1] = mode == 1 ? 1 : 2;
		Triggers[1].type = 30;
		Triggers[1].value = 2 * F1_0;
		Triggers[1].time = 1;
		Triggers[2].type = 36;
		Triggers[2].value = 3 * F1_0;
		Triggers[2].time = 3;
		Num_walls = 2;
		for (int i = 0; i < 2; ++i) {
			Walls[i] = {};
			Walls[i].segnum = 0;
			Walls[i].sidenum = i + 1;
			Walls[i].trigger = i + 1;
			Segments[0].sides[i + 1].wall_num = i;
		}
		if (mode == 2) Triggers[1].flags |= TF_DISABLED;
		if (mode == 3) Segments[0].sides[1].wall_num = -1;
		if (mode == 4) root.seg[0] = Num_segments;
	};
	auto armed = [] {const auto state=xl_compat::seismic_gameplay_snapshot();require(state[0]==3*F1_0&&state[1]==3*F1_0&&state[2].is_null(),"real master seismic child arms strength and pending duration without a started interval"); };
	auto trigger_records = [] { return std::array<trigger, 3>{ Triggers[0], Triggers[1], Triggers[2] }; };
	int cases = 0;
	for (int mode = 0; mode < 5; ++mode) {
		setup(mode);
		const auto actor = Objects[player_slot];
		const auto controls = Controls;
		const auto rng = streams();
		const auto records = trigger_records();
		const auto seismic_before = xl_compat::seismic_gameplay_snapshot();
		int result = check_trigger_sub(0, Player_num, 0);
		require(result == (mode == 4 ? 1 : 0) && !std::memcmp(&actor, &Objects[player_slot], sizeof(actor)) && !std::memcmp(&controls, &Controls, sizeof(Controls)) && streams() == rng, "actual stock bridge master chain preserves actor controls and both RNG streams");
		if (mode == 4) {
			const auto state = xl_compat::seismic_gameplay_snapshot();
			require(!master_named && state == seismic_before && !std::memcmp(records.data(), Triggers, 3 * sizeof(trigger)) && master_results == std::vector<std::array<int, 3>>{ { 0, 0, 1 } }, "out-of-range target is atomic admission rejection not raw unchecked skip");
		} else {
			armed();
			require(master_named == (mode < 2) && Triggers[0].flags == 7 && Triggers[2].flags == 7 && Triggers[1].flags == (mode == 3 ? 3 : 7), "master one-shot consumption and disabled/missing first target still allow second child");
			std::vector<std::array<int, 3>> expected;
			if (mode == 0) expected = { { 1, 0, 0 }, { 1, 0, 0 }, { 2, 0, 0 }, { 2, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
			if (mode == 1) expected = { { 2, 0, 0 }, { 2, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
			if (mode == 2) expected = { { 1, 0, 1 }, { 2, 0, 0 }, { 2, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
			if (mode == 3) expected = { { 2, 0, 0 }, { 2, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
			require(master_results == expected, "exact runtime plus stock bridge diagnostic result sequence, not duplicate child gameplay");
			if (mode == 0) {
				xl_seismic_clear();
				GameTime64 += F1_0;
				const auto cleared = xl_compat::seismic_gameplay_snapshot();
				const auto consumed = trigger_records();
				expected.push_back({ 0, 0, 1 });
				require(check_trigger_sub(0, Player_num, 0) == 1 && master_named == 1 && xl_compat::seismic_gameplay_snapshot() == cleared && master_results == expected && !std::memcmp(consumed.data(), Triggers, 3 * sizeof(trigger)) && !std::memcmp(&actor, &Objects[player_slot], sizeof(actor)) && !std::memcmp(&controls, &Controls, sizeof(Controls)) && streams() == rng, "consumed master repeat adds only rejected diagnostic without replaying effects or changing state/RNG");
			}
		}
		master_trace = false;
		++cases;
	}
	for (int guard = 0; guard < 8; ++guard) {
		setup();
		int index = 0, pnum = Player_num;
		if (guard == 0) pnum = 1;
		if (guard == 1) {
			Game_mode = GM_MULTI;
			Players[Player_num].connected = CONNECT_PLAYING;
		}
		if (guard == 2) {
			Game_mode = GM_MULTI;
			Players[Player_num].connected = 0;
		}
		if (guard == 3) index = -1;
		if (guard == 4) index = 3;
		if (guard == 5) pnum = -1;
		if (guard == 6) pnum = MAX_PLAYERS;
		if (guard == 7) Triggers[0].flags |= TF_DISABLED;
		const auto before = Objects[player_slot], remote = Objects[2];
		const auto controls = Controls;
		const auto rng = streams();
		const auto records = trigger_records();
		const auto seismic_before = xl_compat::seismic_gameplay_snapshot();
		require(check_trigger_sub(index, pnum, 0) == 1 && !master_named && !std::memcmp(&before, &Objects[player_slot], sizeof(before)) && !std::memcmp(&remote, &Objects[2], sizeof(remote)) && !std::memcmp(&controls, &Controls, sizeof(Controls)) && streams() == rng && !std::memcmp(records.data(), Triggers, 3 * sizeof(trigger)) && xl_compat::seismic_gameplay_snapshot() == seismic_before, "real master stock-entry nonlocal multiplayer connected/disconnected index/player and disabled guards");
		const bool bridge = guard == 0 || guard == 1 || guard == 7;
		require(master_results == (bridge ? std::vector<std::array<int, 3>>{ { 0, 0, 1 } } : std::vector<std::array<int, 3>>{}), "stock prevalidation versus runtime bridge guard diagnostic difference");
		master_trace = false;
		++cases;
	}
	xl_seismic_clear();
	Num_walls = 0;
	std::cout << "Actual master stock bridge chain: " << cases << " ordered children,one-shot,diagnostic,missing-target and guard cases passed\n";
}

void boost_contact_controls(bool motion = false)
{
	auto streams = [] {unsigned sim=0,fx=0;d_rand_get_stream_state(D_RNG_SIM,&sim);d_rand_get_stream_state(D_RNG_FX,&fx);return std::array<unsigned,4>{sim,fx,d_rand_get_stream_call_count(D_RNG_SIM),d_rand_get_stream_call_count(D_RNG_FX)}; };
	auto setup = [&](bool adjacent = false) {
		Difficulty_level = 2;
		reset();
		GameTime64 = 20 * F1_0;
		Player_is_dead = Endlevel_sequence = 0;
		xl_wall_trigger_reset();
		boost_trace = true;
		Automap_active = 0;
		Num_segments = 2;
		Highest_segment_index = 1;
		Highest_vertex_index = 15;
		const int offsets[8][3] = { { -1, 1, -1 }, { 1, 1, -1 }, { 1, -1, -1 }, { -1, -1, -1 }, { -1, 1, 1 }, { 1, 1, 1 }, { 1, -1, 1 }, { -1, -1, 1 } };
		for (int room = 0; room < 2; ++room) {
			Segments[room] = {};
			Segments[room].objects = -1;
			for (int side = 0; side < 6; ++side) {
				Segments[room].children[side] = -1;
				Segments[room].sides[side].wall_num = -1;
			}
			for (int i = 0; i < 8; ++i) {
				int index = room * 8 + i;
				Segments[room].verts[i] = index;
				Vertices[index] = { room * 100 * F1_0 + offsets[i][0] * 10 * F1_0, offsets[i][1] * 10 * F1_0, offsets[i][2] * 10 * F1_0 };
			}
		}
		ConsoleObject = &Objects[player_slot];
		auto &a = *ConsoleObject;
		a.type = OBJ_PLAYER;
		a.id = Player_num;
		a.segnum = 0;
		a.pos = {};
		a.orient = { { F1_0, 0, 0 }, { 0, F1_0, 0 }, { 0, 0, F1_0 } };
		a.movement_type = MT_PHYSICS;
		a.control_type = CT_FLYING;
		a.mtype.phys_info.mass = 2 * F1_0;
		a.shields = Players[Player_num].shields = 100 * F1_0;
		Objects[2] = a;
		Objects[2].id = 1;
		Objects[2].signature = 1902;
		Num_walls = 1;
		Walls[0] = {};
		Walls[0].segnum = 0;
		Walls[0].sidenum = 4;
		Walls[0].trigger = 0;
		Segments[0].sides[4].wall_num = 0;
		Num_triggers = 1;
		Triggers[0] = {};
		auto &t = Triggers[0];
		t.type = 15;
		t.value = 1;
		t.time = -1;
		t.num_links = 1;
		t.seg[0] = 1;
		t.side[0] = adjacent ? 5 : 4;
		if (adjacent) {
			Segments[0].children[4] = 1;
			Segments[1].children[5] = 0;
		}
		Controls = {};
		Controls.vertical_thrust_time = 11;
		Controls.forward_thrust_time = 22;
		Controls.sideways_thrust_time = 33;
	};
	auto dispatch = [] { return xl_wall_trigger(0, player_slot, 0); };
	auto contact = [&] {
		matcen_trace = true;
		matcen_actor = ConsoleObject;
		matcen_axis = matcen_protected = false;
		matcen_exits = 1;
		matcen_sounds = matcen_explosions = matcen_damage_records = matcen_damage_traces = matcen_palettes = matcen_warnings = matcen_force_calls = 0;
		matcen_center_sides.clear();
		matcen_events.clear();
		for (int side = 0; side < 6; ++side) {
			Segments[0].children[side] = -1;
			Segments[0].sides[side].wall_num = -1;
		}
		Segments[0].children[0] = 1;
		matcen_before = ConsoleObject->shields;
		reset_shield_warning_damage();
		d_srand(2);
		auto before = streams();
		collide_player_and_materialization_center(ConsoleObject);
		auto after = streams();
		require(ConsoleObject->shields == matcen_before - 4 * F1_0 && Players[Player_num].shields == ConsoleObject->shields && matcen_sounds == 1 && matcen_explosions == 1 && matcen_damage_records == 1 && matcen_damage_traces == 1 && matcen_palettes == 1 && matcen_warnings == 1 && !matcen_force_calls, "real boosted matcen surviving damage and exact output boundaries");
		require(matcen_center_sides == std::vector<int>{ 0 } && matcen_events == std::vector<int>({ 1, 2, 3, 5, 6, 7, 8 }) && after[0] == 2993822286u && after[2] == before[2] + 4 && after[1] == before[1] && after[3] == before[3], "actual contact output order and four SIM draws with complete FX preservation");
		matcen_trace = false;
	};
	if (motion) {
		int cases = 0;
		auto registry = [] {xl_speed_boost_state state{};require(xl_speed_boost_state_count()==1&&xl_speed_boost_state_get(0,&state),"natural boost registry row exists");return state; };
		struct Row {
			int time;
			vms_vector thrust, result;
		};
		const Row rows[] = {
			{ 0, { 40 * F1_0, 80 * F1_0, -120 * F1_0 }, { 6553600, 0, 0 } },
			{ 512, { 40 * F1_0, 80 * F1_0, -120 * F1_0 }, { 5406720, 983040, -1474560 } },
			{ 1024, { 40 * F1_0, 80 * F1_0, -120 * F1_0 }, { 3932160, 1310720, -1966080 } },
			{ 1536, { 40 * F1_0, 80 * F1_0, -120 * F1_0 }, { 3440640, 1966080, -2949120 } },
			{ 2048, { 40 * F1_0, 80 * F1_0, -120 * F1_0 }, { 2621440, 1966080, -2949120 } },
			{ 3072, { 40 * F1_0, 80 * F1_0, -120 * F1_0 }, { 2621440, 2293760, -3440640 } },
			{ 1024, { 600 * F1_0, 800 * F1_0, -1000 * F1_0 }, { 10485760, 3932160, -3932160 } },
			{ 1024, { -600 * F1_0, -800 * F1_0, 1000 * F1_0 }, { 2621440, -3932160, 3932160 } }
		};
		for (const auto &row : rows) {
			setup();
			require(dispatch() == 0, "fresh natural boost action for literal drag row");
			auto &actor = Objects[player_slot];
			actor.mtype.phys_info.flags |= PF_USES_THRUST;
			actor.mtype.phys_info.drag = F1_0 / 2;
			actor.mtype.phys_info.thrust = row.thrust;
			const auto before = actor;
			const auto state = registry();
			const auto controls = Controls;
			const auto rng = streams();
			require(xl_speed_boost_begin(player_slot) == 1 && xl_speed_boost_drag(player_slot, row.time) == 1, "actual begin and accepted boosted drag hooks");
			auto expected = before;
			expected.mtype.phys_info.velocity = row.result;
			const auto after = registry();
			require(!std::memcmp(&expected, &actor, sizeof(actor)) && !std::memcmp(&state, &after, sizeof(state)) && !std::memcmp(&controls, &Controls, sizeof(Controls)) && streams() == rng, "literal full/fractional drag and clamp outputs with entire actor registry Controls and RNG preserved");
			++cases;
		}
		for (bool adjacent : { false, true }) {
			setup(adjacent);
			dispatch();
			auto &actor = Objects[player_slot];
			actor.mtype.phys_info.flags |= PF_USES_THRUST;
			actor.mtype.phys_info.drag = F1_0 / 2;
			actor.mtype.phys_info.thrust = {};
			actor.mtype.phys_info.velocity = { 7 * F1_0, -8 * F1_0, 9 * F1_0 };
			const auto before = actor;
			const auto state = registry();
			const auto controls = Controls;
			const auto rng = streams();
			require(xl_speed_boost_begin(player_slot) == (adjacent ? -1 : 1) && xl_speed_boost_drag(player_slot, 1536) == 1, "positive and negative nonzero boost both consume stored velocity with zero thrust");
			auto expected = before;
			expected.mtype.phys_info.velocity = adjacent ? vms_vector{ 0, 0, -60 * F1_0 } : vms_vector{ 100 * F1_0, 0, 0 };
			const auto after = registry();
			require(!std::memcmp(&expected, &actor, sizeof(actor)) && !std::memcmp(&state, &after, sizeof(state)) && !std::memcmp(&controls, &Controls, sizeof(Controls)) && streams() == rng && xl_speed_boost_player_locked() == !adjacent, "zero-thrust restores stored velocity while negative steering remains contact-unlocked");
			++cases;
		}
		for (int guard = 0; guard < 8; ++guard) {
			setup();
			dispatch();
			auto &actor = Objects[player_slot];
			actor.mtype.phys_info.flags |= PF_USES_THRUST;
			actor.mtype.phys_info.drag = F1_0 / 2;
			actor.mtype.phys_info.thrust = { 40 * F1_0, 80 * F1_0, -120 * F1_0 };
			if (guard == 0) {
				GameTime64 += F1_0;
				Triggers[0].value = 0;
				require(dispatch() == 0, "real action release before motion fallback");
			}
			if (guard == 1) actor.mtype.phys_info.velocity = {};
			if (guard == 2) actor.mtype.phys_info.flags &= ~PF_USES_THRUST;
			if (guard == 4) actor.signature += 100;
			const bool hidden = guard >= 5;
			xl_speed_boost_state saved{};
			if (hidden || guard == 2 || guard == 3) saved = registry();
			if (hidden) {
				extensions = false;
				if (guard == 6) fixture_mission.descent_version = 1;
				if (guard == 7) Game_mode = GM_MULTI;
			}
			const auto before = actor;
			const auto controls = Controls;
			const auto rng = streams();
			const int begin = xl_speed_boost_begin(player_slot), drag = xl_speed_boost_drag(player_slot, guard == 3 ? -1 : 1024);
			require(begin == ((guard == 2 || guard == 3) ? 1 : 0) && drag == 0 && !std::memcmp(&before, &actor, sizeof(actor)) && !std::memcmp(&controls, &Controls, sizeof(Controls)) && streams() == rng, "released zero-velocity no-thrust-flag negative-time signature and inactive hooks return exact fallback without actor/RNG change");
			if (hidden) {
				extensions = true;
				Game_mode = 0;
				fixture_mission.descent_version = 2;
				const auto after = registry();
				require(!std::memcmp(&saved, &after, sizeof(saved)), "shared inactive admission preserves hidden registry unchanged");
			} else if (guard == 2 || guard == 3) {
				const auto after = registry();
				require(!std::memcmp(&saved, &after, sizeof(saved)), "rejected drag preserves complete natural positive registry row");
			} else require(!xl_speed_boost_active(player_slot) && !xl_speed_boost_state_count(), "released cancelled or stale state is no longer active");
			++cases;
		}
		boost_trace = matcen_trace = false;
		xl_speed_boost_clear();
		Num_walls = 0;
		std::cout << "Actual boost motion hooks: " << cases << " natural-registry,full/fractional drag,clamp,restore and fallback cases passed\n";
		return;
	}

	int cases = 0;
	setup();
	const auto before = Objects[player_slot];
	const auto controls = Controls;
	const auto rng = streams();
	require(dispatch() == 0, "Pumo-shaped value-one real boost wall dispatch");
	auto boosted = before;
	boosted.mtype.phys_info.velocity = { 100 * F1_0, 0, 0 };
	require(!std::memcmp(&boosted, &Objects[player_slot], sizeof(boosted)) && !std::memcmp(&controls, &Controls, sizeof(controls)) && streams() == rng && xl_speed_boost_active(player_slot) == 1 && xl_speed_boost_player_locked(), "actual positive boost changes only velocity and locks local player");
	xl_speed_boost_state state{};
	require(xl_speed_boost_state_count() == 1 && xl_speed_boost_state_get(0, &state), "actual positive registry owner");
	xl_speed_boost_state expected{};
	expected.owner = player_slot;
	expected.signature = Objects[player_slot].signature;
	expected.boosted = 1;
	expected.speed = 1;
	expected.source[2] = 10 * F1_0;
	expected.destination[0] = 100 * F1_0;
	expected.destination[2] = 10 * F1_0;
	expected.velocity[0] = 100 * F1_0;
	expected.minimum[0] = 40 * F1_0;
	expected.minimum[1] = expected.minimum[2] = -60 * F1_0;
	expected.maximum[0] = 160 * F1_0;
	expected.maximum[1] = expected.maximum[2] = 60 * F1_0;
	require(!std::memcmp(&expected, &state, sizeof(state)), "literal cube boost source destination velocity clamps and owner registry");
	const auto contact_rng = streams();
	contact();
	require(Objects[player_slot].mtype.phys_info.velocity.x == 100 * F1_0 && !Objects[player_slot].mtype.phys_info.velocity.y && !Objects[player_slot].mtype.phys_info.velocity.z && xl_speed_boost_player_locked(), "actual positive boost registry suppresses matcen velocity write");
	GameTime64 += F1_0;
	Triggers[0].value = 0;
	const auto release_rng = streams();
	require(dispatch() == 0 && !xl_speed_boost_active(player_slot) && !xl_speed_boost_player_locked() && !xl_speed_boost_state_count() && xl_speed_boost_last_speed() == 10 && Objects[player_slot].mtype.phys_info.velocity.x == 60 * F1_0 && !Objects[player_slot].mtype.phys_info.velocity.y && !Objects[player_slot].mtype.phys_info.velocity.z && streams() == release_rng, "real zero-value release normalizes speed60 and clears lock without RNG");
	contact();
	auto after = streams();
	auto final = before;
	final.shields = 92 * F1_0;
	final.mtype.phys_info.velocity = { 4788224, 1905952, 180128 };
	require(!std::memcmp(&final, &Objects[player_slot], sizeof(final)) && after[2] == contact_rng[2] + 8 && after[0] == 2993822286u && after[1] == contact_rng[1] && after[3] == contact_rng[3], "actual trigger lock contact release contact episode exact final actor and streams");
	++cases;
	setup(true);
	const auto adjacent_before = Objects[player_slot];
	const auto adjacent_controls = Controls;
	const auto adjacent_rng = streams();
	require(dispatch() == 0 && xl_speed_boost_active(player_slot) == -1 && !xl_speed_boost_player_locked() && !Controls.vertical_thrust_time && !Controls.forward_thrust_time && !Controls.sideways_thrust_time, "source-valid reciprocal portal action creates negative steering state and clears controls");
	const auto steering = Objects[player_slot].mtype.phys_info.velocity;
	require(steering.x == 0 && steering.y == 0 && steering.z == -60 * F1_0, "adjacent side-five source direction speed60");
	auto adjacent_expected = adjacent_before;
	adjacent_expected.mtype.phys_info.velocity = steering;
	auto cleared_controls = adjacent_controls;
	cleared_controls.vertical_thrust_time = cleared_controls.forward_thrust_time = cleared_controls.sideways_thrust_time = 0;
	require(!std::memcmp(&adjacent_expected, &Objects[player_slot], sizeof(adjacent_expected)) && !std::memcmp(&cleared_controls, &Controls, sizeof(Controls)) && streams() == adjacent_rng, "adjacent action changes only velocity and three control channels without RNG");
	require(xl_speed_boost_state_get(0, &state) && state.owner == player_slot && state.signature == Objects[player_slot].signature && state.boosted == -1 && state.speed == 1 && !state.steps && !state.phase && state.source[0] == 0 && state.source[1] == 0 && state.source[2] == 10 * F1_0 && state.destination[0] == 100 * F1_0 && state.destination[1] == 0 && state.destination[2] == -10 * F1_0, "actual negative steering owner and source destination registry");
	contact();
	require(Objects[player_slot].mtype.phys_info.velocity.x == 856064 && Objects[player_slot].mtype.phys_info.velocity.y == 1905952 && Objects[player_slot].mtype.phys_info.velocity.z == steering.z + 180128 && xl_speed_boost_active(player_slot) == -1, "real negative steering registry permits matcen kick");
	++cases;
	setup();
	dispatch();
	GameTime64 += F1_0;
	Triggers[0].num_links = 0;
	const auto zero_link_rng = streams();
	require(dispatch() == 0 && !xl_speed_boost_state_count() && !xl_speed_boost_player_locked() && xl_speed_boost_last_speed() == 1 && Objects[player_slot].mtype.phys_info.velocity.x == 60 * F1_0 && !Objects[player_slot].mtype.phys_info.velocity.y && !Objects[player_slot].mtype.phys_info.velocity.z && streams() == zero_link_rng, "actual zero-link release retains requested speed setting and normalizes velocity");
	++cases;
	for (int guard = 0; guard < 5; ++guard) {
		setup();
		if (guard == 0) extensions = false;
		if (guard == 1) {
			extensions = false;
			fixture_mission.descent_version = 1;
		}
		if (guard == 2) Game_mode = GM_MULTI;
		if (guard == 3) Player_is_dead = 1;
		const auto before = Objects[player_slot], remote_before = Objects[2];
		const auto controls_before = Controls;
		const auto rng = streams();
		int result = guard == 4 ? xl_wall_trigger(0, 2, 0) : dispatch();
		const int expected_results[5] = { -1, -1, 1, 0, 1 };
		require(result == expected_results[guard] && !xl_speed_boost_state_count() && !std::memcmp(&before, &Objects[player_slot], sizeof(before)) && !std::memcmp(&remote_before, &Objects[2], sizeof(remote_before)) && !std::memcmp(&controls_before, &Controls, sizeof(Controls)) && streams() == rng, "real dispatcher exact result and unchanged local/remote actors controls and streams");
		++cases;
	}
	setup();
	dispatch();
	const auto velocity = Objects[player_slot].mtype.phys_info.velocity;
	Objects[player_slot].signature += 100;
	require(!xl_speed_boost_active(player_slot) && !xl_speed_boost_player_locked() && !xl_speed_boost_state_count() && !std::memcmp(&velocity, &Objects[player_slot].mtype.phys_info.velocity, sizeof(velocity)), "simulated signature reuse clears stale boost ownership without velocity write");
	++cases;
	boost_trace = matcen_trace = false;
	xl_speed_boost_clear();
	Num_walls = 0;
	std::cout << "Actual boost trigger and matcen contact: " << cases << " registry,release,steering,lifetime and scope cases passed\n";
}

void checkpoint_controls()
{
	const vms_matrix identity{ { F1_0, 0, 0 }, { 0, F1_0, 0 }, { 0, 0, F1_0 } }, reverse{ { -F1_0, 0, 0 }, { 0, F1_0, 0 }, { 0, 0, -F1_0 } };
	auto streams = [] {unsigned sim=0,fx=0;d_rand_get_stream_state(D_RNG_SIM,&sim);d_rand_get_stream_state(D_RNG_FX,&fx);return std::array<unsigned,4>{sim,fx,d_rand_get_stream_call_count(D_RNG_SIM),d_rand_get_stream_call_count(D_RNG_FX)}; };
	std::array<unsigned, 4> checkpoint_rng{};
	auto setup = [&] {
		Difficulty_level = 2;
		reset();
		GameTime64 = 20 * F1_0;
		xl_wall_trigger_reset();
		xl_spawn_clear();
		checkpoint_trace = true;
		checkpoint_messages = checkpoint_relinks = checkpoint_deletes = 0;
		checkpoint_marker = -1;
		checkpoint_expected_segment = 1;
		for (int i = 0; i < NUM_MARKERS; ++i) {
			MarkerObject[i] = -1;
			std::memset(MarkerMessage[i], 0, MARKER_MESSAGE_LEN);
		}
		HighlightMarker = -1;
		Num_segments = 5;
		Highest_segment_index = 4;
		Highest_vertex_index = 39;
		const int offsets[8][3] = { { -1, 1, -1 }, { 1, 1, -1 }, { 1, -1, -1 }, { -1, -1, -1 }, { -1, 1, 1 }, { 1, 1, 1 }, { 1, -1, 1 }, { -1, -1, 1 } };
		for (int room = 0; room < 5; ++room) {
			Segments[room] = {};
			Segments[room].objects = -1;
			for (int side = 0; side < 6; ++side) {
				Segments[room].children[side] = -1;
				Segments[room].sides[side].wall_num = -1;
			}
			for (int i = 0; i < 8; ++i) {
				int index = room * 8 + i;
				Segments[room].verts[i] = index;
				Vertices[index] = { room * 100 * F1_0 + offsets[i][0] * 10 * F1_0, offsets[i][1] * 10 * F1_0, offsets[i][2] * 10 * F1_0 };
			}
		}
		ConsoleObject = &Objects[player_slot];
		auto &a = *ConsoleObject;
		a.type = OBJ_PLAYER;
		a.id = Player_num;
		a.segnum = 0;
		a.pos = { 7, 8, 9 };
		a.orient = reverse;
		a.movement_type = MT_PHYSICS;
		a.mtype.phys_info.mass = 2 * F1_0;
		a.mtype.phys_info.velocity = { 123, -456, 789 };
		a.shields = Players[Player_num].shields = 100 * F1_0;
		a.flags = OF_ATTACHED;
		Objects[2] = a;
		Objects[2].id = 1;
		Objects[2].signature = 1802;
		Num_triggers = 1;
		Triggers[0] = {};
		auto &t = Triggers[0];
		t.type = 28;
		t.flags = TF_NO_MESSAGE;
		t.value = 5 * F1_0;
		t.time = -1;
		t.num_links = 4;
		for (int i = 0; i < 4; ++i) {
			t.seg[i] = i + 1;
			t.side[i] = (i % 2) ? 5 : 4;
		}
		checkpoint_rng = streams();
	};
	auto snapshot = [] {std::array<xl_spawn_point,XL_SPAWN_SLOTS> points{};require(xl_spawn_state_count()==XL_SPAWN_SLOTS,"all sixteen configured checkpoint slots");for(int i=0;i<XL_SPAWN_SLOTS;++i)require(xl_spawn_state_get(i,&points[i]),"checkpoint slot read");return points; };
	auto check_points = [&](bool reversed = false) {auto points=snapshot();for(int i=0;i<XL_SPAWN_SLOTS;++i){int link=reversed?3-i%4:i%4;int room=link+1;const auto &v=points[i];const auto &basis=link%2?reverse:identity;require(v.segment==room&&v.side==(link%2?5:4)&&v.position[0]==room*100*F1_0&&!v.position[1]&&!v.position[2]&&!std::memcmp(v.orientation,&basis,sizeof(basis)),"literal cube checkpoint center and identity/reverse-Z orientation");}return points; };
	auto dispatch = [] { return xl_wall_trigger(0, player_slot, 0); };
	auto finish = [&] {require(streams()==checkpoint_rng,"checkpoint dispatch placement markers and guards preserve both complete RNG streams");checkpoint_trace=false; };
	int cases = 0;
	setup();
	const auto initial = Objects[player_slot];
	const auto rng = streams();
	require(!xl_spawn_apply(Player_num, player_slot) && !checkpoint_relinks && !xl_spawn_state_count(), "unconfigured marker-free placement inert");
	require(dispatch() == 0, "actual type28 wall dispatch");
	check_points();
	require(!std::memcmp(&initial, &Objects[player_slot], sizeof(initial)) && !checkpoint_messages && streams() == rng, "silent four-target checkpoint configuration leaves player and both RNG streams unchanged");
	require(xl_spawn_apply(Player_num, player_slot) == 1 && checkpoint_relinks == 1, "actual local checkpoint placement");
	auto placed = initial;
	placed.pos = { 100 * F1_0, 0, 0 };
	placed.orient = identity;
	placed.segnum = 1;
	require(!std::memcmp(&placed, &Objects[player_slot], sizeof(placed)) && streams() == rng, "real placement changes only position orientation and recorded relink segment");
	finish();
	++cases;
	for (int control = 0; control < 3; ++control) {
		setup();
		Triggers[0].flags = control == 0 ? 0 : control == 1 ? TF_NO_MESSAGE | TF_ONE_SHOT
		                                                    : TF_NO_MESSAGE;
		require(dispatch() == 0, "checkpoint HUD one-shot reconfiguration dispatch");
		auto points = check_points();
		require(checkpoint_messages == (control == 0), "checkpoint explicit HUD positive versus no-message");
		if (control == 1) {
			require(Triggers[0].flags == (TF_NO_MESSAGE | TF_ONE_SHOT | TF_DISABLED), "checkpoint one-shot consumption");
			xl_spawn_clear();
			GameTime64 += F1_0;
			require(dispatch() == 1 && !xl_spawn_state_count(), "one-shot suppresses repeated registry configuration");
		}
		if (control == 2) {
			GameTime64 += F1_0;
			for (int i = 0; i < 4; ++i) {
				Triggers[0].seg[i] = 4 - i;
				Triggers[0].side[i] = ((3 - i) % 2) ? 5 : 4;
			}
			require(dispatch() == 0, "checkpoint reversed target reconfiguration");
			check_points(true);
			checkpoint_expected_segment = 4;
			require(xl_spawn_apply(Player_num, player_slot) && Objects[player_slot].pos.x == 400 * F1_0 && !std::memcmp(&Objects[player_slot].orient, &reverse, sizeof(reverse)), "reconfigured first slot actual placement");
		}
		finish();
		++cases;
	}
	for (int control = 0; control < 6; ++control) {
		setup();
		dispatch();
		const auto points = snapshot();
		const auto a = Objects[player_slot];
		const auto rng = streams();
		int player = Player_num, index = player_slot;
		if (control == 0) extensions = false;
		if (control == 1) {
			extensions = false;
			fixture_mission.descent_version = 1;
		}
		if (control == 2) Game_mode = GM_MULTI;
		if (control == 3) player = 1;
		if (control == 4) index = 2;
		if (control == 5) Objects[player_slot].type = OBJ_ROBOT;
		const auto guarded = Objects[player_slot];
		require(!xl_spawn_apply(player, index) && !checkpoint_relinks && !std::memcmp(&guarded, &Objects[player_slot], sizeof(guarded)) && streams() == rng, "checkpoint apply profile local-player and object-type admission guards");
		if (control < 3) {
			xl_wall_trigger_reset();
			require(dispatch() == (control == 2 ? 1 : -1), "actual wall dispatcher profile guards");
		}
		extensions = true;
		Game_mode = 0;
		require(!std::memcmp(points.data(), snapshot().data(), sizeof(points)), "guarded placement and dispatch preserve configured registry");
		finish();
		++cases;
	}
	for (int invalid = 0; invalid < 3; ++invalid) {
		setup();
		dispatch();
		const auto points = snapshot();
		GameTime64 += F1_0;
		if (invalid == 0) Triggers[0].seg[3] = 5;
		if (invalid == 1) Triggers[0].side[3] = 6;
		if (invalid == 2) Triggers[0].num_links = MAX_WALLS_PER_LINK + 1;
		require(dispatch() == 1 && !std::memcmp(points.data(), snapshot().data(), sizeof(points)), "invalid checkpoint target/count does not partly replace registry");
		finish();
		++cases;
	}
	for (int marker_case = 0; marker_case < 5; ++marker_case) {
		setup();
		if (marker_case == 0) {
			require(dispatch() == 0, "configure checkpoint before marker priority test");
			check_points();
		}
		auto &m = Objects[4];
		m = {};
		m.type = OBJ_MARKER;
		m.id = 2;
		m.segnum = 2;
		m.pos = { 201 * F1_0, 2 * F1_0, 3 * F1_0 };
		m.orient = identity;
		MarkerObject[2] = 4;
		std::strcpy(MarkerMessage[2], marker_case == 1 ? "spawn" : "SPAWN");
		if (marker_case == 2) m.type = OBJ_ROBOT;
		if (marker_case == 3) m.id = 3;
		if (marker_case == 4) m.segnum = 5;
		checkpoint_expected_segment = 2;
		const auto before = Objects[player_slot];
		require(xl_spawn_apply(Player_num, player_slot) == (marker_case == 0), "exact uppercase SPAWN requires live matching marker slot type/id/segment");
		if (marker_case == 0) {
			auto expected = before;
			expected.pos = m.pos;
			expected.orient = m.orient;
			expected.segnum = 2;
			require(!std::memcmp(&expected, &Objects[player_slot], sizeof(expected)), "marker placement actual transform and relink");
			checkpoint_marker = 4;
			MarkerObject[7] = 3;
			std::strcpy(MarkerMessage[7], "OTHER");
			HighlightMarker = 2;
			GameTime64 += F1_0;
			require(dispatch() == 0, "checkpoint dispatch clears overriding marker mapping");
			require(checkpoint_deletes == 1 && MarkerObject[2] == -1 && !MarkerMessage[2][0] && HighlightMarker == -1 && MarkerObject[7] == 3 && !std::strcmp(MarkerMessage[7], "OTHER"), "trigger clears only selected SPAWN mapping and records marker deletion");
			checkpoint_expected_segment = 1;
			require(xl_spawn_apply(Player_num, player_slot) == 1, "placement falls back to configured checkpoint after marker mapping removed");
			auto fallback = before;
			fallback.pos = { 100 * F1_0, 0, 0 };
			fallback.orient = identity;
			fallback.segnum = 1;
			require(!std::memcmp(&fallback, &Objects[player_slot], sizeof(fallback)) && checkpoint_relinks == 2 && checkpoint_deletes == 1 && MarkerObject[7] == 3 && !std::strcmp(MarkerMessage[7], "OTHER"), "checkpoint fallback exact transform preserves unrelated marker and does not delete again");
		} else require(!std::memcmp(&before, &Objects[player_slot], sizeof(before)) && !checkpoint_relinks, "stale/lowercase marker no placement");
		finish();
		++cases;
	}
	xl_spawn_clear();
	std::cout << "Actual checkpoint dispatch and placement: " << cases << " registry,geometry,transform,marker,guard and RNG cases passed\n";
}

void fluid_scrape_controls(int selector = 0)
{
	const vms_vector sentinel{ 123, -456, 789 }, lava_delta{ 260376, -18804, -23864 }, idle_delta{ 260836, 8996, -24528 };
	auto streams = [] {unsigned sim=0,fx=0;d_rand_get_stream_state(D_RNG_SIM,&sim);d_rand_get_stream_state(D_RNG_FX,&fx);return std::array<unsigned,4>{sim,fx,d_rand_get_stream_call_count(D_RNG_SIM),d_rand_get_stream_call_count(D_RNG_FX)}; };
	auto setup = [&](int c) {
		Difficulty_level = 2;
		reset();
		GameTime64 = 20 * F1_0;
		FrameTime = F1_0 / 4;
		Player_is_dead = Endlevel_sequence = 0;
		fluid_trace = true;
		fluid_protected = c == 4;
		fluid_inactive_dry = c >= 19;
		fluid_water_queries = 0;
		fluid_boost = c == 0 ? -1 : c == 2 ? 1
		                                   : 0;
		fluid_sounds = fluid_damage_records = fluid_damage_traces = fluid_warnings = 0;
		fluid_palettes.clear();
		fluid_events.clear();
		extensions = c < 16;
		Game_mode = (c == 18 || c == 21) ? GM_MULTI : 0;
		if (c == 17 || c == 20) fixture_mission.descent_version = 1;
		ConsoleObject = &Objects[player_slot];
		auto &a = *ConsoleObject;
		a.type = OBJ_PLAYER;
		a.id = Player_num;
		a.segnum = 0;
		a.control_type = CT_FLYING;
		a.movement_type = MT_PHYSICS;
		a.mtype.phys_info.mass = 2 * F1_0;
		a.mtype.phys_info.velocity = sentinel;
		a.mtype.phys_info.rotvel = { 11, 22, 33 };
		a.mtype.phys_info.thrust = {};
		a.shields = Players[Player_num].shields = 100 * F1_0;
		Objects[2] = a;
		Objects[2].id = 1;
		Objects[2].signature = 1702;
		if (c == 3) Players[Player_num].flags |= PLAYER_FLAGS_INVULNERABLE;
		if (c == 5) a.mtype.phys_info.mass = 0;
		if (c == 6) a.movement_type = MT_NONE;
		Automap_active = c == 7;
		Segments[0].sides[0].tmap_num = (c == 8 || c == 9) ? 399 : 409;
		Segments[0].sides[0].normals[0] = { F1_0, 0, 0 };
		TmapInfo[409] = {};
		TmapInfo[409].damage = fluid_inactive_dry ? 0 : 8 * F1_0;
		TmapInfo[399] = {};
		TmapInfo[399].flags = TMI_WATER;
		if (c == 9) a.mtype.phys_info.thrust.x = 1;
		fluid_point = { F1_0, 2 * F1_0, 3 * F1_0 };
		xl_fluid_clear();
		require(xl_fluid_restore(2 * F1_0, 1), "real fluid clock restore");
		if (c >= 10 && c <= 12) require(xl_fluid_restore(F1_0 / 4 + c - 11, 1), "real fluid delay boundary");
		if (c == 13) require(xl_fluid_restore(-1, 1), "backward clock restore");
		if (c == 14) {
			GameTime64 += 100 * F1_0;
			require(xl_fluid_restore(2 * F1_0, 1), "shifted clock restore");
		}
		reset_shield_warning_damage();
		restore_fixture_boost(fluid_boost);
		d_srand(2);
	};
	int cases = 0;
	for (int c = 0; c <= 21; ++c) {
		if (selector == 1 && c) continue;
		if (selector == 2 && c < 15) continue;
		setup(c);
		auto *a = (c >= 15 && c <= 18) ? &Objects[2] : ConsoleObject;
		const auto before = streams();
		const auto old = *a;
		scrape_player_on_wall(a, 0, 0, &fluid_point);
		const auto after = streams();
		const bool local = c < 15, water = c == 8 || c == 9, damage = local && !water && c != 3 && c != 4, turb = local && (!water && c != 4 || c == 9);
		const bool due = local && c != 10 && c != 11;
		const int draws = !local ? 0 : (turb ? 2 : 0) + 1 + (due ? 4 : 0);
		require(ConsoleObject->shields == 100 * F1_0 - (damage ? 2 * F1_0 : 0) && Players[Player_num].shields == ConsoleObject->shields && !Player_is_dead, "actual fluid surviving damage and local shield mirror");
		require(fluid_damage_records == damage && fluid_damage_traces == damage && fluid_warnings == damage && fluid_sounds == due, "fluid actual damage and sound boundaries");
		const auto &rot = ConsoleObject->mtype.phys_info.rotvel;
		require(rot.x == (turb ? 7737 : 11) && rot.y == 22 && rot.z == (turb ? -3217 : 33), "real fluid turbulence rotation only x/z");
		require(after[2] == before[2] + draws && after[1] == before[1] && after[3] == before[3], "fluid exact SIM draw count and FX preservation");
		unsigned state = !local ? before[0] : draws == 7 ? 1964509445u
		                                  : draws == 5   ? 3840747375u
		                                  : draws == 3   ? 2818548041u
		                                                 : 0u;
		require(after[0] == state, "fluid exact final SIM state");
		require(xl_fluid_scrape_elapsed() == (due ? 0 : (c >= 10 && c <= 12 ? F1_0 / 4 + c - 11 : 2 * F1_0)), "real fluid clock strict due boundary");
		std::vector<std::array<int, 3>> palettes;
		if (damage) palettes.push_back({ 8, -1, -1 });
		if (local && !water && c != 4) palettes.push_back({ 8, 0, 0 });
		require(fluid_palettes == palettes, "actual damage flash and fluid flash order");
		std::vector<int> events;
		if (damage) events = { 1, 2, 3, 4 };
		if (local && !water && c != 4) events.push_back(6);
		if (due) events.push_back(5);
		require(fluid_events == events, "real fluid damage, warning, extra flash and sound dispatch order");
		const bool kick = due && c != 2 && c != 5 && c != 6 && c != 7;
		auto wanted = sentinel;
		const auto delta = turb ? lava_delta : idle_delta;
		if (kick) {
			wanted.x += delta.x;
			wanted.y += delta.y;
			wanted.z += delta.z;
		}
		if (std::memcmp(&wanted, &ConsoleObject->mtype.phys_info.velocity, sizeof(wanted))) std::cerr << "actual fluid velocity=" << ConsoleObject->mtype.phys_info.velocity.x << ',' << ConsoleObject->mtype.phys_info.velocity.y << ',' << ConsoleObject->mtype.phys_info.velocity.z << " shields=" << ConsoleObject->shields << " SIMdraws=" << draws << '\n';
		require(!std::memcmp(&wanted, &ConsoleObject->mtype.phys_info.velocity, sizeof(wanted)), "negative boost steering state must retain actual fluid kick");
		require(xl_speed_boost_active(player_slot) == (extensions ? fluid_boost : 0), "inherited fluid fixture uses actual registry state");
		auto expected_physics = old.mtype.phys_info;
		expected_physics.velocity = wanted;
		if (turb) {
			expected_physics.rotvel.x = 7737;
			expected_physics.rotvel.z = -3217;
		}
		require(!std::memcmp(&expected_physics, &a->mtype.phys_info, sizeof(expected_physics)), "entire physical state preserves every field except expected velocity and x/z turbulence");
		require(fluid_water_queries == fluid_inactive_dry, "inactive local dry surface reaches legacy water query exactly once");
		if (!local) require(!std::memcmp(&old, a, sizeof(old)), "nonlocal and inactive profile actor unchanged");
		fluid_trace = false;
		Automap_active = 0;
		++cases;
	}
	if (!selector) {
		setup(0);
		auto before = streams();
		scrape_player_on_wall(ConsoleObject, 0, 0, &fluid_point);
		const auto velocity = ConsoleObject->mtype.phys_info.velocity;
		fluid_boost = 1;
		restore_fixture_boost(fluid_boost);
		GameTime64 += 2 * F1_0;
		d_srand(2);
		scrape_player_on_wall(ConsoleObject, 0, 0, &fluid_point);
		const auto after = streams();
		require(!std::memcmp(&velocity, &ConsoleObject->mtype.phys_info.velocity, sizeof(velocity)) && ConsoleObject->shields == 96 * F1_0 && fluid_sounds == 2 && xl_speed_boost_active(player_slot) == 1 && after[0] == 1964509445u && after[2] == before[2] + 14 && after[1] == before[1] && after[3] == before[3], "negative steering to positive boost episode retains damage/RNG but stops second kick");
		fluid_trace = false;
		++cases;
	}
	std::cout << "Actual fluid scrape: " << cases << " boost,damage,turbulence,clock,profile and RNG cases passed\n";
}

void matcen_contact_controls(int selector = 0)
{
	const vms_vector sentinel{ 123, -456, 789 }, player_delta{ 856064, 1905952, 180128 }, robot_delta{ 157284, 209712, 0 }, last_exit_delta{ -323104, 2036480, -382528 };
	auto streams = [] { unsigned sim=0,fx=0;d_rand_get_stream_state(D_RNG_SIM,&sim);d_rand_get_stream_state(D_RNG_FX,&fx);return std::array<unsigned,4>{sim,fx,d_rand_get_stream_call_count(D_RNG_SIM),d_rand_get_stream_call_count(D_RNG_FX)}; };
	auto setup = [&](int control) {
		Difficulty_level = 2;
		reset();
		GameTime64 = 20 * F1_0;
		Player_is_dead = Endlevel_sequence = 0;
		xl_object_birth_clear();
		xl_object_trigger_clear();
		matcen_trace = true;
		matcen_locked = control == 0 || control == 15;
		matcen_protected = control == 10;
		matcen_axis = control == 11;
		matcen_sounds = matcen_explosions = matcen_damage_records = matcen_damage_traces = matcen_palettes = matcen_warnings = matcen_force_calls = 0;
		matcen_center_sides.clear();
		matcen_events.clear();
		matcen_recorded_force = {};
		extensions = control < 11 || control == 15;
		Game_mode = control == 14 ? GM_MULTI : 0;
		if (control == 13) fixture_mission.descent_version = 1;
		ConsoleObject = &Objects[player_slot];
		auto &player = *ConsoleObject;
		player.type = OBJ_PLAYER;
		player.id = Player_num;
		player.segnum = 0;
		player.pos = {};
		player.size = 2 * F1_0;
		player.control_type = CT_FLYING;
		player.movement_type = MT_PHYSICS;
		player.mtype.phys_info.mass = 2 * F1_0;
		player.mtype.phys_info.velocity = sentinel;
		player.shields = Players[Player_num].shields = 100 * F1_0;
		Objects[2] = player;
		Objects[2].id = 1;
		Objects[2].signature = 1602;
		Objects[owner_b].type = OBJ_NONE;
		Objects[owner_a] = {};
		auto &robot = Objects[owner_a];
		robot.type = OBJ_ROBOT;
		robot.id = child_type;
		robot.signature = 1603;
		robot.segnum = 0;
		robot.size = 8 * F1_0;
		robot.control_type = CT_AI;
		robot.movement_type = MT_PHYSICS;
		robot.mtype.phys_info.mass = 2 * F1_0;
		robot.mtype.phys_info.velocity = sentinel;
		robot.shields = 100 * F1_0;
		Robot_info[child_type].strength = 100 * F1_0;
		Robot_info[child_type].boss_flag = Robot_info[child_type].companion = 0;
		Robot_info[child_type].exp1_vclip_num = VCLIP_SMALL_EXPLOSION;
		matcen_actor = (control == 2 || control == 11 || control == 15) ? &robot : (control == 8 || control == 12 || control == 13 || control == 14) ? &Objects[2]
		                                                                                                                                             : &player;
		if (control == 6) matcen_actor->mtype.phys_info.mass = 0;
		if (control == 7) matcen_actor->movement_type = MT_NONE;
		if (control == 9) Players[Player_num].flags |= PLAYER_FLAGS_INVULNERABLE;
		Automap_active = control == 5;
		matcen_exits = control == 4 ? 0 : control == 3 ? 2
		                                               : 1;
		if (matcen_exits) Segments[0].children[0] = 1;
		if (matcen_exits == 2) Segments[0].children[1] = 1;
		reset_shield_warning_damage();
		matcen_before = matcen_actor->shields;
		restore_fixture_boost(matcen_locked ? 1 : 0);
		d_srand(2);
	};
	auto invoke = [&] {if(matcen_actor->type==OBJ_PLAYER)collide_player_and_materialization_center(matcen_actor);else collide_robot_and_materialization_center(matcen_actor); };
	auto finish = [&] {matcen_trace=false;Automap_active=0; };
	int cases = 0;
	for (int control = 0; control <= 15; ++control) {
		if (selector == 1 && control != 0) continue;
		if (selector == 2 && control < 11) continue;
		setup(control);
		const bool robot = matcen_actor->type == OBJ_ROBOT;
		const bool remote = !robot && matcen_actor != ConsoleObject;
		const bool classic_robot = control == 11;
		const auto rng = streams();
		const auto old_physics = matcen_actor->mtype.phys_info;
		invoke();
		const auto after = streams();
		const int draws = remote || robot || !matcen_exits ? 0 : matcen_exits * 4;
		const fix damage = remote || control == 9 || control == 10 ? 0 : robot ? F1_0
		                                                                       : 4 * F1_0;
		require(matcen_actor->shields == 100 * F1_0 - damage && (!(!robot && matcen_actor == ConsoleObject) || Players[Player_num].shields == 100 * F1_0 - damage) && !Player_is_dead && !death_calls, "real matcen surviving player/robot damage and local shield mirror");
		require(matcen_sounds == 1 && matcen_explosions == 1 && matcen_force_calls == classic_robot && matcen_damage_records == (!robot && damage != 0) && matcen_damage_traces == (!robot && damage != 0) &&
		            matcen_palettes == (!robot && damage != 0) && matcen_warnings == (!robot && damage != 0) && !births && !ai_calls && !morph_calls,
		        "actual matcen audio/explosion,damage and classic force boundaries");
		require(after[2] == rng[2] + draws && after[0] == (draws == 4 ? 2993822286u : draws == 8 ? 1826196826u
		                                                                                         : rng[0]) &&
		            after[1] == rng[1] && after[3] == rng[3],
		        "matcen real per-exit SIM draws and preserved FX stream");
		std::vector<int> sides;
		if (!remote)
			for (int side = 0; side < matcen_exits; ++side) sides.push_back(side);
		require(matcen_center_sides == sides, "actual last-open-side geometry traversal");
		std::vector<int> events{ 1, 2 };
		for (int side : sides) events.push_back(3);
		if (!robot && damage)
			for (int event : { 5, 6, 7, 8 }) events.push_back(event);
		require(matcen_events == events, "actual matcen audio/explosion/geometry/damage output order");
		vms_vector delta{};
		if (robot && !classic_robot) delta = robot_delta;
		if (!robot && !remote && control != 0 && control != 4 && control != 5 && control != 6 && control != 7) delta = control == 3 ? last_exit_delta : player_delta;
		const vms_vector expected{ sentinel.x + delta.x, sentinel.y + delta.y, sentinel.z + delta.z };
		if (std::memcmp(&expected, &matcen_actor->mtype.phys_info.velocity, sizeof(expected))) std::cerr << "matcen control=" << control << " actual velocity=" << matcen_actor->mtype.phys_info.velocity.x << ',' << matcen_actor->mtype.phys_info.velocity.y << ',' << matcen_actor->mtype.phys_info.velocity.z << " shields=" << matcen_actor->shields << " SIMdraws=" << draws << '\n';
		require(!std::memcmp(&expected, &matcen_actor->mtype.phys_info.velocity, sizeof(expected)), "boost-lock and ordinary matcen velocity-write contract");
		require(xl_speed_boost_player_locked() == (extensions && matcen_locked), "inherited matcen fixture uses actual positive lock registry");
		auto expected_physics = old_physics;
		expected_physics.velocity = expected;
		require(!std::memcmp(&expected_physics, &matcen_actor->mtype.phys_info, sizeof(expected_physics)), "matcen contact changes only actual scoped velocity fields");
		finish();
		++cases;
	}
	if (!selector) {
		setup(0);
		auto rng = streams();
		invoke();
		require(!std::memcmp(&sentinel, &matcen_actor->mtype.phys_info.velocity, sizeof(sentinel)) && matcen_actor->shields == 96 * F1_0, "locked contact keeps velocity before release");
		matcen_locked = false;
		matcen_before = matcen_actor->shields;
		restore_fixture_boost(matcen_locked ? 1 : 0);
		d_srand(2);
		invoke();
		const vms_vector expected{ sentinel.x + player_delta.x, sentinel.y + player_delta.y, sentinel.z + player_delta.z };
		require(!std::memcmp(&expected, &matcen_actor->mtype.phys_info.velocity, sizeof(expected)) && matcen_actor->shields == 92 * F1_0 && Players[Player_num].shields == 92 * F1_0 && matcen_sounds == 2 && matcen_explosions == 2 && matcen_damage_records == 2 && !xl_speed_boost_player_locked() && total_ouch == 8 * F1_0 && streams()[0] == 2993822286u && streams()[2] == rng[2] + 8 && streams()[1] == rng[1] && streams()[3] == rng[3],
		        "real contact resumes velocity kick after bounded boost-lock release while damage continues");
		finish();
		++cases;
	}
	std::cout << "Actual matcen contact: " << cases << " velocity,boost-lock,damage,last-exit,scope and RNG cases passed\n";
}

void unattributed_robot_kill_counters()
{
	for (const bool boss : { false, true })
		for (const bool lethal : { false, true })
			for (const int profile : { 0, 1, 2, 3, 4, 5, 6, 7, 8 }) {
				reset();
				GameTime64 = 20 * F1_0;
				const int victim = boss ? owner_a : 4;
				if (!boss) {
					Objects[victim].type = OBJ_ROBOT;
					Objects[victim].id = child_type;
					Objects[victim].shields = 100 * F1_0;
				}
				int killer = profile == 1 ? -2 : -1;
				if (profile == 2) killer = player_slot;
				if (profile == 3) {
					killer = 2;
					Objects[killer].type = OBJ_CNTRLCEN;
				}
				if (profile == 4) {
					killer = owner_b;
					Objects[killer].id = 6;
					Robot_info[6] = {};
					Robot_info[6].companion = 1;
				}
				if (profile == 5 || profile == 6) extensions = false;
				if (profile == 6) fixture_mission.descent_version = 1;
				if (profile == 7) Game_mode = GM_MULTI;
				if (profile == 8) cheats.robotskillrobots = 1;
				Players[0].num_kills_level = 7;
				Players[0].num_kills_total = 13;
				const fix before = Objects[victim].shields;
				const fix damage = lethal ? before + 1 : 1;
				require(apply_damage_to_robot(&Objects[victim], damage, killer) == lethal &&
				            Objects[victim].shields == before - damage && death_calls == lethal,
				        "unattributed damage retains shield and death effects");
				const int credited = lethal && (killer >= 0 || profile == 5 || profile == 6 || profile == 7);
				require(Players[0].num_kills_level == 7 + credited && Players[0].num_kills_total == 13 + credited,
				        "unattributed XL kill counter gate");
				const bool active = extensions;
				extensions = true;
				require(!trigger_state(owner_a).operated && births == 0, "kill credit is independent of object-trigger operation");
				extensions = active;
			}
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

namespace
{
void splash_controls(int mode = 0)
{
	struct Row {
		vms_vector position;
		fix exact, legacy;
		vms_vector force, rotation, old_force, old_rotation;
	};
	const Row rows[] = {
		{ { 917504, 2359296, 0 }, 67515, 0, { 24470, 62923, 0 }, { -30587, -78653, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
		{ { -917504, -2359296, 0 }, 67515, 0, { -24470, -62923, 0 }, { 30587, 78653, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
		{ { 1572864, 1572864, 1572864 }, 0, 122880, { 0, 0, 0 }, { 0, 0, 0 }, { 78643, 78643, 78643 }, { -98303, -98303, -98303 } },
		{ { -1572864, -1572864, -1572864 }, 0, 122880, { 0, 0, 0 }, { 0, 0, 0 }, { -78643, -78643, -78643 }, { 98303, 98303, 98303 } },
		{ { 1441792, 1441792, 1441792 }, 93138, 276480, { 53772, 53772, 53772 }, { -67215, -67215, -67215 }, { 176947, 176947, 176947 }, { -221183, -221183, -221183 } },
		{ { -1441792, -1441792, -1441792 }, 93138, 276480, { -53772, -53772, -53772 }, { 67215, 67215, 67215 }, { -176947, -176947, -176947 }, { 221183, 221183, 221183 } },
		{ { 1572864, 1966080, 0 }, 77723, 49152, { 48553, 60691, 0 }, { -60691, -75863, 0 }, { 30246, 37809, 0 }, { -37807, -47261, 0 } },
		{ { -1572864, -1966080, 0 }, 77723, 49152, { -48553, -60691, 0 }, { 60691, 75863, 0 }, { -30246, -37809, 0 }, { 37807, 47261, 0 } },
		{ { 0, 0, 0 }, 1966080, 1966080, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
		{ { 0, 0, 0 }, 1966080, 1966080, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
		{ { 655360, 0, 0 }, 1474560, 1474560, { 1474560, 0, 0 }, { -1843200, 0, 0 }, { 1474560, 0, 0 }, { -1843200, 0, 0 } },
		{ { -655360, 0, 0 }, 1474560, 1474560, { -1474560, 0, 0 }, { 1843200, 0, 0 }, { -1474560, 0, 0 }, { 1843200, 0, 0 } },
		{ { 2621440, 0, 0 }, 0, 0, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
		{ { -2621440, 0, 0 }, 0, 0, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
		{ { 2686976, 0, 0 }, 0, 0, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
		{ { -2686976, 0, 0 }, 0, 0, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } },
		{ { 524288, 524288, 524288 }, 1285011, 1351680, { 741896, 741896, 741896 }, { -927370, -927370, -927370 }, { 865074, 865074, 865074 }, { -1081342, -1081342, -1081342 } },
		{ { -524288, -524288, -524288 }, 1285011, 1351680, { -741896, -741896, -741896 }, { 927370, 927370, 927370 }, { -865074, -865074, -865074 }, { 1081342, 1081342, 1081342 } },
		{ { 1, 1, 1 }, 1966080, 1966080, { 1966080, 1966080, 1966080 }, { -2457600, -2457600, -2457600 }, { 1966080, 1966080, 1966080 }, { -2457600, -2457600, -2457600 } },
		{ { -1, -1, -1 }, 1966080, 1966080, { -1966080, -1966080, -1966080 }, { 2457600, 2457600, 2457600 }, { -1966080, -1966080, -1966080 }, { 2457600, 2457600, 2457600 } },
	};
	int cases = 0;
	for (int profile = 0; profile < 4 && mode != 3; ++profile) {
		if ((mode == 1 || mode == 4 || mode == 5) && profile) continue;
		if (mode == 2 && !profile) continue;
		for (unsigned row = 0; row < sizeof(rows) / sizeof(rows[0]); ++row) {
			if (row >= 16 && profile) continue;
			if (mode == 1 && row) continue;
			if (mode == 4 && row != 2) continue;
			if (mode == 5 && row != 16) continue;
			for (int exclusion = 0; exclusion < 3; ++exclusion) {
				if ((mode == 1 || mode == 4 || mode == 5) && exclusion) continue;
				reset();
				xl_object_trigger_clear();
				xl_boss_spawn_clear();
				for (int i = 0; i <= Highest_object_index; ++i) Objects[i] = {};
				Highest_object_index = 3;
				Objects[player_slot].type = OBJ_PLAYER;
				Objects[player_slot].id = Player_num;
				Objects[player_slot].pos = { 1000 * F1_0, 0, 0 };
				auto &victim = Objects[owner_a];
				victim.type = OBJ_ROBOT;
				victim.id = child_type;
				victim.signature = 701;
				victim.shields = 100 * F1_0;
				victim.pos = rows[row].position;
				victim.size = F1_0;
				victim.movement_type = MT_PHYSICS;
				Robot_info[child_type].strength = 200 * F1_0;
				Robot_info[child_type].boss_flag = Robot_info[child_type].companion = 0;
				GameTime64 = 20 * F1_0;
				Difficulty_level = 2;
				extensions = profile != 1;
				fixture_mission.descent_version = profile == 2 ? 1 : 2;
				Game_mode = profile == 3 ? GM_MULTI : 0;
				if (exclusion == 2) victim.flags |= OF_SHOULD_BE_DEAD;
				splash_trace = true;
				splash_visible = exclusion != 1;
				splash_visibility = splash_forces = splash_rotations = 0;
				const auto before = victim;
				const auto parent_before = Objects[player_slot];
				unsigned sim, fx;
				d_rand_get_stream_state(D_RNG_SIM, &sim);
				d_rand_get_stream_state(D_RNG_FX, &fx);
				const auto sim_calls = d_rand_get_stream_call_count(D_RNG_SIM), fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
				vms_vector origin{};
				splash_origin = origin;
				Vclip[7].play_time = F1_0;
				const int before_births = births;
				auto *effect = object_create_explosion_sub(nullptr, 0, &origin, 7 * F1_0, 7, 30 * F1_0, 40 * F1_0, 30 * F1_0, player_slot);
				const fix expected = exclusion ? 0 : profile ? rows[row].legacy
				                                             : rows[row].exact;
				if (mode == 1 || mode == 4) std::cout << "observed damage=" << (100 * F1_0 - victim.shields) << " expected=" << expected << "\n";
				require(effect == &Objects[4] && victim.shields == 100 * F1_0 - expected, row == 2 ? "ordinary concussion rejects false-inclusion diagonal" : "ordinary concussion full-distance radius admission and scalar falloff");
				auto expected_actor = before;
				expected_actor.shields -= expected;
				require(!std::memcmp(&victim, &expected_actor, sizeof(victim)) && !std::memcmp(&Objects[player_slot], &parent_before, sizeof(parent_before)), "splash preserves unrelated actor fields and parent");
				const bool inside = (profile ? rows[row].legacy : rows[row].exact) > 0;
				require(splash_visibility == int(inside && exclusion != 2) && splash_forces == int(inside && !exclusion) && splash_rotations == splash_forces, "splash visibility admission and impulse dispatch counts");

				require(births == before_births + 1 && effect->type == OBJ_FIREBALL && effect->id == 7 && effect->segnum == 0 && !std::memcmp(&effect->pos, &origin, sizeof(origin)) && effect->size == 7 * F1_0 && effect->control_type == CT_EXPLOSION && effect->movement_type == MT_NONE && effect->render_type == RT_FIREBALL && effect->lifeleft == F1_0 && effect->ctype.expl_info.spawn_time == -1 && effect->ctype.expl_info.delete_objnum == -1 && effect->ctype.expl_info.delete_time == -1, "real explosion setup on bounded allocation");
				unsigned after_sim, after_fx;
				d_rand_get_stream_state(D_RNG_SIM, &after_sim);
				d_rand_get_stream_state(D_RNG_FX, &after_fx);
				require(sim == after_sim && fx == after_fx && sim_calls == d_rand_get_stream_call_count(D_RNG_SIM) && fx_calls == d_rand_get_stream_call_count(D_RNG_FX), "splash preserves both complete RNG streams");
				if (mode == 5) std::cout << "observed impulse=" << splash_force.x << "," << splash_force.y << "," << splash_force.z << " damage=" << (100 * F1_0 - victim.shields) << "\n";
				if (splash_forces) {
					const auto &f = profile ? rows[row].old_force : rows[row].force;
					const auto &r = profile ? rows[row].old_rotation : rows[row].rotation;
					require(!std::memcmp(&splash_force, &f, sizeof(f)), "ordinary splash full normalized impulse");
					require(!std::memcmp(&splash_rotation, &r, sizeof(r)), "ordinary splash rotation uses corrected impulse");
				}
				splash_trace = false;
				++cases;
			}
		}
	}
	if (mode == 0 || mode == 3) {
		for (int safety = 0; safety < 4; ++safety) {
			const vms_vector position = safety == 0 ? vms_vector{ 1300000000, 1300000000, 1300000000 } : safety == 2 ? vms_vector{ INT32_MIN, INT32_MIN, INT32_MIN }
			                                                                                                         : vms_vector{ INT32_MAX, INT32_MAX, INT32_MAX };
			reset();
			xl_object_trigger_clear();
			xl_boss_spawn_clear();
			for (int i = 0; i <= Highest_object_index; ++i) Objects[i] = {};
			Highest_object_index = 3;
			Objects[player_slot].type = OBJ_PLAYER;
			Objects[player_slot].pos = { 1000 * F1_0, 0, 0 };
			auto &victim = Objects[owner_a];
			victim.type = OBJ_ROBOT;
			victim.id = child_type;
			victim.signature = 701;
			victim.shields = 100 * F1_0;
			victim.pos = position;
			victim.size = F1_0;
			Robot_info[child_type].strength = 200 * F1_0;
			GameTime64 = 20 * F1_0;
			const auto before = victim;
			splash_trace = true;
			splash_visibility = splash_forces = splash_rotations = 0;
			vms_vector origin = safety == 3 ? vms_vector{ INT32_MIN, INT32_MIN, INT32_MIN } : vms_vector{};
			splash_origin = origin;
			require(object_create_explosion_sub(nullptr, 0, &origin, 7 * F1_0, 7, 30 * F1_0, 40 * F1_0, 30 * F1_0, player_slot) == &Objects[4] && !std::memcmp(&victim, &before, sizeof(before)) && !splash_visibility && !splash_forces && !splash_rotations, "corrected-only distance saturation outside fix range");
			splash_trace = false;
			++cases;
		}
	}
	std::cout << "PASS splash distance " << cases << " cases\n";
}
} // namespace
namespace
{
void self_splash_controls(int mode = 0)
{
	int cases = 0;
	for (int profile = 0; profile < 4; ++profile)
		for (int parent_kind = 0; parent_kind < 3; ++parent_kind)
			for (int axis = 0; axis < 6; ++axis)
				for (int sign : { 1, -1 })
					for (int protection = 0; protection < 5; ++protection) {
						if (axis >= 4 && profile) continue;
						if (axis == 3 && sign < 0) continue;
						if (mode == 1 && (profile || parent_kind || axis != 1 || sign != 1 || protection)) continue;
						if (mode == 3 && axis >= 4) continue;
						if ((mode == 2 || mode == 3) && !profile && parent_kind == 0 && (axis == 1 || axis == 2 || axis >= 4)) continue;
						reset();
						xl_object_trigger_clear();
						xl_boss_spawn_clear();
						for (int i = 0; i <= Highest_object_index; ++i) Objects[i] = {};
						Highest_object_index = 3;
						Player_num = 0;
						ConsoleObject = &Objects[player_slot];
						ConsoleObject->type = OBJ_PLAYER;
						ConsoleObject->id = 0;
						ConsoleObject->signature = 101;
						ConsoleObject->segnum = 0;
						ConsoleObject->shields = 100 * F1_0;
						ConsoleObject->size = 2 * F1_0;
						if (axis == 0) ConsoleObject->pos.x = sign * 20 * F1_0;
						if (axis == 1) ConsoleObject->pos.y = sign * 20 * F1_0;
						if (axis == 2) ConsoleObject->pos.z = sign * 20 * F1_0;
						if (axis == 4) ConsoleObject->pos = { sign * 12 * F1_0, sign * 16 * F1_0, 0 };
						if (axis == 5) ConsoleObject->pos = { sign * 12 * F1_0, 0, sign * 16 * F1_0 };
						if (protection == 4) ConsoleObject->flags |= OF_SHOULD_BE_DEAD;
						std::strcpy(Players[0].callsign, "LOCAL");
						std::strcpy(Players[1].callsign, "REMOTE");
						Players[0].objnum = player_slot;
						Players[0].shields = 100 * F1_0;
						Players[0].flags = protection == 1 ? PLAYER_FLAGS_INVULNERABLE : 0;
						Player_is_dead = 0;
						Endlevel_sequence = 0;
						Controls = {};
						auto &weapon = Objects[2];
						weapon.type = OBJ_WEAPON;
						weapon.id = CONCUSSION_ID;
						weapon.ctype.laser_info.parent_type = OBJ_PLAYER;
						weapon.ctype.laser_info.parent_num = parent_kind == 0 ? player_slot : parent_kind == 1 ? 3
						                                                                                       : -1;
						weapon.ctype.laser_info.parent_signature = parent_kind == 0 ? 101 : parent_kind == 1 ? 301
						                                                                                     : -1;
						Objects[3].type = OBJ_PLAYER;
						Objects[3].id = 1;
						Objects[3].signature = 301;
						Objects[3].pos = { 1000 * F1_0, 0, 0 };
						Weapon_info[CONCUSSION_ID] = {};
						Weapon_info[CONCUSSION_ID].flash = 0;
						GameTime64 = 20 * F1_0;
						Difficulty_level = 2;
						extensions = profile != 1;
						fixture_mission.descent_version = profile == 2 ? 1 : 2;
						Game_mode = profile == 3 ? GM_MULTI : 0;
						self_splash_trace = true;
						self_splash_protected = protection == 2;
						self_splash_visible = protection != 3;
						self_splash_killer = parent_kind == 0 ? ConsoleObject : parent_kind == 1 ? &Objects[3]
						                                                                         : nullptr;
						self_splash_visibility = self_splash_forces = self_splash_rotations = self_splash_telemetry = self_splash_damage_events = self_splash_palettes = 0;
						self_splash_events.clear();
						self_splash_console = self_splash_timers = 0;
						self_splash_damage = axis == 3 ? 30 * F1_0 : 15 * F1_0;
						reset_shield_warning_damage();
						const auto actor_before = *ConsoleObject, weapon_before = weapon, parent_before = Objects[3];
						const auto player_before = Players[0];
						const auto controls_before = Controls;
						unsigned sim, fx;
						d_rand_get_stream_state(D_RNG_SIM, &sim);
						d_rand_get_stream_state(D_RNG_FX, &fx);
						const auto sim_calls = d_rand_get_stream_call_count(D_RNG_SIM), fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
						vms_vector origin{};
						Vclip[7].play_time = F1_0;
						require(object_create_explosion_sub(&weapon, 0, &origin, 7 * F1_0, 7, 30 * F1_0, 40 * F1_0, 30 * F1_0, weapon.ctype.laser_info.parent_num) == &Objects[4], "self splash actual explosion result");
						vms_vector expected_force{}, expected_rotation{};
						fix force = sign * 983040;
						fix rotation = force / (parent_kind == 1 ? 4 : (!profile && parent_kind == 0) ? 2
						                                           : (parent_kind == 0 && axis != 0)  ? 4
						                                                                              : 2);
						if (axis == 0) {
							expected_force.x = force;
							expected_rotation.x = rotation;
						}
						if (axis == 1) {
							expected_force.y = force;
							expected_rotation.y = rotation;
						}
						if (axis == 2) {
							expected_force.z = force;
							expected_rotation.z = rotation;
						}
						if (axis >= 4) {
							expected_force = { sign * 589815, axis == 4 ? sign * 786420 : 0, axis == 5 ? sign * 786420 : 0 };
							expected_rotation = { expected_force.x / (parent_kind == 1 ? 4 : 2), expected_force.y / (parent_kind == 1 ? 4 : 2), expected_force.z / (parent_kind == 1 ? 4 : 2) };
						}
						if (mode == 1) std::cout << "observed self rotation=" << self_splash_rotation.x << "," << self_splash_rotation.y << "," << self_splash_rotation.z << " shields=" << Players[0].shields << "\n";
						auto expected_actor = actor_before;
						auto expected_player = player_before;
						const fix damage = protection ? 0 : self_splash_damage;
						expected_actor.shields -= damage;
						expected_player.shields -= damage;
						require(!std::memcmp(ConsoleObject, &expected_actor, sizeof(expected_actor)) && !std::memcmp(&Players[0], &expected_player, sizeof(expected_player)) && !std::memcmp(&weapon, &weapon_before, sizeof(weapon)) && !std::memcmp(&Objects[3], &parent_before, sizeof(parent_before)) && !std::memcmp(&Controls, &controls_before, sizeof(Controls)), "self splash actual shields and whole actor/player/control preservation");
						require(self_splash_visibility == int(protection != 4) && self_splash_forces == int(protection < 3) && self_splash_rotations == self_splash_forces && self_splash_telemetry == self_splash_forces && self_splash_damage_events == int(!protection) && self_splash_palettes == int(!protection), "self splash exact ingress, force-before-damage and output counts");
						const auto expected_events = protection == 4 ? std::vector<int>{} : protection == 3 ? std::vector<int>{ 1 }
						                                                                : protection        ? std::vector<int>{ 1, 2, 3, 4 }
						                                                                                    : std::vector<int>{ 1, 2, 3, 4, 5, 6, 7, 8 };
						require(self_splash_events == expected_events, "self splash force/rotation/telemetry before accepted damage trace/event/palette");
						unsigned sim_after, fx_after;
						d_rand_get_stream_state(D_RNG_SIM, &sim_after);
						d_rand_get_stream_state(D_RNG_FX, &fx_after);
						require(sim == sim_after && fx == fx_after && sim_calls == d_rand_get_stream_call_count(D_RNG_SIM) && fx_calls == d_rand_get_stream_call_count(D_RNG_FX), "self splash preserves complete SIM and FX streams");
						require(total_ouch == (protection ? 0 : self_splash_damage) && last_damage_time == (protection ? 0 : GameTime64) && self_splash_timers == int(!protection) && self_splash_console == int(protection < 3), "actual shield warning accumulation/timestamp and console boundaries");
						auto &effect = Objects[4];
						require(effect.type == OBJ_FIREBALL && effect.id == 7 && effect.segnum == 0 && !effect.pos.x && !effect.pos.y && !effect.pos.z && effect.size == 7 * F1_0 && effect.control_type == CT_EXPLOSION && effect.movement_type == MT_NONE && effect.render_type == RT_FIREBALL && effect.lifeleft == F1_0 && effect.ctype.expl_info.spawn_time == -1 && effect.ctype.expl_info.delete_objnum == -1 && effect.ctype.expl_info.delete_time == -1 && !death_calls, "self splash real effect setup and surviving lifecycle");
						if (protection < 3) {
							require(!std::memcmp(&self_splash_force, &expected_force, sizeof(expected_force)), "self splash unchanged full impulse before torque assertion");
							require(!std::memcmp(&self_splash_rotation, &expected_rotation, sizeof(expected_rotation)), "XL self splash halves all torque axes only once");
						}
						self_splash_trace = false;
						++cases;
					}
	std::cout << "PASS self splash " << cases << " cases\n";
}
} // namespace

namespace
{
void smart_stun_controls(int mode = 0)
{
	struct Row {
		vms_vector position;
		fix frame, damage, old_damage;
		vms_vector force, old_force, rotation, old_rotation;
		int count, old_count;
		vms_vector spin, old_spin;
	};
	const Row rows[] = {
		{ { 131072, 131072, 131072 }, 1024, 2258204, 2293760, { 1303766, 1303766, 1303766 }, { 1468005, 1468005, 1468005 }, { -1629707, -1629707, -1629707 }, { -1835006, -1835006, -1835006 }, 88, 89, { 85112, -35387, 33792 }, { -86085, 35783, -34181 } },
		{ { -131072, -131072, -131072 }, 1024, 2258204, 2293760, { -1303766, -1303766, -1303766 }, { -1468005, -1468005, -1468005 }, { 1629707, 1629707, 1629707 }, { 1835006, 1835006, 1835006 }, 88, 89, { 85112, -35387, 33792 }, { -86085, 35783, -34181 } },
		{ { 131072, 131072, 131072 }, 1092, 2258204, 2293760, { 1303766, 1303766, 1303766 }, { 1468005, 1468005, 1468005 }, { -1629707, -1629707, -1629707 }, { -1835006, -1835006, -1835006 }, 82, 84, { 79309, -32974, 31488 }, { -81249, 33773, -32261 } },
		{ { -131072, -131072, -131072 }, 1092, 2258204, 2293760, { -1303766, -1303766, -1303766 }, { -1468005, -1468005, -1468005 }, { 1629707, 1629707, 1629707 }, { 1835006, 1835006, 1835006 }, 82, 84, { 79309, -32974, 31488 }, { -81249, 33773, -32261 } },
		{ { 131072, 131072, 131072 }, 2048, 2258204, 2293760, { 1303766, 1303766, 1303766 }, { 1468005, 1468005, 1468005 }, { -1629707, -1629707, -1629707 }, { -1835006, -1835006, -1835006 }, 45, 45, { 43523, -18095, 17280 }, { -43526, 18092, -17282 } },
		{ { -131072, -131072, -131072 }, 2048, 2258204, 2293760, { -1303766, -1303766, -1303766 }, { -1468005, -1468005, -1468005 }, { 1629707, 1629707, 1629707 }, { 1835006, 1835006, 1835006 }, 45, 45, { 43523, -18095, 17280 }, { -43526, 18092, -17282 } },
		{ { 524288, 524288, 524288 }, 1024, 1168492, 1310720, { 674625, 674625, 674625 }, { 838860, 838860, 838860 }, { -843281, -843281, -843281 }, { -1048575, -1048575, -1048575 }, 46, 51, { 44490, -18497, 17664 }, { -49329, 20505, -19587 } },
		{ { -524288, -524288, -524288 }, 1024, 1168492, 1310720, { -674625, -674625, -674625 }, { -838860, -838860, -838860 }, { 843281, 843281, 843281 }, { 1048575, 1048575, 1048575 }, 46, 51, { 44490, -18497, 17664 }, { -49329, 20505, -19587 } },
		{ { 524288, 524288, 524288 }, 1092, 1168492, 1310720, { 674625, 674625, 674625 }, { 838860, 838860, 838860 }, { -843281, -843281, -843281 }, { -1048575, -1048575, -1048575 }, 43, 48, { 41589, -17291, 16512 }, { -46428, 19299, -18435 } },
		{ { -524288, -524288, -524288 }, 1092, 1168492, 1310720, { -674625, -674625, -674625 }, { -838860, -838860, -838860 }, { 843281, 843281, 843281 }, { 1048575, 1048575, 1048575 }, 43, 48, { 41589, -17291, 16512 }, { -46428, 19299, -18435 } },
		{ { 524288, 524288, 524288 }, 2048, 1168492, 1310720, { 674625, 674625, 674625 }, { 838860, 838860, 838860 }, { -843281, -843281, -843281 }, { -1048575, -1048575, -1048575 }, 24, 26, { 23212, -9651, 9216 }, { -25148, 10453, -9985 } },
		{ { -524288, -524288, -524288 }, 2048, 1168492, 1310720, { -674625, -674625, -674625 }, { -838860, -838860, -838860 }, { 843281, 843281, 843281 }, { 1048575, 1048575, 1048575 }, 24, 26, { 23212, -9651, 9216 }, { -25148, 10453, -9985 } },
		{ { 393216, 524288, 0 }, 1024, 1572864, 1546650, { 943704, 1258272, 0 }, { 905343, 1207140, 0 }, { -1179630, -1572840, 0 }, { -1131678, -1508925, 0 }, 61, 60, { 58998, -24529, 23424 }, { -58035, 24123, -23043 } },
		{ { -393216, -524288, 0 }, 1024, 1572864, 1546650, { -943704, -1258272, 0 }, { -905343, -1207140, 0 }, { 1179630, 1572840, 0 }, { 1131678, 1508925, 0 }, 61, 60, { 58998, -24529, 23424 }, { -58035, 24123, -23043 } },
		{ { 393216, 524288, 0 }, 1092, 1572864, 1546650, { 943704, 1258272, 0 }, { 905343, 1207140, 0 }, { -1179630, -1572840, 0 }, { -1131678, -1508925, 0 }, 58, 57, { 56096, -23323, 22272 }, { -55133, 22917, -21891 } },
		{ { -393216, -524288, 0 }, 1092, 1572864, 1546650, { -943704, -1258272, 0 }, { -905343, -1207140, 0 }, { 1179630, 1572840, 0 }, { 1131678, 1508925, 0 }, 58, 57, { 56096, -23323, 22272 }, { -55133, 22917, -21891 } },
		{ { 393216, 524288, 0 }, 2048, 1572864, 1546650, { 943704, 1258272, 0 }, { 905343, 1207140, 0 }, { -1179630, -1572840, 0 }, { -1131678, -1508925, 0 }, 31, 31, { 29982, -12465, 11904 }, { -29984, 12463, -11905 } },
		{ { -393216, -524288, 0 }, 2048, 1572864, 1546650, { -943704, -1258272, 0 }, { -905343, -1207140, 0 }, { 1179630, 1572840, 0 }, { 1131678, 1508925, 0 }, 31, 31, { 29982, -12465, 11904 }, { -29984, 12463, -11905 } },
		{ { 655360, 0, 0 }, 1024, 1572864, 1572864, { 1572864, 0, 0 }, { 1572864, 0, 0 }, { -1966080, 0, 0 }, { -1966080, 0, 0 }, 62, 62, { 59965, -24931, 23808 }, { -59969, 24927, -23811 } },
		{ { -655360, 0, 0 }, 1024, 1572864, 1572864, { -1572864, 0, 0 }, { -1572864, 0, 0 }, { 1966080, 0, 0 }, { 1966080, 0, 0 }, 62, 62, { 59965, -24931, 23808 }, { -59969, 24927, -23811 } },
		{ { 655360, 0, 0 }, 1092, 1572864, 1572864, { 1572864, 0, 0 }, { 1572864, 0, 0 }, { -1966080, 0, 0 }, { -1966080, 0, 0 }, 58, 58, { 56096, -23323, 22272 }, { -56100, 23319, -22275 } },
		{ { -655360, 0, 0 }, 1092, 1572864, 1572864, { -1572864, 0, 0 }, { -1572864, 0, 0 }, { 1966080, 0, 0 }, { 1966080, 0, 0 }, 58, 58, { 56096, -23323, 22272 }, { -56100, 23319, -22275 } },
		{ { 655360, 0, 0 }, 2048, 1572864, 1572864, { 1572864, 0, 0 }, { 1572864, 0, 0 }, { -1966080, 0, 0 }, { -1966080, 0, 0 }, 32, 32, { 30950, -12868, 12288 }, { -30952, 12866, -12290 } },
		{ { -655360, 0, 0 }, 2048, 1572864, 1572864, { -1572864, 0, 0 }, { -1572864, 0, 0 }, { 1966080, 0, 0 }, { 1966080, 0, 0 }, 32, 32, { 30950, -12868, 12288 }, { -30952, 12866, -12290 } },
		{ { 0, 0, 0 }, 1024, 2621440, 2621440, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, 2, 2, { 1934, -804, 768 }, { -1934, 804, -768 } },
		{ { 0, 0, 0 }, 1024, 2621440, 2621440, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, 2, 2, { 1934, -804, 768 }, { -1934, 804, -768 } },
		{ { 0, 0, 0 }, 1092, 2621440, 2621440, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, 2, 2, { 1934, -804, 768 }, { -1934, 804, -768 } },
		{ { 0, 0, 0 }, 1092, 2621440, 2621440, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, 2, 2, { 1934, -804, 768 }, { -1934, 804, -768 } },
		{ { 0, 0, 0 }, 2048, 2621440, 2621440, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, 2, 2, { 1934, -804, 768 }, { -1934, 804, -768 } },
		{ { 0, 0, 0 }, 2048, 2621440, 2621440, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, 2, 2, { 1934, -804, 768 }, { -1934, 804, -768 } },
	};
	int cases = 0;
	for (int profile = 0; profile < 4; ++profile)
		for (unsigned row = 0; row < sizeof(rows) / sizeof(rows[0]); ++row)
			for (int scenario = 0; scenario < 5; ++scenario)
				for (int initial_skip : { 0, 3 }) {
					if (mode == 1 && (profile || row || scenario || initial_skip)) continue;
					if (mode == 2 && (profile || row != 18 || scenario || initial_skip)) continue;
					if ((mode == 3 || mode == 4) && !profile && !scenario) continue;
					if (mode == 4 && !profile && (scenario == 1 || scenario == 4) && row < 18) continue;
					reset();
					xl_object_trigger_clear();
					xl_boss_spawn_clear();
					for (int i = 0; i <= Highest_object_index; ++i) Objects[i] = {};
					Highest_object_index = 3;
					Objects[player_slot].type = OBJ_PLAYER;
					Objects[player_slot].id = Player_num;
					Objects[player_slot].signature = 101;
					Objects[player_slot].pos = { 1000 * F1_0, 0, 0 };
					auto &victim = Objects[owner_a];
					victim.type = OBJ_ROBOT;
					victim.id = child_type;
					victim.signature = 701;
					victim.shields = 100 * F1_0;
					victim.pos = rows[row].position;
					victim.size = F1_0;
					victim.movement_type = MT_PHYSICS;
					victim.control_type = CT_AI;
					victim.ctype.ai_info.behavior = AIB_NORMAL;
					victim.ctype.ai_info.SKIP_AI_COUNT = initial_skip;
					victim.mtype.phys_info.rotthrust = { 11, -22, 33 };
					if (scenario == 2) victim.flags |= OF_SHOULD_BE_DEAD;
					Robot_info[child_type].strength = 200 * F1_0;
					Robot_info[child_type].companion = 0;
					Robot_info[child_type].boss_flag = scenario == 4 ? 21 : 0;
					auto &weapon = Objects[2];
					weapon.type = OBJ_WEAPON;
					weapon.id = SMART_ID;
					weapon.signature = 401;
					weapon.ctype.laser_info.parent_type = OBJ_PLAYER;
					weapon.ctype.laser_info.parent_num = player_slot;
					weapon.ctype.laser_info.parent_signature = 101;
					Weapon_info[SMART_ID] = {};
					Weapon_info[SMART_ID].flash = scenario == 1 ? 0 : 5;
					Weapon_info[SMART_ID].damage_radius = 25 * F1_0;
					Weapon_info[SMART_ID].children = 19;
					for (int difficulty = 0; difficulty < NDL; ++difficulty) Weapon_info[SMART_ID].strength[difficulty] = 40 * F1_0;
					GameTime64 = 20 * F1_0;
					Difficulty_level = 2;
					FrameTime = rows[row].frame;
					extensions = profile != 1;
					fixture_mission.descent_version = profile == 2 ? 1 : 2;
					Game_mode = profile == 3 ? GM_MULTI : 0;
					smart_trace = true;
					splash_trace = true;
					splash_visible = scenario != 3;
					splash_visibility = splash_forces = splash_rotations = 0;
					d_srand(2);
					const auto calls = d_rand_get_call_count();
					unsigned fx;
					d_rand_get_stream_state(D_RNG_FX, &fx);
					const auto fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
					const auto before = victim, parent_before = Objects[player_slot], weapon_before = weapon;
					vms_vector origin{};
					splash_origin = origin;
					Vclip[7].play_time = F1_0;
					auto *effect = object_create_explosion_sub(&weapon, 0, &origin, 7 * F1_0, 7, 40 * F1_0, 25 * F1_0, 40 * F1_0, player_slot);
					const bool accepted = scenario != 2 && scenario != 3, stun = accepted && scenario == 0 && profile != 2;
					const fix damage = accepted ? (profile ? rows[row].old_damage : rows[row].damage) : 0;
					auto expected = before;
					expected.shields -= damage;
					if (stun) {
						expected.ctype.ai_info.SKIP_AI_COUNT = victim.ctype.ai_info.SKIP_AI_COUNT;
						expected.mtype.phys_info.rotthrust = victim.mtype.phys_info.rotthrust;
						expected.mtype.phys_info.flags |= PF_USES_THRUST;
					}
					require(!std::memcmp(&victim, &expected, sizeof(expected)) && !std::memcmp(&Objects[player_slot], &parent_before, sizeof(parent_before)) && !std::memcmp(&weapon, &weapon_before, sizeof(weapon_before)), "Smart actual damage and unrelated actor/weapon preservation");
					require(splash_visibility == int(scenario != 2) && splash_forces == int(accepted) && splash_rotations == splash_forces, "Smart source visibility and physics output counts");
					if (accepted) {
						const auto &force = profile ? rows[row].old_force : rows[row].force;
						const auto &rotation = profile ? rows[row].old_rotation : rows[row].rotation;
						require(!std::memcmp(&splash_force, &force, sizeof(force)) && !std::memcmp(&splash_rotation, &rotation, sizeof(rotation)), "Smart unchanged force and robot rotation before stun assertion");
					}
					require(effect == &Objects[4] && effect->type == OBJ_FIREBALL && effect->id == 7 && effect->segnum == 0 && !effect->pos.x && !effect->pos.y && !effect->pos.z && effect->size == 7 * F1_0 && effect->control_type == CT_EXPLOSION && effect->movement_type == MT_NONE && effect->render_type == RT_FIREBALL && effect->lifeleft == F1_0 && effect->ctype.expl_info.spawn_time == -1 && effect->ctype.expl_info.delete_objnum == -1 && effect->ctype.expl_info.delete_time == -1 && effect->ctype.ai_info.SKIP_AI_COUNT == -1, "real explosion setup retains original union-owned skip admission");
					unsigned sim_after, fx_after;
					d_rand_get_state(&sim_after);
					d_rand_get_stream_state(D_RNG_FX, &fx_after);
					require(d_rand_get_call_count() == calls + (stun ? 3 : 0) && sim_after == (stun ? 2818548041u : 2u) && fx == fx_after && fx_calls == d_rand_get_stream_call_count(D_RNG_FX), "Smart exact three SIM draws/state and preserved FX stream");
					if (mode == 1 || mode == 2) std::cout << "observed skip=" << int(victim.ctype.ai_info.SKIP_AI_COUNT) << " spin=" << victim.mtype.phys_info.rotthrust.x << "," << victim.mtype.phys_info.rotthrust.y << "," << victim.mtype.phys_info.rotthrust.z << " damage=" << damage << "\n";
					if (stun) {
						require(victim.ctype.ai_info.SKIP_AI_COUNT == initial_skip + (profile ? rows[row].old_count : rows[row].count), "Smart stun uses source full force magnitude count");
						const auto &spin = profile ? rows[row].old_spin : rows[row].spin;
						require(!std::memcmp(&victim.mtype.phys_info.rotthrust, &spin, sizeof(spin)), "Smart stun uses source signed draw mapping");
					}
					smart_trace = false;
					splash_trace = false;
					++cases;
				}
	std::cout << "PASS Smart stun " << cases << " cases\n";
}
} // namespace

namespace
{
void placed_mine_controls(int mode = 0)
{
	struct Case {
		int distance;
		fix force, radius, expected_force;
		bool eligible, robot;
	};
	const Case rows[] = {
		{ 90, 225 * F1_0, 180 * F1_0, 7372800, true, true },
		{ 40, 225 * F1_0, 180 * F1_0, 11468800, false, false },
		{ 140, 225 * F1_0, 180 * F1_0, 3276800, false, false },
		{ 180, 225 * F1_0, 180 * F1_0, 0, false, false },
		{ 181, 225 * F1_0, 180 * F1_0, 0, false, false },
		{ 90, 225 * F1_0, 180 * F1_0, 7372800, false, false },
		{ 90, 225 * F1_0, 180 * F1_0, 7372800, false, false },
		{ 90, 225 * F1_0, 180 * F1_0, 7372800, false, false },
		{ 90, 225 * F1_0, 180 * F1_0, 7372800, true, false },
		{ 80, 200 * F1_0 - 2, 160 * F1_0, 6553599, false, false },
		{ 80, 200 * F1_0, 160 * F1_0, 6553600, false, false },
		{ 80, 200 * F1_0 + 1, 160 * F1_0, 6553601, true, false }
	};
	int cases = 0;
	for (int profile = 0; profile < 4; ++profile)
		for (unsigned row = 0; row < sizeof(rows) / sizeof(rows[0]); ++row) {
			if (mode == 1 && (profile || row)) continue;
			if (mode == 2 && !profile && (row == 0 || row == 11)) continue;
			reset();
			xl_object_trigger_clear();
			xl_boss_spawn_clear();
			for (int i = 0; i <= Highest_object_index; ++i) Objects[i] = {};
			Highest_object_index = 4;
			extensions = profile != 1;
			fixture_mission.descent_version = profile == 2 ? 1 : 2;
			Game_mode = profile == 3 ? GM_MULTI : 0;
			Difficulty_level = 2;
			GameTime64 = 20 * F1_0;
			Objects[player_slot].type = OBJ_PLAYER;
			Objects[player_slot].id = Player_num;
			Objects[player_slot].signature = 101;
			Objects[player_slot].pos = { 1000 * F1_0, 0, 0 };
			auto &mine = Objects[2];
			mine.type = OBJ_WEAPON;
			mine.id = row == 8 ? PROXIMITY_ID : PMINE_ID;
			mine.signature = 402;
			mine.segnum = 0;
			mine.pos = { rows[row].distance * F1_0, 0, 0 };
			mine.movement_type = MT_PHYSICS;
			mine.mtype.phys_info.mass = F1_0;
			mine.mtype.phys_info.velocity = { 123, -456, 789 };
			mine.lifeleft = 10 * F1_0;
			mine.ctype.laser_info.parent_type = OBJ_PLAYER;
			mine.ctype.laser_info.parent_num = player_slot;
			mine.ctype.laser_info.parent_signature = 101;
			if (row == 5) mine.flags |= OF_SHOULD_BE_DEAD;
			auto &source = Objects[4];
			source.type = OBJ_WEAPON;
			source.id = EARTHSHAKER_ID;
			source.signature = 404;
			source.segnum = 0;
			source.ctype.laser_info.parent_type = OBJ_PLAYER;
			source.ctype.laser_info.parent_num = player_slot;
			source.ctype.laser_info.parent_signature = 101;
			auto &robot = Objects[owner_a];
			if (rows[row].robot) {
				robot.type = OBJ_ROBOT;
				robot.id = child_type;
				robot.signature = 703;
				robot.segnum = 0;
				robot.pos = { 120 * F1_0, 0, 0 };
				robot.size = F1_0;
				robot.shields = 500 * F1_0;
				robot.control_type = CT_AI;
				robot.ctype.ai_info.behavior = AIB_NORMAL;
				Robot_info[child_type].strength = 500 * F1_0;
				Robot_info[child_type].boss_flag = Robot_info[child_type].companion = 0;
			}
			Weapon_info[EARTHSHAKER_ID] = {};
			Weapon_info[EARTHSHAKER_ID].damage_radius = 180 * F1_0;
			Weapon_info[EARTHSHAKER_ID].children = 44;
			for (int d = 0; d < NDL; ++d) Weapon_info[EARTHSHAKER_ID].strength[d] = 225 * F1_0;
			Weapon_info[mine.id] = {};
			Weapon_info[mine.id].flash = 0;
			Weapon_info[mine.id].children = -1;
			Weapon_info[mine.id].robot_hit_vclip = mine.id == PMINE_ID ? 5 : 7;
			Weapon_info[mine.id].impact_size = 7 * F1_0;
			Weapon_info[mine.id].damage_radius = (mine.id == PMINE_ID ? 40 : 75) * F1_0;
			for (int d = 0; d < NDL; ++d) Weapon_info[mine.id].strength[d] = mine.id == PMINE_ID ? (10 + 5 * d) * F1_0 : d == NDL - 1 ? 3283354
				                                                                                                                      : 50 * F1_0;
			Vclip[7].play_time = F1_0;
			Vclip[5].play_time = 2 * F1_0;
			mine_trace = true;
			mine_repeat = false;
			mine_blocked = row == 6;
			mine_id = mine.id;
			mine_position = mine.pos;
			mine_allocations = mine_audio = mine_classic_audio = mine_children = 0;
			mine_events.clear();
			const bool chain = rows[row].eligible && (row == 8 || profile == 0);
			const bool admitted = row != 3 && row != 4 && row != 5 && row != 7;
			const bool force = admitted && !mine_blocked;
			const auto mine_before = mine, source_before = source, parent_before = Objects[player_slot], robot_before = robot;
			unsigned sim, fx;
			d_rand_get_state(&sim);
			d_rand_get_stream_state(D_RNG_FX, &fx);
			const auto sim_calls = d_rand_get_call_count(), fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
			vms_vector origin{};
			auto *effect = object_create_explosion_sub(row == 7 ? &mine : &source, 0, &origin, 7 * F1_0, 7, 225 * F1_0, rows[row].radius, rows[row].force, player_slot);
			require((mine.flags & ~OF_SHOULD_BE_DEAD) == (mine_before.flags & ~OF_SHOULD_BE_DEAD), "non-death mine flag bits preserved");
			auto preserved = mine_before;
			preserved.flags = mine.flags;
			require(!std::memcmp(&mine, &preserved, sizeof(mine)) && !std::memcmp(&source, &source_before, sizeof(source)) && !std::memcmp(&Objects[player_slot], &parent_before, sizeof(parent_before)), "mine/source/attribution fields preserved apart from dead bit");
			unsigned sim_after, fx_after;
			d_rand_get_state(&sim_after);
			d_rand_get_stream_state(D_RNG_FX, &fx_after);
			require(sim == sim_after && fx == fx_after && sim_calls == d_rand_get_call_count() && fx_calls == d_rand_get_stream_call_count(D_RNG_FX), "nested no-child explosions preserve both RNG streams");
			if (force) require(mine_events.size() > 2 && mine_events[2] == std::array<int, 4>{ 3, 2, rows[row].expected_force, 0 }, "outer mine impulse precedes detonation admission");
			if (mode == 1) std::cout << "observed mine dead=" << int(bool(mine.flags & OF_SHOULD_BE_DEAD)) << " effects=" << mine_allocations << " robot shields=" << robot.shields << "\n";
			require(bool(mine.flags & OF_SHOULD_BE_DEAD) == (row == 5 || chain), "placed mine chains under strict XL threshold predicate");
			require(effect == &Objects[5] && mine_allocations == 1 + int(chain) && mine_children == int(chain) && mine_audio == int(chain) && mine_classic_audio == int(chain && !extensions), "real nested effect and bounded no-child/audio output counts");
			std::vector<std::array<int, 4>> expected{ { 1, 5, 7, int(row == 5) } };
			if (admitted) expected.push_back({ 2, 5, 2, int(row == 5) });
			if (force) expected.push_back({ 3, 2, rows[row].expected_force, 0 });
			if (chain) {
				expected.push_back({ 4, 2, 0, 1 });
				if (!extensions) expected.push_back({ 5, 2, 11, 1 });
				expected.push_back({ 1, 6, Weapon_info[mine_id].robot_hit_vclip, 1 });
				if (rows[row].robot) {
					expected.push_back({ 2, 6, owner_a, 1 });
					expected.push_back({ 3, owner_a, 5 * F1_0, 0 });
					expected.push_back({ 6, owner_a, -409600, 0 });
					expected.push_back({ 7, 500 * F1_0, 5 * F1_0, 495 * F1_0 });
				}
				expected.push_back({ 8, mine_id, NUM_SMART_CHILDREN, 1 });
			}
			if (rows[row].robot) {
				const int before = chain ? 495 : 500;
				expected.push_back({ 2, 5, owner_a, int(chain) });
				expected.push_back({ 3, owner_a, 75 * F1_0, 0 });
				expected.push_back({ 6, owner_a, -6144000, 0 });
				expected.push_back({ 7, before * F1_0, 75 * F1_0, (before - 75) * F1_0 });
			}
			require(mine_events == expected, "exact reentrant force/audio/allocation/damage/child ordering");
			auto expected_robot = robot_before;
			if (rows[row].robot) expected_robot.shields = (chain ? 420 : 425) * F1_0;
			require(!std::memcmp(&robot, &expected_robot, sizeof(robot)), "real nested then outer surviving robot damage");
			for (int slot = 5; slot <= Highest_object_index; ++slot) {
				const auto &e = Objects[slot];
				const bool inner = slot == 6;
				require(e.type == OBJ_FIREBALL && e.id == (inner ? Weapon_info[mine_id].robot_hit_vclip : 7) && e.lifeleft == Vclip[e.id].play_time && e.ctype.expl_info.spawn_time == -1 && e.ctype.expl_info.delete_objnum == -1 && e.ctype.expl_info.delete_time == -1, "actual nested fireball setup");
			}
			if (!profile && !row) {
				mine_repeat = true;
				auto dead_mine = mine;
				auto repeated_robot = robot;
				const auto before_events = mine_events.size();
				require(object_create_explosion_sub(&source, 0, &origin, 7 * F1_0, 7, 225 * F1_0, 180 * F1_0, 225 * F1_0, player_slot) == &Objects[7], "repeat outer creates only one ordinary effect");
				repeated_robot.shields = 345 * F1_0;
				require(!std::memcmp(&mine, &dead_mine, sizeof(mine)) && !std::memcmp(&robot, &repeated_robot, sizeof(robot)) && mine_allocations == 3 && mine_audio == 1 && mine_children == 1, "already-dead mine does not detonate again on repeated outer event");
				const std::vector<std::array<int, 4>> suffix(mine_events.begin() + before_events, mine_events.end());
				require(suffix == std::vector<std::array<int, 4>>{ { 1, 7, 7, 1 }, { 2, 7, owner_a, 1 }, { 3, owner_a, 75 * F1_0, 0 }, { 6, owner_a, -6144000, 0 }, { 7, 420 * F1_0, 75 * F1_0, 345 * F1_0 } }, "repeat outer damage without nested callbacks");
				d_rand_get_state(&sim_after);
				d_rand_get_stream_state(D_RNG_FX, &fx_after);
				require(sim == sim_after && fx == fx_after && sim_calls == d_rand_get_call_count() && fx_calls == d_rand_get_stream_call_count(D_RNG_FX), "repeat outer preserves both complete streams");
			}
			mine_trace = false;
			mine_repeat = false;
			++cases;
		}
	std::cout << "PASS placed mine " << cases << " cases\n";
}
} // namespace

namespace
{
void separating_bump_controls(int mode = 0)
{
	struct Row {
		vms_vector delta, v0, v1, force, rotation;
		int damage, old_damage, active_d1_damage;
		bool old_event, reject;
	};
	const Row rows[] = {
		{ { 655360, 0, 0 }, { -4194304, 0, 0 }, { 4194304, 0, 0 }, { -8388608, 0, 0 }, { -2097152, 0, 0 }, 131072, 524288, 131072, true, true },
		{ { 0, 655360, 0 }, { 0, -4194304, 0 }, { 0, 4194304, 0 }, { 0, -8388608, 0 }, { 0, -2097152, 0 }, 131072, 524288, 131072, true, true },
		{ { 0, 0, 655360 }, { 0, 0, -4194304 }, { 0, 0, 4194304 }, { 0, 0, -8388608 }, { 0, 0, -2097152 }, 131072, 524288, 131072, true, true },
		{ { 655360, 0, 0 }, { 4194304, 0, 0 }, { -4194304, 0, 0 }, { 8388608, 0, 0 }, { 2097152, 0, 0 }, 131072, 524288, 131072, true, false },
		{ { 655360, 0, 0 }, { 0, 0, 0 }, { 4194304, 0, 0 }, { -4194304, 0, 0 }, { -1048576, 0, 0 }, 65536, 262144, 65536, true, false },
		{ { 655360, 0, 0 }, { -4194304, 0, 0 }, { 0, 0, 0 }, { -4194304, 0, 0 }, { -1048576, 0, 0 }, 65536, 262144, 65536, true, false },
		{ { 655360, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, false, false },
		{ { 655360, 0, 0 }, { 0, -4194304, 0 }, { 0, 4194304, 0 }, { 0, -8388608, 0 }, { 0, -2097152, 0 }, 131072, 524288, 131072, true, false },
		{ { 655360, 0, 0 }, { -4194304, 0, 0 }, { -2097152, 0, 0 }, { -2097152, 0, 0 }, { -524288, 0, 0 }, 32768, 131072, 32768, true, false },
		{ { 0, 0, 0 }, { -4194304, 0, 0 }, { 4194304, 0, 0 }, { -8388608, 0, 0 }, { -2097152, 0, 0 }, 131072, 524288, 131072, true, false },
		{ { 65535, 0, 0 }, { -1, 0, 0 }, { 1, 0, 0 }, { -2, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, false, false },
		{ { 65536, 0, 0 }, { -1, 0, 0 }, { 1, 0, 0 }, { -2, 0, 0 }, { 0, 0, 0 }, 0, 0, 0, false, true },
		{ { 1, 0, 0 }, { -65536, 0, 0 }, { 65535, 0, 0 }, { -131071, 0, 0 }, { -32767, 0, 0 }, 2047, 0, 2047, false, false },
		{ { 1, 0, 0 }, { -65535, 0, 0 }, { 65536, 0, 0 }, { -131071, 0, 0 }, { -32767, 0, 0 }, 2047, 0, 2047, false, false },
		{ { 196608, 262144, 0 }, { -6291456, -8388608, 0 }, { 6291456, 8388608, 0 }, { -12582912, -16777216, 0 }, { -3145728, -4194304, 0 }, 327680, 1343488, 335872, true, true },
	};
	int cases = 0;
	for (int profile = 0; profile < 5; ++profile)
		for (unsigned row = 0; row < sizeof(rows) / sizeof(rows[0]); ++row)
			for (int cheat : { 0, 1 })
				for (int swapped : { 0, 1 })
					for (int mirror : { 1, -1 }) {
						if (mode == 1 && (profile || row || cheat || swapped || mirror != 1)) continue;
						if (mode == 2 && (profile || row || cheat != 1 || swapped || mirror != 1)) continue;
						if (mode == 3 && !profile && rows[row].reject) continue;
						reset();
						xl_object_trigger_clear();
						xl_boss_spawn_clear();
						for (int i = 0; i <= Highest_object_index; ++i) Objects[i] = {};
						Highest_object_index = owner_b;
						ConsoleObject = &Objects[player_slot];
						ConsoleObject->type = OBJ_PLAYER;
						ConsoleObject->id = Player_num;
						ConsoleObject->signature = 101;
						auto &a = Objects[owner_a];
						auto &b = Objects[owner_b];
						for (int i = 0; i < 2; ++i) {
							auto &actor = i ? b : a;
							const int type = i ? 8 : child_type;
							Robot_info[type] = {};
							Robot_info[type].strength = 200 * F1_0;
							actor.type = OBJ_ROBOT;
							actor.id = type;
							actor.signature = 703 + i;
							actor.segnum = 0;
							actor.size = 6 * F1_0;
							actor.movement_type = MT_PHYSICS;
							actor.mtype.phys_info.mass = F1_0;
							actor.control_type = CT_AI;
							actor.ctype.ai_info.behavior = AIB_NORMAL;
							actor.shields = 100 * F1_0;
						}
						b.pos = { mirror * rows[row].delta.x, mirror * rows[row].delta.y, mirror * rows[row].delta.z };
						a.mtype.phys_info.velocity = { mirror * rows[row].v0.x, mirror * rows[row].v0.y, mirror * rows[row].v0.z };
						b.mtype.phys_info.velocity = { mirror * rows[row].v1.x, mirror * rows[row].v1.y, mirror * rows[row].v1.z };
						extensions = profile == 0 || profile == 3 || profile == 4;
						fixture_mission.descent_version = (profile == 2 || profile == 4) ? 1 : 2;
						Game_mode = profile == 3 ? GM_MULTI : 0;
						GameTime64 = 20 * F1_0;
						Difficulty_level = 0;
						cheats.robotskillrobots = cheat;
						separating_first = swapped ? &b : &a;
						separating_second = swapped ? &a : &b;
						const int sign = mirror * (swapped ? -1 : 1);
						separating_force = { sign * rows[row].force.x, sign * rows[row].force.y, sign * rows[row].force.z };
						separating_rotation = { sign * rows[row].rotation.x, sign * rows[row].rotation.y, sign * rows[row].rotation.z };
						const auto before_a = a, before_b = b, before_parent = *ConsoleObject;
						const auto player_before = Players[Player_num];
						const auto controls = Controls;
						unsigned sim, fx;
						d_rand_get_state(&sim);
						d_rand_get_stream_state(D_RNG_FX, &fx);
						const auto sim_calls = d_rand_get_call_count(), fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
						separating_trace = true;
						separating_events.clear();
						vms_vector point{};
						collide_robot_and_robot(separating_first, separating_second, &point);
						const bool did_bump = separating_events.size() > 1;
						const bool event = (profile == 0 || profile == 4) ? bool(cheat) : rows[row].old_event;
						const fix damage = event ? (profile == 4 ? rows[row].active_d1_damage : profile ? rows[row].old_damage
						                                                                                : rows[row].damage)
						                         : 0;
						auto expected_a = before_a, expected_b = before_b;
						if (did_bump) {
							expected_a.shields -= damage;
							expected_b.shields -= damage;
						}
						require(!std::memcmp(&a, &expected_a, sizeof(a)) && !std::memcmp(&b, &expected_b, sizeof(b)) && !std::memcmp(ConsoleObject, &before_parent, sizeof(before_parent)) && !std::memcmp(&Controls, &controls, sizeof(Controls)) && !std::memcmp(&Players[Player_num], &player_before, sizeof(player_before)), "physical pair whole-object preservation and real attribution-gated damage");
						std::vector<std::array<int, 5>> expected{ { 0, int(separating_first - Objects), int(separating_second - Objects), 0, 0 } };
						if (did_bump) {
							expected.push_back({ 1, int(separating_first - Objects), int(separating_second - Objects), 0, 0 });
							for (auto *actor : { separating_second, separating_first }) {
								const int direction = actor == separating_second ? 1 : -1;
								expected.push_back({ 2, int(actor - Objects), direction * separating_force.x, direction * separating_force.y, direction * separating_force.z });
								expected.push_back({ 3, int(actor - Objects), direction * separating_rotation.x, direction * separating_rotation.y, direction * separating_rotation.z });
								if (event) expected.push_back({ 4, int(actor - Objects), 100 * F1_0, damage, 100 * F1_0 - damage });
							}
							expected.push_back({ 5, int(separating_first - Objects), int(separating_second - Objects), 0, 0 });
						}
						require(separating_events == expected, "real pair ordered static/pre/force/rotation/damage/post boundaries");
						unsigned sim_after, fx_after;
						d_rand_get_state(&sim_after);
						d_rand_get_stream_state(D_RNG_FX, &fx_after);
						require(sim == sim_after && fx == fx_after && sim_calls == d_rand_get_call_count() && fx_calls == d_rand_get_stream_call_count(D_RNG_FX) && !death_calls && !births && !effect_calls && !sound_calls, "pair no death/score/effect and preserved complete RNG streams");
						if (mode == 1 || mode == 2) std::cout << "observed physical path=" << did_bump << " shields=" << a.shields << "," << b.shields << " robot-damage-cheat=" << cheat << "\n";
						const bool rejected = !profile && rows[row].reject;
						require(did_bump != rejected, "separating ordinary robots must not receive physical bump impulse");
						if (rejected) {
							const auto stable_a = a, stable_b = b;
							separating_events.clear();
							collide_robot_and_robot(separating_first, separating_second, &point);
							require(separating_events == std::vector<std::array<int, 5>>{ { 0, int(separating_first - Objects), int(separating_second - Objects), 0, 0 } } && !std::memcmp(&a, &stable_a, sizeof(a)) && !std::memcmp(&b, &stable_b, sizeof(b)), "repeated separating collision remains physically inert after static exclusion");
							d_rand_get_state(&sim_after);
							d_rand_get_stream_state(D_RNG_FX, &fx_after);
							require(sim == sim_after && fx == fx_after && sim_calls == d_rand_get_call_count() && fx_calls == d_rand_get_stream_call_count(D_RNG_FX), "repeated separating callback preserves streams");
						}
						separating_trace = false;
						++cases;
					}
	std::cout << "PASS separating pair " << cases << " cases\n";
}
} // namespace

int main(int argc, char **argv)
{
	try {
		const struct {
			const char *name;
			void (*run)();
		} groups[] = {
			{ "bump-separating", [] { separating_bump_controls(); } },
			{ "placed-mine", [] { placed_mine_controls(); } },
			{ "smart-stun", [] { smart_stun_controls(); } },
			{ "self-splash", [] { self_splash_controls(); } },
			{ "splash", [] { splash_controls(); } },
			{ "master-forcefield", [] { master_forcefield_controls(); } },
			{ "master-chain", [] { master_chain_controls(); } },
			{ "boost-motion", [] { boost_contact_controls(true); } },
			{ "boost-contact", [] { boost_contact_controls(); } },
			{ "checkpoint", [] { checkpoint_controls(); } },
			{ "fluid-scrape", [] { fluid_scrape_controls(); } },
			{ "matcen-contact", [] { matcen_contact_controls(); } },
			{ "forcefield", [] { forcefield_controls(); } },
			{ "gauss", [] { gauss_combat_controls(); } },
			{ "boss-cycle", [] { boss_cycle_controls(); } },
			{ "reactor-parent", [] { reactor_parent_controls(); } },
			{ "boss-projectile", [] { boss_projectile_controls(); } },
			{ "autoplay-disarm", [] { autoplay_disarm_controls(); } },
			{ "seismic", [] { seismic_gameplay_controls(); } },
			{ "advance", [] { countdown_advance_controls(); } },
			{ "reactor", [] { reactor_damage_controls(); } },
			{ "bump", [] { direct_bump_controls(); } },
			{ "boss-death", [] { boss_death_controls(); } },
			{ "countdown", [] { countdown_controls(); } },
			{ "weak-spot", [] { boss_weak_spot_controls(); } },
			{ "reflection", [] { boss_reflected_weapon_controls(); } },
			{ "weapon-callback", [] { weapon_spawn_callback_controls(); } },
			{ "attachment", [] { natural_spew_attachment_controls(); } },
			{ "threshold-and-attribution", [] { threshold_and_attribution(); } },
			{ "restore-retry-and-owner-isolation", [] { restore_retry_and_owner_isolation(); } },
			{ "nonpermanent-and-classic", [] { nonpermanent_and_classic(); } },
			{ "rejection", [] { rejection_controls(); } },
			{ "disabled-and-one-shot-ordering", [] { disabled_and_one_shot_ordering(); } },
			{ "common-damage-gates", [] { common_damage_gates(); } },
			{ "spawned-child-birth-and-registry", [] { spawned_child_birth_and_registry(); } },
			{ "definition-instance-and-zero-damage", [] { definition_instance_and_zero_damage(); } },
			{ "attacker", [] { robot_attacker_damage_gate(); } },
			{ "unattributed", [] { unattributed_robot_kill_counters(); } },
		};
		bool ran = false;
		for (const auto &group : groups) {
			if (argc == 1 || (argc == 2 && !std::strcmp(argv[1], group.name))) {
				group.run();
				ran = true;
			}
		}
		require(ran, "unknown gameplay consumer group");
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
void xl_slow_motion_off(void)
{
	require(Control_center_destroyed != 0, "time-equipment shutdown follows countdown publication");
	++slow_motion_off_calls;
}
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
weapon_info Weapon_info[MAX_WEAPON_TYPES];
// Only synthetic laser0 is admitted; smart-child energy classification is excluded.
const sbyte Weapon_is_energy[MAX_WEAPON_TYPES] = { 1 };
int N_weapon_types;
tmap_info TmapInfo[MAX_TEXTURES];
int Buddy_objnum = -1;
fix64 Last_gate_time;
fix Gate_interval;
#include "boss_collision_tables.h"
matcen_info RobotCenters[MAX_ROBOT_CENTERS];
int Num_robot_centers;
vclip Vclip[VCLIP_MAXNUM];
vms_vector Vertices[MAX_VERTICES];
char MarkerMessage[NUM_MARKERS][MARKER_MESSAGE_LEN];
int MarkerObject[NUM_MARKERS];
int HighlightMarker;
char *Text_string[N_TEXT_STRINGS]{};
window *Game_wind = nullptr;
int last_drawn_cockpit = 0;
int Newdemo_state = 0;
control_info Controls;
int Highest_vertex_index = 0;
#include "side_table.h"
wall Walls[MAX_WALLS];
int Num_walls;

Mission *Current_mission;
int Player_is_dead;
fix64 Boss_hit_time;
ai_local Ai_local_info[MAX_OBJECTS];
game_cheats cheats;
Arg GameArg;
object *ConsoleObject;
int Endlevel_sequence;
int Next_level_num, Entered_from_level, Do_appearance_effect;
int d_tick_step, PaletteRedAdd, PaletteGreenAdd, PaletteBlueAdd;
int Automap_active;
int Seismic_sound, Seismic_sound_playing, Seismic_tremor_volume, Seismic_tremor_magnitude;
fix64 Next_seismic_sound_time;
fix Level_shake_frequency, Level_shake_duration;
fix64 Buddy_sorry_time;

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
	if (checkpoint_trace) {
		require(index == player_slot && segment == checkpoint_expected_segment, "checkpoint placement relink request");
		++checkpoint_relinks;
	}
	if (teleport_trace) {
		require(index == owner_a && segment == 1, "owned boss relink boundary");
		++teleport_relinks;
	}
	Objects[index].segnum = segment;
}
void compute_segment_center(vms_vector *result, segment *where)
{
	if (teleport_trace) {
		require(where == &Segments[0] || where == &Segments[1], "authored room center boundary");
		++teleport_centers;
	}
	*result = { fix((where - Segments) * 100 * F1_0), 0, 0 };
}
int obj_create(enum object_type_t type, ubyte id, int segment, const vms_vector *position,
               const vms_matrix *orientation, fix size, ubyte control, ubyte movement, ubyte render)
{
	if (mine_trace) {
		++mine_allocations;
		require(mine_allocations <= 2 + (mine_repeat ? 1 : 0) && type == OBJ_FIREBALL && segment == 0 && position && orientation == &vmd_identity_matrix && size == 7 * F1_0 && control == CT_EXPLOSION && movement == MT_NONE && render == RT_FIREBALL, "bounded nested fireball allocation");
		const bool inner = position->x != 0;
		require((inner && mine_allocations == 2 && !std::memcmp(position, &mine_position, sizeof(mine_position)) && id == Weapon_info[mine_id].robot_hit_vclip && (Objects[2].flags & OF_SHOULD_BE_DEAD)) || (!inner && (mine_allocations == 1 || (mine_repeat && mine_allocations == 3)) && !position->y && !position->z && id == 7), "chain dead flag before exact inner allocation and bounded repeat");
		mine_events.push_back({ 1, Highest_object_index + 1, id, int(bool(Objects[2].flags & OF_SHOULD_BE_DEAD)) });
	}
	if (self_splash_trace) require(type == OBJ_FIREBALL && id == 7 && segment == 0 && position && !position->x && !position->y && !position->z && orientation == &vmd_identity_matrix && size == 7 * F1_0 && control == CT_EXPLOSION && movement == MT_NONE && render == RT_FIREBALL, "self splash bounded effect allocation");
	if (splash_trace) require(type == OBJ_FIREBALL && id == 7 && segment == 0 && position && !std::memcmp(position, &splash_origin, sizeof(splash_origin)) && orientation == &vmd_identity_matrix && size == 7 * F1_0 && control == CT_EXPLOSION && movement == MT_NONE && render == RT_FIREBALL, "splash bounded fireball allocator inputs");
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
	created_snapshot = Objects[index];
	created_snapshot_index = index;
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
object *__wrap_object_create_explosion(short segment, vms_vector *point, fix size, int vclip)
{
	if (matcen_trace) {
		require(segment == 0 && point == &matcen_actor->pos && size == (matcen_actor->type == OBJ_PLAYER ? 5 * F1_0 : 3 * F1_0) && vclip == (matcen_actor->type == OBJ_PLAYER ? VCLIP_PLAYER_HIT : VCLIP_SMALL_EXPLOSION), "matcen hit explosion boundary");
		++matcen_explosions;
		matcen_events.push_back(2);
		return nullptr;
	}
	if (reactor_contact_trace) {
		require(segment == 0 && point == reactor_contact_point && size == reactor_contact_size && vclip == VCLIP_SMALL_EXPLOSION, "reactor weapon contact impact boundary");
		++reactor_contact_impacts;
		return nullptr;
	}
	++effect_calls;
	return nullptr;
}
int digi_link_sound_to_pos(int sound, short segment, short side, vms_vector *point, int forever, fix volume)
{
	if (master_forcefield_trace) {
		const auto expected = segment == 0 ? vms_vector{ 11, 22, 33 } : vms_vector{ 44, 55, 66 };
		require(segment >= 0 && segment <= 1 && side == (segment ? 5 : 4) && point && !std::memcmp(point, &expected, sizeof(expected)) && sound == SOUND_FORCEFIELD_OFF && !forever && volume == F1_0, "forcefield off cue exact position and arguments");
		master_forcefield_events.push_back({ 2, segment, Walls[0].type, Walls[1].type });
		return 0;
	}
	if (fluid_trace) {
		require(sound == (Segments[0].sides[0].tmap_num == 399 ? SOUND_SHIP_IN_WATER : SOUND_VOLATILE_WALL_HISS) && segment == 0 && side == 0 && point && !std::memcmp(point, &fluid_point, sizeof(fluid_point)) && !forever && volume == F1_0, "fluid scrape sound arguments");
		++fluid_sounds;
		fluid_events.push_back(5);
		return 0;
	}
	if (matcen_trace) {
		require(sound == (matcen_actor->type == OBJ_PLAYER ? SOUND_PLAYER_GOT_HIT : SOUND_ROBOT_HIT) && segment == 0 && side == 0 && point == &matcen_actor->pos && !forever && volume == F1_0, "matcen contact sound boundary");
		++matcen_sounds;
		matcen_events.push_back(1);
		return 0;
	}
	if (forcefield_trace) {
		require(sound == SOUND_FORCEFIELD_BOUNCE_PLAYER && segment == 0 && side == 0 && point == forcefield_point && !forever && volume == F1_0, "forcefield bounce audio boundary");
		++forcefield_sounds;
		forcefield_events.push_back(3);
		return 0;
	}
	if (teleport_trace) {
		require(sound == 57 && segment == 1 && side == 0 && !forever && volume == F1_0, "boss teleport morph sound");
		teleport_outputs.push_back(2);
		return 0;
	}
	if (reactor_contact_trace && sound == SOUND_CONTROL_CENTER_HIT) {
		require(segment == 0 && side == 0 && !forever && volume == F1_0, "reactor weapon hit sound boundary");
		++reactor_contact_hitsounds;
		return 0;
	}
	if (reactor_trace) {
		require(sound == SOUND_CONTROL_CENTER_DESTROYED && segment == 0 && side == 0 && !forever && volume == F1_0, "reactor destruction audio endpoint");
		++reactor_audio;
		return 0;
	}
	if (weak_spot_collision || (contact_trace && sound == SOUND_WEAPON_HIT_DOOR)) {
		require(sound == SOUND_WEAPON_HIT_DOOR && segment == 0 && side == 0 && !forever && volume == F1_0,
		        "unexpected weak-spot impact sound");
		++weak_spot_sound_calls;
		return 0;
	}
	require(sound == Vclip[VCLIP_MORPHING_ROBOT].sound_num && segment == 0 && side == 0 &&
	            !forever && volume == F1_0,
	        "unexpected linked sound");
	++sound_calls;
	return 0;
}
void start_boss_death_sequence(object *actor)
{
	if (death_trace) xl_boss_start_death(int(actor - Objects));
	++death_calls;
}
void start_robot_death_sequence(object *)
{
	require(false, "unexpected robot death");
}
void __wrap_explode_object(object *actor, fix delay)
{
	if (reactor_trace) {
		require((actor == &Objects[4] || actor == &Objects[owner_b]) && delay == 0, "reactor destruction explosion endpoint");
		++reactor_explosions;
		return;
	}
	if (death_trace) {
		require((actor == &Objects[owner_a] || actor == &Objects[owner_b]) && delay == F1_0 / 4, "actual death explosion endpoint");
		++final_explosions;
		return;
	}
	require(actor->type == OBJ_ROBOT && actor->id == child_type && delay == F1_0 / 4, "unexpected explosion");
	++death_calls;
}
void buddy_message(char *, ...)
{
	require(false, "unexpected buddy message");
}
void newdemo_dump_note_robot_damage(object *actor, fix before, fix damage)
{
	if (separating_trace) {
		require((actor == separating_first || actor == separating_second) && actor->shields == before - damage, "actual separating force damage record");
		separating_events.push_back({ 4, int(actor - Objects), before, damage, actor->shields });
		return;
	}
	if (mine_trace) {
		require(actor == &Objects[owner_a] && actor->shields == before - damage, "actual nested robot shield change");
		mine_events.push_back({ 7, before, damage, actor->shields });
	}
}
void input_demo_record_robot_damage_event(object *, int32_t, int32_t) {}
void input_demo_log_replay_robot_damage(object *, int32_t, int32_t) {}
void input_demo_rng_trace_record_srand(int, const char *, const char *, int, uint32_t, int, uint32_t, int has_after, uint32_t state_after, uint32_t seed)
{
	require((countdown_trace || seismic_trace || teleport_trace || gauss_trace || forcefield_trace || matcen_trace || fluid_trace || smart_trace) && (seed == 2 || (seismic_trace && seed == 7)) && has_after && state_after == seed, "actual gameplay RNG seed record");
}
void input_demo_rng_trace_record_rand(int, const char *, const char *, int,
                                      uint32_t, int, uint32_t, int, uint32_t, int) {}
int wall_is_doorway(segment *, int)
{
	require(false, "unexpected wall geometry");
	return 0;
}
int find_point_seg(const vms_vector *position, int segment)
{
	require(((callback_point_lookup && position == &Objects[owner_a].pos) ||
	         ((weak_spot_collision && position == weak_spot_hit) || (contact_trace && position == contact_point))) &&
	            segment == Objects[owner_a].segnum,
	        "unexpected point lookup");
	++callback_point_calls;
	return segment;
}

int boss_spew_robot(object *, vms_vector *)
{
	require(false, "unexpected legacy boss spew");
	return -1;
}
int xl_custom_model_radius(int, int fallback)
{
	require(false, "unexpected reflection model lookup");
	return fallback;
}

int digi_xlat_sound(int sound)
{
	require(death_trace && sound == 77, "death sound translation");
	return sound;
}
fix piggy_sound_duration(int sound)
{
	require(death_trace && sound == 77, "death sound duration");
	return F1_0;
}
int64_t xl_gameplay_sound_object(int sound, int cls, int owner, int forever, int volume, int range, int loop_start, int loop_end, int decay)
{
	if (teleport_trace) {
		require(sound == 61 && cls == XL_SOUND_ROBOT && owner == owner_a && forever == 1 && volume == F1_0 && range == 512 * F1_0 && loop_start == -1 && loop_end == -1 && !decay, "boss teleport see-loop boundary");
		teleport_outputs.push_back(4);
		return 1;
	}
	require(death_trace && (owner == owner_a || owner == owner_b) && !forever && volume == 4 * F1_0 && loop_start == -1 && loop_end == -1 && !decay, "death audio endpoint");
	if (sound == 77) {
		require(cls == XL_SOUND_ROBOT && range == 1024 * F1_0, "death roll audio");
		++death_sound_events;
	} else {
		require(sound == 58 && cls == XL_SOUND_EXPLOSION && range == 512 * F1_0, "death final audio");
		++final_sound_events;
	}
	return 1;
}
void create_small_fireball_on_object(object *, fix, int)
{
	require(false, "FrameTime zero forbids death fireballs");
}
void wall_toggle(int segment, int side)
{
	require(death_trace && segment == 1 && side == 2 + countdown_wall_calls % 2, "countdown wall target/order");
	++countdown_wall_calls;
}
int PHYSFS_delete(const char *)
{
	require(false, "positive levels cannot delete secret save");
	return 0;
}

void phys_apply_force(object *actor, vms_vector *force)
{
	if (separating_trace) {
		require(actor == separating_first || actor == separating_second, "exact separating pair force owner");
		separating_events.push_back({ 2, int(actor - Objects), force->x, force->y, force->z });
		return;
	}
	if (mine_trace) {
		require((actor == &Objects[2] || actor == &Objects[owner_a]) && force && !force->y && !force->z, "exact mine-chain force output owner/axis");
		mine_events.push_back({ 3, int(actor - Objects), force->x, 0 });
		return;
	}
	if (self_splash_trace) {
		require(actor == ConsoleObject, "self splash force owner");
		++self_splash_forces;
		self_splash_force = *force;
		self_splash_events.push_back(2);
		return;
	}
	if (splash_trace) {
		require(actor == &Objects[owner_a], "splash force victim");
		++splash_forces;
		splash_force = *force;
		return;
	}
	if (matcen_trace) {
		require(!extensions && matcen_axis && actor == matcen_actor && actor->type == OBJ_ROBOT && force->x == 8 * F1_0 && !force->y && !force->z, "classic matcen axis-force dispatch only");
		++matcen_force_calls;
		matcen_recorded_force = *force;
		return;
	}
	if (gauss_trace) {
		require(contact_trace && actor == &Objects[owner_a] && actor->type == OBJ_ROBOT && !Robot_info[actor->id].boss_flag && !force->x && !force->y && !force->z, "ordinary Gauss zero force boundary");
		++gauss_force_calls;
		return;
	}
	require(bump_trace && actor == bump_victim, "unexpected force boundary");
	++bump_force_calls;
	bump_full = *force;
}
void phys_apply_rot(object *actor, vms_vector *force)
{
	if (separating_trace) {
		require(actor == separating_first || actor == separating_second, "exact separating pair rotation owner");
		separating_events.push_back({ 3, int(actor - Objects), force->x, force->y, force->z });
		return;
	}
	if (mine_trace) {
		require(actor == &Objects[owner_a] && force && !force->y && !force->z, "exact nested robot rotation output");
		mine_events.push_back({ 6, int(actor - Objects), force->x, 0 });
		return;
	}
	if (self_splash_trace) {
		require(actor == ConsoleObject, "self splash rotation owner");
		++self_splash_rotations;
		self_splash_rotation = *force;
		self_splash_events.push_back(3);
		return;
	}
	if (splash_trace) {
		require(actor == &Objects[owner_a], "splash rotation victim");
		++splash_rotations;
		splash_rotation = *force;
		return;
	}
	if (forcefield_trace) {
		require(actor == ConsoleObject && actor == &Objects[player_slot] && actor->type == OBJ_PLAYER && actor->movement_type == MT_PHYSICS &&
		            force->x >= -655360 && force->x <= 655320 && force->y >= -655360 && force->y <= 655320 && force->z >= -655360 && force->z <= 655320,
		        "forcefield bounded rotation output");
		forcefield_force = *force;
		++forcefield_rotations;
		forcefield_events.push_back(2);
		return;
	}
	if (gauss_trace) {
		require(contact_trace && actor == &Objects[owner_a] && actor->type == OBJ_ROBOT && !Robot_info[actor->id].boss_flag && !force->x && !force->y && !force->z, "ordinary Gauss zero rotation boundary");
		++gauss_rot_calls;
		return;
	}
	require(bump_trace && actor == bump_victim, "unexpected rotation boundary");
	++bump_rot_calls;
	bump_rotation = *force;
}
void obj_delete(int index)
{
	require(checkpoint_trace && index == checkpoint_marker, "checkpoint selected SPAWN marker delete output");
	++checkpoint_deletes;
}

int xl_speed_boost_destination_valid(const int32_t point[3])
{
	require(boost_fixture_restore && point && !point[0] && !point[1] && !point[2], "only inherited zero-destination fixture restore is permitted");
	return 1;
}
int xl_preview_water_contact(int, int)
{
	require(false, "unused classic fluid volume endpoint");
	return 0;
}
int xl_lava_segment(int)
{
	require(false, "solid fluid excludes volume policy");
	return 0;
}
int xl_preview_water_segment(int segment)
{
	require(fluid_trace && fluid_inactive_dry && !extensions && segment == 0 && ConsoleObject == &Objects[player_slot] && !TmapInfo[409].damage && !TmapInfo[409].flags, "explicit inactive local dry-surface water query");
	++fluid_water_queries;
	return 0;
}

void add_points_to_score(int score)
{
	require(reactor_trace && score == 5000, "reactor score endpoint");
	reactor_score += score;
}
void coop_record_robot_kill(int, int)
{
	require(false, "unexpected coop kill");
}
void ai_do_cloak_stuff(void)
{
	require(reactor_trace, "unexpected reactor cloak update");
	++reactor_cloak;
}
void input_demo_log_reactor_hit(object *actor, fix damage, fix old_shields)
{
	require(reactor_trace && (actor == &Objects[4] || actor == &Objects[owner_b]), "unexpected reactor hit");
	reactor_hit_samples.push_back({ int(actor - Objects), damage, old_shields });
	++reactor_hit_logs;
}
void input_demo_log_reactor_destroyed(object *actor)
{
	require(reactor_trace && (actor == &Objects[4] || actor == &Objects[owner_b]), "unexpected reactor death");
	++reactor_death_logs;
}
fix64 timer_query(void)
{
	if (self_splash_trace) {
		++self_splash_timers;
		self_splash_events.push_back(8);
		return GameTime64;
	}
	if (master_forcefield_trace) return GameTime64;
	if (master_trace) return GameTime64;
	if (boost_trace && !matcen_trace) return GameTime64;
	if (checkpoint_trace) return GameTime64;
	if (fluid_trace) {
		++fluid_warnings;
		fluid_events.push_back(4);
		return GameTime64;
	}
	if (matcen_trace) {
		++matcen_warnings;
		matcen_events.push_back(8);
		return GameTime64;
	}
	if (forcefield_trace) {
		++forcefield_warning_queries;
		forcefield_events.push_back(9);
		return GameTime64;
	}
	require(disarm_trace, "unexpected timer boundary");
	return disarm_clock;
}
int input_demo_replay_peek_environment(input_demo_environment_clock *)
{
	// This consumer exercises live trigger dispatch, not replay timeline admission
	return 0;
}
uint32_t xl_input_demo_cached_clock(void)
{
	// Reading a trigger snapshot must not consume another external timer event
	return static_cast<uint32_t>((disarm_trace ? disarm_clock : GameTime64) * 1000 / F1_0);
}
void xl_input_demo_observe(const char *consumer, int subject, input_demo_environment_clock *clock)
{
	require(!std::strcmp(consumer, "wall_trigger") && subject >= 0 && subject < Num_triggers && clock,
	        "unexpected clock consumer boundary");
	const auto ticks = timer_query() * 1000 / F1_0;
	*clock = { ticks, static_cast<uint32_t>(ticks), 0, static_cast<uint32_t>(ticks) };
}
int xl_preview_no_damage_segment(int segment)
{
	if (self_splash_trace) {
		require(segment == 0, "self splash protection input");
		return self_splash_protected;
	}
	if (fluid_trace) {
		require(segment == 0, "bounded fluid protection segment");
		return fluid_protected;
	}
	if (matcen_trace) {
		require(segment == 0, "bounded matcen protection input");
		return matcen_protected;
	}
	require(forcefield_trace && segment == 0, "bounded forcefield protection input");
	++forcefield_protection_queries;
	return forcefield_protected;
}
int multi_maybe_disable_friendly_fire(object *killer)
{
	if (self_splash_trace) {
		require(killer == self_splash_killer, "self splash friendly fire input");
		return 0;
	}
	if (fluid_trace) {
		require(killer == ConsoleObject, "bounded fluid friendly fire");
		return 0;
	}
	if (matcen_trace) {
		require(killer == ConsoleObject, "bounded matcen friendly-fire input");
		return 0;
	}
	require(forcefield_trace && killer == ConsoleObject, "bounded forcefield friendly-fire input");
	return 0;
}
bool object_is_observer(object *actor)
{
	if (self_splash_trace) {
		require(actor == ConsoleObject, "self splash observer input");
		return false;
	}
	if (fluid_trace) {
		require(actor == ConsoleObject, "bounded fluid observer input");
		return false;
	}
	if (matcen_trace) {
		require(actor == ConsoleObject, "matcen surviving-player observer input");
		return false;
	}
	require(forcefield_trace && actor == ConsoleObject, "bounded player observer input");
	return forcefield_observer;
}
void input_demo_trace_player_shield_change(const char *kind, int32_t before, int32_t after, const char *, const char *)
{
	if (self_splash_trace) {
		require(!std::strcmp(kind, "apply_damage") && before == 100 * F1_0 && after == before - self_splash_damage, "self splash actual shield trace");
		self_splash_events.push_back(5);
		return;
	}
	if (fluid_trace) {
		require(!std::strcmp(kind, "apply_damage") && before == Players[Player_num].shields + 2 * F1_0 && after == before - 2 * F1_0, "actual fluid damage trace");
		++fluid_damage_traces;
		fluid_events.push_back(1);
		return;
	}
	if (matcen_trace) {
		require(!std::strcmp(kind, "apply_damage") && before == matcen_before && after == before - 4 * F1_0, "actual matcen player shield trace");
		++matcen_damage_traces;
		matcen_events.push_back(5);
		return;
	}
	require(forcefield_trace && !std::strcmp(kind, "apply_damage") && before == forcefield_before && after == before - 9 * F1_0 / 2, "actual forcefield shield trace");
	++forcefield_trace_records;
	forcefield_events.push_back(6);
}
void input_demo_record_player_damage_event(int32_t damage, int32_t before, object *killer, int friendly)
{
	if (self_splash_trace) {
		require(damage == self_splash_damage && before == 100 * F1_0 && killer == self_splash_killer && friendly == 1, "self splash actual damage event");
		++self_splash_damage_events;
		self_splash_events.push_back(6);
		return;
	}
	if (fluid_trace) {
		require(damage == 2 * F1_0 && before == Players[Player_num].shields + damage && killer == ConsoleObject && !friendly, "actual fluid damage event");
		++fluid_damage_records;
		fluid_events.push_back(2);
		return;
	}
	if (matcen_trace) {
		require(damage == 4 * F1_0 && before == matcen_before && killer == matcen_actor && !friendly, "actual matcen player damage event");
		++matcen_damage_records;
		matcen_events.push_back(6);
		return;
	}
	require(forcefield_trace && damage == 9 * F1_0 / 2 && before == forcefield_before && killer == ConsoleObject && !friendly, "actual forcefield damage event");
	++forcefield_damage_records;
	forcefield_events.push_back(7);
}
int input_demo_trace_collision_pose_active(void)
{
	if (self_splash_trace) return 0;
	require(contact_trace || forcefield_trace || matcen_trace || fluid_trace, "unexpected collision trace query");
	return 0;
}
unsigned input_demo_trace_collision_frame_index(void)
{
	require(false, "unexpected player trace frame");
	return 0;
}
const char *input_demo_trace_collision_mode_name(void)
{
	require(false, "unexpected player trace mode");
	return "";
}
void input_demo_log_player_damage_probe(const char *, unsigned, int, int32_t, int32_t, int, int, int, int, int, int)
{
	require(false, "unexpected player trace damage");
}
void PALETTE_FLASH_ADD(int r, int g, int b)
{
	if (self_splash_trace) {
		require(r == (self_splash_damage / F1_0) * 4 && g == -(self_splash_damage / 2 / F1_0) && b == g, "self splash actual damage palette");
		++self_splash_palettes;
		self_splash_events.push_back(7);
		return;
	}
	if (fluid_trace) {
		require((r == 8 && g == -1 && b == -1) || (r == 8 && g == 0 && b == 0), "exact fluid damage and scrape flashes");
		fluid_palettes.push_back({ r, g, b });
		fluid_events.push_back(g ? 3 : 6);
		return;
	}
	if (matcen_trace) {
		require(r == 16 && g == -2 && b == -2, "matcen damage palette request");
		++matcen_palettes;
		matcen_events.push_back(7);
		return;
	}
	require(forcefield_trace && ((r == 0 && g == 0 && b == 60) || (r == 16 && g == -2 && b == -2)), "forcefield exact blue/red palette request");
	forcefield_palettes.push_back({ r, g, b });
	forcefield_events.push_back(r ? 8 : 1);
}
void reset_auto_select(void)
{
	require(false, "unexpected player autoselect");
}

int game_is_time_paused(void)
{
	require(countdown_trace || seismic_trace, "unexpected gameplay pause query");
	return countdown_time_paused;
}
void digi_play_sample(int sound, fix volume)
{
	require(countdown_trace, "unexpected countdown sample");
	countdown_samples.emplace_back(sound, volume);
}
void xl_camera_flash_palette_set(void)
{
	require(countdown_trace && PaletteRedAdd == PaletteGreenAdd && PaletteGreenAdd == PaletteBlueAdd, "countdown palette dispatch");
	++countdown_flashes;
	countdown_flash_value = PaletteBlueAdd;
}
void gr_set_current_canvas(grs_canvas *canvas)
{
	require(countdown_trace && !canvas, "countdown expiry canvas");
	++countdown_canvas;
}
int gr_find_closest_color(int r, int g, int b)
{
	require(countdown_trace && r == 62 && g == 62 && b == 62, "countdown expiry color");
	return 19;
}
void gr_clear_canvas(int color)
{
	require(countdown_trace && color == 19, "countdown expiry clear color");
	++countdown_clears;
}
void reset_palette_add(void)
{
	require(countdown_trace, "countdown expiry reset palette");
	++countdown_resets;
	PaletteRedAdd = PaletteGreenAdd = PaletteBlueAdd = 0;
}
int input_demo_finish_replay_from_mine_exit(void)
{
	require(countdown_trace, "countdown replay-exit boundary");
	++countdown_replay_calls;
	return countdown_replay_finish;
}
void DoPlayerDead(void)
{
	require(countdown_trace, "countdown death boundary");
	++countdown_deaths;
}

int xl_message_action(const trigger *)
{
	require(false, "unexpected message");
	return 0;
}
int xl_named_audio_action(trigger *action, int key)
{
	if (master_trace) {
		require(action == &Triggers[1] && key == 1 && action->type == 30 && action->flags == 3 && action->value == 2 * F1_0 && action->time == 1 && !action->num_links, "master named output exact action key and fields");
		const auto state = xl_compat::seismic_gameplay_snapshot();
		require(state[0] == (master_reverse ? 3 * F1_0 : 0) && state[1] == (master_reverse ? 3 * F1_0 : 0) && state[2].is_null(), "named callback observes actual child execution order before any seismic interval");
		++master_named;
		return 0;
	}
	require(reactor_trace && key == DXX_XL_MAX_TRIGGERS && action->type == 30 && action->value == 17 * F1_0 && action->time == 3 && !action->num_links && action->flags == TF_ONE_SHOT, "authored one-shot reactor named sound dispatch");
	++reactor_named;
	return 0;
}
int xl_trigger_texture_action(const trigger *)
{
	require(false, "unexpected texture action");
	return 0;
}

void xl_gameplay_seismic_begin(void)
{
	require(seismic_trace, "seismic audio scope begin");
	++seismic_begins;
}
void xl_gameplay_seismic_end(void)
{
	require(seismic_trace, "seismic audio scope end");
	++seismic_ends;
}
void digi_play_sample_looping(int sound, fix volume, int start, int end)
{
	require(seismic_trace && sound == 42 && volume == F1_0 && start == -1 && end == -1, "seismic loop start");
	seismic_audio.emplace_back(1, volume);
}
void digi_stop_looping_sound(void)
{
	require(seismic_trace, "seismic loop stop");
	seismic_audio.emplace_back(2, 0);
}
void digi_change_looping_volume(fix volume)
{
	require(seismic_trace && volume == 2048, "seismic loop volume");
	seismic_audio.emplace_back(3, volume);
}

void xl_named_audio_frame(void)
{
	require(disarm_trace, "unexpected named-audio frame");
	++disarm_named_frames;
}
void xl_message_frame(void)
{
	require(disarm_trace, "unexpected message frame");
	++disarm_message_frames;
}
void xl_named_audio_wall_frame(int index)
{
	require(disarm_trace && index == 0, "unexpected named-audio wall frame");
	++disarm_named_wall_frames;
}

void xl_preview_trigger_result(int index, int shot, int result)
{
	if (master_forcefield_trace) {
		require(index >= 0 && index < 3 && !shot && (result == 0 || result == 1), "forcefield master diagnostic output");
		return;
	}
	if (master_trace) {
		require(index >= 0 && index < 3 && !shot && (result == 0 || result == 1), "master bridge result fields");
		master_results.push_back({ index, shot, result });
		return;
	}
	if (boost_trace) {
		require(index == 0 && !shot && !result, "boost dispatcher result");
		return;
	}
	if (checkpoint_trace) {
		require(index == 0 && !shot && !result, "checkpoint dispatcher result");
		return;
	}
	require(disarm_trace && index == 0 && !shot && !result, "actual autoplay dispatch result");
	++disarm_results;
}

int xl_source_level_version(void)
{
	require(false, "unexpected mission version");
	return 0;
}
int xl_mission_progress_state(int)
{
	require(false, "unexpected mission progress");
	return 0;
}
void digi_stop_digi_sounds(void)
{
	require(false, "unexpected mission audio stop");
}

int xl_authored_close_door(int, int)
{
	require(false, "unused stock close-door path");
	return 0;
}
void wall_close_door(segment *, int)
{
	require(false, "unused stock door mechanics");
}
int add_light(int, int)
{
	require(false, "unused stock light path");
	return 0;
}
int subtract_light(int, int)
{
	require(false, "unused stock light path");
	return 0;
}
void enable_flicker(int, int)
{
	require(false, "unused stock flicker path");
}
void disable_flicker(int, int)
{
	require(false, "unused stock flicker path");
}
int find_connect_side(segment *base, segment *other)
{
	require(master_forcefield_trace && ((base == &Segments[0] && other == &Segments[1]) || (base == &Segments[1] && other == &Segments[0])) && Segments[0].children[4] == 1 && Segments[1].children[5] == 0, "declared exact reciprocal synthetic segment pair");
	++master_forcefield_connections;
	return base == &Segments[0] ? 5 : 4;
}
void digi_kill_sound_linked_to_segment(int seg, int side, int sound)
{
	require(master_forcefield_trace && seg >= 0 && seg <= 1 && side == (seg ? 5 : 4) && sound == SOUND_FORCEFIELD_HUM, "forcefield hum-stop exact source and side");
	master_forcefield_events.push_back({ 3, seg, Walls[0].type, Walls[1].type });
}
void start_wall_cloak(segment *, int)
{
	require(false, "unused stock wall cloak");
}
void start_wall_decloak(segment *, int)
{
	require(false, "unused stock wall decloak");
}
void kill_stuck_objects(int wall)
{
	require(master_forcefield_trace && wall >= 0 && wall <= 1, "forcefield stuck-cleanup exact wall");
	master_forcefield_events.push_back({ 4, wall, Walls[0].type, Walls[1].type });
}
int xl_equipment_trigger(int)
{
	require(false, "unused stock equipment trigger");
	return 0;
}
void trigger_matcen(int)
{
	require(false, "unused stock matcen trigger");
}
int xl_trigger_illusion_action(const trigger *)
{
	require(false, "unused stock illusion action");
	return 0;
}
void wall_illusion_on(segment *, int)
{
	require(false, "unused stock illusion mechanics");
}
void wall_illusion_off(segment *, int)
{
	require(false, "unused stock illusion mechanics");
}
int input_demo_replay_homing_desync_probe_active(void)
{
	require(master_forcefield_trace, "only forcefield stock instrumentation is disabled");
	return 0;
}
void input_demo_append_replay_probe_message(const char *, object *, const char *)
{
	require(false, "unused stock trigger probe output");
}

int p_secret_level_destroyed(void)
{
	require(false, "unused secret-level decision");
	return 0;
}
void newdemo_record_secret_exit_blown(int)
{
	require(false, "unused secret demo output");
}
int newdemo_stop_quick_recording(void)
{
	require(false, "unused demo control");
	return 0;
}
void EnterSecretLevel(void)
{
	require(false, "unused secret exit");
}

void multi_send_ship_status(void)
{
	require(false, "unused D1 network status");
}
void key_flush(void)
{
	require(false, "unused D1 progression input reset");
}
void newdemo_stop_recording(int)
{
	require(false, "unused D1 demo progression");
}
void do_end_briefing_screens(char *)
{
	require(false, "unused D1 briefing");
}
void con_printf(int priority, const char *format, ...)
{
	if (self_splash_trace) {
		char actual[256], expected[256];
		va_list args;
		va_start(args, format);
		std::vsnprintf(actual, sizeof(actual), format, args);
		va_end(args);
		if (self_splash_killer) std::snprintf(expected, sizeof(expected), "You took %.1f damage from %s's concussion blast, shields now %.1f\n", double(self_splash_damage) / F1_0, Players[self_splash_killer->id].callsign, 100.0 - double(self_splash_damage) / F1_0);
		else std::snprintf(expected, sizeof(expected), "You took %.1f damage from a unknown blast, shields now %.1f\n", double(self_splash_damage) / F1_0, 100.0 - double(self_splash_damage) / F1_0);
		require(priority == CON_NORMAL && !std::strcmp(actual, expected), "self splash exact formatted damage telemetry");
		++self_splash_console;
		return;
	}
	require(false, "unused D1 progression console");
}
void StartNewLevel(int)
{
	require(false, "unused D1 level transition");
}
void DoEndLevelScoreGlitz(int)
{
	require(false, "unused D1 score UI");
}
void window_set_visible(window *, int)
{
	require(false, "unused D1 window state");
}
int window_close(window *)
{
	require(false, "unused D1 window teardown");
	return 0;
}
char *d1_in_d2_menu_resource(enum d1_menu_resource, const char *)
{
	require(false, "unused D1 menu resource");
	return nullptr;
}
int newmenu_do2(char *, char *, int, newmenu_item *, int (*)(newmenu *, d_event *, void *), void *, int, char *)
{
	require(false, "unused D1 menu");
	return 0;
}
void scores_maybe_add_player(int)
{
	require(false, "unused D1 score publication");
}
void reset_time(void)
{
	require(false, "unused D1 progression time reset");
}

void start_endlevel_sequence(void)
{
	require(false, "unexpected end-level sequence");
}
void ExitSecretLevel(void)
{
	require(false, "unexpected secret exit");
}
int xl_trigger_texture_valid(const trigger *)
{
	require(false, "unexpected texture validation");
	return 0;
}
int xl_player_resource_trigger(const trigger *)
{
	require(false, "unexpected player-resource trigger");
	return 0;
}

int HUD_init_message_literal(int cls, const char *text)
{
	if (checkpoint_trace) {
		require(cls == HM_DEFAULT && !std::strcmp(text, "New spawn points set!"), "checkpoint HUD output");
		++checkpoint_messages;
		return 0;
	}
	require(false, "unexpected teleport message");
	return 0;
}

int boss_hud_note_weapon_collision(const object *boss, const object *weapon)
{
	require(contact_trace && boss == &Objects[owner_a] && weapon == &Objects[4], "boss contact HUD boundary");
	++contact_hud;
	return -1;
}
void create_awareness_event(object *weapon, int type)
{
	if (forcefield_trace) {
		require(weapon == ConsoleObject && type == PA_WEAPON_WALL_COLLISION, "forcefield wall-awareness boundary");
		++forcefield_awareness;
		forcefield_events.push_back(5);
		return;
	}
	require(contact_trace && weapon == &Objects[4] && type == PA_WEAPON_ROBOT_COLLISION, "local contact awareness boundary");
	++contact_awareness;
}
void do_ai_robot_hit(object *boss, int type)
{
	require(contact_trace && boss == &Objects[owner_a] && type == PA_WEAPON_ROBOT_COLLISION, "local contact AI notification");
	++contact_ai_hits;
}
void input_demo_record_robot_impact_event(object *weapon, object *boss)
{
	require(contact_trace && weapon == &Objects[4] && boss == &Objects[owner_a], "contact impact record");
	++contact_impact_records;
}
void input_demo_set_awareness_source(const char *tag, int source, int target)
{
	if (forcefield_trace) {
		require(!std::strcmp(tag, "collide_player_wall") && source == player_slot && target == 0, "forcefield awareness source");
		forcefield_events.push_back(4);
		return;
	}
	require(contact_trace && !std::strcmp(tag, "collide_weapon_robot") && source == 4 && target == owner_a, "contact awareness source");
}
void input_demo_log_replay_collision_pair(const char *kind, object *boss, object *weapon, const vms_vector *point)
{
	require(contact_trace && !std::strcmp(kind, "collide_robot_weapon_entry") && boss == &Objects[owner_a] && weapon == &Objects[4] && point == contact_point, "actual contact entry pair");
}
void input_demo_log_replay_weapon_robot_resolution(const char *, object *weapon, object *boss, const vms_vector *point, int32_t damage, int32_t old, int died)
{
	require(contact_trace && weapon == &Objects[4] && boss == &Objects[owner_a] && point == contact_point && damage == contact_requested && old == contact_before && !died, "actual contact damage resolution fields");
	++contact_resolutions;
}
void input_demo_log_weapon_robot_path_event(const char *, object *weapon, object *boss, const vms_vector *point, int, int, int, object *effect)
{
	require(contact_trace && weapon == &Objects[4] && boss == &Objects[owner_a] && point == contact_point && !effect, "actual contact instrumentation pair");
}
void input_demo_trace_robot_damage(object *boss, object *weapon, int old, int damage, int died)
{
	require(contact_trace && boss == &Objects[owner_a] && weapon == &Objects[4] && old == contact_before && damage == contact_requested && !died, "actual contact damage trace");
}
int xl_static_robot_bump(object *boss, object *weapon)
{
	if (separating_trace) {
		require(boss == separating_first && weapon == separating_second && boss != weapon, "exact ordinary robot pair bridge");
		for (auto *actor : { boss, weapon }) require(actor->type == OBJ_ROBOT && actor->movement_type == MT_PHYSICS && actor->mtype.phys_info.mass == F1_0 && actor->control_type == CT_AI && actor->ctype.ai_info.behavior == AIB_NORMAL && !Robot_info[actor->id].boss_flag && !Robot_info[actor->id].companion && !Robot_info[actor->id].thief, "nonstatic nonboss ordinary pair only");
		separating_events.push_back({ 0, int(boss - Objects), int(weapon - Objects), 0, 0 });
		return 0;
	}
	if (gauss_trace) {
		require(contact_trace && boss == &Objects[owner_a] && weapon == &Objects[4] && boss->type == OBJ_ROBOT && weapon->type == OBJ_WEAPON && (weapon->id == GAUSS_ID || weapon->id == 0) &&
		            boss->ctype.ai_info.behavior != 135 && weapon->ctype.ai_info.behavior != 135 && boss->movement_type == MT_PHYSICS && weapon->movement_type == MT_PHYSICS &&
		            boss->mtype.phys_info.mass == F1_0 && weapon->mtype.phys_info.mass == F1_0 && !boss->mtype.phys_info.velocity.x && !boss->mtype.phys_info.velocity.y && !boss->mtype.phys_info.velocity.z &&
		            !weapon->mtype.phys_info.velocity.x && !weapon->mtype.phys_info.velocity.y && !weapon->mtype.phys_info.velocity.z,
		        "Gauss ordinary zero-velocity pair excludes static bridge");
		++contact_static;
		return 0;
	}
	require(contact_trace && boss == &Objects[owner_a] && weapon == &Objects[4] && boss->type == OBJ_ROBOT && weapon->type == OBJ_WEAPON && weapon->id == 0 && Robot_info[boss->id].boss_flag == 25 && !Robot_info[boss->id].energy_blobs && !Weapon_info[0].damage_radius && boss->ctype.ai_info.behavior != 135 && weapon->ctype.ai_info.behavior != 135 && boss->movement_type == MT_PHYSICS && weapon->movement_type == MT_PHYSICS && boss->mtype.phys_info.mass > 0 && weapon->mtype.phys_info.mass > 0 && !boss->mtype.phys_info.velocity.x && !boss->mtype.phys_info.velocity.y && !boss->mtype.phys_info.velocity.z && !weapon->mtype.phys_info.velocity.x && !weapon->mtype.phys_info.velocity.y && !weapon->mtype.phys_info.velocity.z, "ordinary zero-velocity pair excludes static bridge");
	++contact_static;
	return 0;
}

int xl_nonphysical_bump_reflection(object *, vms_vector *, vms_vector *)
{
	require(false, "unexpected nonphysical reflection");
	return 0;
}
void input_demo_log_player_bump_probe(const char *stage, object *boss, object *weapon, const vms_vector *relative, const vms_vector *force, int32_t num, int32_t den, int damage)
{
	if (separating_trace) {
		require(boss == separating_first && weapon == separating_second && num == 2 * F1_0 && den == 2 * F1_0 && damage == 1 && !std::memcmp(relative, &separating_force, sizeof(separating_force)) && !std::memcmp(force, &separating_force, sizeof(separating_force)) && (!std::strcmp(stage, "pre") || !std::strcmp(stage, "post")), "real physical pair pre/post force scaling");
		separating_events.push_back({ !std::strcmp(stage, "pre") ? 1 : 5, int(boss - Objects), int(weapon - Objects), 0, 0 });
		return;
	}
	require(contact_trace && boss == &Objects[owner_a] && weapon == &Objects[4] && !relative->x && !relative->y && !relative->z && !force->x && !force->y && !force->z && num == 2 * F1_0 && den == 2 * F1_0 && !damage, "actual zero-force physical bump trace");
}
void input_demo_debug_log_weapon_robot_path_probe(const char *, void *weapon, void *boss, const void *point)
{
	require(contact_trace && weapon == &Objects[4] && boss == &Objects[owner_a] && point == contact_point, "contact path instrumentation");
}
void input_demo_debug_log_weapon_robot_reason_probe(const char *, void *weapon, void *boss, const void *point)
{
	require(contact_trace && weapon == &Objects[4] && boss == &Objects[owner_a] && point == contact_point, "contact reason instrumentation");
}
void input_demo_debug_log_weapon_robot_accept_seq(void *weapon, void *boss)
{
	require(contact_trace && weapon == &Objects[4] && boss == &Objects[owner_a], "contact accept instrumentation");
}
void input_demo_debug_log_weapon_robot_collision_pose(const char *, void *weapon, void *boss, void *point, int damage, int old)
{
	require(contact_trace && weapon == &Objects[4] && boss == &Objects[owner_a] && point == contact_point && damage == contact_requested && old == contact_before, "contact pose instrumentation");
}
int ok_to_do_omega_damage(object *)
{
	require(false, "unexpected omega");
	return 0;
}
void smega_rock_stuff_at(const vms_vector *)
{
	require(false, "unexpected earthshaker");
}
int xl_static_robot(const object *)
{
	require(false, "unexpected static energy child branch");
	return 0;
}
int xl_static_robot_gauss(object *actor)
{
	require(gauss_trace && contact_trace && actor == &Objects[owner_a] && actor->type == OBJ_ROBOT && actor->ctype.ai_info.behavior != 135 && actor->movement_type == MT_PHYSICS &&
	            !Robot_info[actor->id].boss_flag && !Robot_info[actor->id].companion,
	        "ordinary Gauss excludes static positive decision");
	++gauss_static_calls;
	return 0;
}
void create_smart_children(object *actor, int count)
{
	if (mine_trace) {
		require(actor == &Objects[2] && (actor->id == PMINE_ID || actor->id == PROXIMITY_ID) && Weapon_info[actor->id].children == -1 && count == NUM_SMART_CHILDREN && (actor->flags & OF_SHOULD_BE_DEAD) && mine_allocations == 2, "authenticated no-child mine endpoint only");
		++mine_children;
		mine_events.push_back({ 8, actor->id, count, 1 });
		return;
	}
	require(false, "unexpected smart children");
}
object *__wrap_object_create_badass_explosion(object *, short, vms_vector *, fix, int, fix, fix, fix, int)
{
	require(false, "unexpected splash explosion");
	return nullptr;
}
object *__wrap_explode_badass_weapon(object *, vms_vector *)
{
	require(false, "unexpected splash weapon");
	return nullptr;
}
void input_demo_log_collision_weapon_robot_impact(const char *, unsigned, object *, object *)
{
	require(false, "trace disabled");
}
void obj_attach(object *, object *)
{
	require(false, "unexpected attached effect");
}
void detect_escort_goal_accomplished(int)
{
	require(false, "unexpected escort kill goal");
}

void escort_note_boss_teleported(int owner)
{
	require(teleport_trace && owner == owner_a, "boss teleport escort boundary");
	teleport_outputs.push_back(1);
}
void digi_kill_sound_linked_to_object(int owner)
{
	require(teleport_trace && owner == owner_a, "boss teleport kill old sound");
	teleport_outputs.push_back(3);
}

bool is_observer(void)
{
	if (matcen_trace) return false;
	require(forcefield_trace, "bounded forcefield observer input");
	return forcefield_observer;
}
int wall_hit_process(segment *, int, fix, int, object *)
{
	require(false, "nonforcefield wall branch excluded");
	return 0;
}
int input_demo_replay_collision_probe_active(void)
{
	require(forcefield_trace, "forcefield trace-disabled input");
	return 0;
}
void input_demo_log_replay_player_wall_collision(object *, int, int, fix, const vms_vector *)
{
	require(false, "forcefield replay tracing disabled");
}
void input_demo_log_collision_player_wall_impact(const char *, unsigned, object *, int, int, int, int, int)
{
	require(false, "forcefield pose tracing disabled");
}

void compute_center_point_on_side(vms_vector *center, segment *seg, int side)
{
	if (master_forcefield_trace) {
		require((seg == &Segments[0] && side == 4) || (seg == &Segments[1] && side == 5), "declared synthetic forcefield side center");
		++master_forcefield_centers;
		*center = seg == &Segments[0] ? vms_vector{ 11, 22, 33 } : vms_vector{ 44, 55, 66 };
		return;
	}
	require(matcen_trace && seg == &Segments[0] && side >= 0 && side < matcen_exits, "synthetic matcen side-center input");
	const vms_vector offset = matcen_axis ? vms_vector{ 5 * F1_0, 0, 0 } : side == 0 ? vms_vector{ 3 * F1_0, 4 * F1_0, 0 }
	                                                                                 : vms_vector{ 0, 10 * F1_0, 0 };
	*center = { matcen_actor->pos.x + offset.x, matcen_actor->pos.y + offset.y, matcen_actor->pos.z + offset.z };
	matcen_center_sides.push_back(side);
	matcen_events.push_back(3);
}
void make_random_vector(vms_vector *)
{
	require(false, "classic local matcen random branch excluded");
}

int xl_ambient_object_set_enabled(int target, int enabled)
{
	if (master_forcefield_trace) {
		require(target == 4 && !enabled && Objects[4].type == 20 && Objects[4].id == 2 && Objects[4].render_type == 13 && Walls[0].type == master_forcefield_initial[0] && Walls[1].type == master_forcefield_initial[1], "ambient forcefield disable dispatch before any wall change");
		master_forcefield_events.push_back({ 1, target, Walls[0].type, Walls[1].type });
		return 1;
	}
	require(reactor_trace && (target == 6 || target == 7) && !enabled, "reactor effect dispatch");
	reactor_effect_calls.push_back(target * 10 + 0);
	return reactor_backend == 0;
}
int xl_particle_object_set_enabled(int target, int enabled)
{
	require(reactor_trace && (target == 6 || target == 7) && !enabled, "reactor effect dispatch");
	reactor_effect_calls.push_back(target * 10 + 1);
	return reactor_backend == 1;
}
int xl_lightning_object_set_enabled(int target, int enabled)
{
	require(reactor_trace && (target == 6 || target == 7) && !enabled, "reactor effect dispatch");
	reactor_effect_calls.push_back(target * 10 + 2);
	return reactor_backend == 2;
}
}

extern "C" int object_to_object_visibility(object *effect, object *target, int flags)
{
	if (mine_trace) {
		require((effect == &Objects[5] || effect == &Objects[6] || effect == &Objects[7]) && (target == &Objects[2] || target == &Objects[owner_a]) && flags == 2, "exact nested visibility output boundary");
		mine_events.push_back({ 2, int(effect - Objects), int(target - Objects), int(bool(Objects[2].flags & OF_SHOULD_BE_DEAD)) });
		return target == &Objects[2] ? !mine_blocked : 1;
	}
	if (self_splash_trace) {
		require(effect == &Objects[4] && target == ConsoleObject && flags == 2, "self splash visibility");
		++self_splash_visibility;
		self_splash_events.push_back(1);
		return self_splash_visible;
	}
	require(splash_trace && effect == &Objects[4] && target == &Objects[owner_a] && flags == 2, "splash visibility boundary");
	++splash_visibility;
	return splash_visible;
}
extern "C" {
void multi_send_damage(fix damage, fix shields, ubyte type, ubyte id, ubyte kind, object *source)
{
	if (self_splash_trace) {
		require(damage == self_splash_damage && shields == 100 * F1_0 && type == (self_splash_killer ? self_splash_killer->type : OBJ_WEAPON) && id == (self_splash_killer ? self_splash_killer->id : 0) && kind == DAMAGE_BLAST && source == &Objects[2], "self splash pre-damage telemetry");
		++self_splash_telemetry;
		self_splash_events.push_back(4);
		return;
	}
	require(false, "splash excludes player network damage");
}
int input_demo_recorder_is_active()
{
	if (self_splash_trace) return 0;
	require(false, "splash excludes player recording");
	return 0;
}
int input_demo_replay_is_loaded()
{
	if (self_splash_trace) return 0;
	require(false, "splash excludes player replay");
	return 0;
}
const char *input_demo_debug_activity_mode_name()
{
	require(false, "splash excludes player tracing");
	return "";
}
unsigned input_demo_debug_frame_index()
{
	require(false, "splash excludes player tracing");
	return 0;
}
void input_demo_log_player_weapon_hit(const char *, unsigned, object *, object *, int, const vms_vector *)
{
	require(false, "splash excludes player hit tracing");
}
int xl_gameplay_weapon_explosion(int owner, int water)
{
	if (mine_trace) {
		require(owner == 2 && !water && (Objects[2].flags & OF_SHOULD_BE_DEAD) && mine_allocations == 1, "dead mine before audio and nested explosion");
		++mine_audio;
		mine_events.push_back({ 4, 2, 0, 1 });
		return extensions ? 1 : 0;
	}
	require(false, "splash excludes weapon audio policy");
	return 0;
}
int digi_link_sound_to_object(int sound, short owner, int loop, fix volume)
{
	if (mine_trace) {
		require(sound == 11 && owner == 2 && !loop && volume == F1_0 && !extensions && mine_audio == 1 && mine_allocations == 1 && (Objects[2].flags & OF_SHOULD_BE_DEAD), "classic mine sound before nested allocation");
		++mine_classic_audio;
		mine_events.push_back({ 5, owner, sound, 1 });
		return 0;
	}
	require(false, "splash excludes weapon audio output");
	return 0;
}
}
