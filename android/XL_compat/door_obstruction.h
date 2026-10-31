#ifndef DXX_XL_DOOR_OBSTRUCTION_H
#define DXX_XL_DOOR_OBSTRUCTION_H
#include <cstdint>
namespace xl_compat
{
template <typename Distance>
bool door_sphere_pokes(int radius, unsigned faces, bool pokes_out, Distance distance)
{
	if (!radius) return false;
	unsigned intersections = 0;
	for (unsigned face = 0; face < faces; ++face) {
		const std::int64_t d = distance(face);
		if (d - radius < -250 && d + radius >= -250) ++intersections;
	}
	return intersections && (pokes_out || intersections == faces);
}

inline bool door_ignores_object(int type, bool ignore_marker)
{
	return type == 5 || type == 1 || type == 19 || type == 20 || (ignore_marker && type == 15);
}

// Pinned DoorIsBlocked walks the front list and tests both sides for each object
template <typename World>
bool door_is_blocked(World &world, bool ignore_marker)
{
	for (int object = world.first(); object != -1; object = world.next(object)) {
		if (door_ignores_object(world.type(object), ignore_marker)) continue;
		if (world.pokes(object, false) || world.pokes(object, true)) return true;
	}
	return false;
}
} // namespace xl_compat
#endif
