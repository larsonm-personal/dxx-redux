#include "disarm_controls.h"
#include "../tests/disarm_fixture.h"
static void disarm_corruption(const std::filesystem::path &user)
{
	using xl_disarm_controls::require;
	auto *file = PHYSFS_openWrite("disarm-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot capture disarm trailer");
	std::ifstream input(user / "disarm-state.bin", std::ios::binary);
	const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	const auto original = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
	require(original.at("neutral_robots").size() == 1, "Disarm corruption controls require one earned robot");
	for (int variant = 0; variant < 12; ++variant) {
		auto changed = original;
		auto &rows = changed["neutral_robots"];
		if (variant == 0) rows[0][0] = -1;
		if (variant == 1) rows[0][0] = object_capacity();
		if (variant == 2) rows[0][1] = rows[0][1].get<int>() + 1;
		if (variant == 3) rows.push_back(rows[0]);
		if (variant == 4) rows[0][0] = UINT64_MAX;
		if (variant == 5) rows[0][1] = 0.5;
		if (variant == 6) rows[0].push_back(0);
		if (variant == 7) changed.erase("neutral_robots");
		if (variant == 8) rows = nlohmann::json::object();
		if (variant == 9) rows[0] = { int(Players[Player_num].objnum), int(ConsoleObject->signature) };
		if (variant == 10) rows[0] = nlohmann::json::array();
		if (variant == 11) rows[0][1] = INT64_MIN;
		const auto payload = changed.dump();
		xl_compat::Bytes bytes(encoded.begin(), encoded.begin() + 16);
		bytes.insert(bytes.end(), payload.begin(), payload.end());
		std::uint32_t checksum = 2166136261u;
		for (unsigned char byte : payload) checksum = (checksum ^ byte) * 16777619u;
		xl_test::layout_patch(bytes, 8, unsigned(payload.size()));
		xl_test::layout_patch(bytes, 12, checksum);
		write_bytes((user / "disarm-bad.bin").string().c_str(), bytes);
		file = PHYSFS_openRead("disarm-bad.bin");
		require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file), "Malformed disarm lifetime state admitted");
		file = PHYSFS_openWrite("disarm-after.bin");
		require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot recapture disarm trailer");
		std::ifstream after(user / "disarm-after.bin", std::ios::binary);
		require(xl_compat::Bytes(std::istreambuf_iterator<char>(after), std::istreambuf_iterator<char>()) == encoded, "Rejected disarm state partially published another owned state");
	}
}
static int disarm_test(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		using namespace xl_disarm_controls;
		using xl_disarm_controls::require;
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		xl_test::write_disarm_fixture(root / "missions/neptunxl.hog", root / "missions", Robot_info[0].model_num, Polygon_models[Robot_info[0].model_num].rad, Robot_info[0].strength);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		for (int ordinal : { 1, 2 }) {
			require(load_base_mission("xdisarm", error, sizeof(error)), error);
			StartNewGame(ordinal);
			Game_suspended = SUSP_ROBOTS;
			events();
			consumers();
			lifetimes();
			const auto before = snapshot();
			require(!xl_level_supported("badtarget.rl2", 0) && std::strstr(xl_level_rejection(), "35") && load_level("badtarget.rl2") != 0 && snapshot() == before, "Reprogramming admission lacks an unsupported-action reason or changed disarm state");
			const auto pending = prepare();
			const auto earned = snapshot();
			stop_time();
			disarm_corruption(root / "preview-user");
			char save[] = "xl-disarm.sav", description[] = "XL disarmed robot continuation";
			require(state_save_all_sub(save, description), "Cannot save disarmed robots through ordinary disk path");
			continuation(pending);
			require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active() && snapshot().empty(), "Disarm state leaked into Redux transition");
			consumers(true);
			require(state_restore_all_sub(save, 0) && xl_source_level_version() == (ordinal == 1 ? 23 : 27) && snapshot() == earned, "Disk through Redux lost disarmed robot identities");
			continuation(pending);
			require(!game_is_time_paused(), "Disarm save owner left unbalanced pause");
			std::cout << (ordinal == 1 ? 23 : 27) << "/40 disarm: wall/master/object/autoplay, actual attacks, lifetime reuse, 12 malformed trailers and disk-through-stock passed\n";
		}
		std::cout << "XL disarm runtime controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
