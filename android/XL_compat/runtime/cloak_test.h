#include "cloak.h"
#include "../tests/cloak_fixture.h"

static void write_cloak_fixture(const std::filesystem::path &root)
{
	write_close_door_fixture(root);
	std::ifstream input(root / "close.hog", std::ios::binary);
	const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	xl_compat::Bytes output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(original)) {
		if (member.name == "neptunxl.rl2") {
			const auto source = xl_compat::inspect_level(member.bytes);
			for (int i : { 0, 1 }) {
				const auto row = source.sections[1].offset + i * 25;
				member.bytes.at(row + 16) = WALL_CLOSED;
				member.bytes.at(row + 17) = member.bytes.at(row + 18) = 0;
				member.bytes.at(row + 19) = WALL_DOOR_CLOSED;
				member.bytes.at(row + 21) = 255;
				member.bytes.at(row + 22) = KEY_NONE;
				member.bytes.at(row + 24) = 0;
				member.bytes.at(source.sections[3].offset + i * 52) = i ? TT_CLOSE_WALL : TT_OPEN_WALL;
			}
			member.bytes = xl_test::add_cloak_pairs(member.bytes, 100);
			const auto blockers = xl_compat::runtime_blockers(xl_compat::inspect_level(member.bytes));
			require(blockers.empty(), xl_compat::blocker_message(blockers).c_str());
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "Cloak fixture member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(uint8_t(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "cloak.hog").string().c_str(), output);
	std::ofstream descriptor(root / "cloak.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived XL cloaks\nnum_levels = 1\nneptunxl.rl2\n";
}

static void cloak_matrix(std::ostream &out)
{
	const auto front = Walls[0], back = Walls[1];
	const auto front_side = Segments[24].sides[5], back_side = Segments[21].sides[4];
	out.precision(9);
	unsigned samples = 0;
	const auto sample = [&](int operation, int argument, int paired, int initial) {
		out << operation << ' ' << argument << ' ' << paired << ' ' << initial;
		for (int i : { 0, 1 }) out << ' ' << int(Walls[i].type) << ' ' << int(Walls[i].state) << ' ' << int(Walls[i].cloak_value);
		out << ' ' << Num_cloaking_walls << ' ' << (Num_cloaking_walls ? CloakingWalls[0].time : -1);
		for (int i : { 0, 1 })
			for (int c = 0; c < 4; ++c) out << ' ' << Segments[Walls[i].segnum].sides[Walls[i].sidenum].uvls[c].l;
		const int flat = xl_preview_begin_cloak_face(24, 5);
		out << ' ' << flat << ' ' << (flat ? xl_preview_alpha(1) : -1.0f) << '\n';
		xl_preview_end_face();
		++samples;
	};
	for (int opening : { 0, 1 })
		for (int paired : { 0, 1 })
			for (int initial : { -1, 0, 15, 31 })
				for (int reversal : { 0, 8, 40 }) {
					Walls[0] = front;
					Walls[1] = back;
					Segments[24].sides[5] = front_side;
					Segments[21].sides[4] = back_side;
					Segments[21].sides[4].wall_num = paired ? 1 : -1;
					Num_cloaking_walls = Num_open_doors = 0;
					for (int i : { 0, 1 }) {
						Walls[i].type = opening ? WALL_CLOSED : WALL_OPEN;
						Walls[i].state = WALL_DOOR_CLOSED;
						Walls[i].cloak_value = initial;
						for (int c = 0; c < 4; ++c) Segments[Walls[i].segnum].sides[Walls[i].sidenum].uvls[c].l = (i * 4 + c + 1) * 8192;
					}
					if (opening) start_wall_cloak(&Segments[24], 5);
					else start_wall_decloak(&Segments[24], 5);
					sample(0, opening, paired, initial);
					for (int frame = 1; frame <= 65; ++frame) {
						if (frame == reversal) {
							if (opening) start_wall_decloak(&Segments[24], 5);
							else start_wall_cloak(&Segments[24], 5);
							sample(2, !opening, paired, initial);
						}
						FrameTime = F1_0 / 64;
						wall_frame_process();
						sample(1, FrameTime, paired, initial);
					}
				}
	Walls[0] = front;
	Walls[1] = back;
	Segments[24].sides[5] = front_side;
	Segments[21].sides[4] = back_side;
	Num_cloaking_walls = 0;
	std::cout << samples << " native cloak caller/frame/alpha samples written\n";
}

static void cloak_saved_rejections(char *save)
{
	auto *file = PHYSFSX_openReadBuffered(save);
	require(file, "Cannot read cloak corruption controls");
	xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file)));
	require(PHYSFS_readBytes(file, bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()) && PHYSFS_close(file), "Cannot read saved cloak state");
	const auto *first = reinterpret_cast<const uint8_t *>(CloakingWalls);
	const auto *last = first + Num_cloaking_walls * sizeof(cloaking_wall);
	const auto found = std::search(bytes.begin(), bytes.end(), first, last);
	require(found != bytes.end() && found - bytes.begin() >= 4 && std::search(found + 1, bytes.end(), first, last) == bytes.end(), "Cannot uniquely locate saved cloak table");
	const auto table = static_cast<std::size_t>(found - bytes.begin());
	char invalid_name[] = "badcloak.sav";
	for (int variant = 0; variant < 8; ++variant) {
		const std::vector<cloaking_wall> before(CloakingWalls, CloakingWalls + Num_cloaking_walls);
		auto bad = bytes;
		const auto patch = [&](std::size_t at, uint32_t value, unsigned width) {
			for (unsigned byte = 0; byte < width; ++byte) bad.at(at + byte) = uint8_t(value >> (8 * byte));
		};
		switch (variant) {
			case 0: patch(table - 4, 101, 4); break;
			case 1: patch(table - 4, 0xffffffffu, 4); break;
			case 2: patch(table, uint32_t(Num_walls), 2); break;
			case 3: patch(table + 2, uint32_t(CloakingWalls[0].front_wallnum), 2); break;
			case 4: patch(table + offsetof(cloaking_wall, time), 0xffffffffu, 4); break;
			case 5: patch(table + offsetof(cloaking_wall, time), F1_0 + 1, 4); break;
			case 6: std::copy_n(bad.begin() + table, sizeof(cloaking_wall), bad.begin() + table + sizeof(cloaking_wall)); break;
			case 7: bad.resize(table + sizeof(cloaking_wall) - 1); break;
		}
		write_bytes((std::filesystem::path(PHYSFS_getWriteDir()) / invalid_name).string().c_str(), bad);
		require(!state_restore_all_sub(invalid_name, 0), "Corrupt cloak table was accepted");
		require(Num_cloaking_walls == int(before.size()) && !std::memcmp(CloakingWalls, before.data(), before.size() * sizeof(cloaking_wall)), "Corrupt cloak table was installed");
		require(state_restore_all_sub(save, 0) && Num_cloaking_walls == 100, "Valid cloak save did not recover");
	}
	std::cout << "Eight corrupt/truncated cloak count/owner/time saves rejected before installation; valid recovery passed\n";
}

static void cloak_concurrency()
{
	require(Num_walls == 230, "Cloak fixture lost its 100 additional authored pairs");
	std::vector<int> fronts{ 0 };
	for (int index = 30; index < 230; index += 2) fronts.push_back(index);
	const auto start = [&](int index, bool opening) {
		const auto &wall = Walls[index];
		if (opening) start_wall_cloak(&Segments[wall.segnum], wall.sidenum);
		else start_wall_decloak(&Segments[wall.segnum], wall.sidenum);
	};
	for (int index : fronts) start(index, true);
	require(Num_cloaking_walls == 100 && Walls[228].type == WALL_OPEN && Walls[229].type == WALL_OPEN, "101st cloak request did not preserve source open fallback");
	start(0, true);
	require(Num_cloaking_walls == 100, "Repeated cloak allocated another owner");
	FrameTime = F1_0 / 4;
	wall_frame_process();
	require(xl_cloak_saved_valid(CloakingWalls, Num_cloaking_walls), "Live cloak capacity state is invalid");
	const std::vector<cloaking_wall> saved(CloakingWalls, CloakingWalls + Num_cloaking_walls);
	Players[Player_num].shields = i2f(73);
	Players[Player_num].energy = i2f(61);
	char save[] = "cloaks.sav", description[] = "XL cloak capacity";
	stop_time();
	require(state_save_all_sub(save, description), "Concurrent cloak save failed");
	char error[256]{};
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Cloak stock transition failed");
	require(state_restore_all_sub(save, 0) && Num_cloaking_walls == 100 && !std::memcmp(saved.data(), CloakingWalls, saved.size() * sizeof(cloaking_wall)), "Cloak disk-through-stock lost timers, owners or lights");
	require(Players[Player_num].shields == i2f(73) && Players[Player_num].energy == i2f(61), "Cloak restore lost earned resources");
	start_time();
	cloak_saved_rejections(save);
	FrameTime = F1_0;
	wall_frame_process();
	require(Num_cloaking_walls == 0, "Completion skipped entries shifted by compaction");
	const auto restored_lights = [](const std::vector<cloaking_wall> &entries) {
		for (const auto &entry : entries)
			for (int corner = 0; corner < 4; ++corner) {
				const auto &front = Walls[entry.front_wallnum], &back = Walls[entry.back_wallnum];
				require(Segments[front.segnum].sides[front.sidenum].uvls[corner].l == entry.front_ls[corner] &&
				            Segments[back.segnum].sides[back.sidenum].uvls[corner].l == entry.back_ls[corner],
				        "Large final cloak step did not restore both sides' captured light");
			}
	};
	restored_lights(saved);
	for (int index : fronts) require(Walls[index].type == WALL_OPEN && Walls[index].state == WALL_DOOR_CLOSED, "Concurrent cloak completion lost a wall");
	for (int index : fronts) start(index, false);
	require(Num_cloaking_walls == 100 && Walls[228].type == WALL_CLOSED && Walls[229].type == WALL_CLOSED && Walls[228].state == WALL_DOOR_CLOSED, "Decloak capacity correction did not close both sides");
	FrameTime = F1_0 / 4;
	wall_frame_process();
	start(1, true);
	require(Num_cloaking_walls == 100 && CloakingWalls[0].front_wallnum == 1 && CloakingWalls[0].back_wallnum == 0 && CloakingWalls[0].time == 3 * F1_0 / 4, "Back-side reversal lost owner or reused clock");
	require(xl_cloak_saved_valid(CloakingWalls, Num_cloaking_walls), "Reversed cloak state is invalid");
	stop_time();
	require(state_save_all_sub(save, description) && state_restore_all_sub(save, 0), "Reversed cloak save/restore failed");
	start_time();
	const std::vector<cloaking_wall> reversed(CloakingWalls, CloakingWalls + Num_cloaking_walls);
	FrameTime = 2 * F1_0;
	wall_frame_process();
	require(Num_cloaking_walls == 0 && Walls[0].type == WALL_OPEN, "Reversed cloak did not finish");
	restored_lights(reversed);
	for (std::size_t i = 1; i < fronts.size(); ++i) require(Walls[fronts[i]].type == WALL_CLOSED && Walls[fronts[i]].state == WALL_DOOR_CLOSED, "Large final decloak step lost closed state");
	std::cout << "100 concurrent cloaks, 101st fallback, back-side reversal, compaction and disk-through-stock passed\n";
}

static int cloak_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto output = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		write_cloak_fixture(output / "missions");
		require(load_base_mission("cloak", error, sizeof(error)), error);
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active(), "Cloak fixture did not select XL");
		Game_suspended = SUSP_ROBOTS;
		obj_relink(ConsoleObject - Objects, 0);
		compute_segment_center(&ConsoleObject->pos, &Segments[0]);
		ConsoleObject->mtype.phys_info.velocity = {};
		std::ofstream samples(output / "cloak-samples.txt");
		cloak_matrix(samples);
		require(check_trigger_sub(0, Player_num, 1) == 0 && Num_cloaking_walls == 1, "Authored cloak action did not activate");
		for (int frame = 0; frame < 65; ++frame) {
			FrameTime = F1_0 / 64;
			wall_frame_process();
			doorway_rays(WALL_IS_DOORWAY(&Segments[24], 5));
		}
		require(Num_cloaking_walls == 0 && Walls[0].type == WALL_OPEN, "Authored cloak did not complete");
		require(check_trigger_sub(1, Player_num, 1) == 0 && Num_cloaking_walls == 1, "Authored decloak action did not activate");
		for (int frame = 0; frame < 65; ++frame) {
			FrameTime = F1_0 / 64;
			wall_frame_process();
			doorway_rays(WALL_IS_DOORWAY(&Segments[24], 5));
		}
		require(Num_cloaking_walls == 0 && Walls[0].type == WALL_CLOSED, "Authored decloak did not complete");
		cloak_concurrency();
		std::cout << "XL cloak native controls passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "Cloak native controls failed: " << e.what() << '\n';
		return 1;
	}
}
