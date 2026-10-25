#ifndef DXX_XL_RUNTIME_GEOMETRY_H
#define DXX_XL_RUNTIME_GEOMETRY_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "../lighting.h"
namespace xl_compat
{
// Side-normal setup retains legacy integer magnitude, as in XL SetupSegments
FixedLightPoint geometry_fixed_normal(const FixedLightPoint &a, const FixedLightPoint &b, const FixedLightPoint &c);
// Post-setup camera and distance operations use the default math format 2
FixedLightPoint geometry_fixed_direction(const FixedLightPoint &direction);
// Pinned CFixMatrix::CreateF, ordered right/up/forward; no aspect or zoom scaling
std::array<FixedLightPoint, 3> geometry_camera_basis(const FixedLightPoint &forward);
std::int32_t geometry_fixed_distance(const FixedLightPoint &a, const FixedLightPoint &b);
struct ClassicLightingSide {
	GeometryLightFace face;
	std::array<FixedLightPoint, 2> fixed_normals;
	FixedLightPoint center, segment_center;
	LightPoint average_normal;
	std::int32_t outer_radius = 0;
	bool split_13;
};
// Compile a four-corner classic side for lighting without changing collision geometry
ClassicLightingSide compile_classic_lighting_side(int segment, int side);
struct ClassicVertexNormal {
	LightPoint normal{};
	unsigned contributions = 0;
};
// All classic side corners in segment/side order, including portal faces
// Preserves CRenderNormal's doubled uint8 face counter before normalization
std::vector<ClassicVertexNormal> compile_classic_vertex_normals();
// Default classic setup tolerance is trunc(65536 * 0.001) = 65
std::int32_t classic_segment_average_radius(int segment);
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
