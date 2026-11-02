// Original Pumo authored exit dispatch; no earned traversal or modified triggers
nlohmann::json flow_hub_progress, flow_hub_particles;
std::vector<int> flow_spray_owners;
std::vector<unsigned char> flow_spray_pixels;
nlohmann::json flow_spray_activation;
unsigned flow_exits = 0;
nlohmann::json flow_lightning_activation;
std::vector<unsigned char> flow_lightning_pixels;
nlohmann::json flow_interlude_activation, flow_interlude_progress;
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
	} else if (!std::strncmp(operation, "interlude_", 10)) {
		require(Current_level_num == 10 && Num_segments == 19 && Num_walls == 5 && Num_triggers == 1 &&
		            Highest_object_index >= 1 && Objects[1].type == 20 && Objects[1].id == 2 && Objects[1].segnum == 3,
		        "Original interlude lost its geometry or ambient-fan owner");
		require(Triggers[0].type == 29 && Triggers[0].value == F1_0 && Triggers[0].time == 5 &&
		            !std::strcmp(xl_message_identity(), "32e49a35fd856a74c5f5c334f18f2b0305b81de2d49b7f80c8b80165b14aa440"),
		        "Original interlude lost its authored message table or action");
		if (!std::strcmp(operation, "interlude_message")) {
			require(!game_is_time_paused() && xl_message_draws() > 0 && xl_message_drawn_id() == 1 &&
			            !std::strcmp(xl_message_text(), "Thanks for playing Mineral Valley!\nYou can now exit to the main menu."),
			        "Original interlude did not automatically display its completion message");
		} else if (!std::strcmp(operation, "interlude_capture")) {
			require(game_is_time_paused() && xl_ambient_object_enabled(1), "Interlude capture requires its paused authored sound owner");
			flow_interlude_activation = ambient_objects_snapshot();
			flow_interlude_progress = mission_progress_snapshot();
			Players[Player_num].energy = 77 * F1_0;
		} else if (!std::strcmp(operation, "interlude_disturb")) {
			require(xl_ambient_object_set_enabled(1, 0), "Cannot disturb original ambient fan activation");
			Players[Player_num].energy = 19 * F1_0;
		} else {
			require(!flow_interlude_activation.empty() && ambient_objects_snapshot() == flow_interlude_activation &&
			            mission_progress_snapshot() == flow_interlude_progress && Players[Player_num].energy == 77 * F1_0,
			        "Interlude checkpoint lost ambient activation, progression or player state");
			if (!std::strcmp(operation, "interlude_live")) {
				const auto audio = named_audio_snapshot();
				require(ambient_audio_matches(audio), "Interlude ambient audio differs from its authored resource");
				unsigned owners = 0;
				for (const auto &owner : audio.at("linked").at("owners"))
					if (owner.at("object") == 1 && owner.at("signature") == Objects[1].signature && owner.at("external") == true) ++owners;
				require(owners == 1, "Interlude did not retain exactly one ambient fan owner");
			} else require(!std::strcmp(operation, "interlude_verify"), "Unknown original interlude control");
		}
	} else if (!std::strncmp(operation, "lightning_", 10)) {
		require(Current_level_num == 9 && Highest_object_index >= 307 && Objects[307].type == 20 && Objects[307].id == 1 && Objects[307].segnum == 362,
		        "Original lightning control requires tulev4 owner 307 in segment 362");
		const auto *image = xl_prepared_lightning_image();
		require(image && image->bitmap.width == 256 && image->bitmap.height == 256, "Original lightning lost its owned corona");
		const auto activation = lightning_objects_snapshot();
		require(activation.size() == 1 && activation[0][0] == 307 && activation[0][3] == true, "Original lightning owner population changed");
		if (!std::strcmp(operation, "lightning_capture")) {
			require(game_is_time_paused() && activation[0][2] == true, "Lightning capture requires paused authored activation");
			flow_lightning_activation = activation;
			flow_lightning_pixels = image->bitmap.pixels;
		} else if (!std::strcmp(operation, "lightning_disturb")) {
			require(xl_lightning_object_set_enabled(307, 0), "Cannot disturb original lightning activation");
		} else {
			require(!flow_lightning_activation.empty() && activation == flow_lightning_activation && image->bitmap.pixels == flow_lightning_pixels,
			        "Original lightning lifecycle lost activation or owned corona pixels");
			if (!std::strcmp(operation, "lightning_live")) {
				const auto *emitter = lightning_object_emitter(307);
				require(emitter && emitter->enabled && emitter->bolts.size() == 3, "Original lightning did not create its three authored bolts");
				for (unsigned i = 0; i < emitter->bolts.size(); ++i) {
					const auto *frame = lightning_object_render_frame(307, i);
					require(frame && !frame->core.vertices.empty() && !frame->glow.vertices.empty(), "Original lightning did not retain animated core/glow geometry");
				}
				const auto audio = named_audio_snapshot();
				require(lightning_audio_matches(audio), "Original lightning audio ownership differs from its source profile");
				unsigned sounds = 0;
				for (const auto &owner : audio.at("linked").at("owners"))
					if (owner.at("object") == 307 && owner.at("signature") == Objects[307].signature) ++sounds;
				const bool active = std::any_of(emitter->bolts.begin(), emitter->bolts.end(), [](const auto &bolt) { return bolt.lifetime.nodes > 0; });
				require(sounds == unsigned(active), "Original lightning loop ownership does not follow its active bolt population");
			} else if (!std::strcmp(operation, "lightning_scene")) {
				require(ConsoleObject && ConsoleObject->segnum == 362, "Original lightning scene requires the owner-segment fixed view");
				xl_lightning_scene_probe_request(307);
			} else require(!std::strcmp(operation, "lightning_verify"), "Unknown original lightning control");
		}
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
			} else if (!std::strcmp(operation, "spray_scene")) {
				require(ConsoleObject && ConsoleObject->segnum == 1127 && Objects[419].segnum == 1127 &&
				            std::find(flow_spray_owners.begin(), flow_spray_owners.end(), 419) != flow_spray_owners.end(),
				        "Spray scene requires the original owner 419 face-view pose");
				bool selected_live = false;
				for (const auto &system : particle_systems()) {
					if (!system.count()) continue;
					require(std::holds_alternative<SmokeEmitter>(system.emitter) &&
					            std::find(flow_spray_owners.begin(), flow_spray_owners.end(), system.object) != flow_spray_owners.end(),
					        "Spray scene contains a live non-spray particle system");
					selected_live |= system.object == 419;
				}
				require(selected_live, "Selected original spray owner has no live particles");
				xl_particle_scene_probe_request();
			} else if (!std::strcmp(operation, "spray_gpu")) particle_gpu_probe(image, 8, true);
			else require(!std::strcmp(operation, "spray_verify"), "Unknown spray control");
		}
	} else if (!std::strcmp(operation, "verify")) {
		require(gl_initialized && !GameArg.SysInputDemoNoRender && xl_mission_progress_state(Current_level_num) == 1 &&
		            ConsoleObject->control_type == CT_FLYING && ConsoleObject->movement_type == MT_PHYSICS,
		        "Directed destination lost headed rendering, visited state or player controls");
	} else {
		int from = 0, destination = 0;
		if (!std::strcmp(operation, "exit11")) {
			from = 1;
			destination = 11;
		}
		if (!std::strcmp(operation, "exit12")) {
			from = 11;
			destination = 12;
		}
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
		if (!std::strcmp(operation, "exit9")) {
			from = 8;
			destination = 9;
		}
		if (!std::strcmp(operation, "exit10")) {
			from = 9;
			destination = 10;
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
