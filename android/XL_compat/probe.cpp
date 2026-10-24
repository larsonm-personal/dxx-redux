#include "probe.h"
#include "admission.h"

#include <limits>
#include <stdexcept>

namespace xl_compat
{
namespace
{

void require(bool value, const std::string &why)
{
	if (!value) throw std::runtime_error("gameplay probe: " + why);
}

void put(Bytes &bytes, int value, unsigned width)
{
	const auto bits = static_cast<std::uint32_t>(value);
	for (unsigned i = 0; i < width; ++i) bytes.push_back(static_cast<std::uint8_t>(bits >> (i * 8)));
}

void patch(Bytes &bytes, std::size_t offset, int value, unsigned width = 4)
{
	require(offset <= bytes.size() && width <= bytes.size() - offset, "patch outside file");
	const auto bits = static_cast<std::uint32_t>(value);
	for (unsigned i = 0; i < width; ++i) bytes[offset + i] = static_cast<std::uint8_t>(bits >> (i * 8));
}

int read_offset(const Bytes &bytes, std::size_t offset)
{
	require(offset <= bytes.size() && 4 <= bytes.size() - offset, "offset outside file");
	std::uint32_t value = 0;
	for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(bytes[offset + i]) << (i * 8);
	if (value == 0xffffffff) return -1;
	require(value <= static_cast<std::uint32_t>(std::numeric_limits<int>::max()), "invalid absolute offset");
	return static_cast<int>(value);
}
} // namespace

GameplayProbe prepare_gameplay_probe(const Bytes &source)
{
	GameplayProbe result;
	result.source = inspect_level(source);
	const auto &v = result.source;
	const auto blockers = runtime_blockers(v);
	require(blockers.empty(), blocker_message(blockers));
	require(v.object_triggers == 0, "object triggers have no stock equivalent");
	// Current stock representation limits, checked again by the unchanged stock loader
	require(v.walls.size() <= 255 && v.objects.size() <= 1000 && v.segments.size() <= 9000,
	        "stock representation capacity exceeded");

	auto &out = result.stock_level;
	out.assign(source.begin(), source.begin() + v.mine_offset);
	patch(out, 4, 8);
	put(out, 0, 1);
	put(out, static_cast<int>(v.vertices.size()), 2);
	put(out, static_cast<int>(v.segments.size()), 2);
	for (const auto &vertex : v.vertices)
		for (const auto component : vertex) put(out, component, 4);
	for (const auto &seg : v.segments) {
		require(seg.owner == -1 && seg.group == -1, "nonneutral owner/group");
		int children = 0, walls = 0;
		for (int i = 0; i < 6; ++i) {
			if (seg.children[i] != -1) children |= 1 << i;
			if (seg.sides[i].wall != -1) walls |= 1 << i;
		}
		put(out, children, 1);
		for (int i = 0; i < 6; ++i)
			if (children & (1 << i)) put(out, seg.children[i], 2);
		for (const auto vertex : seg.vertices) put(out, vertex, 2);
		put(out, walls, 1);
		for (int i = 0; i < 6; ++i) {
			if (walls & (1 << i)) {
				require(seg.sides[i].wall >= 0 && seg.sides[i].wall < 255, "unrepresentable wall ID");
				put(out, seg.sides[i].wall, 1);
			}
		}
		for (int i = 0; i < 6; ++i) {
			const auto &side = seg.sides[i];
			if (seg.children[i] != -1 && side.wall == -1) continue;
			require(side.texture >= 0 && side.texture < 910, "unrepresentable texture");
			put(out, side.texture | (side.overlay ? 0x8000 : 0), 2);
			if (side.overlay) put(out, side.overlay, 2);
			for (const auto component : side.uvl) put(out, component, 2);
		}
	}
	for (const auto &seg : v.segments) {
		put(out, seg.function, 1);
		put(out, seg.producer, 1);
		put(out, seg.value, 1);
		put(out, seg.flags, 1);
		put(out, seg.light, 4);
	}
	require(out.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()), "level size overflow");
	const int new_game_offset = static_cast<int>(out.size());
	out.insert(out.end(), source.begin() + v.game_offset, source.end());
	patch(out, 12, new_game_offset);
	patch(out, new_game_offset + 2, 32, 2);
	// Preserve the game records byte for byte, including the neutral legacy object-head
	// padding. Stock game 32 reads explicit section offsets and never consumes that tail
	const auto rebase = [&](std::size_t field) {
		const int old = read_offset(source, v.game_offset + field);
		if (old <= 0) return;
		require(old >= v.game_offset && static_cast<std::size_t>(old) <= source.size(), "invalid game section offset");
		patch(out, new_game_offset + field, old - v.game_offset + new_game_offset);
	};
	rebase(27); // Legacy unused player block
	for (std::size_t i = 0; i < v.sections.size(); ++i) rebase(35 + i * 12);
	return result;
}
} // namespace xl_compat
