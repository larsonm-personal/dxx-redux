#ifndef DXX_XL_WIDE_WALL_FIXTURE_H
#define DXX_XL_WIDE_WALL_FIXTURE_H
#include "../inventory.h"
#include <algorithm>
#include <stdexcept>

namespace xl_test
{
// Derived fixture: retain authored geometry/actions, relocate wall IDs upward
// Padding rows are unreferenced normal walls, not evidence of authored content
inline xl_compat::Bytes remap_walls(const xl_compat::Bytes &original, unsigned count)
{
	const auto level = xl_compat::inspect_level(original);
	if (count < level.walls.size() || count > 65535) throw std::runtime_error("Invalid wide-wall fixture count");
	auto bytes = original;
	const unsigned shift = count - static_cast<unsigned>(level.walls.size());
	const auto patch = [&](std::size_t at, unsigned value, unsigned width) {
		for (unsigned i = 0; i < width; ++i) bytes.at(at + i) = uint8_t(value >> (8 * i));
	};
	std::size_t at = level.mine_offset + 5 + level.vertices.size() * 12;
	for (const auto &segment : level.segments) {
		at += 2; // owner/group
		const auto children = bytes.at(at++);
		for (int side = 0; side < 6; ++side)
			if (children & (1 << side)) at += 2;
		at += 16;
		const auto walls = bytes.at(at++);
		for (int side = 0; side < 6; ++side)
			if (walls & (1 << side)) {
				if (segment.sides[side].wall >= 0) patch(at, unsigned(segment.sides[side].wall) + shift, 2);
				at += 2;
			}
		for (int side = 0; side < 6; ++side)
			if (segment.children[side] == -1 || segment.sides[side].wall >= 0) {
				const bool overlay = (bytes.at(at + 1) & 0x80) != 0;
				at += overlay ? 28 : 26;
			}
	}
	const bool modern = level.game_version >= 37;
	const unsigned stride = modern ? 25 : 24;
	const unsigned table = static_cast<unsigned>(level.sections.at(1).offset);
	const unsigned old_end = table + static_cast<unsigned>(level.walls.size()) * stride;
	bytes.insert(bytes.begin() + old_end, shift * stride, 0);
	for (std::size_t section = 0; section < level.sections.size(); ++section)
		if (level.sections[section].offset >= int(old_end))
			patch(level.game_offset + 35 + section * 12, unsigned(level.sections[section].offset) + shift * stride, 4);
	patch(level.game_offset + 47, table, 4);
	patch(level.game_offset + 51, count, 4);
	patch(level.game_offset + 55, stride, 4);
	std::fill(bytes.begin() + table, bytes.begin() + table + count * stride, 0);
	for (unsigned id = 0; id < shift; ++id) {
		const auto row = table + id * stride;
		patch(row + 12, unsigned(-1), 4);
		bytes.at(row + (modern ? 20 : 19)) = 255;
		bytes.at(row + (modern ? 21 : 20)) = 255;
		bytes.at(row + (modern ? 22 : 21)) = 1;
		bytes.at(row + (modern ? 23 : 22)) = 255;
	}
	for (std::size_t id = 0; id < level.walls.size(); ++id) {
		const auto row = table + (id + shift) * stride;
		for (unsigned byte = 0; byte < stride; ++byte)
			bytes.at(row + byte) = original.at(level.sections.at(1).offset + id * stride + byte);
		if (level.walls[id].linked_wall >= 0) patch(row + 12, unsigned(level.walls[id].linked_wall) + shift, 4);
	}
	return bytes;
}
} // namespace xl_test
#endif
