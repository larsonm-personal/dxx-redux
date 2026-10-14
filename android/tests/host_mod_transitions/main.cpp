/* Host fixture: actual Android PCM overlay code, not an Android engine or mixer */
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#ifdef __linux__
#include <dirent.h>
#endif
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "android_mod_assets.h"
extern "C" {
#include "SDL.h"
#include "args.h"
#include "digi.h"
#include "gr.h"
#include "piggy.h"
#include "physfsx.h"
#include "android_log.h"
/* Platform adapters only: do not replace the loader, PhysFS, or fingerprints */
Arg GameArg{};
digi_sound GameSounds[MAX_SOUND_FILES]{};
int digi_sample_rate = 22050;
volatile int debug_log_enabled[DLOG_COUNT]{};
void debug_log(int, const char *, ...) {}
void android_sound_trace_loaded(int, const char *, long long, int) {}
int PHYSFSX_exists(const char *filename, int ignorecase)
{
	// The real ignorecase.c helper invokes this with ignorecase=0 only
	if (ignorecase) std::abort();
	return PHYSFS_exists(filename);
}
}
using nlohmann::json;
static int current_step = -1;
static void require(bool condition, const char *message)
{
	if (!condition) {
		std::fprintf(stderr, "FAIL step=%d: %s\n", current_step, message);
		std::exit(1);
	}
}
static std::vector<unsigned char> read_bytes(const std::string &path)
{
	std::ifstream in(path, std::ios::binary | std::ios::ate);
	require(in.good(), "independent expected PCM file opens");
	const auto size = in.tellg();
	require(size > 0 && size <= 16 * 1024 * 1024, "bounded independent expected PCM size");
	std::vector<unsigned char> bytes(static_cast<size_t>(size));
	in.seekg(0);
	in.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
	require(in.good(), "independent expected PCM read completes");
	return bytes;
}
static json resource_sample(size_t mounts)
{
	json sample = { { "step", current_step }, { "mounts", mounts } };
#ifdef __linux__
	std::ifstream status("/proc/self/status");
	std::string line;
	while (std::getline(status, line)) {
		if (line.compare(0, 6, "VmRSS:") == 0)
			sample["rss_kib"] = std::strtoull(line.c_str() + 6, nullptr, 10);
	}
	status.close();
	if (DIR *dir = opendir("/proc/self/fd")) {
		int count = -1; // Exclude this observation's own directory descriptor
		while (auto *entry = readdir(dir))
			if (entry->d_name[0] != '.') ++count;
		closedir(dir);
		sample["open_fds"] = count;
	}
#endif
	return sample;
}
int main(int argc, char **argv)
{
	if (argc != 3) return 2;
	std::ifstream input(argv[1]);
	const auto scenario = json::parse(input);
	require(PHYSFS_init(argv[0]), "PhysFS initializes");
	require(SDL_Init(SDL_INIT_TIMER) == 0, "SDL timer initializes");
	std::array<std::array<unsigned char, 31>, MAX_SOUND_FILES> stock{};
	std::array<digi_sound, MAX_SOUND_FILES> originals{};
	for (size_t i = 0; i < stock.size(); ++i) {
		for (size_t j = 0; j < stock[i].size(); ++j) stock[i][j] = static_cast<unsigned char>(i * 31 + j);
		originals[i].data = stock[i].data();
		originals[i].length = stock[i].size();
		originals[i].freq = 11025;
		originals[i].bits = 8;
		GameSounds[i] = originals[i];
	}
	debug_log_enabled[DLOG_GAME] = 1;
	std::vector<std::string> mounted;
	json result = { { "schema", 1 }, { "seed", scenario.at("seed") }, { "scope", "actual shared PCM loader with synthetic stock slots; invoked played callback, no mixer/GPU/save validation" }, { "steps", json::array() } };
	PHYSFS_Version linked{};
	PHYSFS_getLinkedVersion(&linked);
	require(linked.major == PHYSFS_VER_MAJOR && linked.minor == PHYSFS_VER_MINOR && linked.patch == PHYSFS_VER_PATCH,
	        "PhysFS runtime matches selected headers");
	result["physfs_runtime"] = { linked.major, linked.minor, linked.patch };
	unsigned loaded = 0, controls = 0;
	const bool record_usage = scenario.value("record_usage", true);
	result["resource_samples"] = json::array({ resource_sample(0) });
	for (const auto &step : scenario.at("steps")) {
		++current_step;
		if (step.value("unmount_before_reset", false)) {
			const auto resident_before = android_mod_usage_snapshot().at("sounds");
			for (const auto &path : mounted) require(PHYSFS_unmount(path.c_str()), "resident pack unmounts before reset");
			mounted.clear();
			require(android_mod_usage_snapshot().at("sounds") == resident_before,
			        "unmount alone preserves copied resident PCM and loaded owner");
		}
		android_mod_sounds_reset();
		for (size_t i = 0; i < stock.size(); ++i)
			require(GameSounds[i].data == originals[i].data && GameSounds[i].length == originals[i].length &&
			            GameSounds[i].freq == originals[i].freq && GameSounds[i].bits == originals[i].bits,
			        "reset restores stock data pointer, size, rate, and format");
		require(android_mod_usage_snapshot().at("sounds").empty(), "reset clears sound ownership");
		for (const auto &path : mounted) require(PHYSFS_unmount(path.c_str()), "previous pack unmounts");
		mounted.clear();
		// Scenario lists highest priority first, so append in that order
		for (const auto &mount : step.at("mounts")) {
			const auto path = mount.at("path").get<std::string>();
			const auto point = mount.at("point").get<std::string>();
			require(PHYSFS_mount(path.c_str(), point.empty() ? nullptr : point.c_str(), 1), "requested pack mounts");
			mounted.push_back(path);
		}
		GameArg.SndDisableSdlMixer = step.at("legacy").get<bool>();
		digi_sample_rate = step.at("rate");
#ifdef DXX_BUILD_DESCENT_II
		GameArg.SndDigiSampleRate = digi_sample_rate;
#endif
		int slot = 0;
		for (const auto &sample : step.at("samples")) {
			const auto name = sample.at("name").get<std::string>();
			android_mod_sound_apply(slot, name.c_str(), step.at("content_game"));
			auto &sound = GameSounds[slot];
			if (sample.at("expected").is_null()) {
				require(sound.data == originals[slot].data && sound.length == originals[slot].length,
				        "absent or wrong-game overlay preserves stock PCM");
			} else {
				auto expected = read_bytes(sample.at("expected"));
				const int source_rate = sample.at("source_rate");
				int rate = source_rate;
				if (GameArg.SndDisableSdlMixer && source_rate != digi_sample_rate) {
					// Independent rational nearest-neighbor oracle, integer arithmetic
					const auto source = expected;
					expected.resize(source.size() * digi_sample_rate / source_rate);
					for (size_t j = 0; j < expected.size(); ++j)
						expected[j] = source[j * static_cast<size_t>(source_rate) / digi_sample_rate];
					rate = digi_sample_rate;
				}
				require(sound.length == static_cast<int>(expected.size()) && sound.freq == rate && sound.bits == 8,
				        "resident PCM size/rate/format match independent payload");
				require(std::memcmp(sound.data, expected.data(), expected.size()) == 0,
				        "resident PCM equals independently extracted public payload");
				auto entry = android_mod_usage_snapshot().at("sounds").back();
				require(entry.at("sample") == slot && entry.at("source") == sample.at("owner") &&
				            entry.at("resident") == true && entry.at("played") == false,
				        "loaded owner and residency match independently selected pack");
				auto *pointer = sound.data;
				android_mod_sound_apply(slot, name.c_str(), step.at("content_game"));
				require(sound.data == pointer, "repeat apply does not replace resident storage");
				// Negative control: resident corruption must not be attributed as played
				sound.data[0] ^= 1;
				android_mod_sound_played(slot);
				entry = android_mod_usage_snapshot().at("sounds").back();
				require(entry.at("resident") == false && entry.at("played") == false,
				        "changed resident PCM fails fingerprint and played ownership");
				sound.data[0] ^= 1;
				debug_log_enabled[DLOG_GAME] = 0;
				android_mod_sound_played(slot);
				require(android_mod_usage_snapshot().at("sounds").empty(), "logging disabled hides evidence");
				debug_log_enabled[DLOG_GAME] = 1;
				require(android_mod_usage_snapshot().at("sounds").back().at("played") == false,
				        "logging disabled does not mark callback as played");
				android_mod_sound_played(slot);
				entry = android_mod_usage_snapshot().at("sounds").back();
				require(entry.at("resident") == true && entry.at("played") == true,
				        "restored resident callback attributes correct loaded owner");
				++loaded;
				controls += 2;
			}
			++slot;
			require(slot < MAX_SOUND_FILES, "bounded sample slots");
		}
		if (record_usage) result["steps"].push_back({ { "index", current_step }, { "label", step.at("label") }, { "content_game", step.at("content_game") }, { "samples", slot }, { "usage", android_mod_usage_snapshot().at("sounds") } });
		if ((current_step + 1) % 50 == 0) result["resource_samples"].push_back(resource_sample(mounted.size()));
	}
	android_mod_sounds_reset();
	for (size_t i = 0; i < stock.size(); ++i) require(GameSounds[i].data == originals[i].data, "final reset restores stock pointers");
	for (const auto &path : mounted) require(PHYSFS_unmount(path.c_str()), "final unmount");
	require(PHYSFS_deinit(), "PhysFS closes");
	SDL_Quit();
	result["resource_samples"].push_back(resource_sample(0));
	result["executed_steps"] = current_step + 1;
	result["loaded_samples"] = loaded;
	result["negative_controls"] = controls;
	result["status"] = "pass";
	std::ofstream out(argv[2]);
	out << result.dump(2) << '\n';
	require(out.good(), "write result");
	std::printf("PASS steps=%d loaded=%u negative_controls=%u\n", current_step + 1, loaded, controls);
}
