#ifndef DXX_XL_CAMERA_GLASS_H
#define DXX_XL_CAMERA_GLASS_H
#include "camera_splash.h"
#include <array>
#include <vector>

namespace xl_compat
{
struct GlassRouteRoom {
	FixedLightPoint center{};
	std::array<int, 6> children{ -1, -1, -1, -1, -1, -1 };
	std::array<bool, 6> transparent{};
};
inline int glass_distance(const FixedLightPoint &a, const FixedLightPoint &b)
{
	return geometry_default_magnitude(splash_difference(a, b));
}
// Source CSimpleBiDirRouter with cache=-1, including no same-room shortcut
inline int camera_glass_route(const std::vector<GlassRouteRoom> &rooms, int start, int destination,
                              const FixedLightPoint &begin, const FixedLightPoint &end, int limit = 5)
{
	if (start < 0 || destination < 0 || std::size_t(start) >= rooms.size() || std::size_t(destination) >= rooms.size())
		throw std::runtime_error("Invalid glass route endpoint");
	struct Node {
		bool visited = false;
		int depth = 0, previous = -1, edge = -1;
	};
	std::array<std::vector<Node>, 2> paths{ std::vector<Node>(rooms.size()), std::vector<Node>(rooms.size()) };
	std::array<std::vector<int>, 2> queues{ { { start }, { destination } } };
	std::array<std::size_t, 2> tails{};
	const std::array<int, 2> origins{ start, destination };
	paths[0][start].visited = paths[1][destination].visited = true;
	int scanning = 3;
	const int max_depth = limit < 0 ? int(rooms.size()) : limit;
	for (;;)
		for (int direction = 0; direction < 2; ++direction) {
			if (tails[direction] >= queues[direction].size()) return -1;
			const int predecessor = queues[direction][tails[direction]++];
			const int depth = paths[direction][predecessor].depth + 1;
			if (depth > max_depth) {
				scanning &= ~(1 << direction);
				if (!scanning) return -1;
				continue;
			}
			for (int side = 0; side < 6; ++side) {
				const int successor = rooms[predecessor].children[side];
				if (successor < 0) continue;
				if (std::size_t(successor) >= rooms.size()) throw std::runtime_error("Invalid glass route child");
				bool passable = rooms[predecessor].transparent[side];
				if (direction) {
					passable = false;
					for (int opposite = 0; opposite < 6; ++opposite)
						if (rooms[successor].children[opposite] == predecessor) {
							passable = rooms[successor].transparent[opposite];
							break;
						}
				}
				if (!passable) continue;
				auto &node = paths[direction][successor];
				if (node.visited) continue;
				node.previous = predecessor;
				node.edge = side;
				if (paths[!direction][successor].visited) {
					int distance = 0, length = 0;
					for (int branch = 0; branch < 2; ++branch) {
						int next = successor;
						for (;;) {
							const auto &step = paths[branch][next];
							const int previous = step.previous;
							if (previous < 0) break;
							if (previous == origins[branch]) {
								distance = splash_fixed(double(distance) + glass_distance(branch ? end : begin, rooms[next].center));
								break;
							}
							if (++length > 2 * max_depth + 2 || std::size_t(length) > rooms.size()) return -0x7fffffff;
							const int child = rooms[previous].children[step.edge];
							distance = splash_fixed(double(distance) + std::max(1, glass_distance(rooms[previous].center, rooms[child].center)));
							next = previous;
						}
					}
					return distance ? distance : -0x7fffffff;
				}
				node.visited = true;
				node.depth = depth;
				queues[direction].push_back(successor);
			}
		}
}
} // namespace xl_compat
#endif
