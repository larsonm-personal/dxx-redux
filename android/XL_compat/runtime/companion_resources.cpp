#include "companion_resources.h"
#include "resource_validation.h"
#include "target_sources.h"
#include "retained_resource.h"
#include "picosha2.h"
#include "../hog.h"
#include <memory>
#include <stdexcept>
#include <vector>
#include <cctype>
extern "C" {
#include "physfsx.h"
#include "mission.h"
}

namespace xl_compat
{
namespace
{
std::optional<std::vector<std::uint8_t>> read_owned(std::string name, std::size_t expected, TargetSources *target)
{
	if (target) {
		auto bytes = target->archive_bytes(name, expected);
		if (bytes && bytes->size() != expected) throw std::runtime_error("Invalid incoming lighting companion length: " + name);
		return bytes;
	}
	if (PHYSFSEXT_locateCorrectCase(name.data()) != 0) return std::nullopt;
	const char *origin = PHYSFS_getRealDir(name.c_str());
	if (!mission_owns_resource_archive(origin))
		throw std::runtime_error("Lighting companion is shadowed outside the selected mission HOG: " + name);
	using File = std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)>;
	File file(PHYSFS_openRead(name.c_str()), PHYSFS_close);
	if (!file || PHYSFS_fileLength(file.get()) != static_cast<PHYSFS_sint64>(expected))
		throw std::runtime_error("Invalid selected-HOG lighting companion length: " + name);
	std::vector<std::uint8_t> bytes(expected);
	if (PHYSFS_readBytes(file.get(), bytes.data(), expected) != static_cast<PHYSFS_sint64>(expected))
		throw std::runtime_error("Incomplete selected-HOG lighting companion: " + name);
	return bytes;
}
} // namespace

MissionLightingCompanions load_mission_lighting_companions(const std::string &level, TargetSources *target)
{
	MissionLightingCompanions result;
	result.archive = target ? target->archive() : mission_resource_archive();
	if (result.archive.empty()) return result;
	// HOG members are flat, and this API must not interpret arbitrary paths
	if (level.empty() || level.find_first_of("/\\:") != std::string::npos || level.find("..") != std::string::npos)
		throw std::runtime_error("Invalid level identity for mission lighting companions");
	const auto dot = level.find_last_of('.');
	std::string extension = dot == std::string::npos ? "" : level.substr(dot);
	for (auto &c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	if (extension != ".rdl" && extension != ".rl2")
		throw std::runtime_error("Lighting companions require an RDL or RL2 level identity");
	auto corrected = level;
	if (target ? !target->archive_contains(level) : (PHYSFSEXT_locateCorrectCase(corrected.data()) != 0 || !mission_owns_resource_archive(PHYSFS_getRealDir(corrected.c_str()))))
		throw std::runtime_error("Lighting companion level is not owned by the selected mission HOG: " + level);
	const auto stem = corrected.substr(0, corrected.find_last_of('.'));
	if (const auto bytes = read_owned(stem + ".lgt", companion_texture_count * 4, target)) {
		result.brightness = decode_texture_brightness(bytes->data(), bytes->size());
		result.sources.emplace(hog_key(stem + ".lgt"), picosha2::hash256_hex_string(*bytes));
	}
	if (const auto bytes = read_owned(stem + ".clr", companion_texture_count * 13, target)) {
		result.colors = decode_texture_colors(bytes->data(), bytes->size());
		result.sources.emplace(hog_key(stem + ".clr"), picosha2::hash256_hex_string(*bytes));
	}
	return result;
}

std::optional<StartupTextureColors> read_startup_texture_colors()
{
	// PiggyInit resolves the edition defaults once, before any mission HOG is mounted
	constexpr auto size = companion_texture_count * 13;
	for (std::string name : { "data/descent2.clr", "descent2.clr", "data/d2x-xl.hog", "d2x-xl.hog" }) {
		if (PHYSFSEXT_locateCorrectCase(name.data()) != 0) continue;
		const char *origin = PHYSFS_getRealDir(name.c_str());
		if (!xl_resource_owns_installation(origin)) throw std::runtime_error("D2 default colors are shadowed outside startup data: " + name);
		using File = std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)>;
		File file(PHYSFS_openRead(name.c_str()), PHYSFS_close);
		const bool archive = name.compare(name.size() - 4, 4, ".hog") == 0;
		if (!archive && (!file || PHYSFS_fileLength(file.get()) != size)) throw std::runtime_error("Invalid startup texture color length: " + name);
		auto bytes = xl_retain_resource(file.get());
		std::string identity = "descent2.clr";
		if (archive) {
			auto member = xl_find_archive_resource(bytes, ".hog", "descent2.clr", size);
			if (!member) throw std::runtime_error("Startup XL addon lacks descent2.clr");
			bytes = std::move(*member);
			identity = "d2x-xl.hog/descent2.clr";
		}
		return StartupTextureColors{ identity, picosha2::hash256_hex_string(bytes), decode_texture_colors(bytes.data(), bytes.size()) };
	}
	return std::nullopt;
}
} // namespace xl_compat
