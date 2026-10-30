#include "countdown_controls.h"
#include "../tests/countdown_fixture.h"
static void countdown_corruption(const std::filesystem::path &user)
{
	using xl_countdown_controls::require;
	auto *file = PHYSFS_openWrite("countdown-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot capture countdown trailer");
	std::ifstream input(user / "countdown-state.bin", std::ios::binary);
	const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	const auto original = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
	for (int variant = 0; variant < 15; ++variant) {
		auto changed = original;
		auto &row = changed["countdown"];
		if (variant < 7) row[variant] = variant == 0 ? 2 : variant == 1 ? -129 * F1_0
			                                           : variant == 2   ? -129
			                                           : variant == 3   ? 32768
			                                           : variant == 4   ? -1
			                                           : variant == 5   ? 25
			                                                            : F1_0;
		if (variant == 7) row[1] = row[1].get<int>() - 1;
		if (variant == 8) row[0] = 0;
		if (variant == 9) row[3] = UINT64_MAX;
		if (variant == 10) row[4] = 0.5;
		if (variant == 11) row.push_back(0);
		if (variant == 12) changed.erase("countdown");
		if (variant == 13) row[3] = -1;
		if (variant == 14) row[4] = F1_0 * 25 / 1000 + 1;
		const auto payload = changed.dump();
		xl_compat::Bytes bytes(encoded.begin(), encoded.begin() + 16);
		bytes.insert(bytes.end(), payload.begin(), payload.end());
		std::uint32_t checksum = 2166136261u;
		for (unsigned char byte : payload) checksum = (checksum ^ byte) * 16777619u;
		xl_test::layout_patch(bytes, 8, unsigned(payload.size()));
		xl_test::layout_patch(bytes, 12, checksum);
		write_bytes((user / "countdown-bad.bin").string().c_str(), bytes);
		const auto before = xl_countdown_controls::snapshot();
		file = PHYSFS_openRead("countdown-bad.bin");
		require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file) && xl_countdown_controls::equal(before, xl_countdown_controls::snapshot()), "Malformed countdown state admitted or partially published");
		file = PHYSFS_openWrite("countdown-after.bin");
		require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot recapture countdown trailer");
		std::ifstream after(user / "countdown-after.bin", std::ios::binary);
		require(xl_compat::Bytes(std::istreambuf_iterator<char>(after), std::istreambuf_iterator<char>()) == encoded, "Rejected countdown changed other owned state");
	}
}
static int countdown_test(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		using namespace xl_countdown_controls;
		using xl_countdown_controls::require;
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		xl_test::write_countdown_fixture(root / "missions/neptunxl.hog", root / "missions");
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		for (int ordinal : { 1, 2 }) {
			require(load_base_mission("xcount", error, sizeof(error)), error);
			StartNewGame(ordinal);
			Game_suspended = SUSP_ROBOTS;
			events();
			frames();
			for (const char *bad : { "badcount.rl2", "badobject.rl2", "badbase.rl2" }) {
				const auto before = snapshot();
				require(!xl_level_supported(bad, 0) && std::strstr(xl_level_rejection(), "countdown") && load_level(bad) != 0 && equal(before, snapshot()), "Invalid countdown admission lacks reason or changed active state");
			}
			const auto earned = prepare();
			stop_time();
			countdown_corruption(root / "preview-user");
			char save[] = "xl-countdown.sav", description[] = "XL countdown continuation";
			require(state_save_all_sub(save, description), "Cannot save countdown through normal disk path");
			continuation(earned);
			require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Countdown stock transition failed");
			const auto stock = snapshot();
			trigger action{};
			action.type = 25;
			action.time = 99;
			require(!xl_countdown_action(&action), "XL countdown action ran in Redux");
			xl_countdown_frame();
			require(equal(snapshot(), stock), "XL countdown consumer changed Redux state");
			require(state_restore_all_sub(save, 0) && xl_source_level_version() == (ordinal == 1 ? 23 : 27) && equal(snapshot(), earned), "Disk through stock changed exact countdown state");
			continuation(earned);
			require(!game_is_time_paused(), "Native save owner left an unbalanced pause");
			std::cout << (ordinal == 1 ? 23 : 27) << "/40 countdown: wall/master/object/autoplay, warnings, rocking, 15 malformed trailers and disk-through-stock passed\n";
		}
		std::cout << "XL countdown runtime controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
