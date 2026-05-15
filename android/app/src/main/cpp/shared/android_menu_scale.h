#ifndef ANDROID_MENU_SCALE_H
#define ANDROID_MENU_SCALE_H

#include "gr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct android_menu_scale_rect {
	int x;
	int y;
	int w;
	int h;
} android_menu_scale_rect;

typedef struct android_menu_scale_result {
	int active;
	android_menu_scale_rect box;
	android_menu_scale_rect src;
	android_menu_scale_rect dst;
	int crop_left;
	int crop_top;
	float scale;
} android_menu_scale_result;

float android_menu_scale_get_target_fill(void);
int android_menu_scale_compute_cropped(int source_x, int source_y, int source_w,
                                       int source_h, int screen_w, int screen_h, int border_x, int border_y,
                                       android_menu_scale_result *result);
int android_menu_scale_compute_kconfig(int source_x, int source_y, int source_w,
                                       int source_h, int screen_w, int screen_h, android_menu_scale_result *result);
void android_menu_scale_publish(const android_menu_scale_result *result);
void android_menu_scale_clear(void);
int android_menu_scale_get_state(android_menu_scale_result *result);
void android_menu_scale_blit_bitmap(grs_bitmap *bitmap,
                                    const android_menu_scale_result *result, int masked);

#ifdef __cplusplus
}
#endif

#endif