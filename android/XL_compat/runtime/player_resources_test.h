static void write_player_resources_fixture(const std::filesystem::path &root)
{
	write_trigger_flags_fixture(root);
	std::ifstream input(root / "trigflag.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			const auto source = xl_compat::inspect_level(member.bytes);
			const auto patch = [&](std::size_t offset, std::uint32_t value, int width = 4) { for (int i=0;i<width;++i) member.bytes.at(offset+i)=static_cast<unsigned char>(value>>(8*i)); };
			const auto start = source.sections[3].offset;
			std::fill(member.bytes.begin() + start, member.bytes.begin() + start + 8 * 52, 0);
			const auto action = [&](int index, int type, int flags, int value, int time = -1) { const auto base=start+index*52; patch(base,type,1);patch(base+1,flags,1);patch(base+4,value);patch(base+8,time); };
			action(0, 17, 8, 3 * F1_0);
			action(1, 18, 8, 5 * F1_0);
			action(2, 17, TF_ONE_SHOT, 201 * F1_0);
			action(3, 17, 0, -25 * F1_0);
			action(4, 18, 0, -50 * F1_0);
			action(5, 17, 0, 5 * F1_0, 500);
			action(6, 31, 0, 0);
			action(7, 17, 16 | 32 | 64, F1_0);
			patch(start + 6 * 52 + 2, 2, 1);
			for (int i = 0; i < 2; ++i) {
				patch(start + 6 * 52 + 12 + i * 2, source.walls[i].segment, 2);
				patch(start + 6 * 52 + 32 + i * 2, source.walls[i].side, 2);
			}
			// A protected source volume proves authored subtraction is not combat damage
			const auto properties = source.game_offset - (source.vertices.size() + source.segments.size() * 7 + 910) * 13;
			patch(properties + 3 * 13 + 4, 8, 1);
			for (int i = 0; i < 10; ++i) patch(start + 8 * 52 + 4 + i * 53 + 1, TF_DISABLED, 2);
			require(xl_compat::runtime_blockers(xl_compat::inspect_level(member.bytes)).empty(), "resource fixture failed admission");
		}
		char name[13] = {};
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (int i = 0; i < 4; ++i) output.push_back(static_cast<unsigned char>(member.bytes.size() >> (8 * i)));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "resource.hog").string().c_str(), output);
	std::ofstream descriptor(root / "resource.mn2");
	descriptor << "d2x-name = Derived XL resources\nnum_levels = 1\nneptunxl.rl2\n";
}
static int player_resources_test(int argc, char **argv)
{
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		write_player_resources_fixture(root);
		require(load_base_mission("resource", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active() && Num_triggers == 8, "resource source did not load");
		const auto fire = [&](int index) {xl_wall_trigger_reset();return check_trigger_sub(index,Player_num,1); };
		auto &player = Players[Player_num];
		player.shields = player.energy = i2f(100);
		obj_relink(ConsoleObject - Objects, 3);
		compute_segment_center(&ConsoleObject->pos, &Segments[3]);
		require(xl_preview_no_damage_segment(3), "resource control lacks source protection");
		player.flags |= PLAYER_FLAGS_INVULNERABLE;
		check_trigger(&Segments[Walls[0].segnum], Walls[0].sidenum, ConsoleObject - Objects, 1);
		require(player.shields == i2f(97) && ConsoleObject->shields == player.shields, "authored resource action was suppressed by combat protection");
		player.flags &= ~PLAYER_FLAGS_INVULNERABLE;
		require(fire(1) == 0 && player.energy == i2f(95), "energy action did not use current-energy percentage");
		require(fire(6) == 0 && player.shields == i2f(94) && player.energy == i2f(90) + F1_0 / 4, "wall master did not dispatch resource leaves in order");
		// Immediate object masters retain the linked wall action's own value
		trigger master{};
		master.type = 31;
		master.num_links = 1;
		master.seg[0] = Walls[0].segnum;
		master.side[0] = Walls[0].sidenum;
		require(xl_object_master_action(&master, 49) == 0 && player.shields == i2f(91), "object master lost linked wall resource semantics");
		require(fire(3) == 0 && player.shields == i2f(116) && fire(4) == 0 && player.energy == i2f(135) + 3 * F1_0 / 8, "negative resource actions did not replenish");
		require(fire(7) == 0 && Triggers[7].type == 17 && Triggers[7].flags == 112 && player.shields == i2f(115), "inert/alternate resource flags differ");
		// Inert target bytes survive operation and ordinary save restoration
		Triggers[0].num_links = 1;
		Triggers[0].seg[0] = -32768;
		Triggers[0].side[0] = -32768;
		require(fire(0) == 0 && player.shields == i2f(112), "inert resource links were interpreted as geometry");
		xl_wall_trigger_reset();
		require(check_trigger_sub(5, Player_num, 1) == 1, "resource timer did not arm");
		GameTime64 += F1_0 / 4;
		xl_trigger_state pending;
		require(xl_wall_trigger_state(5, &pending) && pending.delay_ms == 500, "resource delay missing");
		char save[] = "xl-resource.sav", description[] = "XL player resources";
		stop_time();
		require(state_save_all_sub(save, description), "resource save failed");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "resource stock transition failed");
		trigger stock{};
		stock.type = 17;
		stock.value = F1_0;
		const auto stock_shields = player.shields;
		require(!xl_player_resource_trigger(&stock) && player.shields == stock_shields, "XL resource handling contaminated stock");
		require(state_restore_all_sub(save, 0) && player.shields == i2f(112) && player.energy == i2f(135) + 3 * F1_0 / 8 && Triggers[0].seg[0] == -32768, "disk lost resources or inert target data");
		xl_trigger_state restored;
		require(xl_wall_trigger_state(5, &restored) && restored.elapsed == pending.elapsed, "disk lost resource deadline");
		GameTime64 += F1_0 / 4 - 1;
		triggers_frame_process();
		require(player.shields == i2f(112), "resource timer fired early");
		++GameTime64;
		triggers_frame_process();
		require(player.shields == i2f(107) && (Triggers[5].flags & TF_DISABLED), "restored resource deadline did not execute");
		Triggers[3].value = -i2f(1000);
		require(fire(3) == 0 && player.shields == i2f(200), "resource replenishment exceeded default cap");
		Triggers[0].value = i2f(200);
		require(fire(0) == 0 && player.shields == 0 && !Player_is_dead, "zero shields incorrectly caused death");
		Triggers[2].value = 1;
		require(fire(2) == 0 && player.shields == -1 && Player_is_dead && (Triggers[2].flags & TF_DISABLED), "negative shield boundary did not start ordinary death");
		const auto dead_energy = player.energy;
		Triggers[1].value = i2f(50);
		require(fire(1) == 0 && player.energy == dead_energy, "already-dead player lost resources again");
		std::cout << "XL player resource controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
