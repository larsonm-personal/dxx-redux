// Derived headed disk-cache control; no original campaign traversal claim
nlohmann::json revisit_first, revisit_third, revisit_saved_progress;
std::vector<unsigned char> revisit_pixels;
unsigned revisit_exits = 0;
int revisit_cached_lives = 0;
unsigned revisit_deaths = 0;
void mission_revisit_probe(const char *operation)
{
	using namespace xl_compat;
	if (!std::strcmp(operation, "start")) {
		require(!Game_wind && GameArg.SysUsePlayersDir, "Revisit fixture requires protected player-directory saves");
		char mission[] = "pfrevis";
		require(load_mission_by_name(mission), "Cannot select derived revisit fixture");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		return;
	}
	require(Game_wind && Current_mission && !std::strcmp(Current_mission_filename, "pfrevis") && xl_mission_progress_active(),
	        "Revisit control lost its derived modern mission");
	const auto verify = [&] {
		require(xl_prepared_particle_image() && xl_prepared_particle_image()->bitmap.pixels == revisit_pixels,
		        "Revisit changed the prepared particle image");
		require(xl_mission_progress_state(1) == 1 && xl_mission_progress_state(2) == 0 && (!revisit_exits || xl_mission_progress_state(3) == 1),
		        "Revisit rolled back the current mission timeline");
		if (Current_level_num == 1)
			require(particle_objects_snapshot() == revisit_first && Players[Player_num].energy == 73 * F1_0, "First cached world/player state differs");
		else
			require(Current_level_num == 3 && particle_objects_snapshot() == revisit_third && Players[Player_num].energy == 41 * F1_0, "Third cached world/player state differs");
		require(ConsoleObject->control_type == CT_FLYING && ConsoleObject->movement_type == MT_PHYSICS, "Revisit lost player controls");
	};
	if (!std::strcmp(operation, "capture")) {
		require(Current_level_num == 1 && game_is_time_paused() && particle_objects_snapshot().size() == 2 && xl_prepared_particle_image(),
		        "Revisit capture requires two paused derived particle owners");
		Players[Player_num].energy = 73 * F1_0;
		Players[Player_num].primary_weapon_flags |= 2;
		require(xl_particle_object_set_enabled(49, 0) && xl_particle_object_set_enabled(50, 1), "Cannot set first revisit world");
		revisit_first = particle_objects_snapshot();
		revisit_pixels = xl_prepared_particle_image()->bitmap.pixels;
		revisit_exits = 0;
		revisit_deaths = 0;
		revisit_cached_lives = Players[Player_num].lives;
		verify();
	} else if (!std::strcmp(operation, "leave") || !std::strcmp(operation, "return")) {
		verify();
		require(!game_is_time_paused() && !GameArg.SysInputDemoNoRender && Num_triggers > 0, "Revisit dispatch requires running headed gameplay");
		const int destination = !std::strcmp(operation, "leave") ? 3 : 1;
		require(Current_level_num != destination, "Revisit dispatch already occupies its destination");
		Triggers[0].type = TT_EXIT;
		Triggers[0].flags = 8;
		Triggers[0].value = destination * F1_0;
		Triggers[0].time = 0;
		Triggers[0].num_links = 0;
		xl_wall_trigger_reset();
		input_demo_set_skip_level_intro(1);
		{
			struct Presentation {
				Presentation()
				{
					GameArg.SysInputDemoNoRender = 1;
				}
				~Presentation()
				{
					GameArg.SysInputDemoNoRender = 0;
				}
			} presentation;
			check_trigger_sub(0, Player_num, 0);
		}
		require(Current_level_num == destination && particle_systems().empty(), "Disk revisit failed to load its destination with fresh particle clouds");
		if (!revisit_exits) {
			Players[Player_num].energy = 41 * F1_0;
			Players[Player_num].primary_weapon = 1;
			require(xl_particle_object_set_enabled(49, 1) && xl_particle_object_set_enabled(50, 1), "Cannot set third revisit world");
			revisit_third = particle_objects_snapshot();
		}
		++revisit_exits;
		verify();
		require(Players[Player_num].primary_weapon == 1, "Revisit lost the selected primary weapon");
	} else if (!std::strcmp(operation, "gpu")) {
		verify();
		particle_gpu_probe(xl_prepared_particle_image());
	} else if (!std::strcmp(operation, "save")) {
		verify();
		revisit_saved_progress = mission_progress_snapshot();
		require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL revisit", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save headed revisit checkpoint");
		xl_mission_progress_mark(2, -1);
		Players[Player_num].energy = F1_0;
	} else if (!std::strcmp(operation, "load")) {
		require(state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore headed revisit checkpoint");
		verify();
		require(mission_progress_snapshot() == revisit_saved_progress, "Ordinary Android checkpoint failed to restore its mission timeline");
		require(android_open_overlay_pause_if_safe(), "Cannot pause restored revisit checkpoint");
	} else if (!std::strcmp(operation, "death")) {
		verify();
		require(Current_level_num == 3 && !game_is_time_paused() && !GameArg.SysInputDemoNoRender && !Player_is_dead,
		        "Reactor-death control requires the running third level");
		// Fixture owner selects this pilot; inspect only its exact owned cache
		const char *destroyed_cache = "Players/xl-level-706c61796572-70667265766973-3.sav";
		require(!std::strcmp(Players[Player_num].callsign, "player") && PHYSFSX_exists(destroyed_cache, 0),
		        "Reactor-death control lacks the departing world's prior cache");
		Triggers[0].type = TT_EXIT;
		Triggers[0].value = F1_0;
		Control_center_destroyed = 1;
		Players[Player_num].lives = revisit_cached_lives + 2;
		input_demo_set_skip_level_intro(1);
		{
			struct Presentation {
				Presentation()
				{
					GameArg.SysInputDemoNoRender = 1;
				}
				~Presentation()
				{
					GameArg.SysInputDemoNoRender = 0;
				}
			} presentation;
			DoPlayerDead();
		}
		require(Current_level_num == 1 && xl_mission_progress_state(3) == -1 && xl_mission_progress_state(1) == 1 &&
		            !PHYSFSX_exists(destroyed_cache, 0) && particle_objects_snapshot() == revisit_first,
		        "Reactor death lost destination world, destroyed mission state or cache invalidation");
		require(!Player_is_dead && Players[Player_num].lives == revisit_cached_lives && Players[Player_num].energy == INITIAL_ENERGY &&
		            Players[Player_num].shields == INITIAL_SHIELDS && Players[Player_num].primary_weapon == 0 &&
		            ConsoleObject->control_type == CT_FLYING && ConsoleObject->movement_type == MT_PHYSICS,
		        "Reactor death differs from cached-player restoration followed by ship reset");
		particle_gpu_probe(xl_prepared_particle_image());
		++revisit_deaths;
	} else throw std::runtime_error("Unknown derived revisit operation");
	++checks;
	report = nlohmann::json{ { "passed", true }, { "checks", checks }, { "revisit_exits", revisit_exits }, { "revisit_deaths", revisit_deaths }, { "revisit_level", Current_level_num } }.dump();
}
