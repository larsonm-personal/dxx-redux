// Controlled transition prerequisite; not an earned exit or campaign qualification
extern "C" void AdvanceLevel(int secret_flag);

static void campaign_level_two()
{
	char error[256] = {};
	require(load_base_mission("d2-xl", error, sizeof(error)), error);
	StartNewGame(1);
	require(Current_level_num == 1 && !std::strcmp(Current_mission_filename, "d2-xl"), "campaign transition requires original mission level one");
	PHYSFS_file *file = PHYSFS_openRead("d2leva-2.rl2");
	require(file != nullptr, "original second level unavailable");
	const auto size = PHYSFS_fileLength(file);
	require(size > 0 && size < 64 * 1024 * 1024, "second level size invalid");
	xl_compat::Bytes bytes(static_cast<std::size_t>(size));
	const auto read = PHYSFS_readBytes(file, bytes.data(), bytes.size());
	PHYSFS_close(file);
	require(read == size, "second level read incomplete");
	const auto source = xl_compat::inspect_level(bytes);
	require(source.level_version == 15 && source.game_version == 33 && source.triggers.size() == 39 && source.matcens.size() == 3,
	        "second level inventory differs from audited scope");
	require(xl_level_supported("d2leva-2.rl2", 0), xl_level_rejection());
	// Invoke the real next-level path directly; this does not earn the level-one exit
	AdvanceLevel(0);
	require(Current_level_num == 2 && !std::strcmp(Gamesave_current_filename, "d2leva-2.rl2") && xl_preview_pilot_active(),
	        "native progression did not activate original level two");
	require(Num_segments == source.segments.size() && Num_walls == source.walls.size() && Num_triggers == source.triggers.size(),
	        "level-two native geometry/wall/trigger counts differ");
	const auto count_robots = [] {
		int count = 0;
		for (int i = 0; i <= Highest_object_index; ++i) count += Objects[i].type == OBJ_ROBOT;
		return count;
	};
	const auto authored_robots = std::count_if(source.objects.begin(), source.objects.end(), [](const auto &o) { return o.type == OBJ_ROBOT; });
	require(count_robots() == authored_robots, "level-two startup lost authored robots");
	require(!cheats.enabled && !cheats.ghostphysics && !cheats.monsterdamage && !cheats.robotfiringsuspended &&
	            !(Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE),
	        "level-two control has gameplay assists");
	const auto start = ConsoleObject->pos;
	const auto energy = Players[Player_num].energy;
	std::set<int> shots;
	int remainder = 0;
	for (int frame = 0; frame < 120 && !Player_is_dead; ++frame) {
		remainder += F1_0;
		FrameTime = remainder / 60;
		remainder %= 60;
		std::memset(&Controls, 0, sizeof(Controls));
		Controls.forward_thrust_time = FrameTime;
		Controls.fire_primary_state = 1;
		calc_game_time();
		GameProcessFrame();
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_WEAPON && Objects[i].ctype.laser_info.parent_type == OBJ_PLAYER) shots.insert(Objects[i].signature);
	}
	std::memset(&Controls, 0, sizeof(Controls));
	require(!Player_is_dead && vm_vec_dist(&start, &ConsoleObject->pos) > F1_0 && !shots.empty() && Players[Player_num].energy < energy,
	        "level-two ordinary movement/fire control failed");
	const auto position = ConsoleObject->pos;
	const auto saved_energy = Players[Player_num].energy, shields = Players[Player_num].shields;
	const auto robots = count_robots();
	const auto lights = face_lights();
	char save[] = "xl-level-two.sav", description[] = "Level two control checkpoint";
	stop_time();
	require(state_save_all_sub(save, description), "level-two checkpoint save failed");
	StartNewGame(1);
	require(state_restore_all_sub(save, 0), "level-two checkpoint restore failed");
	require(Current_level_num == 2 && !std::strcmp(Gamesave_current_filename, "d2leva-2.rl2") && xl_preview_pilot_active() &&
	            vm_vec_dist(&position, &ConsoleObject->pos) == 0 && Players[Player_num].energy == saved_energy &&
	            Players[Player_num].shields == shields && count_robots() == robots && face_lights() == lights,
	        "level-two checkpoint changed identity, player state, robots or lights");
	std::cout << "Counterstrike level two prerequisite: native next-level path, ordinary movement/fire and checkpoint recovery verified; earned exit and branches unverified\n";
}
