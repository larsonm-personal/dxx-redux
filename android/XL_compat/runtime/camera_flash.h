#ifndef DXX_XL_NATIVE_CAMERA_FLASH_H
#define DXX_XL_NATIVE_CAMERA_FLASH_H
#ifdef __cplusplus
extern "C" {
#endif
int xl_camera_flash_add(int red, int green, int blue);
int xl_camera_flash_frame(void);
void xl_camera_flash_palette_set(void);
void xl_camera_flash_clear(void);
#ifdef __cplusplus
}
#include <nlohmann/json_fwd.hpp>
void xl_camera_flash_blast(int force, int flash, bool self);
nlohmann::json xl_camera_flash_snapshot();
bool xl_camera_flash_restore(const nlohmann::json &state, bool apply);
#endif
#endif
