// Controlled events and real SDL output; no autonomous level navigation
#include "audio_pressure_test.h"
#include "../named_sound.h"
namespace named_audio_control
{
#ifdef USE_SDLMIXER
std::array<std::int16_t, 262144> captured{};
std::size_t captured_samples = 0;
bool overflow = false;
void capture(void *, Uint8 *stream, int bytes)
{
	if (bytes < 0 || bytes % 4 || std::size_t(bytes / 2) > captured.size() - captured_samples) {
		overflow = true;
		return;
	}
	std::memcpy(captured.data() + captured_samples, stream, std::size_t(bytes));
	captured_samples += std::size_t(bytes) / 2;
}
void begin_capture()
{
	SDL_PauseAudio(1);
	digi_stop_all_channels();
	captured_samples = 0;
	overflow = false;
	captured.fill(0);
	Mix_SetPostMix(capture, nullptr);
}
void run_output(unsigned frames)
{
	SDL_PauseAudio(0);
	const auto start = SDL_GetTicks();
	for (;;) {
		SDL_LockAudio();
		const bool done = captured_samples >= std::size_t(frames) * 2;
		SDL_UnlockAudio();
		if (done) break;
		require(SDL_GetTicks() - start < 5000, "Named audio actual output timed out");
		SDL_Delay(1);
	}
	SDL_PauseAudio(1);
	require(!overflow, "Named audio actual output overflowed");
}
#endif
nlohmann::json snapshot()
{
	return xl_compat::named_audio_snapshot();
}
nlohmann::json event(int owner)
{
	const auto state = snapshot();
	for (const auto &row : state.at("events"))
		if (row[0] == owner) return row;
	throw std::runtime_error("Missing named audio event owner");
}
std::uint64_t starts()
{
	return xl_compat::named_audio_diagnostics().at("starts").get<std::uint64_t>();
}
void bad_states(const std::filesystem::path &root)
{
	auto *file = PHYSFS_openWrite("audio-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot capture named audio trailer");
	const auto user = root / "preview-user";
	std::ifstream input(user / "audio-state.bin", std::ios::binary);
	const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	const auto before = snapshot();
	const auto lights = light_snapshot();
	require(!before.at("voices").empty(), "Malformed audio controls need a live saved voice");
	for (unsigned variant = 0; variant < 33; ++variant) {
		auto block = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
		auto &audio = block["audio"];
		auto &voice = audio["voices"][0];
		if (variant == 0) audio["source"] = "foreign";
		if (variant == 1) audio["rate"] = 0;
		if (variant == 2) audio["rate"] = 192001;
		if (variant == 3) audio["next"] = 0;
		if (variant == 4) audio["next"] = UINT64_MAX;
		if (variant == 5) audio["events"][0][1] = -2;
		if (variant == 6) audio["events"][0][1] = UINT64_MAX;
		if (variant == 7) audio["events"][0][2] = 1;
		if (variant == 8) audio["events"][0][4] = 1;
		if (variant == 9) audio["events"][0][5] = 1;
		if (variant == 10) audio["events"][1][0] = audio["events"][0][0];
		if (variant == 11) voice[0] = 128;
		if (variant == 12) voice[1] = audio["next"];
		if (variant == 13) voice[2] = 99;
		if (variant == 14) voice[3] = -1;
		if (variant == 15) voice[3] = INT32_MAX;
		if (variant == 16) voice[4] = 508;
		if (variant == 17) voice[5] = 0;
		if (variant == 18) voice[5] = 100000;
		if (variant == 19) audio["voices"].push_back(voice);
		if (variant == 20) audio["rate"] = 44100.5;
		if (variant == 21) audio["extra"] = 0;
		if (variant == 22) audio["events"].erase(0);
		if (variant == 23) block.erase("audio");
		if (variant == 24) audio["cursor"] = -1;
		if (variant == 25) audio["cursor"] = 128;
		if (variant == 26) audio["cursor"] = 0.5;
		if (variant == 27) audio.erase("cursor");
		if (variant == 28) voice[6] = -1;
		if (variant == 29) voice[6] = INT32_MAX;
		if (variant == 30) voice[6] = 8192.5;
		if (variant == 31) voice.erase(6);
		if (variant == 32) voice.push_back(0);
		const auto text = block.dump();
		xl_compat::Bytes bad(encoded.begin(), encoded.begin() + 16);
		bad.insert(bad.end(), text.begin(), text.end());
		std::uint32_t sum = 2166136261u;
		for (unsigned char byte : text) sum = (sum ^ byte) * 16777619u;
		for (unsigned b = 0; b < 4; ++b) {
			bad[8 + b] = static_cast<unsigned char>(text.size() >> (8 * b));
			bad[12 + b] = static_cast<unsigned char>(sum >> (8 * b));
		}
		write_bytes((user / "audio-bad.bin").string().c_str(), bad);
		file = PHYSFS_openRead("audio-bad.bin");
		require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file), "Malformed named audio trailer was accepted");
		require(snapshot() == before && same_light_snapshot(lights, light_snapshot()), "Rejected audio trailer partially changed runtime state");
	}
	file = PHYSFS_openRead("audio-state.bin");
	require(file && xl_wall_trigger_read(file) && PHYSFS_close(file) && snapshot() == before, "Valid named audio trailer did not recover exactly");
	std::cout << "Thirty-three malformed named audio trailers rejected atomically; valid playheads, priorities and cursor recovered\n";
}
} // namespace named_audio_control

// Component audit of unchanged mission resources, without admitting its levels
static int named_audio_original(int argc, char **argv)
{
#ifdef USE_SDLMIXER
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission(find_arg_value(argc, argv, "-named-audio-original"), error, sizeof(error)), error);
		const auto expected = [&](const char *name) { const char *value = find_arg_value(argc, argv, name); require(value && std::atoi(value) > 0, "Original audio audit requires exact positive coverage counts"); return std::atoi(value); };
		const int expected_levels = expected("-audio-levels"), expected_actions = expected("-audio-actions"), expected_waves = expected("-audio-waves");
		const int expected_ambient = find_arg_value(argc, argv, "-audio-ambient") ? expected("-audio-ambient") : 0;
		const int expected_ambient_waves = expected_ambient ? expected("-audio-ambient-waves") : 0;
		for (int rate : { 44100, 48000 }) {
			xl_named_audio_backend_close();
			Mix_CloseAudio();
			require(Mix_OpenAudio(rate, AUDIO_S16SYS, 2, 1024) == 0, "Cannot open original resource output profile");
			SDL_PauseAudio(1);
			require(xl_named_audio_backend_open(rate, AUDIO_S16SYS, 2), "Cannot initialize original resource output profile");
			int levels = 0, actions = 0, ambient = 0;
			std::set<std::string> names;
			nlohmann::json ambient_files = nlohmann::json::object();
			for (int index = 0; index < Last_level - Last_secret_level; ++index) {
				const char *name = index < Last_level ? Level_names[index] : Secret_level_names[index - Last_level];
				const int ordinal = index < Last_level ? index + 1 : -(index - Last_level + 1);
				std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFS_openRead(name), PHYSFS_close);
				const auto length = file ? PHYSFS_fileLength(file.get()) : -1;
				require(length > 0 && length < 128 * 1024 * 1024, "Original level exceeds the resource audit bound");
				xl_compat::Bytes bytes(static_cast<std::size_t>(length));
				require(PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) == length, "Cannot read original audio level");
				const auto source = xl_compat::inspect_level(bytes);
				const auto count = std::count_if(source.triggers.begin(), source.triggers.end(), [](const auto &action) { return action.type == 30; }) + std::count_if(source.object_trigger_records.begin(), source.object_trigger_records.end(), [](const auto &record) { return record.action.type == 30; });
				xl_compat::install_named_audio(xl_compat::prepare_named_audio(name, source, ordinal));
				const auto prepared = xl_compat::named_audio_diagnostics();
				require(prepared.at("actions") == count && prepared.at("voices").empty() && prepared.at("rate") == rate, "Original production audio preparation lost authored actions or started playback");
				for (const auto &filename : prepared.at("named_files")) names.insert(filename.get<std::string>());
				actions += int(count);
				if (expected_ambient) {
					for (const auto &object : source.objects) {
						if (object.type != 20 || object.id != 2 || object.render != 13) continue;
						require(object.render_data.size() == 42, "Original ambient resource has an unexpected payload");
						std::string sound;
						for (unsigned i = 0; i < 39 && object.render_data[i]; ++i) {
							const int byte = object.render_data[i];
							require(byte >= 32 && byte < 127, "Original ambient filename is not ASCII");
							sound += char(byte >= 'A' && byte <= 'Z' ? byte + 'a' - 'A' : byte);
						}
						const auto resource = xl_compat::load_audio_resource(sound + ".wav", name, ordinal);
						const auto &binding = prepared.at("external_resources").at(xl_compat::named_sound_filename(sound + ".wav"));
						require(binding.at("path") == resource.path && binding.at("sample").get<unsigned>() < prepared.at("resources").get<unsigned>(), "Original ambient resource was not installed in its prepared bank");
						const auto pcm = xl_compat::decode_wave_channels(reinterpret_cast<const std::uint8_t *>(resource.bytes.data()), resource.bytes.size());
						xl_compat::Bytes samples;
						samples.reserve(pcm.samples.size() * 2);
						for (const auto value : pcm.samples) {
							samples.push_back(static_cast<std::uint8_t>(value));
							samples.push_back(static_cast<std::uint8_t>(std::uint16_t(value) >> 8));
						}
						ambient_files[resource.path] = { { "sha256", picosha2::hash256_hex_string(resource.bytes) }, { "pcm_sha256", picosha2::hash256_hex_string(samples) }, { "rate", pcm.rate }, { "channels", pcm.source_channels }, { "frames", pcm.samples.size() / pcm.source_channels } };
						++ambient;
					}
				}
				++levels;
			}
			require(levels == expected_levels && actions == expected_actions && names.size() == unsigned(expected_waves), "Original production audio coverage differs");
			std::cout << "Original production audio preparation: " << rate << " Hz, " << levels << " levels, " << actions << " actions, " << names.size() << " owned WAVs\n";
			if (expected_ambient) {
				require(ambient == expected_ambient && ambient_files.size() == unsigned(expected_ambient_waves), "Original ambient resource coverage differs");
				const auto receipt = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / ("ambient-resources-" + std::to_string(rate) + ".json");
				std::ofstream output(receipt, std::ios::binary);
				output << ambient_files.dump(2) << '\n';
				require(bool(output), "Cannot record original ambient resource receipt");
				std::cout << "Original ambient resources: " << ambient << " objects, " << ambient_files.size() << " owned scoped WAVs; spatial runtime remains gated\n";
			}
		}
		xl_named_audio_clear();
		std::cout << "Original audio resources prepared; level admission, audible original events and campaign remain unqualified\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "Original named audio preparation failed: " << error.what() << '\n';
		return 1;
	}
#else
	(void) argc;
	(void) argv;
	return 1;
#endif
}

#ifdef USE_SDLMIXER
#include "shared_audio_test.h"
#include "audio_creation_test.h"
#include "looping_audio_test.h"
#include "seismic_audio_test.h"
#include "gameplay_dispatch_test.h"
#include "combat_audio_test.h"
#include "afterburner_audio_test.h"
#include "actor_audio_test.h"
#include "surface_audio_test.h"
#include "audio_routes_test.h"
#include "external_audio_test.h"
#include "ambient_objects_test.h"
#include "../ambient_sound.h"
#include "../tests/ambient_audio_receipt.h"
#include "gameplay_audio_test.h"
#include "linked_audio_test.h"
#endif
static int named_audio_test(int argc, char **argv)
{
#ifdef USE_SDLMIXER
	using namespace named_audio_control;
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		if (GameArg.SndNoSound || !xl_compat::named_audio_diagnostics().at("backend_ready").get<bool>())
			throw std::runtime_error("Named audio controls require the real SDL_mixer output; nosound=" + std::to_string(GameArg.SndNoSound) + "; " + xl_compat::named_audio_diagnostics().dump());
		const char *selected = find_arg_value(argc, argv, "-audio-level");
		require(selected && (!std::strcmp(selected, "1") || !std::strcmp(selected, "2")), "Named audio controls require -audio-level 1 or 2");
		const int ordinal = std::atoi(selected);
		if (!std::strcmp(find_arg_value(argc, argv, "-named-audio-test"), "ambient")) {
			require(load_base_mission("xsound", error, sizeof(error)), error);
			std::strcpy(Players[Player_num].callsign, "XLTest");
			input_demo_set_skip_level_intro(1);
			StartNewGame(ordinal + 2);
			SDL_PauseAudio(1);
			std::ofstream activation(root / "ambient-activation.txt");
			activation << "XL_AMBIENT_ACTIVATION 1\n";
			unsigned activation_cases = 0;
			xl_ambient_objects_control::run([] { xl_linked_audio_control::output(); }, [&] {
				using namespace xl_compat;
				xl_ambient_audio_receipt::Case receipt;
				const auto source = xl_ambient_objects_control::source_level();
				for (unsigned i = 0; i < source.objects.size(); ++i) {
					const auto &object = source.objects[i];
					if (object.render != 13) continue;
					const auto sound = decode_ambient_sound(object);
					receipt.objects.push_back({ int(i), object.reader_id, sound.volume, int(sound.enabled), sound.filename });
				}
				const auto resources = named_audio_diagnostics().at("external_resources");
				bool objects_started = false;
				for (unsigned i = 0; i < audio_runtime::owners.count; ++i) {
					const auto &owner = audio_runtime::owners.entries[i];
					if (owner.object < 0) {
						require(!objects_started, "Ambient initialization moved a surface owner after effect objects");
						continue;
					}
					objects_started = true;
					std::string filename;
					for (auto it = resources.begin(); it != resources.end(); ++it)
						if (it.value().at("sample") == owner.sample) filename = it.key();
					require(!filename.empty(), "Ambient owner sample has no owned filename");
					receipt.owners.push_back({ { owner.object, owner.sound, owner.sound_class, owner.max_volume, owner.max_distance,
					                            int(owner.flags), owner.loop_start, owner.loop_end, owner.decay, int(owner.external), int(owner.ambient) }, filename });
				}
				std::ofstream out(root / "ambient-audio.txt");
				xl_ambient_audio_receipt::write(out, receipt);
				require(bool(out), "Cannot write actual ambient initializer receipt"); }, [&](const auto &receipt) {
				activation << "case " << activation_cases++ << '\n';
				xl_ambient_activation_receipt::write(activation, receipt); });
			activation << "end " << activation_cases << '\n';
			require(bool(activation) && activation_cases == 6, "Cannot record complete ambient activation coverage");
			Mix_SetPostMix(nullptr, nullptr);
			std::cout << "Loaded ambient objects: activation, target order, real PCM, collision, lifetime and trailer restore passed\n";
			return 0;
		}
		{
			SDL_PauseAudio(1);
			require(load_base_mission("xsound", error, sizeof(error)), error);
			std::strcpy(Players[Player_num].callsign, "XLTest");
			input_demo_set_skip_level_intro(1);
			StartNewGame(ordinal);
			require(!game_is_time_paused(), "Fresh headless audio fixture must begin with an unpaused game clock");
			for (int volume : { 0, 255, 256, 4096, 8192, 12288, 16384, 20480, 24576, 28672, 32768 }) {
				digi_set_digi_volume(volume);
				const auto diagnostics = xl_compat::named_audio_diagnostics();
				require(diagnostics.at("effects_volume") == volume && diagnostics.at("output_volume") == 128, "Named audio channel gain or platform headroom differs");
			}
			digi_set_digi_volume(32768);
			Game_suspended = SUSP_ROBOTS;
			GameTime64 = 10 * F1_0;
			require(xl_source_level_version() == (ordinal == 1 ? 23 : 27) && Num_triggers == 10 && xl_object_trigger_count() == 2, "Named audio fixture layout did not load");
			require(gamesave_level_data_warning_count() == 0, "Inert sound links produced a level repair warning");
			const auto startup = snapshot();
			require(startup.at("linked").at("owners").size() == 1 && startup.at("voices").size() == 1 &&
			            startup.at("linked").at("owners")[0].at("segment") == 74 && startup.at("linked").at("owners")[0].at("side") == 2 &&
			            startup.at("linked").at("owners")[0].at("sound") == 93,
			        "Derived fixture ambient source did not enter the XL registry at startup");
			xl_linked_audio_control::output();
			// Isolate authored trigger controls from the separately checked startup source
			digi_stop_digi_sounds();
			const auto initial = snapshot();
			require(initial.at("events").size() == 11 && initial.at("voices").empty() && initial.at("source").get<std::string>().size() == 64, ("Named audio resources did not install: " + initial.dump()).c_str());
			std::array<trigger, 10> authored;
			std::copy(Triggers, Triggers + 10, authored.begin());
			const auto reset = [&] {
				SDL_PauseAudio(1);
				require(xl_compat::restore_named_audio(initial, true), "Cannot reset controlled audio state");
				std::copy(authored.begin(), authored.end(), Triggers);
				xl_wall_trigger_reset();
				xl_named_audio_resume();
				xl_named_audio_frame();
			};
			xl_named_audio_resume();
			xl_named_audio_frame();
			begin_capture();
			const auto first = starts();
			check_trigger(&Segments[Walls[0].segnum], static_cast<short>(Walls[0].sidenum), static_cast<short>(Players[Player_num].objnum), 0);
			require(starts() == first + 1 && event(0)[2] == 2 * F1_0, "Wall sound or source duration did not dispatch");
			xl_trigger_state scheduled{};
			require(xl_wall_trigger_state(0, &scheduled) && scheduled.delay_ms == -1, "Sound repetitions became an event delay");
			run_output(44100);
			for (std::size_t i = 0; i < captured_samples; ++i) {
				const auto frame = (i / 2) % 22050, channel = i % 2;
				const int raw = int((frame * (channel ? 53 : 37) + (channel ? 3 : 0)) % 511) * 100 - 25500;
				const auto expected = i < 88200 ? std::int16_t(float(raw) * (127.0f / 255.0f)) : std::int16_t(0);
				require(captured[i] == expected, "Actual trigger output lost stereo samples, repetitions or completion");
			}
			require(snapshot().at("voices").empty(), "Finite named audio voice did not finish");
			GameTime64 += 2 * F1_0;
			require(!xl_named_audio_action(&Triggers[0], 0), "Sound cooldown allowed the exact boundary");
			++GameTime64;
			require(xl_named_audio_action(&Triggers[0], 0) == 1, "Sound cooldown did not rearm after the strict boundary");
			reset();
			require(check_trigger_sub(5, Player_num, 1) == 0 && snapshot().at("voices").size() == 1, "Master did not dispatch named sound");
			reset();
			require(check_trigger_sub(6, Player_num, 1) == 0 && snapshot().at("voices")[0][5] == 1000, "Large repeat count or inert sound targets became a delay/dependency");
			reset();
			const auto disabled = starts();
			check_trigger_sub(7, Player_num, 1);
			require(starts() == disabled && check_trigger_sub(3, Player_num, 1) == 0 && (Triggers[3].flags & TF_DISABLED), "Disabled/one-shot sound dispatch differs");
			reset();
			require(xl_named_audio_action(&Triggers[1], 1) == 1 && !xl_named_audio_action(&Triggers[1], 1), "Zero-repeat same-tick rule differs");
			++GameTime64;
			require(xl_named_audio_action(&Triggers[1], 1) == 1 && xl_named_audio_action(&Triggers[2], 2) == 1 && xl_named_audio_action(&Triggers[2], 2) == 1 && snapshot().at("voices").size() == 4, "Zero/negative repeat overlap differs");
			const auto infinite = snapshot();
			for (const auto &voice : infinite.at("voices")) require(voice[5] == -1, "Nonpositive repeat did not create an infinite voice");
			reset();
			const auto pressure_origin = xl_audio_pressure::prepare();
			const auto pressure_start = snapshot();
			begin_capture();
			run_output(4096);
			xl_audio_pressure::output(pressure_start, captured.data(), nullptr, captured_samples, 44100, 128);
			xl_named_audio_pause(1);
			const auto pressure = snapshot();
			require(std::all_of(pressure.at("voices").begin(), pressure.at("voices").end(), [](const auto &voice) { return voice[3] > 0; }), "Full-pool save did not earn live playheads");
			bad_states(root);
			xl_audio_pressure::continuation(pressure_origin);
			xl_named_audio_stop();
			require(snapshot().at("cursor") == (pressure_origin + 1) % xl_compat::audio_voice_limit, "Stopping named voices reset the source allocation cursor");
			require(xl_named_audio_action(&Triggers[2], 2) == 1 && snapshot().at("cursor") != pressure.at("cursor"), "Cannot disturb the saved allocation position");
			auto *pressure_file = PHYSFS_openRead("audio-state.bin");
			require(pressure_file && xl_wall_trigger_read(pressure_file) && PHYSFS_close(pressure_file) && snapshot() == pressure, "Full-pool trailer lost voices or the allocation cursor");
			xl_audio_pressure::continuation(pressure_origin);
			require(xl_compat::restore_named_audio(pressure, true) && snapshot() == pressure, "Cannot recover exact full-pool playback after continuation");
			begin_capture();
			xl_named_audio_pause(0);
			run_output(4096);
			xl_audio_pressure::output(pressure, captured.data(), nullptr, captured_samples, 44100, 128);
			std::cout << "Full 128-voice pool: class threshold, zero/negative priority, real PCM, saved cursor and continuation passed\n";
			reset();
			const unsigned volume_origin = snapshot().at("cursor").get<unsigned>();
			for (unsigned i = 0; i < xl_compat::audio_voice_limit; ++i) {
				digi_set_digi_volume(i == 17 ? 8192 : i == 43 ? 0
				                                              : 32768);
				++GameTime64;
				require(xl_named_audio_action(&Triggers[1], 1) == 1, "Cannot earn different source channel priorities");
			}
			digi_set_digi_volume(32768);
			const auto volumes = snapshot();
			require(volumes.at("voices")[(volume_origin + 17) % 128][6] == 8192 && volumes.at("voices")[(volume_origin + 43) % 128][6] == 0,
			        "Named channel priority lost its start-time volume");
			bad_states(root);
			xl_audio_pressure::replace(1, (volume_origin + 43) % 128);
			require(xl_compat::restore_named_audio(volumes, true), "Cannot restore earned channel volume priorities");
			xl_audio_pressure::replace(1, (volume_origin + 43) % 128);
			xl_audio_pressure::replace(1, (volume_origin + 17) % 128);
			reset();
			check_trigger_sub(8, Player_num, 1);
			const auto cooldown = event(8)[2].get<std::int64_t>();
			GameTime64 += cooldown;
			xl_named_audio_wall_frame(8);
			require(!(Triggers[8].flags & TF_DISABLED) && event(8)[5].get<bool>(), "Wall sound expired at the exact boundary");
			++GameTime64;
			xl_named_audio_wall_frame(8);
			require((Triggers[8].flags & TF_DISABLED) && !event(8)[5].get<bool>(), "Permanent solid-wall sound expiry differs");
			reset();
			xl_wall_trigger_frame();
			require((Triggers[4].flags & TF_DISABLED) && snapshot().at("voices").size() == 2 && event(4)[5].get<bool>() && event(255)[5].get<bool>(), "Wall/object sound autoplay did not execute once");
			int owner = -1;
			for (int i = 0; i < xl_object_trigger_count(); ++i) {
				xl_object_trigger_state state{};
				require(xl_object_trigger_state_get(i, &state), "Cannot inspect audio object owner");
				if (state.value == F1_0) owner = state.owner;
			}
			require(owner >= 0 && Objects[owner].type == OBJ_POWERUP, "Sound removal owner missing");
			Objects[owner].flags |= OF_SHOULD_BE_DEAD;
			obj_delete_all_that_should_be_dead();
			require(Objects[owner].type == OBJ_NONE && event(254)[5].get<bool>(), "Actual object removal did not dispatch its sound");
			reset();
			begin_capture();
			check_trigger_sub(0, Player_num, 1);
			run_output(4096);
			const auto earned = snapshot();
			require(earned.at("voices").size() == 1 && earned.at("voices")[0][3] > 0, "Audio save control did not earn a nonzero playhead");
			digi_pause_digi_sounds();
			begin_capture();
			run_output(4096);
			require(snapshot() == earned && std::all_of(captured.begin(), captured.begin() + captured_samples, [](auto value) { return !value; }), "Pause advanced or emitted named audio");
			digi_resume_digi_sounds();
			require(xl_compat::named_audio_diagnostics().at("paused").get<bool>() && xl_compat::named_audio_diagnostics().at("resume_pending").get<bool>(), "Window activation resumed named audio before a game frame");
			xl_named_audio_frame();
			require(!xl_compat::named_audio_diagnostics().at("paused").get<bool>(), "Native audio did not resume on the first unpaused frame");
			bad_states(root);
			const auto identity = earned.at("source");
			for (const auto &bad : std::array<std::array<const char *, 2>, 4>{ { { "nosnd.rl2", "Missing XL named-audio resource" }, { "badsnd.rl2", "Duplicate XL level text ID" }, { "badid.rl2", "is missing from" }, { "badwave.rl2", "WAVE" } } }) {
				require(!xl_level_supported(bad[0], 0) && std::strstr(xl_level_rejection(), bad[1]) && load_level(bad[0]) != 0, "Named audio resource rejection is missing or unhelpful");
				require(snapshot() == earned, "Rejected named audio level replaced live resources or playheads");
			}
			const auto shadow = root / "audio-shadow";
			std::filesystem::create_directories(shadow);
			write_bytes((shadow / "sound.wav").string().c_str(), { 'x' });
			require(PHYSFS_addToSearchPath(shadow.string().c_str(), 0), "Cannot mount sound ownership control");
			const auto filename = ordinal == 1 ? "audio23.rl2" : "audio27.rl2";
			require(!xl_level_supported(filename, 0) && std::strstr(xl_level_rejection(), "shadowed outside the selected mission HOG") && snapshot() == earned, "Foreign named audio was accepted or changed live playback");
			require(PHYSFS_removeFromSearchPath(shadow.string().c_str()), "Cannot remove sound ownership control");
			char save[] = "audio-earned.sav", description[] = "XL sound playhead";
			stop_time();
			require(state_save_all_sub(save, description), "Cannot save earned sound playhead");
			require(load_base_mission("xschange", error, sizeof(error)) && !load_level("audio23.rl2"), "Changed audio resource control failed");
			auto *file = PHYSFS_openRead("audio-state.bin");
			require(identity != snapshot().at("source") && file && !xl_wall_trigger_read(file) && PHYSFS_close(file) && snapshot().at("voices").empty(), "Foreign SND resource accepted saved audio state");
			require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Audio stock transition failed");
			require(snapshot().at("source") == "" && snapshot().at("voices").empty() && xl_compat::named_audio_diagnostics().at("resources") == 0, "Owned audio leaked into Redux");
			trigger control{};
			control.type = 30;
			control.value = F1_0;
			control.time = 1;
			require(!xl_named_audio_action(&control, 0), "Named audio intercepted Redux gameplay");
			require(state_restore_all_sub(save, 0) && snapshot() == earned, "Disk-through-stock restore changed sound playhead, repeat count or event clock");
			// A real device reopen changes output rate, not the resource or event clock
			xl_named_audio_backend_close();
			Mix_CloseAudio();
			require(Mix_OpenAudio(48000, AUDIO_S16SYS, 2, 1024) == 0, "Cannot reopen the actual mixer at 48 kHz");
			SDL_PauseAudio(1);
			require(xl_named_audio_backend_open(48000, AUDIO_S16SYS, 2) && xl_compat::restore_named_audio(earned, true), "Cannot restore the earned voice at a different output rate");
			const auto resampled = snapshot();
			require(resampled.at("rate") == 48000 && resampled.at("events") == earned.at("events") && resampled.at("voices")[0][3] == earned.at("voices")[0][3].get<unsigned>() * std::uint64_t(48000) / 44100, "Device reopen lost the event clock or fractional playback phase");
			require(xl_named_audio_backend_open(48000, AUDIO_S16SYS, 2), "Idempotent audio backend open failed");
			xl_named_audio_backend_close();
			Mix_CloseAudio();
			require(Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) == 0, "Cannot restore the original actual mixer rate");
			SDL_PauseAudio(1);
			require(xl_named_audio_backend_open(44100, AUDIO_S16SYS, 2) && xl_compat::restore_named_audio(earned, true), "Cannot recover original-rate playback after device reopen");
			begin_capture();
			start_time();
			// Headless disk-through-stock controls retain nested menu pauses
			// Permit only the captured output, without inventing a gameplay frame
			xl_named_audio_pause(0);
			run_output(4096);
			const auto position = earned.at("voices")[0][3].get<std::size_t>();
			for (std::size_t i = 0; i < captured_samples; ++i) {
				const auto frame = (position + i / 2) % 22050, channel = i % 2;
				const int raw = int((frame * (channel ? 53 : 37) + (channel ? 3 : 0)) % 511) * 100 - 25500;
				const auto expected = std::int16_t(float(raw) * (127.0f / 255.0f));
				if (captured[i] != expected) throw std::runtime_error("Restored audio output differs at sample " + std::to_string(i) + ": " + std::to_string(captured[i]) + " != " + std::to_string(expected) + "; " + xl_compat::named_audio_diagnostics().dump());
			}
			xl_shared_audio_control::run(root, initial);
			xl_linked_audio_control::run(root, initial);
			std::cout << (ordinal == 1 ? 23 : 27) << "/40 sounds: wall/master/object/autoplay, actual stereo output, repetitions, pause, atomic ownership and disk-through-stock passed\n";
		}
		Mix_SetPostMix(nullptr, nullptr);
		xl_named_audio_clear();
		SDL_PauseAudio(0);
		std::cout << "XL named audio runtime controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		SDL_PauseAudio(1);
		Mix_SetPostMix(nullptr, nullptr);
		std::cerr << "XL named audio runtime failed: " << error.what() << '\n';
		return 1;
	}
#else
	(void) argc;
	(void) argv;
	std::cerr << "Named audio runtime controls require SDL_mixer\n";
	return 1;
#endif
}
