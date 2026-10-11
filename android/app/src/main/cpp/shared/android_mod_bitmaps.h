/* Android indexed mod textures used by rendering and CPU transparency tests */
#ifndef ANDROID_MOD_BITMAPS_H
#define ANDROID_MOD_BITMAPS_H
#ifdef __cplusplus
extern "C" {
#endif
int android_mod_bitmaps_apply(int content_game, const char *pig_name);
void android_mod_bitmaps_reset(void);
void android_mod_bitmaps_overlay_flags(int *flags, int capacity);
void android_mod_bitmaps_refresh_palette(void);
int android_mod_bitmap_is_owned(int index);
#ifdef __cplusplus
}
#endif
#endif
