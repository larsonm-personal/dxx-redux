#ifndef DXX_XL_PRODUCER_LAYOUT_H
#define DXX_XL_PRODUCER_LAYOUT_H
#include "inventory.h"

namespace xl_compat
{
// Pinned CreateProducer/LoadExtSegmentsCompiled. Inspection retains raw fields;
// initialization can disable a fuel/repair function with an invalid producer ID
inline int producer_function(const Segment &segment)
{
	constexpr int reference_max_fuel_centers = 500;
	return (segment.function == 1 || segment.function == 2) && segment.producer >= reference_max_fuel_centers ? 0 : segment.function;
}

struct ProducerLayout {
	std::vector<int> robots, equipment, stations, robot_index;
};

// CreateGenerator renumbers object producers in geometry order before
// AssignProducer reads disk rows. Serialized producer/value fields do not select rows
inline ProducerLayout producer_layout(const Inventory &level)
{
	ProducerLayout result;
	result.robot_index.resize(level.segments.size(), -1);
	for (std::size_t i = 0; i < level.segments.size(); ++i) {
		const int function = producer_function(level.segments[i]);
		const bool equipment = level.level_version >= 21 && function == 11;
		if (function == 1 || function == 2 || function == 4 || equipment) result.stations.push_back(static_cast<int>(i));
		if (function == 4) {
			result.robot_index[i] = static_cast<int>(result.robots.size());
			result.robots.push_back(static_cast<int>(i));
		} else if (equipment) result.equipment.push_back(static_cast<int>(i));
	}
	return result;
}
} // namespace xl_compat
#endif
