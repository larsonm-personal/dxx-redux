#ifndef DXX_XL_OBJECT_CAPACITY_FIXTURE_H
#define DXX_XL_OBJECT_CAPACITY_FIXTURE_H
#include "../inventory.h"
#include <algorithm>
#include <stdexcept>

namespace xl_test
{
inline xl_compat::Bytes expand_objects(const xl_compat::Bytes &original, unsigned count)
{
	const auto source = xl_compat::inspect_level(original);
	if (count < source.objects.size() || count > 20001) throw std::runtime_error("Invalid object fixture count");
	auto bytes = original;
	const auto patch = [&](std::size_t at, unsigned value) {
		for (unsigned i = 0; i < 4; ++i) bytes.at(at + i) = uint8_t(value >> (8 * i));
	};
	const unsigned base = source.game_version == 40 ? 80 : 79;
	const unsigned table = source.objects.empty() ? unsigned(source.sections[3].offset) : unsigned(source.sections[0].offset);
	const unsigned end = source.objects.empty() ? table : unsigned(source.objects.back().source_end);
	xl_compat::Bytes row(base, 0);
	if (!source.objects.empty()) std::copy_n(original.begin() + source.objects[0].source_offset, base, row.begin());
	row[0] = 13; // Inert light object, no control/movement/render payload
	for (unsigned i = 1; i < 6 + (base == 80); ++i) row[i] = 0;
	row[base - 1] = row[base - 2] = row[base - 3] = 0;
	xl_compat::Bytes added;
	added.reserve((count - source.objects.size()) * base);
	for (unsigned i = unsigned(source.objects.size()); i < count; ++i) added.insert(added.end(), row.begin(), row.end());
	bytes.insert(bytes.begin() + end, added.begin(), added.end());
	for (std::size_t section = 1; section < source.sections.size(); ++section)
		if (source.sections[section].offset >= int(end))
			patch(source.game_offset + 35 + section * 12, unsigned(source.sections[section].offset) + unsigned(added.size()));
	unsigned player = 0;
	for (unsigned i = 0; i < 4; ++i) player |= unsigned(original.at(source.game_offset + 27 + i)) << (8 * i);
	if (int32_t(player) >= int(end)) patch(source.game_offset + 27, player + unsigned(added.size()));
	patch(source.game_offset + 35, table);
	patch(source.game_offset + 39, count);
	return bytes;
}
} // namespace xl_test
#endif
