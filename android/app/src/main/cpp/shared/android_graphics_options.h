#ifndef ANDROID_GRAPHICS_OPTIONS_H
#define ANDROID_GRAPHICS_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

enum android_graphics_option_result {
	ANDROID_GRAPHICS_OPTION_UNKNOWN = 0,
	ANDROID_GRAPHICS_OPTION_OK = 1,
	ANDROID_GRAPHICS_OPTION_PERSIST_FAILED = 2
};

int android_graphics_set_option(const char *name, int value, int persist);
int android_graphics_set_aniso_level(int value, int persist);
int android_graphics_set_msaa_level(int value, int persist);
int android_graphics_set_texfilt(int value, int persist);
int android_graphics_set_gamma_level(int value, int persist);
int android_graphics_set_menu_texfilt(int value, int persist);
int android_graphics_set_hud_texfilt(int value, int persist);
int android_graphics_set_main_view_fov(int value, int persist);
int android_graphics_set_main_view_fov_locked(int value, int persist);
int android_graphics_set_corner_text_inset(int value, int persist);
int android_graphics_set_classic_depth(int value, int persist);
int android_graphics_set_alpha_effects(int value, int persist);
int android_graphics_set_dynlight_color(int value, int persist);
int android_graphics_set_movie_texfilt(int value, int persist);
void android_graphics_apply_pilot_defaults(void);
void android_graphics_set_rounded_corner_text_insets(int surface_width, int surface_height,
                                                     int top_left_px, int bottom_left_px,
                                                     int top_right_px, int bottom_right_px);
int android_graphics_get_corner_text_left_inset(int canvas_width, int canvas_height, int y, int h);
int android_graphics_get_corner_text_right_inset(int canvas_width, int canvas_height, int y, int h);

#ifdef __cplusplus
}
#endif

#endif
