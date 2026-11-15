#ifndef DXX_XL_NATIVE_CAMERA_SPLASH_H
#define DXX_XL_NATIVE_CAMERA_SPLASH_H
struct object;
struct vms_vector;
bool xl_camera_splash_profile(int weapon);
struct weapon_info;
bool xl_camera_splash_profile(int weapon, const weapon_info *weapons, int count);
void xl_camera_splash(object &weapon, object &camera, vms_vector &impact);
void xl_camera_splash_rotation(object &actor, const vms_vector &force);
#endif
