#ifndef MERGED_WALL_DEBUG_H
#define MERGED_WALL_DEBUG_H

#ifdef ANDROID

#include "3d.h"
#include "debug_tex_overlay.h"
#include "gr.h"

struct segment;
struct g3s_point;

extern volatile int g_merged_wall_force_two_pass;

int android_merged_wall_is_logging_target_tmap2(int tmap2);
void android_merged_wall_set_draw_face_context(struct segment *segp, int sidenum,
                                               int tmap1, int tmap2, int wid_flags, int nv, int face_index);
void android_merged_wall_clear_draw_face_context(void);
void android_merged_wall_log_face(struct segment *segp, int sidenum, int tmap1,
                                  int tmap2, int wid_flags, float dot, int nv, int face_index);
int android_merged_wall_next_draw_order(void);
void android_merged_wall_track_face(const struct g3s_point **pointlist, int nv,
                                    int draw_order, const char *route, const char *merge_impl,
                                    const char *decision_reason);
void android_merged_wall_log_cover(const char *shader_kind, const char *botname,
                                   const char *ovlname, const struct g3s_point **pointlist, int nv,
                                   const g3s_uvl *uvl_list, int draw_order, grs_bitmap *cover_bitmap,
                                   int texfilt_level, int menu_texfilt, int hud_texfilt,
                                   int aniso_level);
void android_merged_wall_finish_snapshot(int screen_w, int screen_h,
                                         int sample_r, int sample_g, int sample_b, int sample_a,
                                         int avg_r, int avg_g, int avg_b, int avg_a);

#endif /* ANDROID */

#endif /* MERGED_WALL_DEBUG_H */