#include "seismic_gameplay.h"
static void camera_mine_chain_controls(object &camera)
{
	const auto gauss = Weapon_info[GAUSS_ID], mine_definition = Weapon_info[PROXIMITY_ID];
	const auto pilot = Players[Player_num];
	const auto player = *ConsoleObject;
	std::vector<std::pair<int, int>> hidden;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type != OBJ_NONE && &Objects[i] != ConsoleObject && &Objects[i] != &camera) {
			hidden.emplace_back(i, Objects[i].type);
			Objects[i].type = OBJ_NONE;
		}
	Players[Player_num].flags |= PLAYER_FLAGS_INVULNERABLE;
	for (int strength : { 1900, 2500 }) {
		Weapon_info[GAUSS_ID].damage_radius = 16 * F1_0;
		Weapon_info[GAUSS_ID].children = -1;
		Weapon_info[GAUSS_ID].flash = 0;
		Weapon_info[PROXIMITY_ID].damage_radius = 16 * F1_0;
		Weapon_info[PROXIMITY_ID].children = -1;
		Weapon_info[PROXIMITY_ID].flash = 0;
		for (auto &value : Weapon_info[GAUSS_ID].strength) value = strength * F1_0;
		for (auto &value : Weapon_info[PROXIMITY_ID].strength) value = 2500 * F1_0;
		xl_camera_validate_weapon_definitions();
		std::array<int, MAX_OBJECTS> signatures{};
		const int highest = Highest_object_index;
		for (int i = 0; i <= highest; ++i) signatures[i] = Objects[i].signature;
		const int shot = projectile_test_shot(int(ConsoleObject - Objects), GAUSS_ID);
		const int first = projectile_test_shot(int(ConsoleObject - Objects), PROXIMITY_ID);
		const int second = projectile_test_shot(int(ConsoleObject - Objects), PROXIMITY_ID);
		for (int owner : { shot, first, second }) {
			Objects[owner].pos = camera.pos;
			obj_relink(owner, camera.segnum);
		}
		Objects[first].pos.x += 8 * F1_0;
		require(object_to_object_visibility(&Objects[shot], &Objects[first], FQ_TRANSWALL), "Mine-chain fixture lacks visibility");
		auto impact = camera.pos;
		collide_two_objects(&Objects[shot], &camera, &impact);
		const bool chained = strength == 2500;
		require(bool(Objects[first].flags & OF_SHOULD_BE_DEAD) == chained &&
		            bool(Objects[second].flags & OF_SHOULD_BE_DEAD) == chained,
		        "Camera splash lost threshold or recursive mine detonation");
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type != OBJ_NONE && (i > highest || Objects[i].signature != signatures[i])) obj_delete(i);
	}
	for (const auto &entry : hidden) Objects[entry.first].type = entry.second;
	Weapon_info[GAUSS_ID] = gauss;
	Weapon_info[PROXIMITY_ID] = mine_definition;
	Players[Player_num] = pilot;
	*ConsoleObject = player;
	std::cout << "Camera splash mine threshold and recursive detonation passed\n";
}

static void camera_missile_native_controls(object &camera)
{
	xl_camera_validate_weapon_definitions();
	const auto original_definition = Weapon_info[SMART_ID];
	for (int mode = 0; mode < 3; ++mode) {
		Weapon_info[SMART_ID] = original_definition;
		if (mode == 0) Weapon_info[SMART_ID].strength[4] = 32000 * F1_0;
		if (mode == 1) Weapon_info[SMART_ID].children = N_weapon_types;
		if (mode == 2) Weapon_info[SMART_ID].flash = -1;
		bool rejected = false;
		try {
			XlPreparedResources rejected_resources("__camera_bad_weapons__.rl2");
			rejected_resources.prepare_camera_model();
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		require(rejected, "Camera resource preflight accepted unsupported weapon gameplay");
	}
	Weapon_info[SMART_ID] = original_definition;
	xl_camera_validate_weapon_definitions();
	const auto saved_camera = camera;
	const auto saved_player = *ConsoleObject;
	const auto saved_pilot = Players[Player_num];
	const auto saved_time = GameTime64;
	const auto saved_difficulty = Difficulty_level;
	const auto saved_seismic = xl_compat::seismic_gameplay_snapshot();
	const auto saved_highest = Highest_object_index;
	GameTime64 = 10 * F1_0;
	std::vector<std::pair<int, int>> hidden;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type != OBJ_NONE && &Objects[i] != ConsoleObject && &Objects[i] != &camera) {
			hidden.emplace_back(i, Objects[i].type);
			Objects[i].type = OBJ_NONE;
		}
	unsigned cases = 0, children = 0;
	double maximum_chain = 0;
	for (int id : { PROXIMITY_ID, PMINE_ID, SMART_ID, MEGA_ID, SUPERPROX_ID, EARTHSHAKER_ID, EARTHSHAKER_MEGA_ID, PLAYER_SMART_HOMING_ID, SMART_MINE_HOMING_ID })
		for (int difficulty = 0; difficulty < 5; ++difficulty)
			for (bool reverse : { false, true }) {
				Difficulty_level = difficulty;
				*ConsoleObject = saved_player;
				Players[Player_num] = saved_pilot;
				ConsoleObject->shields = Players[Player_num].shields = 10000 * F1_0;
				Players[Player_num].flags |= PLAYER_FLAGS_INVULNERABLE;
				camera = saved_camera;
				camera.flags = 0;
				xl_seismic_clear();
				const auto &definition = Weapon_info[id];
				const double chain = double(definition.strength[difficulty]) / F1_0 * double(definition.damage_radius) / F1_0 / 4.0 + double(definition.damage_radius) / F1_0 / F1_0;
				maximum_chain = std::max(maximum_chain, chain);
				require(chain < 8000.0, "Original camera missile can cross the mine-chain threshold");
				const int shot = projectile_test_shot(int(ConsoleObject - Objects), id);
				auto &weapon = Objects[shot];
				std::array<int, MAX_OBJECTS> signatures{};
				const int highest = Highest_object_index;
				for (int i = 0; i <= highest; ++i) signatures[i] = Objects[i].signature;
				auto impact = camera.pos;
				collide_two_objects(reverse ? &weapon : &camera, reverse ? &camera : &weapon, &impact);
				require(weapon.flags & OF_SHOULD_BE_DEAD, "Camera missile did not retire after impact");
				require(camera.shields == saved_camera.shields && !(camera.flags & OF_SHOULD_BE_DEAD), "Camera missile damaged an indestructible camera");
				unsigned births = 0, flashes = 0;
				for (int i = 0; i <= Highest_object_index; ++i)
					if (Objects[i].type != OBJ_NONE && (i > highest || Objects[i].signature != signatures[i])) {
						const auto &entry = Objects[i];
						if (entry.type == OBJ_WEAPON) {
							require(entry.id == definition.children && entry.ctype.laser_info.parent_num == ConsoleObject - Objects,
							        "Camera missile lost child type or original shooter credit");
							++births;
						} else {
							require(entry.type == OBJ_FIREBALL, "Camera missile created an unexpected object");
							++flashes;
						}
					}
				require(births == unsigned(definition.children >= 1 ? NUM_SMART_CHILDREN : 0), "Camera impact omitted or duplicated smart children");
				require(flashes == unsigned(definition.robot_hit_vclip >= 0) + unsigned(definition.damage_radius > 0), "Camera missile lost visible impact or radial explosion");
				const auto quake = xl_compat::seismic_gameplay_snapshot();
				unsigned detonations = 0;
				for (const auto &entry : quake[3])
					if (!entry[0].is_null()) ++detonations;
				require(detonations == unsigned(id == EARTHSHAKER_ID ? 2 : 0), "Camera earthquake contact lost source double-detonation ordering");
				for (int i = 0; i <= Highest_object_index; ++i)
					if (Objects[i].type != OBJ_NONE && (i > highest || Objects[i].signature != signatures[i])) obj_delete(i);
				obj_delete(shot);
				children += births;
				++cases;
			}
	// Earthquake is queued before the source parent-signature early return
	xl_seismic_clear();
	const int shot = projectile_test_shot(int(ConsoleObject - Objects), EARTHSHAKER_ID);
	Objects[shot].ctype.laser_info.parent_signature = camera.signature;
	auto impact = camera.pos;
	collide_two_objects(&camera, &Objects[shot], &impact);
	const auto quake = xl_compat::seismic_gameplay_snapshot();
	unsigned detonations = 0;
	for (const auto &entry : quake[3])
		if (!entry[0].is_null()) ++detonations;
	require(detonations == 1 && !(Objects[shot].flags & OF_SHOULD_BE_DEAD), "Camera earthquake parent gate reordered or detonated the weapon");
	obj_delete(shot);
	for (const auto &entry : hidden) Objects[entry.first].type = entry.second;
	Highest_object_index = std::max(Highest_object_index, saved_highest);
	camera = saved_camera;
	*ConsoleObject = saved_player;
	Players[Player_num] = saved_pilot;
	Difficulty_level = saved_difficulty;
	GameTime64 = saved_time;
	require(xl_compat::restore_seismic_gameplay(saved_seismic, true), "Cannot restore earthquake control state");
	std::cout << "Native camera expanded missiles: " << cases << " ordered/difficulty impacts, " << children << " children, earthquake ordering passed; conservative chain bound=" << maximum_chain << " < 8000\n";
}
