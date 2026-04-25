#ifndef ANDROID_GRAPHICS_OPTIONS_H
#define ANDROID_GRAPHICS_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

int android_graphics_set_option(const char *name, int value, int persist);
void android_graphics_set_aniso_level(int value, int persist);
void android_graphics_set_msaa_level(int value, int persist);
void android_graphics_set_texfilt(int value, int persist);
void android_graphics_set_menu_texfilt(int value, int persist);
void android_graphics_set_hud_texfilt(int value, int persist);
void android_graphics_set_classic_depth(int value, int persist);
void android_graphics_set_alpha_effects(int value, int persist);
void android_graphics_set_dynlight_color(int value, int persist);
void android_graphics_set_movie_texfilt(int value, int persist);
void android_graphics_apply_pilot_defaults(void);

#ifdef __cplusplus
}
#endif

#endif