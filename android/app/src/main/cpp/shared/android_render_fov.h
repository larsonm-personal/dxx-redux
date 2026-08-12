#ifndef ANDROID_RENDER_FOV_H
#define ANDROID_RENDER_FOV_H

#include <stdint.h>

void android_render_set_main_view_fov(int fov_degrees);
int android_render_get_main_view_fov(void);
void android_render_set_main_view_fov_locked(int locked_to_base);
int android_render_main_view_fov_effective(void);
int32_t android_render_main_view_zoom(int32_t base_zoom);

#endif
