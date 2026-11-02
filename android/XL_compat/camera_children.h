#ifndef DXX_XL_CAMERA_CHILDREN_H
#define DXX_XL_CAMERA_CHILDREN_H
#include "camera_splash.h"
#include <vector>

namespace xl_compat
{
struct CameraSmartTarget {
	int owner = -1;
	bool player = false, robot = false, cloaked = false, companion = false;
	FixedLightPoint position{};
};
template <class Visible>
std::vector<int> camera_smart_targets(const FixedLightPoint &position, int parent, bool player_parent, bool robot_parent,
                                      const std::vector<CameraSmartTarget> &candidates, Visible visible)
{
	std::vector<int> targets;
	for (const auto &actor : candidates) {
		if (actor.player) {
			if (actor.owner == parent || actor.cloaked) continue;
		} else if (actor.robot) {
			if (actor.cloaked || robot_parent || (player_parent && actor.companion)) continue;
		} else continue;
		if (geometry_default_magnitude(splash_difference(position, actor.position)) < 150 * 65536 && visible(actor.owner)) {
			targets.push_back(actor.owner);
			if (targets.size() == 30) break;
		}
	}
	return targets;
}
template <class Random>
FixedLightPoint camera_smart_direction(const FixedLightPoint &position, const FixedLightPoint *target, Random random)
{
	FixedLightPoint direction{};
	if (target) direction = geometry_default_direction(splash_difference(*target, position));
	FixedLightPoint noise{};
	const int axis = random() % 3;
	const unsigned first = axis == 2 ? 0 : axis == 1 ? 1
	                                                 : 2;
	noise[first] = (16383 - random()) | 1;
	noise[(first + 1) % 3] = 16383 - random();
	noise[(first + 2) % 3] = 16383 - random();
	noise = geometry_default_direction(noise);
	if (!target) return noise;
	for (unsigned i = 0; i < 3; ++i) direction[i] = splash_fixed(double(direction[i]) + splash_multiply(noise[i], 65536 / 4));
	return geometry_default_direction(direction);
}
template <class Random>
int camera_smart_choice(const std::vector<int> &targets, Random random)
{
	return targets.empty() ? -1 : targets[random() % targets.size()];
}
} // namespace xl_compat
#endif
