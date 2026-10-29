#ifndef DXX_XL_RUNTIME_LAYOUT_FIXTURE_H
#define DXX_XL_RUNTIME_LAYOUT_FIXTURE_H
#include "layout_fixture.h"

namespace xl_test
{
struct RuntimeLayoutFixture {
	xl_compat::Bytes bytes;
	std::vector<std::array<std::size_t, 6>> corners;
	std::vector<std::size_t> extras;
	std::size_t fog = 0;
};

// Test-only encoding of unchanged 23/40 geometry/game records in the newer layout
// This is not an authoring converter or proof that an original mission is supported
inline RuntimeLayoutFixture runtime_layout_fixture(const xl_compat::Bytes &original, int version)
{
	const auto source = xl_compat::inspect_level(original);
	if (source.level_version != 23 || source.game_version != 40 || (version != 25 && version != 27))
		throw std::runtime_error("Runtime layout fixture requires 23/40 input and 25/27 output");
	RuntimeLayoutFixture fixture;
	auto &out = fixture.bytes;
	out.assign(original.begin(), original.begin() + source.mine_offset);
	layout_patch(out, 4, version);
	const auto put = [&](int value, int width) { layout_put(out, static_cast<std::uint32_t>(value), width); };
	put(0, 1);
	put(static_cast<int>(source.vertices.size()), 2);
	put(static_cast<int>(source.segments.size()), 2);
	for (const auto &vertex : source.vertices)
		for (int value : vertex) put(value, 4);
	for (const auto &segment : source.segments) {
		put(segment.owner, 1);
		put(segment.group, 1);
		int children = 0, walls = 0;
		for (int side = 0; side < 6; ++side) {
			if (segment.children[side] != -1) children |= 1 << side;
			if (segment.sides[side].wall != -1) walls |= 1 << side;
		}
		put(children, 1);
		for (int side = 0; side < 6; ++side)
			if (children & (1 << side)) put(segment.children[side], 2);
		for (int vertex : segment.vertices) put(vertex, 2);
		put(walls, 1);
		for (int side = 0; side < 6; ++side)
			if (walls & (1 << side)) put(segment.sides[side].wall, 2);
		std::array<std::size_t, 6> corners;
		for (int side = 0; side < 6; ++side) {
			corners[side] = out.size();
			for (int corner : xl_compat::classic_side_corners[side]) put(corner, 1);
			const auto &surface = segment.sides[side];
			if (segment.children[side] != -1 && surface.wall == -1) continue;
			put(surface.texture | (surface.overlay ? 0x8000 : 0), 2);
			if (surface.overlay) put(surface.overlay, 2);
			for (int value : surface.uvl) put(value, 2);
		}
		fixture.corners.push_back(corners);
	}
	for (const auto &segment : source.segments) {
		fixture.extras.push_back(out.size());
		put(segment.function, 1);
		put(segment.producer, 2);
		put(segment.value, 2);
		put(segment.flags, 1);
		put(segment.properties, 1);
		put(segment.damage[0], 2);
		put(segment.damage[1], 2);
		put(segment.light, 4);
	}
	const auto colors = (source.vertices.size() + source.segments.size() * 6 + 910) * 13;
	if (colors > std::size_t(source.game_offset)) throw std::runtime_error("Missing fixture color tables");
	out.insert(out.end(), original.begin() + source.game_offset - colors, original.begin() + source.game_offset);
	const auto game = out.size();
	layout_patch(out, 12, static_cast<std::uint32_t>(game));
	out.insert(out.end(), original.begin() + source.game_offset, original.begin() + source.game_offset + 155);
	fixture.fog = out.size();
	if (version == 27) {
		// Deliberately nondefault records, retained even in this dry fixture
		for (int value : { 51, 102, 153, 8, 204, 102, 0, 3, 64, 128, 192, 12, 127, 255, 63, 5 }) put(value, 1);
	}
	out.insert(out.end(), original.begin() + source.game_offset + 155, original.end());
	const auto rebase = [&](std::size_t field, int old) {
		if (old <= 0) return;
		if (old < source.game_offset + 155 || std::size_t(old) > original.size()) throw std::runtime_error("Invalid fixture section offset");
		layout_patch(out, game + field, static_cast<std::uint32_t>(old - source.game_offset + game + (version == 27 ? 16 : 0)));
	};
	std::uint32_t player = 0;
	for (int i = 0; i < 4; ++i) player |= std::uint32_t(original.at(source.game_offset + 27 + i)) << (i * 8);
	rebase(27, static_cast<std::int32_t>(player));
	for (std::size_t i = 0; i < source.sections.size(); ++i) rebase(35 + i * 12, source.sections[i].offset);
	return fixture;
}
} // namespace xl_test
#endif
