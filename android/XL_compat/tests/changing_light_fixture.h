#ifndef DXX_XL_CHANGING_LIGHT_FIXTURE_H
#define DXX_XL_CHANGING_LIGHT_FIXTURE_H
#include "../inventory.h"
#include <algorithm>
#include <stdexcept>

namespace xl_test
{
// Append full-width authored tables without rewriting the original geometry
inline xl_compat::Bytes changing_light_tables(const xl_compat::Bytes &original,
                                              const std::vector<std::array<int, 4>> &indices, const std::vector<std::array<int, 7>> &deltas,
                                              bool add_actions = false)
{
	const auto source = xl_compat::inspect_level(original);
	auto bytes = original;
	const auto put = [&](int value, int width) {
		for (int i = 0; i < width; ++i) bytes.push_back(static_cast<uint8_t>(static_cast<uint32_t>(value) >> (i * 8)));
	};
	const auto patch = [&](std::size_t at, int value) {
		for (int i = 0; i < 4; ++i) bytes.at(at + i) = static_cast<uint8_t>(static_cast<uint32_t>(value) >> (i * 8));
	};
	patch(source.game_offset + 35 + 7 * 12, static_cast<int>(bytes.size()));
	patch(source.game_offset + 39 + 7 * 12, static_cast<int>(indices.size()));
	patch(source.game_offset + 43 + 7 * 12, 6);
	for (const auto &row : indices) {
		if (row[1] < 0 || row[1] >= 6 || row[2] < 0 || row[2] > (source.game_version >= 34 ? 8191 : 255) || row[3] < 0 || row[3] > 65535)
			throw std::runtime_error("Changing-light fixture index exceeds its source encoding");
		put(row[0], 2);
		if (source.game_version >= 34) put(row[1] | (row[2] << 3), 2);
		else {
			put(row[1], 1);
			put(row[2], 1);
		}
		put(row[3], 2);
	}
	patch(source.game_offset + 35 + 8 * 12, static_cast<int>(bytes.size()));
	patch(source.game_offset + 39 + 8 * 12, static_cast<int>(deltas.size()));
	patch(source.game_offset + 43 + 8 * 12, 8);
	for (const auto &row : deltas) {
		put(row[0], 2);
		for (int i = 1; i < 7; ++i) put(row[i], 1);
	}
	if (add_actions) {
		if (source.light_indices.size() < 2) throw std::runtime_error("Changing-light actions need two original emitters");
		const auto &triggers = source.sections.at(3);
		patch(source.game_offset + 35 + 3 * 12, static_cast<int>(bytes.size()));
		patch(source.game_offset + 39 + 3 * 12, triggers.count + 2);
		patch(source.game_offset + 43 + 3 * 12, 52);
		if (triggers.count) bytes.insert(bytes.end(), original.begin() + triggers.offset, original.begin() + triggers.offset + triggers.count * 52);
		for (int type : { 12, 13 }) {
			put(type, 1);
			put(8, 1);
			put(2, 1);
			put(0, 1);
			put(0, 4);
			put(-1, 4);
			for (int i = 0; i < 10; ++i) put(i < 2 ? source.light_indices[i][0] : 0, 2);
			for (int i = 0; i < 10; ++i) put(i < 2 ? source.light_indices[i][1] : 0, 2);
		}
		// Object-action count/records immediately follow the wall-trigger array
		std::size_t end = original.size();
		for (const auto &section : source.sections)
			if (section.offset > triggers.offset) end = std::min(end, static_cast<std::size_t>(section.offset));
		bytes.insert(bytes.end(), original.begin() + triggers.offset + triggers.count * 52, original.begin() + end);
	}
	return bytes;
}
} // namespace xl_test
#endif
