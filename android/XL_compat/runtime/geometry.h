#ifndef DXX_XL_RUNTIME_GEOMETRY_H
#define DXX_XL_RUNTIME_GEOMETRY_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "../lighting.h"
namespace xl_compat
{
// Uses the inherited integer square-root routine; reference comparison remains required
FixedLightPoint geometry_fixed_normal(const FixedLightPoint &a, const FixedLightPoint &b, const FixedLightPoint &c);
FixedLightPoint geometry_fixed_direction(const FixedLightPoint &direction);
struct ClassicLightingSide {
	GeometryLightFace face;
	std::array<FixedLightPoint, 2> fixed_normals;
	FixedLightPoint center, segment_center;
	LightPoint average_normal;
	bool split_13;
};
// Compile a four-corner classic side for lighting without changing collision geometry
ClassicLightingSide compile_classic_lighting_side(int segment, int side);
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
