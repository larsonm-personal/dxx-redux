#ifndef PLAYSAVE_ANDROID_SHARED_H
#define PLAYSAVE_ANDROID_SHARED_H

/* Shared Android playsave helpers. Implementation lives in playsave_android_shared.c. */

void android_get_default_pilot_prefs(int *cockpit_mode, int *auto_leveling);
void android_get_default_visual_prefs(int *alpha_effects, int *dynlight_color);
void android_get_default_hud_count_prefs(int *show_counts);
void android_get_default_music_prefs(int *source, int *prefer_mission, int *play_order, int *volume);
int plx_read_robot_hostage_counts(const char *path, int *show_counts);
int plx_write_robot_hostage_counts(const char *path, int show_counts);
int plx_read_visual_prefs(const char *path, int *alpha_effects, int *dynlight_color);
int plx_write_visual_prefs(const char *path, int alpha_effects, int dynlight_color);
int plx_read_music_prefs(const char *path, int *source, int *prefer_mission, int *play_order, int *volume);
int plx_write_music_prefs(const char *path, int source, int prefer_mission, int play_order, int volume);
int playsave_android_read_u16le(FILE *f, int *value);
int playsave_android_read_u32le(FILE *f, unsigned int *value);
int playsave_android_patch_keysettings_common(FILE *f, long ks_base,
                                              long control_type_offset, const ubyte *kb, int kb_len, const ubyte *joy,
                                              int joy_len, const ubyte *mouse, int mouse_len, int control_type);

#endif /* PLAYSAVE_ANDROID_SHARED_H */
