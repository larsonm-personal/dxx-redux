#ifdef ANDROID

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "3d.h"
#include "effects.h"
#include "fvi.h"
#ifdef DXX_BUILD_DESCENT_II
#include "gamepal.h"
#endif
#include "gameseg.h"
#include "gameseq.h"
#include "gr.h"
#include "inferno.h"
#include "object.h"
#include "ogl_init.h"
#include "palette.h"
#include "piggy.h"
#include "rle.h"
#include "segment.h"
#include "textures.h"
#include "wall.h"

#include "android_log.h"
#include "android_texture_debug.h"
#include "merged_wall_debug.h"
#include "merged_wall_geometry_hit.h"
#include "timer.h"
#include "gles3_shim.h"

extern GLuint ogl_prog_tex2;
extern unsigned char *ogl_pal;
extern vms_vector Viewer_eye;
void ogl_prog_set_tex2_current_matrix(const GLfloat *matrix, int super);
void ogl_prog_set_tex2_debug_mode(int mode);
void ogl_prog_set_tex2_alpha_cutoff(GLfloat alpha_cutoff);
void ogl_freetexture(struct _ogl_texture *texture);

#define MERGED_WALL_LOG_PT_COUNT            16
#define MERGED_WALL_TRACKED_FACE_MAX        32
#define MERGED_WALL_COVER_EVENT_MAX         64
#define MERGED_WALL_SNAPSHOT_COVER_EXACT    1
#define MERGED_WALL_SNAPSHOT_COVER_BBOX     2
#define MERGED_WALL_SNAPSHOT_OVERDRAW_MAX   6
#define MERGED_WALL_BITMAP_DUMP_MAX         16
#define MERGED_WALL_BITMAP_DUMP_WIDTH       64
#define MERGED_WALL_PROBE_HIT_NONE          0
#define MERGED_WALL_CACHED_TEXMERGE_COUNT   32
#define MERGED_WALL_TEXTURE_READBACK_MAX    16
#define MERGED_WALL_PROBE_DRAW_FACE_MAX     64
#define MERGED_WALL_PROBE_DRAW_FACE_LOG_MAX 8
#define MERGED_WALL_FNV1A_OFFSET            2166136261u

static struct merged_wall_cached_texmerge_entry g_merged_wall_cached_texmerge[MERGED_WALL_CACHED_TEXMERGE_COUNT];
static int g_merged_wall_cached_texmerge_initialized = 0;
int r_mwall_cache_hits = 0;
int r_mwall_cache_misses = 0;
#define MERGED_WALL_PROBE_HIT_BBOX      1
#define MERGED_WALL_PROBE_HIT_POLYGON   2
#define MERGED_WALL_PROBE_HIT_PROJECTED 3
#define MERGED_WALL_RENDER_SAMPLE_COLS  MERGED_WALL_PROBE_RENDER_SAMPLE_COLS
#define MERGED_WALL_RENDER_SAMPLE_ROWS  MERGED_WALL_PROBE_RENDER_SAMPLE_ROWS
#define MERGED_WALL_RENDER_SAMPLE_COUNT MERGED_WALL_PROBE_RENDER_SAMPLE_COUNT
#define MERGED_WALL_DIAG_VSLICE_SAMPLES 5
#define MERGED_WALL_DIAG_LOG_PT_COUNT   4

struct merged_wall_tracked_face {
	int render_pass;
	int draw_seq;
	int draw_order;
	int nv;
	int projected_count;
	struct android_draw_face_context draw_ctx;
	int cover_logged;
	int coverbox_logged;
	int bbox_valid;
	int projected_bbox_valid;
	float min_sx;
	float max_sx;
	float min_sy;
	float max_sy;
	float bbox_area;
	float projected_min_sx;
	float projected_max_sx;
	float projected_min_sy;
	float projected_max_sy;
	float projected_bbox_area;
	float fan_area_012;
	float fan_area_023;
	float alt_area_013;
	float alt_area_123;
	int fan_flip;
	int alt_flip;
	int fan_flat;
	int alt_flat;
	int cull_sensitive;
	char preferred_split[8];
	fix pts[MERGED_WALL_LOG_PT_COUNT][3];
	int pt_projected[MERGED_WALL_LOG_PT_COUNT];
	float pt_sx[MERGED_WALL_LOG_PT_COUNT];
	float pt_sy[MERGED_WALL_LOG_PT_COUNT];
	char route[24];
	char merge_impl[24];
	char decision_reason[40];
	int orient;
	g3s_uvl uvl[MERGED_WALL_LOG_PT_COUNT];
	grs_bitmap *merged_bitmap;
	int merged_slot;
};

struct merged_wall_snapshot_cover_event {
	int kind;
	int face_index;
	int cover_order;
	int ordered;
	struct android_draw_face_context cover_ctx;
	int cover_nv;
	int cover_projected_count;
	int cover_bbox_valid;
	float cover_min_sx;
	float cover_max_sx;
	float cover_min_sy;
	float cover_max_sy;
	int cover_pt_projected[MERGED_WALL_LOG_PT_COUNT];
	float cover_pt_sx[MERGED_WALL_LOG_PT_COUNT];
	float cover_pt_sy[MERGED_WALL_LOG_PT_COUNT];
	float overlap_area;
	char cover_shader[16];
	char cover_bot[24];
	char cover_ovl[24];
};

struct merged_wall_probe_draw_face {
	int valid;
	int render_pass;
	int draw_seq;
	int draw_order;
	int nv;
	int projected_count;
	struct android_draw_face_context draw_ctx;
	grs_bitmap *bitmap;
	int bbox_valid;
	int projected_bbox_valid;
	float min_sx;
	float max_sx;
	float min_sy;
	float max_sy;
	float bbox_area;
	float projected_min_sx;
	float projected_max_sx;
	float projected_min_sy;
	float projected_max_sy;
	float projected_bbox_area;
	int polygon_hit;
	int bbox_hit;
	int projected_hit;
	int hit_score;
	float dist2;
	fix depth_min;
	fix depth_max;
	fix depth_avg;
	fix pts[MERGED_WALL_LOG_PT_COUNT][3];
	int pt_projected[MERGED_WALL_LOG_PT_COUNT];
	float pt_sx[MERGED_WALL_LOG_PT_COUNT];
	float pt_sy[MERGED_WALL_LOG_PT_COUNT];
	g3s_uvl uvl[MERGED_WALL_LOG_PT_COUNT];
};

struct merged_wall_snapshot_focus_cover {
	int valid;
	int event_index;
	int face_rank;
	int partial_rank;
	int center_face;
	int center_cover;
	int center_overlap;
	int canvas_face;
	int canvas_cover;
	int canvas_overlap;
	int face_poly_valid;
	int face_poly_hit;
	int canvas_face_poly_hit;
	int cover_poly_valid;
	int cover_poly_hit;
	int canvas_cover_poly_hit;
	int focus_bucket;
	float focus_dist2;
	float face_dist2;
	float cover_dist2;
	float canvas_focus_dist2;
	float canvas_face_dist2;
	float canvas_cover_dist2;
	float overlap_area;
	int draw_order;
	int cover_order;
	const char *track_box_kind;
};

struct merged_wall_portal_debug_info {
	int wall_num;
	int wall_type;
	int wall_state;
	int wall_clip;
	int wall_keys;
	unsigned int wall_flags;
	int conn_seg;
	int conn_side;
	int conn_child;
	int conn_side_type;
	int conn_wid;
	int conn_tmap1;
	int conn_tmap2;
	int conn_wall_num;
	int conn_wall_type;
	int conn_wall_state;
	int conn_wall_clip;
	int conn_wall_keys;
	unsigned int conn_wall_flags;
};

volatile int g_merged_wall_debug_mode = 0;
volatile int g_merged_wall_snapshot_pending = 0;
volatile int g_merged_wall_snapshot_request_frame = -1;
volatile int g_merged_wall_snapshot_request_mode = 0;
volatile int g_merged_wall_render_pass = 0;
volatile int g_merged_wall_frame_id = 0;
volatile int g_merged_wall_draw_seq = 0;
volatile int g_merged_wall_force_two_pass = 0;
struct android_draw_face_context g_android_draw_face_ctx = { 0 };
struct merged_wall_snapshot_result g_merged_wall_snapshot_result = { 0 };
struct merged_wall_probe_result g_merged_wall_probe_result = { 0 };
struct merged_wall_last_draw_state g_merged_wall_last_draw_state = { 0 };
static struct merged_wall_snapshot_target_cover merged_wall_snapshot_target_cover = { 0 };

static struct merged_wall_tracked_face merged_wall_tracked_faces[MERGED_WALL_TRACKED_FACE_MAX];
static struct merged_wall_snapshot_cover_event merged_wall_cover_events[MERGED_WALL_COVER_EVENT_MAX];
static struct merged_wall_probe_draw_face merged_wall_probe_draw_faces[MERGED_WALL_PROBE_DRAW_FACE_MAX];
static int merged_wall_tracked_face_count = 0;
static int merged_wall_probe_draw_face_count = 0;
static int merged_wall_probe_draw_face_seen_count = 0;
static int merged_wall_tracked_frame_id = -1;
static int merged_wall_draw_order = 0;
static int merged_wall_dumped_cover_textures[MERGED_WALL_BITMAP_DUMP_MAX];
static int merged_wall_dumped_cover_texture_count = 0;
static int merged_wall_readback_cover_textures[MERGED_WALL_BITMAP_DUMP_MAX];
static int merged_wall_readback_cover_texture_count = 0;
static unsigned int merged_wall_texture_readback_handles[MERGED_WALL_TEXTURE_READBACK_MAX];
static int merged_wall_texture_readback_handle_count = 0;
static int merged_wall_cover_frame_id = -1;
static int merged_wall_cover_event_count = 0;
static unsigned int merged_wall_tmap2_upload_seq = 0;
static unsigned int merged_wall_source_log_mask = 0;
static unsigned int merged_wall_diag_first_gl_handle = 0;

static grs_bitmap *merged_wall_get_source_bitmap(grs_bitmap *bitmap);

static const char *merged_wall_debug_mode_name_local(int mode)
{
	switch (mode) {
		case MERGED_WALL_DEBUG_OVERLAY_ALPHA:
			return "alpha";
		case MERGED_WALL_DEBUG_OVERLAY_RGB:
			return "rgb";
		default:
			return "off";
	}
}

static const char *merged_wall_request_mode_name_local(int mode)
{
	switch (mode) {
		case MERGED_WALL_REQUEST_PROBE:
			return "probe";
		case MERGED_WALL_REQUEST_SNAPSHOT:
		default:
			return "snapshot";
	}
}

void android_merged_wall_texmerge_owner_reset(struct merged_wall_texmerge_owner *owner)
{
	owner->first_seg = -1;
	owner->first_side = -1;
	owner->first_face = -1;
	owner->last_seg = -1;
	owner->last_side = -1;
	owner->last_face = -1;
	owner->creation_frame = -1;
	owner->last_use_frame = -1;
}

void android_merged_wall_texmerge_owner_note(struct merged_wall_texmerge_owner *owner)
{
	const int seg = g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.seg : -1;
	const int side = g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.side : -1;
	const int face = g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.face : -1;

	if (owner->creation_frame < 0) {
		owner->first_seg = seg;
		owner->first_side = side;
		owner->first_face = face;
		owner->creation_frame = g_merged_wall_frame_id;
	}
	owner->last_seg = seg;
	owner->last_side = side;
	owner->last_face = face;
	owner->last_use_frame = g_merged_wall_frame_id;
}

void android_merged_wall_log_texmerge_owner(
    const char *event, int slot, int tmap_bottom, int tmap_top,
    grs_bitmap *bottom_bmp, grs_bitmap *top_bmp, int orient,
    const struct merged_wall_texmerge_owner *owner)
{
	const char *bottom_name;
	const char *top_name;
	int force_log;

	if (!android_merged_wall_is_logging_target_bitmap(top_bmp))
		return;
	bottom_name = piggy_game_bitmap_name(bottom_bmp);
	top_name = piggy_game_bitmap_name(top_bmp);
	force_log = event && strcmp(event, "reuse");
	(force_log ? debug_log_force : debug_log)(DLOG_TEXTURE,
	                                          "[mwall_texmerge] event=%s frame=%d pass=%d seq=%d slot=%d seg=%d side=%d face=%d child=%d wid=%d tmap1=%d tmap2=0x%x orient=%d bot=%s ovl=%s first_owner=%d/%d/%d create_frame=%d last_owner=%d/%d/%d last_use_frame=%d",
	                                          event,
	                                          g_merged_wall_frame_id,
	                                          g_merged_wall_render_pass,
	                                          g_merged_wall_draw_seq,
	                                          slot,
	                                          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.seg : -1,
	                                          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.side : -1,
	                                          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.face : -1,
	                                          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.child : -1,
	                                          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.wid_flags : -1,
	                                          tmap_bottom,
	                                          tmap_top,
	                                          orient,
	                                          bottom_name ? bottom_name : "<none>",
	                                          top_name ? top_name : "<none>",
	                                          owner->first_seg,
	                                          owner->first_side,
	                                          owner->first_face,
	                                          owner->creation_frame,
	                                          owner->last_seg,
	                                          owner->last_side,
	                                          owner->last_face,
	                                          owner->last_use_frame);
}

void android_merged_wall_log_cached_texmerge(const char *event,
                                             grs_bitmap *bottom_bmp,
                                             grs_bitmap *overlay_bmp,
                                             int orient,
                                             int width,
                                             int height,
                                             GLuint handle,
                                             int slot,
                                             const struct _ogl_texture *texture)
{
	const char *bottom_name;
	const char *overlay_name;
	GLuint bottom_handle;
	GLuint overlay_handle;
	if (!g_merged_wall_snapshot_pending && !android_merged_wall_is_logging_target_bitmap(overlay_bmp))
		return;
	bottom_name = piggy_game_bitmap_name(bottom_bmp);
	overlay_name = piggy_game_bitmap_name(overlay_bmp);
	bottom_handle = bottom_bmp && bottom_bmp->gltexture
	                    ? bottom_bmp->gltexture->handle
	                    : 0;
	overlay_handle = overlay_bmp && overlay_bmp->gltexture
	                     ? overlay_bmp->gltexture->handle
	                     : 0;
	(event && strcmp(event, "reuse") ? debug_log_force : debug_log)(DLOG_TEXTURE,
	                                                                "[mwall_cache] event=%s frame=%d pass=%d seq=%d seg=%d side=%d face=%d slot=%d orient=%d route=merge_cached merge_impl=gpu_cached_single size=%dx%d handle=%u base_handle=%u overlay_handle=%u internal=0x%x format=0x%x bytes=%d bytesu=%d wrap=%d mip=%d is_png=%d numrend=%lu tex_flags=0x%x bot=%s ovl=%s",
	                                                                event ? event : "unknown",
	                                                                g_merged_wall_frame_id,
	                                                                g_merged_wall_render_pass,
	                                                                g_merged_wall_draw_seq,
	                                                                g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.seg : -1,
	                                                                g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.side : -1,
	                                                                g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.face : -1,
	                                                                slot,
	                                                                orient,
	                                                                width,
	                                                                height,
	                                                                handle,
	                                                                bottom_handle,
	                                                                overlay_handle,
	                                                                texture ? texture->internalformat : 0,
	                                                                texture ? (unsigned int) texture->format : 0,
	                                                                texture ? texture->bytes : 0,
	                                                                texture ? texture->bytesu : 0,
	                                                                texture ? texture->wrapstate : -1,
	                                                                texture ? texture->has_mipmaps : -1,
	                                                                texture ? texture->is_png : -1,
	                                                                texture ? texture->numrend : 0,
	                                                                texture ? texture->flags : 0,
	                                                                bottom_name ? bottom_name : "<none>",
	                                                                overlay_name ? overlay_name : "<none>");
}

int android_merged_wall_cached_texmerge_visible_dim(const struct _ogl_texture *tex,
                                                    int use_width)
{
	int size;
	GLfloat scale;
	if (!tex)
		return 0;
	size = use_width ? tex->w : tex->h;
	scale = use_width ? tex->u : tex->v;
	if (size < 1)
		return 0;
	if (scale > 0.0f && scale < 1.0f)
		return (int) floorf((float) size * scale + 0.5f);
	return size;
}

void android_merged_wall_cached_texmerge_init_bitmap(grs_bitmap *bm,
                                                     struct _ogl_texture *tex,
                                                     int flags,
                                                     unsigned char avg_color,
                                                     int width,
                                                     int height)
{
	memset(bm, 0, sizeof(*bm));
	bm->bm_w = (short) width;
	bm->bm_h = (short) height;
	bm->bm_rowsize = (short) width;
	bm->bm_flags = (sbyte) flags;
	bm->avg_color = avg_color;
	bm->gltexture = tex;
	bm->gltexture_mask = NULL;
}

void android_merged_wall_cached_texmerge_build_uvs(GLfloat *bottom_uv,
                                                   GLfloat *overlay_uv,
                                                   GLfloat bottom_u_max,
                                                   GLfloat bottom_v_max,
                                                   GLfloat overlay_u_max,
                                                   GLfloat overlay_v_max,
                                                   int orient)
{
	static const GLfloat base_u[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
	/* ANDROID: base_v inverted to match GL ES FBO orientation.
	 * The cached-texmerge quad draws at NDC y corners {+1,+1,-1,-1} with
	 * glViewport(0,0,w,h). NDC y=-1 lands at framebuffer pixel y=0, which
	 * reads back as texture v=0. Pairing NDC y=-1 with base_v=1 (not 0)
	 * keeps the cached composite's texture-space V aligned with the CPU
	 * merge_textures_new reference; without this inversion the cached
	 * overlay is Y-flipped, which appears as a U-mirror on orient 1/3
	 * faces and a V-flip on orient 0/2 faces. */
	static const GLfloat base_v[4] = { 1.0f, 1.0f, 0.0f, 0.0f };
	int i;
	for (i = 0; i < 4; ++i) {
		const GLfloat u = base_u[i];
		const GLfloat v = base_v[i];
		bottom_uv[i * 2] = bottom_u_max * u;
		bottom_uv[i * 2 + 1] = bottom_v_max * v;
		switch (orient) {
			case 1:
				overlay_uv[i * 2] = overlay_u_max * (1.0f - v);
				overlay_uv[i * 2 + 1] = overlay_v_max * u;
				break;
			case 2:
				overlay_uv[i * 2] = overlay_u_max * (1.0f - u);
				overlay_uv[i * 2 + 1] = overlay_v_max * (1.0f - v);
				break;
			case 3:
				overlay_uv[i * 2] = overlay_u_max * v;
				overlay_uv[i * 2 + 1] = overlay_v_max * (1.0f - u);
				break;
			default:
				overlay_uv[i * 2] = overlay_u_max * u;
				overlay_uv[i * 2 + 1] = overlay_v_max * v;
				break;
		}
	}
}

void android_merged_wall_cached_texmerge_clear(
    struct merged_wall_cached_texmerge_entry *entries,
    int count,
    void (*free_texture)(struct _ogl_texture *))
{
	int i;
	if (!entries || count <= 0)
		return;
	for (i = 0; i < count; ++i) {
		if (entries[i].texture && free_texture)
			free_texture(entries[i].texture);
		android_merged_wall_cached_texmerge_reset_entry(&entries[i]);
	}
}

void android_merged_wall_cached_texmerge_clear_cache(void)
{
	merged_wall_texture_readback_handle_count = 0;
	if (Game_mode & GM_MULTI)
		debug_log_force(DLOG_TEXTURE,
		                "[mwall_cache] event=clear frame=%d pass=%d seq=%d mode=0x%x entries=%d",
		                g_merged_wall_frame_id,
		                g_merged_wall_render_pass,
		                g_merged_wall_draw_seq,
		                Game_mode,
		                MERGED_WALL_CACHED_TEXMERGE_COUNT);
	android_merged_wall_cached_texmerge_clear(g_merged_wall_cached_texmerge,
	                                          MERGED_WALL_CACHED_TEXMERGE_COUNT,
	                                          ogl_freetexture);
	g_merged_wall_cached_texmerge_initialized = 1;
}

int android_merged_wall_cached_texmerge_choose_size(
    const struct _ogl_texture *bottom_tex,
    const struct _ogl_texture *overlay_tex,
    int max_texture_size,
    int *width,
    int *height)
{
	int result_width;
	int result_height;
	int candidate;
	if (!width || !height)
		return 0;

	result_width = android_merged_wall_cached_texmerge_visible_dim(bottom_tex, 1);
	result_height = android_merged_wall_cached_texmerge_visible_dim(bottom_tex, 0);
	candidate = android_merged_wall_cached_texmerge_visible_dim(overlay_tex, 1);
	if (candidate > result_width)
		result_width = candidate;
	candidate = android_merged_wall_cached_texmerge_visible_dim(overlay_tex, 0);
	if (candidate > result_height)
		result_height = candidate;
	if (result_width < 1)
		result_width = bottom_tex ? bottom_tex->w : 0;
	if (result_height < 1)
		result_height = bottom_tex ? bottom_tex->h : 0;
	if (result_width < 1 || result_height < 1)
		return 0;
	if (max_texture_size > 0 && (result_width > max_texture_size || result_height > max_texture_size))
		return 0;
	*width = result_width;
	*height = result_height;
	return 1;
}

static const char *merged_wall_probe_hit_kind_name_local(int kind)
{
	switch (kind) {
		case MERGED_WALL_PROBE_HIT_POLYGON:
			return "polygon";
		case MERGED_WALL_PROBE_HIT_BBOX:
			return "bbox";
		case MERGED_WALL_PROBE_HIT_PROJECTED:
			return "projected";
		default:
			return "none";
	}
}

static void merged_wall_copy_string(char *dst, unsigned int dst_size, const char *src)
{
	if (!dst || dst_size == 0)
		return;
	if (!src)
		src = "";
	strncpy(dst, src, dst_size - 1);
	dst[dst_size - 1] = '\0';
}

void android_merged_wall_cached_texmerge_reset_entry(
    struct merged_wall_cached_texmerge_entry *entry)
{
	if (!entry)
		return;
	memset(&entry->bitmap, 0, sizeof(entry->bitmap));
	entry->texture = NULL;
	entry->bottom_bmp = NULL;
	entry->top_bmp = NULL;
	entry->slot = -1;
	entry->orient = -1;
	entry->width = 0;
	entry->height = 0;
	entry->last_time_used = -1;
}

static void android_merged_wall_cached_texmerge_wrap_texture(
    struct _ogl_texture *texture, int state)
{
	if (!texture)
		return;
	if (texture->wrapstate != state || texture->numrend < 1) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, state);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, state);
		texture->wrapstate = state;
	}
}

static int android_merged_wall_cached_texmerge_choose_slot(
    const struct merged_wall_cached_texmerge_entry *entries, int count)
{
	int i;
	int slot;
	fix64 lowest_time;
	if (!entries || count <= 0)
		return -1;
	slot = 0;
	lowest_time = entries[0].last_time_used;
	for (i = 0; i < count; ++i) {
		if (!entries[i].texture || entries[i].last_time_used < 0)
			return i;
		if (entries[i].last_time_used < lowest_time) {
			lowest_time = entries[i].last_time_used;
			slot = i;
		}
	}
	return slot;
}

grs_bitmap *android_merged_wall_cached_texmerge_try_reuse(
    struct merged_wall_cached_texmerge_entry *entries, int count,
    grs_bitmap *bottom_bmp, grs_bitmap *overlay_bmp, int orient,
    int *out_slot)
{
	int i;
	if (out_slot)
		*out_slot = -1;
	if (!entries || count <= 0)
		return NULL;
	for (i = 0; i < count; ++i) {
		struct merged_wall_cached_texmerge_entry *entry = &entries[i];
		if (entry->texture && entry->texture->handle > 0 && entry->bottom_bmp == bottom_bmp && entry->top_bmp == overlay_bmp && entry->orient == orient) {
			r_mwall_cache_hits++;
			entry->last_time_used = timer_query();
			if (out_slot)
				*out_slot = entry->slot;
			android_merged_wall_log_cached_texmerge("reuse", bottom_bmp,
			                                        overlay_bmp, orient, entry->width, entry->height,
			                                        entry->texture->handle, entry->slot, entry->texture);
			return &entry->bitmap;
		}
	}
	r_mwall_cache_misses++;
	return NULL;
}

grs_bitmap *android_merged_wall_cached_texmerge_try_reuse_cache(
    grs_bitmap *bottom_bmp, grs_bitmap *overlay_bmp, int orient,
    int *out_slot)
{
	if (!g_merged_wall_cached_texmerge_initialized)
		android_merged_wall_cached_texmerge_clear_cache();
	return android_merged_wall_cached_texmerge_try_reuse(
	    g_merged_wall_cached_texmerge,
	    MERGED_WALL_CACHED_TEXMERGE_COUNT,
	    bottom_bmp, overlay_bmp, orient, out_slot);
}

static struct merged_wall_cached_texmerge_entry *
merged_wall_find_cached_texmerge_bitmap(grs_bitmap *bm)
{
	int i;

	if (!bm || !g_merged_wall_cached_texmerge_initialized)
		return NULL;
	for (i = 0; i < MERGED_WALL_CACHED_TEXMERGE_COUNT; i++) {
		struct merged_wall_cached_texmerge_entry *entry =
		    &g_merged_wall_cached_texmerge[i];

		if (entry->texture && (&entry->bitmap == bm || entry->bitmap.gltexture == bm->gltexture))
			return entry;
	}
	return NULL;
}

void android_merged_wall_cached_texmerge_commit_entry(
    struct merged_wall_cached_texmerge_entry *entry,
    grs_bitmap *bottom_bmp, grs_bitmap *overlay_bmp, int orient,
    int width, int height, int bitmap_flags, unsigned char avg_color,
    int *out_slot)
{
	if (!entry || !entry->texture)
		return;

	entry->bottom_bmp = bottom_bmp;
	entry->top_bmp = overlay_bmp;
	entry->orient = orient;
	entry->width = width;
	entry->height = height;
	entry->last_time_used = timer_query();
	android_merged_wall_cached_texmerge_init_bitmap(&entry->bitmap,
	                                                entry->texture, bitmap_flags, avg_color, width, height);
	if (out_slot)
		*out_slot = entry->slot;
	android_merged_wall_log_cached_texmerge("create", bottom_bmp,
	                                        overlay_bmp, orient, width, height, entry->texture->handle,
	                                        entry->slot, entry->texture);
}

void android_merged_wall_cached_texmerge_set_render_filters(
    struct _ogl_texture *tex, int texfilt_level)
{
	if (!tex)
		return;
	if (texfilt_level > 0) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
}

void android_merged_wall_cached_texmerge_finalize_filters(
    struct _ogl_texture *tex, int texfilt_level, int aniso_level,
    float max_anisotropy)
{
	if (!tex)
		return;
	if (texfilt_level > 0) {
		glGenerateMipmap(GL_TEXTURE_2D);
		tex->has_mipmaps = 1;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		                texfilt_level >= 2 ? GL_LINEAR_MIPMAP_LINEAR
		                                   : GL_LINEAR_MIPMAP_NEAREST);
		if (aniso_level > 1 && max_anisotropy > 1.0f) {
			int applied = aniso_level;

			if (applied > (int) max_anisotropy)
				applied = (int) max_anisotropy;
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT,
			                (GLfloat) applied);
		}
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
}

void android_merged_wall_cached_texmerge_setup_output_texture(
    struct _ogl_texture *tex, int width, int height, int tex_flags,
    int texfilt_level,
    const struct android_ogl_texture_runtime_state *runtime_state)
{
	if (!tex)
		return;

	tex->w = tex->tw = tex->lw = width;
	tex->h = tex->th = height;
	tex->u = 1.0f;
	tex->v = 1.0f;
	tex->is_png = 1;
	tex->has_mipmaps = 0;
	tex->flags = tex_flags;

	glGenTextures(1, &tex->handle);
	android_ogl_bind_texture_2d(runtime_state ? &runtime_state->bind_state : NULL,
	                            tex->handle);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	android_merged_wall_cached_texmerge_set_render_filters(tex, texfilt_level);
	glTexImage2D(GL_TEXTURE_2D, 0, tex->internalformat, width, height, 0,
	             tex->format, GL_UNSIGNED_BYTE, NULL);
}

int android_merged_wall_cached_texmerge_render_to_texture(
    struct _ogl_texture *output_tex, grs_bitmap *bottom_bmp,
    grs_bitmap *overlay_bmp, int orient, int width, int height,
    int texfilt_level, int aniso_level, float max_anisotropy,
    const struct android_ogl_texture_runtime_state *runtime_state)
{
	static const GLfloat identity[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	static const GLfloat vertex_array[12] = {
		-1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		-1.0f, -1.0f, 0.0f
	};
	static const GLfloat color_array[16] = {
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f
	};
	GLfloat bottom_uv[8], overlay_uv[8];
	GLfloat bottom_u_max, bottom_v_max, overlay_u_max, overlay_v_max;
	GLint old_fbo = 0, old_viewport[4] = { 0 }, old_active_tex = GL_TEXTURE0;
	GLboolean old_depth_mask = GL_TRUE;
	GLboolean old_color_mask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	GLboolean had_blend, had_depth, had_cull;
	GLuint fbo = 0;

	if (!output_tex || !bottom_bmp || !overlay_bmp || !bottom_bmp->gltexture || !overlay_bmp->gltexture)
		return 0;

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);
	glGetIntegerv(GL_VIEWPORT, old_viewport);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active_tex);
	had_blend = glIsEnabled(GL_BLEND);
	had_depth = glIsEnabled(GL_DEPTH_TEST);
	had_cull = glIsEnabled(GL_CULL_FACE);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &old_depth_mask);
	glGetBooleanv(GL_COLOR_WRITEMASK, old_color_mask);

	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, output_tex->handle, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		glBindFramebuffer(GL_FRAMEBUFFER, old_fbo);
		glDeleteFramebuffers(1, &fbo);
		return 0;
	}

	glViewport(0, 0, width, height);
	if (had_blend)
		glDisable(GL_BLEND);
	if (had_depth)
		glDisable(GL_DEPTH_TEST);
	if (had_cull)
		glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	bottom_u_max = bottom_bmp->gltexture->u > 0.0f ? bottom_bmp->gltexture->u : 1.0f;
	bottom_v_max = bottom_bmp->gltexture->v > 0.0f ? bottom_bmp->gltexture->v : 1.0f;
	overlay_u_max = overlay_bmp->gltexture->u > 0.0f ? overlay_bmp->gltexture->u : 1.0f;
	overlay_v_max = overlay_bmp->gltexture->v > 0.0f ? overlay_bmp->gltexture->v : 1.0f;
	android_merged_wall_cached_texmerge_build_uvs(bottom_uv, overlay_uv,
	                                              bottom_u_max, bottom_v_max, overlay_u_max, overlay_v_max, orient);

	android_ogl_enable_texture_2d(runtime_state ? runtime_state->texture_2d_enabled : NULL);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	android_merged_wall_cached_texmerge_wrap_texture(bottom_bmp->gltexture,
	                                                 GL_CLAMP_TO_EDGE);
	android_merged_wall_cached_texmerge_wrap_texture(overlay_bmp->gltexture,
	                                                 GL_CLAMP_TO_EDGE);
	glActiveTexture(GL_TEXTURE0);
	android_ogl_bind_texture_2d(runtime_state ? &runtime_state->bind_state : NULL,
	                            bottom_bmp->gltexture->handle);
	glActiveTexture(GL_TEXTURE1);
	android_ogl_bind_texture_2d(runtime_state ? &runtime_state->bind_state : NULL,
	                            overlay_bmp->gltexture->handle);
	glActiveTexture(GL_TEXTURE0);
	gles3_shim_use_external(ogl_prog_tex2);
	ogl_prog_set_tex2_current_matrix(identity, 0);
	ogl_prog_set_tex2_debug_mode(0);
	ogl_prog_set_tex2_alpha_cutoff((overlay_bmp->bm_flags & BM_FLAG_TRANSPARENT)
	                                   ? 0.5f
	                                   : 0.0f);
	glVertexPointer(3, GL_FLOAT, 0, vertex_array);
	glColorPointer(4, GL_FLOAT, 0, color_array);
	glTexCoordPointer(2, GL_FLOAT, 0, bottom_uv);
	gles3_shim_external_texcoord2_pointer(2, GL_FLOAT, 0, overlay_uv);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	gles3_shim_external_texcoord2_pointer(0, GL_FLOAT, 0, NULL);
	gles3_shim_use_external(0);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	android_merged_wall_cached_texmerge_wrap_texture(bottom_bmp->gltexture,
	                                                 GL_REPEAT);
	android_merged_wall_cached_texmerge_wrap_texture(overlay_bmp->gltexture,
	                                                 GL_REPEAT);

	glBindFramebuffer(GL_FRAMEBUFFER, old_fbo);
	glDeleteFramebuffers(1, &fbo);
	android_ogl_bind_texture_2d(runtime_state ? &runtime_state->bind_state : NULL,
	                            output_tex->handle);
	android_merged_wall_cached_texmerge_finalize_filters(output_tex,
	                                                     texfilt_level, aniso_level, max_anisotropy);
	glColorMask(old_color_mask[0], old_color_mask[1], old_color_mask[2],
	            old_color_mask[3]);
	glDepthMask(old_depth_mask);
	if (had_blend)
		glEnable(GL_BLEND);
	if (had_depth)
		glEnable(GL_DEPTH_TEST);
	if (had_cull)
		glEnable(GL_CULL_FACE);
	glViewport(old_viewport[0], old_viewport[1], old_viewport[2],
	           old_viewport[3]);
	glActiveTexture((GLenum) old_active_tex);
	return 1;
}

int android_merged_wall_cached_texmerge_finalize_entry(
    struct merged_wall_cached_texmerge_entry *entry,
    grs_bitmap *bottom_bmp, grs_bitmap *overlay_bmp, int orient,
    int width, int height, int texfilt_level, int aniso_level,
    float max_anisotropy, int bitmap_flags, unsigned char avg_color,
    const struct android_ogl_texture_runtime_state *runtime_state,
    int *out_slot, void (*free_texture)(struct _ogl_texture *))
{
	if (!entry || !entry->texture)
		return 0;

	if (!android_merged_wall_cached_texmerge_render_to_texture(entry->texture,
	                                                           bottom_bmp, overlay_bmp, orient, width, height, texfilt_level,
	                                                           aniso_level, max_anisotropy, runtime_state)) {
		if (free_texture)
			free_texture(entry->texture);
		android_merged_wall_cached_texmerge_reset_entry(entry);
		return 0;
	}

	android_merged_wall_cached_texmerge_commit_entry(entry, bottom_bmp,
	                                                 overlay_bmp, orient, width, height, bitmap_flags, avg_color,
	                                                 out_slot);
	return 1;
}

struct merged_wall_cached_texmerge_entry *
android_merged_wall_cached_texmerge_reserve_entry(
    struct merged_wall_cached_texmerge_entry *entries, int count,
    void (*free_texture)(struct _ogl_texture *))
{
	int slot;
	struct merged_wall_cached_texmerge_entry *entry;

	if (!entries || count <= 0)
		return NULL;

	slot = android_merged_wall_cached_texmerge_choose_slot(entries, count);
	if (slot < 0)
		return NULL;

	entry = &entries[slot];
	if (entry->texture) {
		if (!free_texture)
			return NULL;
		android_merged_wall_log_cached_texmerge("evict", entry->bottom_bmp,
		                                        entry->top_bmp, entry->orient, entry->width, entry->height,
		                                        entry->texture->handle, entry->slot, entry->texture);
		free_texture(entry->texture);
	}
	android_merged_wall_cached_texmerge_reset_entry(entry);
	entry->slot = slot;
	return entry;
}

struct merged_wall_cached_texmerge_entry *
android_merged_wall_cached_texmerge_reserve_cache_entry(
    void (*free_texture)(struct _ogl_texture *))
{
	if (!g_merged_wall_cached_texmerge_initialized)
		android_merged_wall_cached_texmerge_clear_cache();
	return android_merged_wall_cached_texmerge_reserve_entry(
	    g_merged_wall_cached_texmerge,
	    MERGED_WALL_CACHED_TEXMERGE_COUNT,
	    free_texture);
}

static void merged_wall_reset_probe_result(void)
{
	memset(&g_merged_wall_probe_result, 0, sizeof(g_merged_wall_probe_result));
	g_merged_wall_probe_result.seg = -1;
	g_merged_wall_probe_result.side = -1;
	g_merged_wall_probe_result.face = -1;
	g_merged_wall_probe_result.child = -1;
	g_merged_wall_probe_result.tmap1 = -1;
	g_merged_wall_probe_result.tmap2 = 0;
	g_merged_wall_probe_result.orient = 0;
	merged_wall_copy_string(g_merged_wall_probe_result.status,
	                        sizeof(g_merged_wall_probe_result.status), "idle");
	merged_wall_copy_string(g_merged_wall_probe_result.route,
	                        sizeof(g_merged_wall_probe_result.route), "");
	merged_wall_copy_string(g_merged_wall_probe_result.merge_impl,
	                        sizeof(g_merged_wall_probe_result.merge_impl), "");
	merged_wall_copy_string(g_merged_wall_probe_result.ovl_flip_axis,
	                        sizeof(g_merged_wall_probe_result.ovl_flip_axis), "none");
	merged_wall_copy_string(g_merged_wall_probe_result.flip_screen_axis,
	                        sizeof(g_merged_wall_probe_result.flip_screen_axis), "none");
	g_merged_wall_probe_result.u_span = 0.0f;
	g_merged_wall_probe_result.v_span = 0.0f;
	g_merged_wall_probe_result.u_shift_hint = 0.0f;
	g_merged_wall_probe_result.v_shift_hint = 0.0f;
	g_merged_wall_probe_result.cached_anchor_u = 0.0f;
	g_merged_wall_probe_result.cached_anchor_v = 0.0f;
	g_merged_wall_probe_result.legacy_anchor_u = 0.0f;
	g_merged_wall_probe_result.legacy_anchor_v = 0.0f;
	g_merged_wall_probe_result.route_agree = 0;
	g_merged_wall_probe_result.render_sample_valid = 0;
	g_merged_wall_probe_result.render_valid_cells = 0;
	g_merged_wall_probe_result.render_hot_cells = 0;
	g_merged_wall_probe_result.render_hash = 0;
	g_merged_wall_probe_result.render_luma_min = 0;
	g_merged_wall_probe_result.render_luma_max = 0;
	g_merged_wall_probe_result.render_hot_x = -1.0f;
	g_merged_wall_probe_result.render_hot_y = -1.0f;
	memset(g_merged_wall_probe_result.render_sample_luma, 0,
	       sizeof(g_merged_wall_probe_result.render_sample_luma));
	memset(g_merged_wall_probe_result.render_sample_mask, 0,
	       sizeof(g_merged_wall_probe_result.render_sample_mask));
}

static int merged_wall_overlay_index(int tmap2)
{
	return tmap2 & 0x3fff;
}

void android_merged_wall_reset_tmap2_submit_context(struct merged_wall_tmap2_submit_context *ctx)
{
	if (!ctx)
		return;
	memset(ctx, 0, sizeof(*ctx));
	ctx->route = "merge_raw";
	ctx->orig_uand = 0xff;
}

void android_merged_wall_set_tmap2_submit_context(struct merged_wall_tmap2_submit_context *ctx,
                                                  const char *route, int orig_nv, const g3s_codes *cc, int input_behind,
                                                  int temp_points, int clip_applied)
{
	if (!ctx)
		return;
	android_merged_wall_reset_tmap2_submit_context(ctx);
	ctx->route = route ? route : "merge_raw";
	ctx->orig_nv = orig_nv;
	if (cc) {
		ctx->orig_uor = cc->uor;
		ctx->orig_uand = cc->uand;
	}
	ctx->input_behind = input_behind;
	ctx->temp_points = temp_points;
	ctx->clip_applied = clip_applied;
}

void android_merged_wall_log_tmap2_route(const char *route, grs_bitmap *bmbot,
                                         grs_bitmap *bmovl, int nv, int orient)
{
	const char *botname = piggy_game_bitmap_name(bmbot);
	const char *ovlname = piggy_game_bitmap_name(bmovl);

	if (!android_merged_wall_is_logging_target_bitmap(bmovl))
		return;
	debug_log(DLOG_TEXTURE,
	          "[mwall_clip] frame=%d pass=%d seq=%d stage=route route=%s merge_impl=gpu_two_pass seg=%d side=%d face=%d child=%d wid=%d tmap1=%d tmap2=0x%x orig_nv=%d orient=%d super=%d bot=%s ovl=%s",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          route ? route : "merge_raw",
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.seg : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.side : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.face : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.child : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.wid_flags : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.tmap1 : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.tmap2 : 0,
	          nv,
	          orient,
	          !!(bmovl->bm_flags & BM_FLAG_SUPER_TRANSPARENT),
	          botname ? botname : "<none>",
	          ovlname ? ovlname : "<none>");
}

void android_merged_wall_log_upload(struct merged_wall_tmap2_submit_context *ctx,
                                    grs_bitmap *bmbot, grs_bitmap *bmovl, unsigned int prog,
                                    unsigned int merge_vbo, int nv, int orient, int vb, int cb, int tb,
                                    int t2b)
{
	const char *route = ctx ? ctx->route : NULL;
	const char *botname = piggy_game_bitmap_name(bmbot);
	const char *ovlname = piggy_game_bitmap_name(bmovl);
	unsigned int base_handle = bmbot->gltexture ? bmbot->gltexture->handle : 0;
	unsigned int overlay_handle = bmovl->gltexture ? bmovl->gltexture->handle : 0;
	int color_off = vb;
	int tex_off = vb + cb;
	int tex2_off = vb + cb + tb;
	int total = vb + cb + tb + t2b;
	unsigned int upload_id = ++merged_wall_tmap2_upload_seq;

	if (ctx)
		ctx->upload_id = upload_id;
	debug_log(DLOG_TEXTURE,
	          "[mwall_upload] frame=%d pass=%d seq=%d upload_id=%u route=%s merge_impl=gpu_two_pass upload_impl=shim_stream seg=%d side=%d face=%d child=%d wid=%d tmap1=%d tmap2=0x%x orient=%d bot=%s ovl=%s base_handle=%u overlay_handle=%u prog=%u vbo=%u nv=%d vb=%d cb=%d tb=%d t2b=%d total=%d off_v=0 off_c=%d off_t=%d off_t2=%d",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          upload_id,
	          route ? route : "merge_raw",
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.seg : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.side : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.face : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.child : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.wid_flags : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.tmap1 : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.tmap2 : 0,
	          orient,
	          botname ? botname : "<none>",
	          ovlname ? ovlname : "<none>",
	          base_handle,
	          overlay_handle,
	          prog,
	          merge_vbo,
	          nv,
	          vb,
	          cb,
	          tb,
	          t2b,
	          total,
	          color_off,
	          tex_off,
	          tex2_off);
}

void android_merged_wall_get_source_palette_counts(grs_bitmap *bm, int *idx254,
                                                   int *idx255, int *real_flags)
{
	grs_bitmap *src = merged_wall_get_source_bitmap(bm);
	int total, i;

	if (idx254)
		*idx254 = 0;
	if (idx255)
		*idx255 = 0;
	if (real_flags)
		*real_flags = bm ? piggy_bitmap_get_flags(bm) : 0;

	if (!src)
		return;

	total = src->bm_w * src->bm_h;
	for (i = 0; i < total; i++) {
		if (src->bm_data[i] == 254) {
			if (idx254)
				(*idx254)++;
		} else if (src->bm_data[i] == 255) {
			if (idx255)
				(*idx255)++;
		}
	}
}

int android_merged_wall_get_source_alpha_class(unsigned char idx)
{
	if (idx == 255)
		return 0;
	if (idx == 254)
		return 2;
	return 1;
}

float android_merged_wall_get_source_alpha_value(unsigned char idx)
{
	return idx == 255 ? 0.0f : 1.0f;
}

void android_merged_wall_get_source_sample(grs_bitmap *bm,
                                           const float *texcoordovl_array, int nv, float *avg_u, float *avg_v,
                                           int *sample_x, int *sample_y, int *sample_idx)
{
	grs_bitmap *src = merged_wall_get_source_bitmap(bm);
	float sum_u = 0.0f, sum_v = 0.0f;
	float avg_u_local, avg_v_local, wrapped_u, wrapped_v;
	int sample_x_local, sample_y_local;
	int i;

	if (avg_u)
		*avg_u = 0.0f;
	if (avg_v)
		*avg_v = 0.0f;
	if (sample_x)
		*sample_x = -1;
	if (sample_y)
		*sample_y = -1;
	if (sample_idx)
		*sample_idx = -1;

	if (!src || !texcoordovl_array || nv <= 0)
		return;

	for (i = 0; i < nv; i++) {
		sum_u += texcoordovl_array[i * 2];
		sum_v += texcoordovl_array[i * 2 + 1];
	}
	avg_u_local = sum_u / nv;
	avg_v_local = sum_v / nv;
	if (avg_u)
		*avg_u = avg_u_local;
	if (avg_v)
		*avg_v = avg_v_local;
	wrapped_u = avg_u_local - floorf(avg_u_local);
	wrapped_v = avg_v_local - floorf(avg_v_local);
	if (wrapped_u < 0.0f)
		wrapped_u += 1.0f;
	if (wrapped_v < 0.0f)
		wrapped_v += 1.0f;

	sample_x_local = (int) (wrapped_u * src->bm_w);
	sample_y_local = (int) (wrapped_v * src->bm_h);
	if (sample_x_local >= src->bm_w)
		sample_x_local = src->bm_w - 1;
	if (sample_y_local >= src->bm_h)
		sample_y_local = src->bm_h - 1;
	if (sample_x)
		*sample_x = sample_x_local;
	if (sample_y)
		*sample_y = sample_y_local;
	if (sample_idx) {
		*sample_idx = src->bm_data[sample_y_local * src->bm_w + sample_x_local];
	}
}

void android_merged_wall_get_source_filter_sample(grs_bitmap *bm, float sample_u,
                                                  float sample_v, int *idx00, int *idx10, int *idx01, int *idx11,
                                                  float *alpha, int *wrap_u, int *wrap_v)
{
	grs_bitmap *src = merged_wall_get_source_bitmap(bm);
	float wrapped_u, wrapped_v, tex_u, tex_v, frac_u, frac_v, alpha0, alpha1;
	int x0, x1, y0, y1;

	if (idx00)
		*idx00 = -1;
	if (idx10)
		*idx10 = -1;
	if (idx01)
		*idx01 = -1;
	if (idx11)
		*idx11 = -1;
	if (alpha)
		*alpha = -1.0f;
	if (wrap_u)
		*wrap_u = 0;
	if (wrap_v)
		*wrap_v = 0;

	if (!src || src->bm_w <= 0 || src->bm_h <= 0)
		return;

	wrapped_u = sample_u - floorf(sample_u);
	wrapped_v = sample_v - floorf(sample_v);
	if (wrapped_u < 0.0f)
		wrapped_u += 1.0f;
	if (wrapped_v < 0.0f)
		wrapped_v += 1.0f;

	tex_u = wrapped_u * src->bm_w - 0.5f;
	tex_v = wrapped_v * src->bm_h - 0.5f;
	x0 = (int) floorf(tex_u);
	y0 = (int) floorf(tex_v);
	frac_u = tex_u - floorf(tex_u);
	frac_v = tex_v - floorf(tex_v);
	x1 = x0 + 1;
	y1 = y0 + 1;

	if (wrap_u)
		*wrap_u = (x0 < 0 || x1 >= src->bm_w);
	if (wrap_v)
		*wrap_v = (y0 < 0 || y1 >= src->bm_h);

	x0 = ((x0 % src->bm_w) + src->bm_w) % src->bm_w;
	x1 = ((x1 % src->bm_w) + src->bm_w) % src->bm_w;
	y0 = ((y0 % src->bm_h) + src->bm_h) % src->bm_h;
	y1 = ((y1 % src->bm_h) + src->bm_h) % src->bm_h;

	if (idx00)
		*idx00 = src->bm_data[y0 * src->bm_w + x0];
	if (idx10)
		*idx10 = src->bm_data[y0 * src->bm_w + x1];
	if (idx01)
		*idx01 = src->bm_data[y1 * src->bm_w + x0];
	if (idx11)
		*idx11 = src->bm_data[y1 * src->bm_w + x1];

	alpha0 = android_merged_wall_get_source_alpha_value((unsigned char) src->bm_data[y0 * src->bm_w + x0]) * (1.0f - frac_u) + android_merged_wall_get_source_alpha_value((unsigned char) src->bm_data[y0 * src->bm_w + x1]) * frac_u;
	alpha1 = android_merged_wall_get_source_alpha_value((unsigned char) src->bm_data[y1 * src->bm_w + x0]) * (1.0f - frac_u) + android_merged_wall_get_source_alpha_value((unsigned char) src->bm_data[y1 * src->bm_w + x1]) * frac_u;
	if (alpha)
		*alpha = alpha0 * (1.0f - frac_v) + alpha1 * frac_v;
}

void android_merged_wall_get_source_vslice(grs_bitmap *bm, float sample_u,
                                           float min_v, float max_v, int *sample_rows, int *sample_idxs,
                                           int nsamples)
{
	grs_bitmap *src = merged_wall_get_source_bitmap(bm);
	float wrapped_u, span_v;
	int sample_x, i;

	if (!sample_rows || !sample_idxs || nsamples <= 0)
		return;

	for (i = 0; i < nsamples; i++) {
		sample_rows[i] = -1;
		sample_idxs[i] = -1;
	}
	if (!src)
		return;

	wrapped_u = sample_u - floorf(sample_u);
	if (wrapped_u < 0.0f)
		wrapped_u += 1.0f;
	sample_x = (int) (wrapped_u * src->bm_w);
	if (sample_x >= src->bm_w)
		sample_x = src->bm_w - 1;
	span_v = max_v - min_v;

	for (i = 0; i < nsamples; i++) {
		float sample_v = min_v + span_v * ((i + 0.5f) / nsamples);
		float wrapped_v = sample_v - floorf(sample_v);
		int sample_y;

		if (wrapped_v < 0.0f)
			wrapped_v += 1.0f;
		sample_y = (int) (wrapped_v * src->bm_h);
		if (sample_y >= src->bm_h)
			sample_y = src->bm_h - 1;
		sample_rows[i] = sample_y;
		sample_idxs[i] = src->bm_data[sample_y * src->bm_w + sample_x];
	}
}

void android_merged_wall_get_filter_state(const ogl_texture *tex, int *min_filter,
                                          int *mag_filter)
{
	GLint active_tex = GL_TEXTURE0;
	GLint prev_bind = 0;
	GLint min_filter_local = -1;
	GLint mag_filter_local = -1;

	if (min_filter)
		*min_filter = -1;
	if (mag_filter)
		*mag_filter = -1;
	if (!tex || tex->handle <= 0)
		return;

	glGetIntegerv(GL_ACTIVE_TEXTURE, &active_tex);
	glActiveTexture(GL_TEXTURE1);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_bind);
	if ((GLuint) prev_bind != tex->handle)
		glBindTexture(GL_TEXTURE_2D, tex->handle);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
	                    &min_filter_local);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
	                    &mag_filter_local);
	if ((GLuint) prev_bind != tex->handle)
		glBindTexture(GL_TEXTURE_2D, (GLuint) prev_bind);
	glActiveTexture((GLenum) active_tex);
	if (min_filter)
		*min_filter = min_filter_local;
	if (mag_filter)
		*mag_filter = mag_filter_local;
}

void android_merged_wall_get_draw_state(const ogl_texture *tex, int *active_prog,
                                        int *bound_tex0, int *bound_tex1, int *bound_tex2, int *mip1_w)
{
	GLint active_tex = GL_TEXTURE0;
	GLint prev_bind1 = 0;
	GLint active_prog_local = -1;
	GLint bound_tex0_local = -1;
	GLint bound_tex2_local = -1;
	int mip1_w_local = -1;

	if (active_prog)
		*active_prog = -1;
	if (bound_tex0)
		*bound_tex0 = -1;
	if (bound_tex1)
		*bound_tex1 = -1;
	if (bound_tex2)
		*bound_tex2 = -1;
	if (mip1_w)
		*mip1_w = -1;
	glGetIntegerv(GL_CURRENT_PROGRAM, &active_prog_local);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active_tex);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound_tex0_local);
	glActiveTexture(GL_TEXTURE1);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_bind1);
	glActiveTexture(GL_TEXTURE2);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound_tex2_local);
	if (tex && tex->handle > 0)
		mip1_w_local = tex->has_mipmaps ? (tex->tw > 1 ? tex->tw / 2 : 1) : 0;
	glActiveTexture((GLenum) active_tex);
	if (active_prog)
		*active_prog = active_prog_local;
	if (bound_tex0)
		*bound_tex0 = bound_tex0_local;
	if (bound_tex1)
		*bound_tex1 = prev_bind1;
	if (bound_tex2)
		*bound_tex2 = bound_tex2_local;
	if (mip1_w)
		*mip1_w = mip1_w_local;
}

void android_merged_wall_get_gl_state(int *depth_enabled, int *blend_enabled,
                                      int *cull_enabled, unsigned char *depth_writemask, int *depth_func,
                                      int *front_face, int *cull_mode, int *polygon_offset_enabled,
                                      float *polygon_offset_factor, float *polygon_offset_units,
                                      unsigned char color_mask[4], int *draw_fbo)
{
	GLboolean depth_writemask_local = 0;
	GLboolean color_mask_local[4] = { 0, 0, 0, 0 };
	GLint depth_func_local = 0;
	GLint front_face_local = 0;
	GLint cull_mode_local = 0;
	GLint draw_fbo_local = 0;
	GLfloat polygon_offset_factor_local = 0.0f;
	GLfloat polygon_offset_units_local = 0.0f;

	if (depth_enabled)
		*depth_enabled = glIsEnabled(GL_DEPTH_TEST);
	if (blend_enabled)
		*blend_enabled = glIsEnabled(GL_BLEND);
	if (cull_enabled)
		*cull_enabled = glIsEnabled(GL_CULL_FACE);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_writemask_local);
	glGetIntegerv(GL_DEPTH_FUNC, &depth_func_local);
	glGetIntegerv(GL_FRONT_FACE, &front_face_local);
	glGetIntegerv(GL_CULL_FACE_MODE, &cull_mode_local);
	if (polygon_offset_enabled)
		*polygon_offset_enabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);
	glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &polygon_offset_factor_local);
	glGetFloatv(GL_POLYGON_OFFSET_UNITS, &polygon_offset_units_local);
	glGetBooleanv(GL_COLOR_WRITEMASK, color_mask_local);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &draw_fbo_local);
	if (depth_writemask)
		*depth_writemask = (unsigned char) depth_writemask_local;
	if (depth_func)
		*depth_func = depth_func_local;
	if (front_face)
		*front_face = front_face_local;
	if (cull_mode)
		*cull_mode = cull_mode_local;
	if (polygon_offset_factor)
		*polygon_offset_factor = polygon_offset_factor_local;
	if (polygon_offset_units)
		*polygon_offset_units = polygon_offset_units_local;
	if (color_mask) {
		color_mask[0] = (unsigned char) color_mask_local[0];
		color_mask[1] = (unsigned char) color_mask_local[1];
		color_mask[2] = (unsigned char) color_mask_local[2];
		color_mask[3] = (unsigned char) color_mask_local[3];
	}
	if (draw_fbo)
		*draw_fbo = draw_fbo_local;
}

float android_merged_wall_get_triangle_area(const struct g3s_point *p0,
                                            const struct g3s_point *p1, const struct g3s_point *p2)
{
	double x0 = f2fl(p0->p3_sx), y0 = f2fl(p0->p3_sy);
	double x1 = f2fl(p1->p3_sx), y1 = f2fl(p1->p3_sy);
	double x2 = f2fl(p2->p3_sx), y2 = f2fl(p2->p3_sy);

	return (float) (((x0 * (y1 - y2)) + (x1 * (y2 - y0)) +
	                 (x2 * (y0 - y1))) *
	                0.5);
}

void android_merged_wall_get_uv_points(const g3s_uvl *uvl_list,
                                       const float *texcoordovl_array, int nv, float *raw_pts, float *ovl_pts,
                                       int max_points, int *uv_bad)
{
	int i;

	if (uv_bad)
		*uv_bad = 0;
	if (!raw_pts || !ovl_pts || max_points <= 0)
		return;
	for (i = 0; i < max_points * 2; i++) {
		raw_pts[i] = -99.0f;
		ovl_pts[i] = -99.0f;
	}
	if (!uvl_list || !texcoordovl_array || nv <= 0)
		return;
	for (i = 0; i < nv && i < max_points; i++) {
		raw_pts[i * 2] = f2fl(uvl_list[i].u);
		raw_pts[i * 2 + 1] = f2fl(uvl_list[i].v);
		ovl_pts[i * 2] = texcoordovl_array[i * 2];
		ovl_pts[i * 2 + 1] = texcoordovl_array[i * 2 + 1];
		if (uv_bad && (!isfinite(raw_pts[i * 2]) || !isfinite(raw_pts[i * 2 + 1]) || !isfinite(ovl_pts[i * 2]) || !isfinite(ovl_pts[i * 2 + 1])))
			*uv_bad = 1;
	}
}

void android_merged_wall_get_input_codes(const struct g3s_point *const *pointlist,
                                         int nv, g3s_codes *cc, int *input_behind)
{
	int i;

	if (cc) {
		cc->uor = 0;
		cc->uand = 0xff;
	}
	if (input_behind)
		*input_behind = 0;
	for (i = 0; i < nv; i++) {
		const struct g3s_point *p = pointlist[i];

		if (cc) {
			cc->uand &= p->p3_codes;
			cc->uor |= p->p3_codes;
		}
		if (input_behind && (p->p3_codes & CC_BEHIND))
			(*input_behind)++;
	}
}

void android_merged_wall_get_point_code_summary(const struct g3s_point *const *pointlist,
                                                int nv, unsigned int *uor, unsigned int *uand, int *behind_count,
                                                int *temp_points)
{
	int i;

	if (uor)
		*uor = 0;
	if (uand)
		*uand = 0xff;
	if (behind_count)
		*behind_count = 0;
	if (temp_points)
		*temp_points = 0;
	for (i = 0; i < nv; i++) {
		const struct g3s_point *p = pointlist[i];

		if (uor)
			*uor |= p->p3_codes;
		if (uand)
			*uand &= p->p3_codes;
		if (behind_count && (p->p3_codes & CC_BEHIND))
			(*behind_count)++;
		if (temp_points && (p->p3_flags & PF_TEMP_POINT))
			(*temp_points)++;
	}
}

float android_merged_wall_get_screen_area(const struct g3s_point *const *pointlist,
                                          int nv)
{
	double area = 0.0;
	int i;

	if (!pointlist || nv < 3)
		return 0.0f;

	for (i = 0; i < nv; i++) {
		int j = (i + 1) % nv;

		area += (double) f2fl(pointlist[i]->p3_sx) * (double) f2fl(pointlist[j]->p3_sy) - (double) f2fl(pointlist[j]->p3_sx) * (double) f2fl(pointlist[i]->p3_sy);
	}

	return (float) (area * 0.5);
}

void android_merged_wall_log_submit(
    const struct merged_wall_tmap2_submit_context *ctx,
    const struct g3s_point *const *pointlist, int nv)
{
	float sx[6], sy[6];
	unsigned int codes[6], flags[6], post_uor = 0, post_uand = 0xff;
	const char *route = ctx ? ctx->route : NULL;
	char fan_order[96];
	int orig_nv = ctx && ctx->orig_nv > 0 ? ctx->orig_nv : nv;
	int i, j, dup_pairs = 0, zero_count = 0, post_behind = 0;
	int fan_tris = nv >= 3 ? nv - 2 : 0;
	int fan_written = 0;
	int overflow = 0, temp_points = 0, projected = 0;
	int extra = nv > 6 ? nv - 6 : 0;

	if (!pointlist || nv <= 0)
		return;
	for (i = 0; i < 6; i++) {
		sx[i] = -9999.0f;
		sy[i] = -9999.0f;
		codes[i] = 0;
		flags[i] = 0;
	}
	fan_order[0] = '\0';
	for (i = 1; i < nv - 1 && i < 6; i++) {
		int n = snprintf(fan_order + fan_written,
		                 sizeof(fan_order) - fan_written,
		                 "%s0-%d-%d",
		                 fan_written ? "|" : "",
		                 i,
		                 i + 1);

		if (n < 0 || n >= (int) (sizeof(fan_order) - fan_written)) {
			fan_order[sizeof(fan_order) - 1] = '\0';
			break;
		}
		fan_written += n;
	}
	for (i = 0; i < nv; i++) {
		const struct g3s_point *p = pointlist[i];

		post_uor |= p->p3_codes;
		post_uand &= p->p3_codes;
		if (p->p3_codes & CC_BEHIND)
			post_behind++;
		if (p->p3_flags & PF_OVERFLOW)
			overflow++;
		if (p->p3_flags & PF_TEMP_POINT)
			temp_points++;
		if (p->p3_flags & PF_PROJECTED)
			projected++;
		if (p->p3_sx == 0 && p->p3_sy == 0)
			zero_count++;
		if (i < 6) {
			sx[i] = f2fl(p->p3_sx);
			sy[i] = f2fl(p->p3_sy);
			codes[i] = p->p3_codes;
			flags[i] = p->p3_flags;
		}
	}
	for (i = 0; i < nv; i++)
		for (j = i + 1; j < nv; j++)
			if (pointlist[i]->p3_sx == pointlist[j]->p3_sx && pointlist[i]->p3_sy == pointlist[j]->p3_sy)
				dup_pairs++;

	debug_log(DLOG_TEXTURE,
	          "[mwall_submit] frame=%d pass=%d seq=%d route=%s clip=%d orig_nv=%d submit_nv=%d fan_tris=%d fan_head=%s orig_uor=0x%x orig_uand=0x%x input_behind=%d temp_created=%d post_uor=0x%x post_uand=0x%x post_behind=%d overflow=%d temp=%d projected=%d dup=%d zero=%d extra=%d sx=%.1f/%.1f/%.1f/%.1f/%.1f/%.1f sy=%.1f/%.1f/%.1f/%.1f/%.1f/%.1f codes=0x%x/0x%x/0x%x/0x%x/0x%x/0x%x flags=0x%x/0x%x/0x%x/0x%x/0x%x/0x%x",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          route ? route : "merge_raw",
	          ctx ? ctx->clip_applied : 0,
	          orig_nv,
	          nv,
	          fan_tris,
	          fan_order[0] ? fan_order : "<none>",
	          ctx ? ctx->orig_uor : 0,
	          ctx ? ctx->orig_uand : 0xff,
	          ctx ? ctx->input_behind : 0,
	          ctx ? ctx->temp_points : 0,
	          post_uor,
	          post_uand,
	          post_behind,
	          overflow,
	          temp_points,
	          projected,
	          dup_pairs,
	          zero_count,
	          extra,
	          sx[0], sx[1], sx[2], sx[3], sx[4], sx[5],
	          sy[0], sy[1], sy[2], sy[3], sy[4], sy[5],
	          codes[0], codes[1], codes[2], codes[3], codes[4], codes[5],
	          flags[0], flags[1], flags[2], flags[3], flags[4], flags[5]);
}

void android_merged_wall_log_split(
    const struct merged_wall_tmap2_submit_context *ctx,
    const struct g3s_point *const *pointlist, int nv)
{
	float sx[4], sy[4];
	float fan012, fan023, alt013, alt123;
	const char *pick = "same";
	const char *route = ctx ? ctx->route : NULL;
	int orig_nv = ctx && ctx->orig_nv > 0 ? ctx->orig_nv : nv;
	int i, fan_flip, alt_flip, fan_flat, alt_flat;

	if (!pointlist || nv != 4)
		return;

	for (i = 0; i < 4; i++) {
		sx[i] = f2fl(pointlist[i]->p3_sx);
		sy[i] = f2fl(pointlist[i]->p3_sy);
	}

	fan012 = android_merged_wall_get_triangle_area(pointlist[0], pointlist[1], pointlist[2]);
	fan023 = android_merged_wall_get_triangle_area(pointlist[0], pointlist[2], pointlist[3]);
	alt013 = android_merged_wall_get_triangle_area(pointlist[0], pointlist[1], pointlist[3]);
	alt123 = android_merged_wall_get_triangle_area(pointlist[1], pointlist[2], pointlist[3]);
	fan_flip = (fan012 < 0.0f && fan023 > 0.0f) || (fan012 > 0.0f && fan023 < 0.0f);
	alt_flip = (alt013 < 0.0f && alt123 > 0.0f) || (alt013 > 0.0f && alt123 < 0.0f);
	fan_flat = fabsf(fan012) < 0.5f || fabsf(fan023) < 0.5f;
	alt_flat = fabsf(alt013) < 0.5f || fabsf(alt123) < 0.5f;
	if ((fan_flip || fan_flat) != (alt_flip || alt_flat))
		pick = (fan_flip || fan_flat) ? "alt" : "fan";

	debug_log(DLOG_TEXTURE,
	          "[mwall_split] frame=%d pass=%d seq=%d route=%s orig_nv=%d submit_nv=%d fan=%.1f/%.1f alt=%.1f/%.1f fan_flip=%d alt_flip=%d fan_flat=%d alt_flat=%d pick=%s sx=%.1f/%.1f/%.1f/%.1f sy=%.1f/%.1f/%.1f/%.1f",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          route ? route : "merge_raw",
	          orig_nv,
	          nv,
	          fan012,
	          fan023,
	          alt013,
	          alt123,
	          fan_flip,
	          alt_flip,
	          fan_flat,
	          alt_flat,
	          pick,
	          sx[0], sx[1], sx[2], sx[3],
	          sy[0], sy[1], sy[2], sy[3]);
}

void android_merged_wall_log_diag(grs_bitmap *bmbot, grs_bitmap *bmovl,
                                  const g3s_uvl *uvl_list, const float *texcoordovl_array, int nv,
                                  int orient, int super, unsigned int prog, int tex2_debug_mode,
                                  int texfilt_level, int aniso_level)
{
	const char *botname;
	const char *mix_path;
	const char *mix_note;
	fix min_u, max_u, min_v, max_v;
	float min_ou, max_ou, min_ov, max_ov;
	float raw_pts[MERGED_WALL_DIAG_LOG_PT_COUNT * 2];
	float ovl_pts[MERGED_WALL_DIAG_LOG_PT_COUNT * 2];
	float avg_ou, avg_ov;
	float alpha_cutoff, sample_post_alpha, bottom_mix;
	float sample_alpha;
	int src254, src255, bot_real_flags, real_flags;
	int sample_x, sample_y, sample_idx;
	int bilerp00, bilerp10, bilerp01, bilerp11, sample_wrap_u, sample_wrap_v;
	int uv_bad, handle_changed;
	int vslice_rows[MERGED_WALL_DIAG_VSLICE_SAMPLES];
	int vslice_idxs[MERGED_WALL_DIAG_VSLICE_SAMPLES];
	int min_filter, mag_filter, active_prog, bound_tex0, bound_tex1, bound_tex2, mip1_w;
	unsigned int tex0_expected, tex1_expected, tex2_expected, current_handle;
	int i;

	if (!android_merged_wall_is_logging_target_bitmap(bmovl) || nv <= 0)
		return;

	min_u = max_u = uvl_list[0].u;
	min_v = max_v = uvl_list[0].v;
	min_ou = max_ou = texcoordovl_array[0];
	min_ov = max_ov = texcoordovl_array[1];
	for (i = 1; i < nv; i++) {
		if (uvl_list[i].u < min_u) min_u = uvl_list[i].u;
		if (uvl_list[i].u > max_u) max_u = uvl_list[i].u;
		if (uvl_list[i].v < min_v) min_v = uvl_list[i].v;
		if (uvl_list[i].v > max_v) max_v = uvl_list[i].v;
		if (texcoordovl_array[i * 2] < min_ou) min_ou = texcoordovl_array[i * 2];
		if (texcoordovl_array[i * 2] > max_ou) max_ou = texcoordovl_array[i * 2];
		if (texcoordovl_array[i * 2 + 1] < min_ov) min_ov = texcoordovl_array[i * 2 + 1];
		if (texcoordovl_array[i * 2 + 1] > max_ov) max_ov = texcoordovl_array[i * 2 + 1];
	}

	botname = piggy_game_bitmap_name(bmbot);
	bot_real_flags = piggy_bitmap_get_flags(bmbot);
	android_merged_wall_get_source_palette_counts(bmovl, &src254, &src255, &real_flags);
	android_merged_wall_get_source_sample(bmovl, texcoordovl_array, nv,
	                                      &avg_ou, &avg_ov, &sample_x, &sample_y, &sample_idx);
	android_merged_wall_get_source_filter_sample(bmovl, avg_ou, avg_ov,
	                                             &bilerp00, &bilerp10, &bilerp01, &bilerp11,
	                                             &sample_alpha, &sample_wrap_u, &sample_wrap_v);
	android_merged_wall_get_source_vslice(bmovl, avg_ou, min_ov, max_ov,
	                                      vslice_rows, vslice_idxs, MERGED_WALL_DIAG_VSLICE_SAMPLES);
	android_merged_wall_get_uv_points(uvl_list, texcoordovl_array, nv,
	                                  raw_pts, ovl_pts, MERGED_WALL_DIAG_LOG_PT_COUNT, &uv_bad);
	android_merged_wall_get_filter_state(bmovl->gltexture, &min_filter, &mag_filter);
	android_merged_wall_get_draw_state(bmovl->gltexture, &active_prog,
	                                   &bound_tex0, &bound_tex1, &bound_tex2, &mip1_w);
	current_handle = bmovl->gltexture ? bmovl->gltexture->handle : 0;
	if (!merged_wall_diag_first_gl_handle && current_handle)
		merged_wall_diag_first_gl_handle = current_handle;
	handle_changed = current_handle && merged_wall_diag_first_gl_handle && current_handle != merged_wall_diag_first_gl_handle;
	tex0_expected = bmbot->gltexture ? bmbot->gltexture->handle : 0;
	tex1_expected = bmovl->gltexture ? bmovl->gltexture->handle : 0;
	tex2_expected = bmovl->gltexture_mask ? bmovl->gltexture_mask->handle : 0;
	alpha_cutoff = super ? 0.0f : 0.5f;
	sample_post_alpha = sample_alpha;
	if (!super && alpha_cutoff > 0.0f)
		sample_post_alpha = sample_alpha >= alpha_cutoff ? 1.0f : 0.0f;
	bottom_mix = super ? 0.0f : (1.0f - sample_post_alpha);
	mix_path = super ? "super_mask" : "plain_alpha_cutoff";
	mix_note = super ? "mask-controls-final-alpha" : "same-face-bottom-mixes-under-overlay";
	debug_log(DLOG_TEXTURE,
	          "[mwall_diag] frame=%d pass=%d seq=%d dbg=%d orient=%d shader=%s bot=%s raw_uv=%.3f..%.3f/%.3f..%.3f ovl_uv=%.3f..%.3f/%.3f..%.3f flags=0x%x real_flags=0x%x src254=%d src255=%d sample_uv=%.3f/%.3f sample_xy=%d/%d sample_idx=%d vslice_y=%d/%d/%d/%d/%d vslice_idx=%d/%d/%d/%d/%d filt=%d/%d mips=%d texfilt=%d aniso=%d mip1_est=%d tex_handle=%u first_handle=%u handle_changed=%d tex_bytes=%d tex_lw=%d ovl_png=%d ovl_wh=%dx%d tex_wh=%dx%d tex_p2=%dx%d tex_uv=%.3f/%.3f mask=%u",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          tex2_debug_mode,
	          orient,
	          super ? "mask" : "plain",
	          botname ? botname : "<none>",
	          f2fl(min_u), f2fl(max_u), f2fl(min_v), f2fl(max_v),
	          min_ou, max_ou, min_ov, max_ov,
	          bmovl->bm_flags,
	          real_flags,
	          src254,
	          src255,
	          avg_ou,
	          avg_ov,
	          sample_x,
	          sample_y,
	          sample_idx,
	          vslice_rows[0],
	          vslice_rows[1],
	          vslice_rows[2],
	          vslice_rows[3],
	          vslice_rows[4],
	          vslice_idxs[0],
	          vslice_idxs[1],
	          vslice_idxs[2],
	          vslice_idxs[3],
	          vslice_idxs[4],
	          min_filter,
	          mag_filter,
	          bmovl->gltexture ? bmovl->gltexture->has_mipmaps : -1,
	          texfilt_level,
	          aniso_level,
	          mip1_w,
	          current_handle,
	          merged_wall_diag_first_gl_handle,
	          handle_changed,
	          bmovl->gltexture ? bmovl->gltexture->bytes : 0,
	          bmovl->gltexture ? bmovl->gltexture->lw : 0,
	          bmovl->gltexture ? bmovl->gltexture->is_png : -1,
	          bmovl->bm_w, bmovl->bm_h,
	          bmovl->gltexture ? bmovl->gltexture->w : 0,
	          bmovl->gltexture ? bmovl->gltexture->h : 0,
	          bmovl->gltexture ? bmovl->gltexture->tw : 0,
	          bmovl->gltexture ? bmovl->gltexture->th : 0,
	          bmovl->gltexture ? bmovl->gltexture->u : 0.0f,
	          bmovl->gltexture ? bmovl->gltexture->v : 0.0f,
	          bmovl->gltexture_mask ? bmovl->gltexture_mask->handle : 0);
	debug_log(DLOG_TEXTURE,
	          "[mwall_gl] frame=%d pass=%d seq=%d prog=%u/%d tex0=%u/%d tex1=%u/%d tex2=%u/%d",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          prog,
	          active_prog,
	          tex0_expected,
	          bound_tex0,
	          tex1_expected,
	          bound_tex1,
	          tex2_expected,
	          bound_tex2);
	debug_log(DLOG_TEXTURE,
	          "[mwall_uv] frame=%d pass=%d seq=%d nv=%d uv_bad=%d raw0=%.3f/%.3f raw1=%.3f/%.3f raw2=%.3f/%.3f raw3=%.3f/%.3f ovl0=%.3f/%.3f ovl1=%.3f/%.3f ovl2=%.3f/%.3f ovl3=%.3f/%.3f",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          nv,
	          uv_bad,
	          raw_pts[0], raw_pts[1],
	          raw_pts[2], raw_pts[3],
	          raw_pts[4], raw_pts[5],
	          raw_pts[6], raw_pts[7],
	          ovl_pts[0], ovl_pts[1],
	          ovl_pts[2], ovl_pts[3],
	          ovl_pts[4], ovl_pts[5],
	          ovl_pts[6], ovl_pts[7]);
	debug_log(DLOG_TEXTURE,
	          "[mwall_alpha] frame=%d pass=%d seq=%d avg_uv=%.3f/%.3f bilerp=%d/%d/%d/%d alpha=%.3f wrap=%d/%d",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          avg_ou,
	          avg_ov,
	          bilerp00,
	          bilerp10,
	          bilerp01,
	          bilerp11,
	          sample_alpha,
	          sample_wrap_u,
	          sample_wrap_v);
	debug_log(DLOG_TEXTURE,
	          "[mwall_mix] frame=%d pass=%d seq=%d path=%s note=%s seg=%d side=%d face=%d wid=%d child=%d bot=%s bot_real=0x%x ovl_real=0x%x sample_alpha=%.3f cutoff=%.2f post_alpha=%.3f bottom_mix=%.3f mask=%u",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          mix_path,
	          mix_note,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.seg : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.side : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.face : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.wid_flags : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.child : -1,
	          botname ? botname : "<none>",
	          bot_real_flags,
	          real_flags,
	          sample_alpha,
	          alpha_cutoff,
	          sample_post_alpha,
	          bottom_mix,
	          bmovl->gltexture_mask ? bmovl->gltexture_mask->handle : 0);
}

void android_merged_wall_log_state(
    const struct merged_wall_tmap2_submit_context *ctx,
    float screen_area, int depth_enabled, int blend_enabled,
    int cull_enabled, unsigned char depth_writemask, int depth_func,
    int front_face, int cull_mode, int polygon_offset_enabled,
    float polygon_offset_factor, float polygon_offset_units,
    const unsigned char color_mask[4], int draw_fbo)
{
	debug_log(DLOG_TEXTURE,
	          "[mwall_state] frame=%d pass=%d seq=%d depth=%d depthmask=%d depthfunc=0x%x blend=%d cull=%d front=0x%x cullmode=0x%x polyoff=%d polyfactor=%.1f polyunits=%.1f colormask=%d%d%d%d fbo=%d area=%.1f",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          depth_enabled,
	          depth_writemask ? 1 : 0,
	          depth_func,
	          blend_enabled,
	          cull_enabled,
	          front_face,
	          cull_mode,
	          polygon_offset_enabled,
	          polygon_offset_factor,
	          polygon_offset_units,
	          color_mask[0] ? 1 : 0,
	          color_mask[1] ? 1 : 0,
	          color_mask[2] ? 1 : 0,
	          color_mask[3] ? 1 : 0,
	          draw_fbo,
	          screen_area);
	g_merged_wall_last_draw_state.valid = 1;
	g_merged_wall_last_draw_state.frame_id = g_merged_wall_frame_id;
	g_merged_wall_last_draw_state.render_pass = g_merged_wall_render_pass;
	g_merged_wall_last_draw_state.draw_seq = g_merged_wall_draw_seq;
	g_merged_wall_last_draw_state.seg = g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.seg : -1;
	g_merged_wall_last_draw_state.side = g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.side : -1;
	g_merged_wall_last_draw_state.face = g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.face : -1;
	g_merged_wall_last_draw_state.child = g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.child : -1;
	g_merged_wall_last_draw_state.wid_flags = g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.wid_flags : -1;
	g_merged_wall_last_draw_state.tmap1 = g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.tmap1 : -1;
	g_merged_wall_last_draw_state.tmap2 = g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.tmap2 : 0;
	g_merged_wall_last_draw_state.depth_enabled = depth_enabled;
	g_merged_wall_last_draw_state.blend_enabled = blend_enabled;
	g_merged_wall_last_draw_state.cull_enabled = cull_enabled;
	g_merged_wall_last_draw_state.polygon_offset_enabled = polygon_offset_enabled;
	g_merged_wall_last_draw_state.polygon_offset_factor = polygon_offset_factor;
	g_merged_wall_last_draw_state.polygon_offset_units = polygon_offset_units;
	g_merged_wall_last_draw_state.depth_writemask = depth_writemask;
	g_merged_wall_last_draw_state.depth_func = depth_func;
	g_merged_wall_last_draw_state.front_face = front_face;
	g_merged_wall_last_draw_state.cull_mode = cull_mode;
	g_merged_wall_last_draw_state.draw_fbo = draw_fbo;
	g_merged_wall_last_draw_state.screen_area = screen_area;
	strncpy(g_merged_wall_last_draw_state.route,
	        ctx && ctx->route ? ctx->route : "",
	        sizeof(g_merged_wall_last_draw_state.route) - 1);
	g_merged_wall_last_draw_state.route[sizeof(g_merged_wall_last_draw_state.route) - 1] = '\0';
	strncpy(g_merged_wall_last_draw_state.merge_impl, "gpu_two_pass",
	        sizeof(g_merged_wall_last_draw_state.merge_impl) - 1);
	g_merged_wall_last_draw_state.merge_impl[sizeof(g_merged_wall_last_draw_state.merge_impl) - 1] = '\0';
}

static int merged_wall_single_clear_matches_face_context(void)
{
	return g_android_draw_face_ctx.valid && g_android_draw_face_ctx.tmap2 != 0;
}

static int merged_wall_should_clear_secondary_units_for_single(grs_bitmap *bm,
                                                               int *log_clear)
{
	if (log_clear)
		*log_clear = 0;
	if (!bm)
		return 0;
	if (!bm->gltexture || !g_android_draw_face_ctx.valid ||
	    !merged_wall_single_clear_matches_face_context())
		return 0;
	if (log_clear)
		*log_clear = g_merged_wall_snapshot_pending;
	return 1;
}

void android_merged_wall_clear_secondary_units_for_single(grs_bitmap *bm)
{
	GLint before_prog = -1, after_prog = -1;
	GLint before_tex0 = -1, before_tex1 = -1, before_tex2 = -1;
	GLint after_tex0 = -1, after_tex1 = -1, after_tex2 = -1;
	GLint mip1_w = -1;
	const char *bm_name;
	int log_clear = 0;

	if (!merged_wall_should_clear_secondary_units_for_single(bm, &log_clear))
		return;

	bm_name = piggy_game_bitmap_name(bm);
	if (log_clear)
		android_merged_wall_get_draw_state(bm->gltexture, &before_prog,
		                                   &before_tex0, &before_tex1, &before_tex2, &mip1_w);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	if (!log_clear)
		return;
	android_merged_wall_get_draw_state(bm->gltexture, &after_prog,
	                                   &after_tex0, &after_tex1, &after_tex2, &mip1_w);
	debug_log(DLOG_TEXTURE,
	          "[mwall_clear_units] frame=%d pass=%d seq=%d kind=single bm=%s seg=%d side=%d face=%d child=%d wid=%d tmap1=%d tmap2=0x%x prog=%d/%d before=%d/%d/%d after=%d/%d/%d",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          bm_name ? bm_name : "",
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.seg : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.side : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.face : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.child : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.wid_flags : 0,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.tmap1 : -1,
	          g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.tmap2 : 0,
	          before_prog,
	          after_prog,
	          before_tex0,
	          before_tex1,
	          before_tex2,
	          after_tex0,
	          after_tex1,
	          after_tex2);
}

static int merged_wall_is_logging_target_name(const char *name)
{
	return android_texture_debug_matches_target_name(name);
}

static int merged_wall_mark_source_log(const char *bitmapname, unsigned int bit)
{
	if (!merged_wall_is_logging_target_name(bitmapname))
		return 0;
	if (merged_wall_source_log_mask & bit)
		return 0;
	merged_wall_source_log_mask |= bit;
	return 1;
}

void android_merged_wall_log_palette_source(const char *bitmapname,
                                            const unsigned char *data, int width, int height, int bm_flags,
                                            unsigned int bit, const char *source)
{
	int total, idx254, idx255, left255, right255, top255, bottom255;
	int seam_lr_alpha, seam_tb_alpha, i;

	if (!data || width <= 0 || height <= 0 || !merged_wall_mark_source_log(bitmapname, bit))
		return;

	total = width * height;
	idx254 = 0;
	idx255 = 0;
	left255 = 0;
	right255 = 0;
	top255 = 0;
	bottom255 = 0;
	seam_lr_alpha = 0;
	seam_tb_alpha = 0;
	for (i = 0; i < total; i++) {
		if (data[i] == 254)
			idx254++;
		else if (data[i] == 255)
			idx255++;
	}
	for (i = 0; i < height; i++) {
		unsigned char left = data[i * width];
		unsigned char right = data[i * width + width - 1];

		if (left == 255)
			left255++;
		if (right == 255)
			right255++;
		if (android_merged_wall_get_source_alpha_class(left) != android_merged_wall_get_source_alpha_class(right))
			seam_lr_alpha++;
	}
	for (i = 0; i < width; i++) {
		unsigned char top = data[i];
		unsigned char bottom = data[(height - 1) * width + i];

		if (top == 255)
			top255++;
		if (bottom == 255)
			bottom255++;
		if (android_merged_wall_get_source_alpha_class(top) != android_merged_wall_get_source_alpha_class(bottom))
			seam_tb_alpha++;
	}

	debug_log(DLOG_TEXTURE,
	          "[mwall_src] source=%s kind=palette flags=0x%x size=%dx%d idx254=%d idx255=%d opaque=%d edge255=%d/%d/%d/%d seam_alpha=%d/%d",
	          source, bm_flags, width, height, idx254, idx255,
	          total - idx254 - idx255, left255, right255, top255, bottom255,
	          seam_lr_alpha, seam_tb_alpha);
}

void android_merged_wall_log_alpha_source(const char *bitmapname,
                                          const unsigned char *data, int width, int height, int channels,
                                          int bm_flags, unsigned int bit, const char *source)
{
	int total, alpha0, alpha255, alpha_partial, i;

	if (!data || width <= 0 || height <= 0 || !merged_wall_mark_source_log(bitmapname, bit))
		return;

	if (channels < 4) {
		debug_log(DLOG_TEXTURE,
		          "[mwall_src] source=%s kind=rgb flags=0x%x size=%dx%d channels=%d",
		          source, bm_flags, width, height, channels);
		return;
	}

	total = width * height;
	alpha0 = 0;
	alpha255 = 0;
	alpha_partial = 0;
	for (i = 0; i < total; i++) {
		unsigned char alpha = data[i * channels + 3];

		if (!alpha)
			alpha0++;
		else if (alpha == 255)
			alpha255++;
		else
			alpha_partial++;
	}

	debug_log(DLOG_TEXTURE,
	          "[mwall_src] source=%s kind=rgba flags=0x%x size=%dx%d alpha0=%d alpha255=%d alpha_partial=%d",
	          source, bm_flags, width, height, alpha0, alpha255, alpha_partial);
}

int android_merged_wall_is_logging_target_bitmap(grs_bitmap *bm)
{
	const char *bm_name;

	if (!bm)
		return 0;
	bm_name = piggy_game_bitmap_name(bm);
	return merged_wall_is_logging_target_name(bm_name);
}

int android_merged_wall_is_logging_target_tmap2(int tmap2)
{
	int overlay = merged_wall_overlay_index(tmap2);
	grs_bitmap *bm;

	if (!tmap2 || overlay < 0 || overlay >= NumTextures)
		return 0;
	bm = &GameBitmaps[Textures[overlay].index];
	return android_merged_wall_is_logging_target_bitmap(bm);
}

void android_merged_wall_set_draw_face_context(struct segment *segp, int sidenum,
                                               int tmap1, int tmap2, int wid_flags, int nv, int face_index)
{
	if (tmap2 != 0)
		g_merged_wall_draw_seq++;
	g_android_draw_face_ctx.valid = 1;
	g_android_draw_face_ctx.seg = (int) (segp - Segments);
	g_android_draw_face_ctx.side = sidenum;
	g_android_draw_face_ctx.face = face_index;
	g_android_draw_face_ctx.child = segp->children[sidenum];
	g_android_draw_face_ctx.side_type = segp->sides[sidenum].type;
	g_android_draw_face_ctx.nv = nv;
	g_android_draw_face_ctx.wid_flags = wid_flags;
	g_android_draw_face_ctx.tmap1 = tmap1;
	g_android_draw_face_ctx.tmap2 = tmap2;
}

void android_merged_wall_clear_draw_face_context(void)
{
	memset(&g_android_draw_face_ctx, 0, sizeof(g_android_draw_face_ctx));
	g_android_draw_face_ctx.seg = -1;
	g_android_draw_face_ctx.side = -1;
	g_android_draw_face_ctx.face = -1;
	g_android_draw_face_ctx.child = -1;
	g_android_draw_face_ctx.side_type = -1;
	g_android_draw_face_ctx.tmap1 = -1;
	g_android_draw_face_ctx.tmap2 = -1;
}

static int merged_wall_get_screen_bbox(const struct g3s_point **pointlist, int nv,
                                       float *min_sx, float *max_sx, float *min_sy, float *max_sy)
{
	float local_min_sx = 0.0f, local_max_sx = 0.0f;
	float local_min_sy = 0.0f, local_max_sy = 0.0f;
	int i;

	if (!pointlist || nv <= 0)
		return 0;

	for (i = 0; i < nv; i++) {
		float sx, sy;

		if (!(pointlist[i]->p3_flags & PF_PROJECTED))
			return 0;
		sx = f2fl(pointlist[i]->p3_sx);
		sy = f2fl(pointlist[i]->p3_sy);
		if (i == 0) {
			local_min_sx = local_max_sx = sx;
			local_min_sy = local_max_sy = sy;
		} else {
			if (sx < local_min_sx) local_min_sx = sx;
			if (sx > local_max_sx) local_max_sx = sx;
			if (sy < local_min_sy) local_min_sy = sy;
			if (sy > local_max_sy) local_max_sy = sy;
		}
	}

	*min_sx = local_min_sx;
	*max_sx = local_max_sx;
	*min_sy = local_min_sy;
	*max_sy = local_max_sy;
	return 1;
}

static int merged_wall_get_projected_bbox(const struct g3s_point **pointlist, int nv,
                                          int *projected_count,
                                          float *min_sx, float *max_sx,
                                          float *min_sy, float *max_sy)
{
	float local_min_sx = 0.0f, local_max_sx = 0.0f;
	float local_min_sy = 0.0f, local_max_sy = 0.0f;
	int count = 0;
	int i;

	if (projected_count)
		*projected_count = 0;
	if (!pointlist || nv <= 0)
		return 0;

	for (i = 0; i < nv; i++) {
		float sx, sy;

		if (!(pointlist[i]->p3_flags & PF_PROJECTED))
			continue;
		sx = f2fl(pointlist[i]->p3_sx);
		sy = f2fl(pointlist[i]->p3_sy);
		if (count == 0) {
			local_min_sx = local_max_sx = sx;
			local_min_sy = local_max_sy = sy;
		} else {
			if (sx < local_min_sx) local_min_sx = sx;
			if (sx > local_max_sx) local_max_sx = sx;
			if (sy < local_min_sy) local_min_sy = sy;
			if (sy > local_max_sy) local_max_sy = sy;
		}
		count++;
	}

	if (projected_count)
		*projected_count = count;
	if (count <= 0)
		return 0;

	*min_sx = local_min_sx;
	*max_sx = local_max_sx;
	*min_sy = local_min_sy;
	*max_sy = local_max_sy;
	return 1;
}

static int merged_wall_bbox_contains_point(float px, float py,
                                           float min_sx, float max_sx, float min_sy, float max_sy)
{
	return px >= min_sx && px <= max_sx && py >= min_sy && py <= max_sy;
}

static int merged_wall_store_projected_points(const struct g3s_point **pointlist, int nv,
                                              int capacity,
                                              int *pt_projected, float *pt_sx, float *pt_sy)
{
	int count = 0;
	int i;

	if (!pointlist || nv <= 0 || nv > capacity)
		return 0;

	for (i = 0; i < nv; i++) {
		int projected = (pointlist[i]->p3_flags & PF_PROJECTED) != 0;

		if (pt_projected)
			pt_projected[i] = projected;
		if (projected) {
			if (pt_sx)
				pt_sx[i] = f2fl(pointlist[i]->p3_sx);
			if (pt_sy)
				pt_sy[i] = f2fl(pointlist[i]->p3_sy);
			count++;
		} else {
			if (pt_sx)
				pt_sx[i] = 0.0f;
			if (pt_sy)
				pt_sy[i] = 0.0f;
		}
	}

	return count;
}

static int merged_wall_projected_polygon_valid(int nv, int projected_count)
{
	return nv >= 3 && projected_count == nv;
}

static int merged_wall_projected_polygon_contains_point(int nv,
                                                        const int *pt_projected,
                                                        const float *pt_sx,
                                                        const float *pt_sy,
                                                        float px, float py)
{
	float sign = 0.0f;
	int i;

	if (!pt_projected || !pt_sx || !pt_sy || nv < 3)
		return 0;

	for (i = 0; i < nv; i++)
		if (!pt_projected[i])
			return 0;

	for (i = 0; i < nv; i++) {
		int next = (i + 1) % nv;
		float cross = (pt_sx[next] - pt_sx[i]) * (py - pt_sy[i]) -
		              (pt_sy[next] - pt_sy[i]) * (px - pt_sx[i]);

		if (fabsf(cross) <= 0.5f)
			continue;
		if (sign == 0.0f) {
			sign = cross;
			continue;
		}
		if ((sign < 0.0f && cross > 0.0f) || (sign > 0.0f && cross < 0.0f))
			return 0;
	}

	return 1;
}

static float merged_wall_projected_triangle_area(const struct g3s_point *a,
                                                 const struct g3s_point *b, const struct g3s_point *c)
{
	float ax = f2fl(a->p3_sx), ay = f2fl(a->p3_sy);
	float bx = f2fl(b->p3_sx), by = f2fl(b->p3_sy);
	float cx = f2fl(c->p3_sx), cy = f2fl(c->p3_sy);

	return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

static void merged_wall_track_split_geometry(struct merged_wall_tracked_face *track,
                                             const struct g3s_point **pointlist, int nv)
{
	const char *pick = "same";

	if (!track)
		return;
	track->fan_area_012 = 0.0f;
	track->fan_area_023 = 0.0f;
	track->alt_area_013 = 0.0f;
	track->alt_area_123 = 0.0f;
	track->fan_flip = 0;
	track->alt_flip = 0;
	track->fan_flat = 0;
	track->alt_flat = 0;
	track->cull_sensitive = 0;
	merged_wall_copy_string(track->preferred_split, sizeof(track->preferred_split), pick);
	if (!pointlist || nv != 4)
		return;
	track->fan_area_012 = merged_wall_projected_triangle_area(pointlist[0], pointlist[1], pointlist[2]);
	track->fan_area_023 = merged_wall_projected_triangle_area(pointlist[0], pointlist[2], pointlist[3]);
	track->alt_area_013 = merged_wall_projected_triangle_area(pointlist[0], pointlist[1], pointlist[3]);
	track->alt_area_123 = merged_wall_projected_triangle_area(pointlist[1], pointlist[2], pointlist[3]);
	track->fan_flip = (track->fan_area_012 < 0.0f && track->fan_area_023 > 0.0f) || (track->fan_area_012 > 0.0f && track->fan_area_023 < 0.0f);
	track->alt_flip = (track->alt_area_013 < 0.0f && track->alt_area_123 > 0.0f) || (track->alt_area_013 > 0.0f && track->alt_area_123 < 0.0f);
	track->fan_flat = fabsf(track->fan_area_012) < 0.5f || fabsf(track->fan_area_023) < 0.5f;
	track->alt_flat = fabsf(track->alt_area_013) < 0.5f || fabsf(track->alt_area_123) < 0.5f;
	track->cull_sensitive = track->fan_flip || track->fan_flat;
	if ((track->fan_flip || track->fan_flat) != (track->alt_flip || track->alt_flat))
		pick = (track->fan_flip || track->fan_flat) ? "alt" : "fan";
	merged_wall_copy_string(track->preferred_split, sizeof(track->preferred_split), pick);
}

static float merged_wall_bbox_distance_sq(float px, float py,
                                          float min_sx, float max_sx, float min_sy, float max_sy)
{
	float dx = 0.0f, dy = 0.0f;

	if (px < min_sx)
		dx = min_sx - px;
	else if (px > max_sx)
		dx = px - max_sx;
	if (py < min_sy)
		dy = min_sy - py;
	else if (py > max_sy)
		dy = py - max_sy;
	return dx * dx + dy * dy;
}

static float merged_wall_bbox_overlap_area(float min0_sx, float max0_sx,
                                           float min0_sy, float max0_sy, float min1_sx, float max1_sx,
                                           float min1_sy, float max1_sy)
{
	float overlap_min_sx = min0_sx > min1_sx ? min0_sx : min1_sx;
	float overlap_max_sx = max0_sx < max1_sx ? max0_sx : max1_sx;
	float overlap_min_sy = min0_sy > min1_sy ? min0_sy : min1_sy;
	float overlap_max_sy = max0_sy < max1_sy ? max0_sy : max1_sy;
	float overlap_w = overlap_max_sx - overlap_min_sx;
	float overlap_h = overlap_max_sy - overlap_min_sy;

	if (overlap_w <= 0.0f || overlap_h <= 0.0f)
		return 0.0f;
	return overlap_w * overlap_h;
}

static int merged_wall_get_track_overlap_box(const struct merged_wall_tracked_face *track,
                                             float *min_sx, float *max_sx,
                                             float *min_sy, float *max_sy,
                                             float *bbox_area, const char **box_kind)
{
	if (box_kind)
		*box_kind = "none";
	if (!track)
		return 0;
	if (track->bbox_valid && track->bbox_area > 0.0f) {
		*min_sx = track->min_sx;
		*max_sx = track->max_sx;
		*min_sy = track->min_sy;
		*max_sy = track->max_sy;
		*bbox_area = track->bbox_area;
		if (box_kind)
			*box_kind = "full";
		return 1;
	}
	if (track->projected_bbox_valid && track->projected_bbox_area > 0.0f) {
		*min_sx = track->projected_min_sx;
		*max_sx = track->projected_max_sx;
		*min_sy = track->projected_min_sy;
		*max_sy = track->projected_max_sy;
		*bbox_area = track->projected_bbox_area;
		if (box_kind)
			*box_kind = "projected";
		return 1;
	}
	return 0;
}

static const char *merged_wall_snapshot_focus_bucket_name(int bucket)
{
	switch (bucket) {
		case 0:
			return "center_overlap";
		case 1:
			return "center_cover";
		case 2:
			return "center_face";
		default:
			return "nearby";
	}
}

static int merged_wall_snapshot_focus_cover_better(const struct merged_wall_snapshot_focus_cover *candidate,
                                                   const struct merged_wall_snapshot_focus_cover *current_best)
{
	if (!candidate || !candidate->valid)
		return 0;
	if (!current_best || !current_best->valid)
		return 1;
	if (candidate->focus_bucket != current_best->focus_bucket)
		return candidate->focus_bucket < current_best->focus_bucket;
	if (candidate->focus_dist2 < current_best->focus_dist2 - 0.5f)
		return 1;
	if (candidate->focus_dist2 > current_best->focus_dist2 + 0.5f)
		return 0;
	if (candidate->overlap_area > current_best->overlap_area + 0.5f)
		return 1;
	if (candidate->overlap_area < current_best->overlap_area - 0.5f)
		return 0;
	if (candidate->cover_order != current_best->cover_order)
		return candidate->cover_order > current_best->cover_order;
	if (candidate->draw_order != current_best->draw_order)
		return candidate->draw_order > current_best->draw_order;
	return candidate->event_index < current_best->event_index;
}

static void merged_wall_begin_frame_tracking(void)
{
	if (merged_wall_tracked_frame_id == g_merged_wall_frame_id)
		return;
	merged_wall_tracked_frame_id = g_merged_wall_frame_id;
	merged_wall_tracked_face_count = 0;
	merged_wall_probe_draw_face_count = 0;
	merged_wall_probe_draw_face_seen_count = 0;
	merged_wall_draw_order = 0;
}

static void merged_wall_begin_cover_events(void)
{
	if (merged_wall_cover_frame_id == g_merged_wall_frame_id)
		return;
	merged_wall_cover_frame_id = g_merged_wall_frame_id;
	merged_wall_cover_event_count = 0;
}

static int merged_wall_face_matches(const struct merged_wall_tracked_face *track,
                                    const struct g3s_point **pointlist, int nv, int *ordered)
{
	int i, j;
	int used[MERGED_WALL_LOG_PT_COUNT] = { 0 };

	if (track->nv != nv || nv <= 0 || nv > MERGED_WALL_LOG_PT_COUNT)
		return 0;

	*ordered = 1;
	for (i = 0; i < nv; i++) {
		if (track->pts[i][0] != pointlist[i]->p3_vec.x || track->pts[i][1] != pointlist[i]->p3_vec.y || track->pts[i][2] != pointlist[i]->p3_vec.z) {
			*ordered = 0;
			break;
		}
	}
	if (*ordered)
		return 1;

	for (i = 0; i < nv; i++) {
		for (j = 0; j < nv; j++) {
			if (!used[j] && track->pts[j][0] == pointlist[i]->p3_vec.x && track->pts[j][1] == pointlist[i]->p3_vec.y && track->pts[j][2] == pointlist[i]->p3_vec.z) {
				used[j] = 1;
				break;
			}
		}
		if (j == nv)
			return 0;
	}

	return 1;
}

static void merged_wall_init_portal_debug_info(struct merged_wall_portal_debug_info *info)
{
	memset(info, 0, sizeof(*info));
	info->wall_num = -1;
	info->wall_type = -1;
	info->wall_state = -1;
	info->wall_clip = -1;
	info->wall_keys = -1;
	info->conn_seg = -1;
	info->conn_side = -1;
	info->conn_child = -1;
	info->conn_side_type = -1;
	info->conn_wid = -1;
	info->conn_tmap1 = -1;
	info->conn_tmap2 = 0;
	info->conn_wall_num = -1;
	info->conn_wall_type = -1;
	info->conn_wall_state = -1;
	info->conn_wall_clip = -1;
	info->conn_wall_keys = -1;
}

static void merged_wall_fill_portal_debug_info(const struct android_draw_face_context *ctx,
                                               struct merged_wall_portal_debug_info *info)
{
	segment *segp, *conn_seg;
	int side, wall_num, conn_side, conn_wall_num;

	merged_wall_init_portal_debug_info(info);
	if (!ctx || !ctx->valid || ctx->seg < 0 || ctx->seg > Highest_segment_index)
		return;
	side = ctx->side;
	if (side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return;

	segp = &Segments[ctx->seg];
	wall_num = segp->sides[side].wall_num;
	if (wall_num >= 0 && wall_num < Num_walls) {
		wall *wallp = &Walls[wall_num];

		info->wall_num = wall_num;
		info->wall_type = wallp->type;
		info->wall_state = wallp->state;
		info->wall_clip = wallp->clip_num;
		info->wall_keys = wallp->keys;
		info->wall_flags = wallp->flags;
	}

	if (ctx->child < 0 || ctx->child > Highest_segment_index)
		return;

	conn_seg = &Segments[ctx->child];
	info->conn_seg = ctx->child;
	conn_side = find_connect_side(conn_seg, segp);
	info->conn_side = conn_side;
	if (conn_side < 0 || conn_side >= MAX_SIDES_PER_SEGMENT)
		return;

	info->conn_child = conn_seg->children[conn_side];
	info->conn_side_type = conn_seg->sides[conn_side].type;
	info->conn_wid = WALL_IS_DOORWAY(conn_seg, conn_side);
	info->conn_tmap1 = conn_seg->sides[conn_side].tmap_num;
	info->conn_tmap2 = conn_seg->sides[conn_side].tmap_num2;

	conn_wall_num = conn_seg->sides[conn_side].wall_num;
	if (conn_wall_num >= 0 && conn_wall_num < Num_walls) {
		wall *wallp = &Walls[conn_wall_num];

		info->conn_wall_num = conn_wall_num;
		info->conn_wall_type = wallp->type;
		info->conn_wall_state = wallp->state;
		info->conn_wall_clip = wallp->clip_num;
		info->conn_wall_keys = wallp->keys;
		info->conn_wall_flags = wallp->flags;
	}
}

static void merged_wall_log_texture_detail(const char *tag, int rank,
                                           const struct android_draw_face_context *ctx,
                                           int tex_num, const char *layer)
{
	grs_bitmap *bitmap;
	ogl_texture *texture;
	const char *name;
	int real_flags;
	unsigned int mask_handle = 0;
	unsigned int bm_handle = 0;
	int avg_color = 0;
	int rowsize = 0;
	int internalformat = 0;
	unsigned int format = 0;
	int bytesu = 0;
	float prio = 0.0f;
	unsigned long numrend = 0;
	int tex_flags = 0;

	if (tex_num < 0 || tex_num >= NumTextures)
		return;
	bitmap = &GameBitmaps[Textures[tex_num].index];
	texture = bitmap ? bitmap->gltexture : NULL;
	name = piggy_game_bitmap_name(bitmap);
	real_flags = piggy_bitmap_get_flags(bitmap);
	if (bitmap && bitmap->gltexture_mask)
		mask_handle = bitmap->gltexture_mask->handle;
	if (bitmap) {
		bm_handle = bitmap->bm_handle;
		avg_color = bitmap->avg_color;
		rowsize = bitmap->bm_rowsize;
	}
	if (texture) {
		internalformat = texture->internalformat;
		format = (unsigned int) texture->format;
		bytesu = texture->bytesu;
		prio = texture->prio;
		numrend = texture->numrend;
		tex_flags = texture->flags;
	}

	debug_log(DLOG_TEXTURE,
	          "[mwall_tex] tag=%s rank=%d seg=%d side=%d face=%d layer=%s tex=%d name=%s real_flags=0x%x bm_flags=0x%x bm_handle=%u avg=%d rowsize=%d handle=%u is_png=%d w=%d h=%d tw=%d th=%d lw=%d bytes=%d bytesu=%d mip=%d wrap=%d u=%.3f v=%.3f internal=0x%x format=0x%x prio=%.3f numrend=%lu tex_flags=0x%x mask_handle=%u",
	          tag ? tag : "",
	          rank,
	          ctx && ctx->valid ? ctx->seg : -1,
	          ctx && ctx->valid ? ctx->side : -1,
	          ctx && ctx->valid ? ctx->face : -1,
	          layer ? layer : "",
	          tex_num,
	          name ? name : "<none>",
	          real_flags,
	          bitmap ? bitmap->bm_flags : 0,
	          bm_handle,
	          avg_color,
	          rowsize,
	          texture ? texture->handle : 0,
	          texture ? texture->is_png : -1,
	          texture ? texture->w : 0,
	          texture ? texture->h : 0,
	          texture ? texture->tw : 0,
	          texture ? texture->th : 0,
	          texture ? texture->lw : 0,
	          texture ? texture->bytes : 0,
	          bytesu,
	          texture ? texture->has_mipmaps : -1,
	          texture ? texture->wrapstate : -1,
	          texture ? texture->u : 0.0f,
	          texture ? texture->v : 0.0f,
	          internalformat,
	          format,
	          prio,
	          numrend,
	          tex_flags,
	          mask_handle);
}

static void merged_wall_log_face_textures(const char *tag, int rank,
                                          const struct android_draw_face_context *ctx)
{
	if (!ctx || !ctx->valid)
		return;
	merged_wall_log_texture_detail(tag, rank, ctx, ctx->tmap1, "base");
	if (ctx->tmap2 != 0)
		merged_wall_log_texture_detail(tag, rank, ctx, merged_wall_overlay_index(ctx->tmap2), "overlay");
}

static void merged_wall_reset_cover_bitmap_dumps(void)
{
	merged_wall_dumped_cover_texture_count = 0;
}

static void merged_wall_reset_cover_gpu_readbacks(void)
{
	merged_wall_readback_cover_texture_count = 0;
}

static void merged_wall_reset_snapshot_target_cover(void)
{
	memset(&merged_wall_snapshot_target_cover, 0, sizeof(merged_wall_snapshot_target_cover));
}

static int merged_wall_snapshot_target_cover_better(float overlap_area,
                                                    int ordered, int draw_order)
{
	if (!merged_wall_snapshot_target_cover.valid)
		return 1;
	if (ordered != merged_wall_snapshot_target_cover.ordered)
		return ordered > merged_wall_snapshot_target_cover.ordered;
	if (overlap_area > merged_wall_snapshot_target_cover.overlap_area + 0.01f)
		return 1;
	if (overlap_area + 0.01f < merged_wall_snapshot_target_cover.overlap_area)
		return 0;
	return draw_order > merged_wall_snapshot_target_cover.draw_order;
}

static int merged_wall_note_cover_bitmap_dump(int tex_num)
{
	int i;

	for (i = 0; i < merged_wall_dumped_cover_texture_count; i++)
		if (merged_wall_dumped_cover_textures[i] == tex_num)
			return 0;
	if (merged_wall_dumped_cover_texture_count < MERGED_WALL_BITMAP_DUMP_MAX)
		merged_wall_dumped_cover_textures[merged_wall_dumped_cover_texture_count++] = tex_num;
	return 1;
}

static int merged_wall_note_cover_gpu_readback(int tex_num)
{
	int i;

	for (i = 0; i < merged_wall_readback_cover_texture_count; i++)
		if (merged_wall_readback_cover_textures[i] == tex_num)
			return 0;
	if (merged_wall_readback_cover_texture_count < MERGED_WALL_BITMAP_DUMP_MAX)
		merged_wall_readback_cover_textures[merged_wall_readback_cover_texture_count++] = tex_num;
	return 1;
}

static void merged_wall_get_debug_target_centers(float *center_x, float *center_y,
                                                 float *canvas_center_x, float *canvas_center_y)
{
	float local_center_x = 0.0f, local_center_y = 0.0f;
	float local_canvas_center_x = 0.0f, local_canvas_center_y = 0.0f;

	if (grd_curscreen) {
		local_center_x = (float) grd_curscreen->sc_w * 0.5f;
		local_center_y = (float) grd_curscreen->sc_h * 0.5f;
	}
	local_canvas_center_x = local_center_x;
	local_canvas_center_y = local_center_y;
	if (grd_curcanv) {
		local_canvas_center_x = (float) grd_curcanv->cv_bitmap.bm_x + (float) grd_curcanv->cv_bitmap.bm_w * 0.5f;
		local_canvas_center_y = (float) grd_curcanv->cv_bitmap.bm_y + (float) grd_curcanv->cv_bitmap.bm_h * 0.5f;
	}
	if (center_x) *center_x = local_center_x;
	if (center_y) *center_y = local_center_y;
	if (canvas_center_x) *canvas_center_x = local_canvas_center_x;
	if (canvas_center_y) *canvas_center_y = local_canvas_center_y;
}

static int merged_wall_probe_draw_face_better(
    const struct merged_wall_probe_draw_face *candidate,
    const struct merged_wall_probe_draw_face *current)
{
	if (!candidate || !candidate->valid)
		return 0;
	if (!current || !current->valid)
		return 1;
	if (candidate->hit_score != current->hit_score)
		return candidate->hit_score > current->hit_score;
	if (candidate->hit_score > 0) {
		if (candidate->draw_order != current->draw_order)
			return candidate->draw_order > current->draw_order;
		if (candidate->dist2 < current->dist2 - 0.5f)
			return 1;
		if (candidate->dist2 > current->dist2 + 0.5f)
			return 0;
	} else {
		if (candidate->dist2 < current->dist2 - 0.5f)
			return 1;
		if (candidate->dist2 > current->dist2 + 0.5f)
			return 0;
		if (candidate->draw_order != current->draw_order)
			return candidate->draw_order > current->draw_order;
	}
	if (candidate->bbox_area < current->bbox_area - 0.5f)
		return 1;
	if (candidate->bbox_area > current->bbox_area + 0.5f)
		return 0;
	return candidate->render_pass > current->render_pass;
}

static void merged_wall_probe_store_draw_face(
    const struct merged_wall_probe_draw_face *candidate)
{
	struct merged_wall_probe_draw_face swap;
	int index;

	if (!candidate || !candidate->valid)
		return;
	if (merged_wall_probe_draw_face_count < MERGED_WALL_PROBE_DRAW_FACE_MAX) {
		index = merged_wall_probe_draw_face_count++;
		merged_wall_probe_draw_faces[index] = *candidate;
	} else {
		index = MERGED_WALL_PROBE_DRAW_FACE_MAX - 1;
		if (!merged_wall_probe_draw_face_better(candidate,
		                                        &merged_wall_probe_draw_faces[index]))
			return;
		merged_wall_probe_draw_faces[index] = *candidate;
	}
	while (index > 0 &&
	       merged_wall_probe_draw_face_better(&merged_wall_probe_draw_faces[index],
	                                          &merged_wall_probe_draw_faces[index - 1])) {
		swap = merged_wall_probe_draw_faces[index - 1];
		merged_wall_probe_draw_faces[index - 1] = merged_wall_probe_draw_faces[index];
		merged_wall_probe_draw_faces[index] = swap;
		index--;
	}
}

void android_merged_wall_probe_record_draw_face(grs_bitmap *bm,
                                                const struct g3s_point **pointlist,
                                                const g3s_uvl *uvl_list, int nv)
{
	struct merged_wall_probe_draw_face candidate;
	float canvas_center_x = 0.0f;
	float canvas_center_y = 0.0f;
	fix64 depth_sum = 0;
	int i;

	if (!g_merged_wall_snapshot_pending ||
	    g_merged_wall_snapshot_request_mode != MERGED_WALL_REQUEST_PROBE)
		return;
	if (!g_android_draw_face_ctx.valid || !bm || !pointlist ||
	    nv <= 0 || nv > MERGED_WALL_LOG_PT_COUNT)
		return;

	memset(&candidate, 0, sizeof(candidate));
	candidate.valid = 1;
	candidate.render_pass = g_merged_wall_render_pass;
	candidate.draw_seq = g_merged_wall_draw_seq;
	candidate.draw_order = merged_wall_draw_order;
	candidate.nv = nv;
	candidate.draw_ctx = g_android_draw_face_ctx;
	candidate.bitmap = bm;
	candidate.bbox_valid = merged_wall_get_screen_bbox(pointlist, nv,
	                                                   &candidate.min_sx,
	                                                   &candidate.max_sx,
	                                                   &candidate.min_sy,
	                                                   &candidate.max_sy);
	candidate.bbox_area = candidate.bbox_valid
	                          ? (candidate.max_sx - candidate.min_sx) *
	                                (candidate.max_sy - candidate.min_sy)
	                          : 0.0f;
	candidate.projected_bbox_valid = merged_wall_get_projected_bbox(
	    pointlist, nv, &candidate.projected_count,
	    &candidate.projected_min_sx, &candidate.projected_max_sx,
	    &candidate.projected_min_sy, &candidate.projected_max_sy);
	candidate.projected_bbox_area = candidate.projected_bbox_valid
	                                    ? (candidate.projected_max_sx - candidate.projected_min_sx) *
	                                          (candidate.projected_max_sy - candidate.projected_min_sy)
	                                    : 0.0f;
	candidate.projected_count = merged_wall_store_projected_points(pointlist, nv,
	                                                               MERGED_WALL_LOG_PT_COUNT,
	                                                               candidate.pt_projected,
	                                                               candidate.pt_sx,
	                                                               candidate.pt_sy);
	if (!candidate.bbox_valid && !candidate.projected_bbox_valid)
		return;

	merged_wall_get_debug_target_centers(NULL, NULL,
	                                     &canvas_center_x, &canvas_center_y);
	candidate.polygon_hit =
	    merged_wall_projected_polygon_valid(nv, candidate.projected_count) &&
	    merged_wall_projected_polygon_contains_point(nv,
	                                                 candidate.pt_projected,
	                                                 candidate.pt_sx,
	                                                 candidate.pt_sy,
	                                                 canvas_center_x,
	                                                 canvas_center_y);
	candidate.bbox_hit =
	    candidate.bbox_valid &&
	    merged_wall_bbox_contains_point(canvas_center_x, canvas_center_y,
	                                    candidate.min_sx, candidate.max_sx,
	                                    candidate.min_sy, candidate.max_sy);
	candidate.projected_hit =
	    !candidate.bbox_hit && candidate.projected_bbox_valid &&
	    merged_wall_bbox_contains_point(canvas_center_x, canvas_center_y,
	                                    candidate.projected_min_sx,
	                                    candidate.projected_max_sx,
	                                    candidate.projected_min_sy,
	                                    candidate.projected_max_sy);
	candidate.hit_score = candidate.polygon_hit ? 3
	                                            : (candidate.bbox_hit ? 2
	                                                                  : (candidate.projected_hit ? 1 : 0));
	if (candidate.bbox_valid)
		candidate.dist2 = merged_wall_bbox_distance_sq(canvas_center_x,
		                                               canvas_center_y,
		                                               candidate.min_sx,
		                                               candidate.max_sx,
		                                               candidate.min_sy,
		                                               candidate.max_sy);
	else
		candidate.dist2 = merged_wall_bbox_distance_sq(canvas_center_x,
		                                               canvas_center_y,
		                                               candidate.projected_min_sx,
		                                               candidate.projected_max_sx,
		                                               candidate.projected_min_sy,
		                                               candidate.projected_max_sy);
	for (i = 0; i < nv; i++) {
		candidate.pts[i][0] = pointlist[i]->p3_vec.x;
		candidate.pts[i][1] = pointlist[i]->p3_vec.y;
		candidate.pts[i][2] = pointlist[i]->p3_vec.z;
		if (uvl_list)
			candidate.uvl[i] = uvl_list[i];
		else
			memset(&candidate.uvl[i], 0, sizeof(candidate.uvl[i]));
		if (i == 0) {
			candidate.depth_min = candidate.depth_max = pointlist[i]->p3_vec.z;
		} else {
			if (pointlist[i]->p3_vec.z < candidate.depth_min)
				candidate.depth_min = pointlist[i]->p3_vec.z;
			if (pointlist[i]->p3_vec.z > candidate.depth_max)
				candidate.depth_max = pointlist[i]->p3_vec.z;
		}
		depth_sum += pointlist[i]->p3_vec.z;
	}
	candidate.depth_avg = nv > 0 ? (fix) (depth_sum / nv) : 0;
	merged_wall_probe_draw_face_seen_count++;
	merged_wall_probe_store_draw_face(&candidate);
}

static int merged_wall_cover_matches_crosshair_target(
    const struct merged_wall_tracked_face *track,
    int cover_nv,
    const int *cover_pt_projected,
    const float *cover_pt_sx,
    const float *cover_pt_sy,
    int cover_projected_count,
    int cover_bbox_valid,
    float cover_min_sx,
    float cover_max_sx,
    float cover_min_sy,
    float cover_max_sy,
    float overlap_area)
{
	float center_x = 0.0f, center_y = 0.0f;
	float canvas_center_x = 0.0f, canvas_center_y = 0.0f;
	float track_min_sx = 0.0f, track_max_sx = 0.0f;
	float track_min_sy = 0.0f, track_max_sy = 0.0f;
	float track_bbox_area = 0.0f;
	float overlap_min_sx, overlap_max_sx;
	float overlap_min_sy, overlap_max_sy;
	const char *track_box_kind = "none";
	int center_face = 0, canvas_face = 0;
	int center_cover = 0, canvas_cover = 0;
	int center_overlap = 0, canvas_overlap = 0;
	int face_poly_hit = 0, canvas_face_poly_hit = 0;
	int cover_poly_hit = 0, canvas_cover_poly_hit = 0;
	int face_poly_valid, cover_poly_valid;
	int have_track_box;

	if (!track)
		return 0;

	merged_wall_get_debug_target_centers(&center_x, &center_y,
	                                     &canvas_center_x, &canvas_center_y);
	have_track_box = merged_wall_get_track_overlap_box(track,
	                                                   &track_min_sx, &track_max_sx,
	                                                   &track_min_sy, &track_max_sy,
	                                                   &track_bbox_area, &track_box_kind);
	if (cover_bbox_valid) {
		center_cover = merged_wall_bbox_contains_point(center_x, center_y,
		                                               cover_min_sx, cover_max_sx,
		                                               cover_min_sy, cover_max_sy);
		canvas_cover = merged_wall_bbox_contains_point(canvas_center_x, canvas_center_y,
		                                               cover_min_sx, cover_max_sx,
		                                               cover_min_sy, cover_max_sy);
	}
	if (have_track_box) {
		center_face = merged_wall_bbox_contains_point(center_x, center_y,
		                                              track_min_sx, track_max_sx,
		                                              track_min_sy, track_max_sy);
		canvas_face = merged_wall_bbox_contains_point(canvas_center_x, canvas_center_y,
		                                              track_min_sx, track_max_sx,
		                                              track_min_sy, track_max_sy);
		if (cover_bbox_valid && overlap_area > 0.0f) {
			overlap_min_sx = track_min_sx > cover_min_sx ? track_min_sx : cover_min_sx;
			overlap_max_sx = track_max_sx < cover_max_sx ? track_max_sx : cover_max_sx;
			overlap_min_sy = track_min_sy > cover_min_sy ? track_min_sy : cover_min_sy;
			overlap_max_sy = track_max_sy < cover_max_sy ? track_max_sy : cover_max_sy;
			center_overlap = merged_wall_bbox_contains_point(center_x, center_y,
			                                                 overlap_min_sx, overlap_max_sx,
			                                                 overlap_min_sy, overlap_max_sy);
			canvas_overlap = merged_wall_bbox_contains_point(canvas_center_x, canvas_center_y,
			                                                 overlap_min_sx, overlap_max_sx,
			                                                 overlap_min_sy, overlap_max_sy);
		}
	}
	face_poly_valid = merged_wall_projected_polygon_valid(track->nv, track->projected_count);
	if (face_poly_valid) {
		face_poly_hit = merged_wall_projected_polygon_contains_point(track->nv,
		                                                             track->pt_projected,
		                                                             track->pt_sx,
		                                                             track->pt_sy,
		                                                             center_x,
		                                                             center_y);
		canvas_face_poly_hit = merged_wall_projected_polygon_contains_point(track->nv,
		                                                                    track->pt_projected,
		                                                                    track->pt_sx,
		                                                                    track->pt_sy,
		                                                                    canvas_center_x,
		                                                                    canvas_center_y);
	}
	cover_poly_valid = merged_wall_projected_polygon_valid(cover_nv, cover_projected_count);
	if (cover_poly_valid) {
		cover_poly_hit = merged_wall_projected_polygon_contains_point(cover_nv,
		                                                              cover_pt_projected,
		                                                              cover_pt_sx,
		                                                              cover_pt_sy,
		                                                              center_x,
		                                                              center_y);
		canvas_cover_poly_hit = merged_wall_projected_polygon_contains_point(cover_nv,
		                                                                     cover_pt_projected,
		                                                                     cover_pt_sx,
		                                                                     cover_pt_sy,
		                                                                     canvas_center_x,
		                                                                     canvas_center_y);
	}

	return center_overlap || center_cover || center_face || canvas_overlap || canvas_cover || canvas_face || face_poly_hit || canvas_face_poly_hit || cover_poly_hit || canvas_cover_poly_hit;
}

static int merged_wall_cover_is_debug_target(
    const struct merged_wall_tracked_face *track,
    grs_bitmap *cover_bitmap,
    int cover_nv,
    const int *cover_pt_projected,
    const float *cover_pt_sx,
    const float *cover_pt_sy,
    int cover_projected_count,
    int cover_bbox_valid,
    float cover_min_sx,
    float cover_max_sx,
    float cover_min_sy,
    float cover_max_sy,
    float overlap_area)
{
	struct merged_wall_cached_texmerge_entry *cache_entry = NULL;
	const char *name = cover_bitmap ? piggy_game_bitmap_name(cover_bitmap) : NULL;

	if (android_texture_debug_matches_target_name(name))
		return 1;
	cache_entry = merged_wall_find_cached_texmerge_bitmap(cover_bitmap);
	if (cache_entry) {
		const char *bottom_name = cache_entry->bottom_bmp
		                              ? piggy_game_bitmap_name(cache_entry->bottom_bmp)
		                              : NULL;
		const char *top_name = cache_entry->top_bmp
		                           ? piggy_game_bitmap_name(cache_entry->top_bmp)
		                           : NULL;

		if (android_texture_debug_matches_target_name(bottom_name) ||
		    android_texture_debug_matches_target_name(top_name))
			return 1;
	}
	if (!android_texture_debug_target_is_crosshair())
		return 0;
	return merged_wall_cover_matches_crosshair_target(track,
	                                                  cover_nv,
	                                                  cover_pt_projected,
	                                                  cover_pt_sx,
	                                                  cover_pt_sy,
	                                                  cover_projected_count,
	                                                  cover_bbox_valid,
	                                                  cover_min_sx,
	                                                  cover_max_sx,
	                                                  cover_min_sy,
	                                                  cover_max_sy,
	                                                  overlap_area);
}

static unsigned int merged_wall_fnv1a_append(unsigned int hash,
                                             const unsigned char *data, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		hash ^= (unsigned int) data[i];
		hash *= 16777619u;
	}
	return hash;
}

static unsigned char merged_wall_probe_luma(const unsigned char *rgba)
{
	if (!rgba)
		return 0;
	return (unsigned char) ((54u * (unsigned int) rgba[0] +
	                         183u * (unsigned int) rgba[1] +
	                         19u * (unsigned int) rgba[2] + 128u) >>
	                        8);
}

static char merged_wall_probe_luma_hex(unsigned char luma)
{
	static const char hex[] = "0123456789abcdef";

	return hex[(luma >> 4) & 0x0f];
}

static int merged_wall_probe_read_framebuffer_pixel(float sx, float sy,
                                                    int screen_w, int screen_h,
                                                    unsigned char *rgba)
{
	int px;
	int py;

	if (!rgba || screen_w <= 0 || screen_h <= 0)
		return 0;
	px = (int) floorf(sx + 0.5f);
	py = screen_h - 1 - (int) floorf(sy + 0.5f);
	if (px < 0 || px >= screen_w || py < 0 || py >= screen_h)
		return 0;
	glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	return 1;
}

static void merged_wall_probe_capture_render_sample(const struct merged_wall_tracked_face *track,
                                                    int screen_w, int screen_h)
{
	unsigned char sample_luma[MERGED_WALL_RENDER_SAMPLE_COUNT];
	unsigned char sample_valid[MERGED_WALL_RENDER_SAMPLE_COUNT];
	char row_buf[MERGED_WALL_RENDER_SAMPLE_COLS + 1];
	float min_sx = 0.0f, max_sx = 0.0f;
	float min_sy = 0.0f, max_sy = 0.0f;
	float bbox_area = 0.0f;
	const char *box_kind = "none";
	unsigned int hash = MERGED_WALL_FNV1A_OFFSET;
	int polygon_valid;
	int valid_cells = 0;
	int hot_cells = 0;
	int luma_min = 255;
	int luma_max = 0;
	int hot_threshold = 0;
	float hot_sum = 0.0f;
	float hot_sum_x = 0.0f;
	float hot_sum_y = 0.0f;
	float hot_x = -1.0f;
	float hot_y = -1.0f;
	GLenum gl_err = GL_NO_ERROR;
	int row;
	int col;

	if (!track)
		return;
	if (!merged_wall_get_track_overlap_box(track,
	                                       &min_sx, &max_sx,
	                                       &min_sy, &max_sy,
	                                       &bbox_area, &box_kind))
		return;
	if (screen_w <= 0 || screen_h <= 0 || bbox_area <= 0.5f ||
	    max_sx <= min_sx + 0.5f || max_sy <= min_sy + 0.5f)
		return;

	memset(sample_luma, 0, sizeof(sample_luma));
	memset(sample_valid, 0, sizeof(sample_valid));
	polygon_valid = merged_wall_projected_polygon_valid(track->nv, track->projected_count);
	while (glGetError() != GL_NO_ERROR) {}

	for (row = 0; row < MERGED_WALL_RENDER_SAMPLE_ROWS; row++) {
		for (col = 0; col < MERGED_WALL_RENDER_SAMPLE_COLS; col++) {
			unsigned char rgba[4] = { 0, 0, 0, 0 };
			unsigned char sample_byte = 0xff;
			float sx = min_sx + (max_sx - min_sx) * ((float) col + 0.5f) /
			                        (float) MERGED_WALL_RENDER_SAMPLE_COLS;
			float sy = min_sy + (max_sy - min_sy) * ((float) row + 0.5f) /
			                        (float) MERGED_WALL_RENDER_SAMPLE_ROWS;
			int index = row * MERGED_WALL_RENDER_SAMPLE_COLS + col;

			row_buf[col] = '.';
			if (polygon_valid &&
			    !merged_wall_projected_polygon_contains_point(track->nv,
			                                                  track->pt_projected,
			                                                  track->pt_sx,
			                                                  track->pt_sy,
			                                                  sx,
			                                                  sy)) {
				hash = merged_wall_fnv1a_append(hash, &sample_byte, 1);
				continue;
			}
			if (!merged_wall_probe_read_framebuffer_pixel(sx, sy, screen_w, screen_h, rgba)) {
				sample_byte = 0xfe;
				row_buf[col] = '!';
				hash = merged_wall_fnv1a_append(hash, &sample_byte, 1);
				continue;
			}
			sample_byte = merged_wall_probe_luma(rgba);
			sample_valid[index] = 1;
			sample_luma[index] = sample_byte;
			valid_cells++;
			if ((int) sample_byte < luma_min)
				luma_min = (int) sample_byte;
			if ((int) sample_byte > luma_max)
				luma_max = (int) sample_byte;
			row_buf[col] = merged_wall_probe_luma_hex(sample_byte);
			hash = merged_wall_fnv1a_append(hash, &sample_byte, 1);
		}
		row_buf[MERGED_WALL_RENDER_SAMPLE_COLS] = '\0';
		debug_log_force(DLOG_TEXTURE,
		                "[mwall_tap_probe] kind=render_row frame=%d request_frame=%d seg=%d side=%d face=%d box=%s row=%d grid=%s",
		                g_merged_wall_frame_id,
		                g_merged_wall_snapshot_request_frame,
		                track->draw_ctx.valid ? track->draw_ctx.seg : -1,
		                track->draw_ctx.valid ? track->draw_ctx.side : -1,
		                track->draw_ctx.valid ? track->draw_ctx.face : -1,
		                box_kind,
		                row,
		                row_buf);
	}

	gl_err = glGetError();
	if (gl_err != GL_NO_ERROR) {
		debug_log_force(DLOG_TEXTURE,
		                "[mwall_tap_probe] kind=render status=gl_error frame=%d request_frame=%d seg=%d side=%d face=%d err=0x%x",
		                g_merged_wall_frame_id,
		                g_merged_wall_snapshot_request_frame,
		                track->draw_ctx.valid ? track->draw_ctx.seg : -1,
		                track->draw_ctx.valid ? track->draw_ctx.side : -1,
		                track->draw_ctx.valid ? track->draw_ctx.face : -1,
		                (unsigned int) gl_err);
		return;
	}
	if (valid_cells <= 0) {
		debug_log_force(DLOG_TEXTURE,
		                "[mwall_tap_probe] kind=render status=no_valid_cells frame=%d request_frame=%d seg=%d side=%d face=%d box=%s",
		                g_merged_wall_frame_id,
		                g_merged_wall_snapshot_request_frame,
		                track->draw_ctx.valid ? track->draw_ctx.seg : -1,
		                track->draw_ctx.valid ? track->draw_ctx.side : -1,
		                track->draw_ctx.valid ? track->draw_ctx.face : -1,
		                box_kind);
		return;
	}

	hot_threshold = luma_min + ((luma_max - luma_min) * 3) / 4;
	if (luma_max > luma_min && hot_threshold <= luma_min)
		hot_threshold = luma_min + (luma_max - luma_min + 1) / 2;
	for (row = 0; row < MERGED_WALL_RENDER_SAMPLE_ROWS; row++) {
		for (col = 0; col < MERGED_WALL_RENDER_SAMPLE_COLS; col++) {
			float weight;
			int index = row * MERGED_WALL_RENDER_SAMPLE_COLS + col;

			if (!sample_valid[index] || (int) sample_luma[index] < hot_threshold)
				continue;
			weight = 1.0f + (float) ((int) sample_luma[index] - hot_threshold);
			hot_sum += weight;
			hot_sum_x += weight * (((float) col + 0.5f) /
			                       (float) MERGED_WALL_RENDER_SAMPLE_COLS);
			hot_sum_y += weight * (((float) row + 0.5f) /
			                       (float) MERGED_WALL_RENDER_SAMPLE_ROWS);
			hot_cells++;
		}
	}
	if (hot_sum <= 0.0f && luma_max > luma_min) {
		for (row = 0; row < MERGED_WALL_RENDER_SAMPLE_ROWS; row++) {
			for (col = 0; col < MERGED_WALL_RENDER_SAMPLE_COLS; col++) {
				float weight;
				int index = row * MERGED_WALL_RENDER_SAMPLE_COLS + col;

				if (!sample_valid[index] || sample_luma[index] <= luma_min)
					continue;
				weight = (float) ((int) sample_luma[index] - luma_min);
				hot_sum += weight;
				hot_sum_x += weight * (((float) col + 0.5f) /
				                       (float) MERGED_WALL_RENDER_SAMPLE_COLS);
				hot_sum_y += weight * (((float) row + 0.5f) /
				                       (float) MERGED_WALL_RENDER_SAMPLE_ROWS);
				hot_cells++;
			}
		}
	}
	if (hot_sum > 0.0f) {
		hot_x = hot_sum_x / hot_sum;
		hot_y = hot_sum_y / hot_sum;
	}

	g_merged_wall_probe_result.render_sample_valid = 1;
	g_merged_wall_probe_result.render_valid_cells = valid_cells;
	g_merged_wall_probe_result.render_hot_cells = hot_cells;
	g_merged_wall_probe_result.render_hash = hash;
	g_merged_wall_probe_result.render_luma_min = luma_min;
	g_merged_wall_probe_result.render_luma_max = luma_max;
	g_merged_wall_probe_result.render_hot_x = hot_x;
	g_merged_wall_probe_result.render_hot_y = hot_y;
	memcpy(g_merged_wall_probe_result.render_sample_luma, sample_luma,
	       sizeof(g_merged_wall_probe_result.render_sample_luma));
	memcpy(g_merged_wall_probe_result.render_sample_mask, sample_valid,
	       sizeof(g_merged_wall_probe_result.render_sample_mask));
	debug_log_force(DLOG_TEXTURE,
	                "[mwall_tap_probe] kind=render frame=%d request_frame=%d seg=%d side=%d face=%d box=%s screen=%dx%d bbox=%.1f..%.1f/%.1f..%.1f valid_cells=%d hash=0x%08x luma_min=%d luma_max=%d hot_threshold=%d hot_cells=%d hot_xy=%.3f/%.3f",
	                g_merged_wall_frame_id,
	                g_merged_wall_snapshot_request_frame,
	                track->draw_ctx.valid ? track->draw_ctx.seg : -1,
	                track->draw_ctx.valid ? track->draw_ctx.side : -1,
	                track->draw_ctx.valid ? track->draw_ctx.face : -1,
	                box_kind,
	                screen_w,
	                screen_h,
	                min_sx,
	                max_sx,
	                min_sy,
	                max_sy,
	                valid_cells,
	                hash,
	                luma_min,
	                luma_max,
	                hot_threshold,
	                hot_cells,
	                hot_x,
	                hot_y);
}

static grs_bitmap *merged_wall_get_source_bitmap(grs_bitmap *bitmap)
{
	grs_bitmap *src = bitmap;

	if (!src)
		return NULL;
	if (src->bm_flags & BM_FLAG_RLE)
		src = rle_expand_texture(src);
	if (!src || !src->bm_data || src->bm_w <= 0 || src->bm_h <= 0)
		return NULL;
	return src;
}

static void merged_wall_compute_source_stats(grs_bitmap *src,
                                             unsigned int *hash_out,
                                             int *idx254_out,
                                             int *idx255_out)
{
	unsigned int hash = MERGED_WALL_FNV1A_OFFSET;
	int idx254 = 0, idx255 = 0;
	int row_stride, row_bytes;
	int x, y;

	if (!src || !src->bm_data || src->bm_w <= 0 || src->bm_h <= 0) {
		if (hash_out) *hash_out = 0;
		if (idx254_out) *idx254_out = 0;
		if (idx255_out) *idx255_out = 0;
		return;
	}

	row_stride = src->bm_rowsize > 0 ? src->bm_rowsize : src->bm_w;
	row_bytes = src->bm_w;
	for (y = 0; y < src->bm_h; y++) {
		const unsigned char *row = src->bm_data + y * row_stride;

		hash = merged_wall_fnv1a_append(hash, row, row_bytes);
		for (x = 0; x < row_bytes; x++) {
			if (row[x] == 254)
				idx254++;
			if (row[x] == 255)
				idx255++;
		}
	}

	if (hash_out) *hash_out = hash;
	if (idx254_out) *idx254_out = idx254;
	if (idx255_out) *idx255_out = idx255;
}

static unsigned int merged_wall_bitmap_hash(grs_bitmap *bm, int *bytes,
                                            int *idx254, int *idx255)
{
	grs_bitmap *src;
	unsigned int hash = 0;

	if (bytes) *bytes = 0;
	if (idx254) *idx254 = 0;
	if (idx255) *idx255 = 0;
	src = merged_wall_get_source_bitmap(bm);
	if (!src)
		return 0;
	merged_wall_compute_source_stats(src, &hash, idx254, idx255);
	if (bytes)
		*bytes = src->bm_w * src->bm_h;
	return hash;
}

static int merged_wall_note_texture_readback(unsigned int handle)
{
	int i;

	if (handle == 0)
		return 0;
	for (i = 0; i < merged_wall_texture_readback_handle_count; i++)
		if (merged_wall_texture_readback_handles[i] == handle)
			return 0;
	if (merged_wall_texture_readback_handle_count < MERGED_WALL_TEXTURE_READBACK_MAX)
		merged_wall_texture_readback_handles[merged_wall_texture_readback_handle_count++] = handle;
	return 1;
}

static void merged_wall_index_to_rgba(unsigned char idx, int bm_flags,
                                      unsigned char rgba[4])
{
	if (idx == 254 && (bm_flags & BM_FLAG_SUPER_TRANSPARENT)) {
		rgba[0] = 255;
		rgba[1] = 255;
		rgba[2] = 255;
		rgba[3] = 0;
	} else if (idx == TRANSPARENCY_COLOR && (bm_flags & BM_FLAG_TRANSPARENT)) {
		rgba[0] = 0;
		rgba[1] = 0;
		rgba[2] = 0;
		rgba[3] = 0;
	} else {
		rgba[0] = (unsigned char) (gr_palette[idx * 3] * 4);
		rgba[1] = (unsigned char) (gr_palette[idx * 3 + 1] * 4);
		rgba[2] = (unsigned char) (gr_palette[idx * 3 + 2] * 4);
		rgba[3] = 255;
	}
}

static unsigned int merged_wall_palette_hash(const unsigned char *pal)
{
	if (!pal)
		return 0;
	return merged_wall_fnv1a_append(MERGED_WALL_FNV1A_OFFSET, pal, 256 * 3);
}

static unsigned int merged_wall_pack_rgba(const unsigned char rgba[4])
{
	if (!rgba)
		return 0;
	return ((unsigned int) rgba[0] << 24) |
	       ((unsigned int) rgba[1] << 16) |
	       ((unsigned int) rgba[2] << 8) |
	       (unsigned int) rgba[3];
}

static unsigned int merged_wall_palette_index_rgba(const unsigned char *pal,
                                                   int idx, int bm_flags)
{
	unsigned char rgba[4] = { 0, 0, 0, 0 };

	if (idx < 0 || idx > 255)
		return 0;
	if (idx == 254 && (bm_flags & BM_FLAG_SUPER_TRANSPARENT)) {
		rgba[0] = 255;
		rgba[1] = 255;
		rgba[2] = 255;
		rgba[3] = 0;
	} else if (idx == TRANSPARENCY_COLOR && (bm_flags & BM_FLAG_TRANSPARENT)) {
		rgba[0] = 0;
		rgba[1] = 0;
		rgba[2] = 0;
		rgba[3] = 0;
	} else if (pal) {
		rgba[0] = (unsigned char) (pal[idx * 3] * 4);
		rgba[1] = (unsigned char) (pal[idx * 3 + 1] * 4);
		rgba[2] = (unsigned char) (pal[idx * 3 + 2] * 4);
		rgba[3] = 255;
	}
	return merged_wall_pack_rgba(rgba);
}

static int merged_wall_bitmap_sample_index(grs_bitmap *bitmap, int sample)
{
	grs_bitmap *src = merged_wall_get_source_bitmap(bitmap);
	int row_stride, x, y;

	if (!src || !src->bm_data || src->bm_w <= 0 || src->bm_h <= 0)
		return -1;
	row_stride = src->bm_rowsize > 0 ? src->bm_rowsize : src->bm_w;
	if (sample == 0) {
		x = 0;
		y = 0;
	} else if (sample == 1) {
		x = src->bm_w / 2;
		y = src->bm_h / 2;
	} else {
		x = src->bm_w - 1;
		y = src->bm_h - 1;
	}
	return src->bm_data[y * row_stride + x];
}

static void merged_wall_log_texture_gpu_readback(const char *tag, grs_bitmap *bm,
                                                 int force)
{
	grs_bitmap *src;
	ogl_texture *texture;
	const char *name;
	GLuint fbo = 0;
	GLint prev_fbo = 0;
	GLenum status;
	GLenum gl_err = GL_NO_ERROR;
	unsigned char *rgba;
	unsigned int gpu_hash = MERGED_WALL_FNV1A_OFFSET;
	unsigned int expected_hash = MERGED_WALL_FNV1A_OFFSET;
	unsigned int src_hash;
	unsigned int sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;
	unsigned char first_expected[4] = { 0, 0, 0, 0 };
	unsigned char first_actual[4] = { 0, 0, 0, 0 };
	unsigned char p0[4] = { 0, 0, 0, 0 };
	unsigned char pc[4] = { 0, 0, 0, 0 };
	int src_bytes, src254, src255;
	int read_w, read_h, pixel_count;
	int compare = 0;
	int mismatches = 0;
	int first_x = -1, first_y = -1;
	int src_stride = 0;
	int zero_a = 0, black_rgb = 0, bright_rgb = 0;
	int active_prog = 0, bound_tex0 = 0, bound_tex1 = 0, bound_tex2 = 0, mip1_w = 0;
	int x, y, i;

	if (!bm || !bm->gltexture)
		return;
	texture = bm->gltexture;
	if (texture->handle <= 0)
		return;
	if (!force && !merged_wall_note_texture_readback(texture->handle))
		return;
	read_w = texture->w > 0 ? texture->w : texture->tw;
	read_h = texture->h > 0 ? texture->h : texture->th;
	if (read_w <= 0 || read_h <= 0 || read_w > 256 || read_h > 256)
		return;
	pixel_count = read_w * read_h;
	rgba = (unsigned char *) malloc((size_t) pixel_count * 4u);
	if (!rgba)
		return;

	while (glGetError() != GL_NO_ERROR) {}
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, texture->handle, 0);
	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status == GL_FRAMEBUFFER_COMPLETE)
		glReadPixels(0, 0, read_w, read_h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	gl_err = glGetError();
	glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
	if (fbo)
		glDeleteFramebuffers(1, &fbo);
	if (status != GL_FRAMEBUFFER_COMPLETE || gl_err != GL_NO_ERROR) {
		free(rgba);
		while (glGetError() != GL_NO_ERROR) {}
		debug_log_force(DLOG_TEXTURE,
		                "[mwall_texread] tag=%s status=skip reason=%s frame=%d pass=%d seq=%d handle=%u fbo_status=0x%x err=0x%x",
		                tag ? tag : "",
		                status != GL_FRAMEBUFFER_COMPLETE ? "fbo_incomplete" : "readpixels_failed",
		                g_merged_wall_frame_id,
		                g_merged_wall_render_pass,
		                g_merged_wall_draw_seq,
		                texture->handle,
		                status,
		                gl_err);
		return;
	}

	name = piggy_game_bitmap_name(bm);
	src = merged_wall_get_source_bitmap(bm);
	src_hash = merged_wall_bitmap_hash(bm, &src_bytes, &src254, &src255);
	compare = src && src->bm_w == read_w && src->bm_h == read_h;
	if (src)
		src_stride = src->bm_rowsize > 0 ? src->bm_rowsize : src->bm_w;
	for (y = 0; y < read_h; y++) {
		for (x = 0; x < read_w; x++) {
			unsigned char expected[4] = { 0, 0, 0, 255 };
			unsigned char *actual = rgba + ((y * read_w + x) * 4);

			gpu_hash = merged_wall_fnv1a_append(gpu_hash, actual, 4);
			sum_r += actual[0];
			sum_g += actual[1];
			sum_b += actual[2];
			sum_a += actual[3];
			if (actual[3] == 0)
				zero_a++;
			if (actual[0] == 0 && actual[1] == 0 && actual[2] == 0)
				black_rgb++;
			if (actual[0] > 220 && actual[1] > 220 && actual[2] > 220)
				bright_rgb++;
			if (!compare)
				continue;
			merged_wall_index_to_rgba(src->bm_data[y * src_stride + x],
			                          bm->bm_flags, expected);
			expected_hash = merged_wall_fnv1a_append(expected_hash, expected, 4);
			if (memcmp(expected, actual, 4)) {
				mismatches++;
				if (first_x < 0) {
					first_x = x;
					first_y = y;
					memcpy(first_expected, expected, sizeof(first_expected));
					memcpy(first_actual, actual, sizeof(first_actual));
				}
			}
		}
	}
	if (!compare)
		expected_hash = 0;
	memcpy(p0, rgba, sizeof(p0));
	memcpy(pc, rgba + (((read_h / 2) * read_w + (read_w / 2)) * 4),
	       sizeof(pc));
	free(rgba);
	while (glGetError() != GL_NO_ERROR) {}
	android_merged_wall_get_draw_state(texture, &active_prog, &bound_tex0,
	                                   &bound_tex1, &bound_tex2, &mip1_w);
	i = pixel_count > 0 ? pixel_count : 1;
	debug_log_force(DLOG_TEXTURE,
	                "[mwall_texread] tag=%s status=ok frame=%d pass=%d seq=%d seg=%d side=%d face=%d name=%s ptr=%p handle=%u format=0x%x internal=0x%x tex=%dx%d src=%dx%d compare=%d src_hash=0x%08x expected_rgba=0x%08x gpu_rgba=0x%08x mismatches=%d first=%d/%d exp=%u/%u/%u/%u got=%u/%u/%u/%u avg=%u/%u/%u/%u zero_a=%d black=%d bright=%d p0=%u/%u/%u/%u pc=%u/%u/%u/%u prog=%d bound=%d/%d/%d mip1=%d",
	                tag ? tag : "",
	                g_merged_wall_frame_id,
	                g_merged_wall_render_pass,
	                g_merged_wall_draw_seq,
	                g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.seg : -1,
	                g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.side : -1,
	                g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.face : -1,
	                name ? name : "<none>",
	                (void *) bm,
	                texture->handle,
	                (unsigned int) texture->format,
	                (unsigned int) texture->internalformat,
	                read_w,
	                read_h,
	                src ? src->bm_w : 0,
	                src ? src->bm_h : 0,
	                compare,
	                src_hash,
	                expected_hash,
	                gpu_hash,
	                compare ? mismatches : -1,
	                first_x,
	                first_y,
	                first_expected[0], first_expected[1], first_expected[2], first_expected[3],
	                first_actual[0], first_actual[1], first_actual[2], first_actual[3],
	                sum_r / (unsigned int) i,
	                sum_g / (unsigned int) i,
	                sum_b / (unsigned int) i,
	                sum_a / (unsigned int) i,
	                zero_a,
	                black_rgb,
	                bright_rgb,
	                p0[0], p0[1], p0[2], p0[3],
	                pc[0], pc[1], pc[2], pc[3],
	                active_prog,
	                bound_tex0,
	                bound_tex1,
	                bound_tex2,
	                mip1_w);
}

static unsigned char merged_wall_oriented_top_pixel(grs_bitmap *top, int orient,
                                                    int x, int y, int wh)
{
	switch (orient) {
		case 1:
			return top->bm_data[wh * x + ((wh - 1) - y)];
		case 2:
			return top->bm_data[wh * ((wh - 1) - y) + ((wh - 1) - x)];
		case 3:
			return top->bm_data[wh * ((wh - 1) - x) + y];
		default:
			return top->bm_data[wh * y + x];
	}
}

static void merged_wall_log_merge_reference(const char *tag, const char *game,
                                            int tmap_bottom, int tmap_top,
                                            grs_bitmap *bottom_bmp,
                                            grs_bitmap *top_bmp,
                                            grs_bitmap *merged_bmp,
                                            int slot, int orient)
{
	grs_bitmap *bottom = merged_wall_get_source_bitmap(bottom_bmp);
	grs_bitmap *top = merged_wall_get_source_bitmap(top_bmp);
	grs_bitmap *live = merged_wall_get_source_bitmap(merged_bmp);
	unsigned int ref_hash = MERGED_WALL_FNV1A_OFFSET;
	unsigned int live_hash;
	int live_bytes, live254, live255;
	int ref254 = 0, ref255 = 0, mismatches = 0;
	int first_x = -1, first_y = -1;
	int first_ref = -1, first_live = -1;
	int super_merge;
	int x, y, wh;

	if (!bottom || !top || !live)
		return;
	if (bottom->bm_w != bottom->bm_h || top->bm_w != top->bm_h ||
	    bottom->bm_w != top->bm_w || bottom->bm_h != top->bm_h)
		return;
	wh = bottom->bm_w;
	if (live->bm_w != wh || live->bm_h != wh)
		return;
	super_merge = (top_bmp->bm_flags & BM_FLAG_SUPER_TRANSPARENT) != 0;
	for (y = 0; y < wh; y++) {
		for (x = 0; x < wh; x++) {
			unsigned char c = merged_wall_oriented_top_pixel(top, orient, x, y, wh);
			unsigned char ref;
			unsigned char actual;

			if (c == TRANSPARENCY_COLOR)
				ref = bottom->bm_data[y * bottom->bm_rowsize + x];
			else if (super_merge && c == 254)
				ref = TRANSPARENCY_COLOR;
			else
				ref = c;
			actual = live->bm_data[y * live->bm_rowsize + x];
			ref_hash = merged_wall_fnv1a_append(ref_hash, &ref, 1);
			if (ref == 254)
				ref254++;
			if (ref == 255)
				ref255++;
			if (ref != actual) {
				mismatches++;
				if (first_x < 0) {
					first_x = x;
					first_y = y;
					first_ref = ref;
					first_live = actual;
				}
			}
		}
	}
	live_hash = merged_wall_bitmap_hash(merged_bmp,
	                                    &live_bytes, &live254, &live255);
	debug_log_force(DLOG_TEXTURE,
	                "[mwall_merge_ref] tag=%s game=%s frame=%d pass=%d seq=%d slot=%d orient=%d super=%d seg=%d side=%d face=%d tmap1=%d tmap2=0x%x wh=%d ref_hash=0x%08x live_hash=0x%08x mismatches=%d first=%d/%d ref=%d live=%d ref_254=%d ref_255=%d live_254=%d live_255=%d merged_ptr=%p merged_gl=%u",
	                tag ? tag : "",
	                game ? game : "",
	                g_merged_wall_frame_id,
	                g_merged_wall_render_pass,
	                g_merged_wall_draw_seq,
	                slot,
	                orient,
	                super_merge,
	                g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.seg : -1,
	                g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.side : -1,
	                g_android_draw_face_ctx.valid ? g_android_draw_face_ctx.face : -1,
	                tmap_bottom,
	                tmap_top,
	                wh,
	                ref_hash,
	                live_hash,
	                mismatches,
	                first_x,
	                first_y,
	                first_ref,
	                first_live,
	                ref254,
	                ref255,
	                live254,
	                live255,
	                (void *) merged_bmp,
	                merged_bmp && merged_bmp->gltexture ? merged_bmp->gltexture->handle : 0);
}

static void merged_wall_bytes_to_hex(char *out, int out_size,
                                     const unsigned char *data, int len)
{
	static const char hex[] = "0123456789abcdef";
	int i, pos = 0;

	if (!out || out_size <= 0)
		return;
	for (i = 0; i < len && pos + 2 < out_size; i++) {
		out[pos++] = hex[(data[i] >> 4) & 0xf];
		out[pos++] = hex[data[i] & 0xf];
	}
	out[pos] = '\0';
}

static int merged_wall_get_uv_bbox(const g3s_uvl *uvl_list, int nv,
                                   float *min_u, float *max_u,
                                   float *min_v, float *max_v)
{
	float local_min_u = 0.0f, local_max_u = 0.0f;
	float local_min_v = 0.0f, local_max_v = 0.0f;
	int i;

	if (!uvl_list || nv <= 0)
		return 0;

	for (i = 0; i < nv; i++) {
		float u = f2fl(uvl_list[i].u);
		float v = f2fl(uvl_list[i].v);

		if (i == 0) {
			local_min_u = local_max_u = u;
			local_min_v = local_max_v = v;
		} else {
			if (u < local_min_u) local_min_u = u;
			if (u > local_max_u) local_max_u = u;
			if (v < local_min_v) local_min_v = v;
			if (v > local_max_v) local_max_v = v;
		}
	}

	if (min_u) *min_u = local_min_u;
	if (max_u) *max_u = local_max_u;
	if (min_v) *min_v = local_min_v;
	if (max_v) *max_v = local_max_v;
	return 1;
}

static float merged_wall_log2_ratio(float numer, float denom)
{
	if (numer <= 0.0f || denom <= 0.0f)
		return -99.0f;
	return logf(numer / denom) / logf(2.0f);
}

static void merged_wall_log_cover_bitmap_skip(const char *reason,
                                              const char *kind,
                                              const char *shader_kind,
                                              const struct android_draw_face_context *cover_ctx,
                                              grs_bitmap *cover_bitmap)
{
	const char *name;

	if (!g_merged_wall_snapshot_pending || !cover_ctx || !cover_ctx->valid)
		return;
	name = cover_bitmap ? piggy_game_bitmap_name(cover_bitmap) : NULL;
	debug_log(DLOG_TEXTURE,
	          "[mwall_cover_src_skip] reason=%s kind=%s shader=%s seg=%d side=%d face=%d child=%d wid=%d tmap1=%d tmap2=0x%x name=%s has_bitmap=%d has_gl=%d",
	          reason ? reason : "",
	          kind ? kind : "",
	          shader_kind ? shader_kind : "",
	          cover_ctx->seg,
	          cover_ctx->side,
	          cover_ctx->face,
	          cover_ctx->child,
	          cover_ctx->wid_flags,
	          cover_ctx->tmap1,
	          cover_ctx->tmap2,
	          name ? name : "<none>",
	          cover_bitmap != NULL,
	          cover_bitmap && cover_bitmap->gltexture != NULL);
}

static void merged_wall_log_cover_bitmap_dump(const char *kind,
                                              const char *shader_kind, float overlap_area, const char *track_box_kind,
                                              const struct android_draw_face_context *cover_ctx, grs_bitmap *cover_bitmap)
{
	grs_bitmap *src;
	const char *name;
	unsigned int hash = 0;
	int real_flags;
	int idx254 = 0, idx255 = 0;
	int row_stride;
	int row_bytes;
	int total_bytes;
	int y;

	if (!cover_ctx || !cover_ctx->valid || !cover_bitmap)
		return;
	src = merged_wall_get_source_bitmap(cover_bitmap);
	name = piggy_game_bitmap_name(cover_bitmap);
	real_flags = piggy_bitmap_get_flags(cover_bitmap);
	if (!src) {
		debug_log(DLOG_TEXTURE,
		          "[mwall_cover_src] kind=%s shader=%s seg=%d side=%d face=%d child=%d wid=%d tmap1=%d name=%s handle=%u state=missing overlap=%.1f face_box=%s",
		          kind ? kind : "",
		          shader_kind ? shader_kind : "",
		          cover_ctx->seg,
		          cover_ctx->side,
		          cover_ctx->face,
		          cover_ctx->child,
		          cover_ctx->wid_flags,
		          cover_ctx->tmap1,
		          name ? name : "<none>",
		          cover_bitmap->bm_handle,
		          overlap_area,
		          track_box_kind ? track_box_kind : "exact");
		return;
	}

	row_stride = src->bm_rowsize > 0 ? src->bm_rowsize : src->bm_w;
	row_bytes = src->bm_w;
	total_bytes = src->bm_w * src->bm_h;
	merged_wall_compute_source_stats(src, &hash, &idx254, &idx255);

	debug_log(DLOG_TEXTURE,
	          "[mwall_cover_src] kind=%s shader=%s seg=%d side=%d face=%d child=%d wid=%d tmap1=%d name=%s handle=%u real_flags=0x%x bm_flags=0x%x expanded=%d w=%d h=%d rowsize=%d bytes=%d idx254=%d idx255=%d hash=0x%08x overlap=%.1f face_box=%s",
	          kind ? kind : "",
	          shader_kind ? shader_kind : "",
	          cover_ctx->seg,
	          cover_ctx->side,
	          cover_ctx->face,
	          cover_ctx->child,
	          cover_ctx->wid_flags,
	          cover_ctx->tmap1,
	          name ? name : "<none>",
	          cover_bitmap->bm_handle,
	          real_flags,
	          cover_bitmap->bm_flags,
	          src != cover_bitmap,
	          src->bm_w,
	          src->bm_h,
	          row_stride,
	          total_bytes,
	          idx254,
	          idx255,
	          hash,
	          overlap_area,
	          track_box_kind ? track_box_kind : "exact");

	if (!g_merged_wall_snapshot_pending)
		return;
	if (src->bm_w > MERGED_WALL_BITMAP_DUMP_WIDTH || src->bm_h > MERGED_WALL_BITMAP_DUMP_WIDTH) {
		merged_wall_log_cover_bitmap_skip("oversize_bitmap", kind, shader_kind,
		                                  cover_ctx, cover_bitmap);
		return;
	}
	if (!merged_wall_note_cover_bitmap_dump(cover_ctx->tmap1)) {
		merged_wall_log_cover_bitmap_skip("duplicate_tmap1", kind, shader_kind,
		                                  cover_ctx, cover_bitmap);
		return;
	}

	for (y = 0; y < src->bm_h; y++) {
		char hex_row[MERGED_WALL_BITMAP_DUMP_WIDTH * 2 + 1];
		const unsigned char *row = src->bm_data + y * row_stride;

		merged_wall_bytes_to_hex(hex_row, sizeof(hex_row), row, row_bytes);
		debug_log(DLOG_TEXTURE,
		          "[mwall_cover_src_row] kind=%s tmap1=%d name=%s row=%d data=%s",
		          kind ? kind : "",
		          cover_ctx->tmap1,
		          name ? name : "<none>",
		          y,
		          hex_row);
	}
}

static void merged_wall_log_cover_light_state(const char *kind,
                                              const char *shader_kind,
                                              int face_pass, int face_seq, int face_order,
                                              int draw_order, int ordered, float overlap_area,
                                              const char *track_box_kind,
                                              const struct android_draw_face_context *face_ctx,
                                              const struct android_draw_face_context *cover_ctx,
                                              grs_bitmap *cover_bitmap,
                                              const float *color_array, int nv)
{
	const char *name;
	float min_r = 0.0f, min_g = 0.0f, min_b = 0.0f, min_a = 0.0f;
	float max_r = 0.0f, max_g = 0.0f, max_b = 0.0f, max_a = 0.0f;
	float avg_r = 0.0f, avg_g = 0.0f, avg_b = 0.0f, avg_a = 0.0f;
	float v[4][4] = { { 0.0f } };
	int black_rgb = 0, dark_rgb = 0, white_rgb = 0;
	int i;

	if (!cover_ctx || !cover_ctx->valid || !cover_bitmap || !color_array || nv <= 0)
		return;

	name = piggy_game_bitmap_name(cover_bitmap);
	for (i = 0; i < nv; i++) {
		const float r = color_array[i * 4];
		const float g = color_array[i * 4 + 1];
		const float b = color_array[i * 4 + 2];
		const float a = color_array[i * 4 + 3];
		const float rgb_avg = (r + g + b) / 3.0f;

		if (i == 0) {
			min_r = max_r = r;
			min_g = max_g = g;
			min_b = max_b = b;
			min_a = max_a = a;
		} else {
			if (r < min_r) min_r = r;
			if (g < min_g) min_g = g;
			if (b < min_b) min_b = b;
			if (a < min_a) min_a = a;
			if (r > max_r) max_r = r;
			if (g > max_g) max_g = g;
			if (b > max_b) max_b = b;
			if (a > max_a) max_a = a;
		}
		avg_r += r;
		avg_g += g;
		avg_b += b;
		avg_a += a;
		if (r <= 0.001f && g <= 0.001f && b <= 0.001f)
			black_rgb++;
		if (rgb_avg <= 0.05f)
			dark_rgb++;
		if (r >= 0.999f && g >= 0.999f && b >= 0.999f)
			white_rgb++;
		if (i < 4) {
			v[i][0] = r;
			v[i][1] = g;
			v[i][2] = b;
			v[i][3] = a;
		}
	}
	avg_r /= (float) nv;
	avg_g /= (float) nv;
	avg_b /= (float) nv;
	avg_a /= (float) nv;

	debug_log(DLOG_TEXTURE,
	          "[mwall_cover_light] kind=%s shader=%s frame=%d pass=%d seq=%d face_pass=%d face_seq=%d face_order=%d cover_order=%d ordered=%d face_seg=%d face_side=%d face_face=%d face_child=%d face_wid=%d face_tmap1=%d face_tmap2=0x%x seg=%d side=%d face=%d child=%d wid=%d tmap1=%d name=%s nv=%d bm_flags=0x%x real_flags=0x%x no_lighting=%d rgb_min=%.3f/%.3f/%.3f rgb_max=%.3f/%.3f/%.3f rgb_avg=%.3f/%.3f/%.3f alpha_min=%.3f alpha_max=%.3f alpha_avg=%.3f black=%d dark=%d white=%d v0=%.3f/%.3f/%.3f/%.3f v1=%.3f/%.3f/%.3f/%.3f v2=%.3f/%.3f/%.3f/%.3f v3=%.3f/%.3f/%.3f/%.3f overlap=%.1f face_box=%s",
	          kind ? kind : "",
	          shader_kind ? shader_kind : "",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          face_pass,
	          face_seq,
	          face_order,
	          draw_order,
	          ordered,
	          face_ctx && face_ctx->valid ? face_ctx->seg : -1,
	          face_ctx && face_ctx->valid ? face_ctx->side : -1,
	          face_ctx && face_ctx->valid ? face_ctx->face : -1,
	          face_ctx && face_ctx->valid ? face_ctx->child : -1,
	          face_ctx && face_ctx->valid ? face_ctx->wid_flags : 0,
	          face_ctx && face_ctx->valid ? face_ctx->tmap1 : -1,
	          face_ctx && face_ctx->valid ? face_ctx->tmap2 : 0,
	          cover_ctx->seg,
	          cover_ctx->side,
	          cover_ctx->face,
	          cover_ctx->child,
	          cover_ctx->wid_flags,
	          cover_ctx->tmap1,
	          name ? name : "<none>",
	          nv,
	          cover_bitmap->bm_flags,
	          piggy_bitmap_get_flags(cover_bitmap),
	          !!(cover_bitmap->bm_flags & BM_FLAG_NO_LIGHTING),
	          min_r,
	          min_g,
	          min_b,
	          max_r,
	          max_g,
	          max_b,
	          avg_r,
	          avg_g,
	          avg_b,
	          min_a,
	          max_a,
	          avg_a,
	          black_rgb,
	          dark_rgb,
	          white_rgb,
	          v[0][0], v[0][1], v[0][2], v[0][3],
	          v[1][0], v[1][1], v[1][2], v[1][3],
	          v[2][0], v[2][1], v[2][2], v[2][3],
	          v[3][0], v[3][1], v[3][2], v[3][3],
	          overlap_area,
	          track_box_kind ? track_box_kind : "exact");
}

static void merged_wall_log_cover_gpu_skip(const char *reason,
                                           const char *kind,
                                           const char *shader_kind,
                                           const struct android_draw_face_context *cover_ctx,
                                           grs_bitmap *cover_bitmap,
                                           unsigned int status,
                                           unsigned int gl_err)
{
	const char *name = cover_bitmap ? piggy_game_bitmap_name(cover_bitmap) : NULL;

	if (!cover_ctx || !cover_ctx->valid)
		return;
	debug_log(DLOG_TEXTURE,
	          "[mwall_cover_gpu_skip] reason=%s kind=%s shader=%s seg=%d side=%d face=%d child=%d wid=%d tmap1=%d name=%s handle=%u status=0x%x err=0x%x",
	          reason ? reason : "",
	          kind ? kind : "",
	          shader_kind ? shader_kind : "",
	          cover_ctx->seg,
	          cover_ctx->side,
	          cover_ctx->face,
	          cover_ctx->child,
	          cover_ctx->wid_flags,
	          cover_ctx->tmap1,
	          name ? name : "<none>",
	          cover_bitmap && cover_bitmap->gltexture ? cover_bitmap->gltexture->handle : 0,
	          status,
	          gl_err);
}

static void merged_wall_log_cover_gpu_readback(const char *kind,
                                               const char *shader_kind,
                                               int face_pass, int face_seq, int face_order,
                                               int draw_order, int ordered, float overlap_area,
                                               const char *track_box_kind,
                                               const struct android_draw_face_context *face_ctx,
                                               const struct android_draw_face_context *cover_ctx,
                                               grs_bitmap *cover_bitmap,
                                               int is_debug_target)
{
	struct merged_wall_cached_texmerge_entry *cache_entry = NULL;
	grs_bitmap *src;
	ogl_texture *texture;
	const char *name;
	const char *logical_bot_name;
	const char *logical_ovl_name;
	GLuint fbo = 0;
	GLint prev_fbo = 0;
	GLenum status = GL_FRAMEBUFFER_COMPLETE;
	GLenum gl_err = GL_NO_ERROR;
	unsigned char *rgba = NULL;
	unsigned char p0[4] = { 0 }, center[4] = { 0 };
	unsigned int src_hash = 0, gpu_hash = 0;
	int src_idx254 = 0, src_idx255 = 0;
	int read_w, read_h, pixel_count, black_rgb = 0;
	int avg_r = 0, avg_g = 0, avg_b = 0, avg_a = 0;
	int i;
	unsigned int sum_r = 0, sum_g = 0, sum_b = 0, sum_a = 0;

	if (!cover_ctx || !cover_ctx->valid || !cover_bitmap || !cover_bitmap->gltexture)
		return;
	cache_entry = merged_wall_find_cached_texmerge_bitmap(cover_bitmap);
	name = piggy_game_bitmap_name(cover_bitmap);
	logical_bot_name = cache_entry && cache_entry->bottom_bmp
	                       ? piggy_game_bitmap_name(cache_entry->bottom_bmp)
	                       : name;
	logical_ovl_name = cache_entry && cache_entry->top_bmp
	                       ? piggy_game_bitmap_name(cache_entry->top_bmp)
	                       : NULL;
	if (!is_debug_target)
		return;
	if (!merged_wall_note_cover_gpu_readback(cache_entry
	                                             ? 100000 + cache_entry->slot
	                                             : cover_ctx->tmap1))
		return;

	texture = cover_bitmap->gltexture;
	read_w = texture->w > 0 ? texture->w : texture->tw;
	read_h = texture->h > 0 ? texture->h : texture->th;
	if (texture->handle <= 0) {
		merged_wall_log_cover_gpu_skip("missing_handle", kind, shader_kind,
		                               cover_ctx, cover_bitmap, 0, 0);
		return;
	}
	if (read_w <= 0 || read_h <= 0 || read_w > 256 || read_h > 256) {
		merged_wall_log_cover_gpu_skip("oversize_texture", kind, shader_kind,
		                               cover_ctx, cover_bitmap, 0, 0);
		return;
	}

	src = cache_entry && cache_entry->bottom_bmp
	          ? merged_wall_get_source_bitmap(cache_entry->bottom_bmp)
	          : merged_wall_get_source_bitmap(cover_bitmap);
	if (!src && cache_entry)
		src = cache_entry->bottom_bmp;
	if (src)
		merged_wall_compute_source_stats(src, &src_hash, &src_idx254, &src_idx255);

	while (glGetError() != GL_NO_ERROR) {}
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
	                       GL_TEXTURE_2D, texture->handle, 0);
	status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
		if (fbo)
			glDeleteFramebuffers(1, &fbo);
		gl_err = glGetError();
		while (glGetError() != GL_NO_ERROR) {}
		merged_wall_log_cover_gpu_skip("fbo_incomplete", kind, shader_kind,
		                               cover_ctx, cover_bitmap, status, gl_err);
		return;
	}

	pixel_count = read_w * read_h;
	rgba = (unsigned char *) malloc((size_t) pixel_count * 4u);
	if (!rgba) {
		glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
		if (fbo)
			glDeleteFramebuffers(1, &fbo);
		merged_wall_log_cover_gpu_skip("oom", kind, shader_kind,
		                               cover_ctx, cover_bitmap, status, 0);
		return;
	}

	glReadPixels(0, 0, read_w, read_h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	gl_err = glGetError();
	glBindFramebuffer(GL_FRAMEBUFFER, prev_fbo);
	if (fbo)
		glDeleteFramebuffers(1, &fbo);
	if (gl_err != GL_NO_ERROR) {
		free(rgba);
		while (glGetError() != GL_NO_ERROR) {}
		merged_wall_log_cover_gpu_skip("readpixels_failed", kind, shader_kind,
		                               cover_ctx, cover_bitmap, status, gl_err);
		return;
	}

	gpu_hash = merged_wall_fnv1a_append(MERGED_WALL_FNV1A_OFFSET, rgba, pixel_count * 4);
	for (i = 0; i < pixel_count; i++) {
		const unsigned char *px = rgba + i * 4;

		sum_r += px[0];
		sum_g += px[1];
		sum_b += px[2];
		sum_a += px[3];
		if (px[0] == 0 && px[1] == 0 && px[2] == 0)
			black_rgb++;
	}
	avg_r = (int) (sum_r / (unsigned int) pixel_count);
	avg_g = (int) (sum_g / (unsigned int) pixel_count);
	avg_b = (int) (sum_b / (unsigned int) pixel_count);
	avg_a = (int) (sum_a / (unsigned int) pixel_count);
	memcpy(p0, rgba, sizeof(p0));
	memcpy(center,
	       rgba + (((read_h / 2) * read_w + (read_w / 2)) * 4),
	       sizeof(center));
	free(rgba);
	while (glGetError() != GL_NO_ERROR) {}

	if (merged_wall_snapshot_target_cover_better(overlap_area, ordered, draw_order)) {
		struct merged_wall_snapshot_target_cover *target = &merged_wall_snapshot_target_cover;

		memset(target, 0, sizeof(*target));
		target->valid = 1;
		target->ordered = ordered;
		target->render_pass = g_merged_wall_render_pass;
		target->draw_seq = g_merged_wall_draw_seq;
		target->draw_order = face_order;
		target->cover_order = draw_order;
		target->seg = face_ctx && face_ctx->valid ? face_ctx->seg : -1;
		target->side = face_ctx && face_ctx->valid ? face_ctx->side : -1;
		target->face = face_ctx && face_ctx->valid ? face_ctx->face : -1;
		target->child = face_ctx && face_ctx->valid ? face_ctx->child : -1;
		target->wid_flags = face_ctx && face_ctx->valid ? face_ctx->wid_flags : 0;
		target->cover_seg = cover_ctx->seg;
		target->cover_side = cover_ctx->side;
		target->cover_face = cover_ctx->face;
		target->cover_child = cover_ctx->child;
		target->cover_wid_flags = cover_ctx->wid_flags;
		target->tmap1 = cover_ctx->tmap1;
		target->tex_w = read_w;
		target->tex_h = read_h;
		target->src_hash = src_hash;
		target->gpu_hash = gpu_hash;
		target->src_idx254 = src_idx254;
		target->src_idx255 = src_idx255;
		target->gpu_avg_r = avg_r;
		target->gpu_avg_g = avg_g;
		target->gpu_avg_b = avg_b;
		target->gpu_avg_a = avg_a;
		target->gpu_black = black_rgb;
		target->p0_r = p0[0];
		target->p0_g = p0[1];
		target->p0_b = p0[2];
		target->p0_a = p0[3];
		target->center_r = center[0];
		target->center_g = center[1];
		target->center_b = center[2];
		target->center_a = center[3];
		target->overlap_area = overlap_area;
		merged_wall_copy_string(target->kind_name, sizeof(target->kind_name),
		                        kind ? kind : "");
		merged_wall_copy_string(target->face_box, sizeof(target->face_box),
		                        track_box_kind ? track_box_kind : "exact");
		merged_wall_copy_string(target->cover_shader, sizeof(target->cover_shader),
		                        shader_kind ? shader_kind : "");
		merged_wall_copy_string(target->cover_bot, sizeof(target->cover_bot),
		                        logical_bot_name ? logical_bot_name : "");
		merged_wall_copy_string(target->cover_ovl, sizeof(target->cover_ovl),
		                        logical_ovl_name ? logical_ovl_name : "");
	}

	debug_log(DLOG_TEXTURE,
	          "[mwall_cover_gpu] kind=%s shader=%s frame=%d pass=%d seq=%d face_pass=%d face_seq=%d face_order=%d cover_order=%d ordered=%d face_seg=%d face_side=%d face_face=%d face_child=%d face_wid=%d face_tmap1=%d face_tmap2=0x%x seg=%d side=%d face=%d child=%d wid=%d tmap1=%d name=%s handle=%u tex=%dx%d src_hash=0x%08x src_idx254=%d src_idx255=%d gpu_hash=0x%08x gpu_avg=%d/%d/%d/%d gpu_black=%d p0=%d/%d/%d/%d center=%d/%d/%d/%d overlap=%.1f face_box=%s",
	          kind ? kind : "",
	          shader_kind ? shader_kind : "",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          face_pass,
	          face_seq,
	          face_order,
	          draw_order,
	          ordered,
	          face_ctx && face_ctx->valid ? face_ctx->seg : -1,
	          face_ctx && face_ctx->valid ? face_ctx->side : -1,
	          face_ctx && face_ctx->valid ? face_ctx->face : -1,
	          face_ctx && face_ctx->valid ? face_ctx->child : -1,
	          face_ctx && face_ctx->valid ? face_ctx->wid_flags : 0,
	          face_ctx && face_ctx->valid ? face_ctx->tmap1 : -1,
	          face_ctx && face_ctx->valid ? face_ctx->tmap2 : 0,
	          cover_ctx->seg,
	          cover_ctx->side,
	          cover_ctx->face,
	          cover_ctx->child,
	          cover_ctx->wid_flags,
	          cover_ctx->tmap1,
	          name ? name : "<none>",
	          texture->handle,
	          read_w,
	          read_h,
	          src_hash,
	          src_idx254,
	          src_idx255,
	          gpu_hash,
	          avg_r,
	          avg_g,
	          avg_b,
	          avg_a,
	          black_rgb,
	          p0[0], p0[1], p0[2], p0[3],
	          center[0], center[1], center[2], center[3],
	          overlap_area,
	          track_box_kind ? track_box_kind : "exact");
}

static void merged_wall_log_live_cover_state(const char *kind,
                                             const char *shader_kind,
                                             int face_pass, int face_seq, int face_order,
                                             int draw_order, int ordered, float overlap_area,
                                             const char *track_box_kind,
                                             const struct android_draw_face_context *face_ctx,
                                             const struct android_draw_face_context *cover_ctx,
                                             grs_bitmap *cover_bitmap,
                                             const struct g3s_point **pointlist,
                                             const g3s_uvl *uvl_list,
                                             const float *color_array, int nv,
                                             int texfilt_level, int menu_texfilt,
                                             int hud_texfilt, int aniso_level,
                                             int is_debug_target)
{
	struct merged_wall_cached_texmerge_entry *cache_entry = NULL;
	grs_bitmap *src_bitmap = NULL;
	ogl_texture *texture;
	const char *name;
	GLint program = 0, active_tex = GL_TEXTURE0;
	GLint bound_tex0 = 0, bound_tex1 = 0, bound_tex2 = 0;
	GLint depth_enabled = 0, blend_enabled = 0, cull_enabled = 0, scissor_enabled = 0;
	GLboolean depth_writemask = GL_TRUE, color_mask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
	GLint depth_func = 0, front_face = 0, cull_mode = 0, draw_fbo = 0;
	GLint blend_src_rgb = 0, blend_dst_rgb = 0, blend_src_alpha = 0, blend_dst_alpha = 0;
	GLint blend_eq_rgb = 0, blend_eq_alpha = 0;
	GLint polygon_offset_enabled = 0;
	GLfloat polygon_offset_factor = 0.0f, polygon_offset_units = 0.0f;
	GLint viewport[4] = { 0, 0, 0, 0 };
	GLint scissor_box[4] = { 0, 0, 0, 0 };
	GLint tex_min = 0, tex_mag = 0, tex_wrap_s = 0, tex_wrap_t = 0;
	GLint tex_base_level = -1, tex_max_level = -1;
	float bbox_min_sx = 0.0f, bbox_max_sx = 0.0f;
	float bbox_min_sy = 0.0f, bbox_max_sy = 0.0f;
	float screen_w = 0.0f, screen_h = 0.0f;
	float uv_min_u = 0.0f, uv_max_u = 0.0f;
	float uv_min_v = 0.0f, uv_max_v = 0.0f;
	float texel_span_u = 0.0f, texel_span_v = 0.0f;
	float lod_u = -99.0f, lod_v = -99.0f, lod_max = -99.0f;
	int bbox_valid = 0, uv_valid = 0;

	if (!cover_ctx || !cover_ctx->valid)
		return;
	if (!cover_bitmap || !cover_bitmap->gltexture) {
		merged_wall_log_cover_bitmap_skip("missing_bitmap", kind, shader_kind,
		                                  cover_ctx, cover_bitmap);
		return;
	}
	if (!shader_kind || strcmp(shader_kind, "single")) {
		merged_wall_log_cover_bitmap_skip("non_single_shader", kind, shader_kind,
		                                  cover_ctx, cover_bitmap);
		return;
	}
	cache_entry = merged_wall_find_cached_texmerge_bitmap(cover_bitmap);
	if (cover_ctx->tmap2 != 0 && !cache_entry) {
		merged_wall_log_cover_bitmap_skip("cover_has_tmap2", kind, shader_kind,
		                                  cover_ctx, cover_bitmap);
		return;
	}

	texture = cover_bitmap->gltexture;
	name = piggy_game_bitmap_name(cover_bitmap);
	depth_enabled = glIsEnabled(GL_DEPTH_TEST);
	blend_enabled = glIsEnabled(GL_BLEND);
	cull_enabled = glIsEnabled(GL_CULL_FACE);
	scissor_enabled = glIsEnabled(GL_SCISSOR_TEST);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_writemask);
	glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
	glGetIntegerv(GL_CURRENT_PROGRAM, &program);
	glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
	glGetIntegerv(GL_FRONT_FACE, &front_face);
	glGetIntegerv(GL_CULL_FACE_MODE, &cull_mode);
	glGetIntegerv(GL_BLEND_SRC_RGB, &blend_src_rgb);
	glGetIntegerv(GL_BLEND_DST_RGB, &blend_dst_rgb);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src_alpha);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dst_alpha);
	glGetIntegerv(GL_BLEND_EQUATION_RGB, &blend_eq_rgb);
	glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blend_eq_alpha);
	glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &polygon_offset_factor);
	glGetFloatv(GL_POLYGON_OFFSET_UNITS, &polygon_offset_units);
	polygon_offset_enabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &draw_fbo);
	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetIntegerv(GL_SCISSOR_BOX, scissor_box);
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active_tex);
	glActiveTexture(GL_TEXTURE0);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound_tex0);
	if (bound_tex0 > 0) {
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &tex_min);
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &tex_mag);
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &tex_wrap_s);
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &tex_wrap_t);
#ifdef GL_TEXTURE_BASE_LEVEL
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, &tex_base_level);
#endif
#ifdef GL_TEXTURE_MAX_LEVEL
		glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, &tex_max_level);
#endif
	}
	glActiveTexture(GL_TEXTURE1);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound_tex1);
	glActiveTexture(GL_TEXTURE2);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound_tex2);
	glActiveTexture((GLenum) active_tex);
	if (cache_entry) {
		const char *cache_bottom_name =
		    piggy_game_bitmap_name(cache_entry->bottom_bmp);
		const char *cache_top_name =
		    piggy_game_bitmap_name(cache_entry->top_bmp);
		GLuint cache_bottom_handle =
		    cache_entry->bottom_bmp && cache_entry->bottom_bmp->gltexture
		        ? cache_entry->bottom_bmp->gltexture->handle
		        : 0;
		GLuint cache_top_handle =
		    cache_entry->top_bmp && cache_entry->top_bmp->gltexture
		        ? cache_entry->top_bmp->gltexture->handle
		        : 0;

		debug_log(DLOG_TEXTURE,
		          "[mwall_cache_live] kind=%s shader=%s frame=%d pass=%d seq=%d face_pass=%d face_seq=%d face_order=%d cover_order=%d ordered=%d seg=%d side=%d face=%d child=%d wid=%d tmap1=%d tmap2=0x%x slot=%d orient=%d output=%u tex0=%d tex1=%d tex2=%d expected_base=%u expected_ovl=%u base=%s ovl=%s out_min=0x%x out_mag=0x%x out_wrap=0x%x/0x%x out_mip=%d size=%dx%d overlap=%.1f face_box=%s",
		          kind ? kind : "",
		          shader_kind ? shader_kind : "",
		          g_merged_wall_frame_id,
		          g_merged_wall_render_pass,
		          g_merged_wall_draw_seq,
		          face_pass,
		          face_seq,
		          face_order,
		          draw_order,
		          ordered,
		          cover_ctx->seg,
		          cover_ctx->side,
		          cover_ctx->face,
		          cover_ctx->child,
		          cover_ctx->wid_flags,
		          cover_ctx->tmap1,
		          cover_ctx->tmap2,
		          cache_entry->slot,
		          cache_entry->orient,
		          texture->handle,
		          bound_tex0,
		          bound_tex1,
		          bound_tex2,
		          cache_bottom_handle,
		          cache_top_handle,
		          cache_bottom_name ? cache_bottom_name : "<none>",
		          cache_top_name ? cache_top_name : "<none>",
		          tex_min,
		          tex_mag,
		          tex_wrap_s,
		          tex_wrap_t,
		          texture->has_mipmaps,
		          texture->w,
		          texture->h,
		          overlap_area,
		          track_box_kind ? track_box_kind : "exact");
	}
	bbox_valid = merged_wall_get_screen_bbox(pointlist, nv,
	                                         &bbox_min_sx, &bbox_max_sx,
	                                         &bbox_min_sy, &bbox_max_sy);
	if (bbox_valid) {
		screen_w = bbox_max_sx - bbox_min_sx;
		screen_h = bbox_max_sy - bbox_min_sy;
	}
	uv_valid = merged_wall_get_uv_bbox(uvl_list, nv,
	                                   &uv_min_u, &uv_max_u,
	                                   &uv_min_v, &uv_max_v);
	src_bitmap = cache_entry && cache_entry->bottom_bmp
	                 ? merged_wall_get_source_bitmap(cache_entry->bottom_bmp)
	                 : merged_wall_get_source_bitmap(cover_bitmap);
	if (!src_bitmap && cache_entry)
		src_bitmap = cache_entry->bottom_bmp;
	if (src_bitmap && uv_valid) {
		texel_span_u = fabsf(uv_max_u - uv_min_u) * src_bitmap->bm_w;
		texel_span_v = fabsf(uv_max_v - uv_min_v) * src_bitmap->bm_h;
		lod_u = merged_wall_log2_ratio(texel_span_u, screen_w);
		lod_v = merged_wall_log2_ratio(texel_span_v, screen_h);
		lod_max = lod_u > lod_v ? lod_u : lod_v;
	}

	debug_log(DLOG_TEXTURE,
	          "[mwall_cover_live] kind=%s shader=%s frame=%d pass=%d seq=%d face_pass=%d face_seq=%d face_order=%d cover_order=%d ordered=%d face_seg=%d face_side=%d face_face=%d face_child=%d face_wid=%d face_tmap1=%d face_tmap2=0x%x seg=%d side=%d face=%d child=%d wid=%d tmap1=%d name=%s expected=%u program=%d active_tex=0x%x tex0=%d tex1=%d tex2=%d tex_min=0x%x tex_mag=0x%x wrap_s=0x%x wrap_t=0x%x base=%d max=%d depth=%d blend=%d cull=%d scissor=%d depth_mask=%d depth_func=0x%x blend_src_rgb=0x%x blend_dst_rgb=0x%x blend_src_a=0x%x blend_dst_a=0x%x blend_eq_rgb=0x%x blend_eq_a=0x%x front_face=0x%x cull_mode=0x%x poly=%d poly_factor=%.3f poly_units=%.3f color_mask=%d%d%d%d fbo=%d viewport=%d,%d,%d,%d scissor_box=%d,%d,%d,%d overlap=%.1f face_box=%s",
	          kind ? kind : "",
	          shader_kind ? shader_kind : "",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          face_pass,
	          face_seq,
	          face_order,
	          draw_order,
	          ordered,
	          face_ctx && face_ctx->valid ? face_ctx->seg : -1,
	          face_ctx && face_ctx->valid ? face_ctx->side : -1,
	          face_ctx && face_ctx->valid ? face_ctx->face : -1,
	          face_ctx && face_ctx->valid ? face_ctx->child : -1,
	          face_ctx && face_ctx->valid ? face_ctx->wid_flags : 0,
	          face_ctx && face_ctx->valid ? face_ctx->tmap1 : -1,
	          face_ctx && face_ctx->valid ? face_ctx->tmap2 : 0,
	          cover_ctx->seg,
	          cover_ctx->side,
	          cover_ctx->face,
	          cover_ctx->child,
	          cover_ctx->wid_flags,
	          cover_ctx->tmap1,
	          name ? name : "<none>",
	          texture->handle,
	          program,
	          active_tex,
	          bound_tex0,
	          bound_tex1,
	          bound_tex2,
	          tex_min,
	          tex_mag,
	          tex_wrap_s,
	          tex_wrap_t,
	          tex_base_level,
	          tex_max_level,
	          depth_enabled,
	          blend_enabled,
	          cull_enabled,
	          scissor_enabled,
	          depth_writemask,
	          depth_func,
	          blend_src_rgb,
	          blend_dst_rgb,
	          blend_src_alpha,
	          blend_dst_alpha,
	          blend_eq_rgb,
	          blend_eq_alpha,
	          front_face,
	          cull_mode,
	          polygon_offset_enabled,
	          polygon_offset_factor,
	          polygon_offset_units,
	          color_mask[0],
	          color_mask[1],
	          color_mask[2],
	          color_mask[3],
	          draw_fbo,
	          viewport[0],
	          viewport[1],
	          viewport[2],
	          viewport[3],
	          scissor_box[0],
	          scissor_box[1],
	          scissor_box[2],
	          scissor_box[3],
	          overlap_area,
	          track_box_kind ? track_box_kind : "exact");
	debug_log(DLOG_TEXTURE,
	          "[mwall_cover_lod] kind=%s shader=%s face_pass=%d face_seq=%d face_order=%d face_seg=%d face_side=%d face_face=%d face_child=%d face_wid=%d face_tmap1=%d face_tmap2=0x%x seg=%d side=%d face=%d child=%d wid=%d tmap1=%d name=%s has_mips=%d cfg_texfilt=%d cfg_menu_texfilt=%d cfg_hud_texfilt=%d aniso=%d bbox_valid=%d uv_valid=%d screen=%.1fx%.1f uv=%.3f..%.3f/%.3f..%.3f texels=%.1fx%.1f lod_u=%.2f lod_v=%.2f lod_max=%.2f overlap=%.1f face_box=%s",
	          kind ? kind : "",
	          shader_kind ? shader_kind : "",
	          face_pass,
	          face_seq,
	          face_order,
	          face_ctx && face_ctx->valid ? face_ctx->seg : -1,
	          face_ctx && face_ctx->valid ? face_ctx->side : -1,
	          face_ctx && face_ctx->valid ? face_ctx->face : -1,
	          face_ctx && face_ctx->valid ? face_ctx->child : -1,
	          face_ctx && face_ctx->valid ? face_ctx->wid_flags : 0,
	          face_ctx && face_ctx->valid ? face_ctx->tmap1 : -1,
	          face_ctx && face_ctx->valid ? face_ctx->tmap2 : 0,
	          cover_ctx->seg,
	          cover_ctx->side,
	          cover_ctx->face,
	          cover_ctx->child,
	          cover_ctx->wid_flags,
	          cover_ctx->tmap1,
	          name ? name : "<none>",
	          texture ? texture->has_mipmaps : -1,
	          texfilt_level,
	          menu_texfilt,
	          hud_texfilt,
	          aniso_level,
	          bbox_valid,
	          uv_valid,
	          screen_w,
	          screen_h,
	          uv_min_u,
	          uv_max_u,
	          uv_min_v,
	          uv_max_v,
	          texel_span_u,
	          texel_span_v,
	          lod_u,
	          lod_v,
	          lod_max,
	          overlap_area,
	          track_box_kind ? track_box_kind : "exact");
	merged_wall_log_cover_light_state(kind, shader_kind,
	                                  face_pass, face_seq, face_order,
	                                  draw_order, ordered, overlap_area,
	                                  track_box_kind, face_ctx, cover_ctx,
	                                  cover_bitmap, color_array, nv);
	merged_wall_log_cover_gpu_readback(kind, shader_kind,
	                                   face_pass, face_seq, face_order,
	                                   draw_order, ordered, overlap_area,
	                                   track_box_kind, face_ctx, cover_ctx,
	                                   cover_bitmap, is_debug_target);
	merged_wall_log_cover_bitmap_dump(kind, shader_kind, overlap_area,
	                                  track_box_kind, cover_ctx, cover_bitmap);
}

static void merged_wall_log_snapshot_pose(void)
{
	vms_angvec angles = { 0, 0, 0 };
	int seg = -1;
	float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;

	if (ConsoleObject) {
		seg = ConsoleObject->segnum;
		pos_x = f2fl(ConsoleObject->pos.x);
		pos_y = f2fl(ConsoleObject->pos.y);
		pos_z = f2fl(ConsoleObject->pos.z);
		vm_extract_angles_matrix(&angles, &ConsoleObject->orient);
	}

	debug_log_force(DLOG_TEXTURE,
	                "[mwall_snap_pose] replay={\"action\":\"pose_view\",\"segment\":%d,\"x\":%.6f,\"y\":%.6f,\"z\":%.6f,\"pitch\":%d,\"bank\":%d,\"heading\":%d}",
	                seg,
	                pos_x,
	                pos_y,
	                pos_z,
	                (int) angles.p,
	                (int) angles.b,
	                (int) angles.h);
}

static void merged_wall_log_snapshot_last_draw(void)
{
	if (!g_merged_wall_last_draw_state.valid)
		return;

	debug_log(DLOG_TEXTURE,
	          "[mwall_snap_last] frame=%d pass=%d seq=%d seg=%d side=%d face=%d child=%d wid=%d tmap1=%d tmap2=0x%x route=%s merge_impl=%s depth=%d blend=%d cull=%d poly_off=%d poly_factor=%.3f poly_units=%.3f depth_mask=%d depth_func=%d front_face=%d cull_mode=%d fbo=%d area=%.1f",
	          g_merged_wall_last_draw_state.frame_id,
	          g_merged_wall_last_draw_state.render_pass,
	          g_merged_wall_last_draw_state.draw_seq,
	          g_merged_wall_last_draw_state.seg,
	          g_merged_wall_last_draw_state.side,
	          g_merged_wall_last_draw_state.face,
	          g_merged_wall_last_draw_state.child,
	          g_merged_wall_last_draw_state.wid_flags,
	          g_merged_wall_last_draw_state.tmap1,
	          g_merged_wall_last_draw_state.tmap2,
	          g_merged_wall_last_draw_state.route,
	          g_merged_wall_last_draw_state.merge_impl,
	          g_merged_wall_last_draw_state.depth_enabled,
	          g_merged_wall_last_draw_state.blend_enabled,
	          g_merged_wall_last_draw_state.cull_enabled,
	          g_merged_wall_last_draw_state.polygon_offset_enabled,
	          g_merged_wall_last_draw_state.polygon_offset_factor,
	          g_merged_wall_last_draw_state.polygon_offset_units,
	          g_merged_wall_last_draw_state.depth_writemask,
	          g_merged_wall_last_draw_state.depth_func,
	          g_merged_wall_last_draw_state.front_face,
	          g_merged_wall_last_draw_state.cull_mode,
	          g_merged_wall_last_draw_state.draw_fbo,
	          g_merged_wall_last_draw_state.screen_area);
}

static void merged_wall_log_portal(const char *tag,
                                   const struct android_draw_face_context *ctx)
{
	struct merged_wall_portal_debug_info info;

	if (!ctx || !ctx->valid)
		return;
	merged_wall_fill_portal_debug_info(ctx, &info);
	if (info.wall_num < 0 && info.conn_side < 0)
		return;

	debug_log(DLOG_TEXTURE,
	          "[mwall_portal] tag=%s seg=%d side=%d face=%d child=%d wid=%d tmap1=%d tmap2=0x%x wall_num=%d wall_type=%d wall_state=%d wall_flags=0x%x wall_clip=%d wall_keys=%d conn_seg=%d conn_side=%d conn_child=%d conn_side_type=%d conn_wid=%d conn_tmap1=%d conn_tmap2=0x%x conn_wall_num=%d conn_wall_type=%d conn_wall_state=%d conn_wall_flags=0x%x conn_wall_clip=%d conn_wall_keys=%d",
	          tag ? tag : "",
	          ctx->seg,
	          ctx->side,
	          ctx->face,
	          ctx->child,
	          ctx->wid_flags,
	          ctx->tmap1,
	          ctx->tmap2,
	          info.wall_num,
	          info.wall_type,
	          info.wall_state,
	          info.wall_flags,
	          info.wall_clip,
	          info.wall_keys,
	          info.conn_seg,
	          info.conn_side,
	          info.conn_child,
	          info.conn_side_type,
	          info.conn_wid,
	          info.conn_tmap1,
	          info.conn_tmap2,
	          info.conn_wall_num,
	          info.conn_wall_type,
	          info.conn_wall_state,
	          info.conn_wall_flags,
	          info.conn_wall_clip,
	          info.conn_wall_keys);
}

void android_merged_wall_log_face(struct segment *segp, int sidenum, int tmap1,
                                  int tmap2, int wid_flags, float dot, int nv, int face_index)
{
	int overlay = merged_wall_overlay_index(tmap2);
	grs_bitmap *bmbot, *bmovl;
	const char *botname, *ovlname;
	const char *merge_path;
	int bot_real_flags, ovl_real_flags;
	int wall_num = segp->sides[sidenum].wall_num;
	int wall_type = -1, wall_state = -1, wall_flags = 0;

	if (!android_merged_wall_is_logging_target_tmap2(tmap2) || tmap1 >= NumTextures || overlay >= NumTextures)
		return;
	bmbot = &GameBitmaps[Textures[tmap1].index];
	bmovl = &GameBitmaps[Textures[overlay].index];
	botname = piggy_game_bitmap_name(bmbot);
	ovlname = piggy_game_bitmap_name(bmovl);
	bot_real_flags = piggy_bitmap_get_flags(bmbot);
	ovl_real_flags = piggy_bitmap_get_flags(bmovl);
	if (wall_num >= 0) {
		wall_type = Walls[wall_num].type;
		wall_state = Walls[wall_num].state;
		wall_flags = Walls[wall_num].flags;
	}
	if (ovl_real_flags & BM_FLAG_SUPER_TRANSPARENT)
		merge_path = "super_mask";
	else if (ovl_real_flags & BM_FLAG_TRANSPARENT)
		merge_path = "plain_alpha_underlay";
	else
		merge_path = "opaque_overlay";
	debug_log(DLOG_TEXTURE,
	          "[mwall_face] frame=%d pass=%d seq=%d seg=%d side=%d face=%d child=%d side_type=%d nv=%d wid=%d dot=%.4f tmap1=%d tmap2=0x%x orient=%d bot=%s ovl=%s",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          (int) (segp - Segments),
	          sidenum,
	          face_index,
	          segp->children[sidenum],
	          segp->sides[sidenum].type,
	          nv,
	          wid_flags,
	          dot,
	          tmap1,
	          tmap2,
	          (tmap2 >> 14) & 3,
	          botname ? botname : "<none>",
	          ovlname ? ovlname : "<none>");
	debug_log(DLOG_TEXTURE,
	          "[mwall_wall] frame=%d pass=%d seq=%d wid=%d render=%d rendpast=%d fly=%d child=%d wall_num=%d wall_type=%d wall_state=%d wall_flags=0x%x bot_real=0x%x ovl_real=0x%x ovl_trans=%d ovl_super=%d path=%s",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          wid_flags,
	          (wid_flags & WID_RENDER_FLAG) != 0,
	          (wid_flags & WID_RENDPAST_FLAG) != 0,
	          (wid_flags & WID_FLY_FLAG) != 0,
	          segp->children[sidenum],
	          wall_num,
	          wall_type,
	          wall_state,
	          wall_flags,
	          bot_real_flags,
	          ovl_real_flags,
	          !!(ovl_real_flags & BM_FLAG_TRANSPARENT),
	          !!(ovl_real_flags & BM_FLAG_SUPER_TRANSPARENT),
	          merge_path);
}

int android_merged_wall_next_draw_order(void)
{
	merged_wall_begin_frame_tracking();
	return ++merged_wall_draw_order;
}

void android_merged_wall_track_face(const struct g3s_point **pointlist, int nv,
                                    const g3s_uvl *uvl_list, int orient,
                                    int draw_order, const char *route, const char *merge_impl,
                                    const char *decision_reason, grs_bitmap *merged_bitmap,
                                    int merged_slot)
{
	struct merged_wall_tracked_face *track;
	int i, projected = 0;

	if (!g_android_draw_face_ctx.valid || g_android_draw_face_ctx.tmap2 == 0)
		return;
	if (nv <= 0 || nv > MERGED_WALL_LOG_PT_COUNT || merged_wall_tracked_face_count >= MERGED_WALL_TRACKED_FACE_MAX)
		return;

	merged_wall_begin_frame_tracking();
	track = &merged_wall_tracked_faces[merged_wall_tracked_face_count++];
	memset(track, 0, sizeof(*track));
	track->render_pass = g_merged_wall_render_pass;
	track->draw_seq = g_merged_wall_draw_seq;
	track->draw_order = draw_order;
	track->nv = nv;
	track->draw_ctx = g_android_draw_face_ctx;
	track->bbox_valid = merged_wall_get_screen_bbox(pointlist, nv,
	                                                &track->min_sx, &track->max_sx, &track->min_sy, &track->max_sy);
	track->bbox_area = track->bbox_valid
	                       ? (track->max_sx - track->min_sx) * (track->max_sy - track->min_sy)
	                       : 0.0f;
	track->projected_bbox_valid = merged_wall_get_projected_bbox(pointlist, nv,
	                                                             &track->projected_count,
	                                                             &track->projected_min_sx,
	                                                             &track->projected_max_sx,
	                                                             &track->projected_min_sy,
	                                                             &track->projected_max_sy);
	track->projected_bbox_area = track->projected_bbox_valid
	                                 ? (track->projected_max_sx - track->projected_min_sx) *
	                                       (track->projected_max_sy - track->projected_min_sy)
	                                 : 0.0f;
	merged_wall_track_split_geometry(track, pointlist, nv);
	merged_wall_copy_string(track->route, sizeof(track->route), route);
	merged_wall_copy_string(track->merge_impl, sizeof(track->merge_impl), merge_impl);
	merged_wall_copy_string(track->decision_reason, sizeof(track->decision_reason), decision_reason);
	track->orient = orient;
	track->merged_bitmap = merged_bitmap;
	track->merged_slot = merged_slot;
	track->projected_count = merged_wall_store_projected_points(pointlist, nv,
	                                                            MERGED_WALL_LOG_PT_COUNT,
	                                                            track->pt_projected,
	                                                            track->pt_sx,
	                                                            track->pt_sy);
	for (i = 0; i < nv; i++) {
		track->pts[i][0] = pointlist[i]->p3_vec.x;
		track->pts[i][1] = pointlist[i]->p3_vec.y;
		track->pts[i][2] = pointlist[i]->p3_vec.z;
		if (uvl_list)
			track->uvl[i] = uvl_list[i];
		else
			memset(&track->uvl[i], 0, sizeof(track->uvl[i]));
	}
	projected = track->projected_count;
	debug_log(DLOG_TEXTURE,
	          "[mwall_track] frame=%d pass=%d seq=%d order=%d seg=%d side=%d face=%d child=%d nv=%d projected=%d bbox_valid=%d proj_box_valid=%d box=%.1f..%.1f/%.1f..%.1f area=%.1f proj_box=%.1f..%.1f/%.1f..%.1f proj_area=%.1f route=%s merge_impl=%s orient=%d merged=%p slot=%d",
	          g_merged_wall_frame_id,
	          g_merged_wall_render_pass,
	          g_merged_wall_draw_seq,
	          draw_order,
	          track->draw_ctx.valid ? track->draw_ctx.seg : -1,
	          track->draw_ctx.valid ? track->draw_ctx.side : -1,
	          track->draw_ctx.valid ? track->draw_ctx.face : -1,
	          track->draw_ctx.valid ? track->draw_ctx.child : -1,
	          nv,
	          projected,
	          track->bbox_valid,
	          track->projected_bbox_valid,
	          track->min_sx,
	          track->max_sx,
	          track->min_sy,
	          track->max_sy,
	          track->bbox_area,
	          track->projected_min_sx,
	          track->projected_max_sx,
	          track->projected_min_sy,
	          track->projected_max_sy,
	          track->projected_bbox_area,
	          track->route,
	          track->merge_impl,
	          track->orient,
	          (void *) track->merged_bitmap,
	          track->merged_slot);
}

void android_merged_wall_log_cover(const char *shader_kind, const char *botname,
                                   const char *ovlname, const struct g3s_point **pointlist, int nv,
                                   const g3s_uvl *uvl_list, const float *color_array,
                                   int draw_order, grs_bitmap *cover_bitmap,
                                   int texfilt_level, int menu_texfilt, int hud_texfilt,
                                   int aniso_level)
{
	struct android_draw_face_context cover_ctx = g_android_draw_face_ctx;
	float cover_min_sx = 0.0f, cover_max_sx = 0.0f;
	float cover_min_sy = 0.0f, cover_max_sy = 0.0f;
	float cover_bbox_area = 0.0f;
	int cover_projected_count = 0;
	int cover_bbox_valid = 0;
	int cover_pt_projected[MERGED_WALL_LOG_PT_COUNT] = { 0 };
	float cover_pt_sx[MERGED_WALL_LOG_PT_COUNT] = { 0.0f };
	float cover_pt_sy[MERGED_WALL_LOG_PT_COUNT] = { 0.0f };
	int i, ordered;

	merged_wall_begin_frame_tracking();
	if (nv <= 0 || nv > MERGED_WALL_LOG_PT_COUNT)
		return;
	cover_projected_count = merged_wall_store_projected_points(pointlist, nv,
	                                                           MERGED_WALL_LOG_PT_COUNT,
	                                                           cover_pt_projected,
	                                                           cover_pt_sx,
	                                                           cover_pt_sy);
	cover_bbox_valid = merged_wall_get_screen_bbox(pointlist, nv,
	                                               &cover_min_sx, &cover_max_sx, &cover_min_sy, &cover_max_sy);
	if (cover_bbox_valid)
		cover_bbox_area = (cover_max_sx - cover_min_sx) * (cover_max_sy - cover_min_sy);
	for (i = 0; i < merged_wall_tracked_face_count; i++) {
		struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[i];
		int is_debug_target;

		if (track->cover_logged || draw_order <= track->draw_order)
			continue;
		if (!merged_wall_face_matches(track, pointlist, nv, &ordered))
			continue;

		track->cover_logged = 1;
		if (g_merged_wall_snapshot_pending) {
			struct merged_wall_snapshot_cover_event *event;
			float overlap_area = cover_bbox_valid && track->bbox_valid
			                         ? merged_wall_bbox_overlap_area(track->min_sx, track->max_sx,
			                                                         track->min_sy, track->max_sy,
			                                                         cover_min_sx, cover_max_sx,
			                                                         cover_min_sy, cover_max_sy)
			                         : 0.0f;

			merged_wall_begin_cover_events();
			if (merged_wall_cover_event_count < MERGED_WALL_COVER_EVENT_MAX) {
				event = &merged_wall_cover_events[merged_wall_cover_event_count++];
				memset(event, 0, sizeof(*event));
				event->kind = MERGED_WALL_SNAPSHOT_COVER_EXACT;
				event->face_index = i;
				event->cover_order = draw_order;
				event->ordered = ordered;
				event->cover_ctx = cover_ctx;
				event->cover_nv = nv;
				event->cover_projected_count = cover_projected_count;
				event->cover_bbox_valid = cover_bbox_valid;
				event->cover_min_sx = cover_min_sx;
				event->cover_max_sx = cover_max_sx;
				event->cover_min_sy = cover_min_sy;
				event->cover_max_sy = cover_max_sy;
				memcpy(event->cover_pt_projected, cover_pt_projected, sizeof(event->cover_pt_projected));
				memcpy(event->cover_pt_sx, cover_pt_sx, sizeof(event->cover_pt_sx));
				memcpy(event->cover_pt_sy, cover_pt_sy, sizeof(event->cover_pt_sy));
				event->overlap_area = overlap_area;
				merged_wall_copy_string(event->cover_shader,
				                        sizeof(event->cover_shader), shader_kind);
				merged_wall_copy_string(event->cover_bot,
				                        sizeof(event->cover_bot), botname);
				merged_wall_copy_string(event->cover_ovl,
				                        sizeof(event->cover_ovl), ovlname);
			}
			is_debug_target = merged_wall_cover_is_debug_target(track, cover_bitmap,
			                                                    nv,
			                                                    cover_pt_projected,
			                                                    cover_pt_sx,
			                                                    cover_pt_sy,
			                                                    cover_projected_count,
			                                                    cover_bbox_valid,
			                                                    cover_min_sx,
			                                                    cover_max_sx,
			                                                    cover_min_sy,
			                                                    cover_max_sy,
			                                                    overlap_area);
			merged_wall_log_live_cover_state("exact", shader_kind,
			                                 track->render_pass, track->draw_seq, track->draw_order,
			                                 draw_order,
			                                 ordered, overlap_area, "exact", &track->draw_ctx, &cover_ctx, cover_bitmap,
			                                 pointlist, uvl_list, color_array, nv, texfilt_level,
			                                 menu_texfilt, hud_texfilt, aniso_level,
			                                 is_debug_target);
		}
		debug_log(DLOG_TEXTURE,
		          "[mwall_cover] frame=%d face_pass=%d face_seq=%d face_order=%d cover_order=%d cover_shader=%s cover_bot=%s cover_ovl=%s ordered=%d seg=%d side=%d face=%d child=%d side_type=%d wid=%d tmap1=%d tmap2=0x%x cover_seg=%d cover_side=%d cover_face=%d cover_child=%d cover_side_type=%d cover_wid=%d cover_tmap1=%d cover_tmap2=0x%x",
		          g_merged_wall_frame_id,
		          track->render_pass,
		          track->draw_seq,
		          track->draw_order,
		          draw_order,
		          shader_kind ? shader_kind : "",
		          botname ? botname : "",
		          ovlname ? ovlname : "",
		          ordered,
		          track->draw_ctx.valid ? track->draw_ctx.seg : -1,
		          track->draw_ctx.valid ? track->draw_ctx.side : -1,
		          track->draw_ctx.valid ? track->draw_ctx.face : -1,
		          track->draw_ctx.valid ? track->draw_ctx.child : -1,
		          track->draw_ctx.valid ? track->draw_ctx.side_type : -1,
		          track->draw_ctx.valid ? track->draw_ctx.wid_flags : 0,
		          track->draw_ctx.valid ? track->draw_ctx.tmap1 : -1,
		          track->draw_ctx.valid ? track->draw_ctx.tmap2 : 0,
		          cover_ctx.valid ? cover_ctx.seg : -1,
		          cover_ctx.valid ? cover_ctx.side : -1,
		          cover_ctx.valid ? cover_ctx.face : -1,
		          cover_ctx.valid ? cover_ctx.child : -1,
		          cover_ctx.valid ? cover_ctx.side_type : -1,
		          cover_ctx.valid ? cover_ctx.wid_flags : 0,
		          cover_ctx.valid ? cover_ctx.tmap1 : -1,
		          cover_ctx.valid ? cover_ctx.tmap2 : 0);
		merged_wall_log_portal("face", &track->draw_ctx);
		merged_wall_log_portal("cover", &cover_ctx);
		return;
	}

	if (!cover_bbox_valid || cover_bbox_area <= 0.0f)
		return;

	for (i = 0; i < merged_wall_tracked_face_count; i++) {
		float overlap_area;
		float track_min_sx = 0.0f, track_max_sx = 0.0f;
		float track_min_sy = 0.0f, track_max_sy = 0.0f;
		float track_bbox_area = 0.0f;
		const char *track_box_kind = "none";
		struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[i];

		if (track->coverbox_logged || draw_order <= track->draw_order)
			continue;
		if (!merged_wall_get_track_overlap_box(track,
		                                       &track_min_sx, &track_max_sx,
		                                       &track_min_sy, &track_max_sy,
		                                       &track_bbox_area, &track_box_kind))
			continue;

		overlap_area = merged_wall_bbox_overlap_area(track_min_sx, track_max_sx,
		                                             track_min_sy, track_max_sy,
		                                             cover_min_sx, cover_max_sx,
		                                             cover_min_sy, cover_max_sy);
		if (overlap_area < 64.0f)
			continue;
		if (overlap_area < track_bbox_area * 0.20f && overlap_area < cover_bbox_area * 0.20f)
			continue;

		track->coverbox_logged = 1;
		if (g_merged_wall_snapshot_pending) {
			struct merged_wall_snapshot_cover_event *event;
			int is_debug_target;

			merged_wall_begin_cover_events();
			if (merged_wall_cover_event_count < MERGED_WALL_COVER_EVENT_MAX) {
				event = &merged_wall_cover_events[merged_wall_cover_event_count++];
				memset(event, 0, sizeof(*event));
				event->kind = MERGED_WALL_SNAPSHOT_COVER_BBOX;
				event->face_index = i;
				event->cover_order = draw_order;
				event->cover_ctx = cover_ctx;
				event->cover_nv = nv;
				event->cover_projected_count = cover_projected_count;
				event->cover_bbox_valid = cover_bbox_valid;
				event->cover_min_sx = cover_min_sx;
				event->cover_max_sx = cover_max_sx;
				event->cover_min_sy = cover_min_sy;
				event->cover_max_sy = cover_max_sy;
				memcpy(event->cover_pt_projected, cover_pt_projected, sizeof(event->cover_pt_projected));
				memcpy(event->cover_pt_sx, cover_pt_sx, sizeof(event->cover_pt_sx));
				memcpy(event->cover_pt_sy, cover_pt_sy, sizeof(event->cover_pt_sy));
				event->overlap_area = overlap_area;
				merged_wall_copy_string(event->cover_shader,
				                        sizeof(event->cover_shader), shader_kind);
				merged_wall_copy_string(event->cover_bot,
				                        sizeof(event->cover_bot), botname);
				merged_wall_copy_string(event->cover_ovl,
				                        sizeof(event->cover_ovl), ovlname);
			}
			is_debug_target = merged_wall_cover_is_debug_target(track, cover_bitmap,
			                                                    nv,
			                                                    cover_pt_projected,
			                                                    cover_pt_sx,
			                                                    cover_pt_sy,
			                                                    cover_projected_count,
			                                                    cover_bbox_valid,
			                                                    cover_min_sx,
			                                                    cover_max_sx,
			                                                    cover_min_sy,
			                                                    cover_max_sy,
			                                                    overlap_area);

			merged_wall_log_face_textures("snapshot_coverbox_cover", 0, &cover_ctx);
			merged_wall_log_live_cover_state("bbox", shader_kind,
			                                 track->render_pass, track->draw_seq, track->draw_order,
			                                 draw_order,
			                                 -1, overlap_area, track_box_kind, &track->draw_ctx, &cover_ctx, cover_bitmap,
			                                 pointlist, uvl_list, color_array, nv, texfilt_level,
			                                 menu_texfilt, hud_texfilt, aniso_level,
			                                 is_debug_target);
		}
		debug_log(DLOG_TEXTURE,
		          "[mwall_coverbox] frame=%d face_pass=%d face_seq=%d face_order=%d cover_order=%d cover_shader=%s cover_bot=%s cover_ovl=%s seg=%d side=%d face=%d child=%d side_type=%d wid=%d tmap1=%d tmap2=0x%x cover_seg=%d cover_side=%d cover_face=%d cover_child=%d cover_side_type=%d cover_wid=%d cover_tmap1=%d cover_tmap2=0x%x face_box=%s overlap=%.1f",
		          g_merged_wall_frame_id,
		          track->render_pass,
		          track->draw_seq,
		          track->draw_order,
		          draw_order,
		          shader_kind ? shader_kind : "",
		          botname ? botname : "",
		          ovlname ? ovlname : "",
		          track->draw_ctx.valid ? track->draw_ctx.seg : -1,
		          track->draw_ctx.valid ? track->draw_ctx.side : -1,
		          track->draw_ctx.valid ? track->draw_ctx.face : -1,
		          track->draw_ctx.valid ? track->draw_ctx.child : -1,
		          track->draw_ctx.valid ? track->draw_ctx.side_type : -1,
		          track->draw_ctx.valid ? track->draw_ctx.wid_flags : 0,
		          track->draw_ctx.valid ? track->draw_ctx.tmap1 : -1,
		          track->draw_ctx.valid ? track->draw_ctx.tmap2 : 0,
		          cover_ctx.valid ? cover_ctx.seg : -1,
		          cover_ctx.valid ? cover_ctx.side : -1,
		          cover_ctx.valid ? cover_ctx.face : -1,
		          cover_ctx.valid ? cover_ctx.child : -1,
		          cover_ctx.valid ? cover_ctx.side_type : -1,
		          cover_ctx.valid ? cover_ctx.wid_flags : 0,
		          cover_ctx.valid ? cover_ctx.tmap1 : -1,
		          cover_ctx.valid ? cover_ctx.tmap2 : 0,
		          track_box_kind,
		          overlap_area);
		merged_wall_log_portal("face", &track->draw_ctx);
		merged_wall_log_portal("cover", &cover_ctx);
		return;
	}
}

static int merged_wall_face_is_selected(const int *selected, int selected_count,
                                        int track_index)
{
	int i;

	for (i = 0; i < selected_count; i++)
		if (selected[i] == track_index)
			return 1;
	return 0;
}

static int merged_wall_selected_rank(const int *selected, int selected_count,
                                     int track_index)
{
	int i;

	for (i = 0; i < selected_count; i++)
		if (selected[i] == track_index)
			return i;
	return -1;
}

static void merged_wall_log_snapshot_post_focus_overdraw(int focus_rank,
                                                         float center_x, float center_y,
                                                         float canvas_center_x, float canvas_center_y,
                                                         const struct merged_wall_snapshot_focus_cover *candidate)
{
	const struct merged_wall_snapshot_cover_event *focus_event;
	int face_hits = 0;
	int face_logged = 0;
	int cover_hits = 0;
	int cover_logged = 0;
	int i;

	if (!candidate || candidate->event_index < 0 || candidate->event_index >= merged_wall_cover_event_count)
		return;
	focus_event = &merged_wall_cover_events[candidate->event_index];

	for (i = 0; i < merged_wall_tracked_face_count; i++) {
		const struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[i];
		float track_min_sx = 0.0f, track_max_sx = 0.0f;
		float track_min_sy = 0.0f, track_max_sy = 0.0f;
		float track_bbox_area = 0.0f;
		float dist2 = -1.0f;
		const char *track_box_kind = "none";
		int have_track_box;
		int center_face = 0;
		int canvas_face = 0;
		int face_poly_valid;
		int face_poly_hit = 0;
		int canvas_face_poly_hit = 0;

		if (track->draw_order <= focus_event->cover_order)
			continue;
		have_track_box = merged_wall_get_track_overlap_box(track,
		                                                   &track_min_sx, &track_max_sx,
		                                                   &track_min_sy, &track_max_sy,
		                                                   &track_bbox_area, &track_box_kind);
		if (have_track_box) {
			center_face = merged_wall_bbox_contains_point(center_x, center_y,
			                                              track_min_sx, track_max_sx,
			                                              track_min_sy, track_max_sy);
			canvas_face = merged_wall_bbox_contains_point(canvas_center_x, canvas_center_y,
			                                              track_min_sx, track_max_sx,
			                                              track_min_sy, track_max_sy);
			dist2 = merged_wall_bbox_distance_sq(center_x, center_y,
			                                     track_min_sx, track_max_sx,
			                                     track_min_sy, track_max_sy);
		}
		face_poly_valid = merged_wall_projected_polygon_valid(track->nv, track->projected_count);
		if (face_poly_valid) {
			face_poly_hit = merged_wall_projected_polygon_contains_point(track->nv,
			                                                             track->pt_projected,
			                                                             track->pt_sx,
			                                                             track->pt_sy,
			                                                             center_x,
			                                                             center_y);
			canvas_face_poly_hit = merged_wall_projected_polygon_contains_point(track->nv,
			                                                                    track->pt_projected,
			                                                                    track->pt_sx,
			                                                                    track->pt_sy,
			                                                                    canvas_center_x,
			                                                                    canvas_center_y);
		}
		if (!center_face && !canvas_face && !face_poly_hit && !canvas_face_poly_hit)
			continue;

		face_hits++;
		if (face_logged >= MERGED_WALL_SNAPSHOT_OVERDRAW_MAX)
			continue;
		face_logged++;
		debug_log(DLOG_TEXTURE,
		          "[mwall_snapshot_overdraw_face] focus_rank=%d overdraw_rank=%d center_face=%d canvas_face=%d face_poly=%d/%d dist2=%.1f box=%s frame=%d pass=%d seq=%d draw_order=%d seg=%d side=%d face=%d child=%d wid=%d tmap1=%d tmap2=0x%x route=%s merge_impl=%s reason=%s",
		          focus_rank,
		          face_logged,
		          center_face,
		          canvas_face,
		          face_poly_valid ? face_poly_hit : -1,
		          face_poly_valid ? canvas_face_poly_hit : -1,
		          dist2,
		          track_box_kind ? track_box_kind : "none",
		          g_merged_wall_frame_id,
		          track->render_pass,
		          track->draw_seq,
		          track->draw_order,
		          track->draw_ctx.valid ? track->draw_ctx.seg : -1,
		          track->draw_ctx.valid ? track->draw_ctx.side : -1,
		          track->draw_ctx.valid ? track->draw_ctx.face : -1,
		          track->draw_ctx.valid ? track->draw_ctx.child : -1,
		          track->draw_ctx.valid ? track->draw_ctx.wid_flags : 0,
		          track->draw_ctx.valid ? track->draw_ctx.tmap1 : -1,
		          track->draw_ctx.valid ? track->draw_ctx.tmap2 : 0,
		          track->route,
		          track->merge_impl,
		          track->decision_reason);
		merged_wall_log_portal("snapshot_overdraw_face", &track->draw_ctx);
		merged_wall_log_face_textures("snapshot_overdraw_face", face_logged, &track->draw_ctx);
	}

	for (i = 0; i < merged_wall_cover_event_count; i++) {
		const struct merged_wall_snapshot_cover_event *event = &merged_wall_cover_events[i];
		const struct merged_wall_tracked_face *track;
		float overlap_area = 0.0f;
		float track_min_sx = 0.0f, track_max_sx = 0.0f;
		float track_min_sy = 0.0f, track_max_sy = 0.0f;
		float track_bbox_area = 0.0f;
		const char *track_box_kind = "none";
		int have_track_box = 0;
		int center_cover = 0;
		int canvas_cover = 0;
		int center_overlap = 0;
		int canvas_overlap = 0;
		int cover_poly_valid;
		int cover_poly_hit = 0;
		int canvas_cover_poly_hit = 0;

		if (event->cover_order <= focus_event->cover_order)
			continue;
		if (event->face_index < 0 || event->face_index >= merged_wall_tracked_face_count)
			continue;
		track = &merged_wall_tracked_faces[event->face_index];
		if (event->cover_bbox_valid) {
			center_cover = merged_wall_bbox_contains_point(center_x, center_y,
			                                               event->cover_min_sx, event->cover_max_sx,
			                                               event->cover_min_sy, event->cover_max_sy);
			canvas_cover = merged_wall_bbox_contains_point(canvas_center_x, canvas_center_y,
			                                               event->cover_min_sx, event->cover_max_sx,
			                                               event->cover_min_sy, event->cover_max_sy);
		}
		have_track_box = merged_wall_get_track_overlap_box(track,
		                                                   &track_min_sx, &track_max_sx,
		                                                   &track_min_sy, &track_max_sy,
		                                                   &track_bbox_area, &track_box_kind);
		if (have_track_box && event->cover_bbox_valid) {
			float overlap_min_sx = track_min_sx > event->cover_min_sx ? track_min_sx : event->cover_min_sx;
			float overlap_max_sx = track_max_sx < event->cover_max_sx ? track_max_sx : event->cover_max_sx;
			float overlap_min_sy = track_min_sy > event->cover_min_sy ? track_min_sy : event->cover_min_sy;
			float overlap_max_sy = track_max_sy < event->cover_max_sy ? track_max_sy : event->cover_max_sy;

			overlap_area = merged_wall_bbox_overlap_area(track_min_sx, track_max_sx,
			                                             track_min_sy, track_max_sy,
			                                             event->cover_min_sx, event->cover_max_sx,
			                                             event->cover_min_sy, event->cover_max_sy);
			if (overlap_area > 0.0f && merged_wall_bbox_contains_point(center_x, center_y,
			                                                           overlap_min_sx, overlap_max_sx,
			                                                           overlap_min_sy, overlap_max_sy))
				center_overlap = 1;
			if (overlap_area > 0.0f && merged_wall_bbox_contains_point(canvas_center_x, canvas_center_y,
			                                                           overlap_min_sx, overlap_max_sx,
			                                                           overlap_min_sy, overlap_max_sy))
				canvas_overlap = 1;
		}
		if (event->overlap_area > overlap_area)
			overlap_area = event->overlap_area;
		cover_poly_valid = merged_wall_projected_polygon_valid(event->cover_nv,
		                                                       event->cover_projected_count);
		if (cover_poly_valid) {
			cover_poly_hit = merged_wall_projected_polygon_contains_point(event->cover_nv,
			                                                              event->cover_pt_projected,
			                                                              event->cover_pt_sx,
			                                                              event->cover_pt_sy,
			                                                              center_x,
			                                                              center_y);
			canvas_cover_poly_hit = merged_wall_projected_polygon_contains_point(event->cover_nv,
			                                                                     event->cover_pt_projected,
			                                                                     event->cover_pt_sx,
			                                                                     event->cover_pt_sy,
			                                                                     canvas_center_x,
			                                                                     canvas_center_y);
		}
		if (!center_cover && !canvas_cover && !center_overlap && !canvas_overlap &&
		    !cover_poly_hit && !canvas_cover_poly_hit)
			continue;

		cover_hits++;
		if (cover_logged >= MERGED_WALL_SNAPSHOT_OVERDRAW_MAX)
			continue;
		cover_logged++;
		debug_log(DLOG_TEXTURE,
		          "[mwall_snapshot_overdraw_cover] focus_rank=%d overdraw_rank=%d kind=%s center_cover=%d canvas_cover=%d center_overlap=%d canvas_overlap=%d cover_poly=%d/%d overlap=%.1f box=%s frame=%d pass=%d seq=%d draw_order=%d cover_order=%d cover_shader=%s cover_bot=%s cover_ovl=%s seg=%d side=%d face=%d child=%d wid=%d cover_seg=%d cover_side=%d cover_face=%d cover_child=%d cover_wid=%d",
		          focus_rank,
		          cover_logged,
		          event->kind == MERGED_WALL_SNAPSHOT_COVER_EXACT ? "exact" : "bbox",
		          center_cover,
		          canvas_cover,
		          center_overlap,
		          canvas_overlap,
		          cover_poly_valid ? cover_poly_hit : -1,
		          cover_poly_valid ? canvas_cover_poly_hit : -1,
		          overlap_area,
		          track_box_kind ? track_box_kind : "none",
		          g_merged_wall_frame_id,
		          track->render_pass,
		          track->draw_seq,
		          track->draw_order,
		          event->cover_order,
		          event->cover_shader,
		          event->cover_bot,
		          event->cover_ovl,
		          track->draw_ctx.valid ? track->draw_ctx.seg : -1,
		          track->draw_ctx.valid ? track->draw_ctx.side : -1,
		          track->draw_ctx.valid ? track->draw_ctx.face : -1,
		          track->draw_ctx.valid ? track->draw_ctx.child : -1,
		          track->draw_ctx.valid ? track->draw_ctx.wid_flags : 0,
		          event->cover_ctx.valid ? event->cover_ctx.seg : -1,
		          event->cover_ctx.valid ? event->cover_ctx.side : -1,
		          event->cover_ctx.valid ? event->cover_ctx.face : -1,
		          event->cover_ctx.valid ? event->cover_ctx.child : -1,
		          event->cover_ctx.valid ? event->cover_ctx.wid_flags : 0);
		merged_wall_log_portal("snapshot_overdraw_face", &track->draw_ctx);
		merged_wall_log_portal("snapshot_overdraw_cover", &event->cover_ctx);
		merged_wall_log_face_textures("snapshot_overdraw_cover", cover_logged, &event->cover_ctx);
	}

	debug_log(DLOG_TEXTURE,
	          "[mwall_snapshot_overdraw] focus_rank=%d focus_cover_order=%d focus_kind=%s focus_cover=%s later_face_hits=%d later_face_logged=%d later_cover_hits=%d later_cover_logged=%d frame=%d request_frame=%d",
	          focus_rank,
	          focus_event->cover_order,
	          focus_event->kind == MERGED_WALL_SNAPSHOT_COVER_EXACT ? "exact" : "bbox",
	          focus_event->cover_bot,
	          face_hits,
	          face_logged,
	          cover_hits,
	          cover_logged,
	          g_merged_wall_frame_id,
	          g_merged_wall_snapshot_request_frame);
}

static void merged_wall_log_snapshot_focus_covers(float center_x, float center_y,
                                                  float canvas_center_x, float canvas_center_y,
                                                  const int *selected, int selected_count,
                                                  const int *partial_selected, int partial_selected_count)
{
	struct merged_wall_snapshot_focus_cover focus[MERGED_WALL_COVER_EVENT_MAX];
	int candidate_count = 0;
	int logged_count = 0;
	int omitted_count = 0;
	int i;

	memset(focus, 0, sizeof(focus));
	for (i = 0; i < merged_wall_cover_event_count && candidate_count < MERGED_WALL_COVER_EVENT_MAX; i++) {
		const struct merged_wall_snapshot_cover_event *event = &merged_wall_cover_events[i];
		const struct merged_wall_tracked_face *track;
		struct merged_wall_snapshot_focus_cover *candidate = &focus[candidate_count];
		float track_min_sx = 0.0f, track_max_sx = 0.0f;
		float track_min_sy = 0.0f, track_max_sy = 0.0f;
		float track_bbox_area = 0.0f;
		float overlap_area = 0.0f;
		const char *track_box_kind = "none";
		int have_track_box;

		if (event->face_index < 0 || event->face_index >= merged_wall_tracked_face_count)
			continue;
		track = &merged_wall_tracked_faces[event->face_index];
		have_track_box = merged_wall_get_track_overlap_box(track,
		                                                   &track_min_sx, &track_max_sx,
		                                                   &track_min_sy, &track_max_sy,
		                                                   &track_bbox_area, &track_box_kind);
		if (!have_track_box && !event->cover_bbox_valid)
			continue;

		candidate->valid = 1;
		candidate->event_index = i;
		candidate->draw_order = track->draw_order;
		candidate->cover_order = event->cover_order;
		candidate->track_box_kind = track_box_kind;
		if (have_track_box) {
			candidate->center_face = merged_wall_bbox_contains_point(center_x, center_y,
			                                                         track_min_sx, track_max_sx,
			                                                         track_min_sy, track_max_sy);
			candidate->face_dist2 = merged_wall_bbox_distance_sq(center_x, center_y,
			                                                     track_min_sx, track_max_sx,
			                                                     track_min_sy, track_max_sy);
			candidate->canvas_face = merged_wall_bbox_contains_point(canvas_center_x, canvas_center_y,
			                                                         track_min_sx, track_max_sx,
			                                                         track_min_sy, track_max_sy);
			candidate->canvas_face_dist2 = merged_wall_bbox_distance_sq(canvas_center_x, canvas_center_y,
			                                                            track_min_sx, track_max_sx,
			                                                            track_min_sy, track_max_sy);
		}
		if (event->cover_bbox_valid) {
			candidate->center_cover = merged_wall_bbox_contains_point(center_x, center_y,
			                                                          event->cover_min_sx, event->cover_max_sx,
			                                                          event->cover_min_sy, event->cover_max_sy);
			candidate->cover_dist2 = merged_wall_bbox_distance_sq(center_x, center_y,
			                                                      event->cover_min_sx, event->cover_max_sx,
			                                                      event->cover_min_sy, event->cover_max_sy);
			candidate->canvas_cover = merged_wall_bbox_contains_point(canvas_center_x, canvas_center_y,
			                                                          event->cover_min_sx, event->cover_max_sx,
			                                                          event->cover_min_sy, event->cover_max_sy);
			candidate->canvas_cover_dist2 = merged_wall_bbox_distance_sq(canvas_center_x, canvas_center_y,
			                                                             event->cover_min_sx, event->cover_max_sx,
			                                                             event->cover_min_sy, event->cover_max_sy);
		} else {
			candidate->cover_dist2 = candidate->face_dist2;
			candidate->canvas_cover_dist2 = candidate->canvas_face_dist2;
		}
		if (have_track_box && event->cover_bbox_valid) {
			float overlap_min_sx = track_min_sx > event->cover_min_sx ? track_min_sx : event->cover_min_sx;
			float overlap_max_sx = track_max_sx < event->cover_max_sx ? track_max_sx : event->cover_max_sx;
			float overlap_min_sy = track_min_sy > event->cover_min_sy ? track_min_sy : event->cover_min_sy;
			float overlap_max_sy = track_max_sy < event->cover_max_sy ? track_max_sy : event->cover_max_sy;

			overlap_area = merged_wall_bbox_overlap_area(track_min_sx, track_max_sx,
			                                             track_min_sy, track_max_sy,
			                                             event->cover_min_sx, event->cover_max_sx,
			                                             event->cover_min_sy, event->cover_max_sy);
			if (overlap_area > 0.0f && merged_wall_bbox_contains_point(center_x, center_y,
			                                                           overlap_min_sx, overlap_max_sx,
			                                                           overlap_min_sy, overlap_max_sy))
				candidate->center_overlap = 1;
			if (overlap_area > 0.0f && merged_wall_bbox_contains_point(canvas_center_x, canvas_center_y,
			                                                           overlap_min_sx, overlap_max_sx,
			                                                           overlap_min_sy, overlap_max_sy))
				candidate->canvas_overlap = 1;
		}
		if (event->overlap_area > overlap_area)
			overlap_area = event->overlap_area;
		candidate->overlap_area = overlap_area;
		candidate->face_poly_valid = merged_wall_projected_polygon_valid(track->nv, track->projected_count);
		if (candidate->face_poly_valid) {
			candidate->face_poly_hit = merged_wall_projected_polygon_contains_point(track->nv,
			                                                                        track->pt_projected,
			                                                                        track->pt_sx,
			                                                                        track->pt_sy,
			                                                                        center_x,
			                                                                        center_y);
			candidate->canvas_face_poly_hit = merged_wall_projected_polygon_contains_point(track->nv,
			                                                                               track->pt_projected,
			                                                                               track->pt_sx,
			                                                                               track->pt_sy,
			                                                                               canvas_center_x,
			                                                                               canvas_center_y);
		}
		candidate->cover_poly_valid = merged_wall_projected_polygon_valid(event->cover_nv,
		                                                                  event->cover_projected_count);
		if (candidate->cover_poly_valid) {
			candidate->cover_poly_hit = merged_wall_projected_polygon_contains_point(event->cover_nv,
			                                                                         event->cover_pt_projected,
			                                                                         event->cover_pt_sx,
			                                                                         event->cover_pt_sy,
			                                                                         center_x,
			                                                                         center_y);
			candidate->canvas_cover_poly_hit = merged_wall_projected_polygon_contains_point(event->cover_nv,
			                                                                                event->cover_pt_projected,
			                                                                                event->cover_pt_sx,
			                                                                                event->cover_pt_sy,
			                                                                                canvas_center_x,
			                                                                                canvas_center_y);
		}
		candidate->face_rank = merged_wall_selected_rank(selected, selected_count, event->face_index);
		if (candidate->face_rank >= 0)
			candidate->face_rank++;
		else
			candidate->face_rank = 0;
		candidate->partial_rank = 0;
		if (candidate->face_rank <= 0) {
			candidate->partial_rank = merged_wall_selected_rank(partial_selected, partial_selected_count,
			                                                    event->face_index);
			if (candidate->partial_rank >= 0)
				candidate->partial_rank++;
			else
				candidate->partial_rank = 0;
		}
		candidate->focus_bucket = candidate->center_overlap ? 0 : candidate->center_cover ? 1
		                                                      : candidate->center_face    ? 2
		                                                                                  : 3;
		candidate->focus_dist2 = candidate->center_overlap || candidate->center_cover || candidate->center_face
		                             ? 0.0f
		                             : (event->cover_bbox_valid ? candidate->cover_dist2 : candidate->face_dist2);
		candidate->canvas_focus_dist2 = candidate->canvas_overlap || candidate->canvas_cover || candidate->canvas_face
		                                    ? 0.0f
		                                    : (event->cover_bbox_valid ? candidate->canvas_cover_dist2
		                                                               : candidate->canvas_face_dist2);
		candidate_count++;
	}

	if (candidate_count <= 0) {
		debug_log(DLOG_TEXTURE,
		          "[mwall_snapshot] no_focus_cover_candidates frame=%d request_frame=%d",
		          g_merged_wall_frame_id,
		          g_merged_wall_snapshot_request_frame);
		return;
	}

	for (;;) {
		int best_index = -1;

		for (i = 0; i < candidate_count; i++) {
			if (!focus[i].valid)
				continue;
			if (best_index < 0 || merged_wall_snapshot_focus_cover_better(&focus[i], &focus[best_index]))
				best_index = i;
		}
		if (best_index < 0)
			break;
		if (logged_count >= MERGED_WALL_SNAPSHOT_COVER_MAX) {
			omitted_count++;
			focus[best_index].valid = 0;
			continue;
		}
		logged_count++;
		focus[best_index].valid = 0;
		{
			const struct merged_wall_snapshot_focus_cover *candidate = &focus[best_index];
			const struct merged_wall_snapshot_cover_event *event = &merged_wall_cover_events[candidate->event_index];
			const struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[event->face_index];
			float log_track_min_sx = 0.0f, log_track_max_sx = 0.0f;
			float log_track_min_sy = 0.0f, log_track_max_sy = 0.0f;
			float log_track_bbox_area = 0.0f;
			const char *log_track_box_kind = "none";
			int log_have_track_box = merged_wall_get_track_overlap_box(track,
			                                                           &log_track_min_sx,
			                                                           &log_track_max_sx,
			                                                           &log_track_min_sy,
			                                                           &log_track_max_sy,
			                                                           &log_track_bbox_area,
			                                                           &log_track_box_kind);
			const char *rank_source = candidate->face_rank > 0 ? "face" : candidate->partial_rank > 0 ? "partial"
			                                                                                          : "none";
			int rank_value = candidate->face_rank > 0 ? candidate->face_rank : candidate->partial_rank;

			debug_log(DLOG_TEXTURE,
			          "[mwall_snapshot_focus_cover] focus_rank=%d focus=%s kind=%s face_rank=%d rank_source=%s center_face=%d center_cover=%d center_overlap=%d canvas_face=%d canvas_cover=%d canvas_overlap=%d dist2=%.1f canvas_dist2=%.1f face_dist2=%.1f cover_dist2=%.1f overlap=%.1f box=%s ordered=%d frame=%d pass=%d seq=%d draw_order=%d cover_order=%d cover_shader=%s cover_bot=%s cover_ovl=%s seg=%d side=%d face=%d child=%d wid=%d tmap1=%d tmap2=0x%x cover_seg=%d cover_side=%d cover_face=%d cover_child=%d cover_wid=%d cover_tmap1=%d cover_tmap2=0x%x",
			          logged_count,
			          merged_wall_snapshot_focus_bucket_name(candidate->focus_bucket),
			          event->kind == MERGED_WALL_SNAPSHOT_COVER_EXACT ? "exact" : "bbox",
			          rank_value,
			          rank_source,
			          candidate->center_face,
			          candidate->center_cover,
			          candidate->center_overlap,
			          candidate->canvas_face,
			          candidate->canvas_cover,
			          candidate->canvas_overlap,
			          candidate->focus_dist2,
			          candidate->canvas_focus_dist2,
			          candidate->face_dist2,
			          candidate->cover_dist2,
			          candidate->overlap_area,
			          candidate->track_box_kind ? candidate->track_box_kind : "none",
			          event->ordered,
			          g_merged_wall_frame_id,
			          track->render_pass,
			          track->draw_seq,
			          track->draw_order,
			          event->cover_order,
			          event->cover_shader,
			          event->cover_bot,
			          event->cover_ovl,
			          track->draw_ctx.valid ? track->draw_ctx.seg : -1,
			          track->draw_ctx.valid ? track->draw_ctx.side : -1,
			          track->draw_ctx.valid ? track->draw_ctx.face : -1,
			          track->draw_ctx.valid ? track->draw_ctx.child : -1,
			          track->draw_ctx.valid ? track->draw_ctx.wid_flags : 0,
			          track->draw_ctx.valid ? track->draw_ctx.tmap1 : -1,
			          track->draw_ctx.valid ? track->draw_ctx.tmap2 : 0,
			          event->cover_ctx.valid ? event->cover_ctx.seg : -1,
			          event->cover_ctx.valid ? event->cover_ctx.side : -1,
			          event->cover_ctx.valid ? event->cover_ctx.face : -1,
			          event->cover_ctx.valid ? event->cover_ctx.child : -1,
			          event->cover_ctx.valid ? event->cover_ctx.wid_flags : 0,
			          event->cover_ctx.valid ? event->cover_ctx.tmap1 : -1,
			          event->cover_ctx.valid ? event->cover_ctx.tmap2 : 0);
			debug_log(DLOG_TEXTURE,
			          "[mwall_snapshot_focus_box] focus_rank=%d screen_center=%.1f/%.1f canvas_center=%.1f/%.1f face_box_valid=%d face_box=%s:%.1f..%.1f/%.1f..%.1f cover_box_valid=%d cover_box=%.1f..%.1f/%.1f..%.1f face_proj=%d/%d face_poly=%d/%d cover_proj=%d/%d cover_poly=%d/%d",
			          logged_count,
			          center_x,
			          center_y,
			          canvas_center_x,
			          canvas_center_y,
			          log_have_track_box,
			          log_track_box_kind ? log_track_box_kind : "none",
			          log_track_min_sx,
			          log_track_max_sx,
			          log_track_min_sy,
			          log_track_max_sy,
			          event->cover_bbox_valid,
			          event->cover_min_sx,
			          event->cover_max_sx,
			          event->cover_min_sy,
			          event->cover_max_sy,
			          track->projected_count,
			          track->nv,
			          candidate->face_poly_valid ? candidate->face_poly_hit : -1,
			          candidate->face_poly_valid ? candidate->canvas_face_poly_hit : -1,
			          event->cover_projected_count,
			          event->cover_nv,
			          candidate->cover_poly_valid ? candidate->cover_poly_hit : -1,
			          candidate->cover_poly_valid ? candidate->canvas_cover_poly_hit : -1);
			merged_wall_log_portal("snapshot_focus_face", &track->draw_ctx);
			merged_wall_log_portal("snapshot_focus_cover", &event->cover_ctx);
			merged_wall_log_face_textures("snapshot_focus_face", logged_count, &track->draw_ctx);
			merged_wall_log_face_textures("snapshot_focus_cover", logged_count, &event->cover_ctx);
			if (logged_count == 1 &&
			    (candidate->center_cover || candidate->center_overlap ||
			     candidate->canvas_cover || candidate->canvas_overlap ||
			     (candidate->cover_poly_valid &&
			      (candidate->cover_poly_hit || candidate->canvas_cover_poly_hit))))
				merged_wall_log_snapshot_post_focus_overdraw(logged_count,
				                                             center_x,
				                                             center_y,
				                                             canvas_center_x,
				                                             canvas_center_y,
				                                             candidate);
		}
	}

	debug_log(DLOG_TEXTURE,
	          "[mwall_snapshot] focus_cover_candidates=%d focus_cover_logged=%d focus_cover_omitted=%d frame=%d request_frame=%d",
	          candidate_count,
	          logged_count,
	          omitted_count,
	          g_merged_wall_frame_id,
	          g_merged_wall_snapshot_request_frame);
}

static float merged_wall_wrap_unit(float value)
{
	float wrapped = fmodf(value, 1.0f);

	if (wrapped < 0.0f)
		wrapped += 1.0f;
	return wrapped;
}

static void merged_wall_probe_map_overlay_uv(int orient, float u, float v,
                                             float *out_u, float *out_v)
{
	float mapped_u = u;
	float mapped_v = v;

	switch (orient) {
		case 1:
			mapped_u = 1.0f - v;
			mapped_v = u;
			break;
		case 2:
			mapped_u = 1.0f - u;
			mapped_v = 1.0f - v;
			break;
		case 3:
			mapped_u = v;
			mapped_v = 1.0f - u;
			break;
		default:
			break;
	}
	if (out_u)
		*out_u = mapped_u;
	if (out_v)
		*out_v = mapped_v;
}

static void merged_wall_probe_sample_path(const char *route, int orient,
                                          char *dst, unsigned int dst_size)
{
	const char *branch = "B1";

	if (route && !strcmp(route, "merge_cached"))
		branch = "B2";
	else if (route && strstr(route, "old_texmerge"))
		branch = "B3";
	if (!dst || dst_size == 0)
		return;
	snprintf(dst, dst_size, "%s_orient%d", branch, orient);
	dst[dst_size - 1] = '\0';
}

static void merged_wall_probe_flip_axis(const char *route, int orient,
                                        char *dst, unsigned int dst_size)
{
	/* After the base_v FBO-orientation fix in ogl_android_texmerge_build_uvs,
	 * merge_cached no longer Y-flips the composite relative to the CPU
	 * merge_textures_new reference, so no route currently produces an
	 * intrinsic per-orient flip. This function is kept as a model hook so
	 * future route variants can advertise a flip without touching call
	 * sites. */
	(void) route;
	(void) orient;
	merged_wall_copy_string(dst, dst_size, "none");
}

static void merged_wall_probe_map_overlay_uv_for_route(const char *route, int orient,
                                                       float u, float v,
                                                       float *out_u, float *out_v)
{
	float mapped_u = 0.0f;
	float mapped_v = 0.0f;
	char flip_axis[8];

	merged_wall_probe_map_overlay_uv(orient, u, v, &mapped_u, &mapped_v);
	merged_wall_probe_flip_axis(route, orient, flip_axis, sizeof(flip_axis));
	if (!strcmp(flip_axis, "U"))
		mapped_u = 1.0f - mapped_u;
	else if (!strcmp(flip_axis, "V"))
		mapped_v = 1.0f - mapped_v;
	if (out_u)
		*out_u = mapped_u;
	if (out_v)
		*out_v = mapped_v;
}

static const char *merged_wall_probe_screen_axis_name(float dx, float dy)
{
	float adx = fabsf(dx);
	float ady = fabsf(dy);

	if (adx < 1.0f && ady < 1.0f)
		return "unknown";
	if (adx > ady * 1.25f)
		return "horizontal";
	if (ady > adx * 1.25f)
		return "vertical";
	return "diagonal";
}

static void merged_wall_probe_uv_axis_name(const struct merged_wall_tracked_face *track,
                                           int use_u, char *dst, unsigned int dst_size)
{
	float best_delta = 0.0f;
	float best_dx = 0.0f, best_dy = 0.0f;
	int i, j;

	merged_wall_copy_string(dst, dst_size, "unknown");
	if (!track)
		return;
	for (i = 0; i < track->nv; i++) {
		if (!track->pt_projected[i])
			continue;
		for (j = i + 1; j < track->nv; j++) {
			float delta;
			float dx;
			float dy;

			if (!track->pt_projected[j])
				continue;
			delta = use_u
			            ? fabsf((float) f2fl(track->uvl[j].u - track->uvl[i].u))
			            : fabsf((float) f2fl(track->uvl[j].v - track->uvl[i].v));
			if (delta <= best_delta + 0.0001f)
				continue;
			dx = track->pt_sx[j] - track->pt_sx[i];
			dy = track->pt_sy[j] - track->pt_sy[i];
			best_delta = delta;
			best_dx = dx;
			best_dy = dy;
		}
	}
	merged_wall_copy_string(dst, dst_size,
	                        merged_wall_probe_screen_axis_name(best_dx, best_dy));
}

static const char *merged_wall_probe_corner_tag(const struct merged_wall_tracked_face *track,
                                                int index)
{
	float mid_x;
	float mid_y;

	if (!track || index < 0 || index >= track->nv || !track->bbox_valid || !track->pt_projected[index])
		return "NA";
	mid_x = (track->min_sx + track->max_sx) * 0.5f;
	mid_y = (track->min_sy + track->max_sy) * 0.5f;
	if (track->pt_sx[index] <= mid_x)
		return track->pt_sy[index] <= mid_y ? "LT" : "LB";
	return track->pt_sy[index] <= mid_y ? "RT" : "RB";
}

struct merged_wall_tmap_state {
	int tex_num;
	int bitmap_index;
	const char *name;
	int flags;
	int lighting;
	int damage;
	int eclip_num;
	int destroyed;
	int slide_u;
	int slide_v;
	int eclip_flags;
	int eclip_frame_count;
	int eclip_time_left;
	int eclip_frames;
	int eclip_wall_texture;
	int eclip_dest_bm;
	int eclip_frame0_bm;
	int eclip_current_bm;
};

static void merged_wall_hash_int(unsigned int *hash, int value)
{
	if (!hash)
		return;
	*hash = merged_wall_fnv1a_append(*hash, (const unsigned char *) &value,
	                                 sizeof(value));
}

static int merged_wall_texture_valid(int tex_num)
{
	return tex_num >= 0 && tex_num < NumTextures;
}

static const char *merged_wall_texture_name(int tex_num)
{
	grs_bitmap *bitmap;
	const char *name;

	if (!merged_wall_texture_valid(tex_num))
		return "<invalid>";
	bitmap = &GameBitmaps[Textures[tex_num].index];
	name = piggy_game_bitmap_name(bitmap);
	return name ? name : "<none>";
}

static void merged_wall_init_tmap_state(struct merged_wall_tmap_state *state)
{
	if (!state)
		return;
	memset(state, 0, sizeof(*state));
	state->tex_num = -1;
	state->bitmap_index = -1;
	state->name = "<invalid>";
	state->flags = -1;
	state->lighting = 0;
	state->damage = 0;
	state->eclip_num = -1;
	state->destroyed = -1;
	state->slide_u = 0;
	state->slide_v = 0;
	state->eclip_flags = -1;
	state->eclip_frame_count = -1;
	state->eclip_time_left = -1;
	state->eclip_frames = -1;
	state->eclip_wall_texture = -1;
	state->eclip_dest_bm = -1;
	state->eclip_frame0_bm = -1;
	state->eclip_current_bm = -1;
}

static void merged_wall_fill_tmap_state(int tex_num,
                                        struct merged_wall_tmap_state *state)
{
	int eclip_num;

	merged_wall_init_tmap_state(state);
	if (!state || !merged_wall_texture_valid(tex_num))
		return;

	state->tex_num = tex_num;
	state->bitmap_index = Textures[tex_num].index;
	state->name = merged_wall_texture_name(tex_num);
	state->flags = TmapInfo[tex_num].flags;
	state->lighting = TmapInfo[tex_num].lighting;
	state->damage = TmapInfo[tex_num].damage;
	state->eclip_num = TmapInfo[tex_num].eclip_num;
#ifdef DXX_BUILD_DESCENT_II
	state->destroyed = TmapInfo[tex_num].destroyed;
	state->slide_u = TmapInfo[tex_num].slide_u;
	state->slide_v = TmapInfo[tex_num].slide_v;
#endif

	eclip_num = state->eclip_num;
	if (eclip_num >= 0 && eclip_num < Num_effects && eclip_num < MAX_EFFECTS) {
		int current_frame = Effects[eclip_num].frame_count;

		state->eclip_flags = Effects[eclip_num].flags;
		state->eclip_frame_count = current_frame;
		state->eclip_time_left = Effects[eclip_num].time_left;
		state->eclip_frames = Effects[eclip_num].vc.num_frames;
		state->eclip_wall_texture = Effects[eclip_num].changing_wall_texture;
		state->eclip_dest_bm = Effects[eclip_num].dest_bm_num;
		if (Effects[eclip_num].vc.num_frames > 0)
			state->eclip_frame0_bm = Effects[eclip_num].vc.frames[0].index;
		if (current_frame >= 0 && current_frame < Effects[eclip_num].vc.num_frames)
			state->eclip_current_bm =
			    Effects[eclip_num].vc.frames[current_frame].index;
	}
}

static void merged_wall_hash_tmap_state(unsigned int *hash,
                                        const struct merged_wall_tmap_state *state)
{
	if (!hash || !state)
		return;
	merged_wall_hash_int(hash, state->tex_num);
	merged_wall_hash_int(hash, state->bitmap_index);
	merged_wall_hash_int(hash, state->flags);
	merged_wall_hash_int(hash, state->lighting);
	merged_wall_hash_int(hash, state->damage);
	merged_wall_hash_int(hash, state->eclip_num);
	merged_wall_hash_int(hash, state->destroyed);
	merged_wall_hash_int(hash, state->slide_u);
	merged_wall_hash_int(hash, state->slide_v);
	merged_wall_hash_int(hash, state->eclip_flags);
	merged_wall_hash_int(hash, state->eclip_frame_count);
	merged_wall_hash_int(hash, state->eclip_time_left);
	merged_wall_hash_int(hash, state->eclip_frames);
	merged_wall_hash_int(hash, state->eclip_wall_texture);
	merged_wall_hash_int(hash, state->eclip_dest_bm);
	merged_wall_hash_int(hash, state->eclip_frame0_bm);
	merged_wall_hash_int(hash, state->eclip_current_bm);
}

static unsigned int merged_wall_side_uv_signature(const struct side *sidep)
{
	unsigned int hash = MERGED_WALL_FNV1A_OFFSET;
	int i;

	if (!sidep)
		return 0;
	for (i = 0; i < 4; i++) {
		merged_wall_hash_int(&hash, sidep->uvls[i].u);
		merged_wall_hash_int(&hash, sidep->uvls[i].v);
		merged_wall_hash_int(&hash, sidep->uvls[i].l);
	}
	return hash;
}

static int merged_wall_active_door_index_for_wall(int wall_num)
{
	int i, part;

	if (wall_num < 0)
		return -1;
	for (i = 0; i < Num_open_doors; i++) {
		active_door *doorp = &ActiveDoors[i];
		int parts = doorp->n_parts;

		if (parts < 0)
			parts = 0;
		if (parts > 2)
			parts = 2;
		for (part = 0; part < parts; part++)
			if (doorp->front_wallnum[part] == wall_num ||
			    doorp->back_wallnum[part] == wall_num)
				return i;
	}
	return -1;
}

#ifdef DXX_BUILD_DESCENT_II
static int merged_wall_cloaking_wall_index_for_wall(int wall_num)
{
	int i;

	if (wall_num < 0)
		return -1;
	for (i = 0; i < Num_cloaking_walls; i++)
		if (CloakingWalls[i].front_wallnum == wall_num ||
		    CloakingWalls[i].back_wallnum == wall_num)
			return i;
	return -1;
}
#endif

static void merged_wall_probe_log_side_state(const char *source,
                                             const struct merged_wall_tracked_face *track)
{
	const struct android_draw_face_context *ctx;
	segment *segp = NULL;
	struct side *sidep = NULL;
	segment *conn_seg = NULL;
	struct side *conn_sidep = NULL;
	wall *wallp = NULL;
	wall *conn_wallp = NULL;
	active_door *doorp = NULL;
	wclip *clip = NULL;
	struct merged_wall_tmap_state base_state;
	struct merged_wall_tmap_state overlay_state;
	unsigned int side_sig = MERGED_WALL_FNV1A_OFFSET;
	unsigned int uv_sig = 0;
	int segnum, sidenum, child, wall_num, conn_side, conn_child, conn_wall_num;
	int raw_tmap1, raw_tmap2, overlay;
	int wid_flags;
	int wall_type = -1, wall_state = -1, wall_clip = -1, wall_keys = -1;
	int wall_hps = 0, wall_trigger = -1, wall_linked = -1;
	int wall_control = -1, wall_cloak = -1;
	unsigned int wall_flags = 0;
	int conn_type = -1, conn_state = -1;
	unsigned int conn_flags = 0;
	int clip_valid = 0, clip_flags = 0, clip_frames = -1, clip_play_time = -1;
	int clip_frame0 = -1, clip_last = -1;
	int door_index = -1, door_time = -1, door_parts = -1;
	int door_front0 = -1, door_back0 = -1, door_front1 = -1, door_back1 = -1;
	int cloak_index = -1, cloak_time = -1;

	if (!track || !track->draw_ctx.valid)
		return;
	ctx = &track->draw_ctx;
	segnum = ctx->seg;
	sidenum = ctx->side;
	if (segnum < 0 || segnum > Highest_segment_index ||
	    sidenum < 0 || sidenum >= MAX_SIDES_PER_SEGMENT)
		return;

	segp = &Segments[segnum];
	sidep = &segp->sides[sidenum];
	child = segp->children[sidenum];
	wall_num = sidep->wall_num;
	raw_tmap1 = sidep->tmap_num;
	raw_tmap2 = sidep->tmap_num2;
	overlay = raw_tmap2 ? merged_wall_overlay_index(raw_tmap2) : -1;
	wid_flags = WALL_IS_DOORWAY(segp, sidenum);
	uv_sig = merged_wall_side_uv_signature(sidep);
	if (wall_num >= 0 && wall_num < Num_walls) {
		wallp = &Walls[wall_num];
		wall_type = wallp->type;
		wall_flags = wallp->flags;
		wall_state = wallp->state;
		wall_trigger = wallp->trigger;
		wall_clip = wallp->clip_num;
		wall_keys = wallp->keys;
		wall_hps = wallp->hps;
		wall_linked = wallp->linked_wall;
#ifdef DXX_BUILD_DESCENT_II
		wall_control = wallp->controlling_trigger;
		wall_cloak = wallp->cloak_value;
#endif
	}
	if (child >= 0 && child <= Highest_segment_index) {
		conn_seg = &Segments[child];
		conn_side = find_connect_side(conn_seg, segp);
		if (conn_side >= 0 && conn_side < MAX_SIDES_PER_SEGMENT) {
			conn_sidep = &conn_seg->sides[conn_side];
			conn_child = conn_seg->children[conn_side];
			conn_wall_num = conn_sidep->wall_num;
			if (conn_wall_num >= 0 && conn_wall_num < Num_walls) {
				conn_wallp = &Walls[conn_wall_num];
				conn_type = conn_wallp->type;
				conn_state = conn_wallp->state;
				conn_flags = conn_wallp->flags;
			}
		} else {
			conn_child = -1;
			conn_wall_num = -1;
		}
	} else {
		conn_side = -1;
		conn_child = -1;
		conn_wall_num = -1;
	}
	if (wall_clip >= 0 && wall_clip < Num_wall_anims &&
	    WallAnims[wall_clip].num_frames > 0) {
		clip = &WallAnims[wall_clip];
		clip_valid = 1;
		clip_flags = clip->flags;
		clip_frames = clip->num_frames;
		clip_play_time = clip->play_time;
		clip_frame0 = clip->frames[0];
		clip_last = clip->frames[clip->num_frames - 1];
	}
	door_index = merged_wall_active_door_index_for_wall(wall_num >= 0 ? wall_num : conn_wall_num);
	if (door_index >= 0 && door_index < Num_open_doors) {
		doorp = &ActiveDoors[door_index];
		door_time = doorp->time;
		door_parts = doorp->n_parts;
		door_front0 = doorp->front_wallnum[0];
		door_back0 = doorp->back_wallnum[0];
		door_front1 = doorp->front_wallnum[1];
		door_back1 = doorp->back_wallnum[1];
	}
#ifdef DXX_BUILD_DESCENT_II
	cloak_index = merged_wall_cloaking_wall_index_for_wall(wall_num >= 0 ? wall_num : conn_wall_num);
	if (cloak_index >= 0 && cloak_index < Num_cloaking_walls)
		cloak_time = CloakingWalls[cloak_index].time;
#endif

	merged_wall_fill_tmap_state(raw_tmap1, &base_state);
	merged_wall_fill_tmap_state(overlay, &overlay_state);

	merged_wall_hash_int(&side_sig, Current_level_num);
	merged_wall_hash_int(&side_sig, Game_mode);
	merged_wall_hash_int(&side_sig, segnum);
	merged_wall_hash_int(&side_sig, sidenum);
	merged_wall_hash_int(&side_sig, child);
	merged_wall_hash_int(&side_sig, sidep->type);
	merged_wall_hash_int(&side_sig, wid_flags);
	merged_wall_hash_int(&side_sig, raw_tmap1);
	merged_wall_hash_int(&side_sig, raw_tmap2);
	merged_wall_hash_int(&side_sig, wall_num);
	merged_wall_hash_int(&side_sig, wall_type);
	merged_wall_hash_int(&side_sig, (int) wall_flags);
	merged_wall_hash_int(&side_sig, wall_state);
	merged_wall_hash_int(&side_sig, wall_trigger);
	merged_wall_hash_int(&side_sig, wall_clip);
	merged_wall_hash_int(&side_sig, wall_keys);
	merged_wall_hash_int(&side_sig, wall_hps);
	merged_wall_hash_int(&side_sig, wall_linked);
	merged_wall_hash_int(&side_sig, wall_control);
	merged_wall_hash_int(&side_sig, wall_cloak);
	merged_wall_hash_int(&side_sig, conn_side);
	merged_wall_hash_int(&side_sig, conn_child);
	merged_wall_hash_int(&side_sig, conn_wall_num);
	merged_wall_hash_int(&side_sig, conn_type);
	merged_wall_hash_int(&side_sig, (int) conn_flags);
	merged_wall_hash_int(&side_sig, conn_state);
	if (conn_sidep) {
		merged_wall_hash_int(&side_sig, conn_sidep->type);
		merged_wall_hash_int(&side_sig, conn_sidep->tmap_num);
		merged_wall_hash_int(&side_sig, conn_sidep->tmap_num2);
	}
	merged_wall_hash_int(&side_sig, (int) uv_sig);
	merged_wall_hash_int(&side_sig, door_index);
	merged_wall_hash_int(&side_sig, door_time);
	merged_wall_hash_int(&side_sig, door_parts);
	merged_wall_hash_int(&side_sig, door_front0);
	merged_wall_hash_int(&side_sig, door_back0);
	merged_wall_hash_int(&side_sig, door_front1);
	merged_wall_hash_int(&side_sig, door_back1);
	merged_wall_hash_int(&side_sig, cloak_index);
	merged_wall_hash_int(&side_sig, cloak_time);
	merged_wall_hash_int(&side_sig, clip_valid);
	merged_wall_hash_int(&side_sig, clip_flags);
	merged_wall_hash_int(&side_sig, clip_frames);
	merged_wall_hash_int(&side_sig, clip_play_time);
	merged_wall_hash_int(&side_sig, clip_frame0);
	merged_wall_hash_int(&side_sig, clip_last);
	merged_wall_hash_tmap_state(&side_sig, &base_state);
	merged_wall_hash_tmap_state(&side_sig, &overlay_state);

	debug_log_force(DLOG_TEXTURE,
	                "[mwall_tap_probe] kind=side_state source=%s frame=%d request_frame=%d level=%d seg=%d side=%d face=%d side_sig=0x%08x uv_sig=0x%08x child=%d side_type=%d wid=%d draw_tmap1=%d draw_tmap2=0x%x raw_tmap1=%d raw_tmap2=0x%x overlay=%d wall_num=%d wall_type=%d wall_state=%d wall_flags=0x%x wall_clip=%d wall_keys=%d wall_hps=%d wall_trigger=%d wall_linked=%d wall_control=%d wall_cloak=%d open_doors=%d door_idx=%d door_time=%d door_parts=%d door_front=%d/%d door_back=%d/%d cloak_idx=%d cloak_time=%d clip_valid=%d clip_flags=0x%x clip_frames=%d clip_play=%d clip_f0=%d clip_last=%d clip_f0_name=%s clip_last_name=%s conn_seg=%d conn_side=%d conn_child=%d conn_side_type=%d conn_tmap1=%d conn_tmap2=0x%x conn_wall=%d conn_wall_type=%d conn_wall_state=%d conn_wall_flags=0x%x",
	                source ? source : "",
	                g_merged_wall_frame_id,
	                g_merged_wall_snapshot_request_frame,
	                Current_level_num,
	                segnum,
	                sidenum,
	                ctx->face,
	                side_sig,
	                uv_sig,
	                child,
	                sidep->type,
	                wid_flags,
	                ctx->tmap1,
	                ctx->tmap2,
	                raw_tmap1,
	                raw_tmap2,
	                overlay,
	                wall_num,
	                wall_type,
	                wall_state,
	                wall_flags,
	                wall_clip,
	                wall_keys,
	                wall_hps,
	                wall_trigger,
	                wall_linked,
	                wall_control,
	                wall_cloak,
	                Num_open_doors,
	                door_index,
	                door_time,
	                door_parts,
	                door_front0,
	                door_front1,
	                door_back0,
	                door_back1,
	                cloak_index,
	                cloak_time,
	                clip_valid,
	                clip_flags,
	                clip_frames,
	                clip_play_time,
	                clip_frame0,
	                clip_last,
	                merged_wall_texture_name(clip_frame0),
	                merged_wall_texture_name(clip_last),
	                child,
	                conn_side,
	                conn_child,
	                conn_sidep ? conn_sidep->type : -1,
	                conn_sidep ? conn_sidep->tmap_num : -1,
	                conn_sidep ? conn_sidep->tmap_num2 : 0,
	                conn_wall_num,
	                conn_type,
	                conn_state,
	                conn_flags);
	debug_log_force(DLOG_TEXTURE,
	                "[mwall_tap_probe] kind=side_tex_state source=%s frame=%d request_frame=%d level=%d seg=%d side=%d face=%d side_sig=0x%08x base_tex=%d base_name=%s base_bitmap=%d base_flags=0x%x base_light=%d base_damage=%d base_eclip=%d base_destroyed=%d base_slide=%d/%d base_ec_flags=0x%x base_ec_frame=%d base_ec_time_left=%d base_ec_frames=%d base_ec_wall_tex=%d base_ec_dest=%d base_ec_f0_bm=%d base_ec_cur_bm=%d overlay_tex=%d overlay_name=%s overlay_bitmap=%d overlay_flags=0x%x overlay_light=%d overlay_damage=%d overlay_eclip=%d overlay_destroyed=%d overlay_slide=%d/%d overlay_ec_flags=0x%x overlay_ec_frame=%d overlay_ec_time_left=%d overlay_ec_frames=%d overlay_ec_wall_tex=%d overlay_ec_dest=%d overlay_ec_f0_bm=%d overlay_ec_cur_bm=%d",
	                source ? source : "",
	                g_merged_wall_frame_id,
	                g_merged_wall_snapshot_request_frame,
	                Current_level_num,
	                segnum,
	                sidenum,
	                ctx->face,
	                side_sig,
	                base_state.tex_num,
	                base_state.name,
	                base_state.bitmap_index,
	                base_state.flags,
	                base_state.lighting,
	                base_state.damage,
	                base_state.eclip_num,
	                base_state.destroyed,
	                base_state.slide_u,
	                base_state.slide_v,
	                base_state.eclip_flags,
	                base_state.eclip_frame_count,
	                base_state.eclip_time_left,
	                base_state.eclip_frames,
	                base_state.eclip_wall_texture,
	                base_state.eclip_dest_bm,
	                base_state.eclip_frame0_bm,
	                base_state.eclip_current_bm,
	                overlay_state.tex_num,
	                overlay_state.name,
	                overlay_state.bitmap_index,
	                overlay_state.flags,
	                overlay_state.lighting,
	                overlay_state.damage,
	                overlay_state.eclip_num,
	                overlay_state.destroyed,
	                overlay_state.slide_u,
	                overlay_state.slide_v,
	                overlay_state.eclip_flags,
	                overlay_state.eclip_frame_count,
	                overlay_state.eclip_time_left,
	                overlay_state.eclip_frames,
	                overlay_state.eclip_wall_texture,
	                overlay_state.eclip_dest_bm,
	                overlay_state.eclip_frame0_bm,
	                overlay_state.eclip_current_bm);
}

static const char *merged_wall_palette_pointer_name(const unsigned char *pal)
{
	if (!pal)
		return "null";
	if (pal == gr_palette)
		return "gr_palette";
	if (pal == gr_current_pal)
		return "gr_current_pal";
	return "other";
}

static void merged_wall_probe_log_palette_bitmap(const char *source,
                                                 const struct merged_wall_tracked_face *track,
                                                 const char *layer,
                                                 int tex_num,
                                                 grs_bitmap *bitmap)
{
	const struct android_draw_face_context *ctx = track ? &track->draw_ctx : NULL;
	grs_bitmap *src = merged_wall_get_source_bitmap(bitmap);
	const char *name = piggy_game_bitmap_name(bitmap);
	int real_flags = bitmap ? piggy_bitmap_get_flags(bitmap) : 0;
	int src_bytes = 0, idx254 = 0, idx255 = 0;
	int idx0 = merged_wall_bitmap_sample_index(bitmap, 0);
	int idxc = merged_wall_bitmap_sample_index(bitmap, 1);
	int idxlast = merged_wall_bitmap_sample_index(bitmap, 2);
	unsigned int src_hash = merged_wall_bitmap_hash(bitmap, &src_bytes, &idx254,
	                                                &idx255);

	if (!bitmap)
		return;
	debug_log_force(DLOG_TEXTURE,
	                "[mwall_tap_probe] kind=palette_bitmap source=%s frame=%d request_frame=%d level=%d seg=%d side=%d face=%d layer=%s tex=%d name=%s real_flags=0x%x bm_flags=0x%x w=%d h=%d src_hash=0x%08x src_bytes=%d src_254=%d src_255=%d idx0=%d idxc=%d idxlast=%d idx0_rgba=0x%08x/0x%08x/0x%08x idxc_rgba=0x%08x/0x%08x/0x%08x idxlast_rgba=0x%08x/0x%08x/0x%08x idx254_rgba=0x%08x/0x%08x/0x%08x idx255_rgba=0x%08x/0x%08x/0x%08x",
	                source ? source : "",
	                g_merged_wall_frame_id,
	                g_merged_wall_snapshot_request_frame,
	                Current_level_num,
	                ctx && ctx->valid ? ctx->seg : -1,
	                ctx && ctx->valid ? ctx->side : -1,
	                ctx && ctx->valid ? ctx->face : -1,
	                layer ? layer : "",
	                tex_num,
	                name ? name : "<none>",
	                real_flags,
	                bitmap ? bitmap->bm_flags : 0,
	                src ? src->bm_w : 0,
	                src ? src->bm_h : 0,
	                src_hash,
	                src_bytes,
	                idx254,
	                idx255,
	                idx0,
	                idxc,
	                idxlast,
	                merged_wall_palette_index_rgba(gr_palette, idx0, real_flags),
	                merged_wall_palette_index_rgba(gr_current_pal, idx0, real_flags),
	                merged_wall_palette_index_rgba(ogl_pal, idx0, real_flags),
	                merged_wall_palette_index_rgba(gr_palette, idxc, real_flags),
	                merged_wall_palette_index_rgba(gr_current_pal, idxc, real_flags),
	                merged_wall_palette_index_rgba(ogl_pal, idxc, real_flags),
	                merged_wall_palette_index_rgba(gr_palette, idxlast, real_flags),
	                merged_wall_palette_index_rgba(gr_current_pal, idxlast, real_flags),
	                merged_wall_palette_index_rgba(ogl_pal, idxlast, real_flags),
	                merged_wall_palette_index_rgba(gr_palette, 254, real_flags),
	                merged_wall_palette_index_rgba(gr_current_pal, 254, real_flags),
	                merged_wall_palette_index_rgba(ogl_pal, 254, real_flags),
	                merged_wall_palette_index_rgba(gr_palette, 255, real_flags),
	                merged_wall_palette_index_rgba(gr_current_pal, 255, real_flags),
	                merged_wall_palette_index_rgba(ogl_pal, 255, real_flags));
}

static void merged_wall_probe_log_palette_state(const char *source,
                                                const struct merged_wall_tracked_face *track)
{
	const struct android_draw_face_context *ctx;
	int overlay_tex_num = -1;
#ifdef DXX_BUILD_DESCENT_II
	const char *level_palette = Current_level_palette[0] ? Current_level_palette : "<none>";
	const char *last_palette = last_palette_loaded[0] ? last_palette_loaded : "<none>";
	const char *last_pig_palette = last_palette_loaded_pig[0] ? last_palette_loaded_pig : "<none>";
	const char *pigfile = piggy_current_pigfile();
#else
	const char *level_palette = "<d1>";
	const char *last_palette = "<d1>";
	const char *last_pig_palette = "<d1>";
	const char *pigfile = "<d1>";
#endif

	if (!track || !track->draw_ctx.valid)
		return;
	ctx = &track->draw_ctx;
	if (ctx->tmap2 != 0)
		overlay_tex_num = merged_wall_overlay_index(ctx->tmap2);

	debug_log_force(DLOG_TEXTURE,
	                "[mwall_tap_probe] kind=palette_state source=%s frame=%d request_frame=%d level=%d seg=%d side=%d face=%d game_mode=%d gr_ptr=%p current_ptr=%p ogl_ptr=%p ogl_source=%s gr_hash=0x%08x current_hash=0x%08x ogl_hash=0x%08x gr_current_same=%d ogl_gr_same=%d ogl_current_same=%d gamma=%d level_palette=%s last_palette=%s last_pig_palette=%s pig=%s",
	                source ? source : "",
	                g_merged_wall_frame_id,
	                g_merged_wall_snapshot_request_frame,
	                Current_level_num,
	                ctx->seg,
	                ctx->side,
	                ctx->face,
	                Game_mode,
	                (void *) gr_palette,
	                (void *) gr_current_pal,
	                (void *) ogl_pal,
	                merged_wall_palette_pointer_name(ogl_pal),
	                merged_wall_palette_hash(gr_palette),
	                merged_wall_palette_hash(gr_current_pal),
	                merged_wall_palette_hash(ogl_pal),
	                memcmp(gr_palette, gr_current_pal, 256 * 3) == 0,
	                ogl_pal && memcmp(ogl_pal, gr_palette, 256 * 3) == 0,
	                ogl_pal && memcmp(ogl_pal, gr_current_pal, 256 * 3) == 0,
	                (int) gr_palette_gamma,
	                level_palette,
	                last_palette,
	                last_pig_palette,
	                pigfile ? pigfile : "<none>");
	if (merged_wall_texture_valid(ctx->tmap1))
		merged_wall_probe_log_palette_bitmap(source, track, "base",
		                                     ctx->tmap1,
		                                     &GameBitmaps[Textures[ctx->tmap1].index]);
	if (merged_wall_texture_valid(overlay_tex_num))
		merged_wall_probe_log_palette_bitmap(source, track, "overlay",
		                                     overlay_tex_num,
		                                     &GameBitmaps[Textures[overlay_tex_num].index]);
	if (track->merged_bitmap)
		merged_wall_probe_log_palette_bitmap(source, track, "merged",
		                                     -1, track->merged_bitmap);
}

static void merged_wall_probe_log_layer_bitmap(const char *layer,
                                               const struct merged_wall_tracked_face *track,
                                               int tex_num)
{
	grs_bitmap *bitmap;
	ogl_texture *texture;
	const char *name;
	int real_flags;
	unsigned int mask_handle = 0;
	unsigned int bm_handle = 0;
	int avg_color = 0;
	int rowsize = 0;
	int internalformat = 0;
	unsigned int format = 0;
	int bytesu = 0;
	float prio = 0.0f;
	unsigned long numrend = 0;
	int tex_flags = 0;
	unsigned int hash = 0;
	int bytes = 0;
	int idx254 = 0;
	int idx255 = 0;

	if (!track || tex_num < 0 || tex_num >= NumTextures)
		return;
	bitmap = &GameBitmaps[Textures[tex_num].index];
	texture = bitmap ? bitmap->gltexture : NULL;
	name = piggy_game_bitmap_name(bitmap);
	real_flags = piggy_bitmap_get_flags(bitmap);
	if (bitmap && bitmap->gltexture_mask)
		mask_handle = bitmap->gltexture_mask->handle;
	if (bitmap) {
		bm_handle = bitmap->bm_handle;
		avg_color = bitmap->avg_color;
		rowsize = bitmap->bm_rowsize;
	}
	if (texture) {
		internalformat = texture->internalformat;
		format = (unsigned int) texture->format;
		bytesu = texture->bytesu;
		prio = texture->prio;
		numrend = texture->numrend;
		tex_flags = texture->flags;
	}
	hash = merged_wall_bitmap_hash(bitmap, &bytes, &idx254, &idx255);
	debug_log_force(DLOG_TEXTURE,
	                "[mwall_tap_probe] kind=tex frame=%d request_frame=%d seg=%d side=%d face=%d layer=%s tex=%d name=%s real_flags=0x%x bm_flags=0x%x bm_handle=%u avg=%d rowsize=%d bitmap_ptr=%p texture_ptr=%p handle=%u is_png=%d w=%d h=%d tw=%d th=%d lw=%d bytes=%d bytesu=%d mip=%d wrap=%d u=%.3f v=%.3f internal=0x%x format=0x%x prio=%.3f numrend=%lu tex_flags=0x%x src_hash=0x%08x src_bytes=%d src_254=%d src_255=%d mask_handle=%u",
	                g_merged_wall_frame_id,
	                g_merged_wall_snapshot_request_frame,
	                track->draw_ctx.valid ? track->draw_ctx.seg : -1,
	                track->draw_ctx.valid ? track->draw_ctx.side : -1,
	                track->draw_ctx.valid ? track->draw_ctx.face : -1,
	                layer ? layer : "",
	                tex_num,
	                name ? name : "<none>",
	                real_flags,
	                bitmap ? bitmap->bm_flags : 0,
	                bm_handle,
	                avg_color,
	                rowsize,
	                (void *) bitmap,
	                texture ? (void *) texture : NULL,
	                texture ? texture->handle : 0,
	                texture ? texture->is_png : -1,
	                texture ? texture->w : 0,
	                texture ? texture->h : 0,
	                texture ? texture->tw : 0,
	                texture ? texture->th : 0,
	                texture ? texture->lw : 0,
	                texture ? texture->bytes : 0,
	                bytesu,
	                texture ? texture->has_mipmaps : -1,
	                texture ? texture->wrapstate : -1,
	                texture ? texture->u : 0.0f,
	                texture ? texture->v : 0.0f,
	                internalformat,
	                format,
	                prio,
	                numrend,
	                tex_flags,
	                hash,
	                bytes,
	                idx254,
	                idx255,
	                mask_handle);
	merged_wall_log_texture_gpu_readback(layer ? layer : "tap_layer", bitmap, 1);
}

static void merged_wall_probe_log_merged_bitmap(const struct merged_wall_tracked_face *track)
{
	grs_bitmap *bitmap;
	ogl_texture *texture;
	grs_bitmap *source_bot = NULL;
	grs_bitmap *source_ovl = NULL;
	int internalformat = 0;
	unsigned int format = 0;
	int bytesu = 0;
	int tex_flags = 0;
	unsigned int bot_hash = 0, ovl_hash = 0, merged_hash = 0;
	int bot_bytes = 0, ovl_bytes = 0, merged_bytes = 0;
	int bot_254 = 0, bot_255 = 0, ovl_254 = 0, ovl_255 = 0;
	int merged_254 = 0, merged_255 = 0;

	if (!track || !track->merged_bitmap)
		return;
	bitmap = track->merged_bitmap;
	texture = bitmap->gltexture;
	if (track->draw_ctx.valid && track->draw_ctx.tmap1 >= 0 && track->draw_ctx.tmap1 < NumTextures)
		source_bot = &GameBitmaps[Textures[track->draw_ctx.tmap1].index];
	if (track->draw_ctx.valid && track->draw_ctx.tmap2 != 0) {
		int overlay = merged_wall_overlay_index(track->draw_ctx.tmap2);
		if (overlay >= 0 && overlay < NumTextures)
			source_ovl = &GameBitmaps[Textures[overlay].index];
	}
	if (texture) {
		internalformat = texture->internalformat;
		format = (unsigned int) texture->format;
		bytesu = texture->bytesu;
		tex_flags = texture->flags;
	}
	bot_hash = merged_wall_bitmap_hash(source_bot, &bot_bytes, &bot_254, &bot_255);
	ovl_hash = merged_wall_bitmap_hash(source_ovl, &ovl_bytes, &ovl_254, &ovl_255);
	merged_hash = merged_wall_bitmap_hash(bitmap, &merged_bytes, &merged_254,
	                                      &merged_255);
	debug_log_force(DLOG_TEXTURE,
	                "[mwall_tap_probe] kind=merged frame=%d request_frame=%d seg=%d side=%d face=%d route=%s merge_impl=%s slot=%d bitmap_ptr=%p texture_ptr=%p source_bot_ptr=%p source_ovl_ptr=%p handle=%u is_png=%d w=%d h=%d tw=%d th=%d lw=%d bytes=%d bytesu=%d mip=%d wrap=%d u=%.3f v=%.3f internal=0x%x format=0x%x tex_flags=0x%x bot_hash=0x%08x bot_bytes=%d bot_254=%d bot_255=%d ovl_hash=0x%08x ovl_bytes=%d ovl_254=%d ovl_255=%d merged_hash=0x%08x merged_bytes=%d merged_254=%d merged_255=%d",
	                g_merged_wall_frame_id,
	                g_merged_wall_snapshot_request_frame,
	                track->draw_ctx.valid ? track->draw_ctx.seg : -1,
	                track->draw_ctx.valid ? track->draw_ctx.side : -1,
	                track->draw_ctx.valid ? track->draw_ctx.face : -1,
	                track->route,
	                track->merge_impl,
	                track->merged_slot,
	                (void *) bitmap,
	                texture ? (void *) texture : NULL,
	                (void *) source_bot,
	                (void *) source_ovl,
	                texture ? texture->handle : 0,
	                texture ? texture->is_png : -1,
	                texture ? texture->w : 0,
	                texture ? texture->h : 0,
	                texture ? texture->tw : 0,
	                texture ? texture->th : 0,
	                texture ? texture->lw : 0,
	                texture ? texture->bytes : 0,
	                bytesu,
	                texture ? texture->has_mipmaps : -1,
	                texture ? texture->wrapstate : -1,
	                texture ? texture->u : 0.0f,
	                texture ? texture->v : 0.0f,
	                internalformat,
	                format,
	                tex_flags,
	                bot_hash,
	                bot_bytes,
	                bot_254,
	                bot_255,
	                ovl_hash,
	                ovl_bytes,
	                ovl_254,
	                ovl_255,
	                merged_hash,
	                merged_bytes,
	                merged_254,
	                merged_255);
	if (track->draw_ctx.valid)
		merged_wall_log_merge_reference("tap_probe", "tap",
		                                track->draw_ctx.tmap1,
		                                track->draw_ctx.tmap2,
		                                source_bot, source_ovl, bitmap,
		                                track->merged_slot, track->orient);
	merged_wall_log_texture_gpu_readback("tap_merged", bitmap, 1);
}

static int merged_wall_probe_draw_face_hit_kind(
    const struct merged_wall_probe_draw_face *face)
{
	if (!face)
		return MERGED_WALL_PROBE_HIT_NONE;
	if (face->polygon_hit)
		return MERGED_WALL_PROBE_HIT_POLYGON;
	if (face->bbox_hit)
		return MERGED_WALL_PROBE_HIT_BBOX;
	if (face->projected_hit)
		return MERGED_WALL_PROBE_HIT_PROJECTED;
	return MERGED_WALL_PROBE_HIT_NONE;
}

static void merged_wall_probe_track_from_draw_face(
    struct merged_wall_tracked_face *track,
    const struct merged_wall_probe_draw_face *face)
{
	if (!track || !face)
		return;
	memset(track, 0, sizeof(*track));
	track->render_pass = face->render_pass;
	track->draw_seq = face->draw_seq;
	track->draw_order = face->draw_order;
	track->nv = face->nv;
	track->projected_count = face->projected_count;
	track->draw_ctx = face->draw_ctx;
	track->bbox_valid = face->bbox_valid;
	track->projected_bbox_valid = face->projected_bbox_valid;
	track->min_sx = face->min_sx;
	track->max_sx = face->max_sx;
	track->min_sy = face->min_sy;
	track->max_sy = face->max_sy;
	track->bbox_area = face->bbox_area;
	track->projected_min_sx = face->projected_min_sx;
	track->projected_max_sx = face->projected_max_sx;
	track->projected_min_sy = face->projected_min_sy;
	track->projected_max_sy = face->projected_max_sy;
	track->projected_bbox_area = face->projected_bbox_area;
	memcpy(track->pts, face->pts, sizeof(track->pts));
	memcpy(track->pt_projected, face->pt_projected, sizeof(track->pt_projected));
	memcpy(track->pt_sx, face->pt_sx, sizeof(track->pt_sx));
	memcpy(track->pt_sy, face->pt_sy, sizeof(track->pt_sy));
	memcpy(track->uvl, face->uvl, sizeof(track->uvl));
	track->orient = face->draw_ctx.valid ? ((face->draw_ctx.tmap2 >> 14) & 3) : 0;
	track->merged_bitmap = face->draw_ctx.valid && face->draw_ctx.tmap2 != 0 ? face->bitmap : NULL;
	track->merged_slot = -1;
	merged_wall_copy_string(track->route, sizeof(track->route), "drawn_face");
	merged_wall_copy_string(track->merge_impl, sizeof(track->merge_impl),
	                        face->draw_ctx.valid && face->draw_ctx.tmap2 != 0
	                            ? "drawn_tmap2_bitmap"
	                            : "drawn_single_bitmap");
}

static void merged_wall_probe_track_from_context(
    struct merged_wall_tracked_face *track,
    const struct android_draw_face_context *ctx,
    const char *route,
    const char *merge_impl)
{
	if (!track || !ctx)
		return;
	memset(track, 0, sizeof(*track));
	track->draw_ctx = *ctx;
	track->nv = ctx->nv;
	track->orient = ctx->valid ? ((ctx->tmap2 >> 14) & 3) : 0;
	track->merged_slot = -1;
	merged_wall_copy_string(track->route, sizeof(track->route), route ? route : "");
	merged_wall_copy_string(track->merge_impl, sizeof(track->merge_impl),
	                        merge_impl ? merge_impl : "");
}

static int merged_wall_probe_log_geometry_ray(float canvas_center_x,
                                              float canvas_center_y,
                                              struct merged_wall_tracked_face *out_track,
                                              const char **out_status)
{
	struct merged_wall_tracked_face track;
	struct android_draw_face_context ctx;
	vms_vector origin, end, hit_point, uv_point;
	fvi_query fq;
	fvi_info hit;
	enum merged_wall_geometry_outcome outcome;
	segment *segp = NULL;
	struct side *sidep = NULL;
	fix u = 0, v = 0, l = 0;
	int viewer_objnum = -1;
	int startseg = -1;
	int eye_startseg = -1;
	int fate = HIT_NONE;
	int segnum = -1;
	int sidenum = -1;
	const char *status = "no_hit";
	const char *route = "geometry_ray";
	int overlay_tex_num = -1;

	memset(&track, 0, sizeof(track));
	if (out_track)
		memset(out_track, 0, sizeof(*out_track));
	if (out_status)
		*out_status = status;
	if (!Viewer) {
		status = "no_viewer";
		if (out_status)
			*out_status = status;
		debug_log_force(DLOG_TEXTURE,
		                "[mwall_tap_probe] kind=geometry_face status=no_viewer frame=%d request_frame=%d canvas_center=%.1f/%.1f",
		                g_merged_wall_frame_id,
		                g_merged_wall_snapshot_request_frame,
		                canvas_center_x,
		                canvas_center_y);
		return 0;
	}

	origin = Viewer_eye;
	eye_startseg = find_point_seg(&origin, Viewer->segnum);
	if (eye_startseg >= 0)
		startseg = eye_startseg;
	else {
		origin = Viewer->pos;
		startseg = find_point_seg(&origin, Viewer->segnum);
	}
	vm_vec_scale_add(&end, &origin, &Viewer->orient.fvec, i2f(5000));
	memset(&fq, 0, sizeof(fq));
	memset(&hit, 0, sizeof(hit));
	viewer_objnum = (int) (Viewer - Objects);
	fq.p0 = &origin;
	fq.p1 = &end;
	fq.startseg = startseg >= 0 ? startseg : Viewer->segnum;
	fq.rad = 0;
	fq.thisobjnum = (short) viewer_objnum;
	fq.ignore_obj_list = NULL;
	fq.flags = FQ_GET_SEGLIST;
	fate = find_vector_intersection(&fq, &hit);
	outcome = merged_wall_classify_geometry_hit(fate, HIT_WALL, HIT_BAD_P0,
	                                            hit.hit_side_seg, hit.hit_seg, hit.hit_side,
	                                            Highest_segment_index, MAX_SIDES_PER_SEGMENT,
	                                            &segnum, &sidenum);
	status = merged_wall_geometry_outcome_status(outcome);
	if (out_status)
		*out_status = status;
	if (outcome != MERGED_WALL_GEOMETRY_WALL_HIT) {
		debug_log_force(DLOG_TEXTURE,
		                "[mwall_tap_probe] kind=geometry_face status=%s frame=%d request_frame=%d canvas_center=%.1f/%.1f fate=%d viewer_obj=%d viewer_seg=%d eye_startseg=%d startseg=%d n_segs=%d hit_seg=%d hit_side_seg=%d hit_side=%d origin=%d/%d/%d end=%d/%d/%d fvec=%d/%d/%d",
		                status,
		                g_merged_wall_frame_id,
		                g_merged_wall_snapshot_request_frame,
		                canvas_center_x,
		                canvas_center_y,
		                fate,
		                viewer_objnum,
		                Viewer->segnum,
		                eye_startseg,
		                fq.startseg,
		                hit.n_segs,
		                hit.hit_seg,
		                hit.hit_side_seg,
		                hit.hit_side,
		                (int) origin.x,
		                (int) origin.y,
		                (int) origin.z,
		                (int) end.x,
		                (int) end.y,
		                (int) end.z,
		                (int) Viewer->orient.fvec.x,
		                (int) Viewer->orient.fvec.y,
		                (int) Viewer->orient.fvec.z);
		return 0;
	}
	hit_point = hit.hit_pnt;

	segp = &Segments[segnum];
	sidep = &segp->sides[sidenum];
	memset(&ctx, 0, sizeof(ctx));
	ctx.valid = 1;
	ctx.seg = segnum;
	ctx.side = sidenum;
	ctx.face = 0;
	ctx.child = segp->children[sidenum];
	ctx.side_type = sidep->type;
	ctx.nv = 4;
	ctx.wid_flags = WALL_IS_DOORWAY(segp, sidenum);
	ctx.tmap1 = sidep->tmap_num;
	ctx.tmap2 = sidep->tmap_num2;
	merged_wall_probe_track_from_context(&track, &ctx, route,
	                                     "fvi_wall");
	if (ctx.tmap2 != 0)
		overlay_tex_num = merged_wall_overlay_index(ctx.tmap2);

	uv_point = hit_point;
	find_hitpoint_uv(&u, &v, &l, &uv_point, segp, sidenum, 0);
	debug_log_force(DLOG_TEXTURE,
	                "[mwall_tap_probe] kind=geometry_face status=%s frame=%d request_frame=%d level=%d canvas_center=%.1f/%.1f fate=%d viewer_obj=%d viewer_seg=%d eye_startseg=%d startseg=%d n_segs=%d seg=%d side=%d face=%d child=%d side_type=%d wid=%d tmap1=%d tmap2=0x%x overlay=%d origin=%d/%d/%d end=%d/%d/%d fvec=%d/%d/%d hit=%d/%d/%d u_fix=%d v_fix=%d l_fix=%d u=%.6f v=%.6f",
	                status,
	                g_merged_wall_frame_id,
	                g_merged_wall_snapshot_request_frame,
	                Current_level_num,
	                canvas_center_x,
	                canvas_center_y,
	                fate,
	                viewer_objnum,
	                Viewer->segnum,
	                eye_startseg,
	                fq.startseg,
	                hit.n_segs,
	                ctx.seg,
	                ctx.side,
	                ctx.face,
	                ctx.child,
	                ctx.side_type,
	                ctx.wid_flags,
	                ctx.tmap1,
	                ctx.tmap2,
	                overlay_tex_num,
	                (int) origin.x,
	                (int) origin.y,
	                (int) origin.z,
	                (int) end.x,
	                (int) end.y,
	                (int) end.z,
	                (int) Viewer->orient.fvec.x,
	                (int) Viewer->orient.fvec.y,
	                (int) Viewer->orient.fvec.z,
	                (int) hit_point.x,
	                (int) hit_point.y,
	                (int) hit_point.z,
	                (int) u,
	                (int) v,
	                (int) l,
	                f2fl(u),
	                f2fl(v));
	merged_wall_probe_log_side_state("geometry", &track);
	merged_wall_probe_log_palette_state("geometry", &track);
	if (merged_wall_texture_valid(ctx.tmap1))
		merged_wall_probe_log_layer_bitmap("base_geometry", &track, ctx.tmap1);
	if (merged_wall_texture_valid(overlay_tex_num))
		merged_wall_probe_log_layer_bitmap("overlay_geometry", &track, overlay_tex_num);
	if (out_track)
		*out_track = track;
	return 1;
}

static void merged_wall_probe_log_draw_faces(float canvas_center_x,
                                             float canvas_center_y,
                                             int canvas_x, int canvas_y,
                                             int canvas_w, int canvas_h)
{
	struct merged_wall_tracked_face track;
	const char *level_name = Current_level_name[0] ? Current_level_name : "<none>";
	int polygon_hits = 0;
	int bbox_hits = 0;
	int projected_hits = 0;
	int nearest = 0;
	const char *status;
	int logged;
	int i;

	for (i = 0; i < merged_wall_probe_draw_face_count; i++) {
		const struct merged_wall_probe_draw_face *face = &merged_wall_probe_draw_faces[i];

		if (face->polygon_hit)
			polygon_hits++;
		else if (face->bbox_hit)
			bbox_hits++;
		else if (face->projected_hit)
			projected_hits++;
		else
			nearest++;
	}
	logged = merged_wall_probe_draw_face_count < MERGED_WALL_PROBE_DRAW_FACE_LOG_MAX
	             ? merged_wall_probe_draw_face_count
	             : MERGED_WALL_PROBE_DRAW_FACE_LOG_MAX;
	status = merged_wall_probe_draw_face_count <= 0 ? "none"
	                                                : (polygon_hits || bbox_hits || projected_hits ? "hit" : "nearest");
	debug_log_force(DLOG_TEXTURE,
	                "[mwall_tap_probe] kind=all_face status=%s frame=%d request_frame=%d level=%d name=%s canvas=%d,%d,%d,%d canvas_center=%.1f/%.1f seen=%d retained=%d logged=%d polygon_hits=%d bbox_hits=%d projected_hits=%d nearest=%d",
	                status,
	                g_merged_wall_frame_id,
	                g_merged_wall_snapshot_request_frame,
	                Current_level_num,
	                level_name,
	                canvas_x,
	                canvas_y,
	                canvas_w,
	                canvas_h,
	                canvas_center_x,
	                canvas_center_y,
	                merged_wall_probe_draw_face_seen_count,
	                merged_wall_probe_draw_face_count,
	                logged,
	                polygon_hits,
	                bbox_hits,
	                projected_hits,
	                nearest);
	for (i = 0; i < logged; i++) {
		const struct merged_wall_probe_draw_face *face = &merged_wall_probe_draw_faces[i];
		grs_bitmap *bitmap = face->bitmap;
		ogl_texture *texture = bitmap ? bitmap->gltexture : NULL;
		const char *bitmap_name = bitmap ? piggy_game_bitmap_name(bitmap) : NULL;
		unsigned int hash;
		int bytes = 0;
		int idx254 = 0;
		int idx255 = 0;
		int overlay_tex_num = face->draw_ctx.valid && face->draw_ctx.tmap2 != 0
		                          ? merged_wall_overlay_index(face->draw_ctx.tmap2)
		                          : -1;

		hash = merged_wall_bitmap_hash(bitmap, &bytes, &idx254, &idx255);
		debug_log_force(DLOG_TEXTURE,
		                "[mwall_tap_probe] kind=all_face_candidate rank=%d hit=%s polygon_hit=%d bbox_hit=%d projected_hit=%d dist2=%.1f frame=%d request_frame=%d level=%d name=%s pass=%d seq=%d order=%d seg=%d side=%d face=%d child=%d side_type=%d wid=%d tmap1=%d tmap2=0x%x overlay=%d nv=%d projected=%d box=%.1f..%.1f/%.1f..%.1f area=%.1f proj_box=%.1f..%.1f/%.1f..%.1f proj_area=%.1f depth=%d..%d avg=%d bitmap=%s ptr=%p hash=0x%08x bytes=%d idx254=%d idx255=%d bm_handle=%u gl=%u is_png=%d tex_size=%dx%d",
		                i + 1,
		                merged_wall_probe_hit_kind_name_local(
		                    merged_wall_probe_draw_face_hit_kind(face)),
		                face->polygon_hit,
		                face->bbox_hit,
		                face->projected_hit,
		                face->dist2,
		                g_merged_wall_frame_id,
		                g_merged_wall_snapshot_request_frame,
		                Current_level_num,
		                level_name,
		                face->render_pass,
		                face->draw_seq,
		                face->draw_order,
		                face->draw_ctx.valid ? face->draw_ctx.seg : -1,
		                face->draw_ctx.valid ? face->draw_ctx.side : -1,
		                face->draw_ctx.valid ? face->draw_ctx.face : -1,
		                face->draw_ctx.valid ? face->draw_ctx.child : -1,
		                face->draw_ctx.valid ? face->draw_ctx.side_type : -1,
		                face->draw_ctx.valid ? face->draw_ctx.wid_flags : 0,
		                face->draw_ctx.valid ? face->draw_ctx.tmap1 : -1,
		                face->draw_ctx.valid ? face->draw_ctx.tmap2 : 0,
		                overlay_tex_num,
		                face->nv,
		                face->projected_count,
		                face->min_sx,
		                face->max_sx,
		                face->min_sy,
		                face->max_sy,
		                face->bbox_area,
		                face->projected_min_sx,
		                face->projected_max_sx,
		                face->projected_min_sy,
		                face->projected_max_sy,
		                face->projected_bbox_area,
		                (int) face->depth_min,
		                (int) face->depth_max,
		                (int) face->depth_avg,
		                bitmap_name ? bitmap_name : "<none>",
		                (void *) bitmap,
		                hash,
		                bytes,
		                idx254,
		                idx255,
		                bitmap ? bitmap->bm_handle : 0,
		                texture ? texture->handle : 0,
		                texture ? texture->is_png : -1,
		                texture ? texture->w : 0,
		                texture ? texture->h : 0);
		if (i == 0) {
			merged_wall_probe_track_from_draw_face(&track, face);
			merged_wall_probe_log_side_state("all_face", &track);
			merged_wall_probe_log_palette_state("all_face", &track);
			if (face->draw_ctx.valid)
				merged_wall_probe_log_layer_bitmap("base_all_face",
				                                   &track, face->draw_ctx.tmap1);
			if (overlay_tex_num >= 0)
				merged_wall_probe_log_layer_bitmap("overlay_all_face",
				                                   &track, overlay_tex_num);
			if (bitmap)
				merged_wall_log_texture_gpu_readback("all_face_draw", bitmap, 1);
			if (track.merged_bitmap)
				merged_wall_probe_log_merged_bitmap(&track);
		}
	}
}

struct merged_wall_probe_candidate {
	int valid;
	int index;
	int hit_kind;
	int polygon_hit;
	int bbox_hit;
	float dist2;
	float box_area;
	int draw_order;
	int draw_seq;
};

static int merged_wall_probe_candidate_better(const struct merged_wall_probe_candidate *candidate,
                                              const struct merged_wall_probe_candidate *best)
{
	if (!candidate || !candidate->valid)
		return 0;
	if (!best || !best->valid)
		return 1;
	if (candidate->hit_kind != best->hit_kind)
		return candidate->hit_kind > best->hit_kind;
	if (candidate->draw_order != best->draw_order)
		return candidate->draw_order > best->draw_order;
	if (candidate->draw_seq != best->draw_seq)
		return candidate->draw_seq > best->draw_seq;
	if (candidate->dist2 < best->dist2 - 0.5f)
		return 1;
	if (candidate->dist2 > best->dist2 + 0.5f)
		return 0;
	if (candidate->box_area < best->box_area - 0.5f)
		return 1;
	if (candidate->box_area > best->box_area + 0.5f)
		return 0;
	return candidate->index < best->index;
}

static void merged_wall_log_tap_probe(float canvas_center_x, float canvas_center_y,
                                      int canvas_x, int canvas_y, int canvas_w, int canvas_h,
                                      int screen_w, int screen_h,
                                      const int *selected, int selected_count)
{
	struct merged_wall_probe_candidate best = { 0 };
	struct merged_wall_tracked_face geometry_track;
	const char *level_name = Current_level_name[0] ? Current_level_name : "<none>";
	const char *geometry_status = "no_hit";
	int geometry_hit;
	int i;

	merged_wall_probe_log_draw_faces(canvas_center_x, canvas_center_y,
	                                 canvas_x, canvas_y, canvas_w, canvas_h);
	geometry_hit = merged_wall_probe_log_geometry_ray(canvas_center_x,
	                                                  canvas_center_y,
	                                                  &geometry_track,
	                                                  &geometry_status);

	for (i = 0; i < merged_wall_tracked_face_count; i++) {
		const struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[i];
		struct merged_wall_probe_candidate candidate = { 0 };
		int polygon_hit = merged_wall_projected_polygon_valid(track->nv, track->projected_count) &&
		                  merged_wall_projected_polygon_contains_point(track->nv,
		                                                               track->pt_projected,
		                                                               track->pt_sx,
		                                                               track->pt_sy,
		                                                               canvas_center_x,
		                                                               canvas_center_y);
		int bbox_hit = track->bbox_valid &&
		               merged_wall_bbox_contains_point(canvas_center_x, canvas_center_y,
		                                               track->min_sx, track->max_sx,
		                                               track->min_sy, track->max_sy);
		int projected_hit = !bbox_hit && track->projected_bbox_valid &&
		                    merged_wall_bbox_contains_point(canvas_center_x, canvas_center_y,
		                                                    track->projected_min_sx, track->projected_max_sx,
		                                                    track->projected_min_sy, track->projected_max_sy);

		if (!polygon_hit && !bbox_hit && !projected_hit)
			continue;
		candidate.valid = 1;
		candidate.index = i;
		candidate.polygon_hit = polygon_hit;
		candidate.bbox_hit = bbox_hit;
		candidate.hit_kind = polygon_hit ? MERGED_WALL_PROBE_HIT_POLYGON
		                                 : (bbox_hit ? MERGED_WALL_PROBE_HIT_BBOX : MERGED_WALL_PROBE_HIT_PROJECTED);
		if (track->bbox_valid) {
			candidate.dist2 = merged_wall_bbox_distance_sq(canvas_center_x, canvas_center_y,
			                                               track->min_sx, track->max_sx,
			                                               track->min_sy, track->max_sy);
			candidate.box_area = track->bbox_area;
		} else {
			candidate.dist2 = merged_wall_bbox_distance_sq(canvas_center_x, canvas_center_y,
			                                               track->projected_min_sx, track->projected_max_sx,
			                                               track->projected_min_sy, track->projected_max_sy);
			candidate.box_area = track->projected_bbox_area;
		}
		candidate.draw_order = track->draw_order;
		candidate.draw_seq = track->draw_seq;
		if (merged_wall_probe_candidate_better(&candidate, &best))
			best = candidate;
	}

	g_merged_wall_probe_result.valid = 1;
	g_merged_wall_probe_result.frame_id = g_merged_wall_frame_id;
	g_merged_wall_probe_result.request_frame = g_merged_wall_snapshot_request_frame;
	if (!best.valid) {
		int fallback_index = -1;
		const char *fallback_source = "none";
		const char *fallback_box_kind = "none";
		float fallback_min_sx = 0.0f, fallback_max_sx = 0.0f;
		float fallback_min_sy = 0.0f, fallback_max_sy = 0.0f;
		float fallback_area = 0.0f;
		float fallback_dist2 = 0.0f;
		const char *face_status = geometry_hit ? "geometry_hit" : geometry_status;

		for (i = 0; i < selected_count; i++) {
			if (selected[i] >= 0 && selected[i] < merged_wall_tracked_face_count) {
				fallback_index = selected[i];
				fallback_source = "selected";
				break;
			}
		}
		if (fallback_index < 0) {
			for (i = 0; i < merged_wall_tracked_face_count; i++) {
				const struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[i];
				const char *box_kind = "none";
				float min_sx = 0.0f, max_sx = 0.0f, min_sy = 0.0f, max_sy = 0.0f;
				float box_area = 0.0f;
				float dist2;

				if (!merged_wall_get_track_overlap_box(track, &min_sx, &max_sx, &min_sy, &max_sy,
				                                       &box_area, &box_kind))
					continue;
				dist2 = merged_wall_bbox_distance_sq(canvas_center_x, canvas_center_y,
				                                     min_sx, max_sx, min_sy, max_sy);
				if (fallback_index < 0 || dist2 < fallback_dist2 - 0.5f ||
				    (dist2 <= fallback_dist2 + 0.5f &&
				     (track->draw_order > merged_wall_tracked_faces[fallback_index].draw_order ||
				      (track->draw_order == merged_wall_tracked_faces[fallback_index].draw_order &&
				       track->draw_seq > merged_wall_tracked_faces[fallback_index].draw_seq)))) {
					fallback_index = i;
					fallback_source = "nearest";
					fallback_box_kind = box_kind;
					fallback_min_sx = min_sx;
					fallback_max_sx = max_sx;
					fallback_min_sy = min_sy;
					fallback_max_sy = max_sy;
					fallback_area = box_area;
					fallback_dist2 = dist2;
				}
			}
		}
		if (fallback_index >= 0 && fallback_index < merged_wall_tracked_face_count &&
		    !merged_wall_get_track_overlap_box(&merged_wall_tracked_faces[fallback_index],
		                                       &fallback_min_sx, &fallback_max_sx,
		                                       &fallback_min_sy, &fallback_max_sy,
		                                       &fallback_area, &fallback_box_kind))
			fallback_box_kind = "none";
		merged_wall_copy_string(g_merged_wall_probe_result.status,
		                        sizeof(g_merged_wall_probe_result.status), face_status);
		if (geometry_hit) {
			g_merged_wall_probe_result.hit_kind = MERGED_WALL_PROBE_HIT_POLYGON;
			g_merged_wall_probe_result.center_polygon_hit = 1;
			g_merged_wall_probe_result.center_bbox_hit = 1;
			g_merged_wall_probe_result.seg = geometry_track.draw_ctx.seg;
			g_merged_wall_probe_result.side = geometry_track.draw_ctx.side;
			g_merged_wall_probe_result.face = geometry_track.draw_ctx.face;
			g_merged_wall_probe_result.child = geometry_track.draw_ctx.child;
			g_merged_wall_probe_result.wid_flags = geometry_track.draw_ctx.wid_flags;
			g_merged_wall_probe_result.tmap1 = geometry_track.draw_ctx.tmap1;
			g_merged_wall_probe_result.tmap2 = geometry_track.draw_ctx.tmap2;
			g_merged_wall_probe_result.orient = geometry_track.orient;
			merged_wall_copy_string(g_merged_wall_probe_result.route,
			                        sizeof(g_merged_wall_probe_result.route),
			                        geometry_track.route);
			merged_wall_copy_string(g_merged_wall_probe_result.merge_impl,
			                        sizeof(g_merged_wall_probe_result.merge_impl),
			                        geometry_track.merge_impl);
		} else {
			merged_wall_copy_string(g_merged_wall_probe_result.route,
			                        sizeof(g_merged_wall_probe_result.route), "");
			merged_wall_copy_string(g_merged_wall_probe_result.merge_impl,
			                        sizeof(g_merged_wall_probe_result.merge_impl), "");
		}
		merged_wall_copy_string(g_merged_wall_probe_result.ovl_flip_axis,
		                        sizeof(g_merged_wall_probe_result.ovl_flip_axis), "none");
		merged_wall_copy_string(g_merged_wall_probe_result.flip_screen_axis,
		                        sizeof(g_merged_wall_probe_result.flip_screen_axis), "none");
		debug_log_force(DLOG_TEXTURE,
		                "[mwall_tap_probe] kind=face status=%s render_center_hit=0 geometry_hit=%d frame=%d request_frame=%d level=%d name=%s canvas=%d,%d,%d,%d canvas_center=%.1f/%.1f tracked=%d selected=%d geometry_seg=%d geometry_side=%d geometry_face=%d geometry_tmap1=%d geometry_tmap2=0x%x",
		                face_status,
		                geometry_hit,
		                g_merged_wall_frame_id,
		                g_merged_wall_snapshot_request_frame,
		                Current_level_num,
		                level_name,
		                canvas_x,
		                canvas_y,
		                canvas_w,
		                canvas_h,
		                canvas_center_x,
		                canvas_center_y,
		                merged_wall_tracked_face_count,
		                selected_count,
		                geometry_hit ? geometry_track.draw_ctx.seg : -1,
		                geometry_hit ? geometry_track.draw_ctx.side : -1,
		                geometry_hit ? geometry_track.draw_ctx.face : -1,
		                geometry_hit ? geometry_track.draw_ctx.tmap1 : -1,
		                geometry_hit ? geometry_track.draw_ctx.tmap2 : 0);
		if (fallback_index >= 0 && fallback_index < merged_wall_tracked_face_count) {
			const struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[fallback_index];
			int overlay_tex_num = track->draw_ctx.valid && track->draw_ctx.tmap2 != 0
			                          ? merged_wall_overlay_index(track->draw_ctx.tmap2)
			                          : -1;
			int center_hit = fallback_area > 0.0f &&
			                 merged_wall_bbox_contains_point(canvas_center_x, canvas_center_y,
			                                                 fallback_min_sx, fallback_max_sx,
			                                                 fallback_min_sy, fallback_max_sy);
			int rank = merged_wall_selected_rank(selected, selected_count, fallback_index);

			fallback_dist2 = fallback_area > 0.0f
			                     ? merged_wall_bbox_distance_sq(canvas_center_x, canvas_center_y,
			                                                    fallback_min_sx, fallback_max_sx,
			                                                    fallback_min_sy, fallback_max_sy)
			                     : 0.0f;
			debug_log_force(DLOG_TEXTURE,
			                "[mwall_tap_probe] kind=face_candidate status=%s source=%s rank=%d center_hit=%d dist2=%.1f frame=%d request_frame=%d level=%d name=%s box_kind=%s box=%.1f..%.1f/%.1f..%.1f area=%.1f pass=%d seq=%d order=%d seg=%d side=%d face=%d child=%d side_type=%d wid=%d tmap1=%d tmap2=0x%x overlay=%d orient=%d route=%s merge_impl=%s reason=%s",
			                face_status,
			                fallback_source,
			                rank >= 0 ? rank + 1 : 0,
			                center_hit,
			                fallback_dist2,
			                g_merged_wall_frame_id,
			                g_merged_wall_snapshot_request_frame,
			                Current_level_num,
			                level_name,
			                fallback_box_kind,
			                fallback_min_sx,
			                fallback_max_sx,
			                fallback_min_sy,
			                fallback_max_sy,
			                fallback_area,
			                track->render_pass,
			                track->draw_seq,
			                track->draw_order,
			                track->draw_ctx.valid ? track->draw_ctx.seg : -1,
			                track->draw_ctx.valid ? track->draw_ctx.side : -1,
			                track->draw_ctx.valid ? track->draw_ctx.face : -1,
			                track->draw_ctx.valid ? track->draw_ctx.child : -1,
			                track->draw_ctx.valid ? track->draw_ctx.side_type : -1,
			                track->draw_ctx.valid ? track->draw_ctx.wid_flags : 0,
			                track->draw_ctx.valid ? track->draw_ctx.tmap1 : -1,
			                track->draw_ctx.valid ? track->draw_ctx.tmap2 : 0,
			                overlay_tex_num,
			                track->orient,
			                track->route,
			                track->merge_impl,
			                track->decision_reason);
			merged_wall_probe_log_side_state("fallback", track);
			merged_wall_probe_log_palette_state("fallback", track);
			merged_wall_probe_capture_render_sample(track, screen_w, screen_h);
			if (track->draw_ctx.valid)
				merged_wall_probe_log_layer_bitmap("base_fallback", track, track->draw_ctx.tmap1);
			if (overlay_tex_num >= 0)
				merged_wall_probe_log_layer_bitmap("overlay_fallback", track, overlay_tex_num);
			merged_wall_probe_log_merged_bitmap(track);
		}
		return;
	}

	{
		const struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[best.index];
		grs_bitmap *base_bitmap = NULL;
		int base_tex_num = -1;
		int overlay_tex_num = -1;
		int rank = merged_wall_selected_rank(selected, selected_count, best.index);
		float u_min = 0.0f, u_max = 0.0f, v_min = 0.0f, v_max = 0.0f;
		float u_span;
		float v_span;
		float u_shift_hint;
		float v_shift_hint;
		float cached_anchor_u = 0.0f, cached_anchor_v = 0.0f;
		float legacy_anchor_u = 0.0f, legacy_anchor_v = 0.0f;
		float active_anchor_u = 0.0f, active_anchor_v = 0.0f;
		float active_anchor_w = 64.0f, active_anchor_h = 64.0f;
		int route_agree;
		char sample_path[24];
		char flip_axis[8];
		char u_axis[16];
		char v_axis[16];
		char flip_screen_axis[16];

		if (track->draw_ctx.valid && track->draw_ctx.tmap1 >= 0 && track->draw_ctx.tmap1 < NumTextures) {
			base_tex_num = track->draw_ctx.tmap1;
			base_bitmap = &GameBitmaps[Textures[base_tex_num].index];
		}
		if (track->draw_ctx.valid && track->draw_ctx.tmap2 != 0)
			overlay_tex_num = merged_wall_overlay_index(track->draw_ctx.tmap2);
		for (i = 0; i < track->nv; i++) {
			float u = f2fl(track->uvl[i].u);
			float v = f2fl(track->uvl[i].v);

			if (i == 0) {
				u_min = u_max = u;
				v_min = v_max = v;
			} else {
				if (u < u_min) u_min = u;
				if (u > u_max) u_max = u;
				if (v < v_min) v_min = v;
				if (v > v_max) v_max = v;
			}
		}
		u_span = u_max - u_min;
		v_span = v_max - v_min;
		u_shift_hint = merged_wall_wrap_unit(u_min);
		v_shift_hint = merged_wall_wrap_unit(v_min);
		merged_wall_probe_map_overlay_uv_for_route("merge_cached", track->orient,
		                                           u_shift_hint, v_shift_hint,
		                                           &cached_anchor_u, &cached_anchor_v);
		merged_wall_probe_map_overlay_uv_for_route("old_texmerge", track->orient,
		                                           u_shift_hint, v_shift_hint,
		                                           &legacy_anchor_u, &legacy_anchor_v);
		merged_wall_probe_map_overlay_uv_for_route(track->route, track->orient,
		                                           u_shift_hint, v_shift_hint,
		                                           &active_anchor_u, &active_anchor_v);
		if (track->merged_bitmap && track->merged_bitmap->gltexture) {
			active_anchor_w = (float) (track->merged_bitmap->gltexture->w > 0 ? track->merged_bitmap->gltexture->w : track->merged_bitmap->gltexture->tw);
			active_anchor_h = (float) (track->merged_bitmap->gltexture->h > 0 ? track->merged_bitmap->gltexture->h : track->merged_bitmap->gltexture->th);
		} else if (base_bitmap && base_bitmap->gltexture) {
			active_anchor_w = (float) (base_bitmap->gltexture->w > 0 ? base_bitmap->gltexture->w : base_bitmap->gltexture->tw);
			active_anchor_h = (float) (base_bitmap->gltexture->h > 0 ? base_bitmap->gltexture->h : base_bitmap->gltexture->th);
		}
		route_agree = fabsf(cached_anchor_u - legacy_anchor_u) < 0.0001f &&
		              fabsf(cached_anchor_v - legacy_anchor_v) < 0.0001f;
		merged_wall_probe_sample_path(track->route, track->orient,
		                              sample_path, sizeof(sample_path));
		merged_wall_probe_flip_axis(track->route, track->orient,
		                            flip_axis, sizeof(flip_axis));
		merged_wall_probe_uv_axis_name(track, 1, u_axis, sizeof(u_axis));
		merged_wall_probe_uv_axis_name(track, 0, v_axis, sizeof(v_axis));
		if (!strcmp(flip_axis, "U"))
			merged_wall_copy_string(flip_screen_axis, sizeof(flip_screen_axis), u_axis);
		else if (!strcmp(flip_axis, "V"))
			merged_wall_copy_string(flip_screen_axis, sizeof(flip_screen_axis), v_axis);
		else
			merged_wall_copy_string(flip_screen_axis, sizeof(flip_screen_axis), "none");

		merged_wall_copy_string(g_merged_wall_probe_result.status,
		                        sizeof(g_merged_wall_probe_result.status), "ok");
		g_merged_wall_probe_result.hit_kind = best.hit_kind;
		g_merged_wall_probe_result.center_polygon_hit = best.polygon_hit;
		g_merged_wall_probe_result.center_bbox_hit = best.bbox_hit;
		g_merged_wall_probe_result.seg = track->draw_ctx.valid ? track->draw_ctx.seg : -1;
		g_merged_wall_probe_result.side = track->draw_ctx.valid ? track->draw_ctx.side : -1;
		g_merged_wall_probe_result.face = track->draw_ctx.valid ? track->draw_ctx.face : -1;
		g_merged_wall_probe_result.child = track->draw_ctx.valid ? track->draw_ctx.child : -1;
		g_merged_wall_probe_result.wid_flags = track->draw_ctx.valid ? track->draw_ctx.wid_flags : 0;
		g_merged_wall_probe_result.tmap1 = track->draw_ctx.valid ? track->draw_ctx.tmap1 : -1;
		g_merged_wall_probe_result.tmap2 = track->draw_ctx.valid ? track->draw_ctx.tmap2 : 0;
		g_merged_wall_probe_result.orient = track->orient;
		merged_wall_copy_string(g_merged_wall_probe_result.route,
		                        sizeof(g_merged_wall_probe_result.route), track->route);
		merged_wall_copy_string(g_merged_wall_probe_result.merge_impl,
		                        sizeof(g_merged_wall_probe_result.merge_impl), track->merge_impl);
		merged_wall_copy_string(g_merged_wall_probe_result.ovl_flip_axis,
		                        sizeof(g_merged_wall_probe_result.ovl_flip_axis), flip_axis);
		merged_wall_copy_string(g_merged_wall_probe_result.flip_screen_axis,
		                        sizeof(g_merged_wall_probe_result.flip_screen_axis), flip_screen_axis);
		g_merged_wall_probe_result.u_span = u_span;
		g_merged_wall_probe_result.v_span = v_span;
		g_merged_wall_probe_result.u_shift_hint = u_shift_hint;
		g_merged_wall_probe_result.v_shift_hint = v_shift_hint;
		g_merged_wall_probe_result.cached_anchor_u = cached_anchor_u;
		g_merged_wall_probe_result.cached_anchor_v = cached_anchor_v;
		g_merged_wall_probe_result.legacy_anchor_u = legacy_anchor_u;
		g_merged_wall_probe_result.legacy_anchor_v = legacy_anchor_v;
		g_merged_wall_probe_result.route_agree = route_agree;
		merged_wall_probe_capture_render_sample(track, screen_w, screen_h);

		debug_log_force(DLOG_TEXTURE,
		                "[mwall_tap_probe] kind=face status=ok request=crosshair hit=%s rank=%d frame=%d request_frame=%d level=%d name=%s canvas=%d,%d,%d,%d canvas_center=%.1f/%.1f polygon_hit=%d bbox_hit=%d seg=%d side=%d face=%d child=%d wid=%d tmap1=%d tmap2=0x%x orient=%d route=%s merge_impl=%s ovl_sample_path=%s ovl_flip_axis=%s flip_screen_axis=%s",
		                merged_wall_probe_hit_kind_name_local(best.hit_kind),
		                rank >= 0 ? rank + 1 : 0,
		                g_merged_wall_frame_id,
		                g_merged_wall_snapshot_request_frame,
		                Current_level_num,
		                level_name,
		                canvas_x,
		                canvas_y,
		                canvas_w,
		                canvas_h,
		                canvas_center_x,
		                canvas_center_y,
		                best.polygon_hit,
		                best.bbox_hit,
		                g_merged_wall_probe_result.seg,
		                g_merged_wall_probe_result.side,
		                g_merged_wall_probe_result.face,
		                g_merged_wall_probe_result.child,
		                g_merged_wall_probe_result.wid_flags,
		                g_merged_wall_probe_result.tmap1,
		                g_merged_wall_probe_result.tmap2,
		                track->orient,
		                track->route,
		                track->merge_impl,
		                sample_path,
		                flip_axis,
		                flip_screen_axis);
		merged_wall_probe_log_side_state("crosshair", track);
		merged_wall_probe_log_palette_state("crosshair", track);
		debug_log_force(DLOG_TEXTURE,
		                "[mwall_tap_probe] kind=derived frame=%d request_frame=%d seg=%d side=%d face=%d u_min=%.6f u_max=%.6f v_min=%.6f v_max=%.6f u_span=%.6f v_span=%.6f u_shift_hint=%.6f v_shift_hint=%.6f u_screen_axis=%s v_screen_axis=%s cached_anchor_uv=%.6f/%.6f legacy_anchor_uv=%.6f/%.6f active_anchor_px=%.3f/%.3f route_agree=%d",
		                g_merged_wall_frame_id,
		                g_merged_wall_snapshot_request_frame,
		                g_merged_wall_probe_result.seg,
		                g_merged_wall_probe_result.side,
		                g_merged_wall_probe_result.face,
		                u_min,
		                u_max,
		                v_min,
		                v_max,
		                u_span,
		                v_span,
		                u_shift_hint,
		                v_shift_hint,
		                u_axis,
		                v_axis,
		                cached_anchor_u,
		                cached_anchor_v,
		                legacy_anchor_u,
		                legacy_anchor_v,
		                active_anchor_u * active_anchor_w,
		                active_anchor_v * active_anchor_h,
		                route_agree);
		for (i = 0; i < track->nv; i++) {
			debug_log_force(DLOG_TEXTURE,
			                "[mwall_tap_probe] kind=vertex frame=%d request_frame=%d seg=%d side=%d face=%d idx=%d corner=%s projected=%d sx=%.3f sy=%.3f x=%d y=%d z=%d u_fix=%d v_fix=%d u=%.6f v=%.6f",
			                g_merged_wall_frame_id,
			                g_merged_wall_snapshot_request_frame,
			                g_merged_wall_probe_result.seg,
			                g_merged_wall_probe_result.side,
			                g_merged_wall_probe_result.face,
			                i,
			                merged_wall_probe_corner_tag(track, i),
			                track->pt_projected[i],
			                track->pt_sx[i],
			                track->pt_sy[i],
			                (int) track->pts[i][0],
			                (int) track->pts[i][1],
			                (int) track->pts[i][2],
			                (int) track->uvl[i].u,
			                (int) track->uvl[i].v,
			                f2fl(track->uvl[i].u),
			                f2fl(track->uvl[i].v));
		}
		merged_wall_probe_log_layer_bitmap("base", track, base_tex_num);
		merged_wall_probe_log_layer_bitmap("overlay", track, overlay_tex_num);
		merged_wall_probe_log_merged_bitmap(track);
	}
}

void android_merged_wall_request_snapshot(int request_mode)
{
	const char *level_name = Current_level_name[0] ? Current_level_name : "<none>";

	if (request_mode != MERGED_WALL_REQUEST_PROBE)
		request_mode = MERGED_WALL_REQUEST_SNAPSHOT;

	memset(&g_merged_wall_snapshot_result, 0, sizeof(g_merged_wall_snapshot_result));
	if (request_mode == MERGED_WALL_REQUEST_PROBE) {
		merged_wall_reset_probe_result();
		merged_wall_copy_string(g_merged_wall_probe_result.status,
		                        sizeof(g_merged_wall_probe_result.status), "pending");
		g_merged_wall_probe_result.request_frame = g_merged_wall_frame_id;
	}
	merged_wall_reset_cover_bitmap_dumps();
	merged_wall_reset_cover_gpu_readbacks();
	merged_wall_reset_snapshot_target_cover();
	merged_wall_copy_string(g_merged_wall_snapshot_result.status,
	                        sizeof(g_merged_wall_snapshot_result.status), "pending");
	g_merged_wall_snapshot_result.request_frame = g_merged_wall_frame_id;
	g_merged_wall_snapshot_request_frame = g_merged_wall_frame_id;
	g_merged_wall_snapshot_request_mode = request_mode;
	debug_log_force(DLOG_TEXTURE,
	                "[mwall_snap] request: request=%s request_frame=%d frame=%d pass=%d seq=%d level=%d name=%s mode=%d(%s)",
	                merged_wall_request_mode_name_local(request_mode),
	                (int) g_merged_wall_snapshot_request_frame,
	                (int) g_merged_wall_frame_id,
	                (int) g_merged_wall_render_pass,
	                (int) g_merged_wall_draw_seq,
	                Current_level_num,
	                level_name,
	                (int) g_merged_wall_debug_mode,
	                merged_wall_debug_mode_name_local((int) g_merged_wall_debug_mode));
	merged_wall_log_snapshot_pose();
	merged_wall_log_snapshot_last_draw();
	__sync_synchronize();
	g_merged_wall_snapshot_pending = 1;
}

static int android_merged_wall_snapshot_value(const volatile int *value)
{
	return value ? *value : -1;
}

void android_merged_wall_sample_snapshot_framebuffer(
    int screen_w, int screen_h,
    volatile int *sample_r, volatile int *sample_g, volatile int *sample_b, volatile int *sample_a,
    volatile int *avg_r, volatile int *avg_g, volatile int *avg_b, volatile int *avg_a)
{
	if (screen_w > 0 && screen_h > 0) {
		unsigned char rgba[4] = { 0 };
		int sr = 0, sg = 0, sb = 0, sa = 0, n = 0;

		glReadPixels(screen_w / 2, screen_h / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
		if (sample_r)
			*sample_r = rgba[0];
		if (sample_g)
			*sample_g = rgba[1];
		if (sample_b)
			*sample_b = rgba[2];
		if (sample_a)
			*sample_a = rgba[3];
		for (int gy = 1; gy <= 4; gy++) {
			for (int gx = 1; gx <= 4; gx++) {
				int px = screen_w * gx / 5;
				int py = screen_h * gy / 5;

				glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
				sr += rgba[0];
				sg += rgba[1];
				sb += rgba[2];
				sa += rgba[3];
				n++;
			}
		}
		if (avg_r)
			*avg_r = sr / n;
		if (avg_g)
			*avg_g = sg / n;
		if (avg_b)
			*avg_b = sb / n;
		if (avg_a)
			*avg_a = sa / n;
	}

	android_merged_wall_finish_snapshot(screen_w, screen_h,
	                                    android_merged_wall_snapshot_value(sample_r),
	                                    android_merged_wall_snapshot_value(sample_g),
	                                    android_merged_wall_snapshot_value(sample_b),
	                                    android_merged_wall_snapshot_value(sample_a),
	                                    android_merged_wall_snapshot_value(avg_r),
	                                    android_merged_wall_snapshot_value(avg_g),
	                                    android_merged_wall_snapshot_value(avg_b),
	                                    android_merged_wall_snapshot_value(avg_a));
}

void android_merged_wall_finish_snapshot(int screen_w, int screen_h,
                                         int sample_r, int sample_g, int sample_b, int sample_a,
                                         int avg_r, int avg_g, int avg_b, int avg_a)
{
	float center_x = screen_w > 0 ? (float) screen_w * 0.5f : 0.0f;
	float center_y = screen_h > 0 ? (float) screen_h * 0.5f : 0.0f;
	int canvas_x = 0;
	int canvas_y = 0;
	int canvas_w = screen_w;
	int canvas_h = screen_h;
	float canvas_center_x = center_x;
	float canvas_center_y = center_y;
	int selected[MERGED_WALL_SNAPSHOT_FACE_MAX];
	int partial_selected[MERGED_WALL_SNAPSHOT_FACE_MAX];
	int selected_count = 0;
	int partial_selected_count = 0;
	int center_hit_count = 0;
	int relevant_cover_logs = 0;
	int omitted_cover_logs = 0;
	int partial_face_count = 0;
	const char *level_name = Current_level_name[0] ? Current_level_name : "<none>";
	int i, pass;

	if (grd_curcanv) {
		canvas_x = grd_curcanv->cv_bitmap.bm_x;
		canvas_y = grd_curcanv->cv_bitmap.bm_y;
		canvas_w = grd_curcanv->cv_bitmap.bm_w;
		canvas_h = grd_curcanv->cv_bitmap.bm_h;
		canvas_center_x = (float) canvas_x + (float) canvas_w * 0.5f;
		canvas_center_y = (float) canvas_y + (float) canvas_h * 0.5f;
	}

	if (!g_merged_wall_snapshot_pending)
		return;
	if (g_merged_wall_frame_id == g_merged_wall_snapshot_request_frame)
		return;

	merged_wall_begin_cover_events();
	memset(&g_merged_wall_snapshot_result, 0, sizeof(g_merged_wall_snapshot_result));
	g_merged_wall_snapshot_result.request_frame = g_merged_wall_snapshot_request_frame;
	g_merged_wall_snapshot_result.frame_id = g_merged_wall_frame_id;
	g_merged_wall_snapshot_result.screen_w = screen_w;
	g_merged_wall_snapshot_result.screen_h = screen_h;
	g_merged_wall_snapshot_result.center_x = center_x;
	g_merged_wall_snapshot_result.center_y = center_y;
	g_merged_wall_snapshot_result.sample_r = sample_r;
	g_merged_wall_snapshot_result.sample_g = sample_g;
	g_merged_wall_snapshot_result.sample_b = sample_b;
	g_merged_wall_snapshot_result.sample_a = sample_a;
	g_merged_wall_snapshot_result.avg_r = avg_r;
	g_merged_wall_snapshot_result.avg_g = avg_g;
	g_merged_wall_snapshot_result.avg_b = avg_b;
	g_merged_wall_snapshot_result.avg_a = avg_a;
	g_merged_wall_snapshot_result.tracked_count =
	    merged_wall_tracked_frame_id == g_merged_wall_frame_id ? merged_wall_tracked_face_count : 0;
	g_merged_wall_snapshot_result.cover_event_count = merged_wall_cover_event_count;
	g_merged_wall_snapshot_result.target_cover_gpu = merged_wall_snapshot_target_cover;

	for (i = 0; i < MERGED_WALL_SNAPSHOT_FACE_MAX; i++)
		selected[i] = -1;
	for (i = 0; i < MERGED_WALL_SNAPSHOT_FACE_MAX; i++)
		partial_selected[i] = -1;
	for (i = 0; i < merged_wall_tracked_face_count; i++) {
		const struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[i];

		if (!track->bbox_valid && track->projected_bbox_valid)
			partial_face_count++;
		if (!track->bbox_valid)
			continue;
		if (merged_wall_bbox_contains_point(center_x, center_y,
		                                    track->min_sx, track->max_sx, track->min_sy, track->max_sy))
			center_hit_count++;
	}
	g_merged_wall_snapshot_result.center_hit_count = center_hit_count;

	__sync_synchronize();
	g_merged_wall_snapshot_pending = 0;
	debug_log(DLOG_TEXTURE,
	          "[mwall_snapshot] stage=frame frame=%d request_frame=%d level=%d name=%s center=%.1f/%.1f canvas=%d,%d,%d,%d canvas_center=%.1f/%.1f fb_rgba=%d/%d/%d/%d fb_avg=%d/%d/%d/%d tracked=%d partial=%d center_hits=%d cover_events=%d",
	          g_merged_wall_frame_id,
	          g_merged_wall_snapshot_request_frame,
	          Current_level_num,
	          level_name,
	          center_x,
	          center_y,
	          canvas_x,
	          canvas_y,
	          canvas_w,
	          canvas_h,
	          canvas_center_x,
	          canvas_center_y,
	          sample_r,
	          sample_g,
	          sample_b,
	          sample_a,
	          avg_r,
	          avg_g,
	          avg_b,
	          avg_a,
	          g_merged_wall_snapshot_result.tracked_count,
	          partial_face_count,
	          center_hit_count,
	          merged_wall_cover_event_count);

	if (merged_wall_tracked_frame_id != g_merged_wall_frame_id || merged_wall_tracked_face_count <= 0) {
		merged_wall_copy_string(g_merged_wall_snapshot_result.status,
		                        sizeof(g_merged_wall_snapshot_result.status), "no_tracked_faces");
		g_merged_wall_snapshot_result.valid = 1;
		if (g_merged_wall_snapshot_request_mode == MERGED_WALL_REQUEST_PROBE) {
			merged_wall_reset_probe_result();
			g_merged_wall_probe_result.valid = 1;
			g_merged_wall_probe_result.frame_id = g_merged_wall_frame_id;
			g_merged_wall_probe_result.request_frame = g_merged_wall_snapshot_request_frame;
			merged_wall_copy_string(g_merged_wall_probe_result.status,
			                        sizeof(g_merged_wall_probe_result.status), "no_tracked_faces");
		}
		debug_log(DLOG_TEXTURE,
		          "[mwall_snapshot] no_tracked_faces frame=%d request_frame=%d level=%d name=%s",
		          g_merged_wall_frame_id,
		          g_merged_wall_snapshot_request_frame,
		          Current_level_num,
		          level_name);
		g_merged_wall_snapshot_request_mode = 0;
		return;
	}

	for (pass = 0; pass < 2 && selected_count < MERGED_WALL_SNAPSHOT_FACE_MAX; pass++) {
		for (;;) {
			int best_index = -1;
			float best_dist2 = 0.0f;

			for (i = 0; i < merged_wall_tracked_face_count; i++) {
				const struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[i];
				int contains_center;
				float dist2;

				if (!track->bbox_valid || merged_wall_face_is_selected(selected, selected_count, i))
					continue;
				contains_center = merged_wall_bbox_contains_point(center_x, center_y,
				                                                  track->min_sx, track->max_sx, track->min_sy, track->max_sy);
				if (pass == 0 && !contains_center)
					continue;
				if (pass == 1 && contains_center)
					continue;
				dist2 = merged_wall_bbox_distance_sq(center_x, center_y,
				                                     track->min_sx, track->max_sx, track->min_sy, track->max_sy);
				if (best_index < 0 || dist2 < best_dist2 - 0.5f ||
				    (dist2 <= best_dist2 + 0.5f &&
				     (track->bbox_area > merged_wall_tracked_faces[best_index].bbox_area + 0.5f ||
				      (track->bbox_area >= merged_wall_tracked_faces[best_index].bbox_area - 0.5f &&
				       (track->draw_order > merged_wall_tracked_faces[best_index].draw_order ||
				        (track->draw_order == merged_wall_tracked_faces[best_index].draw_order &&
				         track->draw_seq > merged_wall_tracked_faces[best_index].draw_seq)))))) {
					best_index = i;
					best_dist2 = dist2;
				}
			}

			if (best_index < 0)
				break;
			selected[selected_count++] = best_index;
			if (selected_count >= MERGED_WALL_SNAPSHOT_FACE_MAX)
				break;
		}
	}

	if (selected_count <= 0) {
		for (pass = 0; pass < 2 && partial_selected_count < MERGED_WALL_SNAPSHOT_FACE_MAX; pass++) {
			for (;;) {
				int best_index = -1;
				float best_dist2 = 0.0f;

				for (i = 0; i < merged_wall_tracked_face_count; i++) {
					const struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[i];
					int contains_center;
					float dist2;

					if (track->bbox_valid || !track->projected_bbox_valid || track->projected_count <= 0 ||
					    merged_wall_face_is_selected(partial_selected, partial_selected_count, i))
						continue;
					contains_center = merged_wall_bbox_contains_point(center_x, center_y,
					                                                  track->projected_min_sx,
					                                                  track->projected_max_sx,
					                                                  track->projected_min_sy,
					                                                  track->projected_max_sy);
					if (pass == 0 && !contains_center)
						continue;
					if (pass == 1 && contains_center)
						continue;
					dist2 = merged_wall_bbox_distance_sq(center_x, center_y,
					                                     track->projected_min_sx,
					                                     track->projected_max_sx,
					                                     track->projected_min_sy,
					                                     track->projected_max_sy);
					if (best_index < 0 || dist2 < best_dist2 - 0.5f ||
					    (dist2 <= best_dist2 + 0.5f &&
					     (track->projected_count > merged_wall_tracked_faces[best_index].projected_count ||
					      (track->projected_count == merged_wall_tracked_faces[best_index].projected_count &&
					       (track->projected_bbox_area > merged_wall_tracked_faces[best_index].projected_bbox_area + 0.5f ||
					        (track->projected_bbox_area >= merged_wall_tracked_faces[best_index].projected_bbox_area - 0.5f &&
					         (track->draw_order > merged_wall_tracked_faces[best_index].draw_order ||
					          (track->draw_order == merged_wall_tracked_faces[best_index].draw_order &&
					           track->draw_seq > merged_wall_tracked_faces[best_index].draw_seq)))))))) {
						best_index = i;
						best_dist2 = dist2;
					}
				}

				if (best_index < 0)
					break;
				partial_selected[partial_selected_count++] = best_index;
				if (partial_selected_count >= MERGED_WALL_SNAPSHOT_FACE_MAX)
					break;
			}
		}
	}

	merged_wall_log_snapshot_focus_covers(center_x, center_y,
	                                      canvas_center_x, canvas_center_y,
	                                      selected, selected_count,
	                                      partial_selected, partial_selected_count);

	if (selected_count <= 0) {
		merged_wall_copy_string(g_merged_wall_snapshot_result.status,
		                        sizeof(g_merged_wall_snapshot_result.status), "no_projected_faces");
		g_merged_wall_snapshot_result.valid = 1;
		if (g_merged_wall_snapshot_request_mode == MERGED_WALL_REQUEST_PROBE)
			merged_wall_log_tap_probe(canvas_center_x, canvas_center_y,
			                          canvas_x, canvas_y, canvas_w, canvas_h,
			                          screen_w, screen_h,
			                          selected, selected_count);
		debug_log(DLOG_TEXTURE,
		          "[mwall_snapshot] no_projected_faces frame=%d request_frame=%d level=%d name=%s partial_candidates=%d",
		          g_merged_wall_frame_id,
		          g_merged_wall_snapshot_request_frame,
		          Current_level_num,
		          level_name,
		          partial_selected_count);
		for (i = 0; i < partial_selected_count; i++) {
			const struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[partial_selected[i]];
			int center_hit = merged_wall_bbox_contains_point(center_x, center_y,
			                                                 track->projected_min_sx,
			                                                 track->projected_max_sx,
			                                                 track->projected_min_sy,
			                                                 track->projected_max_sy);
			float dist2 = merged_wall_bbox_distance_sq(center_x, center_y,
			                                           track->projected_min_sx,
			                                           track->projected_max_sx,
			                                           track->projected_min_sy,
			                                           track->projected_max_sy);

			debug_log(DLOG_TEXTURE,
			          "[mwall_snapshot_partial] rank=%d center_hit=%d dist2=%.1f frame=%d pass=%d seq=%d order=%d seg=%d side=%d face=%d child=%d side_type=%d wid=%d tmap1=%d tmap2=0x%x nv=%d projected=%d route=%s merge_impl=%s reason=%s full_box_valid=%d full_area=%.1f proj_box=%.1f..%.1f/%.1f..%.1f proj_area=%.1f submit_nv=%d fan=%.1f/%.1f alt=%.1f/%.1f fan_flip=%d alt_flip=%d fan_flat=%d alt_flat=%d cull_sensitive=%d pick=%s",
			          i + 1,
			          center_hit,
			          dist2,
			          g_merged_wall_frame_id,
			          track->render_pass,
			          track->draw_seq,
			          track->draw_order,
			          track->draw_ctx.valid ? track->draw_ctx.seg : -1,
			          track->draw_ctx.valid ? track->draw_ctx.side : -1,
			          track->draw_ctx.valid ? track->draw_ctx.face : -1,
			          track->draw_ctx.valid ? track->draw_ctx.child : -1,
			          track->draw_ctx.valid ? track->draw_ctx.side_type : -1,
			          track->draw_ctx.valid ? track->draw_ctx.wid_flags : 0,
			          track->draw_ctx.valid ? track->draw_ctx.tmap1 : -1,
			          track->draw_ctx.valid ? track->draw_ctx.tmap2 : 0,
			          track->nv,
			          track->projected_count,
			          track->route,
			          track->merge_impl,
			          track->decision_reason,
			          track->bbox_valid,
			          track->bbox_area,
			          track->projected_min_sx,
			          track->projected_max_sx,
			          track->projected_min_sy,
			          track->projected_max_sy,
			          track->projected_bbox_area,
			          track->nv,
			          track->fan_area_012,
			          track->fan_area_023,
			          track->alt_area_013,
			          track->alt_area_123,
			          track->fan_flip,
			          track->alt_flip,
			          track->fan_flat,
			          track->alt_flat,
			          track->cull_sensitive,
			          track->preferred_split);
			merged_wall_log_portal("snapshot_partial", &track->draw_ctx);
			merged_wall_log_face_textures("snapshot_partial", i + 1, &track->draw_ctx);
		}
		for (i = 0; i < merged_wall_cover_event_count; i++) {
			const struct merged_wall_snapshot_cover_event *event = &merged_wall_cover_events[i];
			const struct merged_wall_tracked_face *track;
			float overlap_area = 0.0f;
			int rank;
			int center_face;
			int center_cover = 0;
			int center_overlap = 0;

			if (event->face_index < 0 || event->face_index >= merged_wall_tracked_face_count)
				continue;
			track = &merged_wall_tracked_faces[event->face_index];
			if (track->bbox_valid || !track->projected_bbox_valid)
				continue;
			rank = merged_wall_selected_rank(partial_selected, partial_selected_count, event->face_index);
			if (rank < 0)
				continue;
			center_face = merged_wall_bbox_contains_point(center_x, center_y,
			                                              track->projected_min_sx,
			                                              track->projected_max_sx,
			                                              track->projected_min_sy,
			                                              track->projected_max_sy);
			if (event->cover_bbox_valid) {
				float overlap_min_sx = track->projected_min_sx > event->cover_min_sx ? track->projected_min_sx : event->cover_min_sx;
				float overlap_max_sx = track->projected_max_sx < event->cover_max_sx ? track->projected_max_sx : event->cover_max_sx;
				float overlap_min_sy = track->projected_min_sy > event->cover_min_sy ? track->projected_min_sy : event->cover_min_sy;
				float overlap_max_sy = track->projected_max_sy < event->cover_max_sy ? track->projected_max_sy : event->cover_max_sy;

				center_cover = merged_wall_bbox_contains_point(center_x, center_y,
				                                               event->cover_min_sx, event->cover_max_sx,
				                                               event->cover_min_sy, event->cover_max_sy);
				overlap_area = merged_wall_bbox_overlap_area(track->projected_min_sx, track->projected_max_sx,
				                                             track->projected_min_sy, track->projected_max_sy,
				                                             event->cover_min_sx, event->cover_max_sx,
				                                             event->cover_min_sy, event->cover_max_sy);
				if (overlap_area > 0.0f && merged_wall_bbox_contains_point(center_x, center_y,
				                                                           overlap_min_sx, overlap_max_sx,
				                                                           overlap_min_sy, overlap_max_sy))
					center_overlap = 1;
			}
			if (event->overlap_area > overlap_area)
				overlap_area = event->overlap_area;
			debug_log(DLOG_TEXTURE,
			          "[mwall_snapshot_partial_cover] kind=%s rank=%d center_face=%d center_cover=%d center_overlap=%d overlap=%.1f ordered=%d frame=%d pass=%d seq=%d draw_order=%d cover_order=%d cover_shader=%s cover_bot=%s cover_ovl=%s seg=%d side=%d face=%d child=%d wid=%d cover_seg=%d cover_side=%d cover_face=%d cover_child=%d cover_wid=%d",
			          event->kind == MERGED_WALL_SNAPSHOT_COVER_EXACT ? "exact" : "bbox",
			          rank + 1,
			          center_face,
			          center_cover,
			          center_overlap,
			          overlap_area,
			          event->ordered,
			          g_merged_wall_frame_id,
			          track->render_pass,
			          track->draw_seq,
			          track->draw_order,
			          event->cover_order,
			          event->cover_shader,
			          event->cover_bot,
			          event->cover_ovl,
			          track->draw_ctx.valid ? track->draw_ctx.seg : -1,
			          track->draw_ctx.valid ? track->draw_ctx.side : -1,
			          track->draw_ctx.valid ? track->draw_ctx.face : -1,
			          track->draw_ctx.valid ? track->draw_ctx.child : -1,
			          track->draw_ctx.valid ? track->draw_ctx.wid_flags : 0,
			          event->cover_ctx.valid ? event->cover_ctx.seg : -1,
			          event->cover_ctx.valid ? event->cover_ctx.side : -1,
			          event->cover_ctx.valid ? event->cover_ctx.face : -1,
			          event->cover_ctx.valid ? event->cover_ctx.child : -1,
			          event->cover_ctx.valid ? event->cover_ctx.wid_flags : 0);
			merged_wall_log_portal("snapshot_partial", &track->draw_ctx);
			merged_wall_log_portal("snapshot_partial_cover", &event->cover_ctx);
			merged_wall_log_face_textures("snapshot_cover", rank + 1, &event->cover_ctx);
		}
		g_merged_wall_snapshot_request_mode = 0;
		return;
	}

	g_merged_wall_snapshot_result.selected_count = selected_count;
	merged_wall_copy_string(g_merged_wall_snapshot_result.status,
	                        sizeof(g_merged_wall_snapshot_result.status), "ok");
	g_merged_wall_snapshot_result.valid = 1;

	for (i = 0; i < selected_count; i++) {
		const struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[selected[i]];
		struct merged_wall_snapshot_face *out = &g_merged_wall_snapshot_result.faces[i];
		float dist2 = merged_wall_bbox_distance_sq(center_x, center_y,
		                                           track->min_sx, track->max_sx, track->min_sy, track->max_sy);
		int center_hit = merged_wall_bbox_contains_point(center_x, center_y,
		                                                 track->min_sx, track->max_sx, track->min_sy, track->max_sy);

		memset(out, 0, sizeof(*out));
		out->valid = 1;
		out->rank = i + 1;
		out->center_hit = center_hit;
		out->dist2 = dist2;
		out->render_pass = track->render_pass;
		out->draw_seq = track->draw_seq;
		out->draw_order = track->draw_order;
		out->seg = track->draw_ctx.valid ? track->draw_ctx.seg : -1;
		out->side = track->draw_ctx.valid ? track->draw_ctx.side : -1;
		out->face = track->draw_ctx.valid ? track->draw_ctx.face : -1;
		out->child = track->draw_ctx.valid ? track->draw_ctx.child : -1;
		out->side_type = track->draw_ctx.valid ? track->draw_ctx.side_type : -1;
		out->wid_flags = track->draw_ctx.valid ? track->draw_ctx.wid_flags : 0;
		out->tmap1 = track->draw_ctx.valid ? track->draw_ctx.tmap1 : -1;
		out->tmap2 = track->draw_ctx.valid ? track->draw_ctx.tmap2 : 0;
		out->min_sx = track->min_sx;
		out->max_sx = track->max_sx;
		out->min_sy = track->min_sy;
		out->max_sy = track->max_sy;
		out->bbox_area = track->bbox_area;
		out->fan_area_012 = track->fan_area_012;
		out->fan_area_023 = track->fan_area_023;
		out->alt_area_013 = track->alt_area_013;
		out->alt_area_123 = track->alt_area_123;
		out->fan_flip = track->fan_flip;
		out->alt_flip = track->alt_flip;
		out->fan_flat = track->fan_flat;
		out->alt_flat = track->alt_flat;
		out->cull_sensitive = track->cull_sensitive;
		out->submit_nv = track->nv;
		merged_wall_copy_string(out->preferred_split, sizeof(out->preferred_split), track->preferred_split);
		merged_wall_copy_string(out->route, sizeof(out->route), track->route);
		merged_wall_copy_string(out->merge_impl, sizeof(out->merge_impl), track->merge_impl);
		merged_wall_copy_string(out->decision_reason, sizeof(out->decision_reason), track->decision_reason);
		debug_log(DLOG_TEXTURE,
		          "[mwall_snapshot_face] rank=%d center_hit=%d dist2=%.1f frame=%d pass=%d seq=%d order=%d seg=%d side=%d face=%d child=%d side_type=%d wid=%d tmap1=%d tmap2=0x%x route=%s merge_impl=%s reason=%s box=%.1f..%.1f/%.1f..%.1f area=%.1f submit_nv=%d fan=%.1f/%.1f alt=%.1f/%.1f fan_flip=%d alt_flip=%d fan_flat=%d alt_flat=%d cull_sensitive=%d pick=%s",
		          out->rank,
		          center_hit,
		          dist2,
		          g_merged_wall_frame_id,
		          track->render_pass,
		          track->draw_seq,
		          track->draw_order,
		          out->seg,
		          out->side,
		          out->face,
		          out->child,
		          out->side_type,
		          out->wid_flags,
		          out->tmap1,
		          out->tmap2,
		          out->route,
		          out->merge_impl,
		          out->decision_reason,
		          out->min_sx,
		          out->max_sx,
		          out->min_sy,
		          out->max_sy,
		          out->bbox_area,
		          out->submit_nv,
		          out->fan_area_012,
		          out->fan_area_023,
		          out->alt_area_013,
		          out->alt_area_123,
		          out->fan_flip,
		          out->alt_flip,
		          out->fan_flat,
		          out->alt_flat,
		          out->cull_sensitive,
		          out->preferred_split);
		merged_wall_log_portal("snapshot_face", &track->draw_ctx);
		merged_wall_log_face_textures("snapshot_face", out->rank, &track->draw_ctx);
	}

	for (i = 0; i < merged_wall_cover_event_count; i++) {
		const struct merged_wall_snapshot_cover_event *event = &merged_wall_cover_events[i];
		const struct merged_wall_tracked_face *track;
		struct merged_wall_snapshot_cover *out;
		float overlap_area = 0.0f;
		int rank;
		int center_face;
		int center_cover = 0;
		int center_overlap = 0;

		if (event->face_index < 0 || event->face_index >= merged_wall_tracked_face_count)
			continue;
		track = &merged_wall_tracked_faces[event->face_index];
		if (!track->bbox_valid)
			continue;

		rank = merged_wall_selected_rank(selected, selected_count, event->face_index);
		center_face = merged_wall_bbox_contains_point(center_x, center_y,
		                                              track->min_sx, track->max_sx, track->min_sy, track->max_sy);
		if (event->cover_bbox_valid) {
			float overlap_min_sx = track->min_sx > event->cover_min_sx ? track->min_sx : event->cover_min_sx;
			float overlap_max_sx = track->max_sx < event->cover_max_sx ? track->max_sx : event->cover_max_sx;
			float overlap_min_sy = track->min_sy > event->cover_min_sy ? track->min_sy : event->cover_min_sy;
			float overlap_max_sy = track->max_sy < event->cover_max_sy ? track->max_sy : event->cover_max_sy;

			center_cover = merged_wall_bbox_contains_point(center_x, center_y,
			                                               event->cover_min_sx, event->cover_max_sx,
			                                               event->cover_min_sy, event->cover_max_sy);
			overlap_area = merged_wall_bbox_overlap_area(track->min_sx, track->max_sx,
			                                             track->min_sy, track->max_sy,
			                                             event->cover_min_sx, event->cover_max_sx,
			                                             event->cover_min_sy, event->cover_max_sy);
			if (overlap_area > 0.0f && merged_wall_bbox_contains_point(center_x, center_y,
			                                                           overlap_min_sx, overlap_max_sx, overlap_min_sy, overlap_max_sy))
				center_overlap = 1;
		}
		if (event->overlap_area > overlap_area)
			overlap_area = event->overlap_area;

		if (rank < 0 && !center_cover && !center_overlap)
			continue;
		if (relevant_cover_logs >= MERGED_WALL_SNAPSHOT_COVER_MAX) {
			omitted_cover_logs++;
			continue;
		}

		out = &g_merged_wall_snapshot_result.covers[relevant_cover_logs++];
		memset(out, 0, sizeof(*out));
		out->valid = 1;
		out->kind = event->kind;
		out->rank = rank >= 0 ? rank + 1 : 0;
		out->center_face = center_face;
		out->center_cover = center_cover;
		out->center_overlap = center_overlap;
		out->overlap_area = overlap_area;
		out->ordered = event->ordered;
		out->render_pass = track->render_pass;
		out->draw_seq = track->draw_seq;
		out->draw_order = track->draw_order;
		out->cover_order = event->cover_order;
		out->seg = track->draw_ctx.valid ? track->draw_ctx.seg : -1;
		out->side = track->draw_ctx.valid ? track->draw_ctx.side : -1;
		out->face = track->draw_ctx.valid ? track->draw_ctx.face : -1;
		out->child = track->draw_ctx.valid ? track->draw_ctx.child : -1;
		out->wid_flags = track->draw_ctx.valid ? track->draw_ctx.wid_flags : 0;
		out->cover_seg = event->cover_ctx.valid ? event->cover_ctx.seg : -1;
		out->cover_side = event->cover_ctx.valid ? event->cover_ctx.side : -1;
		out->cover_face = event->cover_ctx.valid ? event->cover_ctx.face : -1;
		out->cover_child = event->cover_ctx.valid ? event->cover_ctx.child : -1;
		out->cover_wid_flags = event->cover_ctx.valid ? event->cover_ctx.wid_flags : 0;
		merged_wall_copy_string(out->cover_shader, sizeof(out->cover_shader), event->cover_shader);
		merged_wall_copy_string(out->cover_bot, sizeof(out->cover_bot), event->cover_bot);
		merged_wall_copy_string(out->cover_ovl, sizeof(out->cover_ovl), event->cover_ovl);
		debug_log(DLOG_TEXTURE,
		          "[mwall_snapshot_cover] kind=%s rank=%d center_face=%d center_cover=%d center_overlap=%d overlap=%.1f ordered=%d frame=%d pass=%d seq=%d draw_order=%d cover_order=%d cover_shader=%s cover_bot=%s cover_ovl=%s seg=%d side=%d face=%d child=%d wid=%d cover_seg=%d cover_side=%d cover_face=%d cover_child=%d cover_wid=%d",
		          event->kind == MERGED_WALL_SNAPSHOT_COVER_EXACT ? "exact" : "bbox",
		          out->rank,
		          center_face,
		          center_cover,
		          center_overlap,
		          overlap_area,
		          event->ordered,
		          g_merged_wall_frame_id,
		          track->render_pass,
		          track->draw_seq,
		          track->draw_order,
		          event->cover_order,
		          out->cover_shader,
		          out->cover_bot,
		          out->cover_ovl,
		          out->seg,
		          out->side,
		          out->face,
		          out->child,
		          out->wid_flags,
		          out->cover_seg,
		          out->cover_side,
		          out->cover_face,
		          out->cover_child,
		          out->cover_wid_flags);
		merged_wall_log_portal("snapshot_face", &track->draw_ctx);
		merged_wall_log_portal("snapshot_cover", &event->cover_ctx);
		merged_wall_log_face_textures("snapshot_cover", out->rank > 0 ? out->rank : 0, &event->cover_ctx);
	}

	g_merged_wall_snapshot_result.relevant_cover_count = relevant_cover_logs;
	g_merged_wall_snapshot_result.omitted_cover_count = omitted_cover_logs;
	if (!relevant_cover_logs)
		debug_log(DLOG_TEXTURE,
		          "[mwall_snapshot] no_relevant_cover_events frame=%d",
		          g_merged_wall_frame_id);
	else if (omitted_cover_logs > 0)
		debug_log(DLOG_TEXTURE,
		          "[mwall_snapshot] omitted_cover_events=%d frame=%d",
		          omitted_cover_logs,
		          g_merged_wall_frame_id);
	if (g_merged_wall_snapshot_request_mode == MERGED_WALL_REQUEST_PROBE)
		merged_wall_log_tap_probe(canvas_center_x, canvas_center_y,
		                          canvas_x, canvas_y, canvas_w, canvas_h,
		                          screen_w, screen_h,
		                          selected, selected_count);
	g_merged_wall_snapshot_request_mode = 0;
}

#endif /* ANDROID */
