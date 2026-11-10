#include "../camera_contact.h"
extern "C" {
#include "automap.h"
#include "cntrlcen.h"
void bump_two_objects(object *, object *, int);
}

// Controlled native dispatch, not multiplayer synchronization
static void ordinary_nonphysical_bump_controls(bool xl, bool protected_profile)
{
	ScopedNetworkTest network;
	const auto saved_physics = ConsoleObject->mtype.phys_info;
	const auto saved_movement = ConsoleObject->movement_type;
	const fix saved_shields = ConsoleObject->shields, saved_player_shields = Players[Player_num].shields;
	const int saved_mode = Game_mode, saved_center_hit = Control_center_been_hit, saved_automap = Automap_active;
	const auto saved_delay = collide_get_collision_delay_last_play_time();
	unsigned saved_rng = 0;
	d_rand_get_state(&saved_rng);
	Automap_active = 0;
	for (const int type : { OBJ_CNTRLCEN, OBJ_CLUTTER }) {
		const int stationary = obj_create(static_cast<object_type_t>(type), 0, ConsoleObject->segnum, &ConsoleObject->pos,
		                                  &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE);
		require(stationary >= 0, "cannot allocate stationary contact control");
		auto &fixed = Objects[stationary];
		fixed.shields = i2f(1000);
		for (const bool multiplayer : { false, true })
			for (const bool reverse : { false, true })
				for (const fix mass : { 0, 1, 3, 32768, 65536, 98317, 262144 })
					for (const fix speed : { 0, 17, -65537, 100001 }) {
						Game_mode = multiplayer ? GM_NETWORK : 0;
						ConsoleObject->movement_type = MT_PHYSICS;
						ConsoleObject->mtype.phys_info = {};
						ConsoleObject->mtype.phys_info.mass = mass;
						const vms_vector before{ speed, speed / 3, -speed };
						ConsoleObject->mtype.phys_info.velocity = before;
						ConsoleObject->shields = Players[Player_num].shields = i2f(1000);
						const bool protected_clutter = type == OBJ_CLUTTER && protected_profile && xl_preview_no_damage_segment(ConsoleObject->segnum);
						std::array<std::int32_t, 3> expected{ before.x, before.y, before.z };
						if (!protected_clutter) {
							if (xl && !multiplayer) expected = xl_compat::camera_reflect_velocity(expected, mass, false);
							else if (mass)
								for (auto &axis : expected) axis += fixmul(fixmul(axis, -mass), fixdiv(F1_0, mass));
						}
						collide_set_collision_delay_last_play_time(GameTime64 - i2f(2));
						unsigned before_rng = 0, after_rng = 0, expected_rng = 0;
						d_rand_get_state(&before_rng);
						const auto before_calls = d_rand_get_call_count();
						fix64 expected_delay = GameTime64 - i2f(2);
						if (!protected_clutter) expected_delay = GameTime64 - d_rand() / 2;
						d_rand_get_state(&expected_rng);
						d_rand_set_state(before_rng);
						d_rand_set_call_count(before_calls);
						auto hit = ConsoleObject->pos;
						collide_two_objects(reverse ? &fixed : ConsoleObject, reverse ? ConsoleObject : &fixed, &hit);
						d_rand_get_state(&after_rng);
						const auto &actual = ConsoleObject->mtype.phys_info.velocity;
						require(actual.x == expected[0] && actual.y == expected[1] && actual.z == expected[2] &&
						            fixed.mtype.phys_info.velocity.x == 0 && fixed.mtype.phys_info.velocity.y == 0 && fixed.mtype.phys_info.velocity.z == 0,
						        "ordinary stationary contact velocity differs from its source profile");
						require(ConsoleObject->shields == i2f(1000) && Players[Player_num].shields == i2f(1000) &&
						            fixed.shields == i2f(1000) && expected_rng == after_rng &&
						            d_rand_get_call_count() - before_calls == unsigned(protected_clutter ? 0 : 1),
						        "stationary contact changed damage or simulation RNG");
						require(collide_get_collision_delay_last_play_time() == expected_delay,
						        "stationary contact changed protected or ordinary sound-gate dispatch");
					}
		obj_delete(stationary);
	}
	ConsoleObject->mtype.phys_info = saved_physics;
	ConsoleObject->movement_type = saved_movement;
	ConsoleObject->shields = saved_shields;
	Players[Player_num].shields = saved_player_shields;
	Game_mode = saved_mode;
	Control_center_been_hit = saved_center_hit;
	Automap_active = saved_automap;
	collide_set_collision_delay_last_play_time(saved_delay);
	d_rand_set_state(saved_rng);
	std::cout << "Ordinary nonphysical reactor/clutter controls passed; XL=" << xl << " protected=" << protected_profile << "\n";
}

// Equal masses and a persistent counterpart isolate the real player force-damage consumer
static void ordinary_force_damage_controls(bool xl, bool protected_profile)
{
	ScopedNetworkTest network;
	const auto saved_physics = ConsoleObject->mtype.phys_info;
	const auto saved_movement = ConsoleObject->movement_type;
	const auto saved_object_flags = ConsoleObject->flags;
	const fix saved_object_shields = ConsoleObject->shields, saved_player_shields = Players[Player_num].shields;
	const auto saved_player_flags = Players[Player_num].flags;
	const int saved_mode = Game_mode, saved_difficulty = Difficulty_level, saved_dead = Player_is_dead, saved_endlevel = Endlevel_sequence;
	const auto saved_delay = collide_get_collision_delay_last_play_time();
	unsigned saved_rng = 0;
	d_rand_get_state(&saved_rng);
	const auto saved_calls = d_rand_get_call_count();
	const int other_index = obj_create(OBJ_CLUTTER, 0, ConsoleObject->segnum, &ConsoleObject->pos,
	                                   &vmd_identity_matrix, F1_0, CT_NONE, MT_PHYSICS, RT_NONE);
	require(other_index >= 0, "cannot allocate force-damage contact control");
	auto &other = Objects[other_index];
	other.mtype.phys_info = {};
	other.mtype.phys_info.mass = F1_0;
	other.mtype.phys_info.flags = PF_PERSISTENT;
	other.shields = i2f(1000);
	for (const bool multiplayer : { false, true })
		for (const bool reverse : { false, true })
			for (const int difficulty : { 0, 1 })
				for (const int direction : { -1, 1 })
					for (const fix raw_damage : { 0, 1, 16384, 21844, 21845, 21846 }) {
						Game_mode = multiplayer ? GM_NETWORK : 0;
						Difficulty_level = difficulty;
						Player_is_dead = Endlevel_sequence = 0;
						Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
						ConsoleObject->flags &= ~(OF_EXPLODING | OF_SHOULD_BE_DEAD);
						ConsoleObject->movement_type = MT_PHYSICS;
						ConsoleObject->mtype.phys_info = {};
						ConsoleObject->mtype.phys_info.mass = F1_0;
						const fix speed = direction * raw_damage * 32;
						ConsoleObject->mtype.phys_info.velocity = { speed, 0, 0 };
						const fix real_shields = i2f(100), object_sentinel = i2f(101);
						Players[Player_num].shields = real_shields;
						ConsoleObject->shields = object_sentinel;
						const bool protected_contact = protected_profile && xl_preview_no_damage_segment(ConsoleObject->segnum);
						const bool dispatched = !protected_contact && ((xl && !multiplayer) || raw_damage >= F1_0 / 3);
						const fix damage = dispatched ? (difficulty == 0 ? raw_damage / 2 : raw_damage) : 0;
						collide_set_collision_delay_last_play_time(GameTime64 - i2f(2));
						unsigned prior_rng = 0, expected_rng = 0, actual_rng = 0;
						d_rand_get_state(&prior_rng);
						const auto prior_calls = d_rand_get_call_count();
						fix64 expected_delay = GameTime64 - i2f(2);
						if (!protected_contact) expected_delay = GameTime64 - d_rand() / 2;
						d_rand_get_state(&expected_rng);
						d_rand_set_state(prior_rng);
						d_rand_set_call_count(prior_calls);
						auto point = ConsoleObject->pos;
						collide_two_objects(reverse ? &other : ConsoleObject, reverse ? ConsoleObject : &other, &point);
						d_rand_get_state(&actual_rng);
						require(Players[Player_num].shields == real_shields - damage &&
						            ConsoleObject->shields == (dispatched ? real_shields - damage : object_sentinel),
						        "ordinary force-damage cutoff, zero dispatch or trainee scaling differs");
						const fix expected_speed = protected_contact ? speed : direction * raw_damage * 24;
						require(ConsoleObject->mtype.phys_info.velocity.x == expected_speed &&
						            other.mtype.phys_info.velocity.x == 0 && other.shields == i2f(1000),
						        "force-damage threshold changed collision response or persistent counterpart");
						require(actual_rng == expected_rng && d_rand_get_call_count() - prior_calls == unsigned(protected_contact ? 0 : 1) &&
						            collide_get_collision_delay_last_play_time() == expected_delay,
						        "force-damage threshold changed collision sound RNG or gate");
					}
	obj_delete(other_index);
	ConsoleObject->mtype.phys_info = saved_physics;
	ConsoleObject->movement_type = saved_movement;
	ConsoleObject->flags = saved_object_flags;
	ConsoleObject->shields = saved_object_shields;
	Players[Player_num].shields = saved_player_shields;
	Players[Player_num].flags = saved_player_flags;
	Game_mode = saved_mode;
	Difficulty_level = saved_difficulty;
	Player_is_dead = saved_dead;
	Endlevel_sequence = saved_endlevel;
	collide_set_collision_delay_last_play_time(saved_delay);
	d_rand_set_state(saved_rng);
	d_rand_set_call_count(saved_calls);
	std::cout << "Actual force-damage boundaries, zero dispatch, trainee/protection and classic/multiplayer controls passed; XL=" << xl << "\n";
}

// Only robots inherit robot-definition boss immunity in XL singleplayer
static void ordinary_bump_boss_scope_controls(bool xl, bool protected_profile)
{
	ScopedNetworkTest network;
	const auto saved_player = *ConsoleObject;
	const auto saved_player_state = Players[Player_num];
	const auto saved_robot = Robot_info[0];
	const auto saved_cheats = cheats;
	const auto saved_frame = FrameTime;
	const int saved_mode = Game_mode, saved_difficulty = Difficulty_level, saved_dead = Player_is_dead, saved_endlevel = Endlevel_sequence;
	const auto saved_delay = collide_get_collision_delay_last_play_time();
	unsigned saved_rng = 0;
	d_rand_get_state(&saved_rng);
	const auto saved_calls = d_rand_get_call_count();
	const int index = obj_create(OBJ_CLUTTER, 0, ConsoleObject->segnum, &ConsoleObject->pos,
	                             &vmd_identity_matrix, F1_0, CT_NONE, MT_PHYSICS, RT_NONE);
	require(index >= 0, "cannot allocate boss-scope clutter control");
	auto &other = Objects[index];
	FrameTime = F1_0 / 60;
	cheats.monsterdamage = 0;
	for (const bool multiplayer : { false, true })
		for (const bool reverse : { false, true })
			for (const bool persistent : { false, true })
				for (const int difficulty : { 0, 1, 4 })
					for (const int direction : { -1, 1 }) {
						physics_info baseline{};
						fix baseline_shields = 0, baseline_player_shields = 0;
						unsigned baseline_rng = 0;
						for (const bool boss_definition : { false, true }) {
							Game_mode = multiplayer ? GM_NETWORK : 0;
							Difficulty_level = difficulty;
							Player_is_dead = Endlevel_sequence = 0;
							Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
							ConsoleObject->flags &= ~(OF_EXPLODING | OF_SHOULD_BE_DEAD);
							ConsoleObject->movement_type = MT_PHYSICS;
							ConsoleObject->mtype.phys_info = {};
							ConsoleObject->mtype.phys_info.mass = F1_0;
							ConsoleObject->mtype.phys_info.velocity = { direction * i2f(8), i2f(2), -i2f(1) };
							Players[Player_num].shields = ConsoleObject->shields = i2f(100);
							other.flags = 0;
							other.shields = i2f(100);
							other.mtype.phys_info = {};
							other.mtype.phys_info.mass = F1_0;
							if (persistent) other.mtype.phys_info.flags = PF_PERSISTENT;
							Robot_info[0].boss_flag = boss_definition ? 1 : 0;
							collide_set_collision_delay_last_play_time(GameTime64 - i2f(2));
							d_srand(9012);
							d_rand_set_call_count(0);
							const bool protected_contact = protected_profile && xl_preview_no_damage_segment(ConsoleObject->segnum);
							unsigned expected_rng = 0, actual_rng = 0;
							const fix64 expected_delay = protected_contact ? GameTime64 - i2f(2) : GameTime64 - d_rand() / 2;
							d_rand_get_state(&expected_rng);
							d_srand(9012);
							d_rand_set_call_count(0);
							auto point = ConsoleObject->pos;
							collide_two_objects(reverse ? &other : ConsoleObject, reverse ? ConsoleObject : &other, &point);
							d_rand_get_state(&actual_rng);
							const bool suppressed = protected_contact || persistent || (boss_definition && !(xl && !multiplayer));
							require((other.mtype.phys_info.velocity.x == 0) == suppressed &&
							            (other.shields == i2f(100)) == suppressed,
							        "clutter inherited unrelated boss immunity or lost persistent/protected behavior");
							require(actual_rng == expected_rng && d_rand_get_call_count() == unsigned(protected_contact ? 0 : 1) &&
							            collide_get_collision_delay_last_play_time() == expected_delay,
							        "boss-scope change altered collision sound RNG or dispatch");
							if (!boss_definition) {
								baseline = other.mtype.phys_info;
								baseline_shields = other.shields;
								baseline_player_shields = Players[Player_num].shields;
								baseline_rng = actual_rng;
							} else if ((xl && !multiplayer) || protected_contact || persistent) {
								require(!std::memcmp(&baseline, &other.mtype.phys_info, sizeof(baseline)) &&
								            baseline_shields == other.shields && baseline_player_shields == Players[Player_num].shields &&
								            baseline_rng == actual_rng,
								        "unrelated boss definition changed XL clutter force, rotation or damage");
							}
						}
					}
	obj_delete(index);
	// Direct physical-pair consumer: actual bosses and persistent robots remain immovable
	const int robot_index = obj_create(OBJ_ROBOT, 0, ConsoleObject->segnum, &ConsoleObject->pos,
	                                   &vmd_identity_matrix, F1_0, CT_AI, MT_PHYSICS, RT_NONE);
	require(robot_index >= 0, "cannot allocate actual boss bump control");
	auto &robot = Objects[robot_index];
	for (const bool multiplayer : { false, true })
		for (const bool reverse : { false, true })
			for (const bool boss : { false, true })
				for (const bool persistent : { false, true }) {
					Game_mode = multiplayer ? GM_NETWORK : 0;
					Difficulty_level = 1;
					Robot_info[0].boss_flag = boss ? 1 : 0;
					ConsoleObject->mtype.phys_info = {};
					ConsoleObject->mtype.phys_info.mass = F1_0;
					ConsoleObject->mtype.phys_info.velocity = { i2f(8), 0, 0 };
					robot.mtype.phys_info = {};
					robot.mtype.phys_info.mass = F1_0;
					if (persistent) robot.mtype.phys_info.flags = PF_PERSISTENT;
					robot.ctype.ai_info.behavior = AIB_NORMAL;
					robot.ctype.ai_info.SKIP_AI_COUNT = 127; // Keep unrelated stochastic AI scheduling outside this control
					unsigned before_rng = 0, after_rng = 0;
					d_rand_get_state(&before_rng);
					const auto before_calls = d_rand_get_call_count();
					bump_two_objects(reverse ? &robot : ConsoleObject, reverse ? ConsoleObject : &robot, 0);
					d_rand_get_state(&after_rng);
					require((robot.mtype.phys_info.velocity.x == 0) == (boss || persistent),
					        "actual robot boss or persistent bump immunity changed");
					require(before_rng == after_rng && before_calls == d_rand_get_call_count(),
					        "robot boss bump control changed simulation RNG");
				}
	obj_delete(robot_index);
	Robot_info[0] = saved_robot;
	FrameTime = saved_frame;
	cheats = saved_cheats;
	*ConsoleObject = saved_player;
	Players[Player_num] = saved_player_state;
	Game_mode = saved_mode;
	Difficulty_level = saved_difficulty;
	Player_is_dead = saved_dead;
	Endlevel_sequence = saved_endlevel;
	collide_set_collision_delay_last_play_time(saved_delay);
	d_rand_set_state(saved_rng);
	d_rand_set_call_count(saved_calls);
	std::cout << "Actual clutter contact boss-scope controls passed; XL=" << xl << " protected=" << protected_profile << "\n";
}

// Actual dispatch: retain the full impulse but derive XL nonplayer damage from rotation force
static void ordinary_nonplayer_force_damage_controls(bool xl, bool protected_profile)
{
	ScopedNetworkTest network;
	const auto saved_player = *ConsoleObject;
	const auto saved_player_state = Players[Player_num];
	const auto saved_robot = Robot_info[0];
	const auto saved_cheats = cheats;
	const auto saved_time = GameTime64, saved_delay = collide_get_collision_delay_last_play_time();
	const auto saved_frame = FrameTime;
	const int saved_mode = Game_mode, saved_difficulty = Difficulty_level, saved_dead = Player_is_dead, saved_endlevel = Endlevel_sequence;
	const int saved_awareness = Num_awareness_events, saved_agitation = Overall_agitation;
	std::array<awareness_event, MAX_AWARENESS_EVENTS> saved_events;
	std::array<ai_cloak_info, MAX_AI_CLOAK_INFO> saved_cloak;
	std::memcpy(saved_events.data(), Awareness_events, sizeof(Awareness_events));
	std::memcpy(saved_cloak.data(), Ai_cloak_info, sizeof(Ai_cloak_info));
	const auto saved_believed_pos = Believed_player_pos;
	const auto saved_believed_seg = Believed_player_seg;
	unsigned saved_rng = 0;
	d_rand_get_state(&saved_rng);
	const auto saved_calls = d_rand_get_call_count();
	FrameTime = F1_0 / 60;
	cheats.monsterdamage = 0;
	cheats.robotfiringsuspended = 1;
	Robot_info[0].strength = i2f(100);
	Robot_info[0].companion = Robot_info[0].kamikaze = Robot_info[0].thief = Robot_info[0].attack_type = 0;
	for (const int type : { OBJ_ROBOT, OBJ_CLUTTER }) {
		Game_mode = 0;
		const int index = obj_create(static_cast<object_type_t>(type), 0, ConsoleObject->segnum, &ConsoleObject->pos,
		                             &vmd_identity_matrix, F1_0, type == OBJ_ROBOT ? CT_AI : CT_NONE, MT_PHYSICS, RT_NONE);
		require(index >= 0, "cannot allocate nonplayer force-damage control");
		auto &other = Objects[index];
		GameTime64 += F1_0; // Production birth registry must admit a mature robot
		for (const bool multiplayer : { false, true }) {
			if (multiplayer && type == OBJ_ROBOT) continue; // No multiplayer robot ownership/synchronization claim
			for (const bool reverse : { false, true })
				for (const int direction : { -1, 1 })
					for (const int difficulty : { 0, 4 })
						for (const bool persistent : { false, true })
							for (const bool boss : { false, true }) {
								if (boss && type != OBJ_ROBOT) continue;
								Game_mode = multiplayer ? GM_NETWORK : 0;
								Difficulty_level = difficulty;
								Player_is_dead = Endlevel_sequence = 0;
								Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
								ConsoleObject->flags &= ~(OF_EXPLODING | OF_SHOULD_BE_DEAD);
								ConsoleObject->movement_type = MT_PHYSICS;
								ConsoleObject->mtype.phys_info = {};
								ConsoleObject->mtype.phys_info.mass = F1_0;
								ConsoleObject->mtype.phys_info.velocity = { direction * i2f(32), 0, 0 };
								Players[Player_num].shields = ConsoleObject->shields = i2f(100);
								other.flags = 0;
								other.shields = i2f(100);
								other.mtype.phys_info = {};
								other.mtype.phys_info.mass = F1_0;
								if (persistent) other.mtype.phys_info.flags = PF_PERSISTENT;
								other.ctype.ai_info.behavior = AIB_NORMAL;
								other.ctype.ai_info.SKIP_AI_COUNT = 127;
								Robot_info[0].boss_flag = boss ? 1 : 0;
								Num_awareness_events = Overall_agitation = 0;
								const bool protected_contact = protected_profile && xl_preview_no_damage_segment(ConsoleObject->segnum);
								const bool blocked_contact = type == OBJ_CLUTTER && protected_contact;
								const bool blocked_other = blocked_contact || persistent || boss;
								const fix magnitude = xl && !multiplayer ? i2f(32) / (4 + difficulty) : i2f(32);
								fix expected_damage = blocked_other ? 0 : magnitude / 8;
								if (type == OBJ_ROBOT) expected_damage /= 2;
								collide_set_collision_delay_last_play_time(GameTime64); // Closed gate avoids collision-effect allocations
								d_srand(9012);
								d_rand_set_call_count(0);
								unsigned expected_rng = 0, actual_rng = 0;
								if (type == OBJ_ROBOT) (void) d_rand(); // Existing awareness agitation draw
								d_rand_get_state(&expected_rng);
								d_srand(9012);
								d_rand_set_call_count(0);
								auto point = ConsoleObject->pos;
								collide_two_objects(reverse ? &other : ConsoleObject, reverse ? ConsoleObject : &other, &point);
								d_rand_get_state(&actual_rng);
								require(other.shields == i2f(100) - expected_damage,
								        "nonplayer collision damage used the wrong XL rotation/full-force vector");
								require(other.mtype.phys_info.velocity.x == (blocked_other ? 0 : direction * i2f(32)) &&
								            ConsoleObject->mtype.phys_info.velocity.x == direction * i2f(blocked_contact ? 32 : 24) &&
								            (other.mtype.phys_info.rotvel.y == 0) == blocked_other,
								        "nonplayer damage-vector correction changed full impulse or rotation");
								require(actual_rng == expected_rng && d_rand_get_call_count() == unsigned(type == OBJ_ROBOT ? 1 : 0) &&
								            collide_get_collision_delay_last_play_time() == GameTime64,
								        "nonplayer force-damage correction changed awareness RNG or closed sound gate");
							}
		}
		obj_delete(index);
	}
	Robot_info[0] = saved_robot;
	cheats = saved_cheats;
	*ConsoleObject = saved_player;
	Players[Player_num] = saved_player_state;
	Game_mode = saved_mode;
	Difficulty_level = saved_difficulty;
	Player_is_dead = saved_dead;
	Endlevel_sequence = saved_endlevel;
	Num_awareness_events = saved_awareness;
	Overall_agitation = saved_agitation;
	std::memcpy(Awareness_events, saved_events.data(), sizeof(Awareness_events));
	std::memcpy(Ai_cloak_info, saved_cloak.data(), sizeof(Ai_cloak_info));
	Believed_player_pos = saved_believed_pos;
	Believed_player_seg = saved_believed_seg;
	FrameTime = saved_frame;
	GameTime64 = saved_time;
	collide_set_collision_delay_last_play_time(saved_delay);
	d_rand_set_state(saved_rng);
	d_rand_set_call_count(saved_calls);
	std::cout << "Actual nonplayer scaled force-damage and full impulse controls passed; XL=" << xl << " protected=" << protected_profile << "\n";
}

// Controlled singleplayer dispatch; this does not qualify multiplayer synchronization
static void no_damage_bump_test(const xl_compat::Inventory &source, int segment)
{
	const auto saved_time = GameTime64;
	const auto saved_delay = collide_get_collision_delay_last_play_time();
	unsigned saved_rng = 0;
	d_rand_get_state(&saved_rng);
	GameTime64 = i2f(100);
	int ordinary_segment = -1;
	for (int i = 0; i < Num_segments; ++i)
		if (source.segments[i].function == 0 && !(xl_compat::segment_property_bits(source.level_version, source.segments[i]) & xl_compat::segment_no_damage)) ordinary_segment = i;
	require(ordinary_segment >= 0, "object collision boundary control needs an ordinary segment");
	obj_relink(ConsoleObject - Objects, segment);
	compute_segment_center(&ConsoleObject->pos, &Segments[segment]);
	auto neutral = source;
	if (neutral.level_version >= 21) neutral.segments[segment].properties &= ~xl_compat::segment_no_damage;
	else neutral.segments[segment].function = 0;
	const bool active_xl = xl_level_extensions_active() != 0;
	xl_test_bind_no_damage(neutral);
	ordinary_nonphysical_bump_controls(active_xl, false);
	ordinary_force_damage_controls(active_xl, false);
	ordinary_bump_boss_scope_controls(active_xl, false);
	ordinary_nonplayer_force_damage_controls(active_xl, false);
	xl_test_bind_no_damage(source);
	ordinary_nonphysical_bump_controls(active_xl, true);
	ordinary_force_damage_controls(active_xl, true);
	ordinary_bump_boss_scope_controls(active_xl, true);
	ordinary_nonplayer_force_damage_controls(active_xl, true);
	for (auto type : { OBJ_CLUTTER, OBJ_PLAYER }) {
		const int id = type == OBJ_PLAYER ? (Player_num + 1) % MAX_PLAYERS : 0;
		const int other_index = obj_create(type, id, segment, &ConsoleObject->pos, &ConsoleObject->orient,
		                                   F1_0, CT_NONE, MT_PHYSICS, RT_NONE);
		require(other_index >= 0, "cannot create no-damage bump control");
		auto &other = Objects[other_index];
		other.mtype.phys_info.mass = ConsoleObject->mtype.phys_info.mass;
		const auto reset = [&] {
			Players[Player_num].shields = ConsoleObject->shields = other.shields = i2f(100);
			ConsoleObject->mtype.phys_info.velocity = { i2f(10), 0, 0 };
			other.mtype.phys_info.velocity = { -i2f(10), 0, 0 };
			ConsoleObject->mtype.phys_info.rotvel = other.mtype.phys_info.rotvel = {};
			collide_set_collision_delay_last_play_time(GameTime64 - i2f(2));
			d_srand(9012);
		};
		xl_test_bind_no_damage(source);
		reset();
		const auto player_before = *ConsoleObject, other_before = other;
		unsigned before_rng = 0, after_rng = 0;
		d_rand_get_state(&before_rng);
		vms_vector hit = ConsoleObject->pos;
		collide_two_objects(ConsoleObject, &other, &hit);
		d_rand_get_state(&after_rng);
		require(!std::memcmp(ConsoleObject, &player_before, sizeof(player_before)) &&
		            !std::memcmp(&other, &other_before, sizeof(other_before)) && Players[Player_num].shields == i2f(100) &&
		            collide_get_collision_delay_last_play_time() == GameTime64 - i2f(2) && before_rng == after_rng,
		        "protected object collision changed response, damage or sound-gate state");
		std::cout << "ND_CONTACT " << (type == OBJ_PLAYER ? 1 : 0) << " 1 "
		          << int(ConsoleObject->mtype.phys_info.velocity.x != player_before.mtype.phys_info.velocity.x) << ' '
		          << int(collide_get_collision_delay_last_play_time() != GameTime64 - i2f(2)) << '\n';
		xl_test_bind_no_damage(neutral);
		reset();
		collide_two_objects(ConsoleObject, &other, &hit);
		require(ConsoleObject->mtype.phys_info.velocity.x != player_before.mtype.phys_info.velocity.x &&
		            other.mtype.phys_info.velocity.x != other_before.mtype.phys_info.velocity.x &&
		            collide_get_collision_delay_last_play_time() != GameTime64 - i2f(2),
		        "ordinary object collision did not bump both actors or enter sound gate");
		std::cout << "ND_CONTACT " << (type == OBJ_PLAYER ? 1 : 0) << " 0 "
		          << int(ConsoleObject->mtype.phys_info.velocity.x != player_before.mtype.phys_info.velocity.x) << ' '
		          << int(collide_get_collision_delay_last_play_time() != GameTime64 - i2f(2)) << '\n';
		// Direct dispatch boundary control: the two actors occupy different segment types
		xl_test_bind_no_damage(source);
		obj_relink(other_index, ordinary_segment);
		reset();
		collide_two_objects(&other, ConsoleObject, &hit);
		if (type == OBJ_PLAYER)
			require(ConsoleObject->mtype.phys_info.velocity.x != player_before.mtype.phys_info.velocity.x &&
			            collide_get_collision_delay_last_play_time() != GameTime64 - i2f(2),
			        "player-pair gate incorrectly used the second player's protected segment");
		else
			require(ConsoleObject->mtype.phys_info.velocity.x == player_before.mtype.phys_info.velocity.x &&
			            collide_get_collision_delay_last_play_time() == GameTime64 - i2f(2),
			        "clutter dispatch used clutter segment instead of protected player segment");
		obj_delete(other_index);
	}
	// Protection must not disable robot damage, contact response or AI awareness
	const int robot_index = obj_create(OBJ_ROBOT, 0, segment, &ConsoleObject->pos, &ConsoleObject->orient,
	                                   F1_0, CT_AI, MT_PHYSICS, RT_POLYOBJ);
	require(robot_index >= 0 && !Robot_info[0].companion && !Robot_info[0].kamikaze && !Robot_info[0].thief,
	        "robot contact control requires an ordinary robot");
	auto &robot = Objects[robot_index];
	GameTime64 += F1_0;
	robot.mtype.phys_info.mass = ConsoleObject->mtype.phys_info.mass;
	const int saved_awareness_count = Num_awareness_events;
	physics_info baseline_robot{}, baseline_player{};
	for (bool protected_profile : { false, true }) {
		xl_test_bind_no_damage(protected_profile ? source : neutral);
		robot.shields = i2f(100);
		apply_damage_to_robot(&robot, i2f(10), ConsoleObject - Objects);
		require(robot.shields == i2f(90), "protection incorrectly prevented robot damage");
		Players[Player_num].shields = ConsoleObject->shields = robot.shields = i2f(100);
		ConsoleObject->mtype.phys_info.velocity = { i2f(10), 0, 0 };
		robot.mtype.phys_info.velocity = { -i2f(10), 0, 0 };
		ConsoleObject->mtype.phys_info.rotvel = robot.mtype.phys_info.rotvel = {};
		collide_set_collision_delay_last_play_time(GameTime64 - i2f(2));
		Num_awareness_events = saved_awareness_count;
		d_srand(9012);
		vms_vector hit = ConsoleObject->pos;
		collide_two_objects(ConsoleObject, &robot, &hit);
		require(Num_awareness_events > saved_awareness_count &&
		            ConsoleObject->mtype.phys_info.velocity.x != i2f(10) &&
		            collide_get_collision_delay_last_play_time() != GameTime64 - i2f(2),
		        "robot contact lost AI awareness, bump or sound-gate dispatch");
		if (!protected_profile) {
			baseline_robot = robot.mtype.phys_info;
			baseline_player = ConsoleObject->mtype.phys_info;
		} else {
			require(Players[Player_num].shields == i2f(100) &&
			            !std::memcmp(&baseline_robot, &robot.mtype.phys_info, sizeof(physics_info)) &&
			            !std::memcmp(&baseline_player, &ConsoleObject->mtype.phys_info, sizeof(physics_info)),
			        "protected robot contact changed physical response or damaged player");
		}
	}
	Num_awareness_events = saved_awareness_count;
	obj_delete(robot_index);
	std::cout << "No-damage robot controls: robot damage, collision physics, sound gate and AI awareness retained\n";
	xl_test_bind_no_damage(source);
	GameTime64 = saved_time;
	collide_set_collision_delay_last_play_time(saved_delay);
	d_rand_set_state(saved_rng);
	std::cout << "No-damage native bump controls: player/clutter and player/player dispatch suppresses response and sound-gate state; ordinary controls bump both objects\n";
}
