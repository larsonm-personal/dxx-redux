#ifndef DXX_XL_CAMERA_ADMISSION_H
#define DXX_XL_CAMERA_ADMISSION_H
#include "inventory.h"
namespace xl_compat
{
inline bool qualified_camera_layout(const Inventory &level)
{
	return legacy_game35_layout(level) || modern_runtime_layout(level) || (level.level_version == 18 && level.game_version == 36);
}

inline bool qualified_camera_object(const Object &object)
{
	return object.type == 16 && object.control == 1 && object.movement == 0 && object.render == 1 && object.flags == 0 &&
	       !object.multiplayer && !object.contains_count && object.control_data.size() == 16 && object.render_data.size() == 33 &&
	       (object.control_data[0] == 128 || object.control_data[0] == 129 || object.control_data[0] == 135);
}
inline bool qualified_camera_binding(const Inventory &level, const ObjectTrigger &record)
{
	if (!qualified_camera_layout(level) || record.action.type != 16 || record.object < 0 || std::size_t(record.object) >= level.objects.size() ||
	    record.action.flags || record.action.time != -1 || record.action.targets.empty()) return false;
	const auto &owner = level.objects[record.object];
	return qualified_camera_object(owner) || (modern_runtime_layout(level) && (owner.type == 2 || owner.type == 20));
}
} // namespace xl_compat
#endif
