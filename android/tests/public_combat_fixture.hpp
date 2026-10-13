// Audited public actors in the existing controlled combat/contact/drop operations
// These are real loaded tables with synthesized geometry, inventory and timing
static nlohmann::json exercise_public_combat_rules(bool native)
{
	using nlohmann::json;
	const std::vector<int> robots = native ? std::vector<int>{ 0, 1, 2, 3, 4, 14, 16, 17, 18, 19, 20 }
	                                       : std::vector<int>{ 25, 30, 31, 33, 37, 38, 39, 40, 42, 50 };
	const std::vector<int> eggs = native ? std::vector<int>{ 0, 14, 20 } : std::vector<int>{ 25, 37, 50 };
	const int actor = native ? 0 : 25, pair_actor = native ? 1 : 40;
	std::vector<int> available;
	json definitions = json::array();
	for (int id = 0; id < N_robot_types; ++id)
		if (Robot_info[id].model_num >= 0) {
			available.push_back(id);
			require(Robot_info[id].model_num < N_polygon_models && Polygon_models[Robot_info[id].model_num].model_data_size > 0,
			        "public combat actor owns a real packaged polygon model");
			definitions.push_back({ id, +Robot_info[id].model_num, +Robot_info[id].boss_flag });
		}
	require(available == robots, "public combat exercises every explicitly audited available robot, without silent skipping");
	for (int id : { LASER_ID_L1, FLARE_ID, CONCUSSION_ID, HOMING_ID })
		require(id < N_weapon_types && Weapon_info[id].strength[2] > 0 && Weapon_info[id].speed[2] > 0,
		        "public combat projectile has loaded non-placeholder physics");
	std::vector<int> powerups = { POW_ENERGY, POW_SHIELD_BOOST, POW_VULCAN_WEAPON };
#ifdef DXX_BUILD_DESCENT_II
	if (!native) powerups.push_back(POW_GAUSS_WEAPON);
#endif
	for (int id : powerups) {
		const int clip = Powerup_info[id].vclip_num;
		require(id < N_powerup_types && clip >= 0 && clip < VCLIP_MAXNUM && Vclip[clip].num_frames > 0 && Vclip[clip].frames[0].index > 0,
		        "public physical drop uses an available pickup clip");
	}
	require(std::strcmp(Level_names[0], native ? "level01.sdl" : "d2leva-1.sl2") == 0,
	        "public combat selects the authentic demo mission");
	const json profile = { { "edition", "shareware" }, { "content_game", native ? "d1" : "d2" }, { "mission", Current_mission_filename }, { "first_level", Level_names[0] }, { "geometry", "controlled-corridor" }, { "robots", robots }, { "definitions", definitions }, { "physical_powerups", powerups }, { "contact_actor", actor }, { "pair_actors", { actor, pair_actor } }, { "egg_actors", eggs }, { "impact_projectiles", { LASER_ID_L1, CONCUSSION_ID, HOMING_ID } }, { "retail_only", { "resistant-boss-quarter-damage", "mega-projectile", "omega-physical-drop" } }, { "replacement_drop_scope", "controlled inventory policy including unavailable artwork IDs" } };
	const auto source = profile.dump(2) + "\n";
	write_fixture(native ? "combat-assets.json" : "combat-assets-d2.json", bytes(source.begin(), source.end()));
	InitWeaponOrdering();
	Game_mode = 0;
	Player_num = Player_is_dead = 0;
	N_players = 1;
	Newdemo_state = ND_STATE_NORMAL;
	std::memset(&cheats, 0, sizeof(cheats));
	FrameTime = F1_0 / 16;
	GameTime64 = 10 * F1_0;
	const auto resources = exercise_player_resource_damage(native);
	json result = { { "pickups", resources["pickups"] }, { "damage", resources["damage"] }, { "drops", exercise_replacement_drops(native) }, { "contact_motion", exercise_contact_motion(native, actor) }, { "robot_pairs", exercise_robot_pairs(native, actor, pair_actor) }, { "resource_drops", exercise_resource_drops(native, actor) }, { "weapon_drops", exercise_weapon_drops(native, actor, true) }, { "robot_drops", exercise_robot_drops(native, eggs) }, { "robot_blasts", exercise_robot_blasts(native, robots) }, { "secondary_explosions", exercise_secondary_explosions(native, robots) }, { "volatile_impacts", exercise_volatile_impacts(native, actor, true) } };
	require(result["pickups"].size() == 30 && result["damage"].size() == 30 && result["drops"].size() == 2160 &&
	            result["contact_motion"]["motion"].size() == 32 && result["contact_motion"]["contacts"].size() == 8 &&
	            result["robot_pairs"].size() == 16 && result["resource_drops"].size() == 36 &&
	            result["weapon_drops"].size() == (native ? 4 : 8) && result["robot_drops"].size() == 27 &&
	            result["robot_blasts"].size() == robots.size() * 4 && result["secondary_explosions"].size() == (native ? 11 : 8) &&
	            result["volatile_impacts"].size() == 30,
	        "complete public combat, contact, physical drop and controlled replacement-policy matrices");
	return result;
}
