// Original-Pumo controlled qualification through the real Android level owner
nlohmann::json original_particle_earned;
std::vector<unsigned char> original_particle_pixels;
unsigned original_particle_actions = 0;
std::vector<std::pair<int, int>> original_particle_triggers;

void original_particle_verify()
{
	require(xl_level_extensions_active() && Current_mission && !std::strcmp(Current_mission_filename, "pmines") && Current_level_num == 2,
	        "Original particle control lost Pumo level 2");
	require(!original_particle_earned.empty() && xl_compat::particle_objects_snapshot() == original_particle_earned,
	        "Original particle activation or lifetime differs from earned state");
	const auto *image = xl_prepared_particle_image();
	require(image && image->bitmap.pixels == original_particle_pixels, "Original particle atlas differs from earned pixels");
	for (const auto &[index, flags] : original_particle_triggers)
		require(index < Num_triggers && Triggers[index].type == 32 && Triggers[index].flags == flags,
		        "Original fire trigger state differs from earned flags");
}

void particle_original_probe(const char *operation)
{
	using namespace xl_compat;
	if (!std::strcmp(operation, "start")) {
		require(!Game_wind, "Original fire qualification must start outside gameplay");
		char mission[] = "pmines";
		require(load_mission_by_name(mission), "Cannot select original Pumo package");
		// Original hub lifecycle has its own owner; this control starts level 2
		if (!xl_level_supported_at("mvlev1.rl2", 0, 2)) throw std::runtime_error(xl_level_rejection());
		// A synchronous diagnostic start must not reenter automation in briefing UI
		input_demo_set_skip_level_intro(1);
		StartNewGame(2);
		return;
	}
	require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "Original fire control requires singleplayer");
	if (!std::strcmp(operation, "capture")) {
		require(game_is_time_paused() && xl_level_extensions_active() && Current_level_num == 2, "Original fire capture requires paused Pumo level 2");
		original_particle_earned = particle_objects_snapshot();
		require(original_particle_earned.size() == 12 && xl_prepared_particle_image(), "Original Pumo lost authored fire owners or image");
		for (const auto &row : original_particle_earned) require(!row[2].get<bool>() && row[3].get<bool>(), "Original fire did not start disabled and alive");
		original_particle_pixels = xl_prepared_particle_image()->bitmap.pixels;
		original_particle_actions = 0;
		original_particle_triggers.clear();
		original_particle_verify();
	} else if (!std::strcmp(operation, "activate")) {
		original_particle_verify();
		require(game_is_time_paused(), "Original trigger control requires paused gameplay");
		for (int index = 0; index < Num_triggers; ++index) {
			const auto &action = Triggers[index];
			if (action.type != 32) continue;
			bool targets_fire = false;
			for (int target = 0; target < action.num_links; ++target)
				for (const auto &row : original_particle_earned)
					if (action.side[target] < 0 && action.seg[target] == row[0].get<int>()) targets_fire = true;
			if (targets_fire) {
				require(action.flags == TF_NO_MESSAGE, "Original fire action flags differ from the original inventory");
				require(xl_wall_trigger(index, Players[Player_num].objnum, 0) == 0, "Original fire activation trigger was not consumed");
				require(action.flags == TF_NO_MESSAGE, "Original reusable fire action changed its flags");
				original_particle_triggers.emplace_back(index, action.flags);
				++original_particle_actions;
			}
		}
		require(original_particle_actions == 2, "Original Pumo fire action count differs from the original inventory");
		original_particle_earned = particle_objects_snapshot();
		unsigned enabled = 0;
		for (const auto &row : original_particle_earned) enabled += row[2].get<bool>();
		require(enabled == 12, "Original Pumo trigger dispatch did not enable all twelve fire owners");
	} else if (!std::strcmp(operation, "live")) {
		original_particle_verify();
		unsigned particles = 0;
		for (const auto &system : particle_systems()) particles += system.count();
		require(particles > 0, "Original Pumo active emitters produced no live particles");
	} else if (!std::strcmp(operation, "scene")) {
		original_particle_verify();
		xl_particle_scene_probe_request();
	} else if (!std::strcmp(operation, "verify")) {
		original_particle_verify();
	} else if (!std::strcmp(operation, "gpu")) {
		original_particle_verify();
		particle_gpu_probe(xl_prepared_particle_image());
	} else if (!std::strcmp(operation, "disturb")) {
		original_particle_verify();
		for (const auto &row : original_particle_earned) require(xl_particle_object_set_enabled(row[0].get<int>(), 0), "Cannot disturb original particle activation");
		require(particle_objects_snapshot() != original_particle_earned, "Original particle disturbance did not change saved activation");
	} else if (!std::strcmp(operation, "reactivate")) {
		original_particle_verify();
		for (const auto &row : original_particle_earned) require(xl_particle_object_set_enabled(row[0].get<int>(), 0), "Cannot disable fire for authored reactivation control");
		require(particle_objects_snapshot() != original_particle_earned, "Original fire reactivation control did not disable owners");
		for (const auto &[index, flags] : original_particle_triggers) {
			require(xl_wall_trigger(index, Players[Player_num].objnum, 0) == 0, "Original reusable fire action was not handled");
			require(Triggers[index].flags == flags, "Original reusable fire action changed its flags");
		}
		original_particle_verify();
	} else if (!std::strcmp(operation, "disk_save")) {
		original_particle_verify();
		require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "Original Pumo fire", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save original Pumo fire");
	} else if (!std::strcmp(operation, "disk_load")) {
		require(!original_particle_earned.empty() && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore original Pumo fire");
		original_particle_verify();
		require(particle_systems().empty(), "Original Pumo restore retained transient fire clouds");
		require(android_open_overlay_pause_if_safe(), "Cannot pause restored original Pumo fire");
	} else if (!std::strcmp(operation, "stock")) {
		require(!xl_level_extensions_active() && particle_objects_snapshot().empty() && particle_systems().empty() && !xl_prepared_particle_image(), "Original Pumo particle owners leaked into stock");
	} else throw std::runtime_error("Unknown original particle control operation");
	++checks;
	report = nlohmann::json{ { "passed", true }, { "particle_owners", original_particle_earned.size() }, { "authored_actions", original_particle_actions }, { "checks", checks } }.dump();
}
