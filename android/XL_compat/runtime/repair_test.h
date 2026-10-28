#include "repair.h"
#include "repair_fixture.h"
static void repair_matrix(const std::filesystem::path &output, const std::vector<int> &segments, int dry)
{
	std::ofstream samples(output / "repair-samples.txt");
	Players[Player_num].shields = i2f(50);
	unsigned count = 0;
	for (int segment : segments)
		for (fix frame : { 0, 1, F1_0 / 60, F1_0 / 4, F1_0 })
			for (fix maximum : { -1, 0, 1, F1_0 / 2, i2f(11), i2f(100) })
				for (fix now : { 0, 1, F1_0 / 4, F1_0 / 4 + 1, i2f(10), i2f(10) + 1 })
					for (fix last : { 0, 1, F1_0 / 4, i2f(11) }) {
						GameTime64 = now;
						FrameTime = frame;
						require(xl_repair_restore(now - last, 1), "Repair matrix clock failed");
						const auto cues = xl_repair_cues();
						const auto amount = repaircen_give_shields(&Segments[segment], maximum);
						samples << (segment == dry ? 0 : 2) << ' ' << frame << ' ' << maximum << ' ' << now << ' ' << last << ' ' << amount << ' ' << GameTime64 - xl_repair_elapsed() << ' ' << xl_repair_cues() - cues << '\n';
						++count;
					}
	for (int segment : segments)
		require(xl_repair_map_color(segment, 123) == (segment == dry ? 123 : BM_XRGB(0, 15, 31)), "Repair automap cue lost function/blocked precedence");
	std::cout << count << " repair samples written\n";
}
static void repair_frames(const std::filesystem::path &output, const std::vector<int> &segments, int repair, int dry, int side, int back)
{
	std::ofstream samples(output / "repair-frames.txt");
	auto &player = *ConsoleObject;
	const int index = &player - Objects;
	player.control_type = CT_NONE;
	player.movement_type = MT_NONE;
	player.lifeleft = IMMORTAL_TIME;
	GameTime64 = i2f(20);
	FrameTime = F1_0 / 4;
	Game_suspended = SUSP_ROBOTS;
	for (int segment : segments) {
		obj_relink(index, segment);
		compute_segment_center(&player.pos, &Segments[segment]);
		for (fix health : { -1, 0, i2f(50), i2f(99), i2f(100), i2f(150) }) {
			Players[Player_num].shields = health;
			const auto energy = Players[Player_num].energy;
			object_move_one(&player);
			samples << (segment == dry ? 0 : 2) << ' ' << FrameTime << ' ' << health << ' ' << energy << ' ' << Players[Player_num].shields << ' ' << Players[Player_num].energy << '\n';
			const auto expected = health < 0 || health >= i2f(100) || segment == dry ? health : std::min(i2f(100), health + 11 * F1_0 / 4);
			require(Players[Player_num].shields == expected && Players[Player_num].energy == energy, "Actual repair frame changed rate, cap, dead player or energy");
		}
	}
	// Cross the real dry/repair portal; segment functions run before movement,
	// so the new room's healing begins on the following object frame
	vms_vector face;
	compute_center_point_on_side(&face, &Segments[repair], side);
	const auto normal = Segments[repair].sides[side].normals[0];
	require(Segments[repair].children[side] == dry && Segments[dry].children[back] == repair, "Repair portal changed");
	player.movement_type = MT_PHYSICS;
	player.mtype.phys_info = {};
	player.mtype.phys_info.mass = F1_0;
	player.size = F1_0 / 8;
	for (int enter : { 1, 0 }) {
		obj_relink(index, enter ? dry : repair);
		vm_vec_scale_add(&player.pos, &face, &normal, enter ? -F1_0 / 2 : F1_0 / 2);
		vm_vec_copy_scale(&player.mtype.phys_info.velocity, &normal, enter ? i2f(4) : -i2f(4));
		Players[Player_num].shields = i2f(50);
		object_move_one(&player);
		require(player.segnum == (enter ? repair : dry) && Players[Player_num].shields == i2f(50) + (enter ? 0 : 11 * F1_0 / 4), "Repair crossing used wrong frame's segment");
		player.mtype.phys_info.velocity = {};
		object_move_one(&player);
		require(Players[Player_num].shields == i2f(50) + 11 * F1_0 / 4, "Repair did not start/stop after physical crossing");
	}
	std::cout << "Actual repair frames: caps, dead/overfull/dry/mixed properties and real portal entry/exit passed\n";
}
static void repair_lifecycle(const std::filesystem::path &output, int repair)
{
	obj_relink(ConsoleObject - Objects, repair);
	compute_segment_center(&ConsoleObject->pos, &Segments[repair]);
	ConsoleObject->mtype.phys_info.velocity = {};
	Players[Player_num].shields = i2f(50);
	require(xl_repair_restore(F1_0 / 8, 1), "Repair clock setup failed");
	char save[] = "repair-earned.sav", description[] = "XL repair clock";
	stop_time();
	require(state_save_all_sub(save, description), "Repair disk save failed");
	auto *file = PHYSFS_openWrite("repair-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Repair trailer capture failed");
	const auto user = output / "preview-user";
	std::ifstream input(user / "repair-state.bin", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	for (int variant = 0; variant < 8; ++variant) {
		auto block = nlohmann::json::parse(bytes.begin() + 16, bytes.end());
		if (variant == 0) block.erase("repair_elapsed");
		if (variant == 1) block["repair_elapsed"] = nlohmann::json::parse("true");
		if (variant == 2) block["repair_elapsed"] = 1.5;
		if (variant == 3) block["repair_elapsed"] = "1";
		if (variant == 4) block["repair_elapsed"] = UINT64_MAX;
		if (variant == 5) block["repair_elapsed"] = INT64_MIN;
		if (variant == 6) {
			block["repair_elapsed"] = 123;
			block["fluid_scrape_elapsed"] = nlohmann::json::parse("false");
		}
		if (variant == 7) {
			block["repair_elapsed"] = 123;
			block["boosts"]["speed"] = 11;
		}
		const auto text = block.dump();
		xl_compat::Bytes bad(bytes.begin(), bytes.begin() + 16);
		bad.insert(bad.end(), text.begin(), text.end());
		std::uint32_t checksum = 2166136261u;
		for (unsigned char byte : text) checksum = (checksum ^ byte) * 16777619u;
		for (int byte = 0; byte < 4; ++byte) {
			bad[8 + byte] = static_cast<unsigned char>(text.size() >> (byte * 8));
			bad[12 + byte] = static_cast<unsigned char>(checksum >> (byte * 8));
		}
		write_bytes((user / "repair-corrupt.bin").string().c_str(), bad);
		file = PHYSFS_openRead("repair-corrupt.bin");
		const int accepted = file ? xl_wall_trigger_read(file) : -1, closed = file ? PHYSFS_close(file) : 0;
		if (accepted || !closed || xl_repair_elapsed() != F1_0 / 8) std::cerr << "Repair corrupt variant=" << variant << " accepted=" << accepted << " closed=" << closed << " elapsed=" << xl_repair_elapsed() << " game=" << GameTime64 << '\n';
		require(!accepted && closed && xl_repair_elapsed() == F1_0 / 8, "Malformed repair trailer partially published state");
	}
	char error[256]{};
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Repair stock transition failed");
	const auto stock = *ConsoleObject;
	require(xl_repair_amount(ConsoleObject->segnum, F1_0) == -1 && !xl_repair_segment(repair) && xl_repair_map_color(repair, 123) == 123 && !std::memcmp(&stock, ConsoleObject, sizeof(stock)), "XL repair leaked into Redux");
	// The Redux repair alternate still uses its original 25-unit rate
	const auto special = Segment2s[ConsoleObject->segnum].special;
	Segment2s[ConsoleObject->segnum].special = SEGMENT_IS_REPAIRCEN;
	FrameTime = F1_0 / 4;
	require(repaircen_give_shields(&Segments[ConsoleObject->segnum], i2f(100)) == 25 * F1_0 / 4, "Redux repair rate changed");
	Segment2s[ConsoleObject->segnum].special = special;
	require(state_restore_all_sub(save, 0) && xl_repair_segment(repair) && Players[Player_num].shields == i2f(50) && xl_repair_elapsed() == F1_0 / 8, "Repair disk restore lost source, health or pending cue");
	start_time();
	FrameTime = F1_0 / 4;
	const auto cues = xl_repair_cues();
	require(repaircen_give_shields(&Segments[repair], F1_0) == F1_0 && xl_repair_cues() == cues, "Restored cue interval replayed too soon");
	GameTime64 += F1_0 / 8 + 1;
	repaircen_give_shields(&Segments[repair], F1_0);
	require(xl_repair_cues() == cues + 1, "Restored repair cue failed at deadline");
	require(xl_repair_restore(-1, 1) && !xl_repair_restore(INT64_MIN, 0) && xl_repair_elapsed() == -1, "Repair backward/overflow clock validation failed");
	require(xl_repair_restore(INT64_MAX, 1), "Representable repair clock rejected");
	++GameTime64;
	require(xl_repair_elapsed() == INT64_MAX, "Repair elapsed overflowed");
	xl_repair_clear();
	std::cout << "Repair lifecycle: disk through stock, pending cue, eight atomic corrupt states and unchanged Redux rate passed\n";
}
static int repair_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto output = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		const bool legacy = !std::strcmp(find_arg_value(argc, argv, "-repair-test"), "legacy");
		if (legacy) write_legacy_repair_fixture(output / "missions");
		else write_repair_fixture(output / "missions");
		const char *mission = legacy ? "oldrep" : "repair", *level = legacy ? "obsrv-xl.rl2" : "neptunxl.rl2";
		require(load_base_mission(mission, error, sizeof(error)), error);
		require(xl_level_supported(level, 0), xl_level_rejection());
		require(!xl_level_supported(level, 1), "Repair admitted multiplayer");
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		repair_matrix(output, legacy ? std::vector<int>{ 0, 6 } : std::vector<int>{ 60, 61, 62, 63, 64, 65, 66, 67 }, legacy ? 6 : 61);
		repair_frames(output, legacy ? std::vector<int>{ 0, 6 } : std::vector<int>{ 60, 61, 62, 64, 65, 66, 67 }, legacy ? 0 : 60, legacy ? 6 : 61, legacy ? 1 : 2, legacy ? 3 : 0);
		repair_lifecycle(output, legacy ? 0 : 60);
		std::cout << "XL repair center controls passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "XL repair failed: " << e.what() << '\n';
		return 1;
	}
}
