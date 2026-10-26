#include "companion_resources.h"
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
std::optional<std::vector<std::uint8_t>> read_owned(std::string name, std::size_t expected)
{
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

MissionLightingCompanions load_mission_lighting_companions(const std::string &level)
{
	MissionLightingCompanions result;
	result.archive = mission_resource_archive();
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
	if (PHYSFSEXT_locateCorrectCase(corrected.data()) != 0 || !mission_owns_resource_archive(PHYSFS_getRealDir(corrected.c_str())))
		throw std::runtime_error("Lighting companion level is not owned by the selected mission HOG: " + level);
	const auto stem = corrected.substr(0, corrected.find_last_of('.'));
	if (const auto bytes = read_owned(stem + ".lgt", companion_texture_count * 4))
		result.brightness = decode_texture_brightness(bytes->data(), bytes->size());
	if (const auto bytes = read_owned(stem + ".clr", companion_texture_count * 13))
		result.colors = decode_texture_colors(bytes->data(), bytes->size());
	return result;
}
} // namespace xl_compat
