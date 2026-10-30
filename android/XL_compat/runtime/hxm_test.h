// Original Alarlof member and companion; controlled combat without navigation
extern "C" void set_robot_state(object *obj, int state);
extern "C" void ai_fire_laser_at_player(object *obj, vms_vector *fire_point, int gun_num, vms_vector *believed_player_pos);
static xl_compat::Bytes hxm_model_receipt(int id)
{
	xl_compat::Bytes bytes;
	const auto append = [&](const void *data, std::size_t size) {
		const auto *first = static_cast<const uint8_t *>(data);
		bytes.insert(bytes.end(), first, first + size);
	};
	auto header = Polygon_models[id];
	header.model_data = nullptr;
	append(&header, sizeof(header));
	append(Polygon_models[id].model_data, Polygon_models[id].model_data_size);
	append(ObjBitmaps, sizeof(ObjBitmaps));
	append(ObjBitmapPtrs, sizeof(ObjBitmapPtrs));
	append(Dying_modelnums, sizeof(Dying_modelnums));
	append(Dead_modelnums, sizeof(Dead_modelnums));
	return bytes;
}
static void hxm_require_supported()
{
	if (!xl_level_supported("alarlof.rl2", 0)) throw std::runtime_error(xl_level_rejection());
}
static void hxm_dependency_controls(const std::filesystem::path &root)
{
	auto *file = PHYSFSX_openReadBuffered("alarlof.hxm");
	require(file, "Original HXM companion missing");
	xl_compat::Bytes original(static_cast<std::size_t>(PHYSFS_fileLength(file)));
	require(PHYSFS_readBytes(file, original.data(), original.size()) == PHYSFS_sint64(original.size()) && PHYSFS_close(file), "Original HXM companion read failed");
	require(original.size() == 11262, "Original HXM size changed");
	const auto shadow = root / "hxm-shadow";
	std::filesystem::create_directories(shadow);
	const auto path = shadow / "alarlof.HXM";
	write_bytes(path.string().c_str(), original);
	require(PHYSFS_addToSearchPath(shadow.string().c_str(), 0), "Cannot mount private HXM controls");
	const auto before = gameplay_definitions();
	const auto model_before = hxm_model_receipt(6);
	const auto world = arena_state();
	const auto put16 = [](xl_compat::Bytes &bytes, std::size_t at, unsigned value) {
		bytes.at(at) = value & 255;
		bytes.at(at + 1) = (value >> 8) & 255;
	};
	for (int variant = 0; variant < 6; ++variant) {
		auto bytes = original;
		const char *reason = "HXM";
		if (variant == 0) bytes.back() = 1;
		if (variant == 1) bytes.resize(2000);
		if (variant == 2) {
			bytes[16 + 116] = 127;
			reason = "missing weapon";
		}
		if (variant == 3) {
			bytes[16 + 100] = MAX_SUBMODELS - 1;
			reason = "gun references missing submodel";
		}
		if (variant == 4) {
			bytes[628 + 452 + 1] = 1;
			reason = "parent chain";
		}
		if (variant == 5) {
			put16(bytes, 504, MAX_SUBMODELS - 1);
			reason = "joint references missing submodel";
		}
		write_bytes(path.string().c_str(), bytes);
		const bool admitted = xl_level_supported("alarlof.rl2", 0) != 0;
		if (admitted || !std::strstr(xl_level_rejection(), reason))
			throw std::runtime_error("Malformed HXM variant " + std::to_string(variant) + ": expected " + reason + ", got " + (admitted ? "admitted" : xl_level_rejection()));
		require(gameplay_definitions() == before && hxm_model_receipt(6) == model_before && arena_state() == world, "Rejected HXM mutated live definitions or combat state");
	}
	auto empty = original;
	put16(empty, 16 + 296, 0);
	put16(empty, 16 + 298, 65535);
	write_bytes(path.string().c_str(), empty);
	require(xl_level_supported("alarlof.rl2", 0), "Unused empty joint offset rejected");
	// Capability must be selected before changed definitions are walked
	auto boss = original;
	boss[16 + 280] = 21;
	write_bytes(path.string().c_str(), boss);
	XlPreparedResources supported_boss("alarlof.rl2");
	require(supported_boss.dependencies().robot_data[3].boss_flag == 21, "Supported custom boss profile was not staged");
	boss[16 + 280] = uint8_t(-21);
	write_bytes(path.string().c_str(), boss);
	require(!xl_level_supported("alarlof.rl2", 0) && std::strstr(xl_level_rejection(), "boss profile"), "Unimplemented signed boss lifecycle admitted");
	require(gameplay_definitions() == before && arena_state() == world, "HXM capability checks mutated live state");
	require(PHYSFS_removeFromSearchPath(shadow.string().c_str()), "Cannot unmount private HXM controls");
	hxm_require_supported();
	std::cout << "HXM malformed payload, dependency, gun/parent/joint, empty animation and boss capability controls passed\n";
}

static void hxm_projectile_controls(const std::filesystem::path &root)
{
	const int model = Weapon_info[CONCUSSION_ID].model_num;
	const auto base_radius = Polygon_models[model].rad;
	std::ifstream input(root / "missions/hxmtest.hog", std::ios::binary);
	const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	xl_compat::Bytes derived{ 'D', 'H', 'F' };
	unsigned changed = 0;
	for (auto member : xl_compat::read_hog(original)) {
		if (!d_stricmp(member.name.c_str(), "alarlof.hxm")) {
			require(member.bytes.size() == 11262 && member.bytes[624] == 6, "Derived projectile HXM source changed");
			// Keep the custom model/textures; omit robot/joint overrides which need it
			xl_compat::Bytes projectile(member.bytes.begin(), member.bytes.begin() + 8);
			projectile.resize(16, 0);
			projectile.insert(projectile.end(), member.bytes.begin() + 620, member.bytes.end());
			for (unsigned shift = 0; shift < 32; shift += 8) projectile[20 + shift / 8] = uint8_t(unsigned(model) >> shift);
			member.bytes = std::move(projectile);
			++changed;
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "Derived HXM member name too long");
		std::strcpy(name, member.name.c_str());
		derived.insert(derived.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) derived.push_back(uint8_t(member.bytes.size() >> shift));
		derived.insert(derived.end(), member.bytes.begin(), member.bytes.end());
	}
	require(changed == 1, "Derived projectile fixture changed no HXM");
	write_bytes((root / "missions/hxmshot.hog").string().c_str(), derived);
	{
		std::ofstream descriptor(root / "missions/hxmshot.mn2", std::ios::binary);
		descriptor << "d2x-name = Derived HXM projectile\nnum_levels = 1\nalarlof.rl2\n";
	}
	char error[256]{};
	require(load_base_mission("hxmshot", error, sizeof(error)), error);
	hxm_require_supported();
	StartNewGame(1);
	const auto fire = [&]() {
		const int shot = Laser_create_new(&ConsoleObject->orient.fvec, &ConsoleObject->pos, ConsoleObject->segnum,
		                                  ConsoleObject - Objects, CONCUSSION_ID, 0);
		require(shot >= 0 && Objects[shot].rtype.pobj_info.model_num == model &&
		            Objects[shot].size == fixdiv(325446, Weapon_info[CONCUSSION_ID].po_len_to_width_ratio),
		        "Custom projectile did not use computed XL radius");
		require(Polygon_models[model].rad == base_radius, "Custom projectile overwrote base model radius");
		obj_delete(shot);
	};
	fire();
	char save[] = "hxmshot.sav", description[] = "Custom projectile model";
	stop_time();
	require(state_save_all_sub(save, description), "Custom projectile save failed");
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Projectile stock transition failed");
	const int stock_shot = Laser_create_new(&ConsoleObject->orient.fvec, &ConsoleObject->pos, ConsoleObject->segnum,
	                                        ConsoleObject - Objects, CONCUSSION_ID, 0);
	require(stock_shot >= 0 && Objects[stock_shot].size == fixdiv(base_radius, Weapon_info[CONCUSSION_ID].po_len_to_width_ratio),
	        "Custom projectile radius polluted stock firing");
	obj_delete(stock_shot);
	require(state_restore_all_sub(save, 0), "Custom projectile restore through stock failed");
	require(!game_is_time_paused(), "Native save/restore left the simulation paused");
	fire();
	std::cout << "Derived HXM projectile computed radius, firing, disk through stock and stock firing isolation passed\n";
}

static int hxm_test(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		require(load_base_mission("d2", error, sizeof(error)), error);
		const auto stock_robot = Robot_info[3];
		const auto stock_radius = Polygon_models[6].rad;
		require(stock_radius == 316909, "HXM stock model radius changed");
		const auto stock_definitions = gameplay_definitions();
		const auto stock_model = hxm_model_receipt(6);
		require(load_base_mission("hxmtest", error, sizeof(error)), error);
		hxm_require_supported();
		require(gameplay_definitions() == stock_definitions && hxm_model_receipt(6) == stock_model, "HXM preflight changed live definitions");
		new_player_config();
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active(), "Original HXM normal game startup failed");
		Game_suspended = SUSP_ROBOTS;
		constexpr int actor = 100;
		auto check_actor = [&]() {
			const auto &robot = Objects[actor];
			require(robot.type == OBJ_ROBOT && robot.id == 3 && robot.segnum == 639, "Original custom actor identity changed");
			require(Robot_info[3].strength == i2f(750) && Robot_info[3].n_guns == 8 &&
			            Robot_info[3].weapon_type == 22 && Robot_info[3].weapon_type2 == 15,
			        "Original custom combat definition not installed");
			require(robot.rtype.pobj_info.model_num == 6 && robot.size == stock_radius &&
			            Polygon_models[6].rad == stock_radius && Polygon_models[6].model_data_size == 9844,
			        "Original custom model/base collision radius not installed");
			require(xl_custom_model_radius(6, stock_radius) == 325446, "Custom projectile radius differs from XL source");
			require(robot.mtype.phys_info.mass == Robot_info[3].mass && robot.mtype.phys_info.drag == Robot_info[3].drag,
			        "Custom actor physics not initialized");
		};
		check_actor();
		require(Objects[actor].shields == i2f(750), "Custom health was replaced with serialized or stock health");
		for (int state = 0; state < N_ANIM_STATES; ++state) {
			set_robot_state(&Objects[actor], state);
			for (int gun = 0; gun < Robot_info[3].n_guns; ++gun) {
				vms_vector muzzle;
				calc_gun_point(&muzzle, &Objects[actor], gun);
				require(find_point_seg(&muzzle, Objects[actor].segnum) >= 0, "Custom gun transform left the mine");
			}
		}
		for (const int gun : { 1, 0 }) {
			const int weapon = gun ? Robot_info[3].weapon_type : Robot_info[3].weapon_type2;
			vms_vector muzzle;
			calc_gun_point(&muzzle, &Objects[actor], gun);
			Ai_local_info[actor].next_fire = Ai_local_info[actor].next_fire2 = 0;
			ai_fire_laser_at_player(&Objects[actor], &muzzle, gun, &ConsoleObject->pos);
			int shot = -1;
			for (int i = 0; i <= Highest_object_index; ++i)
				if (Objects[i].type == OBJ_WEAPON && Objects[i].id == weapon && Objects[i].ctype.laser_info.parent_num == actor) {
					require(shot < 0, "Custom firing created duplicate projectiles");
					shot = i;
				}
			require(shot >= 0 && (gun ? Ai_local_info[actor].next_fire : Ai_local_info[actor].next_fire2) > 0,
			        "Custom actor AI failed to select/fire its configured weapon and arm its cooldown");
			obj_delete(shot);
		}
		require(!apply_damage_to_robot(&Objects[actor], i2f(37), ConsoleObject - Objects) && Objects[actor].shields == i2f(713),
		        "Custom actor combat ignored replacement health");
		const auto custom_definitions = gameplay_definitions();
		const auto custom_model = hxm_model_receipt(6);
		require(custom_model != stock_model, "HXM model/texture replacement left stock content installed");
		char save[] = "hxmtest.sav", description[] = "Original custom robot combat";
		stop_time();
		require(state_save_all_sub(save, description), "Custom gameplay save failed");
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(),
		        "Custom gameplay stock transition failed");
		require(Robot_info[3].strength == stock_robot.strength && Polygon_models[6].rad == stock_radius &&
		            xl_custom_model_radius(6, stock_radius) == stock_radius && gameplay_definitions() == stock_definitions && hxm_model_receipt(6) == stock_model,
		        "HXM definitions or custom radius leaked into stock gameplay");
		require(state_restore_all_sub(save, 0), "Custom gameplay restore through stock failed");
		check_actor();
		require(Objects[actor].shields == i2f(713) && gameplay_definitions() == custom_definitions && hxm_model_receipt(6) == custom_model,
		        "HXM restore lost earned combat or replacement definitions");
		require(!game_is_time_paused(), "Native save/restore left the simulation paused");
		require(apply_damage_to_robot(&Objects[actor], i2f(714), ConsoleObject - Objects), "Restored custom actor cannot be killed");
		StartNewGame(1);
		check_actor();
		require(Objects[actor].shields == i2f(750) && hxm_model_receipt(6) == custom_model, "Repeated HXM game retained old combat state or stale model/texture resources");
		hxm_dependency_controls(root);
		hxm_projectile_controls(root);
		std::cout << "Original HXM normal startup, custom model/guns/projectiles/combat, disk through stock and repeated load passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "HXM gameplay failed: " << error.what() << '\n';
		return 1;
	}
}
