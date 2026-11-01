#include "camera_children.h"
#include "../camera_children.h"
static void camera_children_native_controls(object &camera)
{
	const auto saved_player = *ConsoleObject;
	const auto saved_pilot = Players[Player_num];
	const auto saved_time = GameTime64;
	std::vector<std::pair<int, int>> hidden;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type != OBJ_NONE && &Objects[i] != ConsoleObject && &Objects[i] != &camera) {
			hidden.emplace_back(i, Objects[i].type);
			Objects[i].type = OBJ_NONE;
		}
	const auto point = [](const vms_vector &v) { return xl_compat::FixedLightPoint{ v.x, v.y, v.z }; };
	unsigned cases = 0, births = 0, repeats = 0, damaged = 0;
	for (int id : { SMART_ID, SUPERPROX_ID })
		for (bool robot_parent : { true, false })
			for (int available : { 0, 1, 2 })
				for (unsigned seed : { 0u, 17u, 99u }) {
					*ConsoleObject = saved_player;
					Players[Player_num] = saved_pilot;
					if (robot_parent && !available) Players[Player_num].flags |= PLAYER_FLAGS_CLOAKED;
					else Players[Player_num].flags &= ~PLAYER_FLAGS_CLOAKED;
					std::vector<int> targets;
					for (int i = 0; i < 2; ++i) {
						int owner = projectile_robot();
						targets.push_back(owner);
						Objects[owner].pos = ConsoleObject->pos;
						Objects[owner].pos.x += (i + 1) * F1_0;
						Objects[owner].ctype.ai_info.CLOAKED = i >= available;
						Objects[owner].shields = 1000 * F1_0;
					}
					int parent = robot_parent ? targets[0] : int(ConsoleObject - Objects);
					const int shot = projectile_test_shot(parent, id);
					auto &weapon = Objects[shot];
					const auto &definition = Weapon_info[weapon.id];
					require(definition.children > 0, "Smart test requires an original child definition");
					const auto &child_definition = Weapon_info[definition.children];
					std::vector<int> eligible;
					if (robot_parent) {
						if (available) eligible.push_back(int(ConsoleObject - Objects));
					} else
						for (int i = 0; i < available; ++i) eligible.push_back(targets[i]);
					for (int goal : eligible) require(object_to_object_visibility(&weapon, &Objects[goal], FQ_TRANSWALL), "Smart target fixture lacks visibility");
					struct Expected {
						int target;
						vms_vector velocity;
					};
					std::vector<Expected> expected;
					d_srand(seed);
					for (int i = 0; i < NUM_SMART_CHILDREN; ++i) {
						const int target = xl_compat::camera_smart_choice(eligible, [] { return d_rand(); });
						const auto goal = target < 0 ? xl_compat::FixedLightPoint{} : point(Objects[target].pos);
						const auto direction = xl_compat::camera_smart_direction(point(weapon.pos), target < 0 ? nullptr : &goal, [] { return d_rand(); });
						fix speed = child_definition.speed[Difficulty_level];
						if (child_definition.speedvar != 128) speed = fixmul(speed, F1_0 - ((d_rand() * child_definition.speedvar) >> 6));
						speed /= 4;
						if (child_definition.thrust) speed /= 2;
						vms_vector native{ direction[0], direction[1], direction[2] }, velocity{};
						vm_vec_copy_scale(&velocity, &native, speed);
						expected.push_back({ target, velocity });
					}
					unsigned expected_rng = 0;
					d_rand_get_state(&expected_rng);
					std::array<int, MAX_OBJECTS> signatures{};
					const int highest = Highest_object_index;
					for (int i = 0; i <= highest; ++i) signatures[i] = Objects[i].signature;
					d_srand(seed);
					xl_camera_smart_children(weapon);
					unsigned actual_rng = 0;
					d_rand_get_state(&actual_rng);
					require(actual_rng == expected_rng, "Smart children lost target/direction/constructor RNG order");
					std::vector<int> created;
					for (int i = 0; i <= Highest_object_index; ++i)
						if (Objects[i].type != OBJ_NONE && (i > highest || Objects[i].signature != signatures[i])) {
							require(Objects[i].type == OBJ_WEAPON && Objects[i].id == definition.children, "Smart children created unexpected objects");
							created.push_back(i);
						}
					require(created.size() == NUM_SMART_CHILDREN, "Smart child count differs from source");
					// The allocator reuses descending free slots; match by creation signature
					std::sort(created.begin(), created.end(), [](int a, int b) { return Objects[a].signature < Objects[b].signature; });
					for (unsigned i = 0; i < created.size(); ++i) {
						const auto &child = Objects[created[i]];
						if (child.ctype.laser_info.parent_num != parent || child.ctype.laser_info.parent_type != Objects[parent].type ||
						    child.ctype.laser_info.parent_signature != Objects[parent].signature || child.ctype.laser_info.track_goal != expected[i].target ||
						    point(child.pos) != point(weapon.pos) || point(child.mtype.phys_info.velocity) != point(expected[i].velocity))
							std::cerr << "Smart mismatch id=" << id << " robot=" << robot_parent << " available=" << available << " seed=" << seed << " child=" << i
							          << " parent=" << child.ctype.laser_info.parent_num << "/" << parent << " type=" << child.ctype.laser_info.parent_type << "/" << int(Objects[parent].type)
							          << " sig=" << child.ctype.laser_info.parent_signature << "/" << Objects[parent].signature << " goal=" << child.ctype.laser_info.track_goal << "/" << expected[i].target
							          << " pos=" << child.pos.x << "," << child.pos.y << "," << child.pos.z << "/" << weapon.pos.x << "," << weapon.pos.y << "," << weapon.pos.z
							          << " vel=" << child.mtype.phys_info.velocity.x << "," << child.mtype.phys_info.velocity.y << "," << child.mtype.phys_info.velocity.z
							          << "/" << expected[i].velocity.x << "," << expected[i].velocity.y << "," << expected[i].velocity.z << "\n";
						require(child.ctype.laser_info.parent_num == parent && child.ctype.laser_info.parent_type == Objects[parent].type &&
						            child.ctype.laser_info.parent_signature == Objects[parent].signature && child.ctype.laser_info.track_goal == expected[i].target &&
						            point(child.pos) == point(weapon.pos) && point(child.mtype.phys_info.velocity) == point(expected[i].velocity),
						        "Smart child lost native ownership, target, position or launch direction");
						if (eligible.size() > 1 && i && expected[i].target == expected[i - 1].target) ++repeats;
						++births;
					}
					const bool retain = id == SUPERPROX_ID && !robot_parent && available == 2 && seed == 99;
					if (!robot_parent && available && !retain) {
						auto &victim = Objects[targets[0]];
						const int shields = victim.shields;
						GameTime64 += 2 * F1_0;
						collide_two_objects(&Objects[created[0]], &victim, &victim.pos);
						require(victim.shields < shields, "Native smart child did not retain robot damage");
						++damaged;
					}
					obj_delete(shot);
					if (!retain) {
						for (int owner : created) obj_delete(owner);
						for (int owner : targets) obj_delete(owner);
					}
					++cases;
				}
	for (const auto &entry : hidden) Objects[entry.first].type = entry.second;
	*ConsoleObject = saved_player;
	Players[Player_num] = saved_pilot;
	GameTime64 = saved_time;
	require(repeats > 0 && damaged > 0, "Native smart child controls missed repeated targets or damage");
	std::cout << "Native camera smart children: " << cases << " profiles, " << births << " births, " << repeats << " repeated targets, " << damaged << " damaging child contacts passed\n";
	for (int id : { PROXIMITY_ID, SMART_ID, MEGA_ID, SUPERPROX_ID, EARTHSHAKER_ID, PLAYER_SMART_HOMING_ID, SMART_MINE_HOMING_ID, EARTHSHAKER_MEGA_ID })
		std::cout << "Camera missile definition " << id << ": strength=" << Weapon_info[id].strength[Difficulty_level] << " radius=" << Weapon_info[id].damage_radius << " children=" << int(Weapon_info[id].children) << "\n";
}

static std::vector<std::array<int, 16>> camera_children_capture()
{
	std::vector<std::array<int, 16>> result;
	for (int i = 0; i <= Highest_object_index; ++i) {
		const auto &child = Objects[i];
		if (child.type != OBJ_WEAPON || (child.id != PLAYER_SMART_HOMING_ID && child.id != SMART_MINE_HOMING_ID && child.id != EARTHSHAKER_MEGA_ID)) continue;
		const auto &laser = child.ctype.laser_info;
		const auto &velocity = child.mtype.phys_info.velocity;
		result.push_back({ i, child.signature, child.id, laser.parent_num, laser.parent_type, laser.parent_signature, laser.track_goal,
		                   child.pos.x, child.pos.y, child.pos.z, velocity.x, velocity.y, velocity.z, child.lifeleft, child.shields, child.mtype.phys_info.flags });
	}
	return result;
}
