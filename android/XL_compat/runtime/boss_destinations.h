#ifndef XL_BOSS_DESTINATIONS_H
#define XL_BOSS_DESTINATIONS_H
#include <algorithm>
#include <vector>
namespace xl_compat
{
// Validated XL topology; engine passability and size queries remain callbacks
template <class Child, class Passable, class Group, class Fits>
std::vector<int> boss_destinations(int home, int count, Child child, Passable passable, Group group, Fits fits)
{
	const int home_group = group(home);
	std::vector<int> result{ home }, queue{ home };
	std::vector<bool> visited(count);
	const auto maximum = static_cast<std::size_t>((std::min) (1000, count));
	for (std::size_t tail = 0; tail < queue.size(); ++tail)
		for (int side = 0; side < 6; ++side) {
			const int target = child(queue[tail], side);
			if (!passable(queue[tail], side) || target < 0 || target >= count) continue;
			if (visited[target] || group(target) != home_group) continue;
			visited[target] = true;
			if (!fits(target)) continue;
			queue.push_back(target);
			// The reference queues the boundary child but omits it from the list
			if (result.size() >= maximum - 1) return result;
			result.push_back(target);
		}
	return result;
}
} // namespace xl_compat
#endif
