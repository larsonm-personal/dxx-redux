/* android port: coop indicator lines
 * Draws faint 3D path lines to the guidebot (D2, light blue) and nearest
 * player (light green) during coop gameplay.  Lines inside a keep-out
 * sphere around the player ship are not drawn.
 *
 * Shared between D1 and D2 builds -- guidebot code is D2-only.
 */
#ifdef __ANDROID__

#include <string.h>
#include "3d.h"
#include "gr.h"
#include "vecmat.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "multi.h"
#include "render.h"
#include "ai.h"
#include "aistruct.h"
#include "console.h"
#include "kconfig.h"
#include "window.h"
#include "android_net_log.h"
#ifdef DXX_BUILD_DESCENT_II
#include "escort.h"
#endif

#include "coop_indicator_lines.h"

/* -- tuning ---------------------------------------------------------- */
#define PATH_UPDATE_INTERVAL 30 /* frames between path recomputation */
#define MAX_INDICATOR_DEPTH  25 /* max path segments to compute/draw */
#define LINE_FADE_LEVEL      20 /* higher = more transparent (0-31)  */
#define KEEPOUT_RADIUS_MULT  3  /* keep-out sphere = ship radius * N */

/* -- cached paths ---------------------------------------------------- */
typedef struct {
	point_seg segs[MAX_INDICATOR_DEPTH + 4];
	short count;
	int target_objnum; /* object index of target, -1=none   */
} indicator_path;

static indicator_path s_player_path;
#ifdef DXX_BUILD_DESCENT_II
static indicator_path s_buddy_path;
#endif
static int s_frame_counter;

/* -- helpers --------------------------------------------------------- */

/* Check whether a segment is currently in the portal-rendered visible set */
static int seg_is_visible(int segnum)
{
	int i;
	for (i = 0; i < N_render_segs; i++)
		if (Render_list[i] == segnum)
			return 1;
	return 0;
}

/* Find the nearest connected coop player. Returns player index or -1 */
static int find_nearest_player(void)
{
	int best = -1;
	fix best_dist = 0x7fffffff;
	int i;

	for (i = 0; i < N_players; i++) {
		fix d;
		object *obj;
		if (i == Player_num)
			continue;
		if (Players[i].connected != CONNECT_PLAYING)
			continue;
		obj = &Objects[Players[i].objnum];
		if (obj->type != OBJ_PLAYER)
			continue;
		d = vm_vec_dist_quick(&ConsoleObject->pos, &obj->pos);
		if (d < best_dist) {
			best_dist = d;
			best = i;
		}
	}
	return best;
}

/* Compute a segment path between two segments into dst.
 * Overwrites the first waypoint with the player's actual position so
 * the line originates from the ship rather than a segment center. */
static void compute_path(indicator_path *dst, int start_seg, int end_seg)
{
	dst->count = 0;
	if (start_seg < 0 || end_seg < 0 || start_seg == end_seg)
		return;
	create_path_points(ConsoleObject, start_seg, end_seg,
	                   dst->segs, &dst->count,
	                   MAX_INDICATOR_DEPTH, 0, 0, -1);
	if (dst->count > MAX_INDICATOR_DEPTH)
		dst->count = MAX_INDICATOR_DEPTH;
	/* anchor first point to the player ship */
	if (dst->count > 0)
		dst->segs[0].point = ConsoleObject->pos;
}

/* -- path update (throttled) ----------------------------------------- */

static void update_paths(void)
{
	int my_seg = ConsoleObject->segnum;

	/* nearest player path */
	{
		int pi = find_nearest_player();
		if (pi >= 0) {
			object *pobj = &Objects[Players[pi].objnum];
			s_player_path.target_objnum = Players[pi].objnum;
			compute_path(&s_player_path, my_seg, pobj->segnum);
		} else {
			s_player_path.count = 0;
			s_player_path.target_objnum = -1;
		}
	}

#ifdef DXX_BUILD_DESCENT_II
	/* guidebot path -- always compute from player to guidebot */
	if (Buddy_objnum >= 0 && Buddy_objnum <= Highest_object_index &&
	    Objects[Buddy_objnum].type == OBJ_ROBOT) {
		object *buddy = &Objects[Buddy_objnum];
		s_buddy_path.target_objnum = Buddy_objnum;
		compute_path(&s_buddy_path, my_seg, buddy->segnum);
	} else {
		s_buddy_path.count = 0;
		s_buddy_path.target_objnum = -1;
	}
#endif
}

/* -- drawing --------------------------------------------------------- */

/* Check if a target object is currently visible on screen (its segment
 * is in the portal-rendered set). If so, no path line needed. */
static int target_is_on_screen(int objnum)
{
	if (objnum < 0 || objnum > Highest_object_index)
		return 0;
	return seg_is_visible(Objects[objnum].segnum);
}

/* Draw a faint 3D path for visible segments, clipping lines against
 * a keep-out sphere around the player ship so the near-ship area
 * stays clear. */
static void draw_path_lines(const indicator_path *path, int color)
{
	int i;
	g3s_point prev_pt, cur_pt;
	int prev_visible, cur_visible;
	int prev_valid = 0;
	int prev_in_keepout = 0;
	int past_keepout = 0; /* once we exit the sphere, stay out */
	fix prev_dist = 0;
	fix keepout_r = ConsoleObject->size * KEEPOUT_RADIUS_MULT;

	if (path->count < 2)
		return;

	/* skip if the target is already visible on screen */
	if (target_is_on_screen(path->target_objnum))
		return;

	gr_setcolor(color);
	gr_settransblend(LINE_FADE_LEVEL, GR_BLEND_ADDITIVE_A);

	for (i = 0; i < path->count; i++) {
		fix dist = vm_vec_dist_quick(&ConsoleObject->pos,
		                             &path->segs[i].point);
		/* Only apply keepout near the start of the path; once clear
		 * of the sphere, don't re-enter (mid-path segments can be
		 * geographically close through walls) */
		int in_keepout = (!past_keepout && dist < keepout_r);
		if (!in_keepout && !past_keepout && prev_valid)
			past_keepout = 1;

		cur_visible = seg_is_visible(path->segs[i].segnum);
		g3_rotate_point(&cur_pt, &path->segs[i].point);

		if (prev_valid && (prev_visible || cur_visible)) {
			if (!prev_in_keepout && !in_keepout) {
				/* both outside sphere -- draw full segment */
				g3_draw_line(&prev_pt, &cur_pt);
			} else if (prev_in_keepout && !in_keepout) {
				/* crossing OUT of sphere: clip start to sphere boundary */
				fix t_den = dist - prev_dist;
				if (t_den > 0) {
					vms_vector delta, clipped;
					g3s_point clipped_pt;
					fix t = fixdiv(keepout_r - prev_dist, t_den);
					vm_vec_sub(&delta, &path->segs[i].point,
					           &path->segs[i - 1].point);
					vm_vec_scale_add(&clipped,
					                 &path->segs[i - 1].point,
					                 &delta, t);
					g3_rotate_point(&clipped_pt, &clipped);
					g3_draw_line(&clipped_pt, &cur_pt);
				}
			}
			/* no INTO-sphere or both-inside cases after past_keepout */
		}

		prev_pt = cur_pt;
		prev_visible = cur_visible;
		prev_in_keepout = in_keepout;
		prev_dist = dist;
		prev_valid = 1;
	}

	gr_settransblend(GR_FADE_OFF, GR_BLEND_NORMAL);
}

/* -- post-restore diagnostics ---------------------------------------- */
static int s_diag_frames;

void coop_indicator_diag_trigger(void)
{
	s_diag_frames = 120; /* ~4 seconds, logged every 10th frame */
}

/* -- common coop check ----------------------------------------------- */
static int coop_qol_active(void)
{
	if (!(Game_mode & GM_MULTI_COOP))
		return 0;
	if (!(Netgame.game_flags & NETGAME_FLAG_COOP_QOL))
		return 0;
	return 1;
}

/* -- public API ------------------------------------------------------ */

/* 3D path lines -- call from render.c between render_mine/g3_end_frame */
void coop_indicator_lines_render(void)
{
	int color_green, color_blue;

	if (!coop_qol_active())
		return;

	/* throttled path update */
	if (s_frame_counter <= 0) {
		update_paths();
		s_frame_counter = PATH_UPDATE_INTERVAL;
	}
	s_frame_counter--;

	/* per-frame diagnostics after coop restore (every 10th frame) */
	if (s_diag_frames > 0) {
		s_diag_frames--;
		if (s_diag_frames % 10 == 0) {
			extern int Player_is_dead;
			extern window *Game_wind;
			char _diag_buf[512];
			int gw_front = (Game_wind && Game_wind == window_get_front());
			snprintf(_diag_buf, sizeof(_diag_buf),
			         "diag[%d]: ct=%d mt=%d pf=0x%x dead=%d"
			         " vel=%d,%d,%d thrust=%d,%d,%d"
			         " fwd=%d pitch=%d hdg=%d side=%d"
			         " gw_front=%d",
			         s_diag_frames,
			         ConsoleObject->control_type,
			         ConsoleObject->movement_type,
			         ConsoleObject->mtype.phys_info.flags,
			         Player_is_dead,
			         ConsoleObject->mtype.phys_info.velocity.x,
			         ConsoleObject->mtype.phys_info.velocity.y,
			         ConsoleObject->mtype.phys_info.velocity.z,
			         ConsoleObject->mtype.phys_info.thrust.x,
			         ConsoleObject->mtype.phys_info.thrust.y,
			         ConsoleObject->mtype.phys_info.thrust.z,
			         Controls.forward_thrust_time,
			         Controls.pitch_time,
			         Controls.heading_time,
			         Controls.sideways_thrust_time,
			         gw_front);
			con_printf(CON_NORMAL, "%s", _diag_buf);
			android_net_log("COOP", _diag_buf);
		}
	}

	/* re-anchor first waypoint to current player position every frame
	 * so the keepout-sphere distance for point 0 is always 0 */
	if (s_player_path.count > 0)
		s_player_path.segs[0].point = ConsoleObject->pos;
#ifdef DXX_BUILD_DESCENT_II
	if (s_buddy_path.count > 0)
		s_buddy_path.segs[0].point = ConsoleObject->pos;
#endif

	color_green = BM_XRGB(10, 31, 10);
	color_blue = BM_XRGB(10, 10, 31);

	draw_path_lines(&s_player_path, color_green);

#ifdef DXX_BUILD_DESCENT_II
	draw_path_lines(&s_buddy_path, color_blue);
#endif
}

#endif /* __ANDROID__ */
