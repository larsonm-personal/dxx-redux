#ifndef DXX_XL_RUNTIME_LIGHT_SOURCES_H
#define DXX_XL_RUNTIME_LIGHT_SOURCES_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "geometry.h"
#include "../companion_lighting.h"
#include <vector>
namespace xl_compat
{
struct ClassicGeometryLight {
	int segment, side;
	ClassicLightingSide geometry;
	ClassicFaceLight material;
	LightPoint emission_direction;
};
// Classic D1 full-color method-0 materials in compiled face order
// No light-table sorting, visibility, variable-light state or shading is implied
std::vector<ClassicGeometryLight> compile_classic_geometry_lights(const TextureBrightnessTable &brightness, const TextureColorTable &colors);
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
