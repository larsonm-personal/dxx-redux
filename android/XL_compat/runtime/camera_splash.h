#ifndef DXX_XL_NATIVE_CAMERA_SPLASH_H
#define DXX_XL_NATIVE_CAMERA_SPLASH_H
struct object;
struct vms_vector;
void xl_camera_splash(object &weapon, object &camera, vms_vector &impact);
void xl_camera_splash_rotation(object &actor, const vms_vector &force);
#endif
