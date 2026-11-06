extern "C" fix d1_in_d2_ai_robot_blast_damage(const object *, fix);

// Derived companions alter only the boss role and, for removal, the placed ID
static void hxm_boss_binding_controls(const std::filesystem::path &root, bool vertigo = false)
{
	std::ifstream file(root / "missions/hxmtest.hog", std::ios::binary);
	const xl_compat::Bytes original{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	for (const int role : { 21, 0, -1, -23, -27 }) {
		const bool introduce = role != 0;
		const std::string mission = role == -27 ? "hxmneg27" : role == -23 ? "hxmneg23"
		                                                   : role == -1    ? "hxmnend"
		                                                   : introduce     ? "hxmboss"
		                                                                   : "hxmplain";
		xl_compat::Bytes output{ 'D', 'H', 'F' };
		unsigned companions = 0, levels = 0;
		for (auto member : xl_compat::read_hog(original)) {
			if (!d_stricmp(member.name.c_str(), "alarlof.hxm")) {
				require(member.bytes.size() == 11262 && member.bytes.at(12) == 3, "HXM role fixture source changed");
				member.bytes.at(12) = introduce ? 3 : 31;
				member.bytes.at(16 + 280) = static_cast<uint8_t>(role);
				++companions;
			}
			if (!d_stricmp(member.name.c_str(), "alarlof.rl2")) {
				const auto level = xl_compat::inspect_level(member.bytes);
				require(level.objects.size() > 100 && level.objects[100].type == OBJ_ROBOT && level.objects[100].id == 3,
				        "HXM role fixture actor changed");
				if (!introduce) member.bytes.at(level.objects[100].source_offset + 1) = 31;
				++levels;
			}
			char name[13]{};
			require(member.name.size() < sizeof(name), "HXM role fixture member name too long");
			std::strcpy(name, member.name.c_str());
			output.insert(output.end(), name, name + sizeof(name));
			for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(uint8_t(member.bytes.size() >> shift));
			output.insert(output.end(), member.bytes.begin(), member.bytes.end());
		}
		require(companions == 1 && levels == 1, "HXM role fixture source members missing");
		write_bytes((root / "missions" / (mission + ".hog")).string().c_str(), output);
		std::ofstream descriptor(root / "missions" / (mission + ".mn2"));
		descriptor << "d2x-name = Derived HXM boss role\nnum_levels = 1\nalarlof.rl2\n";
	}
	const auto roles_match = [] {
		std::set<int> authored, registered;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_ROBOT && Robot_info[Objects[i].id].boss_flag != 0) authored.insert(i);
		for (int i = 0; i < xl_boss_spawn_count(); ++i) {
			xl_boss_spawn_state state{};
			require(xl_boss_spawn_state_get(i, &state), "HXM boss binding state missing");
			if (state.owner >= 0) registered.insert(state.owner);
		}
		require(authored == registered, "Boss bindings do not match final HXM definitions");
	};
	char error[256]{};
	require(load_base_mission("hxmboss", error, sizeof(error)), error);
	require(!Robot_info[3].boss_flag, "Introduced HXM boss already has a base boss role");
	StartNewGame(1);
	require(Robot_info[3].boss_flag == 21 && Objects[100].id == 3, "Introduced HXM boss definition was not installed");
	roles_match();
	const int objectives = xl_level_objectives_remaining();
	require(objectives > 0 && apply_damage_to_robot(&Objects[100], Objects[100].shields + F1_0, ConsoleObject - Objects),
	        "Introduced HXM boss did not receive lethal combat");
	require(xl_boss_is_dying(100) && xl_level_objectives_remaining() == objectives && !Control_center_destroyed,
	        "Introduced HXM boss skipped its death roll or started countdown early");
	char save[] = "hxm-boss.sav", description[] = "HXM boss death roll";
	stop_time();
	require(state_save_all_sub(save, description), "HXM boss rolling-death save failed");
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_boss_spawn_count(),
	        "HXM boss bindings leaked into stock level");
	require(state_restore_all_sub(save, 0) && Robot_info[3].boss_flag == 21 && xl_boss_is_dying(100) &&
	            xl_level_objectives_remaining() == objectives && !Control_center_destroyed,
	        "HXM boss death roll or objectives lost through stock/disk restore");
	GameTime64 += BOSS_DEATH_DURATION + 1;
	xl_boss_deaths_frame();
	require(!xl_boss_is_dying(100) && xl_level_objectives_remaining() == objectives - 1 &&
	            bool(Control_center_destroyed) == (objectives == 1),
	        "Restored HXM boss did not retire exactly one objective");

	// Signed roles preserve the profile while clearing the reference bEndsLevel bit
	for (int signed_role : (vertigo ? std::vector<int>{ -1, -23, -27 } : std::vector<int>{ -1, -23 })) {
		require(load_base_mission(signed_role == -1 ? "hxmnend" : signed_role == -23 ? "hxmneg23"
		                                                                             : "hxmneg27",
		                          error, sizeof(error)),
		        error);
		StartNewGame(1);
		require(Robot_info[3].boss_flag == signed_role && Objects[100].id == 3, "Non-ending HXM boss lost its signed source identity");
		roles_match();
		const int nonending_objectives = xl_level_objectives_remaining();
		require(nonending_objectives == objectives - 1, "Non-ending boss became a required objective");
		require(xl_boss_spawn_default_type(signed_role, 0) == xl_boss_spawn_default_type(signed_role == -1 ? 22 : -signed_role, 0), "Signed boss chose a different source spawn profile");
		xl_boss_spawn_state before{}, after{};
		int slot = -1;
		for (int i = 0; i < xl_boss_spawn_count(); ++i) {
			require(xl_boss_spawn_state_get(i, &before), "Non-ending boss state missing");
			if (before.owner == 100) {
				slot = i;
				break;
			}
		}
		require(slot >= 0, "Non-ending boss missing runtime binding");
		GameTime64 += i2f(20);
		Objects[100].ctype.ai_info.CLOAKED = 0;
		xl_boss_ai_frame(100, 1);
		require(xl_boss_spawn_state_get(slot, &after) && Objects[100].ctype.ai_info.CLOAKED == 1,
		        "Non-ending boss did not enter the reference cloak lifecycle");
		for (const int kind : { LASER_ID, VULCAN_ID }) {
			const auto health = Objects[100].shields;
			const int shot = Laser_create_new(&ConsoleObject->orient.fvec, &Objects[100].pos, Objects[100].segnum,
			                                  ConsoleObject - Objects, kind, 0);
			require(shot >= 0, "Non-ending boss weapon control creation failed");
			auto hit = Objects[100].pos;
			collide_robot_and_weapon(&Objects[100], &Objects[shot], &hit);
			require((((signed_role == -23 && kind == LASER_ID) || (signed_role == -27 && kind == VULCAN_ID)) ? Objects[100].shields == health : Objects[100].shields < health) && !xl_boss_is_dying(100),
			        "Signed boss energy/kinetic vulnerability differs from reference profile");
			if (Objects[shot].type != OBJ_NONE) obj_delete(shot);
		}
		for (fix damage : { 0, 1, 3, 4, 1001, F1_0 })
			require(d1_in_d2_ai_robot_blast_damage(&Objects[100], damage) == (signed_role == -27 ? damage / 4 : damage),
			        "Signed boss radial resistance differs from pinned quarter-damage rule");
		require(apply_damage_to_robot(&Objects[100], Objects[100].shields + F1_0, ConsoleObject - Objects) && xl_boss_is_dying(100),
		        "Non-ending boss skipped lethal combat/death roll");
		stop_time();
		require(state_save_all_sub(save, description), "Non-ending boss save failed");
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_boss_spawn_count(),
		        "Non-ending boss leaked into classic runtime");
		require(state_restore_all_sub(save, 0) && Robot_info[3].boss_flag == signed_role && xl_boss_is_dying(100) &&
		            xl_level_objectives_remaining() == nonending_objectives,
		        "Non-ending boss identity/state changed on restore");
		// Remove other objectives in this synthetic control: even the last optional
		// boss must not start a countdown when its death roll completes
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_CNTRLCEN) Objects[i].shields = -1;
		require(xl_level_objectives_remaining() == 0 && !Control_center_destroyed, "Non-ending isolated objective setup failed");
		GameTime64 += BOSS_DEATH_DURATION + 1;
		xl_boss_deaths_frame();
		require(!xl_boss_is_dying(100) && !Control_center_destroyed && xl_level_objectives_remaining() == 0,
		        "Non-ending boss incorrectly ended the level");
	}
	require(load_base_mission("hxmplain", error, sizeof(error)), error);
	require(Robot_info[31].boss_flag > 0, "Removed HXM boss has no base boss role");
	StartNewGame(1);
	require(Objects[100].id == 31 && !Robot_info[31].boss_flag, "Removed HXM boss definition was not installed");
	roles_match();
	const int ordinary_objectives = xl_level_objectives_remaining();
	require(apply_damage_to_robot(&Objects[100], Objects[100].shields + F1_0, ConsoleObject - Objects) &&
	            !xl_boss_is_dying(100) && xl_level_objectives_remaining() == ordinary_objectives && !Control_center_destroyed,
	        "Ordinary HXM robot entered a stale boss lifecycle");
	StartNewGame(1);
	roles_match();
	require(load_base_mission("hxmtest", error, sizeof(error)), error);
	StartNewGame(1);
	roles_match();
	std::cout << "HXM boss roles passed: introduced/removed definitions, lethal combat, death roll, disk through stock and repeated normal load\n";
}
