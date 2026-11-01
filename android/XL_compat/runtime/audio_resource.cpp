#include "audio_resource.h"
#include "resource_validation.h"
#include "../named_sound.h"
#include "../scoped_sound.h"
#include "../wave.h"
#include "../classic_bank.h"
#include <SDL.h>
#include <memory>
#include <stdexcept>
#ifdef __ANDROID__
#include "android_mission_assets.h"
#endif
extern "C" {
#include "inferno.h"
#include "physfsx.h"
#include "mission.h"
}

namespace xl_compat
{
namespace
{
int incoming_ordinal(const char *level, int ordinal)
{
	if (!Current_mission || !level || !*level) throw std::runtime_error("XL sound folders require an active mission and level");
	if (ordinal) {
		if (ordinal > Last_level || ordinal < Last_secret_level ||
		    SDL_strcasecmp(level, ordinal < 0 ? Secret_level_names[-ordinal - 1] : Level_names[ordinal - 1]))
			throw std::runtime_error("XL sound folder ordinal does not match the incoming mission level");
		return ordinal;
	}
	int found = 0;
	for (int candidate = Last_secret_level; candidate <= Last_level; ++candidate) {
		if (!candidate) continue;
		const char *name = candidate < 0 ? Secret_level_names[-candidate - 1] : Level_names[candidate - 1];
		if (SDL_strcasecmp(level, name)) continue;
		if (found) throw std::runtime_error("XL sound folders require an explicit incoming level number for a repeated level filename");
		found = candidate;
	}
	if (!found) throw std::runtime_error("XL sound folders require a level listed in the selected mission descriptor");
	return found;
}

bool owned_path(std::string &path, bool archive, bool installation = false)
{
	if (PHYSFSEXT_locateCorrectCase(path.data()) != 0) return false;
	const char *origin = PHYSFS_getRealDir(path.c_str());
	bool owned = archive ? mission_owns_resource_archive(origin) != 0 : mission_owns_resource_directory(origin) != 0;
#ifdef __ANDROID__
	if (!archive && *android_mission_assets_key()) {
		char resolved[PATH_MAX];
		owned = android_mission_assets_find_owned_file(path.c_str(), resolved, sizeof(resolved)) != 0;
		if (owned) path = resolved;
	}
#endif
	if (!owned && !(installation && xl_resource_owns_installation(origin))) throw std::runtime_error(std::string("XL named-audio resource is shadowed outside the selected mission ") + (archive ? "HOG: " : "sound folders: ") + path);
	PHYSFS_Stat info;
	if (!PHYSFS_stat(path.c_str(), &info) || info.filetype != PHYSFS_FILETYPE_REGULAR)
		throw std::runtime_error("XL sound resource is not a regular file: " + path);
	return true;
}

AudioResource read_audio(std::string path, std::size_t limit = max_wave_bytes)
{
	std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFS_openRead(path.c_str()), PHYSFS_close);
	const auto length = file ? PHYSFS_fileLength(file.get()) : -1;
	if (length < 0 || std::uint64_t(length) > limit)
		throw std::runtime_error("Invalid bounded XL sound resource: " + path);
	std::string bytes(std::size_t(length), '\0');
	if (PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) != length)
		throw std::runtime_error("Truncated XL sound resource: " + path);
	return { std::move(path), std::move(bytes) };
}

std::string mission_sound_identity()
{
	std::string mission = mission_resource_archive();
	const auto slash = mission.find_last_of("/\\");
	if (slash != std::string::npos) mission.erase(0, slash + 1);
	const auto dot = mission.find_last_of('.');
	if (dot == std::string::npos || SDL_strcasecmp(mission.c_str() + dot, ".hog"))
		throw std::runtime_error("XL sound folders require an owned mission HOG identity");
	mission.resize(dot);
	return mission;
}
} // namespace

AudioResource load_audio_resource(const std::string &authored, const char *level, int ordinal)
{
	auto filename = named_sound_filename(authored);
	// Archive resources do not depend on an ordinal, including diagnostic loads
	if (owned_path(filename, true)) return read_audio(std::move(filename));
	const auto mission = mission_sound_identity();
	const auto paths = named_sound_paths(mission, authored, incoming_ordinal(level, ordinal));
	for (std::size_t i = 1; i < paths.size(); ++i) {
		auto path = paths[i];
		if (owned_path(path, false)) return read_audio(std::move(path));
	}
	auto addon = "sounds/d2x-xl/" + filename;
	if (owned_path(addon, false, true)) return read_audio(std::move(addon));
	throw std::runtime_error("Missing XL sound " + filename + "; checked the mission HOG, " + paths[1] + " and " + paths.back() + "; checked global addon " + addon + " in the owned startup installation");
}

std::optional<ClassicAudioScope> prepare_classic_audio_scope(const char *level, int ordinal, int bank_rate)
{
	if (!*mission_resource_archive()) return std::nullopt;
	const auto mission = mission_sound_identity();
	// A diagnostic translation with no mod folder needs no descriptor ordinal
	std::string root = "mods/" + mission;
	if (PHYSFSEXT_locateCorrectCase(root.data()) != 0) return std::nullopt;
	const int incoming = incoming_ordinal(level, ordinal);
	const std::string bank = "mods/" + mission + "/" + mission + (bank_rate >= 22050 ? ".s22" : ".s11");
	auto corrected_bank = bank;
	ClassicAudioScope scope{ mission, bank, incoming, bank_rate, {}, {} };
	if (owned_path(corrected_bank, false))
		scope.selected_bank = read_audio(std::move(corrected_bank), max_classic_bank_bytes);
	else {
		// CFile::Exist/Open fall back to the mission HOG after the mod folder
		auto archived_bank = mission + (bank_rate >= 22050 ? ".s22" : ".s11");
		if (owned_path(archived_bank, true)) scope.selected_bank = read_audio(std::move(archived_bank), max_classic_bank_bytes);
	}
	return scope;
}

std::optional<AudioResource> load_classic_audio_resource(const std::string &sample, ClassicAudioScope &scope)
{
	if (scope.selected_bank) return std::nullopt;
	const auto locate = [&](const std::string &candidate, bool archive) {
		const auto slash = candidate.find_last_of('/');
		const auto directory = slash == std::string::npos ? std::string{} : candidate.substr(0, slash);
		auto [entry, inserted] = scope.directories.try_emplace(directory);
		if (inserted) {
			auto corrected = directory;
			if (corrected.empty() || PHYSFSEXT_locateCorrectCase(corrected.data()) == 0) {
				std::unique_ptr<char *, decltype(&PHYSFS_freeList)> names(PHYSFS_enumerateFiles(corrected.c_str()), PHYSFS_freeList);
				if (!names) throw std::runtime_error("Cannot enumerate XL sound folder: " + corrected);
				for (char **name = names.get(); *name; ++name) {
					std::string lower = *name;
					SDL_strlwr(lower.data());
					entry->second.insert(std::move(lower));
				}
			}
		}
		auto filename = slash == std::string::npos ? candidate : candidate.substr(slash + 1);
		SDL_strlwr(filename.data());
		if (!entry->second.count(filename)) return std::string{};
		auto path = candidate;
		return owned_path(path, archive) ? path : std::string{};
	};
	const auto selected = resolve_scoped_sound(scope.mission, sample, scope.ordinal, scope.rate,
	                                           [&](const std::string &candidate) {
		                                           // Bank selection was captured before walking any native names
		                                           if (candidate == scope.bank || candidate == scope.bank.substr(scope.bank.find_last_of('/') + 1)) return std::string{};
		                                           return locate(candidate, candidate.find('/') == std::string::npos);
	                                           });
	if (selected.path.empty()) return std::nullopt;
	return read_audio(selected.path);
}
} // namespace xl_compat
