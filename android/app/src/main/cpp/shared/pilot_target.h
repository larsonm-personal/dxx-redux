#pragma once
#include <cmath>

// Ordinary D2 pilot input, shared by the Android controller and native flight tests
namespace pilot_target
{
inline bool center_ray_open(const object &candidate)
{
	fvi_query query = {};
	fvi_info hit = {};
	vms_vector target_point = candidate.pos;
	query.p0 = &ConsoleObject->pos;
	query.p1 = &target_point;
	query.startseg = ConsoleObject->segnum;
	query.thisobjnum = (int) (ConsoleObject - Objects);
	/* Conservative selection: no transparent-wall bypass, no claim about gun offsets */
	return find_vector_intersection(&query, &hit) == HIT_NONE;
}

inline int nearest_robot(double range)
{
	int target_slot = -1;
	double nearest = range;
	for (int i = 0; i <= Highest_object_index; ++i) {
		const object &candidate = Objects[i];
		if (candidate.type != OBJ_ROBOT || candidate.shields < 0 ||
		    (candidate.flags & OF_SHOULD_BE_DEAD) || Robot_info[candidate.id].companion) continue;
		const double dx = f2fl(candidate.pos.x) - f2fl(ConsoleObject->pos.x);
		const double dy = f2fl(candidate.pos.y) - f2fl(ConsoleObject->pos.y);
		const double dz = f2fl(candidate.pos.z) - f2fl(ConsoleObject->pos.z);
		const double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
		if (distance < nearest && center_ray_open(candidate)) {
			target_slot = i;
			nearest = distance;
		}
	}
	return target_slot;
}
} // namespace pilot_target
