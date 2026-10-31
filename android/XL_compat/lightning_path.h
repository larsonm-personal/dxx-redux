#ifndef DXX_XL_LIGHTNING_PATH_H
#define DXX_XL_LIGHTNING_PATH_H
#include "lightning_effect.h"
#include <vector>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
using LightningPoint = std::array<int, 3>;
namespace lightning_detail
{
inline LightningPoint add(const LightningPoint &a, const LightningPoint &b)
{
	return { checked(std::int64_t(a[0]) + b[0]), checked(std::int64_t(a[1]) + b[1]), checked(std::int64_t(a[2]) + b[2]) };
}
inline LightningPoint subtract(const LightningPoint &a, const LightningPoint &b)
{
	return { checked(std::int64_t(a[0]) - b[0]), checked(std::int64_t(a[1]) - b[1]), checked(std::int64_t(a[2]) - b[2]) };
}
inline LightningPoint multiply(const LightningPoint &point, int scale)
{
	LightningPoint result{};
	for (unsigned axis = 0; axis < 3; ++axis)
		result[axis] = checked(std::int64_t(double(point[axis]) / 65536.0 * scale));
	return result;
}
} // namespace lightning_detail

struct LightningNode {
	LightningPoint base{}, position{}, next{}, offset{};
	std::array<LightningPoint, 2> plane{};
	std::array<int, 2> delta{};
};

inline void lightning_node_setup(LightningNode &node, const LightningPoint &position, const std::array<LightningPoint, 2> &plane)
{
	node.base = node.position = position;
	node.offset = {};
	node.plane = plane;
	// Source Setup preserves the prior erratic displacement history
}

inline void lightning_node_offset(LightningNode &node, int frames)
{
	if (frames <= 0) throw std::runtime_error("XL lightning interpolation requires positive frames");
	node.offset = lightning_detail::multiply(lightning_detail::subtract(node.next, node.position), 65536 / frames);
}

// Style-0 displacement without clamp; caller admission must enforce that profile
template <class Random>
void lightning_node_erratic(LightningNode &node, int frames, int amplitude, bool in_plane, bool from_end, bool random_bolt, int index, Random &&random)
{
	using namespace lightning_detail;
	node.next = node.base;
	for (unsigned axis = 0; axis < (in_plane ? 1u : 2u); ++axis) {
		int delta = checked(std::int64_t(amplitude / 2) - int(random() * amplitude));
		if (!random_bolt) {
			index -= from_end;
			delta = checked(std::int64_t(delta) * 3);
		}
		for (int previous = 1; previous <= 2; ++previous)
			if (index - previous > 0) delta = checked(std::int64_t(delta) + (previous + 1LL) * node.delta[axis]);
		node.delta[axis] = delta /= 6;
		node.next = add(node.next, multiply(node.plane[axis], delta));
	}
	lightning_node_offset(node, frames);
}

inline void lightning_smooth_nodes(std::vector<LightningNode> &nodes)
{
	for (std::size_t i = 1; i + 1 < nodes.size(); ++i)
		for (unsigned axis = 0; axis < 3; ++axis)
			nodes[i].next[axis] = lightning_detail::checked(std::int64_t(nodes[i - 1].next[axis] / 4) + nodes[i].next[axis] / 2 + nodes[i + 1].next[axis] / 4);
}

inline void lightning_compute_offsets(std::vector<LightningNode> &nodes, int frames, bool random_bolt)
{
	if (nodes.size() < 2) throw std::runtime_error("XL lightning path has too few nodes");
	const auto end = nodes.size() - 1 - !random_bolt;
	for (std::size_t i = 1; i < end; ++i) lightning_node_offset(nodes[i], frames);
}

inline void lightning_animate_nodes(std::vector<LightningNode> &nodes, bool initial, bool random_bolt)
{
	if (nodes.size() < 2) throw std::runtime_error("XL lightning path has too few nodes");
	for (std::size_t i = 1; i < nodes.size() - !random_bolt; ++i)
		nodes[i].position = initial ? nodes[i].next : lightning_detail::add(nodes[i].position, nodes[i].offset);
}

template <class Random>
void lightning_random_erratic_path(std::vector<LightningNode> &nodes, int frames, int amplitude, bool in_plane, Random &&random)
{
	if (nodes.size() < 2) throw std::runtime_error("XL lightning path has too few nodes");
	nodes.front().next = nodes.front().position;
	nodes.front().offset = {};
	amplitude = lightning_detail::checked(std::int64_t(amplitude) * 4);
	for (std::size_t i = 1; i < nodes.size(); ++i)
		// Source passes its plane flag as bFromEnd on the random style-0 path
		lightning_node_erratic(nodes[i], frames, amplitude, false, in_plane, true, int(i - 1), random);
	lightning_smooth_nodes(nodes);
	lightning_compute_offsets(nodes, frames, true);
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
