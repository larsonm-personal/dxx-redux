// Android automation: derived wet/dry hub with the original object-316 payload
std::string bubble_fixture_mission;
nlohmann::json bubble_saved_activation, bubble_saved_audio;
std::vector<unsigned char> bubble_saved_pixels, bubble_saved_smoke;
bool bubble_dry_bound = false;

void bubble_lifecycle_probe(const char *operation)
{
	using namespace xl_compat;
	if (!std::strcmp(operation, "start") || !std::strcmp(operation, "start_dry")) {
		require(!Game_wind, "Bubble fixture must start outside gameplay");
		bubble_fixture_mission = !std::strcmp(operation, "start") ? "pbubble" : "pbubdry";
		require(load_mission_by_name(bubble_fixture_mission.data()), "Cannot select derived bubble mission");
		if (!xl_level_supported_at("pm-hub.rl2", 0, 1)) throw std::runtime_error(xl_level_rejection());
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		report = nlohmann::json{ { "passed", true }, { "checks", checks } }.dump();
		return;
	}
	if (!std::strcmp(operation, "stock")) {
		require(!xl_level_extensions_active() && !xl_prepared_particle_image(0) && !xl_prepared_particle_image(2) && particle_systems().empty(), "Bubble resources or emitters leaked into stock");
		return;
	}
	if (!std::strcmp(operation, "load")) {
		require(!bubble_saved_pixels.empty() && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore derived bubble checkpoint");
		require(android_open_overlay_pause_if_safe(), "Cannot pause restored bubble checkpoint");
	}
	require(Game_wind && xl_level_extensions_active() && !std::strcmp(Current_mission_filename, bubble_fixture_mission.c_str()) && Current_level_num == 1, "Bubble control lost its derived mission");
	const auto *bubble = xl_prepared_particle_image(2), *smoke = xl_prepared_particle_image(0);
	require(bubble && smoke && bubble->bitmap.width == 512 && smoke->bitmap.width == 512 && !xl_prepared_particle_image(), "Bubble source images are missing or substituted");
	const auto activation = particle_objects_snapshot();
	require(activation.size() == 1 && activation[0][0] == 1, "Bubble fixture lost its authored owner");
	const auto audio = named_audio_snapshot();
	const auto &owners = audio.at("linked").at("owners");
	unsigned linked = 0;
	for (const auto &owner : owners)
		if (owner.at("object") == 1) {
			require(owner.at("external").get<bool>() && owner.at("class") == 0 && owner.at("max_volume") == F1_0 / 2 && owner.at("max_distance") == 256 * F1_0, "Bubble device sound differs from source parameters");
			++linked;
		}
	require(linked == 1 && particle_audio_matches(audio), "Bubble device lost its prepared linked sound");
	unsigned particles = 0;
	for (const auto &system : particle_systems()) {
		require(system.object == 1 && std::holds_alternative<BubbleEmitter>(system.emitter), "Bubble selected a different emitter");
		particles += system.count();
		if (!std::strcmp(operation, "scene") && system.count()) {
			const auto &emitter = std::get<BubbleEmitter>(system.emitter);
			const auto &point = emitter.slots[emitter.first].motion.position;
			con_printf(CON_NORMAL, "XL bubble scene: object=(%d,%d,%d) particle=(%d,%d,%d) eye=(%d,%d,%d) forward=(%d,%d,%d)\n", Objects[1].pos.x, Objects[1].pos.y, Objects[1].pos.z, point[0], point[1], point[2], Viewer->pos.x, Viewer->pos.y, Viewer->pos.z, Viewer->orient.fvec.x, Viewer->orient.fvec.y, Viewer->orient.fvec.z);
		}
	}
	if (!std::strcmp(operation, "live") || !std::strcmp(operation, "scene")) {
		require(activation[0][2] == true && particles > 0, "Wet bubble emitted no live particles");
		if (!std::strcmp(operation, "scene")) xl_particle_scene_probe_request();
	} else if (!std::strcmp(operation, "dry")) {
		require(bubble_fixture_mission == "pbubdry" && bubble_dry_bound && activation[0][2] == false && !particles, "Dry bubble retained particles after disable or never bound an emitter");
	} else if (!std::strcmp(operation, "disable") || !std::strcmp(operation, "enable")) {
		if (bubble_fixture_mission == "pbubdry" && !particle_systems().empty()) bubble_dry_bound = true;
		require(xl_particle_object_set_enabled(1, !std::strcmp(operation, "enable")), "Cannot change bubble activation");
	} else if (!std::strcmp(operation, "drained")) {
		require(activation[0][2] == false && !particles, "Disabled bubble did not drain");
	} else if (!std::strcmp(operation, "save")) {
		require(game_is_time_paused(), "Bubble checkpoint requires pause");
		// face_view can move the paused listener after its last normal audio sync
		xl_audio_sync_objects();
		AudioSnapshotPause pause;
		bubble_saved_activation = activation;
		bubble_saved_audio = named_audio_snapshot();
		bubble_saved_pixels = bubble->bitmap.pixels;
		bubble_saved_smoke = smoke->bitmap.pixels;
		require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "Derived XL bubble", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save derived bubble checkpoint");
	} else if (!std::strcmp(operation, "verify") || !std::strcmp(operation, "load")) {
		require(game_is_time_paused() && !bubble_saved_pixels.empty() && bubble->bitmap.pixels == bubble_saved_pixels && smoke->bitmap.pixels == bubble_saved_smoke && activation == bubble_saved_activation, "Bubble resource or activation changed after lifecycle");
		if (!std::strcmp(operation, "load")) {
			if (audio.at("voices") != bubble_saved_audio.at("voices") || audio.at("linked") != bubble_saved_audio.at("linked")) {
				for (const char *key : { "voices", "linked" }) {
					const auto &before = !std::strcmp(key, "linked") ? bubble_saved_audio.at(key).at("owners") : bubble_saved_audio.at(key);
					const auto &after = !std::strcmp(key, "linked") ? audio.at(key).at("owners") : audio.at(key);
					con_printf(CON_NORMAL, "XL bubble restore %s rows: %u -> %u\n", key, unsigned(before.size()), unsigned(after.size()));
					for (unsigned i = 0; i < (std::max) (before.size(), after.size()); ++i) {
						const auto old_row = i < before.size() ? before[i] : nlohmann::json();
						const auto new_row = i < after.size() ? after[i] : nlohmann::json();
						if (old_row != new_row) {
							con_printf(CON_NORMAL, "XL bubble restore %s[%u] before=%s\n", key, i, old_row.dump().c_str());
							con_printf(CON_NORMAL, "XL bubble restore %s[%u] after=%s\n", key, i, new_row.dump().c_str());
						}
					}
				}
			}
			require(audio.at("voices") == bubble_saved_audio.at("voices") && audio.at("linked") == bubble_saved_audio.at("linked"), "Bubble checkpoint changed linked playback phase or owner");
			require(particle_systems().empty(), "Bubble checkpoint retained transient clouds");
		}
	} else throw std::runtime_error("Unknown bubble lifecycle operation");
	++checks;
	report = nlohmann::json{ { "passed", true }, { "checks", checks } }.dump();
}
