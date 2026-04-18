#ifdef ANDROID

#include <stdio.h>
#include <string.h>

#include "3d.h"
#include "gameseg.h"
#include "inferno.h"
#include "object.h"
#include "piggy.h"
#include "segment.h"
#include "textures.h"
#include "wall.h"

#include "android_log.h"
#include "merged_wall_debug.h"

#define MERGED_WALL_LOG_PT_COUNT         4
#define MERGED_WALL_TRACKED_FACE_MAX     32
#define MERGED_WALL_COVER_EVENT_MAX      64
#define MERGED_WALL_SNAPSHOT_COVER_EXACT 1
#define MERGED_WALL_SNAPSHOT_COVER_BBOX  2

struct merged_wall_tracked_face {
	int render_pass;
	int draw_seq;
	int draw_order;
	int nv;
	struct android_draw_face_context draw_ctx;
	int cover_logged;
	int coverbox_logged;
	int bbox_valid;
	float min_sx;
	float max_sx;
	float min_sy;
	float max_sy;
	float bbox_area;
	fix pts[MERGED_WALL_LOG_PT_COUNT][3];
	char route[24];
	char merge_impl[24];
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
struct android_draw_face_context g_android_draw_face_ctx = { 0 };
struct merged_wall_snapshot_result g_merged_wall_snapshot_result = { 0 };

static struct merged_wall_tracked_face merged_wall_tracked_faces[MERGED_WALL_TRACKED_FACE_MAX];
static struct merged_wall_snapshot_cover_event merged_wall_cover_events[MERGED_WALL_COVER_EVENT_MAX];
static int merged_wall_tracked_face_count = 0;
static int merged_wall_tracked_frame_id = -1;
static int merged_wall_draw_order = 0;
static int merged_wall_cover_frame_id = -1;
static int merged_wall_cover_event_count = 0;

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

static int merged_wall_bbox_contains_point(float px, float py,
                                           float min_sx, float max_sx, float min_sy, float max_sy)
{
	return px >= min_sx && px <= max_sx && py >= min_sy && py <= max_sy;
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
	int used[MERGED_WALL_LOG_PT_COUNT] = { 0, 0, 0, 0 };

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
                                    int draw_order, const char *route, const char *merge_impl)
{
	struct merged_wall_tracked_face *track;
	int i;

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
	merged_wall_copy_string(track->route, sizeof(track->route), route);
	merged_wall_copy_string(track->merge_impl, sizeof(track->merge_impl), merge_impl);
	for (i = 0; i < nv; i++) {
		track->pts[i][0] = pointlist[i]->p3_vec.x;
		track->pts[i][1] = pointlist[i]->p3_vec.y;
		track->pts[i][2] = pointlist[i]->p3_vec.z;
	}
}

void android_merged_wall_log_cover(const char *shader_kind, const char *botname,
                                   const char *ovlname, const struct g3s_point **pointlist, int nv,
                                   int draw_order)
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
				event->overlap_area = cover_bbox_valid && track->bbox_valid
				                          ? merged_wall_bbox_overlap_area(track->min_sx, track->max_sx,
				                                                          track->min_sy, track->max_sy,
				                                                          cover_min_sx, cover_max_sx,
				                                                          cover_min_sy, cover_max_sy)
				                          : 0.0f;
				merged_wall_copy_string(event->cover_shader,
				                        sizeof(event->cover_shader), shader_kind);
				merged_wall_copy_string(event->cover_bot,
				                        sizeof(event->cover_bot), botname);
				merged_wall_copy_string(event->cover_ovl,
				                        sizeof(event->cover_ovl), ovlname);
			}
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
		struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[i];

		if (track->coverbox_logged || draw_order <= track->draw_order || !track->bbox_valid || track->bbox_area <= 0.0f)
			continue;

		overlap_area = merged_wall_bbox_overlap_area(track->min_sx, track->max_sx,
		                                             track->min_sy, track->max_sy,
		                                             cover_min_sx, cover_max_sx,
		                                             cover_min_sy, cover_max_sy);
		if (overlap_area < 64.0f)
			continue;
		if (overlap_area < track->bbox_area * 0.20f && overlap_area < cover_bbox_area * 0.20f)
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
		          "[mwall_coverbox] frame=%d face_pass=%d face_seq=%d face_order=%d cover_order=%d cover_shader=%s cover_bot=%s cover_ovl=%s seg=%d side=%d face=%d child=%d side_type=%d wid=%d tmap1=%d tmap2=0x%x cover_seg=%d cover_side=%d cover_face=%d cover_child=%d cover_side_type=%d cover_wid=%d cover_tmap1=%d cover_tmap2=0x%x overlap=%.1f",
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

void android_merged_wall_request_snapshot(void)
{
	memset(&g_merged_wall_snapshot_result, 0, sizeof(g_merged_wall_snapshot_result));
	merged_wall_copy_string(g_merged_wall_snapshot_result.status,
	                        sizeof(g_merged_wall_snapshot_result.status), "pending");
	g_merged_wall_snapshot_result.request_frame = g_merged_wall_frame_id;
	g_merged_wall_snapshot_request_frame = g_merged_wall_frame_id;
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
	int selected_count = 0;
	int center_hit_count = 0;
	int relevant_cover_logs = 0;
	int omitted_cover_logs = 0;
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
	for (i = 0; i < merged_wall_tracked_face_count; i++) {
		const struct merged_wall_tracked_face *track = &merged_wall_tracked_faces[i];

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
	          "[mwall_snapshot] stage=frame frame=%d center=%.1f/%.1f fb_rgba=%d/%d/%d/%d fb_avg=%d/%d/%d/%d tracked=%d center_hits=%d cover_events=%d",
	          g_merged_wall_frame_id,
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
	          center_hit_count,
	          merged_wall_cover_event_count);

	if (merged_wall_tracked_frame_id != g_merged_wall_frame_id || merged_wall_tracked_face_count <= 0) {
		merged_wall_copy_string(g_merged_wall_snapshot_result.status,
		                        sizeof(g_merged_wall_snapshot_result.status), "no_tracked_faces");
		g_merged_wall_snapshot_result.valid = 1;
		debug_log(DLOG_TEXTURE,
		          "[mwall_snapshot] no_tracked_faces frame=%d request_frame=%d",
		          g_merged_wall_frame_id,
		          g_merged_wall_snapshot_request_frame);
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
				if (best_index < 0 || dist2 < best_dist2 - 0.5f || (dist2 <= best_dist2 + 0.5f && (track->draw_order > merged_wall_tracked_faces[best_index].draw_order || (track->draw_order == merged_wall_tracked_faces[best_index].draw_order && track->bbox_area > merged_wall_tracked_faces[best_index].bbox_area)))) {
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
		merged_wall_copy_string(g_merged_wall_snapshot_result.status,
		                        sizeof(g_merged_wall_snapshot_result.status), "no_projected_faces");
		g_merged_wall_snapshot_result.valid = 1;
		debug_log(DLOG_TEXTURE,
		          "[mwall_snapshot] no_projected_faces frame=%d",
		          g_merged_wall_frame_id);
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
		merged_wall_copy_string(out->route, sizeof(out->route), track->route);
		merged_wall_copy_string(out->merge_impl, sizeof(out->merge_impl), track->merge_impl);
		debug_log(DLOG_TEXTURE,
		          "[mwall_snapshot_face] rank=%d center_hit=%d dist2=%.1f frame=%d pass=%d seq=%d order=%d seg=%d side=%d face=%d child=%d side_type=%d wid=%d tmap1=%d tmap2=0x%x route=%s merge_impl=%s box=%.1f..%.1f/%.1f..%.1f area=%.1f",
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
		          out->min_sx,
		          out->max_sx,
		          out->min_sy,
		          out->max_sy,
		          out->bbox_area);
		merged_wall_log_portal("snapshot_face", &track->draw_ctx);
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