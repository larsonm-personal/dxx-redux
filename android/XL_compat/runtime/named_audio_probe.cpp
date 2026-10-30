// Android diagnostic controls for the labeled derived audio fixture
#include "named_audio_probe.h"
#include "named_audio.h"
#include "named_audio_state.h"
#include "shared_audio.h"
#include "linked_audio.h"
#include "shared_audio_pcm_test.h"
#include "seismic_audio_test.h"
#include "gameplay_dispatch_test.h"
#include "preview.h"
#include "object_trigger.h"
#include "android_mission_assets.h"
#include <SDL.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
extern "C" {
#include "inferno.h"
#include "sounds.h"
#include "game.h"
#include "gameseq.h"
#include "gamesave.h"
#include "object.h"
#include "player.h"
#include "switch.h"
#include "wall.h"
#include "digi.h"
#include "songs.h"
#include "state_android_shared.h"
#include "android_meta_actions.h"
#include "android_audio_capture.h"
void obj_delete_all_that_should_be_dead(void);
}
#include "audio_pressure_test.h"

namespace
{
using nlohmann::json;
std::string failure;
json earned;
fix energy = 0;
int version = 0;
int scope_ordinal = 0;
unsigned output_checks = 0;
std::uint64_t checked_samples = 0;
std::uint64_t music_samples = 0;
std::array<std::int16_t, 524288> captured{};
std::array<std::int16_t, 524288> captured_music{};
std::size_t captured_samples = 0;
unsigned capture_rate = 0;
bool invalid = false;
bool pressure_earned = false;
bool shared_earned = false;
bool linked_earned = false;
unsigned shared_checks = 0;
unsigned linked_checks = 0;
unsigned seismic_checks = 0;
unsigned dispatch_checks = 0;
unsigned pressure_origin = 0, pressure_checks = 0;
void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
json snapshot()
{
	return xl_compat::named_audio_snapshot();
}
void fixture()
{
	require(xl_level_extensions_active() && Num_triggers == 10 && xl_object_trigger_count() == 2 && Triggers[0].type == 30 && Triggers[0].value == F1_0 + F1_0 / 2 && Triggers[6].time == 1000, "Named audio controls require the labeled derived fixture");
	require(gamesave_level_data_warning_count() == 0, "Android inert sound links produced a level repair warning");
}
void capture(const SDL_AudioSpec *spec, const Uint8 *data, const Uint8 *music, int length, long long)
{
	if (!music || spec->format != AUDIO_S16SYS || spec->channels != 2 || unsigned(spec->freq) != capture_rate || length < 0 || length % 4) {
		invalid = true;
		return;
	}
	const auto count = (std::min) (std::size_t(length / 2), captured.size() - captured_samples);
	std::memcpy(captured.data() + captured_samples, data, count * sizeof(std::int16_t));
	std::memcpy(captured_music.data() + captured_samples, music, count * sizeof(std::int16_t));
	captured_samples += count;
}
void capture_begin()
{
	SDL_PauseAudio(1);
	digi_stop_all_channels();
	songs_set_volume(0);
	digi_set_digi_volume(32768);
	captured_samples = 0;
	invalid = false;
	capture_rate = xl_compat::named_audio_diagnostics().at("rate").get<unsigned>();
	require(capture_rate >= 8000 && capture_rate <= 96000, "Android audio output rate exceeds the diagnostic capture bound");
	androidaud_set_capture_callback(capture);
}
void capture_run(unsigned frames)
{
	SDL_PauseAudio(0);
	const auto start = SDL_GetTicks();
	for (;;) {
		SDL_LockAudio();
		const bool done = captured_samples >= std::size_t(frames) * 2;
		SDL_UnlockAudio();
		if (done) break;
		require(SDL_GetTicks() - start < 3500, "Android actual audio output timed out");
		SDL_Delay(1);
	}
	SDL_PauseAudio(1);
	androidaud_set_capture_callback(nullptr);
	require(!invalid, "Android output changed its captured PCM format");
}
void reset_events()
{
	xl_named_audio_pause(1);
	auto empty = snapshot();
	empty["voices"] = json::array();
	empty["loop"] = json::array();
	empty["linked"]["owners"] = json::array();
	empty["linked"]["deferred"] = bool(0);
	for (auto &row : empty["events"]) {
		row[1] = -1;
		row[2] = -1;
		row[3] = 0;
		row[4] = 0;
		row[5] = false;
	}
	require(xl_compat::restore_named_audio(empty, true), "Cannot reset controlled named audio events");
	xl_wall_trigger_reset();
}
int raw_sample(std::uint64_t frame, unsigned channel, bool mono)
{
	return (int((frame * (channel && !mono ? 53 : 37) + (channel && !mono ? 3 : 0)) % 511) * 100) - 25500;
}
void compare_output(std::uint64_t position, unsigned repetitions, bool mono, bool silence = false, int gain = 20)
{
	// Independent fixture waveform evaluated at output timestamps
	const std::uint64_t source_rate = mono ? 22050 : 44100;
	const std::uint64_t source_frames = source_rate / 2;
	const std::uint64_t output_frames = (source_frames * capture_rate + source_rate / 2) / source_rate;
	for (std::size_t i = 0; i < captured_samples; ++i) {
		int expected = 0;
		if (!silence && position + i / 2 < output_frames * repetitions) {
			const auto frame = (position + i / 2) % output_frames;
			const auto phase = frame * source_rate;
			const auto first = (std::min) (phase / capture_rate, source_frames - 1);
			const auto second = (std::min) (first + 1, source_frames - 1);
			const auto fraction = std::int64_t(phase % capture_rate);
			const auto value = (std::int64_t(raw_sample(first, unsigned(i % 2), mono)) * (capture_rate - fraction) + std::int64_t(raw_sample(second, unsigned(i % 2), mono)) * fraction) / capture_rate;
			// Normal Android SFX gain is 1/4 with 5/8 gameplay headroom
			expected = int(float(value) * (127.0f / 255.0f)) * gain / 128;
		}
		// The existing driver tap captures music before SFX; queued music can
		// remain after a volume change and must not masquerade as sound output
		if (captured_music[i]) ++music_samples;
		expected = (std::clamp) (expected + int(captured_music[i]), -32768, 32767);
		if (captured[i] != expected) {
			throw std::runtime_error("Android named-audio PCM differs at sample " + std::to_string(i) + ": " + std::to_string(captured[i]) + " != " + std::to_string(expected));
		}
	}
	checked_samples += captured_samples;
	++output_checks;
}
void verify()
{
	const auto current = snapshot();
	if (earned.is_null() || current != earned || xl_source_level_version() != version || Players[Player_num].energy != energy || xl_compat::named_audio_diagnostics().at("paused") != true)
		throw std::runtime_error("Android audio restoration differs: " + json::diff(earned, current).dump() + "; version=" + std::to_string(xl_source_level_version()) + "; energy=" + std::to_string(Players[Player_num].energy) + "; paused=" + xl_compat::named_audio_diagnostics().at("paused").dump());
	if (pressure_earned) {
		xl_audio_pressure::continuation(pressure_origin);
		require(xl_compat::restore_named_audio(earned, true) && snapshot() == earned, "Android pressure continuation did not recover exact saved voices and cursor");
		++pressure_checks;
	}
	if (linked_earned) {
		const int slot = earned.at("loop")[0].get<int>();
		digi_change_looping_volume(F1_0 / 4);
		const auto changed = snapshot();
		bool controlled = false;
		for (const auto &voice : changed.at("voices"))
			if (voice[0] == slot) controlled = voice[6] == F1_0 / 8;
		require(controlled && changed.at("loop") == earned.at("loop"), "Android restored loop control lost its channel identity");
		require(xl_compat::restore_named_audio(earned, true) && snapshot() == earned, "Android loop control could not recover exact saved state");
	}
}

void compare_pressure_output(const json &state)
{
	xl_audio_pressure::output(state, captured.data(), captured_music.data(), captured_samples, capture_rate, 20);
	for (std::size_t i = 0; i < captured_samples; ++i)
		if (captured_music[i]) ++music_samples;
	checked_samples += captured_samples;
	++output_checks;
}

void compare_shared_output(const json &state, unsigned &category = shared_checks)
{
	xl_shared_audio_pcm::output(state, captured.data(), captured_music.data(), captured_samples, 20);
	for (std::size_t i = 0; i < captured_samples; ++i)
		if (captured_music[i]) ++music_samples;
	checked_samples += captured_samples;
	++output_checks;
	++category;
}

void prepare_shared()
{
	reset_events();
	capture_begin();
	require(xl_compat::named_audio_diagnostics().at("classic_samples") == 183 && xl_compat::named_audio_diagnostics().at("resources") == 187, "Android shared bank omitted stock or named samples");
	const int ordinary = xl_audio_start_sample(0, 3, F1_0 / 2, 0, 1, -1, -1);
	require(ordinary >= 0 && xl_audio_channel_playing(ordinary), "Android ordinary sample did not enter the shared pool");
	++GameTime64;
	require(xl_named_audio_action(&Triggers[0], 0), "Android named sample did not join the shared pool");
	auto starting = snapshot();
	require(starting.at("voices").size() == 2, "Android mixed bank did not use one channel registry");
	xl_named_audio_pause(0);
	capture_run(4096);
	compare_shared_output(starting);
	capture_begin();
	require(xl_audio_set_channel_volume(ordinary, 2 * F1_0) && xl_audio_set_channel_pan(ordinary, 65536), "Android shared live controls failed");
	starting = snapshot();
	digi_set_digi_volume(0);
	capture_run(4096);
	compare_shared_output(starting);
	xl_audio_stop_channel(ordinary);
	require(!xl_audio_channel_playing(ordinary) && snapshot().at("voices").size() == 1, "Android stopping an ordinary sample changed named playback");
	reset_events();
	capture_begin();
	const auto origin = snapshot().at("cursor").get<unsigned>();
	for (unsigned i = 0; i < 128; ++i) {
		if (i < 64) require(xl_audio_start_sample(int(i % 2), 3, i == 17 ? 8192 : F1_0, int(i * 1024), 1, -1, -1) >= 0, "Android ordinary pool fill failed");
		else {
			++GameTime64;
			require(xl_named_audio_action(&Triggers[1], 1), "Android named pool fill failed");
		}
	}
	require(snapshot().at("voices").size() == 128, "Android mixed pool did not retain all 128 voices");
	require(xl_audio_start_sample(1, 3, F1_0, 16384, 1, -1, -1) == int((origin + 17) % 128), "Android shared replacement selected the wrong class or volume");
	++GameTime64;
	require(xl_named_audio_action(&Triggers[1], 1), "Android shared named replacement failed");
	starting = snapshot();
	require(starting.at("voices")[(origin + 64) % 128][1] == starting.at("next").get<std::uint64_t>() - 1, "Android shared named replacement selected the wrong class");
	xl_named_audio_pause(0);
	capture_run(4096);
	compare_shared_output(starting);
	xl_named_audio_pause(1);
	earned = snapshot();
	require(std::all_of(earned.at("voices").begin(), earned.at("voices").end(), [](const auto &row) { return row[3] > 0; }), "Android mixed persistence did not earn nonzero playheads");
	energy = Players[Player_num].energy;
	version = xl_source_level_version();
	pressure_earned = false;
	shared_earned = true;
	linked_earned = false;
	SDL_PauseAudio(0);
	verify();
}

#include "linked_audio_probe_test.h"

void scope_output(int action)
{
	reset_events();
	capture_begin();
	Triggers[action].flags = 0;
	require(check_trigger_sub(action, Player_num, 1) == 0 && snapshot().at("voices").size() == 1, "Android scoped wall action failed");
	xl_named_audio_pause(0);
	capture_run(4096);
	const int ordinal = Current_level_num;
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
	for (std::size_t i = 0; i < captured_samples; ++i) {
		const int sample = std::int16_t(float(i % 2 ? right : left) * (127.0f / 255.0f));
		const int expected = (std::clamp) (sample * 20 / 128 + int(captured_music[i]), -32768, 32767);
		require(captured[i] == expected, "Android scoped resource produced the wrong stereo PCM");
		if (captured_music[i]) ++music_samples;
	}
	checked_samples += captured_samples;
	++output_checks;
	xl_named_audio_pause(1);
	SDL_PauseAudio(0);
}

void scope_control(const char *operation)
{
	if (!std::strcmp(operation, "disk_load")) {
		require(!earned.is_null() && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android scoped audio");
		verify();
		require(Current_level_num == scope_ordinal && *android_mission_assets_key(), "Scoped disk restore selected the wrong owned mission/level");
		require(android_open_overlay_pause_if_safe(), "Cannot pause restored scoped audio controls");
		return;
	}
	fixture();
	require(Current_mission && !std::strcmp(Current_mission_filename, "xscope") && Last_level == 3 && Last_secret_level == -1 && *android_mission_assets_key(), "Scoped audio requires the published derived package");
	if (!std::strcmp(operation, "next2") || !std::strcmp(operation, "secret") || !std::strcmp(operation, "next3")) {
		require(!game_is_time_paused(), "Scoped transitions require unpaused gameplay");
		const int next = !std::strcmp(operation, "next2") ? 2 : !std::strcmp(operation, "secret") ? -1
		                                                                                          : 3;
		const auto previous = snapshot();
		StartNewLevelSub(next, 1, 0);
		const auto current = snapshot();
		require(Current_level_num == next && current.at("source") != previous.at("source") &&
		            std::all_of(current.at("voices").begin(), current.at("voices").end(), [&](const auto &voice) { return voice[1] >= previous.at("next"); }),
		        "Scoped transition retained the prior resource or voice");
		return;
	}
	require(game_is_time_paused(), "Android scoped controls require paused gameplay");
	if (!std::strcmp(operation, "output")) {
		const auto diagnostic = xl_compat::named_audio_diagnostics();
		const std::string folder = Current_level_num == 1 ? "level01/" : Current_level_num == 2 ? "level02/"
		                                                             : Current_level_num == -1  ? "slevel01/"
		                                                                                        : "";
		require(diagnostic.at("files") == json::array({ "shared.wav", "mods/xscope/sounds/" + folder + "scoped.wav", "mods/xscope/sounds/mission.wav", "tiny.wav" }), "Android selected an incorrect sound scope or priority");
		for (int action : { 0, 1, 2 }) scope_output(action);
	} else if (!std::strcmp(operation, "prepare")) {
		pressure_earned = false;
		scope_output(1);
		earned = snapshot();
		energy = Players[Player_num].energy;
		version = xl_source_level_version();
		scope_ordinal = Current_level_num;
		require(scope_ordinal == 2 && earned.at("voices").size() == 1 && earned.at("voices")[0][3] > 0, "Scoped save must earn level-two playback");
		verify();
		require(!xl_level_supported("scope23.rl2", 0) && std::strstr(xl_level_rejection(), "explicit incoming level number"), "Android ambiguous sound scope passed admission");
		verify();
	} else if (!std::strcmp(operation, "verify")) {
		verify();
		require(Current_level_num == scope_ordinal, "Scoped save restored a different incoming level number");
	} else if (!std::strcmp(operation, "disk_save")) {
		verify();
		require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL sound scope", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android scoped audio");
	} else if (!std::strcmp(operation, "resume_output")) {
		verify();
		capture_begin();
		xl_named_audio_pause(0);
		capture_run(4096);
		for (std::size_t i = 0; i < captured_samples; ++i) {
			const int sample = std::int16_t(float(i % 2 ? 9000 : -3000) * (127.0f / 255.0f));
			require(captured[i] == (std::clamp) (sample * 20 / 128 + int(captured_music[i]), -32768, 32767), "Restored scoped voice produced another level's audio");
		}
		checked_samples += captured_samples;
		++output_checks;
		xl_named_audio_pause(1);
		require(xl_compat::restore_named_audio(earned, true), "Cannot recover scoped output playhead");
		SDL_PauseAudio(0);
		verify();
	} else require(false, "Unknown Android scoped audio operation");
}
} // namespace

json xl_named_audio_probe_diagnostics()
{
	return { { "output_checks", output_checks }, { "checked_samples", checked_samples }, { "music_samples", music_samples }, { "rate", capture_rate }, { "pressure_checks", pressure_checks }, { "shared_checks", shared_checks }, { "linked_checks", linked_checks }, { "seismic_checks", seismic_checks }, { "dispatch_checks", dispatch_checks } };
}

const char *xl_named_audio_probe(const char *operation)
{
	try {
		require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "Named audio controls require running singleplayer");
		if (!std::strncmp(operation, "scope_", 6)) {
			scope_control(operation + 6);
		} else if (!std::strcmp(operation, "stock")) {
			require(!xl_level_extensions_active() && snapshot().at("source") == "" && snapshot().at("voices").empty() && xl_compat::named_audio_diagnostics().at("resources") == 0, "Named audio leaked into Redux");
			require(!Seismic_sound_playing && !Seismic_tremor_volume, "XL seismic control state leaked into Android stock gameplay");
		} else if (!std::strcmp(operation, "disk_load")) {
			require(!earned.is_null() && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android named audio");
			verify();
			require(android_open_overlay_pause_if_safe(), "Cannot pause restored audio controls");
		} else {
			fixture();
			if (!std::strcmp(operation, "next")) {
				require(!game_is_time_paused() && Current_level_num == 1 && Last_level == 2, "Sound transition requires the running two-level fixture");
				StartNewLevelSub(2, 1, 0);
				const auto next = snapshot();
				// Both fixture levels intentionally contain identical sound resources
				require(Current_level_num == 2 && xl_source_level_version() == 27 && next.at("source") == earned.at("source") &&
				            std::all_of(next.at("voices").begin(), next.at("voices").end(), [&](const auto &voice) { return voice[1] >= earned.at("next"); }) &&
				            std::any_of(next.at("events").begin(), next.at("events").end(), [](const auto &row) { return row[0] == 0 && row[1] == -1 && row[3] == 0; }),
				        "Sound level transition retained an old voice or event");
			} else {
				require(game_is_time_paused(), "Controlled Android audio requires paused gameplay");
				if (!std::strcmp(operation, "events")) {
					const auto autoplay = snapshot();
					require((Triggers[4].flags & TF_DISABLED) && std::any_of(autoplay.at("voices").begin(), autoplay.at("voices").end(), [](const auto &voice) { return voice[4] == 255 && voice[5] == -1; }), "Android startup did not play the wall/object autoplay sounds");
					reset_events();
					Triggers[4].flags = 128;
					xl_wall_trigger_frame();
					require((Triggers[4].flags & TF_DISABLED) && snapshot().at("voices").size() == 1, "Android wall audio autoplay differs");
					int owner = -1;
					for (int i = 0; i < xl_object_trigger_count(); ++i) {
						xl_object_trigger_state state{};
						require(xl_object_trigger_state_get(i, &state), "Cannot inspect Android audio owner");
						if (state.value == F1_0) owner = state.owner;
					}
					require(owner >= 0 && Objects[owner].type == OBJ_POWERUP, "Android audio removal owner missing");
					Objects[owner].flags |= OF_SHOULD_BE_DEAD;
					obj_delete_all_that_should_be_dead();
					require(Objects[owner].type == OBJ_NONE && snapshot().at("voices").size() == 2, "Android object removal did not start its audio");
					reset_events();
					require(check_trigger_sub(5, Player_num, 1) == 0 && snapshot().at("voices").size() == 1, "Android master did not start its sound child");
					reset_events();
					const auto before = xl_compat::named_audio_diagnostics().at("starts");
					check_trigger_sub(7, Player_num, 1);
					require(xl_compat::named_audio_diagnostics().at("starts") == before, "Android disabled sound executed");
					require(check_trigger_sub(3, Player_num, 1) == 0 && (Triggers[3].flags & TF_DISABLED), "Android sound one-shot flag differs");
					reset_events();
					require(xl_named_audio_action(&Triggers[1], 1) && xl_named_audio_action(&Triggers[2], 2), "Android infinite sound actions failed");
					const auto state = snapshot();
					require(state.at("voices").size() == 2 && state.at("voices")[0][5] == -1 && state.at("voices")[1][5] == -1, "Android zero/negative repeats did not remain infinite");
					reset_events();
					// Every normal SFX slider position, including exact mute output
					const std::array<int, 9> gains{ 0, 2, 5, 7, 10, 12, 15, 17, 20 };
					for (unsigned level = 0; level < gains.size(); ++level) {
						capture_begin();
						digi_set_digi_volume(int(level) * 4096);
						require(xl_named_audio_action(&Triggers[0], 0), "Cannot start the Android volume control");
						const auto diagnostics = xl_compat::named_audio_diagnostics();
						require(diagnostics.at("effects_volume") == int(level) * 4096 && diagnostics.at("output_volume") == 20 && snapshot().at("voices")[0][10] == int(level) * 16, "Android named audio channel gain or platform headroom differs");
						xl_named_audio_pause(0);
						capture_run(4096);
						compare_output(0, 2, false, false, gains[level]);
						reset_events();
						SDL_PauseAudio(0);
					}
					digi_set_digi_volume(32768);
				} else if (!std::strcmp(operation, "output") || !std::strcmp(operation, "mono")) {
					const bool mono = !std::strcmp(operation, "mono");
					reset_events();
					capture_begin();
					const int index = mono ? 8 : 0;
					Triggers[index].flags = mono ? 8 : 0;
					require(check_trigger_sub(index, Player_num, 1) == 0, "Android sound action failed");
					xl_named_audio_pause(0);
					capture_run(capture_rate * (mono ? 3 : 2) / 2 + 1024);
					compare_output(0, mono ? 3 : 2, mono);
					require(snapshot().at("voices").empty(), "Android finite sound did not complete naturally");
					xl_named_audio_pause(1);
					SDL_PauseAudio(0);
				} else if (!std::strcmp(operation, "prepare") || !std::strcmp(operation, "pressure")) {
					shared_earned = false;
					linked_earned = false;
					pressure_earned = !std::strcmp(operation, "pressure");
					reset_events();
					capture_begin();
					if (pressure_earned) pressure_origin = xl_audio_pressure::prepare();
					else {
						Triggers[0].flags = 0;
						require(check_trigger_sub(0, Player_num, 1) == 0, "Cannot earn Android named audio state");
					}
					const auto starting = snapshot();
					xl_named_audio_pause(0);
					capture_run(4096);
					if (pressure_earned) compare_pressure_output(starting);
					else compare_output(0, 2, false);
					xl_named_audio_pause(1);
					earned = snapshot();
					energy = Players[Player_num].energy;
					version = xl_source_level_version();
					if (pressure_earned)
						require(earned.at("voices").size() == xl_compat::audio_voice_limit && earned.at("cursor") != 0 &&
						            std::all_of(earned.at("voices").begin(), earned.at("voices").end(), [](const auto &voice) { return voice[3] > 0; }),
						        "Android pressure save lacks full live playback and a nonzero allocation position");
					else require(earned.at("voices").size() == 1 && earned.at("voices")[0][3] > 0 && earned.at("voices")[0][5] == 2, "Android save control lacks earned playback");
					SDL_PauseAudio(0);
					verify();
					for (const char *name : { "nosnd.rl2", "badsnd.rl2", "badid.rl2", "badwave.rl2" }) {
						require(!xl_level_supported(name, 0), "Android admitted an invalid sound resource");
						verify();
					}
				} else if (!std::strcmp(operation, "shared")) prepare_shared();
				else if (!std::strcmp(operation, "linked")) prepare_linked();
				else if (!std::strcmp(operation, "verify")) verify();
				else if (!std::strcmp(operation, "pause_output")) {
					verify();
					capture_begin();
					capture_run(4096);
					compare_output(0, 0, false, true);
					verify();
					SDL_PauseAudio(0);
				} else if (!std::strcmp(operation, "resume_output")) {
					verify();
					capture_begin();
					xl_named_audio_pause(0);
					capture_run(4096);
					if (linked_earned) compare_linked_output(earned);
					else if (shared_earned) compare_shared_output(earned);
					else if (pressure_earned) compare_pressure_output(earned);
					else compare_output(earned.at("voices")[0][3].get<unsigned>(), 2, false);
					xl_named_audio_pause(1);
					require(xl_compat::restore_named_audio(earned, true), "Cannot restore controlled output phase");
					SDL_PauseAudio(0);
					verify();
				} else if (!std::strcmp(operation, "disturb")) {
					verify();
					Players[Player_num].energy -= F1_0;
					xl_named_audio_stop();
					require(snapshot().at("voices").empty(), "Named audio disturbance did not stop playback");
				} else if (!std::strcmp(operation, "disk_save")) {
					verify();
					require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL sound phase", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android named audio");
				} else require(false, "Unknown named audio control operation");
			}
		}
		return nullptr;
	} catch (const std::exception &error) {
		androidaud_set_capture_callback(nullptr);
		xl_named_audio_pause(game_is_time_paused());
		SDL_PauseAudio(0);
		failure = error.what();
		return failure.c_str();
	}
}
