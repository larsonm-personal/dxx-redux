// Original level bytes with HXM removed only in the derived layout fixture
static int format34_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		std::ifstream input(root / "orig34.hog", std::ios::binary);
		xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() }, derived{ 'D', 'H', 'F' };
		unsigned removed = 0;
		for (const auto &member : xl_compat::read_hog(original)) {
			std::string lower = member.name;
			std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (lower == "alarlof.hxm") {
				++removed;
				continue;
			}
			char name[13]{};
			require(member.name.size() < sizeof(name), "15/34 member name exceeds HOG capacity");
			std::strcpy(name, member.name.c_str());
			derived.insert(derived.end(), name, name + sizeof(name));
			for (unsigned shift = 0; shift < 32; shift += 8) derived.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
			derived.insert(derived.end(), member.bytes.begin(), member.bytes.end());
		}
		require(removed == 1, "15/34 derived fixture did not remove exactly the unqualified HXM");
		write_bytes((root / "format34.hog").string().c_str(), derived);
		{
			std::ofstream descriptor(root / "format34.mn2", std::ios::binary);
			descriptor << "d2x-name = Derived format 15-34\nnum_levels = 1\nalarlof.rl2\n";
		}
		require(load_base_mission("orig34", error, sizeof(error)), error);
		require(!xl_level_supported("alarlof.rl2", 0) && std::string(xl_level_rejection()).find("HXM definitions") != std::string::npos, "Original HXM gate was bypassed");
		std::cout << "Original Alarlof HXM remains rejected; derived layout fixture omits only that member\n";
		require(load_base_mission("format34", error, sizeof(error)), error);
		if (!xl_level_supported("alarlof.rl2", 0)) throw std::runtime_error(xl_level_rejection());
		require(!xl_level_supported("alarlof.rl2", 1), "15/34 multiplayer admitted");
		auto *file = PHYSFS_openRead("alarlof.rl2");
		require(file, "15/34 original member missing");
		xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file)));
		require(PHYSFS_readBytes(file, bytes.data(), bytes.size()) == static_cast<PHYSFS_sint64>(bytes.size()) && PHYSFS_close(file), "15/34 source read failed");
		const auto source = xl_compat::inspect_level(bytes);
		require(source.level_version == 15 && source.game_version == 34 && source.segments.size() == 803 && source.light_indices.size() == 68, "15/34 original identity changed");
		require(!load_level("alarlof.rl2"), "15/34 load failed");
		require(xl_level_extensions_active() && Num_segments == source.segments.size() && Num_vertices == source.vertices.size() && Num_walls == source.walls.size() && Num_triggers == source.triggers.size(), "15/34 native counts differ");
		require(source.level_name == Current_level_name, "15/34 header shifted level name");
		for (std::size_t i = 0; i < source.vertices.size(); ++i)
			require(Vertices[i].x == source.vertices[i][0] && Vertices[i].y == source.vertices[i][1] && Vertices[i].z == source.vertices[i][2], "15/34 vertex changed");
		for (std::size_t i = 0; i < source.segments.size(); ++i) {
			require((Segment2s[i].s2_flags & ~3) == (source.segments[i].flags & ~3), "15/34 reserved flags lost");
			for (int side = 0; side < 6; ++side)
				require(Segments[i].children[side] == source.segments[i].children[side], "15/34 child changed");
		}
		int native = 0;
		for (const auto &row : source.light_indices) {
			int remaining = row[2], target = row[3];
			do {
				const int chunk = std::min(remaining, 127);
				require(native < Num_static_lights && Dl_indices[native].segnum == row[0] && Dl_indices[native].sidenum == row[1] && Dl_indices[native].count == chunk && Dl_indices[native].index == target, "15/34 native packed index differs");
				++native;
				remaining -= chunk;
				target += chunk;
			} while (remaining);
		}
		require(native == Num_static_lights, "15/34 native light count differs");
		for (std::size_t i = 0; i < source.delta_lights.size(); ++i) {
			const auto &row = source.delta_lights[i];
			const auto &actual = Delta_lights[i];
			require(actual.segnum == row[0] && actual.sidenum == row[1], "15/34 delta target differs");
			for (int corner = 0; corner < 4; ++corner) require(actual.vert_light[corner] == row[corner + 3], "15/34 delta contribution differs");
		}
		changing_lights(source, "alarlof.rl2", 68);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active(), "15/34 normal game startup failed");
		Game_suspended = SUSP_ROBOTS;
		Players[Player_num].shields = i2f(73);
		Players[Player_num].energy = i2f(61);
		const auto light = source.light_indices.front();
		require(subtract_light(light[0], light[1]) == 1, "15/34 earned light state failed");
		const auto expected = face_lights();
		char save[] = "format34.sav", description[] = "XL 15/34 light state";
		stop_time();
		require(state_save_all_sub(save, description), "15/34 save failed");
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "15/34 stock transition leaked runtime");
		require(state_restore_all_sub(save, 0) && xl_level_extensions_active() && !std::strcmp(Current_mission_filename, "format34") && !std::strcmp(Gamesave_current_filename, "alarlof.rl2"), "15/34 saved source not restored");
		require(face_lights() == expected && (Light_subtracted[light[0]] & (1 << light[1])) && Players[Player_num].shields == i2f(73) && Players[Player_num].energy == i2f(61), "15/34 saved light/player state differs");
		start_time();
		require(add_light(light[0], light[1]) == 1, "15/34 restored light cannot be enabled");
		std::cout << "XL 15/34 native geometry, 68 packed light sources, 5083 deltas and disk-through-stock controls passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "XL 15/34 failed: " << e.what() << '\n';
		return 1;
	}
}
