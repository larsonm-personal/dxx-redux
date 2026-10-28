#ifndef DXX_XL_FLICKER_FIXTURE_H
#define DXX_XL_FLICKER_FIXTURE_H
#include "../inventory.h"
#include <stdexcept>
namespace xl_test
{
inline xl_compat::Bytes flicker_table(const xl_compat::Bytes &original, const std::vector<std::array<int, 5>> &rows)
{
	const auto source = xl_compat::inspect_level(original);
	xl_compat::Bytes bytes(original.begin(), original.begin() + source.flicker_offset);
	const auto put = [&](int value, int width) {
		for (int b = 0; b < width; ++b) bytes.push_back(static_cast<uint8_t>(static_cast<uint32_t>(value) >> (b * 8)));
	};
	const auto patch = [&](std::size_t at, int value) {
		for (int b = 0; b < 4; ++b) bytes.at(at + b) = static_cast<uint8_t>(static_cast<uint32_t>(value) >> (b * 8));
	};
	put(static_cast<int>(rows.size()), 4);
	for (const auto &row : rows) {
		put(row[0], 2);
		put(row[1], 2);
		for (int i = 2; i < 5; ++i) put(row[i], 4);
	}
	const int delta = static_cast<int>(rows.size() * 16) - static_cast<int>(source.flicker_lights.size() * 16);
	bytes.insert(bytes.end(), original.begin() + source.flicker_offset + 4 + source.flicker_lights.size() * 16, original.end());
	patch(8, source.mine_offset + delta);
	patch(12, source.game_offset + delta);
	const auto offset = [&](std::size_t at) {
		uint32_t result = 0;
		for (int b = 0; b < 4; ++b) result |= uint32_t(original.at(at + b)) << (b * 8);
		return static_cast<int32_t>(result);
	};
	const int player = offset(source.game_offset + 27);
	if (player > 0) patch(source.game_offset + delta + 27, player + delta);
	for (std::size_t i = 0; i < source.sections.size(); ++i)
		if (source.sections[i].offset > 0) patch(source.game_offset + delta + 35 + i * 12, source.sections[i].offset + delta);
	return bytes;
}
} // namespace xl_test
#endif
