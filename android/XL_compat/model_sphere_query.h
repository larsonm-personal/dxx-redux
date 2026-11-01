#ifndef DXX_XL_MODEL_SPHERE_QUERY_H
#define DXX_XL_MODEL_SPHERE_QUERY_H
#include "model_hitbox_query.h"
namespace xl_compat
{
// Source FLOAT_COLLISION_MATH=1 path; -1 is a miss, zero is a valid result
inline int32_t model_sphere_query(FixedLightPoint &hit, const FixedLightPoint &start,
                                  const FixedLightPoint &end, const FixedLightPoint &center, int32_t radius)
{
	using Vector = std::array<float, 3>;
	const auto convert = [](const FixedLightPoint &p) { return Vector{ p[0] / 65536.0f, p[1] / 65536.0f, p[2] / 65536.0f }; };
	const auto subtract = [](const Vector &a, const Vector &b) { return Vector{ a[0] - b[0], a[1] - b[1], a[2] - b[2] }; };
	const auto magnitude = [](const Vector &p) { return float(::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2])); };
	const auto fixed = [](float value) { return hitbox_fixed(value * 65536.0f); };
	const auto origin = convert(start), sphere = convert(center);
	const auto relative = subtract(sphere, origin);
	auto direction = subtract(convert(end), origin);
	const float length = magnitude(direction), r = radius / 65536.0f;
	Vector intersection = origin;
	float distance;
	if (length == 0) {
		distance = magnitude(relative);
		if (!(distance < r)) return -1;
	} else {
		for (auto &axis : direction) axis /= length;
		const float projection = direction[0] * relative[0] + direction[1] * relative[1] + direction[2] * relative[2];
		if (projection < 0 || projection > length + r) return -1;
		for (unsigned axis = 0; axis < 3; ++axis) intersection[axis] = origin[axis] + direction[axis] * projection;
		const float perpendicular = magnitude(subtract(intersection, sphere));
		if (!(perpendicular < r)) return -1;
		// Match the source's global sqrt overload, including intermediate precision
		distance = float(projection - ::sqrt(r * r - perpendicular * perpendicular));
		if (distance > length) return -1;
		if (distance < 0) {
			intersection = origin;
			distance = magnitude(relative);
		} else
			for (unsigned axis = 0; axis < 3; ++axis) intersection[axis] = origin[axis] + direction[axis] * distance;
	}
	hit = { fixed(intersection[0]), fixed(intersection[1]), fixed(intersection[2]) };
	return fixed(distance);
}
} // namespace xl_compat
#endif
