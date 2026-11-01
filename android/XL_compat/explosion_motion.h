#ifndef DXX_XL_EXPLOSION_MOTION_H
#define DXX_XL_EXPLOSION_MOTION_H
#include "camera_splash.h"
namespace xl_compat
{
inline FixedLightPoint explosion_motion(const FixedLightPoint &origin, const FixedLightPoint &viewer, int distance, int duration, int remaining)
{
	if (!duration) return origin;
	const auto direction = geometry_default_direction(splash_difference(viewer, origin));
	const int offset = splash_fixed(double(distance) * (double(duration) - remaining) / double(duration));
	FixedLightPoint result;
	for (unsigned axis = 0; axis < 3; ++axis) result[axis] = splash_fixed(double(origin[axis]) + splash_multiply(direction[axis], offset));
	return result;
}
} // namespace xl_compat
#endif
