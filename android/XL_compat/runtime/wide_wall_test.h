#include "../tests/wide_wall_fixture.h"
extern "C" {
#include "newdemo.h"
#include "gamemine.h"
extern PHYSFS_file *infile, *outfile;
int newdemo_read_frame_information(int rewrite);
}

static void wide_wall_demo(int front, int back)
{
	const auto read = [](const char *name) {
		auto *file = PHYSFSX_openReadBuffered(name);
		require(file, "Cannot read wall demo control");
		xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file)));
		require(PHYSFS_readBytes(file, bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()) && PHYSFS_close(file), "Wall demo control read failed");
		return bytes;
	};
	for (int first : { 0, 254, 255, 256, front }) {
		const bool wide = first >= DXX_CLASSIC_MAX_WALLS;
		const int second = wide ? back : 1;
		outfile = PHYSFSX_openWriteBuffered("wide-cloak.dem");
		require(outfile, "Cannot create wall demo control");
		Newdemo_state = ND_STATE_RECORDING;
		newdemo_record_cloaking_wall(first, second, WALL_CLOAKED, WALL_DOOR_CLOAKING, 12, F1_0, F1_0 * 2, F1_0 * 3, F1_0 * 4);
		newdemo_record_start_frame(F1_0 / 10);
		const uint8_t eof = 0;
		require(PHYSFS_writeBytes(outfile, &eof, 1) == 1 && PHYSFS_close(outfile), "Cannot close wall demo control");
		outfile = nullptr;
		const auto recorded = read("wide-cloak.dem");
		require(recorded[0] == (wide ? 53 : 44) && (wide ? (recorded[1] | unsigned(recorded[2]) << 8) : recorded[1]) == unsigned(first), "Wall demo writer narrowed identity or changed classic encoding");
		for (int rewrite : { 0, 1 }) {
			infile = PHYSFSX_openReadBuffered("wide-cloak.dem");
			require(infile, "Cannot decode wall demo control");
			Newdemo_state = ND_STATE_PLAYBACK;
			Newdemo_vcr_state = ND_STATE_PAUSED;
			if (rewrite) outfile = PHYSFSX_openWriteBuffered("wide-cloak.rewrite");
			require(!rewrite || outfile, "Cannot rewrite wall demo control");
			require(newdemo_read_frame_information(rewrite) == 1 && PHYSFS_tell(infile) == PHYSFS_sint64(recorded.size() - 1), "Wall demo decoding lost framing");
			require(PHYSFS_close(infile), "Cannot close decoded wall demo");
			infile = nullptr;
			if (rewrite) {
				require(PHYSFS_close(outfile), "Cannot close rewritten wall demo");
				outfile = nullptr;
				require(read("wide-cloak.rewrite") == xl_compat::Bytes(recorded.begin(), recorded.end() - 1), "Wall demo rewrite changed identities or payload");
			} else {
				require(Walls[first].type == WALL_CLOAKED && Walls[second].type == WALL_CLOAKED &&
				            Walls[first].state == WALL_DOOR_CLOAKING && Walls[first].cloak_value == 12,
				        "Decoded cloak event did not update its original walls");
			}
		}
		if (wide) {
			const std::vector<wall> before(Walls, Walls + Num_walls);
			for (unsigned offset : { 1u, 3u })
				for (unsigned invalid : { 2047u, 32767u, 65534u }) {
					auto bytes = recorded;
					bytes[offset] = uint8_t(invalid);
					bytes[offset + 1] = uint8_t(invalid >> 8);
					write_bytes((std::filesystem::path(PHYSFS_getWriteDir()) / "wide-invalid.dem").string().c_str(), bytes);
					infile = PHYSFSX_openReadBuffered("wide-invalid.dem");
					require(infile && newdemo_read_frame_information(0) == 1 && PHYSFS_close(infile), "Invalid cloak event lost bounded frame decoding");
					infile = nullptr;
					require(!std::memcmp(Walls, before.data(), before.size() * sizeof(wall)), "Invalid cloak identity modified walls");
				}
		}
		// A one-sided wall's missing back identity must remain recordable
		outfile = PHYSFSX_openWriteBuffered("wide-one-sided.dem");
		require(outfile, "Cannot record a one-sided cloak event");
		Newdemo_state = ND_STATE_RECORDING;
		newdemo_record_cloaking_wall(first, -1, WALL_CLOAKED, WALL_DOOR_CLOAKING, 12, F1_0, F1_0, F1_0, F1_0);
		newdemo_record_start_frame(F1_0 / 10);
		require(PHYSFS_writeBytes(outfile, &eof, 1) == 1 && PHYSFS_close(outfile), "Cannot close one-sided cloak event");
		outfile = nullptr;
		const auto one_sided = read("wide-one-sided.dem");
		require(one_sided[wide ? 3 : 2] == 255 && (!wide || one_sided[4] == 255), "Missing back-wall sentinel changed while recording");
		Walls[first].type = WALL_NORMAL;
		Newdemo_state = ND_STATE_PLAYBACK;
		Newdemo_vcr_state = ND_STATE_PAUSED;
		for (int rewrite : { 0, 1 }) {
			infile = PHYSFSX_openReadBuffered("wide-one-sided.dem");
			if (rewrite) outfile = PHYSFSX_openWriteBuffered("one-sided.rewrite");
			require(infile && (!rewrite || outfile) && newdemo_read_frame_information(rewrite) == 1 && PHYSFS_close(infile), "One-sided cloak decoding failed");
			infile = nullptr;
			require(Walls[first].type == WALL_CLOAKED, "One-sided playback skipped its front wall");
			if (rewrite) {
				require(PHYSFS_close(outfile), "One-sided rewrite close failed");
				outfile = nullptr;
				require(read("one-sided.rewrite") == xl_compat::Bytes(one_sided.begin(), one_sided.end() - 1), "One-sided rewrite changed its missing back sentinel");
			}
		}
	}
	Newdemo_state = Newdemo_vcr_state = ND_STATE_NORMAL;
	std::cout << "Actual classic and wide cloak writers, frame decoding and byte-preserving rewrite passed\n";
}

static void write_wide_wall_fixture(const std::filesystem::path &root, unsigned count)
{
	write_close_door_fixture(root);
	std::ifstream input(root / "close.hog", std::ios::binary);
	const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	xl_compat::Bytes output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(original)) {
		if (member.name == "neptunxl.rl2") member.bytes = xl_test::remap_walls(member.bytes, count);
		char name[13]{};
		require(member.name.size() < sizeof(name), "Wide-wall member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(uint8_t(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "widewall.hog").string().c_str(), output);
	std::ofstream descriptor(root / "widewall.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived XL wide walls\nnum_levels = 1\nneptunxl.rl2\n";
}

static void wide_wall_rejections(const std::filesystem::path &root)
{
	auto *file = PHYSFSX_openReadBuffered("neptunxl.rl2");
	require(file, "Wide source missing for rejection controls");
	xl_compat::Bytes original(static_cast<std::size_t>(PHYSFS_fileLength(file)));
	require(PHYSFS_readBytes(file, original.data(), original.size()) == PHYSFS_sint64(original.size()) && PHYSFS_close(file), "Cannot read wide source");
	const auto source = xl_compat::inspect_level(original);
	const auto world = arena_state();
	const std::vector<wall> walls(Walls, Walls + Num_walls);
	// A copied private adapter is not an independently admissible stock mine
	write_bytes((std::filesystem::path(PHYSFS_getWriteDir()) / "wide-private.mine").string().c_str(), { DXX_XL_WIDE_WALL_MINE });
	file = PHYSFSX_openReadBuffered("wide-private.mine");
	require(file && load_mine_data_compiled(file) == -1 && PHYSFS_close(file), "Unowned private mine encoding was accepted");
	require(arena_state() == world && !xl_loading_wide_walls(), "Rejected private mine changed the world or granted permission");
	const auto shadow = root / "wide-wall-shadow";
	std::filesystem::create_directories(shadow);
	require(PHYSFS_addToSearchPath(shadow.string().c_str(), 0), "Cannot mount wall rejection controls");
	for (int variant = 0; variant < 3; ++variant) {
		auto bytes = original;
		if (!variant) bytes = xl_test::remap_walls(original, DXX_XL_MAX_WALLS + 1);
		else if (variant == 1) {
			for (int byte = 0; byte < 4; ++byte) bytes.at(source.game_offset + 51 + byte) = byte ? 0 : 1;
		} else bytes.resize(source.game_offset + 10);
		write_bytes((shadow / "neptunxl.rl2").string().c_str(), bytes);
		const char *owner = PHYSFS_getRealDir("neptunxl.rl2");
		require(owner && std::filesystem::weakly_canonical(owner) == std::filesystem::weakly_canonical(shadow), "Malformed wall control does not own the resolved level");
		require(!xl_level_supported("neptunxl.rl2", 0), "Invalid wide-wall source was admitted");
		if (!variant) require(std::strstr(xl_level_rejection(), "more than 2047 walls"), "Wide-wall capacity rejection lacks its limit");
		if (variant == 1) require(std::strstr(xl_level_rejection(), "invalid wall reference"), "Bad wall identity rejection lacks its reason");
		require(arena_state() == world && Num_walls == int(walls.size()) && !std::memcmp(Walls, walls.data(), walls.size() * sizeof(wall)), "Rejected wide-wall source changed the running world");
		require(!xl_loading_wide_walls(), "Rejected source left private mine permission active");
	}
	require(PHYSFS_removeFromSearchPath(shadow.string().c_str()), "Cannot unmount wall rejection controls");
	require(xl_level_supported("neptunxl.rl2", 0), "Wide-wall admission did not recover");
	std::cout << "Over-capacity, invalid-reference and truncated wall sources rejected without gameplay mutation\n";
}

static void wide_wall_saved_count_controls(char *save, int limit)
{
	const int original_count = Num_walls;
	auto *file = PHYSFSX_openReadBuffered(save);
	require(file, "Cannot read wall-count save control");
	xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file)));
	require(PHYSFS_readBytes(file, bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()) && PHYSFS_close(file), "Cannot read saved walls");
	const auto *first = reinterpret_cast<const uint8_t *>(Walls);
	const auto *last = first + Num_walls * sizeof(wall);
	const auto at = std::search(bytes.begin(), bytes.end(), first, last);
	require(at != bytes.end() && at - bytes.begin() >= 4 && std::search(at + 1, bytes.end(), first, last) == bytes.end(), "Cannot uniquely locate packed saved walls");
	const auto count_offset = static_cast<std::size_t>(at - bytes.begin() - 4);
	char invalid_save[] = "wide-invalid.sav";
	for (uint32_t count : { 0xffffffffu, uint32_t(limit + 1) }) {
		auto invalid = bytes;
		for (unsigned byte = 0; byte < 4; ++byte) invalid[count_offset + byte] = uint8_t(count >> (byte * 8));
		write_bytes((std::filesystem::path(PHYSFS_getWriteDir()) / invalid_save).string().c_str(), invalid);
		require(!state_restore_all_sub(invalid_save, 0), "Invalid saved wall count was accepted");
		require(Num_walls == original_count && !xl_loading_wide_walls(), "Failed restore committed an invalid count or retained private permission");
		require(state_restore_all_sub(save, 0), "Valid wide-wall restore did not recover after invalid count");
	}
	std::cout << "Negative and over-capacity saved wall counts rejected before array reads; valid restore recovered\n";
}

static int wide_wall_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto output = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		write_wide_wall_fixture(output / "missions", DXX_XL_MAX_WALLS);
		require(load_base_mission("widewall", error, sizeof(error)), error);
		if (!xl_level_supported("neptunxl.rl2", 0)) throw std::runtime_error(xl_level_rejection());
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active() && !xl_loading_wide_walls() && Num_walls == DXX_XL_MAX_WALLS, "Wide-wall load or transient context failed");
		route_snapshot_summary snapshot{};
		require(level_metadata_get_canonical_route_snapshot(&snapshot) && snapshot.wall_count == DXX_XL_MAX_WALLS && snapshot.trigger_count == Num_triggers, "Metadata snapshot narrowed the XL wall table");
		const int front = Segments[24].sides[5].wall_num, back = Segments[21].sides[4].wall_num;
		require(front == 2017 && back == 2018, "Authored wall identities narrowed during native loading");
		Game_suspended = SUSP_ROBOTS;
		obj_relink(ConsoleObject - Objects, 0);
		compute_segment_center(&ConsoleObject->pos, &Segments[0]);
		ConsoleObject->mtype.phys_info.velocity = {};
		Players[Player_num].shields = i2f(73);
		Players[Player_num].energy = i2f(61);
		FrameTime = F1_0 / 32;
		doorway_rays(WALL_IS_DOORWAY(&Segments[24], 5));
		require(check_trigger_sub(0, Player_num, 1) == 0, "High-index authored door activation failed");
		for (int frame = 0; frame < 512 && Walls[front].state != WALL_DOOR_WAITING; ++frame) wall_frame_process();
		require(Walls[front].state == WALL_DOOR_WAITING && Walls[back].state == WALL_DOOR_WAITING && Num_open_doors == 1, "High-index door did not open");
		require(ActiveDoors[0].front_wallnum[0] == front && ActiveDoors[0].back_wallnum[0] == back, "Active door narrowed high identities");
		doorway_rays(WALL_IS_DOORWAY(&Segments[24], 5));
		for (int frame = 0; frame < 8; ++frame) wall_frame_process();
		const auto earned = ActiveDoors[0];
		char save[] = "widewall.sav", description[] = "XL wide wall state";
		stop_time();
		require(state_save_all_sub(save, description), "High-index wall save failed");
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Wide-wall stock transition failed");
		require(Num_walls <= DXX_CLASSIC_MAX_WALLS && !xl_loading_wide_walls(), "Wide capacity permission leaked into stock");
		require(state_restore_all_sub(save, 0), "High-index door restore through stock failed");
		start_time();
		require(Num_walls == DXX_XL_MAX_WALLS && Segments[24].sides[5].wall_num == front &&
		            Num_open_doors == 1 && !std::memcmp(&earned, &ActiveDoors[0], sizeof(earned)) &&
		            Players[Player_num].shields == i2f(73) && Players[Player_num].energy == i2f(61),
		        "Wide wall restore lost identities, timing or earned resources");
		require(check_trigger_sub(1, Player_num, 1) == 0 && Walls[front].state == WALL_DOOR_CLOSING, "Restored high-index authored close failed");
		FrameTime = F1_0 / 32;
		for (int frame = 0; frame < 512 && Walls[front].state != WALL_DOOR_CLOSED; ++frame) {
			wall_frame_process();
			doorway_rays(WALL_IS_DOORWAY(&Segments[24], 5));
		}
		require(Walls[front].state == WALL_DOOR_CLOSED && Num_open_doors == 0, "Restored high-index closing failed");
		// Cloak a real high-index pair; saved animation must retain both identities
		Walls[front].type = Walls[back].type = WALL_CLOSED;
		Walls[front].state = Walls[back].state = WALL_DOOR_CLOSED;
		start_wall_cloak(&Segments[24], 5);
		for (int frame = 0; frame < 8; ++frame) wall_frame_process();
		require(Num_cloaking_walls == 1 && CloakingWalls[0].front_wallnum == front && CloakingWalls[0].back_wallnum == back, "High-index cloak animation lost identities");
		const auto cloaking = CloakingWalls[0];
		stop_time();
		require(state_save_all_sub(save, description), "High-index cloak save failed");
		for (int frame = 0; frame < 80; ++frame) wall_frame_process();
		require(state_restore_all_sub(save, 0) && Num_cloaking_walls == 1 && !std::memcmp(&cloaking, &CloakingWalls[0], sizeof(cloaking)), "High-index cloak restore lost timer or identities");
		start_time();
		wide_wall_demo(front, back);
		require(state_restore_all_sub(save, 0), "Recovery after wall demo controls failed");
		wide_wall_saved_count_controls(save, DXX_XL_MAX_WALLS);
		wide_wall_rejections(output);
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Final stock capacity control failed");
		require(state_save_all_sub(save, description), "Stock wall-count control save failed");
		wide_wall_saved_count_controls(save, DXX_CLASSIC_MAX_WALLS);
		std::cout << "XL wide-wall load, authored opening/closing, collision, disk through stock and earned state passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "Wide-wall controls failed: " << error.what() << '\n';
		return 1;
	}
}
