// Original tunnel geometry with added scenery; controlled pose, real cinematic
nlohmann::json flyout_particle_world;
std::vector<unsigned char> flyout_particle_pixels;
void mission_flyout_probe(const char *operation)
{
	using namespace xl_compat;
	require(Game_wind && Current_mission && !std::strcmp(Current_mission_filename, "pmines") && xl_mission_progress_active() && GameArg.SysUsePlayersDir,
	        "Flyout cache control requires the protected modern Pumo fixture");
	if (!std::strcmp(operation, "prepare")) {
		require(Current_level_num == 2 && game_is_time_paused() && xl_prepared_particle_image(), "Flyout setup requires paused level2 with its atlas");
		endlevel_runtime_state state{};
		endlevel_get_runtime_state(&state);
		require(state.data_loaded && state.exit_segment >= 0, "Derived scenery did not load for the original exterior tunnel");
		int approach = -1;
		for (int segment = 0; segment <= Highest_segment_index && approach < 0; ++segment)
			for (int side = 0; side < 6; ++side) {
				endlevel_route route{};
				if (Segments[segment].children[side] == state.exit_segment && !endlevel_validate_route(segment, side, &route)) {
					approach = segment;
					break;
				}
			}
		require(approach >= 0, "Original exterior lacks a valid approach tunnel");
		obj_relink(ConsoleObject - Objects, approach);
		compute_segment_center(&ConsoleObject->pos, &Segments[approach]);
		vms_vector destination, heading;
		compute_segment_center(&destination, &Segments[state.exit_segment]);
		vm_vec_normalized_dir(&heading, &destination, &ConsoleObject->pos);
		vm_vector_2_matrix(&ConsoleObject->orient, &heading, nullptr, nullptr);
		vm_vec_sub(&ConsoleObject->last_pos, &ConsoleObject->pos, &heading);
		ConsoleObject->mtype.phys_info.velocity = {};
		ConsoleObject->mtype.phys_info.thrust = {};
		Control_center_destroyed = 0;
		Players[Player_num].energy = 83 * F1_0;
		flyout_particle_world = particle_objects_snapshot();
		flyout_particle_pixels = xl_prepared_particle_image()->bitmap.pixels;
	} else if (!std::strcmp(operation, "start")) {
		require(Current_level_num == 2 && !game_is_time_paused() && !Control_center_destroyed && !GameArg.SysInputDemoNoRender,
		        "Intact flyout must start in running headed level2");
		int exit = -1;
		for (int i = 0; i < Num_triggers; ++i)
			if (Triggers[i].type == TT_EXIT && Triggers[i].value == 3 * F1_0) exit = i;
		require(exit >= 0, "Original tunnel has no authored destination3 exit");
		input_demo_set_skip_level_intro(1);
		check_trigger_sub(exit, Player_num, 0);
		require(Endlevel_sequence == 1 && !Control_center_destroyed, "Exit did not enter the intact rendered flythrough");
	} else if (!std::strcmp(operation, "return")) {
		require(Current_level_num == 3 && !Endlevel_sequence && !game_is_time_paused(), "Flyout did not finish through normal level advancement");
		require(xl_mission_progress_state(2) == 1 && xl_mission_progress_state(3) == 1, "Intact flyout lost mission visit state");
		// Only this derived control supplies a return exit for the cache check
		require(Num_triggers > 0, "Derived return requires a trigger slot");
		Triggers[0].type = TT_EXIT;
		Triggers[0].flags = 8;
		Triggers[0].value = 2 * F1_0;
		Triggers[0].time = 0;
		Triggers[0].num_links = 0;
		xl_wall_trigger_reset();
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
		require(Current_level_num == 2 && !Endlevel_sequence && Viewer == ConsoleObject &&
		            ConsoleObject->control_type == CT_FLYING && ConsoleObject->movement_type == MT_PHYSICS &&
		            find_point_seg(&ConsoleObject->pos, ConsoleObject->segnum) == ConsoleObject->segnum,
		        "Flyout cache restored an exterior pose, cinematic controls or camera viewer");
		require(Players[Player_num].energy == 83 * F1_0 && particle_objects_snapshot() == flyout_particle_world &&
		            xl_mission_progress_state(3) == 1 && xl_prepared_particle_image() && xl_prepared_particle_image()->bitmap.pixels == flyout_particle_pixels,
		        "Flyout cache lost player/world state, current mission progress or particle resource");
		particle_gpu_probe(xl_prepared_particle_image());
	} else throw std::runtime_error("Unknown flyout cache operation");
	++checks;
	report = nlohmann::json{ { "passed", true }, { "checks", checks }, { "flyout_level", Current_level_num }, { "flyout_returned", !std::strcmp(operation, "return") } }.dump();
}
