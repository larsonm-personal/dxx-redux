// Original Pumo authored exit dispatch; no earned traversal or modified triggers
nlohmann::json flow_hub_progress, flow_hub_particles;
std::vector<int> flow_spray_owners;
std::vector<unsigned char> flow_spray_pixels;
nlohmann::json flow_spray_activation;
unsigned flow_exits = 0;
void mission_flow_probe(const char *operation)
{
	using namespace xl_compat;
	require(Game_wind && Current_mission && !std::strcmp(Current_mission_filename, "pmines") && xl_level_extensions_active(),
	        "Original exit control lost the Pumo mission");
	require(GameArg.SysUsePlayersDir, "Exit controls require protected player-directory caches");
	if (!std::strcmp(operation, "end_ready")) {
		require(Current_level_num == 1 && Last_level == 14 && Num_triggers > 5 && Triggers[5].type == TT_EXIT &&
		            Triggers[5].value == 29 * F1_0 && !(Triggers[5].flags & TF_DISABLED),
		        "Original hub campaign-end trigger differs from its authored destination");
	} else if (!std::strcmp(operation, "capture")) {
		require(Current_level_num == 1 && game_is_time_paused(), "Capture requires the paused original hub");
		flow_hub_progress = mission_progress_snapshot();
		flow_hub_particles = particle_objects_snapshot();
		flow_exits = 0;
	} else if (!std::strcmp(operation, "restored")) {
		require(Current_level_num == 1 && mission_progress_snapshot() == flow_hub_progress && particle_objects_snapshot() == flow_hub_particles,
		        "Original hub checkpoint lost its campaign or particle state");
	} else if (!std::strncmp(operation, "spray_", 6)) {
		require(Current_level_num == 8, "Spray control requires original Pumo level 8");
		const auto *image = xl_prepared_particle_image(0);
		require(image && image->bitmap.width == 512 && image->bitmap.height == 512, "Original spray lost its smoke atlas");
		if (!std::strcmp(operation, "spray_capture")) {
			auto *file = PHYSFSX_openReadBuffered(Gamesave_current_filename);
			require(file && PHYSFS_fileLength(file) > 0 && PHYSFS_fileLength(file) <= 64 * 1024 * 1024, "Spray source is missing or oversized");
			Bytes bytes(std::size_t(PHYSFS_fileLength(file)));
			const auto count = PHYSFS_readBytes(file, bytes.data(), bytes.size());
			PHYSFS_close(file);
			require(count == PHYSFS_sint64(bytes.size()), "Spray source read was truncated");
			const auto level = inspect_level(bytes);
			flow_spray_owners.clear();
			for (std::size_t i = 0; i < level.objects.size(); ++i)
				if (level.objects[i].render == 11 && level.objects[i].render_data.at(11) == 1) flow_spray_owners.push_back(int(i));
			require(flow_spray_owners.size() == 15, "Original spray population changed");
			flow_spray_pixels = image->bitmap.pixels;
			flow_spray_activation = particle_objects_snapshot();
		} else if (!std::strcmp(operation, "spray_disturb")) {
			require(flow_spray_owners.size() == 15 && xl_particle_object_set_enabled(flow_spray_owners.front(), 0), "Cannot disturb spray activation");
		} else {
			require(!flow_spray_owners.empty() && flow_spray_pixels == image->bitmap.pixels && flow_spray_activation == particle_objects_snapshot(), "Spray lifecycle lost owned pixels or activation");
			if (!std::strcmp(operation, "spray_live")) {
				for (int owner : flow_spray_owners) {
					bool live = false;
					for (const auto &system : particle_systems())
						if (system.object == owner && std::holds_alternative<SmokeEmitter>(system.emitter) && system.count() > 0) live = true;
					require(live, "Original spray owner did not emit live smoke-profile particles");
				}
			} else if (!std::strcmp(operation, "spray_gpu")) particle_gpu_probe(image, 8, true);
			else require(!std::strcmp(operation, "spray_verify"), "Unknown spray control");
		}
	} else if (!std::strcmp(operation, "verify")) {
		require(gl_initialized && !GameArg.SysInputDemoNoRender && xl_mission_progress_state(Current_level_num) == 1 &&
		            ConsoleObject->control_type == CT_FLYING && ConsoleObject->movement_type == MT_PHYSICS,
		        "Directed destination lost headed rendering, visited state or player controls");
	} else {
		int from = 0, destination = 0;
		if (!std::strcmp(operation, "exit2")) {
			from = 1;
			destination = 2;
		}
		if (!std::strcmp(operation, "exit3")) {
			from = 2;
			destination = 3;
		}
		if (!std::strcmp(operation, "exit4")) {
			from = 3;
			destination = 4;
		}
		if (!std::strcmp(operation, "exit14")) {
			from = 4;
			destination = 14;
		}
		if (!std::strcmp(operation, "exit5")) {
			from = 14;
			destination = 5;
		}
		if (!std::strcmp(operation, "exit6")) {
			from = 5;
			destination = 6;
		}
		if (!std::strcmp(operation, "exit7")) {
			from = 6;
			destination = 7;
		}
		if (!std::strcmp(operation, "exit8")) {
			from = 7;
			destination = 8;
		}
		require(from && Current_level_num == from && !game_is_time_paused() && !GameArg.SysInputDemoNoRender,
		        "Authored route dispatch started in the wrong level or pause state");
		int index = -1;
		for (int i = 0; i < Num_triggers; ++i)
			if (Triggers[i].type == TT_EXIT && Triggers[i].value == destination * F1_0 && !(Triggers[i].flags & TF_DISABLED)) {
				require(index < 0, "Authored route has ambiguous matching exits");
				index = i;
			}
		require(index >= 0, "Original level lacks the expected available authored exit");
		input_demo_set_skip_level_intro(1);
		{
			// Suppress synchronous scoring/briefing menus, retain actual save/load
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
			check_trigger_sub(index, Player_num, 0);
		}
		require(Current_level_num == destination, "Original authored exit did not reach its destination");
		if (destination == 6) {
			unsigned mines = 0;
			for (int i = 0; i <= Highest_object_index; ++i) {
				const auto &object = Objects[i];
				if (object.type != OBJ_WEAPON || object.id != PMINE_ID) continue;
				++mines;
				require(object.render_type == RT_POLYOBJ && object.rtype.pobj_info.model_num == Weapon_info[PMINE_ID].model_num &&
				            object.ctype.laser_info.last_hitobj == -1 &&
				            object.size == Polygon_models[Weapon_info[PMINE_ID].model_num].rad &&
				            object.mtype.phys_info.mass == Weapon_info[PMINE_ID].mass && object.mtype.phys_info.drag == Weapon_info[PMINE_ID].drag &&
				            (object.mtype.phys_info.flags & PF_FREE_SPINNING),
				        "Original placed mine lost loader normalization");
			}
			require(mines > 0, "Original level 6 placed-mine control is empty");
			const auto model = Weapon_info[PMINE_ID].model_num;
			Weapon_info[PMINE_ID].model_num = -1;
			const bool rejected = !xl_level_supported_at(Gamesave_current_filename, 0, Current_level_num) &&
			                      std::strstr(xl_level_rejection(), "placed mine requires a polygon model");
			Weapon_info[PMINE_ID].model_num = model;
			require(rejected && xl_level_supported_at(Gamesave_current_filename, 0, Current_level_num),
			        "Placed-mine dependency rejection or recovery failed");
		}
		++flow_exits;
	}
	++checks;
	report = nlohmann::json{ { "passed", true }, { "checks", checks }, { "flow_exits", flow_exits }, { "flow_level", Current_level_num }, { "flow_file", Gamesave_current_filename } }.dump();
}
