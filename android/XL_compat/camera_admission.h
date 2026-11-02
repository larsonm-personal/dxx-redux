#ifndef DXX_XL_CAMERA_ADMISSION_H
#define DXX_XL_CAMERA_ADMISSION_H
#include "inventory.h"
namespace xl_compat
{
inline bool qualified_camera_object(const Object &object)
{
	return object.type == 16 && object.control == 1 && object.movement == 0 && object.render == 1 && object.flags == 0 &&
	       !object.multiplayer && !object.contains_count && object.control_data.size() == 16 && object.render_data.size() == 33 &&
	       (object.control_data[0] == 128 || object.control_data[0] == 129);
}
inline bool qualified_camera_binding(const Inventory &level, const ObjectTrigger &record)
{
	return modern_runtime_layout(level) && record.action.type == 16 && record.object >= 0 && std::size_t(record.object) < level.objects.size() &&
	       qualified_camera_object(level.objects[record.object]) && record.action.flags == 0 && record.action.time == -1 &&
	       !record.action.targets.empty();
}
} // namespace xl_compat
#endif
