#ifndef MERGED_WALL_DEBUG_H
#define MERGED_WALL_DEBUG_H

#ifdef ANDROID

#include "3d.h"
#include "debug_tex_overlay.h"
#include "gr.h"
#include "ogl_texture_android.h"

struct segment;
struct g3s_point;
struct _ogl_texture;

struct merged_wall_cached_texmerge_entry {
	grs_bitmap bitmap;
	struct _ogl_texture *texture;
	grs_bitmap *bottom_bmp;
	grs_bitmap *top_bmp;
	int slot;
	int orient;
	int width;
	int height;
	fix64 last_time_used;
};

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
void android_merged_wall_consume_experiment_pending_apply(void);
void android_merged_wall_log_cached_texmerge(
    const char *event,
    grs_bitmap *bottom_bmp,
    grs_bitmap *overlay_bmp,
    int orient,
    int width,
    int height,
    GLuint handle,
    int slot,
    const struct _ogl_texture *texture);
int android_merged_wall_cached_texmerge_visible_dim(
    const struct _ogl_texture *tex,
    int use_width);
void android_merged_wall_cached_texmerge_init_bitmap(
    grs_bitmap *bm,
    struct _ogl_texture *tex,
    int flags,
    unsigned char avg_color,
    int width,
    int height);
void android_merged_wall_cached_texmerge_build_uvs(
    GLfloat *bottom_uv,
    GLfloat *overlay_uv,
    GLfloat bottom_u_max,
    GLfloat bottom_v_max,
    GLfloat overlay_u_max,
    GLfloat overlay_v_max,
    int orient);
void android_merged_wall_cached_texmerge_clear(
    struct merged_wall_cached_texmerge_entry *entries,
    int count);
int android_merged_wall_cached_texmerge_choose_size(
    const struct _ogl_texture *bottom_tex,
    const struct _ogl_texture *overlay_tex,
    int max_texture_size,
    int *width,
    int *height);
void android_merged_wall_cached_texmerge_reset_entry(
    struct merged_wall_cached_texmerge_entry *entry);
grs_bitmap *android_merged_wall_cached_texmerge_try_reuse(
    struct merged_wall_cached_texmerge_entry *entries, int count,
    grs_bitmap *bottom_bmp, grs_bitmap *overlay_bmp, int orient,
    int *out_slot);
void android_merged_wall_cached_texmerge_commit_entry(
    struct merged_wall_cached_texmerge_entry *entry,
    grs_bitmap *bottom_bmp, grs_bitmap *overlay_bmp, int orient,
    int width, int height, int bitmap_flags, unsigned char avg_color,
    int *out_slot);
void android_merged_wall_cached_texmerge_set_render_filters(
    struct _ogl_texture *tex, int texfilt_level);
void android_merged_wall_cached_texmerge_finalize_filters(
    struct _ogl_texture *tex, int texfilt_level, int aniso_level,
    float max_anisotropy);
void android_merged_wall_cached_texmerge_setup_output_texture(
    struct _ogl_texture *tex, int width, int height, int tex_flags,
    int texfilt_level,
    const struct android_ogl_texture_runtime_state *runtime_state);
int android_merged_wall_cached_texmerge_render_to_texture(
    struct _ogl_texture *output_tex, grs_bitmap *bottom_bmp,
    grs_bitmap *overlay_bmp, int orient, int width, int height,
    int texfilt_level, int aniso_level, float max_anisotropy,
    const struct android_ogl_texture_runtime_state *runtime_state);
int android_merged_wall_cached_texmerge_finalize_entry(
    struct merged_wall_cached_texmerge_entry *entry,
    grs_bitmap *bottom_bmp, grs_bitmap *overlay_bmp, int orient,
    int width, int height, int texfilt_level, int aniso_level,
    float max_anisotropy, int bitmap_flags, unsigned char avg_color,
    const struct android_ogl_texture_runtime_state *runtime_state,
    int *out_slot, void (*free_texture)(struct _ogl_texture *));
struct merged_wall_cached_texmerge_entry *
android_merged_wall_cached_texmerge_reserve_entry(
    struct merged_wall_cached_texmerge_entry *entries, int count,
    void (*free_texture)(struct _ogl_texture *));
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
void android_merged_wall_get_filter_state(const struct _ogl_texture *tex,
                                          int *min_filter, int *mag_filter);
void android_merged_wall_get_draw_state(const struct _ogl_texture *tex,
                                        int *active_prog, int *bound_tex0,
                                        int *bound_tex1, int *bound_tex2,
                                        int *mip1_w);
void android_merged_wall_get_gl_state(int *depth_enabled, int *blend_enabled,
                                      int *cull_enabled,
                                      unsigned char *depth_writemask,
                                      int *depth_func, int *front_face,
                                      int *cull_mode,
                                      int *polygon_offset_enabled,
                                      float *polygon_offset_factor,
                                      float *polygon_offset_units,
                                      unsigned char color_mask[4],
                                      int *draw_fbo);
float android_merged_wall_get_triangle_area(const struct g3s_point *p0,
                                            const struct g3s_point *p1,
                                            const struct g3s_point *p2);
void android_merged_wall_get_uv_points(const g3s_uvl *uvl_list,
                                       const float *texcoordovl_array,
                                       int nv, float *raw_pts,
                                       float *ovl_pts, int max_points,
                                       int *uv_bad);
void android_merged_wall_log_palette_source(const char *bitmapname,
                                            const unsigned char *data,
                                            int width, int height,
                                            int bm_flags,
                                            unsigned int bit,
                                            const char *source);
void android_merged_wall_log_alpha_source(const char *bitmapname,
                                          const unsigned char *data,
                                          int width, int height,
                                          int channels, int bm_flags,
                                          unsigned int bit,
                                          const char *source);
void android_merged_wall_log_submit(
    const struct merged_wall_tmap2_submit_context *ctx,
    const struct g3s_point *const *pointlist, int nv);
void android_merged_wall_log_split(
    const struct merged_wall_tmap2_submit_context *ctx,
    const struct g3s_point *const *pointlist, int nv);
void android_merged_wall_log_diag(grs_bitmap *bmbot, grs_bitmap *bmovl,
                                  const g3s_uvl *uvl_list,
                                  const float *texcoordovl_array, int nv,
                                  int orient, int super,
                                  unsigned int prog, int tex2_debug_mode,
                                  int texfilt_level, int aniso_level);
void android_merged_wall_log_state(
    const struct merged_wall_tmap2_submit_context *ctx,
    float screen_area, int depth_enabled, int blend_enabled,
    int cull_enabled, unsigned char depth_writemask, int depth_func,
    int front_face, int cull_mode, int polygon_offset_enabled,
    float polygon_offset_factor, float polygon_offset_units,
    const unsigned char color_mask[4], int draw_fbo,
    int force_cull_off, int force_polygon_offset, int force_depth_off);
void android_merged_wall_clear_secondary_units_for_single(grs_bitmap *bm);
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