/* debug_tex_overlay.h -- per-texture debug label overlay
 *
 * During 3D rendering, g3_draw_tmap accumulates screen positions and
 * texture names into g_debug_tex_labels[].  After 3D rendering (in 2D
 * mode), the labels are drawn as colored text centered on each polygon.
 * Green = hires PNG replacement, yellow = base game texture.
 *
 * Toggle via automation "set_debug" or adb broadcast GAME_COMMAND. */

#ifndef DEBUG_TEX_OVERLAY_H
#define DEBUG_TEX_OVERLAY_H

#ifdef ANDROID

#define DEBUG_TEX_MAX_LABELS 256

struct debug_tex_label {
	int sx, sy;   /* screen position (center of polygon) */
	int is_hires; /* 1 = hires PNG, 0 = base game */
	int seg;
	int side;
	int face;
	int anchor_group;   /* 0 = normal label, >0 = merged label line */
	int anchor_samples; /* contributors accumulated into this anchor */
	char name[24];      /* truncated texture name */
};

extern struct debug_tex_label g_debug_tex_labels[DEBUG_TEX_MAX_LABELS];
extern int g_debug_tex_label_count;
extern volatile int g_debug_tex_overlay_active;

#define MERGED_WALL_DEBUG_NONE          0
#define MERGED_WALL_DEBUG_OVERLAY_ALPHA 1
#define MERGED_WALL_DEBUG_OVERLAY_RGB   2

#define MERGED_WALL_EXPERIMENT_DEFAULT                      0
#define MERGED_WALL_EXPERIMENT_FORCE_LEGACY_TEXMERGE        10
#define MERGED_WALL_EXPERIMENT_CLEAR_SECONDARY_UNITS_SINGLE 11

struct android_draw_face_context {
	int valid;
	int seg;
	int side;
	int face;
	int child;
	int side_type;
	int nv;
	int wid_flags;
	int tmap1;
	int tmap2;
};

#define MERGED_WALL_SNAPSHOT_FACE_MAX  6
#define MERGED_WALL_SNAPSHOT_COVER_MAX 12

struct merged_wall_snapshot_face {
	int valid;
	int rank;
	int center_hit;
	float dist2;
	int render_pass;
	int draw_seq;
	int draw_order;
	int seg;
	int side;
	int face;
	int child;
	int side_type;
	int wid_flags;
	int tmap1;
	int tmap2;
	float min_sx;
	float max_sx;
	float min_sy;
	float max_sy;
	float bbox_area;
	float fan_area_012;
	float fan_area_023;
	float alt_area_013;
	float alt_area_123;
	int fan_flip;
	int alt_flip;
	int fan_flat;
	int alt_flat;
	int cull_sensitive;
	int submit_nv;
	char preferred_split[8];
	char route[24];
	char merge_impl[24];
	char decision_reason[40];
};

struct merged_wall_snapshot_cover {
	int valid;
	int kind;
	int rank;
	int center_face;
	int center_cover;
	int center_overlap;
	float overlap_area;
	int ordered;
	int render_pass;
	int draw_seq;
	int draw_order;
	int cover_order;
	int seg;
	int side;
	int face;
	int child;
	int wid_flags;
	int cover_seg;
	int cover_side;
	int cover_face;
	int cover_child;
	int cover_wid_flags;
	char cover_shader[16];
	char cover_bot[24];
	char cover_ovl[24];
};

struct merged_wall_snapshot_target_cover {
	int valid;
	int ordered;
	int render_pass;
	int draw_seq;
	int draw_order;
	int cover_order;
	int seg;
	int side;
	int face;
	int child;
	int wid_flags;
	int cover_seg;
	int cover_side;
	int cover_face;
	int cover_child;
	int cover_wid_flags;
	int tmap1;
	int tex_w;
	int tex_h;
	unsigned int src_hash;
	unsigned int gpu_hash;
	int src_idx254;
	int src_idx255;
	int gpu_avg_r;
	int gpu_avg_g;
	int gpu_avg_b;
	int gpu_avg_a;
	int gpu_black;
	int p0_r;
	int p0_g;
	int p0_b;
	int p0_a;
	int center_r;
	int center_g;
	int center_b;
	int center_a;
	float overlap_area;
	char kind_name[8];
	char face_box[16];
	char cover_shader[16];
	char cover_bot[24];
};

struct merged_wall_snapshot_result {
	int valid;
	char status[24];
	int frame_id;
	int request_frame;
	int screen_w;
	int screen_h;
	float center_x;
	float center_y;
	int sample_r;
	int sample_g;
	int sample_b;
	int sample_a;
	int avg_r;
	int avg_g;
	int avg_b;
	int avg_a;
	int tracked_count;
	int center_hit_count;
	int cover_event_count;
	int selected_count;
	int relevant_cover_count;
	int omitted_cover_count;
	struct merged_wall_snapshot_target_cover target_cover_gpu;
	struct merged_wall_snapshot_face faces[MERGED_WALL_SNAPSHOT_FACE_MAX];
	struct merged_wall_snapshot_cover covers[MERGED_WALL_SNAPSHOT_COVER_MAX];
};

struct merged_wall_last_draw_state {
	int valid;
	int frame_id;
	int render_pass;
	int draw_seq;
	int seg;
	int side;
	int face;
	int child;
	int wid_flags;
	int tmap1;
	int tmap2;
	int depth_enabled;
	int blend_enabled;
	int cull_enabled;
	int polygon_offset_enabled;
	float polygon_offset_factor;
	float polygon_offset_units;
	int depth_writemask;
	int depth_func;
	int front_face;
	int cull_mode;
	int draw_fbo;
	float screen_area;
	int force_cull_off;
	int force_polygon_offset;
	int force_depth_off;
	char route[24];
	char merge_impl[24];
};

#define DEBUG_TEX_LABEL_SET_FACE(lbl, ctxptr)                              \
	do {                                                                   \
		(lbl)->seg = ((ctxptr) && (ctxptr)->valid) ? (ctxptr)->seg : -1;   \
		(lbl)->side = ((ctxptr) && (ctxptr)->valid) ? (ctxptr)->side : -1; \
		(lbl)->face = ((ctxptr) && (ctxptr)->valid) ? (ctxptr)->face : -1; \
	} while (0)

extern volatile int g_merged_wall_debug_mode;
extern volatile int g_merged_wall_experiment_mode;
extern volatile int g_merged_wall_experiment_pending_apply;
extern volatile int g_merged_wall_snapshot_pending;
extern volatile int g_merged_wall_snapshot_request_frame;
extern volatile int g_merged_wall_render_pass;
extern volatile int g_merged_wall_frame_id;
extern volatile int g_merged_wall_draw_seq;
extern struct android_draw_face_context g_android_draw_face_ctx;
extern struct merged_wall_snapshot_result g_merged_wall_snapshot_result;
extern struct merged_wall_last_draw_state g_merged_wall_last_draw_state;

void android_merged_wall_request_snapshot(void);

#endif /* ANDROID */
#endif /* DEBUG_TEX_OVERLAY_H */
