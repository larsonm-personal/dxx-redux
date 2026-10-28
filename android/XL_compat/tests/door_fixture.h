#ifndef DXX_XL_DOOR_FIXTURE_H
#define DXX_XL_DOOR_FIXTURE_H
#include "cloak_fixture.h"

namespace xl_test
{
// Reuse the audited portal/table writer, then make the added pairs animated doors
inline xl_compat::Bytes add_door_pairs(const xl_compat::Bytes &original, unsigned pairs)
{
	const auto source = xl_compat::inspect_level(original);
	if (source.walls.empty() || source.walls[0].clip < 0) throw std::runtime_error("Door fixture needs an authored wall clip");
	auto bytes = add_cloak_pairs(original, pairs);
	const auto expanded = xl_compat::inspect_level(bytes);
	for (std::size_t i = source.walls.size(); i < expanded.walls.size(); ++i) {
		const auto row = expanded.sections[1].offset + i * 25;
		bytes.at(row + 16) = 2;
		bytes.at(row + 21) = static_cast<uint8_t>(source.walls[0].clip);
		bytes.at(row + 24) = 31;
	}
	return bytes;
}
} // namespace xl_test
#endif
