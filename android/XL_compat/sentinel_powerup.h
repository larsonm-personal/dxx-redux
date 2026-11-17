#ifndef DXX_XL_SENTINEL_POWERUP_H
#define DXX_XL_SENTINEL_POWERUP_H
#include "inventory.h"

namespace xl_compat
{
inline bool sentinel_fusion_header(int type, int id, int control, int movement, int render, int flags)
{
	return type == 7 && id == 16 && control == 255 && movement == 255 && render == 5 && flags == 0;
}
// Qualified original Fusion shape: the pinned reader consumes no union payload,
// Verify selects powerup control, and movement 255 remains stationary
inline bool sentinel_fusion_shape(const Object &object)
{
	return sentinel_fusion_header(object.type, object.id, object.control, object.movement, object.render, object.flags) &&
	       !object.multiplayer && !object.contains_count && object.movement_data.empty() && object.control_data.empty() &&
	       object.render_data == std::vector<int>{ 40, 0, 0 };
}
inline bool stationary_powerup_animation_fallback(int type, int id, int control, int movement, int render, int flags, int clip)
{
	// Pinned CObject::Read tests MAX_ANIMATIONS_D2 (110), not the native bank size
	return type == 7 && (id == 16 || id == 22) && control == 13 && movement == 0 && render == 5 && flags == 0 && (clip < 0 || clip >= 110);
}
} // namespace xl_compat
#endif
