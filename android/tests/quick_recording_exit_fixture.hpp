// Lifecycle diagnostic using an authored exit, not a public replay capture
#ifndef DXX_BUILD_DESCENT_II
static void test_quick_recording_exit_boundary()
{
	using nlohmann::json;
	int wall = -1;
	for (int i = 0; i < Num_walls; ++i)
		if (Walls[i].trigger >= 0 && (Triggers[Walls[i].trigger].flags & TRIGGER_EXIT)) {
			wall = i;
			break;
		}
	require(wall >= 0, "authored exit trigger exists");
	const int segment = Walls[wall].segnum, side = Walls[wall].sidenum;
	// Open the existing exit door before this bounded lifecycle diagnostic
	FrameTime = F1_0 / 20;
	wall_open_door(&Segments[segment], side);
	for (int step = 0; step < 80 && !(Walls[wall].flags & WALL_DOOR_OPENED); ++step) wall_frame_process();
	require(Walls[wall].flags & WALL_DOOR_OPENED, "authored exit door is traversable");
	vms_vector center, face, inward;
	compute_segment_center(&center, &Segments[segment]);
	compute_center_point_on_side(&face, &Segments[segment], side);
	vm_vec_normalized_dir(&inward, &center, &face);
	obj_relink(Players[Player_num].objnum, segment);
	vm_vec_scale_add(&ConsoleObject->pos, &face, &inward, ConsoleObject->size + F1_0);
	ConsoleObject->last_pos = ConsoleObject->pos;
	vm_vec_copy_scale(&ConsoleObject->mtype.phys_info.velocity, &inward, -100 * F1_0);
	Controls = {};
	FrameTime = F1_0 / 20;
	std::vector<std::string> previous_files;
	char **previous = PHYSFS_enumerateFiles("input_demo_recordings/new");
	for (char **file = previous; file && *file; ++file) previous_files.emplace_back(*file);
	PHYSFS_freeList(previous);
	stop_time(); // The checkpoint writer consumes its caller's save pause
	require(!Endlevel_sequence && input_demo_start_quick_recording_common(), "start native quick recording before exit");
	std::printf("EXIT_SETUP wall=%d segment=%d side=%d type=%d child=%d pos=%d,%d,%d velocity=%d,%d,%d\n", wall, segment, side, Walls[wall].type, Segments[segment].children[side], ConsoleObject->pos.x, ConsoleObject->pos.y, ConsoleObject->pos.z, ConsoleObject->mtype.phys_info.velocity.x, ConsoleObject->mtype.phys_info.velocity.y, ConsoleObject->mtype.phys_info.velocity.z);
	unsigned frames = 0;
	while (!Endlevel_sequence && frames < 10) {
		do_weapon_n_item_stuff();
		calc_game_time();
		GameProcessFrame();
		++frames;
		std::printf("EXIT_FRAME f=%u level=%d segment=%d endlevel=%d recording=%d pos=%d,%d,%d\n", frames, Current_level_num, ConsoleObject->segnum, Endlevel_sequence, input_demo_recorder_is_active(), ConsoleObject->pos.x, ConsoleObject->pos.y, ConsoleObject->pos.z);
	}
	require(Endlevel_sequence && !input_demo_recorder_is_active() && Newdemo_state == ND_STATE_NORMAL,
	        "quick recording stops on completed exit-entry frame");
	input_demo_result actual;
	input_demo_capture_current_result(&actual);
	actual.frame_count = frames;
	std::string actual_text, error;
	require(input_demo_result_to_json_text(actual, &actual_text, &error), error.c_str());
	char **files = PHYSFS_enumerateFiles("input_demo_recordings/new");
	std::string path;
	for (char **file = files; file && *file; ++file) {
		const std::string name(*file);
		if (std::find(previous_files.begin(), previous_files.end(), name) == previous_files.end() &&
		    name.size() >= 9 && name.substr(name.size() - 9) == ".dximdemo") path = "input_demo_recordings/new/" + name;
	}
	PHYSFS_freeList(files);
	require(!path.empty(), "quick recording wrote its original terminal trailer");
	PHYSFS_file *file = PHYSFS_openRead(path.c_str());
	require(file != nullptr, "read native quick recording");
	std::string text(static_cast<size_t>(PHYSFS_fileLength(file)), '\0');
	require(PHYSFS_readBytes(file, &text[0], text.size()) == static_cast<PHYSFS_sint64>(text.size()) && PHYSFS_close(file), "read complete native recording");
	std::istringstream lines(text);
	std::string line;
	json expected;
	unsigned recorded_frames = 0;
	while (std::getline(lines, line)) {
		const auto record = json::parse(line);
		if (record["type"] == "result") expected = record["result"];
		if (record["type"] == "frame") ++recorded_frames;
	}
	require(recorded_frames == frames && expected == json::parse(actual_text), "terminal trailer equals completed gameplay frame, including exit state");
	calc_game_time();
	GameProcessFrame();
	require(!input_demo_recorder_is_active(), "next flythrough frame is not recorded");
	file = PHYSFS_openRead(path.c_str());
	require(file && PHYSFS_fileLength(file) == static_cast<PHYSFS_sint64>(text.size()) && PHYSFS_close(file), "flythrough does not append a recorded frame");
	std::printf("PASS: quick recording snapshots completed exit frame %u without flythrough frames\n", frames);
}
#endif
