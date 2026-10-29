#ifndef DXX_XL_LAYOUT_FIXTURE_H
#define DXX_XL_LAYOUT_FIXTURE_H
#include "../inventory.h"
#include <algorithm>
#include <stdexcept>

namespace xl_test
{
struct LayoutFixture {
	xl_compat::Bytes bytes;
	std::size_t mine = 0, vertices = 0, extras = 0, colors = 0, game = 0, fog = 0, walls = 0;
	std::array<std::size_t, 6> corners{};
};
inline void layout_put(xl_compat::Bytes &bytes, std::uint32_t value, int width)
{
	for (int i = 0; i < width; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
}
inline void layout_patch(xl_compat::Bytes &bytes, std::size_t at, std::uint32_t value, int width = 4)
{
	for (int i = 0; i < width; ++i) bytes.at(at + i) = static_cast<std::uint8_t>(value >> (8 * i));
}
// Encoding controls, not playable geometry: one segment has a self-linked open
// portal, one wall, an oriented overlay, signed UVs, and nondefault fog values
inline LayoutFixture layout_fixture(int version, int shape = 0, int producer = -1, int value = -1)
{
	if ((version != 23 && version != 25 && version != 27) || (version == 23 && shape))
		throw std::runtime_error("Invalid modern layout fixture profile");
	LayoutFixture result;
	auto &b = result.bytes;
	auto put = [&](std::uint32_t n, int width) { layout_put(b, n, width); };
	put(0x504c564c, 4);
	put(version, 4);
	b.resize(23);
	put(0, 1);
	put(30, 4);
	put(0xffffffff, 4);
	put(0, 4);
	for (int i = 0; i < 10; ++i) put(0, 4);
	result.mine = b.size();
	layout_patch(b, 8, static_cast<std::uint32_t>(result.mine));
	put(0, 1);
	put(8, 2);
	put(1, 2);
	for (int i = 0; i < 24; ++i) put(i * 65536, 4);
	put(255, 1);
	put(42, 1);
	put(1 << 5, 1);
	put(0, 2);
	result.vertices = b.size();
	for (int i = 0; i < 8; ++i) put(shape == 2 && i == 2 ? 0xfff8 : i, 2);
	put(1, 1);
	put(0, 2);
	for (int side = 0; side < 6; ++side) {
		result.corners[side] = b.size();
		if (version >= 25) {
			auto corners = xl_compat::classic_side_corners[side];
			if (shape == 1) std::rotate(corners.begin(), corners.begin() + 1, corners.end());
			if (shape == 2) {
				const auto end = std::remove(corners.begin(), corners.end(), 2);
				std::fill(end, corners.end(), 255);
			}
			for (const auto corner : corners) put(corner, 1);
		}
		if (side == 5) continue;
		put((side == 2 ? 0x8000 : 0) | (side + 1), 2);
		if (side == 2) put(0xc005, 2);
		for (int corner = 0; corner < 4; ++corner) {
			put(static_cast<std::uint32_t>(-32768 + corner * 100), 2);
			put(32767 - corner * 50, 2);
			put(65535 - corner, 2);
		}
	}
	result.extras = b.size();
	put(1, 1);
	put(static_cast<std::uint32_t>(producer), version >= 25 ? 2 : 1);
	put(static_cast<std::uint32_t>(value), version >= 25 ? 2 : 1);
	put(3, 1);
	put(32, 1);
	put(0xffff, 2);
	put(32767, 2);
	put(65536, 4);
	result.colors = b.size();
	for (int i = 0; i < 8 + 6 + 910; ++i) {
		put(1, 1);
		put(2147483647, 4);
		put(1073741824, 4);
		put(0, 4);
	}
	result.game = b.size();
	layout_patch(b, 12, static_cast<std::uint32_t>(result.game));
	put(0x6705, 2);
	put(40, 2);
	b.resize(b.size() + 31);
	for (int i = 0; i < 10; ++i) {
		put(0xffffffff, 4);
		put(0, 4);
		put(0, 4);
	}
	result.fog = b.size();
	if (version >= 27)
		for (int row = 0; row < 4; ++row)
			for (int value : { row * 85, 255 - row * 85, row + 127, row ? 255 : 0 }) put(value, 1);
	for (unsigned char c : std::string("Layout control\n")) put(c, 1);
	put(0, 2);
	result.walls = b.size();
	layout_patch(b, result.game + 47, static_cast<std::uint32_t>(b.size()));
	layout_patch(b, result.game + 51, 1);
	layout_patch(b, result.game + 55, 25);
	put(0, 4);
	put(0, 4);
	put(65536, 4);
	put(0xffffffff, 4);
	put(5, 1);
	put(0, 2);
	put(0, 1);
	put(255, 1);
	put(255, 1);
	put(1, 1);
	put(255, 1);
	put(0, 1);
	layout_patch(b, result.game + 71, static_cast<std::uint32_t>(b.size()));
	put(0, 4);
	return result;
}
} // namespace xl_test
#endif
