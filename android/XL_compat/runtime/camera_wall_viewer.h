#ifndef DXX_XL_CAMERA_WALL_VIEWER_H
#define DXX_XL_CAMERA_WALL_VIEWER_H
#include "geometry.h"
#include "motion_math.h"
extern "C" {
#include "object.h"
#include "segment.h"
}
namespace xl_compat
{
// CCamera::Setup owns a private zero-initialized viewer for ordinary wall cameras
inline object camera_wall_viewer(int segment, int side)
{
	if (segment < 0 || segment >= Num_segments || side < 0 || side >= 6)
		throw std::runtime_error("Wall camera source face is outside the loaded level");
	const auto geometry = compile_classic_lighting_side(segment, side);
	const auto &normal = geometry.fixed_normals[0];
	if (normal == FixedLightPoint{}) throw std::runtime_error("Wall camera source face has no normal");
	object viewer{};
	viewer.orient = teleport_matrix(teleport_angles({ normal[0], normal[1], normal[2] }));
	viewer.pos = { geometry.center[0], geometry.center[1], geometry.center[2] };
	viewer.segnum = segment;
	return viewer;
}
} // namespace xl_compat
#endif
