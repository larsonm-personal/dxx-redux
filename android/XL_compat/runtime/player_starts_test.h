extern "C" {
extern int NumNetPlayerPositions;
void gameseq_init_network_players(void);
}

static void player_start_controls(const std::filesystem::path &root)
{
	std::ifstream file(root / "missions/hxmtest.hog", std::ios::binary);
	const xl_compat::Bytes original{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	std::ofstream samples(root / "player-start-samples.txt");
	samples << 8 << '\n';
	for (int count : { 1, 8, 9, 16 })
		for (int ghosts : { 0, 1 }) {
			const std::string mission = "pst" + std::to_string(count) + std::to_string(ghosts);
			xl_compat::Bytes output{ 'D', 'H', 'F' };
			xl_compat::Inventory inventory;
			for (auto member : xl_compat::read_hog(original)) {
				if (!d_stricmp(member.name.c_str(), "alarlof.rl2")) {
					const auto level = xl_compat::inspect_level(member.bytes);
					int rank = 0;
					for (const auto &object : level.objects) {
						if (object.type != OBJ_PLAYER && object.type != OBJ_GHOST) continue;
						member.bytes.at(object.source_offset) = rank < count ? OBJ_PLAYER : OBJ_COOP;
						++rank;
					}
					for (const auto &object : level.objects) {
						if (rank >= count) break;
						if (object.type != OBJ_ROBOT) continue;
						member.bytes.at(object.source_offset) = OBJ_PLAYER;
						++rank;
					}
					inventory = xl_compat::inspect_level(member.bytes);
				}
				char name[13]{};
				require(member.name.size() < sizeof(name), "Player-start fixture member name too long");
				std::strcpy(name, member.name.c_str());
				output.insert(output.end(), name, name + sizeof(name));
				for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(uint8_t(member.bytes.size() >> shift));
				output.insert(output.end(), member.bytes.begin(), member.bytes.end());
			}
			write_bytes((root / "missions" / (mission + ".hog")).string().c_str(), output);
			{
				std::ofstream descriptor(root / "missions" / (mission + ".mn2"));
				descriptor << "d2x-name = XL player starts\nnum_levels = 1\nalarlof.rl2\n";
			}
			std::vector<int> selected, removed;
			for (std::size_t i = 0; i < inventory.objects.size(); ++i) {
				const int type = inventory.objects[i].type;
				if (type == OBJ_PLAYER || type == OBJ_GHOST) selected.push_back(int(i));
				if (type == OBJ_COOP) removed.push_back(int(i));
			}
			require(selected.size() == std::size_t(count) && selected.front() == 0, "Player-start fixture selection changed");
			removed.insert(removed.end(), selected.begin() + 1, selected.end());
			char error[256]{};
			require(load_base_mission(mission.c_str(), error, sizeof(error)), error);
			for (int i = MAX_PLAYERS; i < MAX_PLAYERS + 4; ++i) Players[i].objnum = -300 - i;
			StartNewGame(1);
			if (ghosts) {
				// Runtime ghost selection is independent of authored ghost-model admission
				require(!load_level("alarlof.rl2"), "Ghost cleanup control reload failed");
				for (std::size_t rank = 1; rank < selected.size(); rank += 2) {
					Objects[selected[rank]].type = OBJ_GHOST;
					inventory.objects[selected[rank]].type = OBJ_GHOST;
				}
				for (int i = MAX_PLAYERS; i < MAX_PLAYERS + 4; ++i) Players[i].objnum = -300 - i;
				gameseq_init_network_players();
				gameseq_remove_unused_players();
			}
			require(xl_level_extensions_active() && NumNetPlayerPositions == std::min(count, MAX_PLAYERS) && Players[0].objnum == 0,
			        "XL singleplayer startup changed bounded slots or selected player");
			for (int i = MAX_PLAYERS; i < MAX_PLAYERS + 4; ++i)
				require(Players[i].objnum == -300 - i, "Surplus starts wrote outside network slots");
			for (int i : removed) require(Objects[i].type == OBJ_NONE, "Unused start survived cleanup");
			const auto &position = inventory.objects[0].position;
			require(Objects[0].type == OBJ_PLAYER && Objects[0].id == 0 && Objects[0].pos.x == position[0] &&
			            Objects[0].pos.y == position[1] && Objects[0].pos.z == position[2],
			        "Selected start position/identity changed");
			object_runtime_state state{};
			object_get_runtime_state(&state);
			samples << count << ' ' << ghosts << ' ' << inventory.objects.size() << '\n';
			for (const auto &object : inventory.objects) samples << int(object.type) << ' ';
			samples << '\n'
			        << removed.size() << '\n';
			for (std::size_t i = 0; i < removed.size(); ++i) {
				const int actual = state.free_obj_list[state.num_objects + removed.size() - 1 - i];
				require(actual == removed[i], "Unused start deletion/allocator order differs from source");
				samples << actual << ' ';
			}
			samples << '\n';
			if (count == 16) {
				const auto definitions = gameplay_definitions();
				char save[] = "player-starts.sav", description[] = "Sixteen XL starts";
				stop_time();
				require(state_save_all_sub(save, description), "Sixteen-start save failed");
				require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Start state leaked into classic level");
				require(state_restore_all_sub(save, 0) && gameplay_definitions() == definitions && Objects[0].type == OBJ_PLAYER,
				        "Sixteen-start disk identity or player state changed");
				for (int i : removed) require(Objects[i].type == OBJ_NONE, "Disk restore revived unused starts");
			}
		}
	require(bool(samples), "Cannot write player-start source evidence");
	char error[256]{};
	require(load_base_mission("hxmtest", error, sizeof(error)), error);
	StartNewGame(1);
	std::cout << "XL player starts: eight normal/ghost/coop/allocator/slot-bound cases and disk through stock passed\n";
}
