#ifndef DXX_XL_CAMERA_SPLASH_H
#define DXX_XL_CAMERA_SPLASH_H
#include "lighting.h"
#include <cstdint>
#include <stdexcept>

namespace xl_compat
{
inline std::int32_t splash_fixed(double value)
{
	if (value < INT32_MIN || value > INT32_MAX) throw std::runtime_error("XL splash exceeds fixed-point bounds");
	return std::int32_t(value);
}
inline std::int32_t splash_multiply(std::int32_t a, std::int32_t b)
{
	return splash_fixed(double(b) / 65536.0 * double(a));
}
inline std::int32_t splash_divide(std::int32_t a, std::int32_t b)
{
	return splash_fixed(b ? double(a) / double(b) * 65536.0 : 1);
}
inline FixedLightPoint splash_difference(const FixedLightPoint &a, const FixedLightPoint &b)
{
	FixedLightPoint result;
	for (unsigned axis = 0; axis < 3; ++axis) result[axis] = splash_fixed(double(a[axis]) - b[axis]);
	return result;
}
// Source compares target radius but scales with weapon size, including FixDiv(0, 0) == 1
inline FixedLightPoint camera_splash_impact(const FixedLightPoint &impact, const FixedLightPoint &target, int target_radius, int weapon_size)
{
	auto relative = splash_difference(impact, target);
	const auto distance = geometry_default_magnitude(relative);
	if (distance == target_radius) return impact;
	FixedLightPoint result;
	for (unsigned axis = 0; axis < 3; ++axis)
		result[axis] = splash_fixed(double(target[axis]) + splash_divide(splash_multiply(relative[axis], weapon_size), distance));
	return result;
}
struct CameraSplashSample {
	int distance = 0, damage = 0, force = 0;
	FixedLightPoint impulse{};
	bool inside = false;
};
inline CameraSplashSample camera_splash_sample(const FixedLightPoint &position, const FixedLightPoint &impact, int max_damage, int radius, int max_force)
{
	if (radius <= 0 || max_damage < 0 || max_force < 0) throw std::runtime_error("Invalid camera splash strength/radius");
	CameraSplashSample result;
	const auto relative = splash_difference(position, impact);
	result.distance = geometry_default_magnitude(relative);
	if (result.distance >= radius) return result;
	result.inside = true;
	result.damage = max_damage - splash_fixed(double(result.distance) / double(radius) * double(max_damage));
	result.force = max_force - splash_fixed(double(result.distance) / double(radius) * double(max_force));
	const auto direction = geometry_default_direction(relative);
	for (unsigned axis = 0; axis < 3; ++axis) result.impulse[axis] = splash_multiply(direction[axis], result.force);
	return result;
}
} // namespace xl_compat
#endif
