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
#ifdef DXX_BUILD_DESCENT_II
#include "wall.h"
#include "robot.h"
#include "weapon.h"
#include "sounds.h"
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
		require(played, "actual SDL mixer accepted restored event PCM");
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
#ifdef DXX_BUILD_DESCENT_II
	setenv("SDL_AUDIODRIVER", "dummy", 1);
	GameArg.SndNoMusic = GameArg.SysInputDemoNoRender = 1;
	GameArg.SndDigiSampleRate = 22050;
	digi_select_system(SDLMIXER_SYSTEM);
	require(digi_init() == 0, "initialize real mixer with dummy output device");
	require(PHYSFS_mount("UUD2SP1_4.dxa", "author", 1), "original author archive for independent PCM comparison");
	require(properties_init(), "stock properties");
	piggy_read_sounds();
	require(android_mod_usage_snapshot().at("sounds").empty(), "stock sounds are not reported as mods");
	trace.push_back({ { "case", "stock" } });
	for (int rate : { 11025, 22050 }) {
		GameArg.SndDigiSampleRate = rate;
		require(PHYSFS_mount("uud2sp-sounds.dxa", nullptr, 0), "enable UUD2SP");
		reload();
		require(WallAnims[5].open_sound == 88 && WallAnims[9].close_sound == 89 && WallAnims[9].play_time == 65536, "semantic door assignments and combined timing applied");
		verify_bank(rate, false);
		play_events();
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
