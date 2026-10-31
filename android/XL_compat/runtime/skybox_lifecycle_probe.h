// Derived hub: original geometry/POG, explicitly substituted smoke/outdoors bytes
xl_compat::Inventory skybox_earned_level;
std::vector<unsigned char> skybox_earned_pixels;
std::string skybox_fixture_mission = "pskybox";

void skybox_lifecycle_probe(const char *operation)
{
	if (!std::strcmp(operation, "start") || !std::strcmp(operation, "start_smoke")) {
		require(!Game_wind, "Skybox lifecycle must start outside gameplay");
		skybox_fixture_mission = !std::strcmp(operation, "start_smoke") ? "psmoke" : "pskybox";
		require(load_mission_by_name(skybox_fixture_mission.data()), "Cannot select derived skybox fixture");
		if (!xl_level_supported_at("pm-hub.rl2", 0, 1)) throw std::runtime_error(xl_level_rejection());
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		return;
	}
	if (!std::strcmp(operation, "stock")) {
		require(!xl_level_extensions_active(), "Skybox fixture retained XL state in stock");
		require(!xl_prepared_particle_image(0) && !xl_prepared_particle_image() && xl_compat::particle_systems().empty(), "Particle resources or clouds leaked into stock");
		for (int i = 0; i < 439; ++i) require(!xl_skybox_segment(i), "Skybox membership leaked into stock");
		return;
	}
	if (!std::strcmp(operation, "load")) {
		require(!skybox_earned_pixels.empty() && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore derived skybox save");
		require(android_open_overlay_pause_if_safe(), "Cannot pause restored skybox fixture");
	}
	const bool capture = !std::strcmp(operation, "capture");
	const bool disturb = !std::strcmp(operation, "disturb");
	require(capture || disturb || !std::strcmp(operation, "verify") || !std::strcmp(operation, "load"), "Unknown skybox lifecycle operation");
	require(Game_wind && game_is_time_paused() && xl_level_extensions_active() && !std::strcmp(Current_mission_filename, skybox_fixture_mission.c_str()) && Current_level_num == 1,
	        "Skybox lifecycle lost its paused derived hub");
	if (capture) {
		auto *file = PHYSFSX_openReadBuffered(Gamesave_current_filename);
		require(file && PHYSFS_fileLength(file) > 0 && PHYSFS_fileLength(file) <= 64 * 1024 * 1024, "Skybox fixture geometry is missing or oversized");
		xl_compat::Bytes bytes(std::size_t(PHYSFS_fileLength(file)));
		const auto count = PHYSFS_readBytes(file, bytes.data(), bytes.size());
		PHYSFS_close(file);
		require(count == PHYSFS_sint64(bytes.size()), "Skybox fixture read was truncated");
		skybox_earned_level = xl_compat::inspect_level(bytes);
	}
	require(skybox_earned_level.segments.size() == std::size_t(Num_segments) && Num_segments == 439, "Skybox geometry count differs after lifecycle");
	unsigned skyboxes = 0;
	for (int i = 0; i < Num_segments; ++i) {
		const bool expected = skybox_earned_level.segments[i].function == 10;
		require(bool(xl_skybox_segment(i)) == expected, "Normal lifecycle did not rebind authored skybox membership");
		skyboxes += expected;
	}
	require(skyboxes == 295 && !xl_skybox_rendering(), "Skybox lifecycle lost authored owners or leaked draw state");
	const auto pixels = skybox_probe_pixels();
	if (capture) skybox_earned_pixels = pixels;
	else if (disturb) require(pixels != skybox_earned_pixels, "Authored skybox fixed-view witness did not distinguish another textured side");
	else require(pixels == skybox_earned_pixels, "Authored skybox frame changed after resource/save lifecycle");
	con_printf(CON_NORMAL, "XL authored skybox lifecycle: 295 memberships and fixed-view pixels verified (%s)\n", operation);
}
