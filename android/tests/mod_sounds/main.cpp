#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "android_mod_assets.h"
extern "C" {
#include "SDL.h"
#include "args.h"
#include "bm.h"
#include "digi.h"
#include "dxxerror.h"
#include "piggy.h"
#include "physfsx.h"
#include "u_mem.h"
#include "android_log.h"
#ifdef DXX_BUILD_DESCENT_II
#include "wall.h"
#include "robot.h"
#include "weapon.h"
#include "sounds.h"
#include "object.h"
#include "game.h"
#include "rng_shared.h"
int do_robot_dying_frame(object *, fix64, fix, sbyte *, int, fix, fix);
extern int Num_sound_files;
#endif
}
using nlohmann::json;
static void require(bool value, const char *message)
{
	if (!value) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}
#ifdef DXX_BUILD_DESCENT_II
static std::vector<unsigned char> read(const std::string &path)
{
	auto *file = PHYSFSX_openReadBuffered(path.c_str());
	require(file != nullptr, path.c_str());
	std::vector<unsigned char> bytes(PHYSFS_fileLength(file));
	require(PHYSFS_readBytes(file, bytes.data(), bytes.size()) == static_cast<PHYSFS_sint64>(bytes.size()), "complete fixture read");
	PHYSFS_close(file);
	return bytes;
}
static unsigned word(const std::vector<unsigned char> &data, size_t offset)
{
	require(offset + 4 <= data.size(), "sound header bounds");
	return data[offset] | data[offset + 1] << 8 | data[offset + 2] << 16 | data[offset + 3] << 24;
}
static void verify_bank(int rate, bool hires)
{
	const auto bank = read(rate == 22050 ? "author/descent2.s22" : "author/descent2.s11");
	const unsigned count = word(bank, 8);
	require(Num_sound_files == static_cast<int>(count), "all original sound slots registered");
	const auto usage = android_mod_usage_snapshot().at("sounds");
	require(usage.size() == count, "all bank samples reported in Mod Details");
	int overlays = 0;
	for (unsigned i = 0; i < count; ++i) {
		const size_t header = 12 + i * 20;
		std::string name(reinterpret_cast<const char *>(bank.data() + header), 8);
		name.resize(strnlen(name.c_str(), 8));
		const unsigned length = word(bank, header + 8), offset = 12 + count * 20 + word(bank, header + 16);
		require(offset + length <= bank.size(), "sound data bounds");
		const auto &sound = GameSounds[i];
		const std::string replacement = "Sounds/" + name + ".r22";
		if (hires && PHYSFS_exists(replacement.c_str())) {
			const auto pcm = read(replacement);
			require(sound.length == pcm.size() && std::memcmp(sound.data, pcm.data(), pcm.size()) == 0 && sound.freq == 22050, "higher-priority PCM wins");
			require(usage[i].at("source") == "d2-hires-sounds.dxa", "PCM owner reported");
			++overlays;
		} else {
			require(sound.length == length && std::memcmp(sound.data, bank.data() + offset, length) == 0 && sound.freq == rate, "original bank PCM and rate preserved");
			require(usage[i].at("source") == "uud2sp-sounds.dxa", "bank owner reported");
		}
		require(usage[i].at("resident") == true, "resident fingerprint matches");
	}
	if (hires) require(overlays > 100, "substantial high-res overlay coverage");
}
static void verify_authored_timing()
{
	const std::string root = PHYSFS_getWriteDir();
	const std::string directory = root + "/authored-timing";
	require(PHYSFS_mkdir("authored-timing") && PHYSFS_mkdir("authored-timing/Sounds") && PHYSFS_mount(directory.c_str(), nullptr, 0), "isolate authored duration overlay");
	const char *name = piggy_sound_duration_name(0);
	require(name && *name, "canonical authored sound identity");
	const auto path = directory + "/Sounds/" + name + ".r22";
	FILE *file = std::fopen(path.c_str(), "wb");
	std::vector<unsigned char> pcm(22051, 128);
	require(file && std::fwrite(pcm.data(), 1, pcm.size(), file) == pcm.size() && std::fclose(file) == 0, "write odd-length authored PCM");
	const auto arguments = GameArg;
	const auto object = Objects[0];
	const fix old_frame_time = FrameTime;
	const fix64 old_game_time = GameTime64;
	const ubyte logical = Sounds[0], alternate = AltSounds[0];
	Sounds[0] = 0; AltSounds[0] = 255;
	GameArg.SysLowMem = 1; GameArg.SndNoSound = 1;
	const auto original = GameSounds[0];
	const fix original_duration = piggy_sound_duration(0);
	for (int rate : { 11025, 22050 }) for (int legacy : { 0, 1 }) for (int omitted : { 0, 1 }) {
		GameArg.SndDigiSampleRate = rate;
		GameArg.SndDisableSdlMixer = legacy;
		GameSounds[0] = original;
		if (omitted) GameSounds[0].data = reinterpret_cast<unsigned char *>(-1);
		android_mod_sound_apply(0, name, 2);
		require(piggy_sound_duration(0) == fixdiv(22051, 22050), "authored duration survives output rate, legacy rounding and lowmem omission");
		if (!omitted) require(GameSounds[0].length == (legacy && rate == 11025 ? 11025 : 22051), "production loader actually exercises odd-length resampling");
		for (int after_boundary : { 0, 1 }) {
			FrameTime = 0;
			GameTime64 = 12 * F1_0 - fixdiv(22051, 22050) + after_boundary;
			sbyte playing = 0;
			d_srand(1234); d_srand_fx(5678);
			d_rand_reset_call_count(); d_rand_set_stream_call_count(D_RNG_FX, 0);
			do_robot_dying_frame(&Objects[0], 10 * F1_0, 2 * F1_0, &playing, 0, F1_0, F1_0);
			require(playing == after_boundary, "real loaded overlay retains exact original strict death-roll boundary");
			require(d_rand_get_call_count() + d_rand_get_stream_call_count(D_RNG_FX) == static_cast<unsigned>(1 - after_boundary), "death cadence retains original one-draw early branch and zero-draw first late branch");
		}
		android_mod_sounds_reset();
		require(piggy_sound_duration(0) == original_duration, "overlay reset restores canonical metadata");
	}
	GameSounds[0] = original;
	android_mod_sound_apply(0, "_timing_missing_playback_", 2);
	require(piggy_sound_duration(0) == fixdiv(22051, 22050) && GameSounds[0].data == original.data && GameSounds[0].length == original.length,
	        "canonical-name metadata is independent of playback name and does not replace PCM");
	android_mod_sounds_reset();
	const int playback_count = Num_sound_files;
	Num_sound_files = 0;
	android_mod_sound_timing_apply(0);
	require(piggy_sound_duration(0) == fixdiv(22051, 22050) && GameSounds[0].data == original.data,
	        "canonical-only metadata remains available without a playback slot");
	Num_sound_files = playback_count;
	android_mod_sounds_reset();
	GameSounds[0] = original; GameArg = arguments;
	Objects[0] = object; FrameTime = old_frame_time; GameTime64 = old_game_time;
	Sounds[0] = logical; AltSounds[0] = alternate;
	require(PHYSFS_unmount(directory.c_str()) && std::remove(path.c_str()) == 0, "release authored timing fixture");
}

static void play_events()
{
	// Exercise the real logical-sound mapping and SDL mixer for restored event assignments
	const int events[] = { WallAnims[5].open_sound, WallAnims[9].close_sound,
		                   Robot_info[0].see_sound, Robot_info[24].see_sound, Weapon_info[40].wall_hit_sound };
	for (const int event : events) {
		const int sample = digi_xlat_sound(event);
		require(sample >= 0 && sample < Num_sound_files, "restored event maps to a loaded sample");
		digi_play_sample(event, F1_0);
		bool played = false;
		const auto usage = android_mod_usage_snapshot();
		for (const auto &entry : usage.at("sounds"))
			if (entry.at("sample") == sample) played = entry.at("played");
		require(played == (debug_log_enabled[DLOG_GAME] != 0), "mixer playback reporting follows Game Logs");
		digi_stop_all_channels();
	}
}
static void reload()
{
	digi_stop_all_channels();
	digi_free_cached_sounds();
	android_mod_sounds_reset();
	require(read_sndfile(), "reload sound headers");
	piggy_read_sounds();
	piggy_load_mission_data();
}
#endif
int main(int argc, char **argv)
{
	if (argc != 2) return 2;
	mem_init();
	error_init([](const char *message) { std::fprintf(stderr, "%s\n", message); });
	require(PHYSFS_init(nullptr) && PHYSFS_setWriteDir(".") && PHYSFS_mount(".", nullptr, 1), "isolated filesystem");
	json trace = json::array();
	debug_log_set_enabled(DLOG_GAME, 1);
#ifdef DXX_BUILD_DESCENT_II
	setenv("SDL_AUDIODRIVER", "dummy", 1);
	GameArg.SndNoMusic = GameArg.SysInputDemoNoRender = 1;
	GameArg.SndDigiSampleRate = 22050;
	digi_select_system(SDLMIXER_SYSTEM);
	require(digi_init() == 0, "initialize real mixer with dummy output device");
	require(PHYSFS_mount("UUD2SP1_4.dxa", "author", 1), "original author archive for independent PCM comparison");
	require(properties_init(), "stock properties");
	piggy_read_sounds();
	verify_authored_timing();
	require(android_mod_usage_snapshot().at("sounds").empty(), "stock sounds are not reported as mods");
	trace.push_back({ { "case", "stock" } });
	for (int rate : { 11025, 22050 }) {
		GameArg.SndDigiSampleRate = rate;
		require(PHYSFS_mount("uud2sp-sounds.dxa", nullptr, 0), "enable UUD2SP");
		reload();
		require(WallAnims[5].open_sound == 88 && WallAnims[9].close_sound == 89 && WallAnims[9].play_time == 65536, "semantic door assignments and combined timing applied");
		verify_bank(rate, false);
		// Disabling reports must not disable actual mod playback or touch the report
		debug_log_set_enabled(DLOG_GAME, 0);
		const std::string marker = "previous diagnostic report\n";
		std::FILE *report = std::fopen("mod_usage.json", "wb");
		require(report != nullptr, "create previous report");
		require(std::fwrite(marker.data(), 1, marker.size(), report) == marker.size(), "write previous report");
		std::fclose(report);
		play_events();
		const auto disabled = android_mod_usage_snapshot();
		require(!disabled.at("enabled").get<bool>() && disabled.at("sounds").empty() && disabled.at("presentation").empty(), "disabled introspection omits mod evidence");
		SDL_Delay(1001);
		android_mod_usage_flush();
		const auto previous = read("mod_usage.json");
		require(std::string(previous.begin(), previous.end()) == marker, "disabled reports perform no file write");
		debug_log_set_enabled(DLOG_GAME, 1);
		verify_bank(rate, false);
		play_events();
		android_mod_usage_flush();
		require(json::parse(read("mod_usage.json")).at("sounds").size() == 208, "re-enabling Game Logs publishes loaded mod evidence");
		trace.push_back({ { "case", "uud_only" }, { "rate", rate }, { "usage", android_mod_usage_snapshot() } });
		require(PHYSFS_mount("d2-hires-sounds.dxa", nullptr, 1), "enable lower-priority high-res sounds");
		reload();
		verify_bank(rate, false);
		play_events();
		trace.push_back({ { "case", "bank_first" }, { "rate", rate } });
		require(PHYSFS_unmount("d2-hires-sounds.dxa") && PHYSFS_mount("d2-hires-sounds.dxa", nullptr, 0), "move high-res above bank");
		reload();
		verify_bank(rate, true);
		play_events();
		trace.push_back({ { "case", "pcm_first" }, { "rate", rate } });
		reload();
		verify_bank(rate, true);
		play_events();
		trace.push_back({ { "case", "reload" }, { "rate", rate } });
		require(PHYSFS_unmount("d2-hires-sounds.dxa") && PHYSFS_unmount("uud2sp-sounds.dxa"), "disable sound mods");
		// The stock fixture includes only S22; read_sndfile's normal fallback selects it
		reload();
		require(android_mod_usage_snapshot().at("sounds").empty() && WallAnims[5].open_sound == 146, "disable restores stock sound bytes and assignments");
		trace.push_back({ { "case", "disabled" }, { "rate", rate } });
	}
	digi_close();
	gamedata_close();
#else
	require(android_mod_usage_snapshot().at("sounds").empty(), "D1 remains unaffected by D2 sound-bank support");
	trace.push_back({ { "case", "d1_unaffected" } });
#endif
	std::FILE *out = std::fopen(argv[1], "wb");
	require(out != nullptr, "open trace");
	const auto text = trace.dump(2) + "\n";
	require(std::fwrite(text.data(), 1, text.size(), out) == text.size(), "write trace");
	std::fclose(out);
	PHYSFS_deinit();
	return 0;
}
