#include "../tests/trigger_capacity_fixture.h"

static void write_trigger_capacity_fixture(const std::filesystem::path &root)
{
	write_close_door_fixture(root);
	std::ifstream input(root / "close.hog", std::ios::binary);
	const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	xl_compat::Bytes output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(original)) {
		if (member.name == "neptunxl.rl2") {
			member.bytes = xl_test::expand_triggers(member.bytes, DXX_XL_MAX_TRIGGERS);
			const auto source = xl_compat::inspect_level(member.bytes);
			const auto patch = [&](std::size_t at, unsigned value, unsigned width = 4) {
				for (unsigned i = 0; i < width; ++i) member.bytes.at(at + i) = uint8_t(value >> (8 * i));
			};
			for (unsigned id = 0; id < DXX_XL_MAX_TRIGGERS; ++id) {
				const auto row = source.sections[3].offset + id * 52;
				std::fill(member.bytes.begin() + row, member.bytes.begin() + row + 52, 0);
				member.bytes.at(row) = TT_LIGHT_ON;
				patch(row + 8, unsigned(-1));
			}
			const auto action = [&](int id, int type, int target, int flags, int delay) {
				const auto row = source.sections[3].offset + id * 52;
				member.bytes.at(row) = uint8_t(type);
				member.bytes.at(row + 1) = uint8_t(flags);
				member.bytes.at(row + 2) = 1;
				patch(row + 8, unsigned(delay));
				patch(row + 12, unsigned(source.walls[target].segment), 2);
				patch(row + 32, unsigned(source.walls[target].side), 2);
			};
			action(128, TT_OPEN_WALL, 0, 16, -1);
			action(253, TT_CLOSE_WALL, 0, TF_ONE_SHOT, -1);
			action(99, 31, 0, 0, -1);
			action(100, 31, 1, 0, 1000);
			action(127, 33, 1, 0, -1);
			action(200, 32, 1, 0, -1);
			action(252, 31, 0, 0, 1000);
			for (int wall : { 0, 1 }) {
				const auto row = source.sections[1].offset + wall * 25;
				member.bytes.at(row + 16) = WALL_CLOSED;
				patch(row + 17, 0, 2);
				member.bytes.at(row + 19) = WALL_DOOR_CLOSED;
			}
			const int hosts[] = { 128, 253, 99, 100, 127, 200, 252 };
			for (unsigned wall = 0; wall < sizeof(hosts) / sizeof(hosts[0]); ++wall)
				member.bytes.at(source.sections[1].offset + wall * 25 + 20) = uint8_t(hosts[wall]);
			const auto blockers = xl_compat::runtime_blockers(xl_compat::inspect_level(member.bytes));
			require(blockers.empty(), xl_compat::blocker_message(blockers).c_str());
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "Trigger fixture member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(uint8_t(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "trigcap.hog").string().c_str(), output);
	std::ofstream descriptor(root / "trigcap.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived XL triggers\nnum_levels = 1\nneptunxl.rl2\n";
}

static void trigger_capacity_cross(int wall)
{
	const auto &host = Walls[wall];
	check_trigger(&Segments[host.segnum], short(host.sidenum), short(Players[Player_num].objnum), 0);
}

static void trigger_capacity_object_links()
{
	int owner = -1;
	for (int index = 0; index <= Highest_object_index; ++index)
		if (Objects[index].type == OBJ_POWERUP) {
			owner = index;
			break;
		}
	require(owner >= 0, "Object-link control has no nonplayer owner");
	const auto host = Walls[7];
	const std::vector<trigger> original(Triggers, Triggers + Num_triggers);
	for (int id : { 128, 253, 254, 255 })
		for (int type : { 33, 32 }) {
			Walls[7].trigger = static_cast<sbyte>(id);
			xl_compat::Inventory sample;
			// Source loading validates targets against the authored table bounds
			sample.segments.resize(Num_segments);
			sample.objects.resize(Highest_object_index + 1);
			sample.object_triggers = 1;
			sample.object_trigger_records.push_back({ { type, 0, 0, -1, { { host.segnum, host.sidenum } } }, owner });
			xl_object_trigger_load(sample);
			xl_object_trigger_event(owner, 0);
			if (id < Num_triggers) require(bool(Triggers[id].flags & TF_DISABLED) == (type == 33), "Object action failed to resolve its high wall-trigger target");
			else require(!std::memcmp(Triggers, original.data(), original.size() * sizeof(trigger)), "Missing or invalid object action target changed trigger state");
		}
	Walls[7] = host;
	std::memcpy(Triggers, original.data(), original.size() * sizeof(trigger));
	xl_object_trigger_clear();
	std::cout << "Object enable/disable resolved high targets and skipped missing/invalid byte identities\n";
}

static void trigger_capacity_finish_wall(int type)
{
	for (int frame = 0; frame < 65; ++frame) {
		FrameTime = F1_0 / 64;
		wall_frame_process();
		doorway_rays(WALL_IS_DOORWAY(&Segments[24], 5));
	}
	require(Num_cloaking_walls == 0 && Walls[0].type == type && Walls[1].type == type, "High-ID action did not complete its real wall transition");
}

static void trigger_capacity_saved_controls(char *save, int limit)
{
	auto *file = PHYSFSX_openReadBuffered(save);
	require(file, "Cannot read trigger save control");
	xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file)));
	require(PHYSFS_readBytes(file, bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()) && PHYSFS_close(file), "Cannot read saved trigger bytes");
	xl_compat::Bytes packed;
	for (int index = 0; index < Num_triggers; ++index) {
		const auto *row = reinterpret_cast<const uint8_t *>(&Triggers[index]);
		packed.insert(packed.end(), row, row + TRIGGER_DISK_SIZE);
	}
	const auto at = std::search(bytes.begin(), bytes.end(), packed.begin(), packed.end());
	require(at != bytes.end() && at - bytes.begin() >= 4 && std::search(at + 1, bytes.end(), packed.begin(), packed.end()) == bytes.end(), "Saved trigger table is not uniquely located");
	const auto table = static_cast<std::size_t>(at - bytes.begin());
	const int original_count = Num_triggers;
	char invalid_name[] = "trigcap-invalid.sav";
	for (int variant = 0; variant < (xl_level_extensions_active() ? 4 : 3); ++variant) {
		auto invalid = bytes;
		if (variant == 2) invalid.resize(table + TRIGGER_DISK_SIZE / 2);
		else {
			const unsigned count = variant == 0 ? unsigned(-1) : variant == 1 ? unsigned(limit + 1)
			                                                                  : unsigned(original_count - 1);
			for (unsigned byte = 0; byte < 4; ++byte) invalid[table - 4 + byte] = uint8_t(count >> (8 * byte));
		}
		write_bytes((std::filesystem::path(PHYSFS_getWriteDir()) / invalid_name).string().c_str(), invalid);
		require(!state_restore_all_sub(invalid_name, 0), "Malformed saved trigger table accepted");
		require(Num_triggers == original_count && !xl_loading_extended_triggers(), "Invalid saved trigger count installed or load permission leaked");
		require(state_restore_all_sub(save, 0), "Valid trigger restore failed after malformed table");
	}
}

static void trigger_capacity_demo()
{
	const auto read = [](const char *name) {
		auto *file = PHYSFSX_openReadBuffered(name);
		require(file, "Cannot read high-trigger demo");
		xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file)));
		require(PHYSFS_readBytes(file, bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()) && PHYSFS_close(file), "Cannot read high-trigger demo payload");
		return bytes;
	};
	StartNewGame(1);
	outfile = PHYSFSX_openWriteBuffered("trigcap.dem");
	require(outfile, "Cannot record high-trigger demo");
	Newdemo_state = ND_STATE_RECORDING;
	trigger_capacity_cross(0);
	newdemo_record_start_frame(F1_0 / 10);
	const uint8_t eof = 0;
	require(PHYSFS_writeBytes(outfile, &eof, 1) == 1 && PHYSFS_close(outfile), "Cannot close high-trigger demo");
	outfile = nullptr;
	const auto bytes = read("trigcap.dem");
	require(bytes.size() > 17 && bytes[0] == 9 && xl_preview_trigger_activated(128), "Actual wall crossing did not record a high-ID trigger");
	for (int rewrite : { 0, 1 }) {
		Newdemo_state = Newdemo_vcr_state = ND_STATE_NORMAL;
		StartNewGame(1);
		require(!xl_preview_trigger_activated(128), "Fresh level retained diagnostic activation");
		Newdemo_state = ND_STATE_PLAYBACK;
		Newdemo_vcr_state = ND_STATE_PLAYBACK;
		infile = PHYSFSX_openReadBuffered("trigcap.dem");
		if (rewrite) outfile = PHYSFSX_openWriteBuffered("trigcap.rewrite");
		require(infile && (!rewrite || outfile) && newdemo_read_frame_information(rewrite) == 1 && PHYSFS_close(infile), "High-ID trigger demo decoding failed");
		infile = nullptr;
		if (rewrite) {
			require(PHYSFS_close(outfile), "High-ID rewrite close failed");
			outfile = nullptr;
			require(read("trigcap.rewrite") == xl_compat::Bytes(bytes.begin(), bytes.end() - 1), "High-ID trigger demo rewrite changed payload");
		} else require(xl_preview_trigger_activated(128) && Triggers[128].type == TT_CLOSE_WALL && Num_cloaking_walls == 1, "Playback failed to resolve the signed high trigger identity");
	}
	Newdemo_state = Newdemo_vcr_state = ND_STATE_NORMAL;
	std::cout << "High-ID authored crossing recorded, played through real activation and rewritten byte-for-byte\n";
}

static void trigger_capacity_rejections(const std::filesystem::path &root)
{
	auto *file = PHYSFSX_openReadBuffered("neptunxl.rl2");
	require(file, "Trigger source missing");
	xl_compat::Bytes original(static_cast<std::size_t>(PHYSFS_fileLength(file)));
	require(PHYSFS_readBytes(file, original.data(), original.size()) == PHYSFS_sint64(original.size()) && PHYSFS_close(file), "Cannot read trigger source");
	const auto source = xl_compat::inspect_level(original);
	const auto world = arena_state();
	const std::vector<trigger> before(Triggers, Triggers + Num_triggers);
	const auto shadow = root / "trigger-shadow";
	std::filesystem::create_directories(shadow);
	require(PHYSFS_addToSearchPath(shadow.string().c_str(), 0), "Cannot mount trigger rejection controls");
	for (int variant = 0; variant < 3; ++variant) {
		auto bytes = original;
		if (!variant) bytes = xl_test::expand_triggers(original, 255);
		else if (variant == 1) bytes.at(source.sections[1].offset + 20) = 254;
		else bytes.resize(source.sections[3].offset + 254 * 52 - 1);
		write_bytes((shadow / "neptunxl.rl2").string().c_str(), bytes);
		const char *owner = PHYSFS_getRealDir("neptunxl.rl2");
		require(owner && std::filesystem::weakly_canonical(owner) == std::filesystem::weakly_canonical(shadow), "Malformed trigger control does not own the resolved level");
		require(!xl_level_supported("neptunxl.rl2", 0), "Malformed trigger source admitted");
		if (!variant) require(std::strstr(xl_level_rejection(), "more than 254 triggers"), "Trigger capacity error omits limit");
		if (variant == 1) require(std::strstr(xl_level_rejection(), "invalid wall trigger"), "Trigger identity error omits reason");
		require(arena_state() == world && Num_triggers == int(before.size()) && !std::memcmp(Triggers, before.data(), before.size() * sizeof(trigger)) && !xl_loading_extended_triggers(), "Read-only trigger rejection mutated world or granted loader permission");
	}
	require(PHYSFS_removeFromSearchPath(shadow.string().c_str()) && xl_level_supported("neptunxl.rl2", 0), "Trigger rejection did not recover");
	const auto probe = xl_compat::prepare_gameplay_probe(original);
	write_bytes((std::filesystem::path(PHYSFS_getWriteDir()) / "trig-unowned.rl2").string().c_str(), probe.stock_level);
	char error[256]{};
	require(load_base_mission("d2", error, sizeof(error)), error);
	require(load_level("trig-unowned.rl2") != 0 && !xl_loading_extended_triggers(), "Unowned stock adapter bypassed the 100-trigger limit");
	std::cout << "Over-capacity, invalid ID, truncated and unowned trigger sources rejected with recovery\n";
}

static int trigger_capacity_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto output = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		write_trigger_capacity_fixture(output / "missions");
		require(load_base_mission("trigcap", error, sizeof(error)), error);
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active() && !xl_loading_extended_triggers() && Num_triggers == 254, "Extended trigger load or transient permission failed");
		require(wall_trigger_index(Walls[0].trigger) == 128 && wall_trigger_index(Walls[1].trigger) == 253 && wall_trigger_index(Walls[0].controlling_trigger) == 253, "Native trigger identity narrowed");
		route_snapshot_summary snapshot{};
		require(level_metadata_get_canonical_route_snapshot(&snapshot) && snapshot.trigger_count == 254 && snapshot.wall_count == Num_walls, "Metadata snapshot rejected the high-trigger table");
		trigger_capacity_object_links();
		// Restore authored object bindings after the synthetic endpoint controls
		StartNewGame(1);
		Game_suspended = SUSP_ROBOTS;
		obj_relink(ConsoleObject - Objects, 0);
		compute_segment_center(&ConsoleObject->pos, &Segments[0]);
		ConsoleObject->mtype.phys_info.velocity = {};
		trigger_capacity_cross(4); // 127 disables 253
		require(Triggers[253].flags & TF_DISABLED, "ID 127 did not disable high target");
		trigger_capacity_cross(1);
		require(!Num_cloaking_walls && !xl_preview_trigger_activated(253), "Disabled high trigger operated");
		trigger_capacity_cross(5); // 200 enables 253
		require(!(Triggers[253].flags & TF_DISABLED), "ID 200 did not enable high target");
		trigger_capacity_cross(2); // 99 -> 128
		require(Num_cloaking_walls == 1 && Triggers[128].type == TT_CLOSE_WALL && xl_preview_trigger_activated(99) && xl_preview_trigger_activated(128), "Master did not reach high alternate action");
		trigger_capacity_finish_wall(WALL_OPEN);
		trigger_capacity_cross(3); // 100 -> delayed 253
		xl_trigger_state pending{};
		require(xl_wall_trigger_state(100, &pending) && pending.operated && pending.delay_ms == 1000 && Walls[0].type == WALL_OPEN, "High delayed master did not retain pending state");
		Players[Player_num].energy = i2f(61);
		char save[] = "trigcap.sav", description[] = "XL high trigger pending";
		stop_time();
		require(state_save_all_sub(save, description), "High trigger save failed");
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active() && Num_triggers <= 100, "Trigger stock transition failed");
		require(!xl_loading_extended_triggers() && !xl_preview_trigger_activated(128), "Trigger context leaked into Redux");
		require(state_restore_all_sub(save, 0), "High trigger save failed through stock");
		start_time();
		xl_trigger_state restored{};
		require(Num_triggers == 254 && Triggers[128].type == TT_CLOSE_WALL && xl_wall_trigger_state(100, &restored) && restored.elapsed == pending.elapsed && restored.delay_ms == pending.delay_ms && Players[Player_num].energy == i2f(61), "High trigger restore lost earned state");
		GameTime64 += F1_0 / 2;
		xl_wall_trigger_frame();
		require(Walls[0].type == WALL_OPEN && !(Triggers[253].flags & TF_DISABLED), "Restored high trigger fired before its deadline");
		GameTime64 += F1_0 / 2 + 1;
		xl_wall_trigger_frame();
		require(Num_cloaking_walls == 1 && (Triggers[253].flags & TF_DISABLED) && xl_preview_trigger_activated(253), "Restored master lost high target or one-shot state");
		trigger_capacity_finish_wall(WALL_CLOSED);
		stop_time();
		require(state_save_all_sub(save, description), "Completed high trigger save failed");
		trigger_capacity_saved_controls(save, DXX_XL_MAX_TRIGGERS);
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Stock save control load failed");
		char stock_save[] = "trig-stock.sav";
		require(Num_triggers > 0 && state_save_all_sub(stock_save, description), "Stock trigger save control failed");
		trigger_capacity_saved_controls(stock_save, DXX_CLASSIC_MAX_TRIGGERS);
		start_time();
		require(load_base_mission("trigcap", error, sizeof(error)), error);
		trigger_capacity_demo();
		trigger_capacity_rejections(output);
		std::cout << "XL trigger capacity native controls passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "Trigger capacity controls failed: " << e.what() << '\n';
		return 1;
	}
}
