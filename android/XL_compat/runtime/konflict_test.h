// Original mission feature checks, without navigation or a completion claim
#include "texture_brightness.h"
static int konflict_test(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission("konflict", error, sizeof(error)), error);
		require(Current_mission->enhanced == MISSION_ENHANCED_XL && Last_level == 1, "original XL descriptor identity lost");
		auto read_member = [](const char *name) {
			std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFS_openRead(name), PHYSFS_close);
			require(file && PHYSFS_fileLength(file.get()) > 0 && PHYSFS_fileLength(file.get()) < 1024 * 1024, "missing bounded acceptance member");
			xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file.get())));
			require(PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()), "incomplete acceptance member");
			return bytes;
		};
		const auto original_bytes = read_member("konflict.rl2");
		const auto original = xl_compat::inspect_level(original_bytes);
		const auto lgt = read_member("konflict.lgt");
		const auto authored = xl_compat::decode_texture_brightness(lgt.data(), lgt.size());
		require(xl_level_supported("konflict.rl2", 0), xl_level_rejection());
		require(!xl_level_supported("konflict.rl2", 1), "unqualified multiplayer admitted");
		std::strcpy(Players[Player_num].callsign, "XLTest");
		Difficulty_level = 2;
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_preview_pilot_active() && Num_segments == 505 && Num_vertices == 1603 && Num_walls == 54 && Num_triggers == 1,
		        "original Konflict runtime geometry differs");
		require(!std::strcmp(Gamesave_current_filename, "konflict.rl2"), "runtime lost original level identity");
		for (std::size_t i = 0; i < original.vertices.size(); ++i)
			require(Vertices[i].x == original.vertices[i][0] && Vertices[i].y == original.vertices[i][1] && Vertices[i].z == original.vertices[i][2], "translated vertex differs");
		for (std::size_t i = 0; i < original.segments.size(); ++i) {
			require(Segment2s[i].special == original.segments[i].function, "translated segment function differs");
			for (int corner = 0; corner < 8; ++corner)
				require(Segments[i].verts[corner] == original.segments[i].vertices[corner], "translated segment vertex differs");
		}
		int native_index = 0;
		for (const auto &index : original.light_indices) {
			int remaining = index[2], target = index[3];
			do {
				const int chunk = (std::min) (remaining, 127);
				require(native_index < Num_static_lights, "translated light index missing");
				const auto &actual = Dl_indices[native_index++];
				require(actual.segnum == index[0] && actual.sidenum == index[1] && actual.count == chunk && actual.index == target, "translated packed light index differs");
				remaining -= chunk;
				target += chunk;
			} while (remaining);
		}
		require(native_index == Num_static_lights, "extra translated light indices");
		for (std::size_t i = 0; i < original.delta_lights.size(); ++i) {
			const auto &expected = original.delta_lights[i];
			const auto &actual = Delta_lights[i];
			require(actual.segnum == expected[0] && actual.sidenum == expected[1], "translated delta target differs");
			for (int corner = 0; corner < 4; ++corner)
				require(actual.vert_light[corner] == expected[corner + 3], "translated delta contribution differs");
		}
		int brightness = -1;
		for (std::size_t i = 0; i < authored.size(); ++i)
			require(xl_prepared_texture_brightness(int(i), &brightness) && brightness == authored[i], "owned authored brightness table differs");
		require(xl_prepared_texture_brightness(44, &brightness) && brightness == 3930, "authored brightness override missing");
		require(xl_prepared_texture_brightness(275, &brightness) && brightness == 86460, "authored brightness replaced by default");
		int emitter_count = 0, changed_emitters = 0;
		for (int segment = 0; segment < Num_segments; ++segment)
			for (int side = 0; side < 6; ++side) {
				xl_texture_emitter emitter{};
				if (!xl_preview_texture_emitter(segment, side, &emitter)) continue;
				++emitter_count;
				int default_brightness = 0;
				for (const auto &entry : xl_compat::texture_brightness)
					if (entry.texture == emitter.texture) default_brightness = entry.brightness;
				if (authored[emitter.texture] != default_brightness) {
					if (++changed_emitters == 1)
						std::cout << "Konflict authored emitter witness: segment=" << segment << " side=" << side << " texture=" << emitter.texture << '\n';
				}
			}
		require(emitter_count > 0 && changed_emitters > 0, "authored brightness has no checked runtime consumers");
		std::cout << "Konflict geometry/light records match original; emitters=" << emitter_count << " authored-difference emitters=" << changed_emitters << '\n';
		const auto shadow = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "foreign-lighting";
		std::filesystem::create_directories(shadow);
		write_bytes((shadow / "konflict.lgt").string().c_str(), xl_compat::Bytes(lgt.size(), 0));
		require(PHYSFS_addToSearchPath(shadow.string().c_str(), 0), "cannot mount foreign brightness control");
		const auto geometry_before = std::string(reinterpret_cast<const char *>(Segments), sizeof(*Segments) * Num_segments);
		const auto sim_calls = d_rand_get_stream_call_count(D_RNG_SIM);
		require(!xl_level_supported("konflict.rl2", 0) && std::string(xl_level_rejection()).find("shadowed outside") != std::string::npos,
		        "foreign brightness shadow was not rejected helpfully");
		require(load_level("konflict.rl2") != 0, "foreign brightness shadow reached commit");
		require(xl_preview_pilot_active() && xl_prepared_texture_brightness(275, &brightness) && brightness == 86460 &&
		            geometry_before == std::string(reinterpret_cast<const char *>(Segments), sizeof(*Segments) * Num_segments) &&
		            sim_calls == d_rand_get_stream_call_count(D_RNG_SIM),
		        "failed brightness preflight changed active state");
		require(PHYSFS_removeFromSearchPath(shadow.string().c_str()), "cannot unmount foreign brightness control");
		require(xl_level_supported("konflict.rl2", 0), "brightness shadow recovery failed");
		std::cout << "Konflict: foreign brightness rejected before commit; owned lighting, geometry and RNG preserved\n";
		const auto light = Dl_indices[0];
		require(subtract_light(light.segnum, light.sidenum) == 1, "changing light did not switch off");
		const auto expected_lights = face_lights();
		const auto expected_energy = Players[Player_num].energy;
		char save[] = "xl-konflict.sav", description[] = "Konflict feature persistence";
		stop_time();
		require(state_save_all_sub(save, description) != 0, "Konflict save failed");
		add_light(light.segnum, light.sidenum);
		Players[Player_num].energy = 1;
		require(state_restore_all_sub(save, 0) != 0, "Konflict restore failed");
		require(xl_preview_pilot_active() && !std::strcmp(Current_mission_filename, "konflict") && Current_level_num == 1,
		        "restore lost XL mission identity");
		require(face_lights() == expected_lights && Players[Player_num].energy == expected_energy, "restore lost light or player state");
		require(xl_prepared_texture_brightness(275, &brightness) && brightness == 86460, "restore lost owned brightness");
		require(ControlCenterTriggers.num_links == original.reactor_targets.size(), "reactor target count differs");
		for (int i = 0; i < ControlCenterTriggers.num_links; ++i)
			require(ControlCenterTriggers.seg[i] == original.reactor_targets[i][0] && ControlCenterTriggers.side[i] == original.reactor_targets[i][1], "reactor target differs");
		require(Triggers[0].type == TT_EXIT && original.triggers[0].type == TT_EXIT, "original exit trigger was not preserved");
		object *reactor = nullptr;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_CNTRLCEN) reactor = &Objects[i];
		require(reactor && !Control_center_destroyed, "original live reactor missing");
		do_controlcen_destroyed_stuff(reactor);
		require(Control_center_destroyed && Countdown_timer > 0, "native reactor did not start countdown");
		for (int i = 0; i < ControlCenterTriggers.num_links; ++i) {
			const auto &side = Segments[ControlCenterTriggers.seg[i]].sides[ControlCenterTriggers.side[i]];
			require(side.wall_num >= 0 && Walls[side.wall_num].state != WALL_DOOR_CLOSED, "reactor did not toggle linked door");
		}
		std::cout << "Konflict: native reactor countdown and linked doors passed; exit trigger preserved\n";
		int exit_wall = -1;
		for (int i = 0; i < Num_walls; ++i)
			if (wall_trigger_index(Walls[i].trigger) == 0) exit_wall = i;
		require(exit_wall >= 0 && xl_test_stop_after_level(), "controlled exit requires bounded completion fixture");
		const auto &exit = Walls[exit_wall];
		check_trigger(&Segments[exit.segnum], exit.sidenum, Players[Player_num].objnum, 0);
		require(xl_preview_pilot_completed() && Players[Player_num].shields > 0, "native exit dispatch did not complete living player");
		std::cout << "Konflict: controlled native exit dispatch completed; route traversal remains unverified\n";
		std::cout << "Konflict: original descriptor, 17/36 startup, authored LGT, switched-light save/restore passed\n";
		require(load_base_mission("d2", error, sizeof(error)), error);
		require(load_level("d2leva-1.rl2") == 0, "stock recovery load failed");
		require(!xl_preview_pilot_active() && !xl_prepared_texture_brightness(275, &brightness), "Konflict state leaked into stock mission");
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		for (int variant = 0; variant < 4; ++variant) {
			// Derived controls, never substituted for the original acceptance fixture
			xl_compat::Bytes hog{ 'D', 'H', 'F' };
			auto member = [&](const char *name, const xl_compat::Bytes &bytes) {
				char filename[13] = {};
				std::strcpy(filename, name);
				hog.insert(hog.end(), filename, filename + sizeof(filename));
				for (unsigned shift = 0; shift < 32; shift += 8) hog.push_back(static_cast<unsigned char>(bytes.size() >> shift));
				hog.insert(hog.end(), bytes.begin(), bytes.end());
			};
			member("konflict.rl2", original_bytes);
			if (variant == 0) member("konflict.lgt", { 0 });
			if (variant == 1) member("konflict.clr", xl_compat::Bytes(910 * 13, 0));
			if (variant == 3) member("konflict.lgt", xl_compat::Bytes(910 * 4, 0));
			write_bytes((root / "kfcheck.hog").string().c_str(), hog);
			std::ofstream descriptor(root / "kfcheck.mn2");
			descriptor << "d2x-name = Companion control\nnum_levels = 1\nkonflict.rl2\n";
			descriptor.close();
			require(bool(descriptor) && load_base_mission("kfcheck", error, sizeof(error)), error);
			if (variant < 2) {
				require(!xl_level_supported("konflict.rl2", 0), "unsupported companion admitted");
				const std::string reason = xl_level_rejection();
				require(reason.find(variant == 0 ? "companion length" : "CLR companion overrides") != std::string::npos, "unsupported companion lacks helpful rejection");
				require(load_level("konflict.rl2") != 0 && !xl_preview_pilot_active(), "unsupported companion reached runtime");
			} else if (variant == 2) {
				require(xl_level_supported("konflict.rl2", 0) && load_level("konflict.rl2") == 0, "absent LGT did not retain default fallback");
				require(!xl_prepared_texture_brightness(275, &brightness), "absent LGT retained previous override");
			} else {
				require(xl_level_supported("konflict.rl2", 0) && load_level("konflict.rl2") == 0, "valid zero LGT rejected");
				require(xl_prepared_texture_brightness(275, &brightness) && brightness == 0, "zero override fell back to default");
				for (int segment = 0; segment < Num_segments; ++segment)
					for (int side = 0; side < 6; ++side) {
						xl_texture_emitter emitter{};
						require(!xl_preview_texture_emitter(segment, side, &emitter), "zero LGT did not disable runtime texture emitters");
					}
			}
			require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "companion control stock recovery failed");
		}
		std::cout << "Konflict derived controls: malformed LGT and unsupported CLR rejected; absent LGT uses defaults; zero LGT disables emitters\n";
		const auto loose_root = root.parent_path();
		write_bytes((loose_root / "konflict.rl2").string().c_str(), original_bytes);
		write_bytes((loose_root / "konflict.lgt").string().c_str(), lgt);
		{
			std::ofstream descriptor(root / "kfloose.mn2");
			descriptor << "d2x-name = Loose lighting control\nnum_levels = 1\nkonflict.rl2\n";
			descriptor.close();
			require(bool(descriptor), "cannot write loose lighting control descriptor");
		}
		require(load_base_mission("kfloose", error, sizeof(error)), error);
		require(!xl_level_supported("konflict.rl2", 0) && std::string(xl_level_rejection()).find("owned mission HOG") != std::string::npos,
		        "loose lighting companion was silently ignored");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "loose companion stock recovery failed");
		std::filesystem::remove(loose_root / "konflict.rl2");
		std::filesystem::remove(loose_root / "konflict.lgt");
		std::cout << "Konflict derived control: loose companion explicitly rejected\n";
		std::cout << "Konflict: stock isolation passed; uninterrupted playthrough and multiplayer unqualified\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "Konflict feature check failed: " << error.what() << '\n';
		return 1;
	}
}
