#ifndef ANDROID_TEXTURE_DEBUG_H
#define ANDROID_TEXTURE_DEBUG_H

#ifdef ANDROID

#include "gr.h"
#include "ogl_init.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ANDROID_TEXTURE_DEBUG_TARGET_NONE      0
#define ANDROID_TEXTURE_DEBUG_TARGET_NAME      1
#define ANDROID_TEXTURE_DEBUG_TARGET_CROSSHAIR 2
#define ANDROID_TEXTURE_DEBUG_TARGET_NAME_MAX  24

void android_texture_debug_set_target(const char *value);
const char *android_texture_debug_get_target_display(void);
int android_texture_debug_target_is_crosshair(void);
int android_texture_debug_matches_target_name(const char *bitmapname);

void android_texture_debug_log_upload_source(const char *bitmapname,
                                             const char *path, const char *source_name, const unsigned char *data,
                                             int width, int height, int row_stride, int bm_flags, int real_flags);
void android_texture_debug_log_upload_expanded(const char *bitmapname,
                                               const char *path, ogl_texture *tex, const unsigned char *data,
                                               int bm_flags, int data_format);
void android_texture_debug_log_mip_upload(const char *bitmapname,
                                          const char *path, grs_bitmap *bm, int texfilt, int load_texfilt,
                                          int source_levels, int uploaded_levels, int generated_mips,
                                          int compressed_upload);

#ifdef __cplusplus
}
#endif

#endif /* ANDROID */

#endif /* ANDROID_TEXTURE_DEBUG_H */
