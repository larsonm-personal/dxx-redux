#include "trigger_state.h"
static void write_trigger_scheduler_fixture(const std::filesystem::path &root, bool device_profile = false)
{
	write_trigger_chain_fixture(root);
	std::ifstream source(root / "chain.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			const auto inventory = xl_compat::inspect_level(member.bytes);
			const auto &section = *std::find_if(inventory.sections.begin(), inventory.sections.end(), [](const auto &s) { return s.name == "triggers"; });
			const auto patch = [&](std::size_t offset, std::uint32_t value, int width = 4) {
				for (int i = 0; i < width; ++i) member.bytes.at(offset + i) = static_cast<unsigned char>(value >> (i * 8));
			};
			const auto action = [&](int index, int type, int flags, int value, int time, std::initializer_list<int> links) {
				const auto offset = section.offset + index * 52;
				std::fill(member.bytes.begin() + offset, member.bytes.begin() + offset + 52, 0);
				patch(offset, type, 1);
				patch(offset + 1, flags, 1);
				patch(offset + 2, static_cast<unsigned>(links.size()), 1);
				patch(offset + 4, value);
				patch(offset + 8, time);
				int link = 0;
				for (int target : links) {
					patch(offset + 12 + link * 2, inventory.walls.at(target).segment, 2);
					patch(offset + 32 + link++ * 2, inventory.walls.at(target).side, 2);
				}
			};
			action(0, 31, 8, 0, device_profile ? 10000 : 1000, { 1 });
			action(1, 14, 0, 0, -1, {});
			patch(section.offset + 52 + 2, 1, 1);
			patch(section.offset + 52 + 12, 1, 2);
			action(2, 32, 0, 0, -1, { 0, 5 });
			action(3, 33, 0, 0, -1, { 0, 5 });
			action(4, TT_ILLUSION_OFF, 16, 0, -1, { 8 });
			action(5, 31, 128 | 8 | TF_DISABLED, 0, device_profile ? -60000 : -1000, { 7 });
			action(6, 31, 0, 3 * F1_0, -1, {});
			action(7, 33, 0, 0, -1, { 6 });
			const auto &walls = *std::find_if(inventory.sections.begin(), inventory.sections.end(), [](const auto &s) { return s.name == "walls"; });
			patch(walls.offset + 8 * 25 + 16, WALL_ILLUSION, 1);
			patch(walls.offset + 8 * 25 + 17, 0, 2);
			require(xl_compat::runtime_blockers(xl_compat::inspect_level(member.bytes)).empty(), "scheduler source failed feature admission");
		}
		char name[13] = {};
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "timing.hog").string().c_str(), output);
	std::ofstream descriptor(root / "timing.mn2");
	descriptor << "d2x-name = Derived timed controls\nnum_levels = 1\nneptunxl.rl2\n";
}

static int trigger_scheduler_test(int argc, char **argv)
{
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		const bool export_only = !std::strcmp(find_arg_value(argc, argv, "-trigger-scheduler-test"), "export");
		write_trigger_scheduler_fixture(root, export_only);
		if (export_only) {
			std::cout << "Derived Android scheduler fixture generated; no runtime qualification\n";
			return 0;
		}
		require(load_base_mission("timing", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active() && xl_level_trigger_state_required() && Num_triggers == 8, "scheduler source did not load");
		GameTime64 = i2f(10);
		timer_update();
		const int player = ConsoleObject - Objects;
		const auto pose = [&](int segment) { obj_relink(player, segment); compute_segment_center(&ConsoleObject->pos, &Segments[segment]); };
		const auto advance = [&](fix ticks) { GameTime64 += ticks; timer_update(); triggers_frame_process(); };
		const auto unblock = [] { timer_delay(F1_0 * 4 / 5); timer_update(); };
		const auto state = [&](int index) { xl_trigger_state result; require(xl_wall_trigger_state(index, &result), "missing scheduler state"); return result; };
		pose(3);
		require(xl_wall_trigger(0, player, 1) == 1 && state(0).delay_ms == 1000 && state(0).elapsed == 0, "positive delay did not arm");
		int other = -1;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_POWERUP) {
				other = i;
				break;
			}
		require(other >= 0, "missing nonplayer scheduling actor");
		require(xl_wall_trigger(0, other, 0) == 1 && state(0).object == player && state(0).shot == 1, "repeated pending activation replaced first actor");
		advance(F1_0 - 1);
		require(ConsoleObject->segnum == 3 && state(0).operated, "delayed chain fired before boundary");
		advance(1);
		require(ConsoleObject->segnum == 1 && !state(0).operated && !(Triggers[0].flags & TF_DISABLED) && Triggers[0].time == 1000, "permanent action did not execute and rearm at boundary");
		unblock();
		pose(3);
		require(xl_wall_trigger(0, player, 1) == 1, "permanent delayed chain did not rearm");
		require(xl_wall_trigger(3, player, 0) == 0 && (Triggers[0].flags & TF_DISABLED), "disable did not suspend pending work");
		advance(i2f(2));
		require(ConsoleObject->segnum == 3 && state(0).operated, "disabled pending action fired");
		require(xl_wall_trigger(2, player, 0) == 0, "pending action did not re-enable");
		Triggers[5].flags |= TF_DISABLED;
		advance(0);
		require(ConsoleObject->segnum == 1 && !state(0).operated, "overdue re-enabled action did not run on next frame");
		unblock();
		pose(3);
		require(xl_wall_trigger(0, player, 1) == 1, "save fixture did not arm");
		advance(F1_0 / 4);
		const auto pending = state(0);
		char save[] = "xl-timing.sav", description[] = "Pending XL timer";
		stop_time();
		require(state_save_all_sub(save, description) != 0, "pending scheduler save failed");
		advance(i2f(2));
		require(ConsoleObject->segnum == 1, "save disturbance did not execute timer");
		require(state_restore_all_sub(save, 0) != 0 && ConsoleObject->segnum == 3 && state(0).elapsed == pending.elapsed && state(0).delay_ms == pending.delay_ms, "pending deadline was not restored");
		advance(F1_0 - static_cast<fix>(pending.elapsed) - 1);
		require(ConsoleObject->segnum == 3, "restored pending action fired early");
		advance(1);
		require(ConsoleObject->segnum == 1, "restored pending action missed deadline");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "scheduler stock transition failed");
		auto *file = PHYSFS_openWrite("stock-xl-state.bin");
		require(file && xl_wall_trigger_write(file) && PHYSFS_tell(file) == 0 && PHYSFS_close(file), "XL state polluted stock save bytes");
		require(state_restore_all_sub(save, 0) != 0 && !std::strcmp(Current_mission_filename, "timing") && state(0).elapsed == pending.elapsed, "stock-to-pending-source restoration failed");
		std::array<xl_trigger_state, MAX_TRIGGERS> rebased{};
		for (int i = 0; i < Num_triggers; ++i) rebased[i] = state(i);
		GameTime64 = 0;
		require(xl_wall_trigger_restore(rebased.data(), Num_triggers) && state(0).elapsed == pending.elapsed, "pending state could not rebase to an earlier restore epoch");
		advance(F1_0 - static_cast<fix>(pending.elapsed) - 1);
		require(ConsoleObject->segnum == 3, "rebased timer fired before its remaining duration");
		advance(1);
		require(ConsoleObject->segnum == 1, "rebased timer lost its remaining duration");
		// Random delay is selected once, uses simulation RNG, and survives restore
		xl_wall_trigger_reset();
		Triggers[0].flags |= TF_DISABLED;
		Triggers[5].flags = 128 | 8;
		Triggers[6].value = 3;
		unsigned before_rng, expected_rng, actual_rng;
		d_rand_get_state(&before_rng);
		const auto before_calls = d_rand_get_call_count();
		const int choice = d_rand() % 10;
		d_rand_get_state(&expected_rng);
		d_rand_set_state(before_rng);
		d_rand_set_call_count(before_calls);
		advance(0);
		d_rand_get_state(&actual_rng);
		require(state(5).delay_ms == 100 + choice * 100 && actual_rng == expected_rng && d_rand_get_call_count() == before_calls + 1 && Triggers[6].value == 3, "random autoplay delay used wrong selection or RNG stream");
		const int ticks = static_cast<fix>(float(state(5).delay_ms) * (65536.0f / 1000.0f));
		advance(ticks - 1);
		require(Triggers[6].value == 3, "random action fired before chosen deadline");
		advance(1);
		require(Triggers[6].value == 4 && !state(5).operated, "permanent autoplay did not execute once and clear operation");
		unblock();
		advance(0);
		require(state(5).operated && Triggers[6].value == 4, "autoplay did not rearm on a later frame");
		const auto random_pending = state(5);
		d_rand_get_state(&before_rng);
		char random_save[] = "xl-random.sav";
		stop_time();
		require(state_save_all_sub(random_save, description) != 0, "random timer save failed");
		advance(i2f(2));
		require(state_restore_all_sub(random_save, 0) != 0 && state(5).delay_ms == random_pending.delay_ms && state(5).elapsed == random_pending.elapsed, "restore lost selected randomized delay");
		d_rand_get_state(&actual_rng);
		// Native restore resets the SIM diagnostic count while restoring its seed
		require(actual_rng == before_rng && d_rand_get_call_count() == 0, "randomized restore rerolled simulation RNG");
		// All malformed records are rejected before publishing scheduler arrays
		std::array<xl_trigger_state, MAX_TRIGGERS> snapshot{};
		for (int i = 0; i < Num_triggers; ++i) snapshot[i] = state(i);
		for (int field = 0; field < 5; ++field) {
			auto bad = snapshot;
			if (field == 0) bad[5].elapsed = -1;
			if (field == 1) bad[5].delay_ms = 900001;
			if (field == 2) bad[5].object = MAX_OBJECTS;
			if (field == 3) bad[5].shot = 2;
			if (field == 4) bad[5].operated = 2;
			require(!xl_wall_trigger_restore(bad.data(), Num_triggers) && state(5).delay_ms == random_pending.delay_ms, "malformed restore published partial state");
		}
		auto overflow = snapshot;
		overflow[5].elapsed = INT64_MAX;
		require(!xl_wall_trigger_restore(overflow.data(), Num_triggers), "scheduler elapsed time exceeded native epoch bounds");
		file = PHYSFS_openWrite("scheduler-state.bin");
		require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "scheduler extension probe write failed");
		const auto user = root.parent_path() / "preview-user";
		std::ifstream saved_state(user / "scheduler-state.bin", std::ios::binary);
		xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(saved_state), std::istreambuf_iterator<char>() };
		require(encoded.size() > 16, "missing bounded scheduler payload");
		for (int variant = 0; variant < 6; ++variant) {
			auto corrupt = encoded;
			if (variant == 0) corrupt.clear();
			if (variant == 1) corrupt.resize(15);
			if (variant == 2) corrupt[12] ^= 1;
			if (variant == 3) std::fill(corrupt.begin() + 8, corrupt.begin() + 12, 255);
			if (variant >= 4) {
				auto payload = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
				payload["states"][0] = { 0, 1000, player, 0, 1 };
				if (variant == 4) payload["states"][5][2] = std::uint64_t(1) << 32;
				else payload["source"] = payload["source"].get<std::uint64_t>() ^ 1;
				const auto text = payload.dump();
				corrupt.resize(16);
				corrupt.insert(corrupt.end(), text.begin(), text.end());
				std::uint32_t checksum = 2166136261u;
				for (unsigned char byte : text) checksum = (checksum ^ byte) * 16777619u;
				for (int byte = 0; byte < 4; ++byte) {
					corrupt[8 + byte] = static_cast<unsigned char>(text.size() >> (byte * 8));
					corrupt[12 + byte] = static_cast<unsigned char>(checksum >> (byte * 8));
				}
			}
			write_bytes((user / "scheduler-corrupt.bin").string().c_str(), corrupt);
			file = PHYSFS_openRead("scheduler-corrupt.bin");
			require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file), "corrupt scheduling extension accepted");
			require(!state(0).operated && state(5).delay_ms == random_pending.delay_ms, "corrupt extension published partial state");
		}
		Triggers[5].flags |= TF_DISABLED;
		xl_wall_trigger_reset();
		require(xl_wall_trigger(4, player, 1) == 0 && Triggers[4].type == TT_ILLUSION_ON && (Walls[8].flags & WALL_ILLUSION_OFF), "alternating stock child did not change type after action");
		unblock();
		require(xl_wall_trigger(4, player, 1) == 0 && Triggers[4].type == TT_ILLUSION_OFF && !(Walls[8].flags & WALL_ILLUSION_OFF), "alternating action did not restore opposite behavior");
		// Enable/disable alternation shares post-action timing with classic pairs
		xl_wall_trigger_reset();
		Triggers[7].type = 33;
		Triggers[7].flags = 16;
		Triggers[6].value = 3;
		require(xl_wall_trigger(7, player, 0) == 0 && Triggers[7].type == 32 && Triggers[6].value == 4, "disable did not alternate to enable after increment");
		unblock();
		require(xl_wall_trigger(7, player, 0) == 0 && Triggers[7].type == 33 && Triggers[6].value == 3, "alternate enable did not decrement original master count");
		Triggers[7].flags = 0;
		// Immediate autoplay is single-use; immediate permanent autoplay is suppressed
		xl_wall_trigger_reset();
		Triggers[5].time = -1;
		Triggers[5].flags = 128;
		Triggers[6].value = 3;
		advance(0);
		advance(0);
		require(Triggers[6].value == 4 && Triggers[5].flags == (128 | TF_DISABLED), "immediate autoplay was not single-use");
		xl_wall_trigger_reset();
		Triggers[5].flags = 128 | 8;
		advance(0);
		require(!state(5).operated && Triggers[6].value == 4, "immediate permanent autoplay was not suppressed");
		// One-shot still wins over a permanent autoplay repeat
		xl_wall_trigger_reset();
		Triggers[5].time = 100;
		Triggers[5].flags = 128 | 8 | TF_ONE_SHOT;
		advance(0);
		advance(static_cast<fix>(100.0f * (65536.0f / 1000.0f)));
		advance(F1_0);
		require((Triggers[5].flags & TF_DISABLED) && !state(5).operated && Triggers[6].value == 5, "one-shot permanent autoplay repeated");
		const auto old_enable = Triggers[2], old_autoplay = Triggers[5];
		Triggers[2].flags = 128;
		Triggers[2].num_links = 1;
		Triggers[2].seg[0] = Walls[5].segnum;
		Triggers[2].side[0] = Walls[5].sidenum;
		Triggers[5].flags = 128 | 8 | TF_DISABLED;
		Triggers[5].time = 1000;
		xl_wall_trigger_reset();
		advance(0);
		require(state(5).operated && !state(5).elapsed, "enabled later-index autoplay did not start in serialized frame order");
		Triggers[2].flags = 128 | TF_DISABLED;
		Triggers[5].type = 32;
		Triggers[5].flags = 128;
		Triggers[5].time = -1;
		Triggers[5].num_links = 1;
		Triggers[5].seg[0] = Walls[2].segnum;
		Triggers[5].side[0] = Walls[2].sidenum;
		xl_wall_trigger_reset();
		advance(0);
		require(!state(2).operated && !(Triggers[2].flags & TF_DISABLED), "earlier-index autoplay ran before next frame");
		advance(0);
		require(state(2).operated && (Triggers[2].flags & TF_DISABLED), "earlier-index autoplay did not run on next frame");
		Triggers[2] = old_enable;
		Triggers[5] = old_autoplay;
		// A nonpermanent delayed action disables after countdown even without one-shot
		xl_wall_trigger_reset();
		Triggers[1].time = 1000;
		pose(3);
		require(xl_wall_trigger(1, player, 0) == 1, "delayed teleport ran immediately");
		advance(F1_0);
		require(ConsoleObject->segnum == 1 && (Triggers[1].flags & TF_DISABLED) && Triggers[1].time == 0, "nonpermanent completion did not disable and clear time");
		xl_wall_trigger_reset();
		Triggers[0].flags = 0;
		Triggers[0].time = 1000;
		Triggers[1].flags = 0;
		Triggers[1].time = -1;
		const auto other_before = Objects[other];
		pose(3);
		require(xl_wall_trigger(0, other, 0) == 1 && state(0).object == other, "nonplayer delayed master did not retain actor");
		advance(F1_0);
		require(Objects[other].segnum == 1 && ConsoleObject->segnum == 3, "queued nonplayer master moved the local player instead");
		obj_relink(other, other_before.segnum);
		const int other_next = Objects[other].next, other_prev = Objects[other].prev;
		Objects[other] = other_before;
		Objects[other].next = other_next;
		Objects[other].prev = other_prev;
		xl_wall_trigger_reset();
		GameTime64 = 0;
		Triggers[1].flags = 0;
		Triggers[1].time = 100;
		pose(3);
		require(xl_wall_trigger(1, player, 0) == 1 && state(1).operated, "zero-epoch activation did not arm");
		advance(static_cast<fix>(100.0f * (65536.0f / 1000.0f)));
		require(ConsoleObject->segnum == 1 && (Triggers[1].flags & TF_DISABLED), "zero-epoch delayed action never completed");
		xl_wall_trigger_reset();
		Triggers[1].time = 100;
		Triggers[1].flags = 8 | TF_ONE_SHOT;
		pose(3);
		const int dead_before = Player_is_dead;
		Player_is_dead = 1;
		require(xl_wall_trigger(1, player, 0) == 1, "dead-player delay did not retain scheduling semantics");
		advance(static_cast<fix>(100.0f * (65536.0f / 1000.0f)));
		require(ConsoleObject->segnum == 3 && (Triggers[1].flags & TF_DISABLED) && !state(1).operated, "dead-player teleport did not preserve post-action one-shot behavior");
		Player_is_dead = dead_before;
		std::ofstream samples(root.parent_path() / "scheduler-samples.txt");
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
				const bool pending = xl_wall_trigger(0, player, 1) == 1;
				const int selected = state(0).delay_ms;
				int before = -1, after = -1;
				if (pending) {
					const int ticks = static_cast<fix>(float(selected) * (65536.0f / 1000.0f));
					GameTime64 += ticks - 1;
					before = xl_wall_trigger(0, player, 1);
					++GameTime64;
					after = xl_wall_trigger(0, player, 1);
					require(before == 1 && after == 0, "scheduler sample missed exact boundary");
				}
				samples << time << ' ' << token << ' ' << selected << ' ' << pending << ' ' << before << ' ' << after << '\n';
				// Advance the stream even for non-random source times
				if (!pending || time >= 0) d_rand();
			}
		samples.close();
		require(bool(samples), "scheduler samples not written");
		std::cout << "XL wall trigger scheduler controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
