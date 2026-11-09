#include "../camera_contact.h"
extern "C" {
#include "automap.h"
#include "cntrlcen.h"
}

// Controlled native dispatch, not multiplayer synchronization
static void ordinary_nonphysical_bump_controls(bool xl, bool protected_profile)
{
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
	xl_test_bind_no_damage(neutral);
	ordinary_nonphysical_bump_controls(true, false);
	xl_test_bind_no_damage(source);
	ordinary_nonphysical_bump_controls(true, true);
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
