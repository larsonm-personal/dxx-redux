#ifndef DXX_XL_TIME_EQUIPMENT_H
#define DXX_XL_TIME_EQUIPMENT_H
#include "inventory.h"
namespace xl_compat
{
// Retail HAM omits these addon entries; original Warp Shock time pickups use 4 units
// Only newly spawned pickups with absent size metadata use this portable default
constexpr int time_equipment_spawn_radius = 4 * 65536;
constexpr bool time_equipment_powerup(int id)
{
	return id == 48 || id == 49;
}
inline bool level_has_time_equipment(const Inventory &level)
{
	for (const auto &object : level.objects)
		if ((object.type == 7 && time_equipment_powerup(object.id)) ||
		    (object.contains_count > 0 && object.contains_type == 7 && time_equipment_powerup(object.contains_id))) return true;
	for (const auto &maker : level.equipment)
		if (std::uint32_t(maker[1]) & (3u << 16)) return true;
	return false;
}
} // namespace xl_compat
#endif
