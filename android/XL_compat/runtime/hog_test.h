#include "../tests/hog_fixture.h"
#include "../hog.h"
#include "recorded_future_test.h"
#include "hog_checkpoint_test.h"
#include "original_lighting_test.h"
#include <charconv>
static xl_compat::Bytes hog_test_read(const char *name)
{
	std::unique_ptr<PHYSFS_File, decltype(&PHYSFS_close)> file(PHYSFS_openRead(name), PHYSFS_close);
	require(file && PHYSFS_fileLength(file.get()) >= 0, "Cannot open archive member");
	xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file.get())));
	require(PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) == static_cast<PHYSFS_sint64>(bytes.size()), "Cannot read full archive member");
	return bytes;
}

static void original_lighting_controls(const std::filesystem::path &root)
{
	char error[256]{};
	new_player_config();
	input_demo_set_skip_level_intro(1);
	const auto start = [&](const char *mission, int ordinal) {
		require(load_base_mission(mission, error, sizeof(error)), error);
		StartNewGame(ordinal);
		require(Current_level_num == ordinal && mission_owns_resource_archive(PHYSFS_getRealDir(Gamesave_current_filename)), "Lighting control lost original ordinal or HOG");
	};
	start("vignette-xl", 6);
	const auto before = xl_original_lighting_face(113, 1);
	require(!xl_level_extensions_active() && xl_classic_lighting_vertex_count() == unsigned(Num_vertices) &&
	            before.at("brightness") == 83840 && before.at("raw_rgb") == std::array<int, 3>{ 2054847098, 2054847098, 2054847098 } &&
	            before.at("sources").at("d2x-xl.hog/descent2.clr") == "a0cefca9be4746f2bd271f7b21f6687e18cb4b2e7b544c7190fa127346aa454e",
	        "Native Vignette did not install the original brightness and startup colors");
	const auto original_lights = face_lights();
	stop_time();
	require(game_is_time_paused() && xl_original_lighting_face(113, 1) == before, "Pause changed the installed face");
	start_time();
	require(subtract_light(113, 1) && face_lights() != original_lights && !xl_original_lighting_face(113, 1).contains("emitter"), "Native light switch did not apply its deltas");
	require(add_light(113, 1) && xl_original_lighting_face(113, 1).at("sources") == before.at("sources"), "Native light restoration changed source ownership");
	char save[] = "vignette-light.sav", description[] = "Native lighting";
	stop_time();
	require(state_save_all_sub(save, description), "Cannot save native original lighting");
	StartNewLevelSub(2, 1, 0);
	require(!xl_prepared_classic_lighting() && xl_level_extensions_active(), "Native lighting survived into an extended mine");
	require(state_restore_all_sub(save, 0) && Current_level_num == 6 && before.at("raw_rgb") == xl_original_lighting_face(113, 1).at("raw_rgb"), "Disk restore lost native original colors");
	const auto restored = xl_original_lighting_face(113, 1);

	// Use Vignette's native Standoff geometry in both lighting controls. Chaos also contains classic lightning objects.
	// The original POGs remain in the readiness corpus. Only these isolated controls omit them.
	const auto standoff_geometry = hog_test_read("standoff.rl2");
	for (const auto &pair : { std::pair{ "ctheo-xl", "ctlight" }, std::pair{ "vignette-xl", "vglight" } }) {
		std::ifstream input(root / "missions" / (std::string(pair.first) + ".hog"), std::ios::binary);
		require(bool(input), "Cannot open original Standoff distribution");
		const xl_compat::Bytes archive{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
		xl_compat::Bytes control{ 'D', 'H', 'F' };
		for (const auto &member : xl_compat::read_hog(archive)) {
			auto name = xl_compat::hog_key(member.name);
			if (name == "standoff.rl2")
				xl_test::hog_member(control, name, standoff_geometry, false);
			else if (name == "standoff.lgt" || name == "standoff.clr" || name == "standoff.hxm")
				xl_test::hog_member(control, name, member.bytes, false);
		}
		write_bytes((root / "missions" / (std::string(pair.second) + ".hog")).string().c_str(), control);
		std::ofstream descriptor(root / "missions" / (std::string(pair.second) + ".mn2"));
		descriptor << "d2x-name = " << pair.second << " light control\nnum_levels = 1\nstandoff.rl2\n";
		require(bool(descriptor), "Cannot write component descriptor");
	}
	const std::array<int, 3> chaos{ 33686018, 917943990, 1650614882 }, vignette{ 0, 0, 1684300900 };
	const auto examine = [&](const char *name, int segment, const auto &rgb) {
		start(name, 1);
		const auto clr = hog_test_read("standoff.clr"), lgt = hog_test_read("standoff.lgt");
		const auto colors = xl_compat::decode_texture_colors(clr.data(), clr.size());
		const auto *installed = xl_prepared_classic_lighting();
		require(installed && installed->brightness == xl_compat::decode_texture_brightness(lgt.data(), lgt.size()), "Installed Standoff brightness differs from the selected HOG");
		for (std::size_t index = 0; index < colors.size(); ++index) {
			const auto &actual = installed->colors->at(index);
			require(actual.channels == colors[index].channels && actual.serialized_index == colors[index].serialized_index, "Installed Standoff colors differ from the selected HOG");
		}
		const auto result = xl_original_lighting_face(segment, 2);
		require(result.at("texture") == 380 && result.at("brightness") == 32750 && result.at("raw_rgb") == rgb, "Light from the other Standoff HOG leaked into this one");
		return result;
	};
	const auto first = examine("ctlight", 235, chaos);
	auto *mission = mission_prepare_by_name("vglight");
	require(mission != nullptr, "Cannot prepare incoming Vignette lighting control");
	{
		struct Discard {
			mission_preparation **mission;
			~Discard()
			{
				mission_discard_prepared(mission);
			}
		} discard{ &mission };
		auto incoming = XlPreparedResources::prepare_level(mission, 1);
		XlPreparedResources moved(std::move(*incoming));
		incoming.reset();
		require(moved.classic_lighting() && moved.classic_lighting()->colors->at(380).channels == vignette &&
		            xl_original_lighting_face(235, 2) == first,
		        "Incoming target read or changed the live lighting");
	}
	require(xl_original_lighting_face(235, 2) == first && !xl_level_supported_at("standoff.rl2", 0, 2) && xl_original_lighting_face(235, 2) == first,
	        "Discard or rejected signed ordinal changed native lighting");
	const auto second = examine("vglight", 235, vignette);
	require(examine("ctlight", 235, chaos) == first, "Return to the first Standoff HOG changed its displayed colors");
	start("kkr-xl", 1);
	require(!xl_prepared_classic_lighting() && xl_flicker_count() == 36, "Original flicker control lost its level-owned records");
	const auto previous = flicker_snapshot();
	flicker_frame(F1_0);
	const auto next = flicker_snapshot();
	require(!same_flickers(previous, next), "Original flicker clock failed to advance");
	stop_time();
	require(same_flickers(next, flicker_snapshot()), "Pause changed the flicker clock");
	start_time();
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_prepared_classic_lighting() && !xl_classic_lighting_vertex_count() && !xl_flicker_count(), "Lighting survived stock retirement");
	const nlohmann::json receipt{ { "original", before }, { "restored", restored }, { "chaos_component", first }, { "vignette_component", second }, { "flickers", previous.size() } };
	std::ofstream out(root / "original-lighting.json");
	out << receipt.dump(2) << '\n';
	require(bool(out), "Cannot write original lighting observations");
	std::cout << "Original native lighting, separate Standoff component controls, incoming ownership, flicker and stock retirement passed\n";
}
static int hog_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		if (!std::strcmp(find_arg_value(argc, argv, "-hog-test"), "original-lighting")) {
			original_lighting_controls(root);
			return 0;
		}
		if (!std::strcmp(find_arg_value(argc, argv, "-hog-test"), "original-mixed")) {
			require(load_base_mission("belialsxl", error, sizeof(error)), error);
			new_player_config();
			input_demo_set_skip_level_intro(1);
			StartNewGame(1);
			char disk[] = "mixed.sav", description[] = "Mixed original";
			for (const int ordinal : { 1, -2 }) {
				if (Current_level_num != ordinal) StartNewLevelSub(ordinal, 1, 0);
				require(Current_level_num == ordinal && (xl_level_extensions_active() != 0) == (ordinal == 1), "Incorrect live mine format");
				Players[Player_num].energy -= F1_0;
				const auto before = hog_checkpoint_observation();
				require(!xl_level_supported_at(Secret_level_names[1], 0, 1) && before == hog_checkpoint_observation(), "Wrong signed ordinal disturbed the running mine");
				stop_time();
				require(state_save_all_sub(disk, description), "Cannot save original mixed mission");
				StartNewLevelSub(ordinal == 1 ? -2 : 1, 1, 0);
				require(Current_level_num != ordinal && (xl_level_extensions_active() != 0) != (ordinal == 1), "Mixed mission failed to change geometry modes");
				require(state_restore_all_sub(disk, 0) && hog_checkpoint_observation() == before, "Disk restore lost original mine, resource or player state");
			}
			require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_resource_texture_scope_enabled() && xl_resource_mod_sources().empty(), "Mixed mission resource ownership survived a stock change");
			std::cout << "Original mixed mission passed both disk restore directions, signed ordinal rejection and stock retirement\n";
			return 0;
		}
		if (!std::strcmp(find_arg_value(argc, argv, "-hog-test"), "original-replay")) {
			const char *mission = find_arg_value(argc, argv, "-mission");
			require(mission && load_base_mission(mission, error, sizeof(error)), "Cannot select original HOG mission");
			new_player_config();
			input_demo_set_skip_level_intro(1);
			Difficulty_level = 2;
			d_srand_stream(D_RNG_SIM, 1);
			d_rand_reset_stream_call_count(D_RNG_SIM);
			const char *selected = find_arg_value(argc, argv, "-original-level");
			int ordinal = 1;
			if (selected) {
				const char *end = selected + std::strlen(selected);
				const auto result = std::from_chars(selected, end, ordinal);
				require(result.ec == std::errc() && result.ptr == end, "Invalid signed replay ordinal");
			}
			require(ordinal && ordinal >= Last_secret_level && ordinal <= Last_level, "Original replay ordinal outside the selected descriptor");
			StartNewGame(ordinal);
			require(Current_level_num == ordinal && !Game_suspended && !cheats.enabled && mission_owns_resource_archive(PHYSFS_getRealDir(Gamesave_current_filename)), "Original HOG replay requires an unassisted mission-owned start");
			xl_recorded_future(root, "original-hog", true, xl_level_extensions_active() != 0);
			return 0;
		}
		std::ifstream original(root / "missions" / "neptunxl.hog", std::ios::binary);
		const xl_compat::Bytes source{ std::istreambuf_iterator<char>(original), std::istreambuf_iterator<char>() };
		const auto members = xl_compat::read_hog(source);
		xl_compat::Bytes bytes{ 'D', '2', 'X' };
		for (const auto &member : members) xl_test::hog_member(bytes, member.name, member.bytes, !member.bytes.empty());
		const std::string long_name = "A-long-owned-resource-with-more-than-thirteen-characters.bin";
		const xl_compat::Bytes payload{ 17, 31, 42, 93, 0, 255 };
		xl_test::hog_member(bytes, long_name, payload, true);
		xl_test::hog_member(bytes, "empty", {}, false);
		for (const auto stem : { "xhogs", "hlegacy", "hlegacy-long" }) {
			if (!std::strcmp(stem, "hlegacy")) {
				bytes[1] = 'H';
				bytes[2] = 'F';
			}
			const auto path = root / "missions" / (std::string(stem) + ".hog");
			write_bytes(path.string().c_str(), bytes);
			std::ofstream(root / "missions" / (std::string(stem) + ".mn2")) << "d2x-name = Derived XL " << stem << "\ntype = normal\nnum_levels = 1\nneptunxl.rl2\n";
			require(PHYSFS_mount(path.string().c_str(), "container-check", 0), "Cannot mount extended archive");
			const auto lookup = "container-check/" + xl_compat::hog_key(long_name);
			require(hog_test_read(lookup.c_str()) == payload && !PHYSFS_exists("container-check/alias.bin"), "Long-name selection or alias isolation differs");
			require(std::filesystem::equivalent(path, PHYSFS_getRealDir(lookup.c_str())), "Archive lost resource owner identity");
			std::unique_ptr<PHYSFS_File, decltype(&PHYSFS_close)> a(PHYSFS_openRead(lookup.c_str()), PHYSFS_close), b(PHYSFS_openRead(lookup.c_str()), PHYSFS_close);
			unsigned char value = 0;
			require(a && b && PHYSFS_seek(a.get(), 4) && PHYSFS_readBytes(a.get(), &value, 1) == 1 && !value && PHYSFS_tell(b.get()) == 0 && PHYSFS_readBytes(b.get(), &value, 1) == 1 && value == 17, "Archive streams share cursor state");
			require(!PHYSFS_seek(a.get(), payload.size() + 1) && PHYSFS_seek(a.get(), payload.size()) && PHYSFS_readBytes(a.get(), &value, 1) == 0, "Archive stream escaped its member bounds");
			require(!PHYSFS_unmount(path.string().c_str()), "Archive unmounted with open streams");
			a.reset();
			b.reset();
			char **names = PHYSFS_enumerateFiles("container-check");
			bool found = false;
			for (char **p = names; p && *p; ++p) found |= long_name == *p;
			PHYSFS_freeList(names);
			require(found && PHYSFS_unmount(path.string().c_str()) && !PHYSFS_exists(lookup.c_str()), "Archive enumeration or unmount cleanup failed");
			require(load_base_mission(stem, error, sizeof(error)), error);
			std::strcpy(Players[Player_num].callsign, "XLTest");
			input_demo_set_skip_level_intro(1);
			StartNewGame(1);
			require(xl_level_extensions_active() && Num_segments > 0 && hog_test_read(long_name.c_str()) == payload, "Extended mission failed ordinary startup/resource lookup");
			for (const auto &member : members) require(hog_test_read(member.name.c_str()) == member.bytes, "Mounted extended member differs from original payload");
			const auto identity = xl_level_trigger_identity();
			const auto light = light_snapshot();
			char save[] = "hog-earned.sav", description[] = "XL extended container";
			stop_time();
			require(state_save_all_sub(save, description), "Cannot save extended mission");
			require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !PHYSFS_exists(long_name.c_str()), "Extended resource leaked into stock");
			require(state_restore_all_sub(save, 0) && xl_level_trigger_identity() == identity && same_light_snapshot(light, light_snapshot()) && hog_test_read(long_name.c_str()) == payload, "Extended archive restore lost owner, source or light state");
			require(!game_is_time_paused(), "Native save/restore left the simulation paused");
			auto corrupt = bytes;
			corrupt.pop_back();
			const auto bad = root / (std::string(stem) + "-bad.hog");
			write_bytes(bad.string().c_str(), corrupt);
			require(!PHYSFS_mount(bad.string().c_str(), "bad-container", 0) && xl_level_trigger_identity() == identity && hog_test_read(long_name.c_str()) == payload, "Malformed archive altered live mission ownership");
		}
		std::cout << "Extended HOG native lifecycle passed: DHF/D2X mixed entries, independent streams, owner identity, exact member payloads, disk through stock and malformed isolation\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "Extended HOG failed: " << e.what() << '\n';
		return 1;
	}
}
