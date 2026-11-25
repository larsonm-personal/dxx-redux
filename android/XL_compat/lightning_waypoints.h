#ifndef DXX_XL_LIGHTNING_WAYPOINTS_H
#define DXX_XL_LIGHTNING_WAYPOINTS_H

#include "lightning_path.h"
#include <cmath>
#include <map>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
struct LightningWaypoint {
	LightningPoint position{};
	int speed = 0, next = -1, previous = -1, route = -1;
};

struct LightningWaypointState {
	int node = -1;
	bool reverse = false, waiting = false;
};

struct LightningWaypointOwner {
	int node, partner;
};

struct LightningWaypoints {
	std::vector<LightningWaypoint> nodes;
	std::map<int, LightningWaypointOwner> owners;
};

inline float waypoint_distance(const LightningPoint &offset)
{
	const float x = float(offset[0]) / 65536.0f, y = float(offset[1]) / 65536.0f, z = float(offset[2]) / 65536.0f;
	return std::sqrt(x * x + y * y + z * z);
}

inline int waypoint_successor(const LightningWaypoint &point, bool reverse)
{
	return reverse ? point.previous : point.next;
}

inline bool resume_waypoint_pair(LightningWaypointState &state, LightningWaypointState *partner)
{
	if (!state.waiting) return true;
	if (partner && !partner->waiting) return false;
	state.waiting = false;
	if (partner) partner->waiting = false;
	return true;
}

inline bool hop_lightning_waypoint(const std::vector<LightningWaypoint> &nodes, LightningWaypointState &state,
                                   LightningPoint &position, bool paired)
{
	const int start = state.node;
	do {
		state.node = waypoint_successor(nodes[state.node], state.reverse);
		const auto &node = nodes[state.node];
		position = node.position;
		if (node.next == node.previous) state.reverse = !state.reverse;
		if (state.node == start) return false;
		if (node.speed > 0) return !state.waiting;
		state.waiting = paired;
	} while (true);
}

inline void move_lightning_waypoint(const std::vector<LightningWaypoint> &nodes, LightningWaypointState &state,
                                    LightningWaypointState *partner, LightningPoint &position, float slow_motion)
{
	if (!resume_waypoint_pair(state, partner)) return;
	float remaining = 1.0f;
	// Admission requires positive-speed legs longer than a full tick.
	for (int leg = 0; leg < 2; ++leg) {
		const auto &point = nodes[state.node];
		const auto &successor = nodes[waypoint_successor(point, state.reverse)];
		const float step = float(point.speed) / 40.0f * remaining / slow_motion;
		const float left = waypoint_distance(lightning_detail::subtract(successor.position, position));
		if (left > step) {
			const auto direction = lightning_detail::subtract(successor.position, point.position);
			const float distance = waypoint_distance(direction);
			for (unsigned axis = 0; axis < position.size(); ++axis) {
				const float unit = (float(direction[axis]) / 65536.0f) / distance;
				const auto offset = std::int64_t(unit * step * 65536.0f);
				position[axis] = lightning_detail::checked(std::int64_t(position[axis]) + offset);
			}
			return;
		}
		if (!hop_lightning_waypoint(nodes, state, position, partner != nullptr)) return;
		remaining = 1.0f - left / step;
		if (remaining < 1.0e-6f) return;
	}
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
