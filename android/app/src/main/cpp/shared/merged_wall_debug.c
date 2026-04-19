#ifdef ANDROID

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "3d.h"
#include "gameseg.h"
#include "gameseq.h"
#include "inferno.h"
#include "object.h"
#include "ogl_init.h"
#include "piggy.h"
#include "rle.h"
#include "segment.h"
#include "textures.h"
#include "wall.h"

#include "android_log.h"
#include "merged_wall_debug.h"

#define MERGED_WALL_LOG_PT_COUNT         16
#define MERGED_WALL_TRACKED_FACE_MAX     32
#define MERGED_WALL_COVER_EVENT_MAX      64
#define MERGED_WALL_SNAPSHOT_COVER_EXACT 1
#define MERGED_WALL_SNAPSHOT_COVER_BBOX  2
#define MERGED_WALL_BITMAP_DUMP_MAX      16
#define MERGED_WALL_BITMAP_DUMP_WIDTH    64

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
	char route[24];
	char merge_impl[24];
	char decision_reason[40];
};

struct merged_wall_snapshot_cover_event {
	int kind;
	int face_index;
	int cover_order;
	int ordered;
	struct android_draw_face_context cover_ctx;
	int cover_bbox_valid;
	float cover_min_sx;
	float cover_max_sx;
	float cover_min_sy;
	float cover_max_sy;
	float overlap_area;
	char cover_shader[16];
	char cover_bot[24];
	char cover_ovl[24];
};

struct merged_wall_snapshot_focus_cover {
	int valid;
	int event_index;
	int face_rank;
	int partial_rank;
	int center_face;
	int center_cover;
	int center_overlap;
	int focus_bucket;
	float focus_dist2;
	float face_dist2;
	float cover_dist2;
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
volatile int g_merged_wall_experiment_mode = 0;
volatile int g_merged_wall_experiment_pending_apply = 0;
volatile int g_merged_wall_snapshot_pending = 0;
volatile int g_merged_wall_snapshot_request_frame = -1;
volatile int g_merged_wall_render_pass = 0;
volatile int g_merged_wall_frame_id = 0;
volatile int g_merged_wall_draw_seq = 0;
volatile int g_merged_wall_force_two_pass = 0;
struct android_draw_face_context g_android_draw_face_ctx = { 0 };
struct merged_wall_snapshot_result g_merged_wall_snapshot_result = { 0 };
struct merged_wall_last_draw_state g_merged_wall_last_draw_state = { 0 };

static struct merged_wall_tracked_face merged_wall_tracked_faces[MERGED_WALL_TRACKED_FACE_MAX];
static struct merged_wall_snapshot_cover_event merged_wall_cover_events[MERGED_WALL_COVER_EVENT_MAX];
static int merged_wall_tracked_face_count = 0;
static int merged_wall_tracked_frame_id = -1;
static int merged_wall_draw_order = 0;
static int merged_wall_dumped_cover_textures[MERGED_WALL_BITMAP_DUMP_MAX];
static int merged_wall_dumped_cover_texture_count = 0;
static int merged_wall_cover_frame_id = -1;
static int merged_wall_cover_event_count = 0;

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

static const char *merged_wall_experiment_name_local(int mode)
{
	switch (mode) {
		case MERGED_WALL_EXPERIMENT_FORCE_LEGACY_TEXMERGE:
			return "force_legacy_texmerge";
		default:
			return "default";
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

static int merged_wall_overlay_index(int tmap2)
{
	return tmap2 & 0x3fff;
}

static int merged_wall_is_named_overlay(int tmap2, const char *name)
{
	int overlay = merged_wall_overlay_index(tmap2);
	grs_bitmap *bm;
	const char *bm_name;

	if (!tmap2 || overlay < 0 || overlay >= NumTextures)
		return 0;
	bm = &GameBitmaps[Textures[overlay].index];
	bm_name = piggy_game_bitmap_name(bm);
	return bm_name && !d_stricmp(bm_name, name);
}

int android_merged_wall_is_logging_target_tmap2(int tmap2)
{
	if (!tmap2)
		return 0;
	return merged_wall_is_named_overlay(tmap2, "metl154");
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
	unsigned int hash = 2166136261u;
	int real_flags;
	int idx254 = 0, idx255 = 0;
	int row_stride;
	int row_bytes;
	int total_bytes;
	int x, y;

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

static void merged_wall_log_live_cover_state(const char *kind,
                                             const char *shader_kind,
                                             int face_pass, int face_seq, int face_order,
                                             int draw_order, int ordered, float overlap_area,
                                             const char *track_box_kind,
                                             const struct android_draw_face_context *face_ctx,
                                             const struct android_draw_face_context *cover_ctx,
                                             grs_bitmap *cover_bitmap,
                                             const struct g3s_point **pointlist,
                                             const g3s_uvl *uvl_list, int nv,
                                             int texfilt_level, int menu_texfilt,
                                             int hud_texfilt, int aniso_level)
{
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
	if (cover_ctx->tmap2 != 0) {
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
	src_bitmap = merged_wall_get_source_bitmap(cover_bitmap);
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

	debug_log(DLOG_TEXTURE,
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
	          "[mwall_snap_last] frame=%d pass=%d seq=%d seg=%d side=%d face=%d child=%d wid=%d tmap1=%d tmap2=0x%x route=%s merge_impl=%s depth=%d blend=%d cull=%d poly_off=%d poly_factor=%.3f poly_units=%.3f depth_mask=%d depth_func=%d front_face=%d cull_mode=%d fbo=%d area=%.1f force_cull=%d force_poly=%d force_depth=%d",
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
	          g_merged_wall_last_draw_state.screen_area,
	          g_merged_wall_last_draw_state.force_cull_off,
	          g_merged_wall_last_draw_state.force_polygon_offset,
	          g_merged_wall_last_draw_state.force_depth_off);
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
                                    int draw_order, const char *route, const char *merge_impl,
                                    const char *decision_reason)
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
	for (i = 0; i < nv; i++) {
		track->pts[i][0] = pointlist[i]->p3_vec.x;
		track->pts[i][1] = pointlist[i]->p3_vec.y;
		track->pts[i][2] = pointlist[i]->p3_vec.z;
	}
	projected = track->projected_count;
	debug_log(DLOG_TEXTURE,
	          "[mwall_track] frame=%d pass=%d seq=%d order=%d seg=%d side=%d face=%d child=%d nv=%d projected=%d bbox_valid=%d proj_box_valid=%d box=%.1f..%.1f/%.1f..%.1f area=%.1f proj_box=%.1f..%.1f/%.1f..%.1f proj_area=%.1f route=%s merge_impl=%s",
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
	          track->merge_impl);
}

void android_merged_wall_log_cover(const char *shader_kind, const char *botname,
                                   const char *ovlname, const struct g3s_point **pointlist, int nv,
                                   const g3s_uvl *uvl_list, int draw_order, grs_bitmap *cover_bitmap,
                                   int texfilt_level, int menu_texfilt, int hud_texfilt,
                                   int aniso_level)
{
	struct android_draw_face_context cover_ctx = g_android_draw_face_ctx;
	float cover_min_sx = 0.0f, cover_max_sx = 0.0f;
	float cover_min_sy = 0.0f, cover_max_sy = 0.0f;
	float cover_bbox_area = 0.0f;
	int cover_bbox_valid = 0;
	int i, ordered;

	merged_wall_begin_frame_tracking();
	cover_bbox_valid = merged_wall_get_screen_bbox(pointlist, nv,
	                                               &cover_min_sx, &cover_max_sx, &cover_min_sy, &cover_max_sy);
	if (cover_bbox_valid)
		cover_bbox_area = (cover_max_sx - cover_min_sx) * (cover_max_sy - cover_min_sy);
	for (i = 0; i < merged_wall_tracked_face_count; i++) {
		struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[i];

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
				event->cover_bbox_valid = cover_bbox_valid;
				event->cover_min_sx = cover_min_sx;
				event->cover_max_sx = cover_max_sx;
				event->cover_min_sy = cover_min_sy;
				event->cover_max_sy = cover_max_sy;
				event->overlap_area = overlap_area;
				merged_wall_copy_string(event->cover_shader,
				                        sizeof(event->cover_shader), shader_kind);
				merged_wall_copy_string(event->cover_bot,
				                        sizeof(event->cover_bot), botname);
				merged_wall_copy_string(event->cover_ovl,
				                        sizeof(event->cover_ovl), ovlname);
			}
			merged_wall_log_live_cover_state("exact", shader_kind,
			                                 track->render_pass, track->draw_seq, track->draw_order,
			                                 draw_order,
			                                 ordered, overlap_area, "exact", &track->draw_ctx, &cover_ctx, cover_bitmap,
			                                 pointlist, uvl_list, nv, texfilt_level,
			                                 menu_texfilt, hud_texfilt, aniso_level);
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

			merged_wall_begin_cover_events();
			if (merged_wall_cover_event_count < MERGED_WALL_COVER_EVENT_MAX) {
				event = &merged_wall_cover_events[merged_wall_cover_event_count++];
				memset(event, 0, sizeof(*event));
				event->kind = MERGED_WALL_SNAPSHOT_COVER_BBOX;
				event->face_index = i;
				event->cover_order = draw_order;
				event->cover_ctx = cover_ctx;
				event->cover_bbox_valid = cover_bbox_valid;
				event->cover_min_sx = cover_min_sx;
				event->cover_max_sx = cover_max_sx;
				event->cover_min_sy = cover_min_sy;
				event->cover_max_sy = cover_max_sy;
				event->overlap_area = overlap_area;
				merged_wall_copy_string(event->cover_shader,
				                        sizeof(event->cover_shader), shader_kind);
				merged_wall_copy_string(event->cover_bot,
				                        sizeof(event->cover_bot), botname);
				merged_wall_copy_string(event->cover_ovl,
				                        sizeof(event->cover_ovl), ovlname);
			}
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
		if (g_merged_wall_snapshot_pending) {
			merged_wall_log_face_textures("snapshot_coverbox_cover", 0, &cover_ctx);
			merged_wall_log_live_cover_state("bbox", shader_kind,
			                                 track->render_pass, track->draw_seq, track->draw_order,
			                                 draw_order,
			                                 -1, overlap_area, track_box_kind, &track->draw_ctx, &cover_ctx, cover_bitmap,
			                                 pointlist, uvl_list, nv, texfilt_level,
			                                 menu_texfilt, hud_texfilt, aniso_level);
		}
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

static void merged_wall_log_snapshot_focus_covers(float center_x, float center_y,
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
		}
		if (event->cover_bbox_valid) {
			candidate->center_cover = merged_wall_bbox_contains_point(center_x, center_y,
			                                                          event->cover_min_sx, event->cover_max_sx,
			                                                          event->cover_min_sy, event->cover_max_sy);
			candidate->cover_dist2 = merged_wall_bbox_distance_sq(center_x, center_y,
			                                                      event->cover_min_sx, event->cover_max_sx,
			                                                      event->cover_min_sy, event->cover_max_sy);
		} else {
			candidate->cover_dist2 = candidate->face_dist2;
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
		}
		if (event->overlap_area > overlap_area)
			overlap_area = event->overlap_area;
		candidate->overlap_area = overlap_area;
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
			const char *rank_source = candidate->face_rank > 0 ? "face" : candidate->partial_rank > 0 ? "partial"
			                                                                                          : "none";
			int rank_value = candidate->face_rank > 0 ? candidate->face_rank : candidate->partial_rank;

			debug_log(DLOG_TEXTURE,
			          "[mwall_snapshot_focus_cover] focus_rank=%d focus=%s kind=%s face_rank=%d rank_source=%s center_face=%d center_cover=%d center_overlap=%d dist2=%.1f face_dist2=%.1f cover_dist2=%.1f overlap=%.1f box=%s ordered=%d frame=%d pass=%d seq=%d draw_order=%d cover_order=%d cover_shader=%s cover_bot=%s cover_ovl=%s seg=%d side=%d face=%d child=%d wid=%d tmap1=%d tmap2=0x%x cover_seg=%d cover_side=%d cover_face=%d cover_child=%d cover_wid=%d cover_tmap1=%d cover_tmap2=0x%x",
			          logged_count,
			          merged_wall_snapshot_focus_bucket_name(candidate->focus_bucket),
			          event->kind == MERGED_WALL_SNAPSHOT_COVER_EXACT ? "exact" : "bbox",
			          rank_value,
			          rank_source,
			          candidate->center_face,
			          candidate->center_cover,
			          candidate->center_overlap,
			          candidate->focus_dist2,
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
			merged_wall_log_portal("snapshot_focus_face", &track->draw_ctx);
			merged_wall_log_portal("snapshot_focus_cover", &event->cover_ctx);
			merged_wall_log_face_textures("snapshot_focus_face", logged_count, &track->draw_ctx);
			merged_wall_log_face_textures("snapshot_focus_cover", logged_count, &event->cover_ctx);
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

void android_merged_wall_request_snapshot(void)
{
	const char *level_name = Current_level_name[0] ? Current_level_name : "<none>";

	memset(&g_merged_wall_snapshot_result, 0, sizeof(g_merged_wall_snapshot_result));
	merged_wall_reset_cover_bitmap_dumps();
	merged_wall_copy_string(g_merged_wall_snapshot_result.status,
	                        sizeof(g_merged_wall_snapshot_result.status), "pending");
	g_merged_wall_snapshot_result.request_frame = g_merged_wall_frame_id;
	g_merged_wall_snapshot_request_frame = g_merged_wall_frame_id;
	debug_log(DLOG_TEXTURE,
	          "[mwall_snap] request: request_frame=%d frame=%d pass=%d seq=%d level=%d name=%s mode=%d(%s) experiment=%d(%s) two_pass=%d",
	          (int) g_merged_wall_snapshot_request_frame,
	          (int) g_merged_wall_frame_id,
	          (int) g_merged_wall_render_pass,
	          (int) g_merged_wall_draw_seq,
	          Current_level_num,
	          level_name,
	          (int) g_merged_wall_debug_mode,
	          merged_wall_debug_mode_name_local((int) g_merged_wall_debug_mode),
	          (int) g_merged_wall_experiment_mode,
	          merged_wall_experiment_name_local((int) g_merged_wall_experiment_mode),
	          (int) g_merged_wall_force_two_pass);
	merged_wall_log_snapshot_pose();
	merged_wall_log_snapshot_last_draw();
	__sync_synchronize();
	g_merged_wall_snapshot_pending = 1;
}

void android_merged_wall_finish_snapshot(int screen_w, int screen_h,
                                         int sample_r, int sample_g, int sample_b, int sample_a,
                                         int avg_r, int avg_g, int avg_b, int avg_a)
{
	float center_x = screen_w > 0 ? (float) screen_w * 0.5f : 0.0f;
	float center_y = screen_h > 0 ? (float) screen_h * 0.5f : 0.0f;
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
	          "[mwall_snapshot] stage=frame frame=%d request_frame=%d level=%d name=%s center=%.1f/%.1f fb_rgba=%d/%d/%d/%d fb_avg=%d/%d/%d/%d tracked=%d partial=%d center_hits=%d cover_events=%d",
	          g_merged_wall_frame_id,
	          g_merged_wall_snapshot_request_frame,
	          Current_level_num,
	          level_name,
	          center_x,
	          center_y,
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
		debug_log(DLOG_TEXTURE,
		          "[mwall_snapshot] no_tracked_faces frame=%d request_frame=%d level=%d name=%s",
		          g_merged_wall_frame_id,
		          g_merged_wall_snapshot_request_frame,
		          Current_level_num,
		          level_name);
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
	                                      selected, selected_count,
	                                      partial_selected, partial_selected_count);

	if (selected_count <= 0) {
		merged_wall_copy_string(g_merged_wall_snapshot_result.status,
		                        sizeof(g_merged_wall_snapshot_result.status), "no_projected_faces");
		g_merged_wall_snapshot_result.valid = 1;
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
}

#endif /* ANDROID */
