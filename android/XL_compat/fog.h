#ifndef DXX_XL_FOG_H
#define DXX_XL_FOG_H
#include "inventory.h"
#include <algorithm>
#include <stdexcept>

namespace xl_compat
{
using FogColor = std::array<float, 4>;
using FogColors = std::array<FogColor, 4>;

inline int fog_type(int properties)
{
	return properties & segment_water ? 1 : properties & segment_lava    ? 2
	                                    : properties & segment_light_fog ? 3
	                                    : properties & segment_dense_fog ? 4
	                                                                     : 0;
}

inline int fog_type(const Inventory &level, int segment)
{
	return segment >= 0 && std::size_t(segment) < level.segments.size() ? fog_type(segment_property_bits(level.level_version, level.segments[segment])) : 0;
}

inline FogColors fog_colors(const Inventory &level)
{
	FogColors colors{ { { .2f, .4f, .6f, 160.f }, { .8f, .4f, 0.f, 60.f }, { .7f, .7f, .7f, 240.f }, { .7f, .7f, .7f, 100.f } } };
	if (level.level_version < 27) return colors;
	if (level.fog_settings.size() != colors.size()) throw std::runtime_error("Authored fog requires four complete color/distance records");
	for (std::size_t i = 0; i < colors.size(); ++i) {
		const auto &bytes = level.fog_settings[i];
		for (int value : bytes)
			if (value < 0 || value > 255) throw std::runtime_error("Authored fog record exceeds unsigned byte range");
		colors[i] = { bytes[0] / 255.f, bytes[1] / 255.f, bytes[2] / 255.f, bytes[3] * 20.f };
	}
	return colors;
}

// Existing pre-27 water profiles retain their qualified non-volumetric option
inline bool volumetric_fog_required(const Inventory &level)
{
	return std::any_of(level.segments.begin(), level.segments.end(), [&](const Segment &segment) {
		const int properties = segment_property_bits(level.level_version, segment);
		return (level.level_version >= 27 && fog_type(properties)) || (properties & (segment_light_fog | segment_dense_fog));
	});
}

struct FogBoundary {
	int type = 0, segment = -1, side = -1;
	bool reversed = false;
};

// Source RenderFogFaces draws the owner's far cap and its neighbor's near cap
// Equal-type portals are internal. An external boundary reverses culling for near
inline FogBoundary fog_boundary(const Inventory &level, int segment, int side, bool near_cap)
{
	if (segment < 0 || std::size_t(segment) >= level.segments.size() || side < 0 || side >= 6) return {};
	const int type = fog_type(level, segment);
	const int child = level.segments[segment].children[side];
	if (!type || fog_type(level, child) == type) return {};
	if (!near_cap) return { type, segment, side, false };
	if (child < 0 || std::size_t(child) >= level.segments.size()) return { type, segment, side, true };
	for (int other = 0; other < 6; ++other)
		if (level.segments[child].children[other] == segment) return { type, child, other, false };
	return {};
}
} // namespace xl_compat
#endif
