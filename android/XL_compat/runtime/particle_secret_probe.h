// Derived automatic secret-save control; transition presentation is suppressed
extern "C" void EnterSecretLevel(void);
nlohmann::json secret_particle_base, secret_particle_saved;
std::vector<unsigned char> secret_particle_pixels;
unsigned secret_particle_entries = 0, secret_particle_returns = 0;

void particle_secret_probe(const char *operation)
{
	using namespace xl_compat;
	if (!std::strcmp(operation, "start")) {
		require(!Game_wind, "Derived secret control must start outside gameplay");
		char mission[] = "pfsecret";
		require(load_mission_by_name(mission), "Cannot select derived particle secret fixture");
		require(GameArg.SysUsePlayersDir, "Derived secret control requires protected player-directory saves");
		PHYSFS_delete("Players/secret.sgb");
		PHYSFS_delete("Players/secret.sgc");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		return;
	}
	require(Game_wind && Current_mission && !std::strcmp(Current_mission_filename, "pfsecret") && xl_level_extensions_active(),
	        "Derived particle secret control lost its mission");
	const auto image_matches = [&] {
		require(xl_prepared_particle_image() && xl_prepared_particle_image()->bitmap.pixels == secret_particle_pixels,
		        "Secret transition changed retained particle image pixels");
	};
	const auto verify = [&] {
		const auto &state = Current_level_num < 0 ? secret_particle_saved : secret_particle_base;
		require(!state.empty() && particle_objects_snapshot() == state, "Secret transition changed authored particle ownership/activation");
		image_matches();
	};
	if (!std::strcmp(operation, "base")) {
		require(Current_level_num == 1 && game_is_time_paused() && particle_objects_snapshot().size() == 2 && xl_prepared_particle_image(),
		        "Derived particle base requires two paused loaded owners");
		require(xl_particle_object_set_enabled(49, 0) && xl_particle_object_set_enabled(50, 1), "Cannot establish distinct base particle activation");
		secret_particle_base = particle_objects_snapshot();
		secret_particle_pixels = xl_prepared_particle_image()->bitmap.pixels;
		secret_particle_entries = secret_particle_returns = 0;
		verify();
	} else if (!std::strcmp(operation, "enter") || !std::strcmp(operation, "return")) {
		verify();
		require(!game_is_time_paused() && !GameArg.SysInputDemoNoRender, "Secret transition requires running headed gameplay");
		const bool entering = !std::strcmp(operation, "enter");
		require(Current_level_num == (entering ? 1 : -1), "Secret transition started in the wrong level");
		{
			// Avoid synchronous presentation menus reentering the automation action
			// The real automatic save/load executes; headed GPU checks follow return
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
			if (entering) EnterSecretLevel();
			else ExitSecretLevel();
		}
		require(Current_level_num == (entering ? -1 : 1), "Real secret transition did not reach its destination");
		require(particle_systems().empty(), "Secret transition retained transient particle systems");
		image_matches();
		if (entering && !secret_particle_entries) {
			const auto fresh = particle_objects_snapshot();
			require(fresh.size() == 2 && fresh[0][2].get<bool>() && !fresh[1][2].get<bool>(), "First secret visit inherited base activation");
			require(xl_particle_object_set_enabled(49, 1) && xl_particle_object_set_enabled(50, 1), "Cannot establish distinct secret activation");
			secret_particle_saved = particle_objects_snapshot();
		}
		verify();
		if (entering) ++secret_particle_entries;
		else ++secret_particle_returns;
		require(PHYSFSX_exists(entering ? "Players/secret.sgb" : "Players/secret.sgc", 0), "Secret transition omitted its automatic save file");
	} else if (!std::strcmp(operation, "gpu")) {
		verify();
		particle_gpu_probe(xl_prepared_particle_image());
	} else if (!std::strcmp(operation, "live")) {
		verify();
		unsigned live = 0;
		for (const auto &system : particle_systems()) live += unsigned(system.emitter.count);
		require(live > 0, "Secret transition did not resume particle emission");
	} else throw std::runtime_error("Unknown derived particle secret operation");
	++checks;
	report = nlohmann::json{ { "passed", true }, { "checks", checks }, { "particle_owners", 2 }, { "secret_entries", secret_particle_entries }, { "secret_returns", secret_particle_returns } }.dump();
}
