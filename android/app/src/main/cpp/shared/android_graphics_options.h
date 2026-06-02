#ifndef ANDROID_GRAPHICS_OPTIONS_H
#define ANDROID_GRAPHICS_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

int android_graphics_set_option(const char *name, int value, int persist);
void android_graphics_set_aniso_level(int value, int persist);
void android_graphics_set_msaa_level(int value, int persist);
void android_graphics_set_texfilt(int value, int persist);
void android_graphics_set_gamma_level(int value, int persist);
void android_graphics_set_menu_texfilt(int value, int persist);
void android_graphics_set_hud_texfilt(int value, int persist);
void android_graphics_set_corner_text_inset(int value, int persist);
void android_graphics_set_classic_depth(int value, int persist);
void android_graphics_set_alpha_effects(int value, int persist);
void android_graphics_set_dynlight_color(int value, int persist);
void android_graphics_set_movie_texfilt(int value, int persist);
void android_graphics_apply_pilot_defaults(void);
void android_graphics_set_rounded_corner_text_insets(int surface_width, int left_px, int right_px);
int android_graphics_get_corner_text_left_inset(int canvas_width);
int android_graphics_get_corner_text_right_inset(int canvas_width);

#ifdef __cplusplus
}
#endif

#endif
