// Derived authored events and controlled clock boundaries; no level navigation
static void write_object_chain_delay_fixture(const std::filesystem::path &root)
{
	write_object_event_fixture(root);
	std::ifstream input(root / "events.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			const auto source = xl_compat::inspect_level(member.bytes);
			const auto patch = [&](std::size_t offset, std::uint32_t value, int width = 4) {
				for (int i = 0; i < width; ++i) member.bytes.at(offset + i) = static_cast<unsigned char>(value >> (8 * i));
			};
			const auto offset = source.sections[3].offset;
			member.bytes.insert(member.bytes.begin() + offset, 2 * 52, 0);
			for (std::size_t i = 0; i < source.sections.size(); ++i)
				if (source.sections[i].offset > offset) patch(source.game_offset + 35 + i * 12, source.sections[i].offset + 2 * 52);
			patch(source.game_offset + 75, 6);
			std::fill(member.bytes.begin() + offset, member.bytes.begin() + offset + 6 * 52, 0);
			const auto target = [&](int wall) { return std::array<int, 2>{ source.walls[wall].segment, source.walls[wall].side }; };
			const auto action = [&](int index, int type, int flags, int time, int value, std::initializer_list<std::array<int, 2>> links) {
				const auto base = offset + index * 52;
				patch(base, type, 1);
				patch(base + 1, flags, 1);
				patch(base + 2, static_cast<unsigned>(links.size()), 1);
				patch(base + 4, value);
				patch(base + 8, time);
				int link = 0;
				for (const auto &entry : links) {
					patch(base + 12 + link * 2, entry[0], 2);
					patch(base + 32 + link++ * 2, entry[1], 2);
				}
			};
			action(0, 31, 8, 20000, 0, { target(1), target(2), target(3) });
			action(1, TT_ILLUSION_OFF, 8 | 16, 1500, 0, { target(8) });
			action(2, 14, 8, 2000, 0, { { 70, 0 }, { 84, 0 } });
			action(3, 31, 8, 500, 0, { target(4), target(5) });
			action(4, 17, 8, 750, i2f(3), {});
			action(5, 18, 8, 500, i2f(5), {});
			const auto wall_offset = source.sections[1].offset;
			for (std::size_t i = 0; i < source.walls.size(); ++i) patch(wall_offset + i * 25 + 20, i < 6 ? static_cast<unsigned>(i) : 255, 1);
			// Only the robot's removal master is enabled; other fixture owners stay inert
			for (int i = 0; i < 10; ++i) {
				const auto base = offset + 6 * 52 + 4 + i * 53;
				patch(base, 31, 1);
				patch(base + 1, i == 0 ? 0 : TF_DISABLED, 2);
				patch(base + 3, 1, 1);
				patch(base + 5, 0);
				patch(base + 9, -1);
				patch(base + 13, source.walls[0].segment, 2);
				patch(base + 33, source.walls[0].side, 2);
			}
			const auto blockers = xl_compat::runtime_blockers(xl_compat::inspect_level(member.bytes));
			require(blockers.empty(), xl_compat::blocker_message(blockers).c_str());
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "delayed fixture member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "objdelay.hog").string().c_str(), output);
	std::ofstream descriptor(root / "objdelay.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived delayed events\nnum_levels = 1\nneptunxl.rl2\n";
}

static int object_chain_delay_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		write_object_chain_delay_fixture(root);
		std::ifstream manifest_file(root / "events-fixture.json");
		const auto manifest = nlohmann::json::parse(manifest_file);
		const int robot = manifest.at("robot");
		const auto powerups = manifest.at("powerups").get<std::vector<int>>();
		require(load_base_mission("objdelay", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active() && Num_triggers == 6 && xl_object_trigger_count() == 10, "delayed source did not load");
		GameTime64 = i2f(20);
		const auto state = [](int index) { xl_trigger_state result; require(xl_wall_trigger_state(index, &result), "missing delayed state"); return result; };
		const auto ticks = [](int milliseconds) { return static_cast<fix>(float(milliseconds) * (65536.0f / 1000.0f)); };
		const auto advance = [&](fix duration) { GameTime64 += duration; timer_update(); triggers_frame_process(); };
		trigger master{};
		master.type = 31;
		master.num_links = 1;
		master.seg[0] = Walls[0].segnum;
		master.side[0] = Walls[0].sidenum;
		char initial[] = "delay-initial.sav", pending_save[] = "delay-pending.sav", description[] = "Delayed object event";
		stop_time();
		require(state_save_all_sub(initial, description), "initial delayed save failed");
		const int original_signature = Objects[robot].signature;
		const int segment = Objects[robot].segnum;
		const auto position = Objects[robot].pos;
		Objects[robot].flags |= OF_SHOULD_BE_DEAD;
		obj_delete_all_that_should_be_dead();
		require(state(0).operated && state(0).attached && state(0).signature == original_signature && state(0).object == robot, "real removal did not retain pending event context");
		const int replacement = obj_create(OBJ_POWERUP, POW_ENERGY, segment, &position, &vmd_identity_matrix, F1_0, CT_POWERUP, MT_NONE, RT_NONE);
		require(replacement == robot && Objects[replacement].signature != original_signature, "removed owner slot was not reused");
		advance(ticks(250));
		const auto pending = state(0);
		stop_time();
		require(state_save_all_sub(pending_save, description), "pending removed-owner save failed");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "delayed stock transition failed");
		require(!xl_level_extensions_active() && !xl_wall_trigger_state(0, nullptr) && xl_wall_trigger(0, ConsoleObject - Objects, 0) == -1, "delayed XL state leaked into stock dispatch");
		require(state_restore_all_sub(pending_save, 0) && state(0).elapsed == pending.elapsed && state(0).attached && state(0).signature == original_signature, "disk lost deleted actor or remaining delay");
		advance(ticks(20000) - static_cast<fix>(pending.elapsed) - 1);
		require(!state(1).operated, "master dispatched children early");
		advance(1);
		require(!state(0).operated && state(1).operated && state(2).operated && state(3).operated && state(1).attached && state(2).signature == original_signature, "deleted-owner master did not propagate context");
		advance(ticks(500));
		require(state(4).operated && state(5).operated && state(4).attached && state(4).signature == original_signature, "nested delayed master lost source identity");
		advance(ticks(500));
		require(Players[Player_num].energy == i2f(95) && Players[Player_num].shields == i2f(100), "nested energy deadline or shield ordering differs");
		advance(ticks(250));
		require(Players[Player_num].shields == i2f(97), "removed owner suppressed delayed resource action");
		advance(ticks(250));
		require((Walls[8].flags & WALL_ILLUSION_OFF) && Triggers[1].type == TT_ILLUSION_ON, "removed owner suppressed delayed geometry or alternate flag");
		unsigned before_rng, after_rng;
		d_rand_get_state(&before_rng);
		advance(ticks(500));
		d_rand_get_state(&after_rng);
		require(Objects[replacement].segnum == segment && !std::memcmp(&Objects[replacement].pos, &position, sizeof(position)) && before_rng == after_rng && !state(2).operated, "stale delayed teleport moved a reused slot or consumed destination RNG");
		// Live attached actors retain object-teleport semantics through a pending save
		require(state_restore_all_sub(initial, 0), "live delayed source restore failed");
		master.seg[0] = Walls[2].segnum;
		master.side[0] = Walls[2].sidenum;
		const auto orientation = Objects[robot].orient;
		const auto velocity = Objects[robot].mtype.phys_info.velocity;
		require(xl_object_master_action(&master, robot) == 0 && state(2).attached && state(2).signature == Objects[robot].signature, "live delayed teleport did not arm");
		require(xl_object_master_action(&master, powerups[0]) == 0 && state(2).object == robot, "later actor replaced the first scheduled actor");
		Triggers[2].flags |= TF_DISABLED;
		advance(ticks(2500));
		require(Objects[robot].segnum == segment && state(2).operated, "disabled pending attached action fired");
		Triggers[2].flags &= ~TF_DISABLED;
		advance(0);
		require((Objects[robot].segnum == 70 || Objects[robot].segnum == 84) && !std::memcmp(&orientation, &Objects[robot].orient, sizeof(orientation)) && !std::memcmp(&velocity, &Objects[robot].mtype.phys_info.velocity, sizeof(velocity)) && !state(2).operated, "live delayed event used wall transform or failed to rearm");
		// The same actor ownership check applies to ordinary XL wall timers
		require(state_restore_all_sub(initial, 0), "wall identity source restore failed");
		require(xl_wall_trigger(2, powerups[0], 0) == 1 && !state(2).attached, "nonplayer wall teleport did not arm");
		const int pickup_segment = Objects[powerups[0]].segnum;
		const auto pickup_position = Objects[powerups[0]].pos;
		obj_delete(powerups[0]);
		const int pickup_replacement = obj_create(OBJ_POWERUP, POW_ENERGY, pickup_segment, &pickup_position, &vmd_identity_matrix, F1_0, CT_POWERUP, MT_NONE, RT_NONE);
		require(pickup_replacement == powerups[0], "wall actor reuse control missing");
		advance(ticks(2000));
		require(Objects[pickup_replacement].segnum == pickup_segment && !state(2).operated, "ordinary XL wall timer moved replacement actor");
		// Structural and lifecycle validation must reject the whole state atomically
		require(state_restore_all_sub(pending_save, 0), "malformed-state setup failed");
		std::array<xl_trigger_state, MAX_TRIGGERS> states{};
		for (int i = 0; i < Num_triggers; ++i) states[i] = state(i);
		for (int variant = 0; variant < 5; ++variant) {
			auto bad = states;
			bad[0].elapsed += 1;
			if (variant == 0) bad[1].attached = 2;
			if (variant == 1) bad[1].attached = 1;
			if (variant == 2) bad[1].signature = 1;
			if (variant == 3) bad[0].signature = -1;
			if (variant == 4) bad[0].object = MAX_OBJECTS;
			require(!xl_wall_trigger_restore(bad.data(), Num_triggers) && state(0).elapsed == states[0].elapsed && state(0).signature == original_signature, "malformed delayed state partially published");
		}
		auto *file = PHYSFS_openWrite("delay-state.bin");
		require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "delayed trailer capture failed");
		const auto user = root.parent_path() / "preview-user";
		std::ifstream encoded_file(user / "delay-state.bin", std::ios::binary);
		const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(encoded_file), std::istreambuf_iterator<char>() };
		require(encoded.size() > 16, "delayed trailer missing");
		for (int variant = 0; variant < 10; ++variant) {
			auto block = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
			block["states"][0][0] = states[0].elapsed + 1;
			if (variant == 0) block.erase("actors");
			if (variant == 1) block["actors"].erase(block["actors"].begin());
			if (variant == 2) block["actors"][0].push_back(0);
			if (variant == 3) block["actors"][0][0] = 2;
			if (variant == 4) block["actors"][0][1] = -1;
			if (variant == 5) block["actors"][0][1] = UINT64_MAX;
			if (variant == 6) block["actors"][0][0] = nlohmann::json::parse("true");
			if (variant == 7) block["actors"][0][1] = 1.5;
			if (variant == 8) block["actors"][1][1] = 1;
			if (variant == 9) block["schema"] = 5;
			const auto text = block.dump();
			xl_compat::Bytes bad(encoded.begin(), encoded.begin() + 16);
			bad.insert(bad.end(), text.begin(), text.end());
			std::uint32_t checksum = 2166136261u;
			for (unsigned char byte : text) checksum = (checksum ^ byte) * 16777619u;
			for (int byte = 0; byte < 4; ++byte) {
				bad[8 + byte] = static_cast<unsigned char>(text.size() >> (byte * 8));
				bad[12 + byte] = static_cast<unsigned char>(checksum >> (byte * 8));
			}
			write_bytes((user / "delay-corrupt.bin").string().c_str(), bad);
			file = PHYSFS_openRead("delay-corrupt.bin");
			require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file) && state(0).elapsed == states[0].elapsed && state(0).signature == original_signature, "malformed actor trailer partially published");
		}
		// Reference samples isolate defined timing from intentional context correction
		require(state_restore_all_sub(initial, 0), "source sample restore failed");
		master.seg[0] = Walls[0].segnum;
		master.side[0] = Walls[0].sidenum;
		std::ofstream samples(root.parent_path() / "object-delay-samples.txt");
		for (int time : { -900001, -900000, -1000, -101, -100, -99, 0, 99, 100, 101, 1000, 900000, 900001, INT32_MIN })
			for (int seed = 0; seed < 10; ++seed) {
				xl_wall_trigger_reset();
				GameTime64 = i2f(20);
				Triggers[0].type = 31;
				Triggers[0].flags = 8;
				Triggers[0].num_links = 0;
				Triggers[0].time = time;
				unsigned rng;
				d_rand_get_state(&rng);
				const int token = d_rand() % 10;
				d_rand_set_state(rng);
				xl_object_master_action(&master, robot);
				const int selected = state(0).delay_ms;
				const bool pending_action = selected >= 0;
				int before = -1, after = -1;
				if (pending_action) {
					advance(ticks(selected) - 1);
					before = state(0).operated;
					advance(1);
					after = state(0).operated;
					require(before == 1 && after == 0, "attached scheduler missed exact source boundary");
				}
				samples << time << ' ' << token << ' ' << selected << ' ' << pending_action << ' ' << before << ' ' << after << '\n';
				if (!pending_action || time >= 0) d_rand();
			}
		samples.close();
		require(bool(samples), "delayed source samples not written");
		std::cout << "XL delayed object chain controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
