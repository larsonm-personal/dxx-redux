#ifndef DXX_XL_TRIGGER_CAPACITY_FIXTURE_H
#define DXX_XL_TRIGGER_CAPACITY_FIXTURE_H
#include "../inventory.h"
#include <algorithm>
#include <stdexcept>

namespace xl_test
{
// Preserve authored rows and the object-trigger trailer while expanding the table
inline xl_compat::Bytes expand_triggers(const xl_compat::Bytes &original, unsigned count)
{
	const auto source = xl_compat::inspect_level(original);
	if (count < source.triggers.size() || count > 65535 || source.sections[3].offset < 0)
		throw std::runtime_error("Invalid trigger fixture count");
	auto bytes = original;
	const auto patch = [&](std::size_t at, unsigned value) {
		for (unsigned i = 0; i < 4; ++i) bytes.at(at + i) = uint8_t(value >> (8 * i));
	};
	const unsigned table = unsigned(source.sections[3].offset);
	const unsigned old_end = table + unsigned(source.triggers.size()) * 52;
	const unsigned added = (count - unsigned(source.triggers.size())) * 52;
	bytes.insert(bytes.begin() + old_end, added, 0);
	for (std::size_t section = 0; section < source.sections.size(); ++section)
		if (section != 3 && source.sections[section].offset >= int(old_end))
			patch(source.game_offset + 35 + section * 12, unsigned(source.sections[section].offset) + added);
	unsigned player_offset = 0;
	for (unsigned i = 0; i < 4; ++i) player_offset |= unsigned(bytes.at(source.game_offset + 27 + i)) << (8 * i);
	if (int32_t(player_offset) >= int(old_end)) patch(source.game_offset + 27, player_offset + added);
	patch(source.game_offset + 75, count);
	patch(source.game_offset + 79, 52);
	for (unsigned index = unsigned(source.triggers.size()); index < count; ++index) {
		const auto row = table + index * 52;
		bytes.at(row) = 13; // Inert light-on row with no targets
		patch(row + 8, unsigned(-1));
	}
	return bytes;
}
} // namespace xl_test
#endif
