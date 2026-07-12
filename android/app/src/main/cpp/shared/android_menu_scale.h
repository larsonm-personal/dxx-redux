#ifndef ANDROID_MENU_SCALE_H
#define ANDROID_MENU_SCALE_H

#include "gr.h"
#include "newmenu.h"

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
	int direct_render;
	android_menu_scale_rect box;
	android_menu_scale_rect src;
	android_menu_scale_rect dst;
	int render_w;
	int render_h;
	int crop_left;
	int crop_top;
	float scale;
	float render_scale;
} android_menu_scale_result;

typedef struct android_menu_scale_draw_state {
	unsigned int screen_w;
	unsigned int screen_h;
	float fnt_scale_x;
	float fnt_scale_y;
} android_menu_scale_draw_state;

typedef void (*android_menu_scale_canvas_draw_fn)(void *userdata,
                                                  grs_canvas *canvas);
typedef int (*android_menu_scale_result_draw_fn)(
    void *userdata, grs_canvas *canvas,
    const android_menu_scale_result *result);

float android_menu_scale_get_target_fill(void);
int android_menu_scale_compute_cropped(int source_x, int source_y, int source_w,
                                       int source_h, int screen_w, int screen_h, int border_x, int border_y,
                                       android_menu_scale_result *result);
int android_menu_scale_compute_kconfig(int source_x, int source_y, int source_w,
                                       int source_h, int screen_w, int screen_h, int *scroll_y,
                                       android_menu_scale_result *result);
int android_menu_scale_draw_kconfig(int source_x, int source_y, int source_w,
                                    int source_h, int screen_w, int screen_h,
                                    int *scroll_y, grs_canvas *window_canvas,
                                    android_menu_scale_canvas_draw_fn draw_contents,
                                    void *userdata);
int android_menu_scale_draw_result(
    android_menu_scale_result *result, int screen_w, int screen_h,
    int source_masked, int render_masked,
    android_menu_scale_canvas_draw_fn draw_source,
    android_menu_scale_result_draw_fn draw_scaled, void *userdata);
void android_menu_scale_scroll_by(int *scroll_y, int delta_y);
void android_menu_scale_publish(const android_menu_scale_result *result);
void android_menu_scale_clear(void);
int android_menu_scale_get_state(android_menu_scale_result *result);
int android_menu_scale_begin_scaled_draw(float scale, android_menu_scale_draw_state *state);
void android_menu_scale_end_scaled_draw(const android_menu_scale_draw_state *state);
void android_menu_scale_blit_bitmap(grs_bitmap *bitmap,
                                    const android_menu_scale_result *result, int masked);
void android_menu_scale_blit_bitmap_region(grs_bitmap *bitmap,
                                           const android_menu_scale_result *result,
                                           int source_y);
void android_menu_scale_blit_source_region(grs_bitmap *bitmap,
                                           const android_menu_scale_result *result, int masked);
int android_menu_scale_round_coord(int value, float scale);
void android_menu_scale_items(newmenu_item *dst, const newmenu_item *src,
                              int count, float scale);

#ifdef __cplusplus
}
#endif

#endif
