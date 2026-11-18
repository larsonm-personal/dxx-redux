// Android automation only: native lifecycle composed with ordinary disk restore
nlohmann::json strip_phases;
std::vector<wall> strip_walls;
std::vector<active_door> strip_doors;
std::vector<expl_wall> strip_explosions;

void strip_verify()
{
	require(xl_level_extensions_active() && !strip_phases.empty() && xl_door_phases_snapshot() == strip_phases &&
	            std::size_t(Num_walls) == strip_walls.size() && !std::memcmp(Walls, strip_walls.data(), strip_walls.size() * sizeof(wall)) &&
	            std::size_t(Num_open_doors) == strip_doors.size() &&
	            (strip_doors.empty() || !std::memcmp(ActiveDoors, strip_doors.data(), strip_doors.size() * sizeof(active_door))) &&
	            strip_explosions.size() == MAX_EXPLODING_WALLS && !std::memcmp(expl_wall_list, strip_explosions.data(), sizeof(expl_wall_list)),
	        "Android strip restore changed phases, walls, door clocks or explosion timers");
}

void strip_control(const char *operation)
{
	if (!std::strcmp(operation, "prepare")) {
		require(game_is_time_paused() && xl_level_extensions_active() && Num_walls > 3 && xl_pog_door_frames(Walls[0].clip_num) == 16,
		        "Android strip control requires a paused derived fixture");
		for (int i = Highest_object_index; i >= 0; --i)
			if (Objects[i].type != OBJ_NONE && i != ConsoleObject - Objects) obj_delete(i);
		obj_relink(ConsoleObject - Objects, 10);
		compute_segment_center(&ConsoleObject->pos, &Segments[10]);
		ConsoleObject->size = F1_0 / 16;
		ConsoleObject->mtype.phys_info.velocity = {};
		const int clip = Walls[0].clip_num, independent = Num_walls - 2;
		for (int index : { 0, 1, independent, independent + 1 }) {
			auto &wall = Walls[index];
			wall.hps = WALL_HPS;
			auto &side = Segments[wall.segnum].sides[wall.sidenum];
			side.tmap_num = WallAnims[clip].frames[0];
			side.tmap_num2 = 0;
		}
		const fix before = FrameTime;
		if (Walls[0].type == WALL_DOOR) {
			wall_open_door(&Segments[Walls[0].segnum], Walls[0].sidenum);
			FrameTime = 3 * (WallAnims[clip].play_time / 16);
			wall_frame_process();
			wall_open_door(&Segments[Walls[independent].segnum], Walls[independent].sidenum);
			FrameTime = WallAnims[clip].play_time / 16;
			wall_frame_process();
			require(Num_open_doors == 2, "Android strip controls lost independent doors");
		} else if (WallAnims[clip].flags & WCF_EXPLODES) {
			wall_damage(&Segments[Walls[0].segnum], Walls[0].sidenum, WALL_HPS);
			FrameTime = F1_0 / 4;
			do_exploding_wall_frame();
			require(expl_wall_list[0].segnum == Walls[0].segnum && expl_wall_list[0].time == F1_0 / 4, "Android strip explosion timer differs");
		} else {
			wall_damage(&Segments[Walls[0].segnum], Walls[0].sidenum, WALL_HPS / 2 + 1);
			require(!(Walls[0].flags & WALL_BLASTED), "Android damaged strip destroyed prematurely");
		}
		FrameTime = before;
	} else if (!std::strcmp(operation, "disk_save")) {
		strip_phases = xl_door_phases_snapshot();
		strip_walls.assign(Walls, Walls + Num_walls);
		strip_doors.assign(ActiveDoors, ActiveDoors + Num_open_doors);
		strip_explosions.assign(expl_wall_list, expl_wall_list + MAX_EXPLODING_WALLS);
		require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL wall strip", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android wall strip");
	} else if (!std::strcmp(operation, "disturb")) {
		strip_verify();
		const fix before = FrameTime;
		FrameTime = WallAnims[Walls[0].clip_num].play_time;
		if (Walls[0].type == WALL_DOOR) wall_frame_process();
		else if (WallAnims[Walls[0].clip_num].flags & WCF_EXPLODES) do_exploding_wall_frame();
		else wall_destroy(&Segments[Walls[0].segnum], Walls[0].sidenum);
		FrameTime = before;
		require(xl_door_phases_snapshot() != strip_phases, "Android strip disturbance did not change native phase");
	} else if (!std::strcmp(operation, "disk_load")) {
		require(!strip_phases.empty() && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android strip");
		strip_verify();
		require(android_open_overlay_pause_if_safe(), "Cannot pause restored Android strip");
		debug_log_force(DLOG_GAME, "Android wall strip restored with exact phases, walls, door clocks and explosion timers");
	} else if (!std::strcmp(operation, "verify")) strip_verify();
	else require(false, "Unknown Android strip operation");
}
