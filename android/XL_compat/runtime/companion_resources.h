#ifndef DXX_XL_COMPANION_RESOURCES_H
#define DXX_XL_COMPANION_RESOURCES_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "../companion_lighting.h"
#include <optional>
#include <string>

namespace xl_compat
{
struct MissionLightingCompanions {
	std::string archive;
	std::optional<TextureBrightnessTable> brightness;
	std::optional<TextureColorTable> colors;
};
// Selected-HOG resources only; loose/context-root companions are not qualified
// A foreign shadow or malformed present resource throws rather than falling back
MissionLightingCompanions load_mission_lighting_companions(const std::string &level);
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
