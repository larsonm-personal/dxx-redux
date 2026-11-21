#ifndef DXX_XL_NATIVE_CAMERA_CHILDREN_H
#define DXX_XL_NATIVE_CAMERA_CHILDREN_H
/* XL weapon definitions use zero as well as -1 for an absent child missile */
static inline int xl_weapon_defines_children(int child_id)
{
	return child_id > 0;
}
#ifdef __cplusplus
struct object;
void xl_camera_smart_children(object &weapon);
#endif
#endif
