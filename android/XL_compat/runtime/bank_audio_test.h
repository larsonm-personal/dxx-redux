#include "../tests/bank_fixture.h"
#include "bank_output_test.h"
namespace xl_bank_control
{
using namespace named_audio_control;
void run(const std::filesystem::path &root)
{
	const auto baseline_state = snapshot();
	const auto baseline = xl_compat::prepare_classic_playback({});
	const auto bank_path = root / "mods" / "xscope" / "xscope.s22";
	const auto low_path = root / "mods" / "xscope" / "xscope.s11";
	const auto wave_path = root / "mods" / "xscope" / "sounds" / (std::string(piggy_sound_name(0)) + ".wav");
	write_bytes(wave_path.string().c_str(), { 'b', 'a', 'd' });
	std::vector<std::pair<std::string, xl_compat::Bytes>> native;
	for (int slot = 0; const char *name = piggy_sound_name(slot); ++slot) {
		const auto &sound = GameSounds[slot];
		require(sound.freq == 22050 && sound.bits == 8 && sound.length > 0, "Bank fixture requires the original 22 kHz native bank");
		native.push_back({ name, xl_compat::Bytes(sound.data, sound.data + sound.length) });
	}
	const auto full = xl_bank_fixture::make(native);
	write_bytes(bank_path.string().c_str(), full);
	const auto complete = xl_compat::prepare_classic_playback(xl_compat::prepare_classic_audio("scope23.rl2", 1, 44100));
	require(complete.samples.size() == native.size(), "Full mission bank omitted native samples");
	for (unsigned slot = 0; slot < native.size(); ++slot)
		require(complete.samples[slot].pcm.samples == baseline.samples[slot].pcm.samples, "Full original-byte bank changed canonical source PCM");
	for (const auto &empty : { xl_bank_fixture::make({}), xl_bank_fixture::make({ { "unknown", { 99 } } }) }) {
		write_bytes(bank_path.string().c_str(), empty);
		const auto replacements = xl_compat::prepare_classic_audio("scope23.rl2", 1, 44100);
		require(replacements.samples.empty() && !replacements.identity.empty(), "Empty-name bank did not suppress WAVs or preserve its identity");
		const auto retained = xl_compat::prepare_classic_playback(replacements);
		require(retained.identity != baseline.identity, "Empty-name bank lost its saved-resource identity");
		for (unsigned slot = 0; slot < native.size(); ++slot)
			require(retained.samples[slot].pcm.samples == baseline.samples[slot].pcm.samples, "Empty-name bank changed a native sample");
	}
	auto partial = xl_bank_fixture::make({ { native[0].first, { 7, 8 } }, { native[0].first, xl_compat::Bytes(11025, 200) }, { "unknown", { 99 } } });
	for (unsigned i = 0; i < 3; ++i) partial[12 + 16 + i] = 255; // Out-of-range earlier duplicate is superseded
	write_bytes(bank_path.string().c_str(), partial);
	const auto low = xl_bank_fixture::make({ { native[0].first, xl_compat::Bytes(5512, 80) } });
	write_bytes(low_path.string().c_str(), low);
	std::set<std::string> identities;
	for (int rate : { 11025, 22050, 44100, 48000 }) {
		const auto replacements = xl_compat::prepare_classic_audio("scope23.rl2", -1, rate);
		require(replacements.samples.size() == 1 && replacements.samples[0].sample == 0 &&
		            replacements.samples[0].path == (rate < 22050 ? "mods/xscope/xscope.s11" : "mods/xscope/xscope.s22"),
		        "Bank selection lost its rate, registered name or signed ordinal");
		identities.insert(replacements.identity);
		const auto merged = xl_compat::prepare_classic_playback(replacements);
		for (unsigned slot = 1; slot < native.size(); ++slot)
			require(merged.samples[slot].pcm.samples == baseline.samples[slot].pcm.samples &&
			            merged.samples[slot].source_hash == baseline.samples[slot].source_hash,
			        "Partial mission bank changed an omitted native sample");
		require(snapshot() == baseline_state, "Bank preparation changed live playback");
	}
	require(identities.size() == 2, "Bank rates lost distinct resource identities");
	for (const auto &bad : { xl_compat::Bytes{ 'b', 'a', 'd' }, xl_compat::Bytes(partial.begin(), partial.begin() + 53) }) {
		write_bytes(bank_path.string().c_str(), bad);
		require(!xl_level_supported_at("scope23.rl2", 0, 1) && std::strstr(xl_level_rejection(), "Invalid XL mission sound bank mods/xscope/xscope.s22") && snapshot() == baseline_state,
		        "Malformed bank fell back to WAVs, lost its path or changed live playback");
	}
	write_bytes(bank_path.string().c_str(), partial);
	const auto foreign = root / "bank-foreign";
	const auto shadow = foreign / "mods" / "xscope" / "xscope.s22";
	std::filesystem::create_directories(shadow.parent_path());
	write_bytes(shadow.string().c_str(), partial);
	require(PHYSFS_addToSearchPath(foreign.string().c_str(), 0), "Cannot mount foreign bank control");
	const bool rejected = !xl_level_supported_at("scope23.rl2", 0, 1) && std::strstr(xl_level_rejection(), "shadowed outside the selected mission") && snapshot() == baseline_state;
	require(PHYSFS_removeFromSearchPath(foreign.string().c_str()) && rejected, "Foreign custom bank was admitted");
	int logical = -1;
	for (int id = 0; id < MAX_SOUNDS; ++id)
		if (xl_gameplay_sound_sample(id) == 0) {
			logical = id;
			break;
		}
	require(logical >= 0, "Bank fixture has no ordinary sample route");
	std::uint64_t checked = 0;
	int physical = 0;
	const auto output = [&](unsigned byte = 200, unsigned source_rate = 22050, unsigned source_frames = 11025, unsigned frames = 4096) {
		const auto start = snapshot();
		begin_capture();
		xl_named_audio_pause(0);
		run_output(frames);
		xl_bank_output::compare(start, captured.data(), nullptr, captured_samples, 128, physical + 4, byte, source_rate, source_frames);
		checked += captured_samples;
		xl_named_audio_pause(1);
	};
	nlohmann::json earned;
	for (int ordinal : { 1, 2, -1 }) {
		LoadLevel(ordinal, 0);
		SDL_PauseAudio(1);
		require(Current_level_num == ordinal && xl_level_extensions_active(), "Bank transition failed");
		digi_stop_digi_sounds();
		digi_set_digi_volume(32768);
		digi_play_sample(logical, F1_0);
		require(snapshot().at("voices").size() == 1 && snapshot().at("voices")[0][2] == 4, "Ordinary sound did not use the mission bank");
		output(200, 22050, 11025, 24000);
		require(snapshot().at("voices").empty(), "Finite bank sound did not finish at the authored sample length");
		digi_stop_digi_sounds();
		Viewer = ConsoleObject;
		require(xl_gameplay_sound_object(logical, XL_SOUND_AMBIENT, Players[Player_num].objnum, 1, F1_0, 256 * F1_0, -1, -1, 0) >= 0 &&
		            xl_gameplay_sound_position(logical, ConsoleObject->segnum, 0, &ConsoleObject->pos, 1, F1_0 / 2, 256 * F1_0) >= 0,
		        "Bank linked consumers failed");
		xl_audio_sync_objects();
		require(snapshot().at("voices").size() == 2, "Bank owners did not start both channels");
		output(200, 22050, 11025, ordinal == 1 ? 45124 : 4096);
		earned = snapshot();
	}
	char save[] = "bank-earned.sav", description[] = "XL mission bank";
	require(state_save_all_sub(save, description), "Cannot save mission bank state");
	char error[256]{};
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active() && snapshot().at("source") == "", "Custom bank leaked into Redux");
	require(state_restore_all_sub(save, 0) && Current_level_num == -1 && snapshot() == earned, "Saved custom bank failed stock transition");
	output();
	for (int rate : { 48000, 44100 }) {
		xl_named_audio_backend_close();
		Mix_CloseAudio();
		require(Mix_OpenAudio(rate, AUDIO_S16SYS, 2, 1024) == 0, "Cannot reopen bank backend");
		SDL_PauseAudio(1);
		require(xl_named_audio_backend_open(rate, AUDIO_S16SYS, 2) && xl_compat::restore_named_audio(earned, true), "Cannot restore bank at another output rate");
		output();
	}
	auto changed = partial;
	changed.back() ^= 1; // Even an ignored member belongs to the saved bank identity
	write_bytes(bank_path.string().c_str(), changed);
	LoadLevel(-1, 0);
	SDL_PauseAudio(1);
	const auto current = snapshot();
	require(current.at("source") != earned.at("source") && !xl_compat::restore_named_audio(earned, true) && snapshot() == current, "Changed bank accepted stale saved playback");
	write_bytes(bank_path.string().c_str(), partial);
	LoadLevel(-1, 0);
	SDL_PauseAudio(1);
	require(xl_compat::restore_named_audio(earned, true), "Bank identity did not recover after restoring bytes");
	output();
	xl_named_audio_backend_close();
	Mix_CloseAudio();
	require(Mix_OpenAudio(11025, AUDIO_S16SYS, 2, 1024) == 0, "Cannot open S11 playback profile");
	SDL_PauseAudio(1);
	require(xl_named_audio_backend_open(11025, AUDIO_S16SYS, 2), "Cannot activate S11 backend");
	LoadLevel(-1, 0);
	SDL_PauseAudio(1);
	digi_stop_digi_sounds();
	digi_play_sample(logical, F1_0);
	output(80, 11025, 5512, 6000);
	require(snapshot().at("voices").empty(), "S11 bank sound did not finish at its selected source rate");
	xl_named_audio_backend_close();
	Mix_CloseAudio();
	require(Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024) == 0, "Cannot recover ordinary bank backend");
	SDL_PauseAudio(1);
	require(xl_named_audio_backend_open(44100, AUDIO_S16SYS, 2), "Cannot activate recovered backend");
	for (const auto &path : { bank_path, low_path, wave_path }) std::filesystem::remove(path);
	LoadLevel(1, 0);
	SDL_PauseAudio(1);
	require(xl_compat::prepare_classic_audio("scope23.rl2", 1, 44100).samples.empty() && xl_compat::prepare_classic_playback({}).identity == baseline.identity,
	        "Removing mission bank did not recover original samples");
	require(load_base_mission("xbkhog", error, sizeof(error)), error);
	StartNewGame(1);
	SDL_PauseAudio(1);
	const auto archived = xl_compat::prepare_classic_audio("scope23.rl2", 1, 44100);
	require(archived.samples.size() == 1 && archived.samples[0].path == "xbkhog.s22" && archived.samples[0].name == "laser01", "Owned HOG bank was ignored or lost to a WAV");
	// Packaged controls name laser01; native slot zero is a different sample
	physical = archived.samples[0].sample;
	logical = -1;
	for (int id = 0; id < MAX_SOUNDS; ++id)
		if (xl_gameplay_sound_sample(id) == physical) {
			logical = id;
			break;
		}
	require(logical >= 0 && !std::strcmp(piggy_sound_name(physical), "laser01"), "Archive bank lacks its named ordinary route");
	digi_stop_digi_sounds();
	digi_play_sample(logical, F1_0);
	require(snapshot().at("voices").size() == 1 && snapshot().at("voices")[0][2] == physical + 4, "Archive bank voice selected the wrong native sample");
	output();
	const auto archived_state = snapshot();
	const auto override_bank = root / "mods" / "xbkhog" / "xbkhog.s22";
	write_bytes(override_bank.string().c_str(), { 'b', 'a', 'd' });
	require(!xl_level_supported_at("scope23.rl2", 0, 1) && std::strstr(xl_level_rejection(), "Invalid XL mission sound bank mods/xbkhog/xbkhog.s22") && snapshot() == archived_state,
	        "Invalid folder bank fell through to the HOG bank");
	std::filesystem::remove(override_bank);
	require(xl_level_supported_at("scope23.rl2", 0, 1), "HOG bank did not recover after removing a malformed override");
	write_bytes((foreign / "xbkhog.s22").string().c_str(), partial);
	require(PHYSFS_addToSearchPath(foreign.string().c_str(), 0), "Cannot mount foreign archive-bank shadow");
	const bool archive_rejected = !xl_level_supported_at("scope23.rl2", 0, 1) && std::strstr(xl_level_rejection(), "selected mission HOG") && snapshot() == archived_state;
	require(PHYSFS_removeFromSearchPath(foreign.string().c_str()) && archive_rejected, "Foreign archive-bank shadow was accepted");
	require(load_base_mission("xhwav", error, sizeof(error)), error);
	StartNewGame(1);
	SDL_PauseAudio(1);
	for (int ordinal : { 1, 2 }) {
		if (ordinal != 1) {
			LoadLevel(ordinal, 0);
			SDL_PauseAudio(1);
		}
		const auto selected = xl_compat::prepare_classic_audio("scope23.rl2", ordinal, 44100);
		require(selected.samples.size() == 1 && selected.samples[0].path == (ordinal == 1 ? "laser01.wav" : "mods/xhwav/sounds/level02/laser01.wav"),
		        "Loaded WAV priority differs from actual CFile folder/HOG order");
		if (ordinal == 1) {
			const auto prior = snapshot();
			write_bytes((foreign / "laser01.wav").string().c_str(), { 'b', 'a', 'd' });
			require(PHYSFS_addToSearchPath(foreign.string().c_str(), 0), "Cannot mount foreign archive-WAV shadow");
			const bool wave_rejected = !xl_level_supported_at("scope23.rl2", 0, 1) && std::strstr(xl_level_rejection(), "selected mission HOG") && snapshot() == prior;
			require(PHYSFS_removeFromSearchPath(foreign.string().c_str()) && wave_rejected, "Foreign archive-WAV shadow was accepted");
		}
		digi_stop_digi_sounds();
		digi_play_sample(logical, F1_0);
		const auto wave_state = snapshot();
		require(wave_state.at("voices").size() == 1 && wave_state.at("voices")[0][2] == physical + 4, "Archive WAV voice selected the wrong native sample");
		begin_capture();
		xl_named_audio_pause(0);
		run_output(4096);
		xl_classic_output::compare(wave_state, ordinal == 1 ? 3 : 2, captured.data(), nullptr, captured_samples, 128, physical + 4);
		xl_classic_playback_control::checked += captured_samples;
	}
	std::cout << "XL mission banks passed: " << native.size() << " original samples, partial retention, S11/S22 selection, signed scopes, atomic rejection, persistence, " << checked << " bank and " << xl_classic_playback_control::checked << " archive-WAV PCM samples\n";
}
} // namespace xl_bank_control
