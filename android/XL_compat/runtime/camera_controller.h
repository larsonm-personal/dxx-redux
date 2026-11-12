#ifndef DXX_XL_CAMERA_CONTROLLER_H
#define DXX_XL_CAMERA_CONTROLLER_H
#ifdef __cplusplus
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "../camera_object_state.h"
#include "../camera_orientation.h"
#include "../inventory.h"
struct XlCameraControllerState {
	int monitor = -1, owner = -1, signature = -1, type = -1, id = -1;
	xl_compat::CameraRotation rotation;
	xl_compat::CameraMatrix original{};
};
void xl_camera_controller_load(const xl_compat::Inventory &level);
bool xl_camera_controller_get(int owner, XlCameraControllerState &state);
#ifdef _MSC_VER
#pragma pack(pop)
#endif
extern "C" {
#endif
void xl_camera_controller_clear(void);
void xl_camera_controller_bind_level(void);
void xl_camera_controller_begin_frame(void);
unsigned xl_camera_controller_ticks(void);
void xl_camera_controller_initial_ticks(unsigned ticks);
void xl_camera_controller_frame(int owner);
int xl_camera_controller_restore(int apply);
#ifdef __cplusplus
}
#endif
#endif
