extern "C" {
#include "physics.h"
}

// Original shared-face geometry; controlled motion, not route navigation
static void no_damage_boundary_test(const xl_compat::Inventory &source)
{
	int protected_segment = -1, ordinary_segment = -1, portal_side = -1;
	for (int segment = 0; segment < Num_segments && protected_segment < 0; ++segment) {
		if (source.segments[segment].function != 13) continue;
		for (int side = 0; side < 6; ++side) {
			const int child = Segments[segment].children[side];
			if (child < 0 || source.segments[child].function != 0 ||
			    Segments[segment].sides[side].wall_num != -1) continue;
			protected_segment = segment;
			ordinary_segment = child;
			portal_side = side;
			break;
		}
	}
	require(protected_segment >= 0, "no-damage boundary fixture has no open original shared face");
	const auto saved_object = *ConsoleObject;
	const auto saved_player = Players[Player_num];
	const auto saved_frame = FrameTime;
	const auto saved_laser = Next_laser_fire_time, saved_missile = Next_missile_fire_time;
	Next_laser_fire_time = Next_missile_fire_time = GameTime64;
	std::vector<ubyte> saved_flags;
	for (int i = 0; i <= Highest_object_index; ++i) {
		saved_flags.push_back(Objects[i].flags);
		// Paired geometry controls must not collect authored pickups on only one pass
		if (&Objects[i] != ConsoleObject) Objects[i].flags |= OF_SHOULD_BE_DEAD;
	}
	unsigned saved_rng = 0;
	d_rand_get_state(&saved_rng);
	vms_vector portal, inside, outside;
	compute_center_point_on_side(&portal, &Segments[protected_segment], portal_side);
	compute_segment_center(&inside, &Segments[protected_segment]);
	compute_segment_center(&outside, &Segments[ordinary_segment]);
	// Both endpoints are strictly inside their original cells, near the shared face
	for (auto *point : { &inside, &outside }) {
		vm_vec_sub(point, point, &portal);
		vm_vec_scale(point, F1_0 / 4);
		vm_vec_add2(point, &portal);
	}
	require(find_point_seg(&inside, protected_segment) == protected_segment &&
	            find_point_seg(&outside, ordinary_segment) == ordinary_segment,
	        "no-damage boundary endpoints are not in their authored cells");
	auto neutral = source;
	for (auto &segment : neutral.segments)
		if (segment.function == 13) segment.function = 0;
	FrameTime = F1_0 / 4;
	for (bool entering : { true, false }) {
		const int from = entering ? ordinary_segment : protected_segment;
		const int to = entering ? protected_segment : ordinary_segment;
		auto start = entering ? outside : inside, finish = entering ? inside : outside;
		object baseline{};
		unsigned baseline_rng = 0;
		for (bool protected_profile : { false, true }) {
			xl_test_bind_no_damage(protected_profile ? source : neutral);
			obj_relink(ConsoleObject - Objects, from);
			ConsoleObject->pos = start;
			ConsoleObject->orient = vmd_identity_matrix;
			ConsoleObject->mtype.phys_info = {};
			ConsoleObject->mtype.phys_info.mass = F1_0;
			vm_vec_sub(&ConsoleObject->mtype.phys_info.velocity, &finish, &start);
			vm_vec_scale(&ConsoleObject->mtype.phys_info.velocity, i2f(4));
			Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
			Players[Player_num].shields = ConsoleObject->shields = i2f(100);
			d_srand(9012);
			do_physics_sim(ConsoleObject);
			require(ConsoleObject->segnum == to, "native physics did not cross the no-damage shared face");
			unsigned rng = 0;
			d_rand_get_state(&rng);
			if (!protected_profile) {
				baseline = *ConsoleObject;
				baseline_rng = rng;
			} else {
				std::cout << "Boundary comparison " << from << "->" << to
				          << " pos=" << ConsoleObject->pos.x << ',' << ConsoleObject->pos.y << ',' << ConsoleObject->pos.z
				          << " baseline=" << baseline.pos.x << ',' << baseline.pos.y << ',' << baseline.pos.z
				          << " rng=" << rng << '/' << baseline_rng << " size=" << ConsoleObject->size << '\n';
				require(!std::memcmp(&ConsoleObject->pos, &baseline.pos, sizeof(vms_vector)) &&
				            !std::memcmp(&ConsoleObject->mtype.phys_info, &baseline.mtype.phys_info, sizeof(physics_info)) &&
				            rng == baseline_rng,
				        "no-damage changed shared-face movement or physics RNG");
			}
			require(bool(xl_preview_no_damage_segment(ConsoleObject->segnum)) == (protected_profile && entering),
			        "no-damage property did not follow native segment crossing");
			require(bool(allowed_to_fire_laser()) == !(protected_profile && entering) &&
			            bool(allowed_to_fire_missile()) == !(protected_profile && entering),
			        "primary/missile firing gates did not follow native segment crossing");
			apply_damage_to_player(ConsoleObject, nullptr, i2f(10), 0);
			require(Players[Player_num].shields == i2f(protected_profile && entering ? 100 : 90),
			        "damage suppression did not change immediately across the shared face");
		}
	}
	for (bool entering : { true, false }) {
		const int from = entering ? ordinary_segment : protected_segment;
		const int to = entering ? protected_segment : ordinary_segment;
		auto start = entering ? outside : inside, finish = entering ? inside : outside;
		vms_vector baseline_position{};
		for (bool protected_profile : { false, true }) {
			xl_test_bind_no_damage(neutral);
			const int weapon_index = Laser_create_new(&ConsoleObject->orient.fvec, &start, from,
			                                          ConsoleObject - Objects, LASER_ID_L1, 0);
			require(weapon_index >= 0, "cannot create shared-face incoming projectile");
			auto &weapon = Objects[weapon_index];
			weapon.mtype.phys_info.drag = 0;
			weapon.mtype.phys_info.thrust = {};
			vm_vec_sub(&weapon.mtype.phys_info.velocity, &finish, &start);
			vm_vec_scale(&weapon.mtype.phys_info.velocity, i2f(4));
			xl_test_bind_no_damage(protected_profile ? source : neutral);
			do_physics_sim(&weapon);
			require(weapon.segnum == to && !(weapon.flags & OF_SHOULD_BE_DEAD), "projectile failed original shared-face crossing");
			if (!protected_profile) baseline_position = weapon.pos;
			else require(!std::memcmp(&baseline_position, &weapon.pos, sizeof(vms_vector)), "protection changed projectile crossing position");
			obj_relink(ConsoleObject - Objects, to);
			ConsoleObject->pos = finish;
			Players[Player_num].shields = ConsoleObject->shields = i2f(100);
			weapon.ctype.laser_info.parent_type = OBJ_CNTRLCEN;
			weapon.ctype.laser_info.parent_num = -1;
			weapon.shields = i2f(10);
			weapon.ctype.laser_info.multiplier = F1_0;
			weapon.mtype.phys_info.flags &= ~PF_PERSISTENT;
			vms_vector hit = finish;
			collide_player_and_weapon(ConsoleObject, &weapon, &hit);
			const bool protected_hit = protected_profile && entering;
			require(Players[Player_num].shields == i2f(protected_hit ? 100 : 90) &&
			            bool(weapon.flags & OF_SHOULD_BE_DEAD) == !protected_hit,
			        "crossing projectile impact used departure instead of arrival protection");
			obj_delete(weapon_index);
		}
	}
	std::cout << "No-damage projectile boundary: both directions preserve flight; arrival property controls damage and projectile survival\n";
	obj_relink(ConsoleObject - Objects, saved_object.segnum);
	const int next = ConsoleObject->next, prev = ConsoleObject->prev;
	*ConsoleObject = saved_object;
	ConsoleObject->next = next;
	ConsoleObject->prev = prev;
	Players[Player_num] = saved_player;
	for (std::size_t i = 0; i < saved_flags.size(); ++i) Objects[i].flags = saved_flags[i];
	FrameTime = saved_frame;
	Next_laser_fire_time = saved_laser;
	Next_missile_fire_time = saved_missile;
	d_srand(saved_rng);
	xl_test_bind_no_damage(source);
	std::cout << "No-damage native boundary: original shared face crossed in both directions; movement and RNG match ordinary controls; damage and firing gates follow arrival segment\n";
}
