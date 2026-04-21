#ifndef MERGED_WALL_DEBUG_H
#define MERGED_WALL_DEBUG_H

#ifdef ANDROID

#include "3d.h"
#include "debug_tex_overlay.h"
#include "gr.h"

struct segment;
struct g3s_point;

struct merged_wall_tmap2_submit_context {
	const char *route;
	int orig_nv;
	unsigned int orig_uor;
	unsigned int orig_uand;
	int input_behind;
	int temp_points;
	int clip_applied;
	unsigned int upload_id;
};

extern volatile int g_merged_wall_force_two_pass;

const char *android_merged_wall_experiment_name(int mode);
int android_merged_wall_is_logging_target_bitmap(grs_bitmap *bm);
int android_merged_wall_is_logging_target_tmap2(int tmap2);
void android_merged_wall_reset_tmap2_submit_context(struct merged_wall_tmap2_submit_context *ctx);
void android_merged_wall_set_tmap2_submit_context(struct merged_wall_tmap2_submit_context *ctx,
                                                  const char *route, int orig_nv,
                                                  const g3s_codes *cc, int input_behind,
                                                  int temp_points, int clip_applied);
void android_merged_wall_log_tmap2_route(const char *route, grs_bitmap *bmbot,
                                         grs_bitmap *bmovl, int nv, int orient);
void android_merged_wall_log_upload(struct merged_wall_tmap2_submit_context *ctx,
                                    grs_bitmap *bmbot, grs_bitmap *bmovl,
                                    unsigned int prog, unsigned int merge_vbo,
                                    int nv, int orient, int vb, int cb, int tb,
                                    int t2b);
void android_merged_wall_get_source_palette_counts(grs_bitmap *bm, int *idx254,
                                                   int *idx255, int *real_flags);
int android_merged_wall_get_source_alpha_class(unsigned char idx);
float android_merged_wall_get_source_alpha_value(unsigned char idx);
void android_merged_wall_get_source_sample(grs_bitmap *bm,
                                           const float *texcoordovl_array, int nv,
                                           float *avg_u, float *avg_v,
                                           int *sample_x, int *sample_y,
                                           int *sample_idx);
void android_merged_wall_get_source_filter_sample(grs_bitmap *bm, float sample_u,
                                                  float sample_v, int *idx00,
                                                  int *idx10, int *idx01,
                                                  int *idx11, float *alpha,
                                                  int *wrap_u, int *wrap_v);
void android_merged_wall_get_source_vslice(grs_bitmap *bm, float sample_u,
                                           float min_v, float max_v,
                                           int *sample_rows, int *sample_idxs,
                                           int nsamples);
void android_merged_wall_get_input_codes(const struct g3s_point *const *pointlist,
                                         int nv, g3s_codes *cc, int *input_behind);
void android_merged_wall_get_point_code_summary(const struct g3s_point *const *pointlist,
                                                int nv, unsigned int *uor,
                                                unsigned int *uand,
                                                int *behind_count,
                                                int *temp_points);
float android_merged_wall_get_screen_area(const struct g3s_point *const *pointlist,
                                          int nv);
void android_merged_wall_set_draw_face_context(struct segment *segp, int sidenum,
                                               int tmap1, int tmap2, int wid_flags, int nv, int face_index);
void android_merged_wall_clear_draw_face_context(void);
void android_merged_wall_log_face(struct segment *segp, int sidenum, int tmap1,
                                  int tmap2, int wid_flags, float dot, int nv, int face_index);
int android_merged_wall_next_draw_order(void);
void android_merged_wall_track_face(const struct g3s_point **pointlist, int nv,
                                    const g3s_uvl *uvl_list, int orient,
                                    int draw_order, const char *route, const char *merge_impl,
                                    const char *decision_reason, grs_bitmap *merged_bitmap,
                                    int merged_slot);
void android_merged_wall_log_cover(const char *shader_kind, const char *botname,
                                   const char *ovlname, const struct g3s_point **pointlist, int nv,
                                   const g3s_uvl *uvl_list, const float *color_array,
                                   int draw_order, grs_bitmap *cover_bitmap,
                                   int texfilt_level, int menu_texfilt, int hud_texfilt,
                                   int aniso_level);
void android_merged_wall_finish_snapshot(int screen_w, int screen_h,
                                         int sample_r, int sample_g, int sample_b, int sample_a,
                                         int avg_r, int avg_g, int avg_b, int avg_a);

#endif /* ANDROID */

#endif /* MERGED_WALL_DEBUG_H */