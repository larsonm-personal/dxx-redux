#ifndef ANDROID_TEXTURE_DEBUG_H
#define ANDROID_TEXTURE_DEBUG_H

#ifdef ANDROID

#include "3d.h"
#include "gr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ANDROID_TEXTURE_DEBUG_TARGET_NONE      0
#define ANDROID_TEXTURE_DEBUG_TARGET_NAME      1
#define ANDROID_TEXTURE_DEBUG_TARGET_CROSSHAIR 2
#define ANDROID_TEXTURE_DEBUG_TARGET_NAME_MAX  24

extern float g_font_rgb_override[3];
extern int g_ogl_render_context;
void android_texture_debug_set_target(const char *value);
const char *android_texture_debug_get_target_display(void);
int android_texture_debug_target_is_crosshair(void);
int android_texture_debug_matches_target_name(const char *bitmapname);
int android_texture_debug_get_label_anchor(const g3s_point *const *pointlist,
                                           int nv, int *sx, int *sy);
void android_texture_debug_add_overlay_label(const g3s_point *const *pointlist,
                                             int nv, grs_bitmap *bm,
                                             int y_offset);
void android_texture_debug_add_joined_labels(const g3s_point *const *pointlist,
                                             int nv, grs_bitmap *bmbot,
                                             grs_bitmap *bmovl);
void android_texture_debug_log_render_bind(int *render_log_count,
                                           grs_bitmap *bm);

#ifdef __cplusplus
}
#endif

#endif /* ANDROID */

#endif /* ANDROID_TEXTURE_DEBUG_H */
