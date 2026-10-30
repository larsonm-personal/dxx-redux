#include "seismic_controls.h"
#include "../tests/layout_fixture.h"
inline void seismic_trailer_corruption(const std::filesystem::path &user)
{
	using xl_seismic_controls::check;
	auto *file = PHYSFS_openWrite("seismic-state.bin");
	check(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot capture earned seismic trailer");
	std::ifstream input(user / "seismic-state.bin", std::ios::binary);
	const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	const auto original = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
	for (int variant = 0; variant < 8; ++variant) {
		auto changed = original;
		auto &state = changed["seismic"];
		if (variant == 0) changed.erase("seismic");
		if (variant == 1) state.push_back(0);
		if (variant == 2) state[0] = UINT64_MAX;
		if (variant == 3) state[2] = { 1, 2 };
		if (variant == 4) state[3][0][1] = -1;
		if (variant == 5) state[4] = UINT64_MAX;
		if (variant == 6) state[5] = 25;
		if (variant == 7) state[6] = F1_0;
		const auto payload = changed.dump();
		xl_compat::Bytes bytes(encoded.begin(), encoded.begin() + 16);
		bytes.insert(bytes.end(), payload.begin(), payload.end());
		std::uint32_t checksum = 2166136261u;
		for (unsigned char byte : payload) checksum = (checksum ^ byte) * 16777619u;
		xl_test::layout_patch(bytes, 8, unsigned(payload.size()));
		xl_test::layout_patch(bytes, 12, checksum);
		write_bytes((user / "seismic-bad.bin").string().c_str(), bytes);
		file = PHYSFS_openRead("seismic-bad.bin");
		check(file && !xl_wall_trigger_read(file) && PHYSFS_close(file), "Malformed seismic trailer admitted");
		file = PHYSFS_openWrite("seismic-after.bin");
		check(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot recapture rejected seismic trailer");
		std::ifstream after(user / "seismic-after.bin", std::ios::binary);
		check(xl_compat::Bytes(std::istreambuf_iterator<char>(after), std::istreambuf_iterator<char>()) == encoded, "Rejected seismic trailer partially changed owned state");
	}
}
inline void native_seismic_test(const std::filesystem::path &root, int ordinal)
{
	using namespace xl_seismic_controls;
	char error[256]{};
	check(load_base_mission("xseismic", error, sizeof(error)), error);
	std::strcpy(Players[Player_num].callsign, "XLTest");
	input_demo_set_skip_level_intro(1);
	StartNewGame(ordinal);
	Game_suspended = SUSP_ROBOTS;
	SDL_PauseAudio(1);
	digi_set_digi_volume(32768);
	check(!game_is_time_paused(), "Fresh seismic fixture must start unpaused");
	events();
	const auto output = [] {
		const auto state = xl_compat::named_audio_snapshot();
		named_audio_control::begin_capture();
		xl_named_audio_pause(0);
		named_audio_control::run_output(2048);
		xl_shared_audio_pcm::output(state, named_audio_control::captured.data(), nullptr, named_audio_control::captured_samples, 128, 0);
	};
	xl_seismic_audio_control::run(output);
	for (const char *bad : { "badquake.rl2", "badqobj.rl2" }) {
		const auto before = seismic_gameplay_snapshot();
		check(!xl_level_supported(bad, 0) && std::strstr(xl_level_rejection(), "seismic") && load_level(bad) != 0 && seismic_gameplay_snapshot() == before, "Seismic admission lacks a helpful error or changes live state");
	}
	const auto earned = prepare();
	const auto rotation = ConsoleObject->mtype.phys_info.rotvel;
	output();
	const auto audio = xl_compat::named_audio_snapshot();
	stop_time();
	paused();
	seismic_trailer_corruption(root / "preview-user");
	char save[] = "xl-seismic.sav", description[] = "XL seismic continuation";
	check(state_save_all_sub(save, description), "Cannot save earned seismic physics");
	continuation();
	check(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Seismic stock transition failed");
	const auto stock = seismic_gameplay_snapshot();
	trigger action{};
	action.type = 36;
	action.value = F1_0;
	action.time = 3;
	check(!xl_seismic_action(&action) && !xl_seismic_frame() && !xl_seismic_detonation(nullptr) && seismic_gameplay_snapshot() == stock, "XL seismic hooks changed Redux gameplay");
	check(stock[0] == 0 && stock[1] == 0 && stock[2].is_null() && stock[5] == 0 && stock[6] == 0, "XL seismic state leaked into the stock level");
	check(state_restore_all_sub(save, 0) && xl_source_level_version() == (ordinal == 1 ? 23 : 27) && seismic_gameplay_snapshot() == earned && xl_compat::named_audio_snapshot() == audio && !std::memcmp(&rotation, &ConsoleObject->mtype.phys_info.rotvel, sizeof(rotation)), "Seismic disk-through-stock lost exact physics, timing, distance scale or audio");
	check(!game_is_time_paused(), "Seismic restore left a paused simulation");
	continuation();
	std::cout << (ordinal == 1 ? 23 : 27) << "/40 seismic: wall/master/object/autoplay, source physics, companion, distance, wide magnitude, pause and disk-through-stock passed\n";
}
