#ifndef DXX_XL_CLOAK_FIXTURE_H
#define DXX_XL_CLOAK_FIXTURE_H
#include "../inventory.h"
#include <stdexcept>

namespace xl_test
{
// Derived 23/40 fixture: add closed wall pairs to existing open portals
inline xl_compat::Bytes add_cloak_pairs(const xl_compat::Bytes &original, unsigned additional)
{
	const auto source = xl_compat::inspect_level(original);
	if (source.level_version != 23 || source.game_version != 40) throw std::runtime_error("Cloak fixture requires 23/40 geometry");
	auto rooms = source.segments;
	std::vector<xl_compat::Wall> added;
	for (std::size_t segment = 0; segment < rooms.size() && added.size() < additional * 2; ++segment)
		for (int side = 0; side < 6 && added.size() < additional * 2; ++side) {
			auto &room = rooms[segment];
			const int child = room.children[side];
			if (child <= int(segment) || child >= int(rooms.size()) || room.sides[side].wall != -1) continue;
			int connected = -1;
			for (int face = 0; face < 6; ++face)
				if (rooms[child].children[face] == int(segment)) connected = face;
			if (connected < 0 || rooms[child].sides[connected].wall != -1) continue;
			for (const auto location : { std::array<int, 2>{ int(segment), side }, std::array<int, 2>{ child, connected } }) {
				auto &face = rooms[location[0]].sides[location[1]];
				face.wall = int(source.walls.size() + added.size());
				face.texture = 1;
				face.overlay = 0;
				face.uvl = { 0, 0, 8192, 2048, 0, 8192, 2048, 2048, 8192, 0, 2048, 8192 };
				added.push_back({ location[0], location[1], 0, -1, 5, 0, 0, 255, -1, 1, 255, 0 });
			}
		}
	if (added.size() != additional * 2) throw std::runtime_error("Not enough original portals for cloak capacity fixture");
	const std::size_t geometry_start = source.mine_offset + 5 + source.vertices.size() * 12;
	auto old_end = geometry_start;
	for (const auto &room : source.segments) {
		old_end += 2;
		const auto children = original.at(old_end++);
		for (int side = 0; side < 6; ++side)
			if (children & (1 << side)) old_end += 2;
		old_end += 16;
		const auto walls = original.at(old_end++);
		for (int side = 0; side < 6; ++side)
			if (walls & (1 << side)) old_end += 2;
		for (int side = 0; side < 6; ++side)
			if (room.children[side] == -1 || room.sides[side].wall >= 0) old_end += (original.at(old_end + 1) & 0x80) ? 28 : 26;
	}
	xl_compat::Bytes geometry;
	const auto put = [](xl_compat::Bytes &to, int value, unsigned width) {
		for (unsigned byte = 0; byte < width; ++byte) to.push_back(uint8_t(uint32_t(value) >> (byte * 8)));
	};
	for (const auto &room : rooms) {
		put(geometry, room.owner, 1);
		put(geometry, room.group, 1);
		int children = 0, walls = 0;
		for (int side = 0; side < 6; ++side) {
			if (room.children[side] != -1) children |= 1 << side;
			if (room.sides[side].wall >= 0) walls |= 1 << side;
		}
		put(geometry, children, 1);
		for (int side = 0; side < 6; ++side)
			if (children & (1 << side)) put(geometry, room.children[side], 2);
		for (int vertex : room.vertices) put(geometry, vertex, 2);
		put(geometry, walls, 1);
		for (int side = 0; side < 6; ++side)
			if (walls & (1 << side)) put(geometry, room.sides[side].wall, 2);
		for (int side = 0; side < 6; ++side) {
			const auto &face = room.sides[side];
			if (room.children[side] != -1 && face.wall < 0) continue;
			put(geometry, face.texture | (face.overlay ? 0x8000 : 0), 2);
			if (face.overlay) put(geometry, face.overlay, 2);
			for (int value : face.uvl) put(geometry, value, 2);
		}
	}
	xl_compat::Bytes result(original.begin(), original.begin() + geometry_start);
	result.insert(result.end(), geometry.begin(), geometry.end());
	result.insert(result.end(), original.begin() + old_end, original.end());
	const int delta = int(geometry.size()) - int(old_end - geometry_start);
	const int wall_end = source.sections[1].offset + int(source.walls.size()) * 25;
	xl_compat::Bytes wall_bytes;
	for (const auto &wall : added) {
		for (int value : { wall.segment, wall.side, wall.hitpoints, wall.linked_wall }) put(wall_bytes, value, 4);
		put(wall_bytes, wall.type, 1);
		put(wall_bytes, wall.flags, 2);
		for (int value : { wall.state, wall.trigger, wall.clip, wall.keys, wall.controlling_trigger, wall.cloak }) put(wall_bytes, value, 1);
	}
	result.insert(result.begin() + wall_end + delta, wall_bytes.begin(), wall_bytes.end());
	const auto patch = [&](std::size_t offset, int value) {
		for (unsigned byte = 0; byte < 4; ++byte) result.at(offset + byte) = uint8_t(uint32_t(value) >> (byte * 8));
	};
	const int game = source.game_offset + delta;
	patch(12, game);
	const auto rebase = [&](unsigned field) {
		uint32_t offset = 0;
		for (unsigned byte = 0; byte < 4; ++byte) offset |= uint32_t(original.at(source.game_offset + field + byte)) << (byte * 8);
		if (int32_t(offset) > 0) patch(game + field, int(offset) + delta + (int(offset) >= wall_end ? int(wall_bytes.size()) : 0));
	};
	rebase(27);
	for (unsigned section = 0; section < source.sections.size(); ++section) rebase(35 + section * 12);
	patch(game + 51, int(source.walls.size() + added.size()));
	const auto check = xl_compat::inspect_level(result);
	if (check.walls.size() != source.walls.size() + added.size() || check.vertices != source.vertices || check.segments.size() != source.segments.size()) throw std::runtime_error("Cloak fixture changed original geometry or lost walls");
	for (std::size_t index = 0; index < check.segments.size(); ++index)
		if (check.segments[index].vertices != source.segments[index].vertices || check.segments[index].children != source.segments[index].children)
			throw std::runtime_error("Cloak fixture changed original portal connectivity");
	return result;
}
} // namespace xl_test
#endif
