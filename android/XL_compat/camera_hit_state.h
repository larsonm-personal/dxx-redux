#ifndef DXX_XL_CAMERA_HIT_STATE_H
#define DXX_XL_CAMERA_HIT_STATE_H
#include "model_hitbox_query.h"
namespace xl_compat
{
// Default nDamageModel=0 profile from pinned playerprofile.cpp
struct CameraHitState {
	int32_t repaired = 0, critical_time = 0, shield_time = 0;
	std::array<FixedLightPoint, 3> points{};
	std::array<int32_t, 3> times{};
	unsigned next = 0;
	int32_t repaired_elapsed(int32_t ticks) const
	{
		return hitbox_fixed(double(int64_t(ticks) - repaired));
	}
	void restore_repaired(int32_t ticks, int32_t elapsed)
	{
		const int32_t restored = hitbox_fixed(double(int64_t(ticks) - elapsed));
		*this = {};
		repaired = restored;
	}
	FixedLightPoint register_hit(FixedLightPoint hit, const FixedLightPoint &position, int32_t size, int type, int32_t ticks, int shield_duration, bool nostalgia = false)
	{
		if (nostalgia || (type != 2 && type != 4 && type != 9)) return hit;
		if (next >= points.size() || (shield_duration != 500 && shield_duration != 2000)) throw std::runtime_error("Camera hit state exceeds qualified profile");
		auto direction = geometry_default_direction(hitbox_difference(hit, position));
		repaired = ticks;
		if (int64_t(ticks) - critical_time < shield_duration / 4) return hit;
		for (unsigned axis = 0; axis < 3; ++axis) hit[axis] = hitbox_fixed(double(size) / 65536.0 * direction[axis]);
		shield_time = ticks;
		for (unsigned i = 0; i < points.size(); ++i)
			if (geometry_default_magnitude(hitbox_difference(points[i], hit)) < 4096) {
				for (unsigned axis = 0; axis < 3; ++axis) hit[axis] = hitbox_fixed(double(int64_t(points[i][axis]) + hit[axis]) / 2.0);
				direction = geometry_default_direction(hit);
				for (unsigned axis = 0; axis < 3; ++axis) hit[axis] = hitbox_fixed(double(size) / 65536.0 * direction[axis]);
				points[next] = hit;
				// Source writes the matched slot's timestamp but the current slot's point
				times[i] = ticks;
				return points[next];
			}
		points[next] = hit;
		times[next] = ticks;
		next = (next + 1) % points.size();
		return points[next];
	}
};
} // namespace xl_compat
#endif
