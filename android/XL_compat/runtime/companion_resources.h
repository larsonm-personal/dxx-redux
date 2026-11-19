#ifndef DXX_XL_COMPANION_RESOURCES_H
#define DXX_XL_COMPANION_RESOURCES_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "../companion_lighting.h"
#include <optional>
#include <map>
#include <string>

namespace xl_compat
{
struct MissionLightingCompanions {
	std::string archive;
	std::optional<TextureBrightnessTable> brightness;
	std::optional<TextureColorTable> colors;
	std::map<std::string, std::string> sources;
};
// Selected-HOG resources only; loose/context-root companions are not qualified
// A foreign shadow or malformed present resource throws rather than falling back
class TargetSources;
MissionLightingCompanions load_mission_lighting_companions(const std::string &level, TargetSources *target = nullptr);
struct StartupTextureColors {
	std::string source, hash;
	TextureColorTable colors;
};
std::optional<StartupTextureColors> read_startup_texture_colors();
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
