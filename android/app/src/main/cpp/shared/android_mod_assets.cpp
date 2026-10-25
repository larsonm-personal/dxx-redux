/* Android optional PCM overlays and successful asset-use diagnostics */
#include "android_mod_assets.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cstdio>
#include <climits>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <SDL.h>
#if defined(DXX_XL_PILOT) && defined(DXX_BUILD_DESCENT_II)
#include "../../../../../XL_compat/scoped_sound.h"
#include "../../../../../XL_compat/wave.h"
#include "android_mission_assets.h"
#endif
extern "C" {
#include "args.h"
#include "digi.h"
#include "gr.h"
#include "piggy.h"
#include "physfsx.h"
#include "android_log.h"
#include "android_sound_trace.h"
#include "sound_trace_fingerprint.h"
#if defined(DXX_XL_PILOT) && defined(DXX_BUILD_DESCENT_II)
#include "mission.h"
#include "error.h"
#endif
}

namespace
{
struct sound_overlay {
	digi_sound original{};
	std::vector<unsigned char> bytes;
	std::string path, origin;
	sound_trace_fingerprint fingerprint{};
	bool played = false;
};
std::array<sound_overlay, MAX_SOUND_FILES> sounds;
nlohmann::json presentation = nlohmann::json::array();
bool dirty = true;
Uint32 last_flush;
#if defined(DXX_XL_PILOT) && defined(DXX_BUILD_DESCENT_II)
int scoped_ordinal;
unsigned scoped_generation;
std::string scoped_archive, scoped_identity, scoped_key;
#endif

std::string real_path(const char *filename)
{
	char corrected[PATH_MAX];
	std::snprintf(corrected, sizeof(corrected), "%s", filename);
	PHYSFSEXT_locateCorrectCase(corrected);
	return corrected;
}

std::string origin_of(const std::string &path)
{
	const char *origin = PHYSFS_getRealDir(path.c_str());
	return origin ? origin : "";
}

std::string hash_text(sound_trace_fingerprint fingerprint)
{
	char value[17];
	std::snprintf(value, sizeof(value), "%016llx", (unsigned long long) fingerprint.hash);
	return value;
}

int select_sound_source(const std::string (&origins)[2], const std::string &bank_origin)
{
	int selected = -1;
	char **search = PHYSFS_getSearchPath();
	if (search) {
		for (char **p = search; *p && selected == -1; ++p) {
			for (int i = 0; i < 2; ++i)
				if (!origins[i].empty() && origins[i] == *p) {
					selected = i;
					break;
				}
			if (selected == -1 && !bank_origin.empty() && bank_origin == *p) selected = -2;
		}
		PHYSFS_freeList(search);
	}
	return selected;
}
} // namespace

#if defined(DXX_XL_PILOT) && defined(DXX_BUILD_DESCENT_II)
void android_mod_sound_level_prepare(int ordinal)
{
	scoped_ordinal = ordinal;
	scoped_generation = android_mission_assets_generation();
	scoped_key = android_mission_assets_key();
	scoped_archive = mission_resource_archive();
	scoped_identity.clear();
	if (scoped_key.empty() || scoped_archive.empty()) return;
	const auto slash = scoped_archive.find_last_of("/\\");
	auto name = scoped_archive.substr(slash == std::string::npos ? 0 : slash + 1);
	const auto dot = name.find_last_of('.');
	if (dot == std::string::npos || SDL_strcasecmp(name.c_str() + dot, ".hog")) return;
	scoped_identity = name.substr(0, dot);
}

// Imported D1 installation rebuilds the sample registry for each incoming level
static bool apply_scoped_d1_sound(int sample, const char *name)
{
	if (scoped_identity.empty() || !scoped_ordinal ||
	    scoped_generation != android_mission_assets_generation() ||
	    scoped_key != android_mission_assets_key() || scoped_archive != mission_resource_archive()) return false;
	try {
		const auto selected = xl_compat::resolve_scoped_sound(scoped_identity, name, scoped_ordinal,
		                                                      GameArg.SndDigiSampleRate, [](const std::string &path) {
			                                                      char resolved[PATH_MAX];
			                                                      return android_mission_assets_find_owned_file(path.c_str(), resolved, sizeof(resolved)) ? std::string(resolved) : std::string{};
		                                                      });
		if (selected.path.empty()) return false;
		if (selected.bank) throw std::runtime_error("Mission sound bank requires unsupported scoped-bank decoding: " + selected.path);
		PHYSFS_file *file = PHYSFS_openRead(selected.path.c_str());
		if (!file) throw std::runtime_error("Cannot open owned WAV: " + selected.path);
		const auto length = PHYSFS_fileLength(file);
		if (length <= 0 || length > xl_compat::max_wave_bytes) {
			PHYSFS_close(file);
			throw std::runtime_error("Invalid owned WAV size: " + selected.path);
		}
		std::vector<unsigned char> source(static_cast<size_t>(length));
		const bool complete = PHYSFS_readBytes(file, source.data(), length) == length;
		PHYSFS_close(file);
		if (!complete) throw std::runtime_error("Incomplete owned WAV: " + selected.path);
		const auto pcm = xl_compat::decode_wave(source.data(), source.size());
		// Both Redux backends consume unsigned 8-bit source PCM
		const int rate = GameArg.SndDisableSdlMixer ? GameArg.SndDigiSampleRate : static_cast<int>(pcm.rate);
		auto bytes = xl_compat::wave_unsigned8(pcm, rate);
		auto &entry = sounds[sample];
		auto &sound = GameSounds[sample];
		entry.original = sound;
		entry.bytes = std::move(bytes);
		sound.data = entry.bytes.data();
		sound.length = static_cast<unsigned int>(entry.bytes.size());
		sound.bits = 8;
		sound.freq = rate;
		piggy_sound_duration_override(sample, static_cast<int>(pcm.mono.size()), pcm.rate);
		android_mod_sound_loaded(sample, selected.path.c_str());
		debug_log(DLOG_GAME, "[XL] scoped sound=%d scope=%s ordinal=%d frames=%zu source_rate=%u output_rate=%d", sample, selected.scope.c_str(), scoped_ordinal, pcm.mono.size(), pcm.rate, rate);
		return true;
	} catch (const std::exception &e) {
		Error("Cannot load mission sound %s: %s", name, e.what());
		return true;
	}
}
#endif

void android_mod_sounds_reset(void)
{
	for (size_t i = 0; i < sounds.size(); ++i) {
		auto &entry = sounds[i];
		if (!entry.bytes.empty() && GameSounds[i].data == entry.bytes.data())
			GameSounds[i] = entry.original;
#ifdef DXX_BUILD_DESCENT_II
		piggy_sound_duration_override(i, -1, 0);
#endif
		entry = sound_overlay{};
	}
	dirty = true;
}

void android_mod_sound_loaded(int sample, const char *filename)
{
	if (sample < 0 || sample >= MAX_SOUND_FILES) return;
	auto &entry = sounds[sample];
	entry.path = real_path(filename);
	entry.origin = origin_of(entry.path);
	entry.fingerprint = sound_trace_fingerprint_bytes(GameSounds[sample].data, GameSounds[sample].length);
	entry.played = false;
	dirty = true;
	android_sound_trace_loaded(sample, filename, 0, 1);
	debug_log(DLOG_GAME, "[MOD] sound=%d file='%s' source='%s' bytes=%zu rate=%d hash=%s",
	          sample, entry.path.c_str(), entry.origin.c_str(), entry.fingerprint.length,
	          GameSounds[sample].freq, hash_text(entry.fingerprint).c_str());
}

void android_mod_sound_bank_loaded(int sample, const char *filename)
{
	const auto origin = origin_of(real_path(filename));
	// Stock banks are not mods; record samples supplied by a DXA sound bank
	if (origin.size() >= 4 && SDL_strcasecmp(origin.c_str() + origin.size() - 4, ".dxa") == 0)
		android_mod_sound_loaded(sample, filename);
}

#ifdef DXX_BUILD_DESCENT_II
void android_mod_sound_timing_apply(int sample)
{
	const char *name = piggy_sound_duration_name(sample);
	if (!name || !*name) return;
	// Numeric HAM references and loose PCM names come from the canonical bank
	std::string paths[] = { real_path((std::string("Sounds/") + name + ".r22").c_str()),
		                    real_path((std::string("Sounds/") + name + ".raw").c_str()) };
	std::string origins[] = { origin_of(paths[0]), origin_of(paths[1]) };
	const int selected = select_sound_source(origins, origin_of(piggy_sound_duration_bank()));
	if (selected < 0) return;
	PHYSFS_file *metadata = PHYSFS_openRead(paths[selected].c_str());
	if (!metadata) return;
	const PHYSFS_sint64 length = PHYSFS_fileLength(metadata);
	PHYSFS_close(metadata);
	// Timing is an authored property even if the playback backend cannot convert PCM
	if (length > 0 && length <= SOUND_TRACE_MAX_BYTES)
		piggy_sound_duration_override(sample, static_cast<int>(length), selected == 0 ? 22050 : 11025);
}
#endif

void android_mod_sound_apply(int sample, const char *name, int content_game)
{
	if (sample < 0 || sample >= MAX_SOUND_FILES || !name || !*name) return;
	auto &entry = sounds[sample];
	auto &sound = GameSounds[sample];
	if (!entry.bytes.empty()) return;
#if defined(DXX_XL_PILOT) && defined(DXX_BUILD_DESCENT_II)
	if (content_game == 1 && apply_scoped_d1_sound(sample, name)) return;
#endif
#ifndef DXX_BUILD_DESCENT_II
	if (!sound.data || sound.data == (void *) -1 || !sound.length) return;
#endif
	const char *prefix = "";
#ifdef DXX_BUILD_DESCENT_II
	if (content_game == 1) prefix = "d1-mods/";
#else
	(void) content_game;
#endif
	// Archive priority wins across formats; prefer 22 kHz within the same archive
	std::string paths[] = { real_path((std::string(prefix) + "Sounds/" + name + ".r22").c_str()),
		                    real_path((std::string(prefix) + "Sounds/" + name + ".raw").c_str()) };
	std::string origins[] = { origin_of(paths[0]), origin_of(paths[1]) };
#ifdef DXX_BUILD_DESCENT_II
	if (content_game == 2) android_mod_sound_timing_apply(sample);
#endif
	const int selected = select_sound_source(origins, entry.origin);
	if (selected < 0) return;
	PHYSFS_file *file = PHYSFS_openRead(paths[selected].c_str());
	if (!file) return;
	const PHYSFS_sint64 length = PHYSFS_fileLength(file);
	if (length <= 0 || length > SOUND_TRACE_MAX_BYTES) {
		PHYSFS_close(file);
		debug_log(DLOG_GAME, "[MOD] rejected PCM file='%s' bytes=%lld", paths[selected].c_str(), (long long) length);
		return;
	}
#ifdef DXX_BUILD_DESCENT_II
	// Timing was read independently above, including low-memory omitted samples
	if (!sound.data || sound.data == (void *) -1 || !sound.length) {
		PHYSFS_close(file);
		return;
	}
#endif
	std::vector<unsigned char> bytes(static_cast<size_t>(length));
	const bool complete = PHYSFS_readBytes(file, bytes.data(), length) == length;
	PHYSFS_close(file);
	if (!complete) return;
	int rate = selected == 0 ? 22050 : 11025;
#ifdef DXX_BUILD_DESCENT_II
	const int output_rate = GameArg.SndDigiSampleRate;
#else
	const int output_rate = digi_sample_rate;
#endif
	// The legacy SDL audio backend has a fixed sample rate
	if (GameArg.SndDisableSdlMixer && rate != output_rate) {
		const size_t count = bytes.size() * output_rate / rate;
		if (!count || count > SOUND_TRACE_MAX_BYTES) return;
		std::vector<unsigned char> converted(count);
		for (size_t i = 0; i < count; ++i)
			converted[i] = bytes[(std::min) (bytes.size() - 1, i * rate / output_rate)];
		bytes = std::move(converted);
		rate = output_rate;
	}
	entry.original = sound;
	entry.bytes = std::move(bytes);
	sound.data = entry.bytes.data();
	sound.length = static_cast<unsigned int>(entry.bytes.size());
	sound.bits = 8;
	sound.freq = rate;
	android_mod_sound_loaded(sample, paths[selected].c_str());
	android_sound_trace_loaded(sample, name, 0, 1);
}

void android_mod_sound_played(int sample)
{
	if (!debug_log_enabled[DLOG_GAME] || sample < 0 || sample >= MAX_SOUND_FILES) return;
	auto &entry = sounds[sample];
	if (entry.path.empty() || entry.played) return;
	const auto current = sound_trace_fingerprint_bytes(GameSounds[sample].data, GameSounds[sample].length);
	if (sound_trace_fingerprint_match(current, entry.fingerprint) != 1) return;
	entry.played = true;
	dirty = true;
	debug_log(DLOG_GAME, "[MOD] played sound=%d file='%s' source='%s' hash=%s",
	          sample, entry.path.c_str(), entry.origin.c_str(), hash_text(current).c_str());
}

void android_mod_asset_loaded(const char *kind, const char *filename, int width, int height)
{
	const auto path = real_path(filename);
	const auto origin = origin_of(path);
	if (origin.empty()) return;
	nlohmann::json asset = { { "kind", kind }, { "file", path }, { "source", origin }, { "width", width }, { "height", height } };
	if (std::find(presentation.begin(), presentation.end(), asset) != presentation.end()) return;
	presentation.push_back(std::move(asset));
	dirty = true;
	debug_log(DLOG_GAME, "[MOD] loaded %s='%s' source='%s' size=%dx%d", kind, path.c_str(), origin.c_str(), width, height);
}

nlohmann::json android_mod_usage_snapshot()
{
	nlohmann::json entries = nlohmann::json::array();
	// On-demand introspection must also respect the user's logging switch
	if (!debug_log_enabled[DLOG_GAME])
		return { { "schema", 1 }, { "enabled", false }, { "sounds", entries }, { "presentation", entries } };
	for (size_t i = 0; i < sounds.size(); ++i) {
		const auto &entry = sounds[i];
		if (entry.path.empty()) continue;
		const auto current = sound_trace_fingerprint_bytes(GameSounds[i].data, GameSounds[i].length);
		entries.push_back({ { "sample", i }, { "file", entry.path }, { "source", entry.origin }, { "bytes", entry.fingerprint.length }, { "hash", hash_text(entry.fingerprint) }, { "rate", GameSounds[i].freq }, { "played", entry.played }, { "resident", sound_trace_fingerprint_match(current, entry.fingerprint) == 1 } });
	}
	return { { "schema", 1 }, { "enabled", true }, { "sounds", std::move(entries) }, { "presentation", presentation } };
}

void android_mod_usage_flush(void)
{
	if (!debug_log_enabled[DLOG_GAME] || !dirty || SDL_GetTicks() - last_flush < 1000) return;
	const char *dir = PHYSFS_getWriteDir();
	if (!dir) return;
	// Schema and filename are shared with ModUsage.kt
	const std::string target = std::string(dir) + "/mod_usage.json";
	const std::string temporary = target + ".tmp";
	FILE *file = std::fopen(temporary.c_str(), "wb");
	if (!file) return;
	const auto json = android_mod_usage_snapshot().dump(2) + "\n";
	const bool written = std::fwrite(json.data(), 1, json.size(), file) == json.size();
	const bool closed = std::fclose(file) == 0;
	if (written && closed && std::rename(temporary.c_str(), target.c_str()) == 0) dirty = false;
	last_flush = SDL_GetTicks();
}
