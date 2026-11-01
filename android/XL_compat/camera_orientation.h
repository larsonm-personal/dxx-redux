#ifndef DXX_XL_CAMERA_ORIENTATION_H
#define DXX_XL_CAMERA_ORIENTATION_H
#include "camera_splash.h"
#include <cmath>
namespace xl_compat
{
using CameraMatrix = std::array<FixedLightPoint, 3>;
// CFixMatrix::CreateFU retains source cross-product floor and normalization order
inline CameraMatrix camera_repair_orientation(FixedLightPoint forward, FixedLightPoint up)
{
	forward = geometry_default_direction(forward);
	if (forward == FixedLightPoint{}) throw std::runtime_error("Camera orientation has no forward direction");
	up = geometry_default_direction(up);
	FixedLightPoint right{};
	const auto fallback = [&] {
		if (!forward[0] && !forward[2]) {
			right = { 65536, 0, 0 };
			up = { 0, 0, forward[1] < 0 ? 65536 : -65536 };
		} else {
			right = geometry_default_direction({ forward[2], 0, -forward[0] });
			up = geometry_fixed_cross(forward, right);
		}
	};
	if (up == FixedLightPoint{}) fallback();
	right = geometry_default_direction(geometry_fixed_cross(up, forward));
	if (right == FixedLightPoint{}) fallback();
	up = geometry_fixed_cross(forward, right);
	return { right, up, forward };
}
inline CameraMatrix camera_orientation(const CameraMatrix &original, std::int16_t heading)
{
	const double angle = double(heading) * 2.0 * 3.141592653589793240 / 65536.0;
	const int sine = splash_fixed(std::sin(angle) * 65536.0), cosine = splash_fixed(std::cos(angle) * 65536.0);
	const CameraMatrix rotation{ { { cosine, 0, -sine }, { 0, 65536, 0 }, { sine, 0, cosine } } };
	CameraMatrix combined{};
	for (unsigned column = 0; column < 3; ++column)
		for (unsigned row = 0; row < 3; ++row)
			combined[column][row] = splash_fixed((double(original[0][row]) * rotation[column][0] +
			                                      double(original[1][row]) * rotation[column][1] + double(original[2][row]) * rotation[column][2]) /
			                                     65536.0);
	return camera_repair_orientation(combined[2], combined[1]);
}
} // namespace xl_compat
#endif
