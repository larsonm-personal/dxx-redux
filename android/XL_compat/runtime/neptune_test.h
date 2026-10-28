// Original mission feature checks, without navigation or a completion claim
#include "texture_brightness.h"
extern "C" {
#include "fuelcen.h"
#include "collide.h"
#include "powerup.h"
}
static int neptune_test(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission("neptunxl", error, sizeof(error)), error);
		require(Current_mission->enhanced == MISSION_ENHANCED_XL && Last_level == 1, "original XL descriptor identity lost");
		auto read_member = [](const char *name) {
			std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFS_openRead(name), PHYSFS_close);
			require(file && PHYSFS_fileLength(file.get()) > 0 && PHYSFS_fileLength(file.get()) < 1024 * 1024, "missing bounded acceptance member");
			xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file.get())));
			require(PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()), "incomplete acceptance member");
			return bytes;
		};
		const auto original_bytes = read_member("neptunxl.rl2");
		const auto original = xl_compat::inspect_level(original_bytes);
		const auto lgt = read_member("neptunxl.lgt");
		const auto clr = read_member("neptunxl.clr");
		const auto pog = read_member("neptunxl.pog");
		const std::array<int, 7> pog_indices{ 1243, 1246, 1256, 1289, 1290, 1291, 1442 };
		const std::array<unsigned, 7> pog_hashes{ 1802764526u, 928977817u, 929496110u, 3423223186u, 4198776589u, 547759460u, 3566167754u };
		auto check_pog = [&](bool expected) {
			for (std::size_t i = 0; i < pog_indices.size(); ++i) {
				unsigned bytes = 0, hash = 0;
				const bool installed = xl_prepared_pog_receipt(pog_indices[i], &bytes, &hash) != 0;
				require(installed == expected, "POG installed ownership differs");
				if (expected) require(bytes == 4096 && hash == pog_hashes[i], "installed POG payload differs from original");
			}
		};
		const auto authored = xl_compat::decode_texture_brightness(lgt.data(), lgt.size());
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
		require(!xl_level_supported("neptunxl.rl2", 1), "unqualified multiplayer admitted");
		std::strcpy(Players[Player_num].callsign, "XLTest");
		Difficulty_level = 2;
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_preview_pilot_active() && Num_segments == 93 && Num_vertices == 287 && Num_walls == 30 && Num_triggers == 1,
		        "original Neptune runtime geometry differs");
		require(!std::strcmp(Gamesave_current_filename, "neptunxl.rl2"), "runtime lost original level identity");
		check_pog(true);
		// Exercise authored station functions with bounded frame time, without navigation
		const auto prior_frame_time = FrameTime;
		FrameTime = F1_0 / 60;
		int fuel_stations = 0, repair_stations = 0;
		for (int segment = 0; segment < Num_segments; ++segment) {
			const auto fuel = fuelcen_give_fuel(&Segments[segment], F1_0);
			const auto repair = repaircen_give_shields(&Segments[segment], F1_0);
			require((fuel > 0) == (Segment2s[segment].special == SEGMENT_IS_FUELCEN), "fuel station behavior differs from authored function");
			require((repair > 0) == (Segment2s[segment].special == SEGMENT_IS_REPAIRCEN), "repair station behavior differs from authored function");
			require(fuel <= F1_0 && repair <= F1_0, "station exceeded requested resource amount");
			fuel_stations += fuel > 0;
			repair_stations += repair > 0;
			require(fuelcen_give_fuel(&Segments[segment], 0) == 0 && repaircen_give_shields(&Segments[segment], 0) == 0, "full player still receives station resources");
		}
		FrameTime = prior_frame_time;
		require(fuel_stations == 3 && repair_stations == 0, "original station coverage changed");
		std::cout << "Neptune: all three fuel stations and absent repair delivery passed with ordinary-segment controls\n";
		for (std::size_t i = 0; i < original.vertices.size(); ++i)
			require(Vertices[i].x == original.vertices[i][0] && Vertices[i].y == original.vertices[i][1] && Vertices[i].z == original.vertices[i][2], "translated vertex differs");
		for (std::size_t i = 0; i < original.segments.size(); ++i) {
			require(Segment2s[i].special == original.segments[i].function, "translated segment function differs");
			for (int corner = 0; corner < 8; ++corner)
				require(Segments[i].verts[corner] == original.segments[i].vertices[corner], "translated segment vertex differs");
		}
		require_owned_light_records(original);
		int brightness = -1;
		for (std::size_t i = 0; i < authored.size(); ++i)
			require(xl_prepared_texture_brightness(int(i), &brightness) && brightness == authored[i], "owned authored brightness table differs");
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
						std::cout << "Neptune authored emitter witness: segment=" << segment << " side=" << side << " texture=" << emitter.texture << '\n';
				}
			}
		require(emitter_count > 0, "authored brightness has no checked runtime consumers");
		std::cout << "Neptune geometry/light records match original; emitters=" << emitter_count << " authored-difference emitters=" << changed_emitters << '\n';
		for (const char *extension : { ".lgt", ".clr" }) {
			const auto shadow = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / (std::string("foreign") + extension);
			std::filesystem::create_directories(shadow);
			write_bytes((shadow / (std::string("neptunxl") + extension)).string().c_str(), extension[1] == 'l' ? lgt : clr);
			require(PHYSFS_addToSearchPath(shadow.string().c_str(), 0), "cannot mount foreign companion control");
			const auto before = face_lights();
			const auto calls = d_rand_get_stream_call_count(D_RNG_SIM);
			require(!xl_level_supported("neptunxl.rl2", 0) && std::string(xl_level_rejection()).find("shadowed outside") != std::string::npos, "foreign companion was not rejected helpfully");
			require(load_level("neptunxl.rl2") != 0 && xl_preview_pilot_active() && face_lights() == before && d_rand_get_stream_call_count(D_RNG_SIM) == calls, "foreign companion changed live state");
			require(PHYSFS_removeFromSearchPath(shadow.string().c_str()), "cannot unmount foreign companion control");
		}
		xl_light_index light{};
		require(xl_light_index_get(0, &light), "Missing owned light source");
		require(subtract_light(light.segment, light.side) == 1, "changing light did not switch off");
		const auto expected_lights = face_lights();
		const auto expected_energy = Players[Player_num].energy;
		char save[] = "xl-neptunxl.sav", description[] = "Neptune feature persistence";
		stop_time();
		require(state_save_all_sub(save, description) != 0, "Neptune save failed");
		add_light(light.segment, light.side);
		Players[Player_num].energy = 1;
		require(state_restore_all_sub(save, 0) != 0, "Neptune restore failed");
		require(xl_preview_pilot_active() && !std::strcmp(Current_mission_filename, "neptunxl") && Current_level_num == 1,
		        "restore lost XL mission identity");
		require(face_lights() == expected_lights && Players[Player_num].energy == expected_energy, "restore lost light or player state");
		check_pog(true);
		require(ControlCenterTriggers.num_links == original.reactor_targets.size(), "reactor target count differs");
		const auto pickup_player = Players[Player_num];
		std::set<int> tested_powerups;
		for (int i = 0; i <= Highest_object_index; ++i) {
			auto &object = Objects[i];
			if (object.type != OBJ_POWERUP || !tested_powerups.insert(object.id).second) continue;
			Players[Player_num] = pickup_player;
			require(do_powerup(&object) != 0, "original powerup was not collectible by fresh player");
		}
		require(tested_powerups.size() == 11, "original powerup-kind coverage changed");
		require(state_restore_all_sub(save, 0) != 0 && xl_preview_pilot_active(), "pickup controls could not restore original checkpoint");
		std::cout << "Neptune: all eleven authored powerup kinds collected; checkpoint restored\n";
		for (int i = 0; i < ControlCenterTriggers.num_links; ++i)
			require(ControlCenterTriggers.seg[i] == original.reactor_targets[i][0] && ControlCenterTriggers.side[i] == original.reactor_targets[i][1], "reactor target differs");
		require(Triggers[0].type == TT_EXIT && original.triggers[0].type == TT_EXIT, "original exit trigger was not preserved");
		object *reactor = nullptr;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_CNTRLCEN) reactor = &Objects[i];
		require(reactor && !Control_center_destroyed, "original live reactor missing");
		int tested_doors = 0, locked_doors = 0;
		for (int i = 0; i < Num_walls; ++i) {
			auto &wall = Walls[i];
			if (wall.type != WALL_DOOR) continue;
			const auto result = wall_hit_process(&Segments[wall.segnum], wall.sidenum, 0, Player_num, ConsoleObject);
			if (wall.flags & WALL_DOOR_LOCKED) {
				++locked_doors;
				require(result == WHP_NO_KEY && wall.state == WALL_DOOR_CLOSED, "locked original door opened before reactor");
			} else {
				++tested_doors;
				require(result == WHP_DOOR && wall.state == WALL_DOOR_OPENING, "ordinary original door did not open on interaction");
			}
		}
		require(tested_doors > 0 && locked_doors > 0, "original door controls were not exercised");
		std::cout << "Neptune: authored door interactions passed; ordinary=" << tested_doors << " locked=" << locked_doors << '\n';
		const auto reactor_shields = reactor->shields;
		require(reactor_shields > F1_0, "original reactor has no damageable shield reserve");
		apply_damage_to_controlcen(reactor, F1_0, Players[Player_num].objnum);
		require(reactor->shields == reactor_shields - F1_0 && !Control_center_destroyed, "nonlethal player reactor damage failed");
		apply_damage_to_controlcen(reactor, reactor->shields + F1_0, Players[Player_num].objnum);
		require(Control_center_destroyed && Countdown_timer > 0, "native reactor did not start countdown");
		for (int i = 0; i < ControlCenterTriggers.num_links; ++i) {
			const auto &side = Segments[ControlCenterTriggers.seg[i]].sides[ControlCenterTriggers.side[i]];
			require(side.wall_num >= 0 && Walls[side.wall_num].state != WALL_DOOR_CLOSED, "reactor did not toggle linked door");
		}
		std::cout << "Neptune: native reactor countdown and linked doors passed; exit trigger preserved\n";
		int exit_wall = -1;
		for (int i = 0; i < Num_walls; ++i)
			if (wall_trigger_index(Walls[i].trigger) == 0) exit_wall = i;
		require(exit_wall >= 0 && xl_test_stop_after_level(), "controlled exit requires bounded completion fixture");
		const auto &exit = Walls[exit_wall];
		check_trigger(&Segments[exit.segnum], exit.sidenum, Players[Player_num].objnum, 0);
		require(xl_preview_pilot_completed() && Players[Player_num].shields > 0, "native exit dispatch did not complete living player");
		std::cout << "Neptune: controlled native exit dispatch completed; route traversal remains unverified\n";
		std::cout << "Neptune: original descriptor, 23/40 startup, authored LGT, switched-light save/restore passed\n";
		require(load_base_mission("d2", error, sizeof(error)), error);
		require(load_level("d2leva-1.rl2") == 0, "stock recovery load failed");
		require(!xl_preview_pilot_active() && !xl_prepared_texture_brightness(275, &brightness), "Neptune state leaked into stock mission");
		check_pog(false);
		require(state_restore_all_sub(save, 0) != 0 && xl_preview_pilot_active() && face_lights() == expected_lights, "stock-to-Neptune save restoration failed");
		check_pog(true);
		for (std::size_t i = 0; i < authored.size(); ++i)
			require(xl_prepared_texture_brightness(int(i), &brightness) && brightness == authored[i], "restored authored brightness differs");
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		for (int variant = 0; variant < 3; ++variant) {
			require(load_base_mission("d2", error, sizeof(error)), error);
			xl_compat::Bytes hog{ 'D', 'H', 'F' };
			auto append = [&](const char *name, const xl_compat::Bytes &bytes) {
				char field[13] = {};
				std::strcpy(field, name);
				hog.insert(hog.end(), field, field + sizeof(field));
				for (unsigned shift = 0; shift < 32; shift += 8) hog.push_back(static_cast<unsigned char>(bytes.size() >> shift));
				hog.insert(hog.end(), bytes.begin(), bytes.end());
			};
			append("neptunxl.rl2", original_bytes);
			append("neptunxl.lgt", lgt);
			append("neptunxl.pog", pog);
			if (variant == 0) append("neptunxl.clr", { 0 });
			if (variant == 1) append("neptunxl.clr", xl_compat::Bytes(910 * 13, 0x55));
			write_bytes((root / "npcheck.hog").string().c_str(), hog);
			std::ofstream descriptor(root / "npcheck.mn2");
			descriptor << "d2x-name = CLR precedence control\nnum_levels = 1\nneptunxl.rl2\n";
			descriptor.close();
			require(load_base_mission("npcheck", error, sizeof(error)), error);
			if (variant == 0) {
				require(!xl_level_supported("neptunxl.rl2", 0) && std::string(xl_level_rejection()).find("companion length") != std::string::npos, "malformed owned CLR accepted");
				continue;
			}
			require(xl_level_supported("neptunxl.rl2", 0) && load_level("neptunxl.rl2") == 0, "conflicting or absent CLR wrongly blocked");
			for (int segment = 0; segment < Num_segments; ++segment)
				for (int side = 0; side < 6; ++side) {
					xl_texture_emitter emitter{};
					if (!xl_preview_texture_emitter(segment, side, &emitter)) continue;
					for (int channel = 0; channel < 3; ++channel)
						require(emitter.rgb[channel] == float(original.texture_colors[emitter.texture].rgb[channel]), "CLR changed embedded emitter color");
				}
		}
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0 && !xl_preview_pilot_active(), "final stock recovery failed");
		std::cout << "Neptune resource controls: foreign LGT/CLR rejected without state changes; malformed owned CLR rejected; conflicting/absent CLR preserve embedded emitter colors; stock-to-save restoration passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
