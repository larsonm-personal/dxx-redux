// Real scoped resources and SDL output on repeated normal/secret filenames
#include "mission_prepare.h"
static void audio_incoming_source_controls(const std::filesystem::path &root)
{
#ifdef USE_SDLMIXER
	using namespace named_audio_control;
	const auto directory = root / "incoming-audio";
	std::filesystem::create_directories(directory);
	for (const char *extension : { ".mn2", ".hog" })
		std::filesystem::copy_file(root / "missions" / (std::string("xscope") + extension), directory / (std::string("xscope") + extension), std::filesystem::copy_options::overwrite_existing);
	std::filesystem::create_directories(directory / "mods/xscope");
	std::filesystem::copy(root / "mods/xscope/sounds", directory / "mods/xscope/sounds", std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
	const auto changed = directory / "mods/xscope/sounds/level02/scoped.wav";
	std::filesystem::copy_file(root / "mods/xscope/sounds/level01/scoped.wav", changed, std::filesystem::copy_options::overwrite_existing);
	const auto original_level = [] {
		std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFS_openRead("scope23.rl2"), PHYSFS_close);
		return xl_compat::inspect_level(xl_retain_resource(file.get()));
	}();
	XlPreparedResources outgoing("scope23.rl2", &original_level, 2);
	const auto before = snapshot();
	const auto definitions = gameplay_definitions();
	const auto live = Current_mission;
	const auto game_time = GameTime64;
	uint32_t sim;
	d_rand_get_state(&sim);
	const auto calls = d_rand_get_call_count();
	const std::string origin = PHYSFS_getRealDir("scope23.rl2");
	const auto physical = std::filesystem::absolute(directory).string();
	require(PHYSFS_mount(physical.c_str(), "__incoming_audio", 1), "Cannot mount incoming audio descriptor");
	auto *mission = mission_prepare_from_current_dir("__incoming_audio/xscope.mn2");
	struct Release {
		mission_preparation **mission;
		~Release()
		{
			mission_discard_prepared(mission);
		}
	} release{ &mission };
	require(mission && PHYSFS_unmount(physical.c_str()), "Cannot retain isolated incoming audio descriptor");
	{
		auto discarded = XlPreparedResources::prepare_level(mission, 2);
		require(discarded->level_inventory().level_version == original_level.level_version, "Incoming audio lost target level bytes");
	}
	auto incoming = XlPreparedResources::prepare_level(mission, 2);
	auto stale_publication = XlPreparedResources::prepare_level(mission, 2);
	XlPreparedResources moved(std::move(*incoming));
	incoming.reset();
	write_bytes(changed.string().c_str(), { 'b', 'a', 'd' });
	bool rejected = false;
	try {
		auto invalid = XlPreparedResources::prepare_level(mission, 2);
	} catch (const std::runtime_error &) {
		rejected = true;
	}
	require(rejected && mission_discard_prepared(&mission), "Malformed incoming sound was accepted or retained busy source handles");
	uint32_t after_sim;
	d_rand_get_state(&after_sim);
	require(snapshot() == before && Current_mission == live && GameTime64 == game_time && definitions == gameplay_definitions() &&
	            after_sim == sim && d_rand_get_call_count() == calls && origin == PHYSFS_getRealDir("scope23.rl2"),
	        "Incoming audio preparation, move, retry or discard changed the live game");
	xl_named_audio_backend_close();
	Mix_CloseAudio();
	require(Mix_OpenAudio(48000, AUDIO_S16SYS, 2, 1024) == 0, "Cannot reopen incoming audio control at 48 kHz");
	SDL_PauseAudio(1);
	require(xl_named_audio_backend_open(48000, AUDIO_S16SYS, 2) && xl_compat::restore_named_audio(before, true), "Cannot retain outgoing voices after rate change");
	const auto changed_rate = snapshot();
	rejected = false;
	try {
		xl_set_prepared_resources("__rejected_audio__.rl2", std::move(stale_publication));
	} catch (const std::runtime_error &) {
		rejected = true;
	}
	require(rejected && snapshot() == changed_rate && !xl_install_prepared_hxm("__rejected_audio__.rl2"),
	        "Stale audio handoff replaced the published level owner or changed live voices");
	rejected = false;
	try {
		moved.install_messages();
	} catch (const std::runtime_error &) {
		rejected = true;
	}
	require(rejected && snapshot() == changed_rate, "Stale prepared audio rate changed live voices before rejection");
	xl_named_audio_backend_close();
	Mix_CloseAudio();
	require(Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) == 0, "Cannot recover incoming audio control output");
	SDL_PauseAudio(1);
	require(xl_named_audio_backend_open(44100, AUDIO_S16SYS, 2) && xl_compat::restore_named_audio(before, true), "Cannot recover outgoing audio before publication");
	moved.install_messages();
	moved.install_messages();
	require(snapshot().at("source") != before.at("source") && Current_mission == live,
	        "Incoming audio publication reused outgoing equal-name sound sources");
	begin_capture();
	xl_named_audio_pause(0);
	require(xl_named_audio_action(&Triggers[1], 1), "Incoming scoped audio did not accept its retained action");
	run_output(4096);
	for (std::size_t i = 0; i < captured_samples; ++i)
		require(captured[i] == std::int16_t(float(i % 2 ? -7500 : 2500) * (127.0f / 255.0f)),
		        "Incoming audio publication reread malformed files or played outgoing PCM");
	outgoing.install_messages();
	require(xl_compat::restore_named_audio(before, true) && snapshot() == before,
	        "Outgoing audio did not resume its retained bank and saved voices");
	rejected = false;
	try {
		moved.install_messages();
	} catch (const std::runtime_error &) {
		rejected = true;
	}
	require(rejected && snapshot() == before, "Retired prepared audio owner replaced live voices");
	std::cout << "Prepared audio publication: real output-rate rejection, active repeat and retired-owner rejection preserve live voices\n";
	std::cout << "Incoming level audio: isolated equal-name sources, malformed retry, move/discard and retained actual PCM publication passed\n";
#else
	(void) root;
#endif
}
static int audio_scope_test(int argc, char **argv)
{
#ifdef USE_SDLMIXER
	using namespace named_audio_control;
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		require(load_base_mission("xscope", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		SDL_PauseAudio(1);
		StartNewGame(1);
		digi_set_digi_volume(32768);
		Game_suspended = SUSP_ROBOTS;
		if (!std::strcmp(find_arg_value(argc, argv, "-audio-scope-test"), "banks")) {
			xl_bank_control::run(root);
			Mix_SetPostMix(nullptr, nullptr);
			xl_named_audio_clear();
			SDL_PauseAudio(0);
			return 0;
		}
		std::map<int, nlohmann::json> saved;
		std::uint64_t checked = 0;
		bool first = true;
		for (int ordinal : { 1, 2, -1, 3, 1 }) {
			if (!first) LoadLevel(ordinal, 0);
			first = false;
			SDL_PauseAudio(1);
			GameTime64 = 10 * F1_0;
			const char *filename = ordinal == 3 ? "scope27.rl2" : "scope23.rl2";
			require(Current_level_num == ordinal && xl_source_level_version() == (ordinal == 3 ? 27 : 23), "Scoped audio lost the incoming level ordinal or layout");
			const std::string folder = ordinal == 1 ? "level01/" : ordinal == 2 ? "level02/"
			                                                   : ordinal == -1  ? "slevel01/"
			                                                                    : "";
			const auto diagnostic = xl_compat::named_audio_diagnostics();
			require(diagnostic.at("files") == nlohmann::json::array({ "shared.wav", "mods/xscope/sounds/" + folder + "scoped.wav", "mods/xscope/sounds/mission.wav", "tiny.wav" }), "Scoped audio selected the wrong priority or folder");
			const auto incoming = snapshot();
			const auto &incoming_owners = incoming.at("linked").at("owners");
			require(incoming_owners.size() == 1 && incoming_owners[0].at("segment") == 74 && incoming_owners[0].at("side") == 2 &&
			            incoming_owners[0].at("sound") == 93 && incoming.at("voices").size() <= 1 && gamesave_level_data_warning_count() == 0,
			        "Incoming scope retained unrelated owners or repaired inert links");
			for (const auto &voice : incoming.at("voices"))
				require(voice[4] == 0 && voice[2] == incoming_owners[0].at("sample"), "Incoming scope retained outgoing named playback");
			for (const auto &event : incoming.at("events")) require(event[1] == -1, "Incoming scope retained an outgoing named event");
			digi_stop_digi_sounds();
			const auto initial = snapshot();
			require(xl_level_supported_at(filename, 0, ordinal), xl_level_rejection());
			require(snapshot() == initial, "Successful scoped preflight changed playback");
			if (ordinal != 3) {
				require(!xl_level_supported(filename, 0) && std::strstr(xl_level_rejection(), "explicit incoming level number"), "Ambiguous filename silently chose a level sound folder");
				require(!xl_level_supported_at(filename, 0, 3) && std::strstr(xl_level_rejection(), "ordinal does not match"), "Mismatched incoming ordinal accepted");
			}
			for (int action : { 0, 1, 2 }) {
				require(xl_compat::restore_named_audio(initial, true), "Cannot reset scoped output control");
				begin_capture();
				xl_named_audio_pause(0);
				require(check_trigger_sub(action, Player_num, 1) == 0 && snapshot().at("voices").size() == 1, "Scoped wall action did not start a voice");
				run_output(4096);
				const int left = action == 0 ? 1000 : action == 2 ? 1500
				                                  : ordinal == 1  ? 2500
				                                  : ordinal == 2  ? -3000
				                                  : ordinal == -1 ? 11000
				                                                  : 8000;
				const int right = action == 0 ? -2000 : action == 2 ? -1500
				                                    : ordinal == 1  ? -7500
				                                    : ordinal == 2  ? 9000
				                                    : ordinal == -1 ? -5000
				                                                    : -4000;
				for (std::size_t i = 0; i < captured_samples; ++i)
					require(captured[i] == std::int16_t(float(i % 2 ? right : left) * (127.0f / 255.0f)), "Scoped actual PCM differs from its authored source");
				checked += captured_samples;
			}
			xl_named_audio_pause(1);
			const auto earned = snapshot();
			if (saved.count(ordinal)) require(saved.at(ordinal).at("source") == earned.at("source"), "Returning to a scope changed stable resource identity");
			for (const auto &[previous, state] : saved)
				if (previous != ordinal) require(state.at("source") != earned.at("source") && !xl_compat::restore_named_audio(state, true) && snapshot() == earned, "Another scope's saved voices were accepted or changed state");
			saved[ordinal] = earned;
			const auto scope_path = root / "mods" / "xscope" / "sounds" / folder / "scoped.wav";
			std::ifstream wave_file(scope_path, std::ios::binary);
			const xl_compat::Bytes original{ std::istreambuf_iterator<char>(wave_file), std::istreambuf_iterator<char>() };
			wave_file.close();
			require(original.size() > 44, "Missing scoped fixture WAV");
			write_bytes(scope_path.string().c_str(), xl_compat::Bytes{ 'b', 'a', 'd' });
			const bool malformed = !xl_level_supported_at(filename, 0, ordinal) && std::strstr(xl_level_rejection(), "Invalid XL named sound");
			write_bytes(scope_path.string().c_str(), original);
			require(malformed && snapshot() == earned && xl_level_supported_at(filename, 0, ordinal), "Malformed preferred sound fell back or changed live playback");
			const auto shadow = root / "foreign";
			const auto shadow_file = shadow / "mods" / "xscope" / "sounds" / folder / "scoped.wav";
			std::filesystem::create_directories(shadow_file.parent_path());
			write_bytes(shadow_file.string().c_str(), original);
			std::filesystem::create_directories(shadow / "missions");
			std::filesystem::copy_file(root / "missions" / "xscope.mn2", shadow / "missions" / "xscope.mn2", std::filesystem::copy_options::overwrite_existing);
			require(PHYSFS_addToSearchPath(shadow.string().c_str(), 0), "Cannot mount foreign sound control");
			const bool foreign = !xl_level_supported_at(filename, 0, ordinal) && std::strstr(xl_level_rejection(), "shadowed outside the selected mission sound folders");
			require(PHYSFS_removeFromSearchPath(shadow.string().c_str()), "Cannot release foreign sound control");
			require(foreign && snapshot() == earned && xl_level_supported_at(filename, 0, ordinal), "Foreign sound was accepted or damaged current playback");
			const auto mission_sound = root / "mods" / "xscope" / "sounds" / "mission.wav";
			const auto withheld = mission_sound.string() + ".withheld";
			std::filesystem::rename(mission_sound, withheld);
			const bool missing = !xl_level_supported_at(filename, 0, ordinal) && std::strstr(xl_level_rejection(), "Missing XL sound mission.wav") && std::strstr(xl_level_rejection(), "global addon");
			std::filesystem::rename(withheld, mission_sound);
			require(missing && snapshot() == earned && xl_level_supported_at(filename, 0, ordinal), "Missing sound rejection omitted the searched scopes or changed playback");
			// Capture normal level two, whose filename is also used by level one
			if (ordinal == 2) {
				char save[] = "scope-earned.sav", description[] = "XL level two sound scope";
				stop_time();
				require(state_save_all_sub(save, description), "Cannot save scoped audio to disk");
				require(!game_is_time_paused(), "Scoped audio save unbalanced the simulation pause");
			}
		}
		require(saved.size() == 4, "Scoped audio omitted a normal, secret or fallback context");
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Scoped audio stock transition failed");
		require(snapshot().at("source") == "" && snapshot().at("voices").empty(), "Scoped audio leaked into Redux");
		char save[] = "scope-earned.sav";
		require(state_restore_all_sub(save, 0) && Current_level_num == 2 && snapshot() == saved.at(2), "Disk restore chose a stale or ambiguous scoped sound folder");
		begin_capture();
		xl_named_audio_pause(0);
		run_output(4096);
		for (std::size_t i = 0; i < captured_samples; ++i)
			require(captured[i] == std::int16_t(float(i % 2 ? -1500 : 1500) * (127.0f / 255.0f)), "Scoped disk restore did not produce the saved PCM");
		checked += captured_samples;
		classic_audio_controls(root);
		audio_incoming_source_controls(root);
		Mix_SetPostMix(nullptr, nullptr);
		xl_named_audio_clear();
		SDL_PauseAudio(0);
		std::cout << "XL sound scope runtime passed: four contexts, repeated normal/secret names, priority, atomic rejections, disk-through-stock and " << checked << " actual PCM samples\n";
		return 0;
	} catch (const std::exception &error) {
		SDL_PauseAudio(1);
		Mix_SetPostMix(nullptr, nullptr);
		std::cerr << "XL sound scope runtime failed: " << error.what() << '\n';
		return 1;
	}
#else
	(void) argc;
	(void) argv;
	return 1;
#endif
}
