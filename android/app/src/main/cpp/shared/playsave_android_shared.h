#ifndef PLAYSAVE_ANDROID_SHARED_H
#define PLAYSAVE_ANDROID_SHARED_H

/* Shared Android playsave helpers. Implementation lives in playsave_android_shared.c. */

void android_get_default_pilot_prefs(int *cockpit_mode, int *auto_leveling);
void android_get_default_visual_prefs(int *alpha_effects, int *dynlight_color);
int plx_read_visual_prefs(const char *path, int *alpha_effects, int *dynlight_color);
int plx_write_visual_prefs(const char *path, int alpha_effects, int dynlight_color);
int playsave_android_read_u16le(FILE *f, int *value);
int playsave_android_read_u32le(FILE *f, unsigned int *value);
int playsave_android_patch_keysettings_common(FILE *f, long ks_base,
                                              long control_type_offset, const ubyte *kb, int kb_len, const ubyte *joy,
                                              int joy_len, const ubyte *mouse, int mouse_len, int control_type);

#endif /* PLAYSAVE_ANDROID_SHARED_H */