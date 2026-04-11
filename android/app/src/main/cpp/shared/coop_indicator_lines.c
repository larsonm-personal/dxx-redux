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
#include "wall.h"
#include "switch.h"
#include "cntrlcen.h"
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

/* Check whether a target object is directly visible on the player's screen.
 * Returns 1 if the target's segment is in the render list AND the target
 * position projects to within the 3D viewport.  Used to suppress indicator
 * lines when the player can already see the target. */
static int target_is_on_screen(int objnum)
{
	/* globvars.h is a private 3d/ header not on our include path */
	extern int Canvas_width, Canvas_height;
	g3s_point pt;
	if (objnum < 0 || objnum > Highest_object_index)
		return 0;
	if (!seg_is_visible(Objects[objnum].segnum))
		return 0;
	g3_rotate_point(&pt, &Objects[objnum].pos);
	if (pt.p3_codes & CC_BEHIND)
		return 0;
	g3_project_point(&pt);
	if (pt.p3_flags & PF_OVERFLOW)
		return 0;
	if (pt.p3_sx < 0 || pt.p3_sx > i2f(Canvas_width))
		return 0;
	if (pt.p3_sy < 0 || pt.p3_sy > i2f(Canvas_height))
		return 0;
	return 1;
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

/* Find the segment containing the mine exit trigger, or -1 */
static int find_exit_segment(void)
{
	int i, j;
	for (i = 0; i < Num_triggers; i++) {
#ifdef DXX_BUILD_DESCENT_II
		if (Triggers[i].type != TT_EXIT)
			continue;
#else
		if (!(Triggers[i].flags & TRIGGER_EXIT))
			continue;
#endif
		for (j = 0; j < Num_walls; j++)
			if (Walls[j].trigger == i)
				return Walls[j].segnum;
	}
	return -1;
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

	/* nearest player path -- fall back to exit when reactor blown and
	 * no other players are in the mine */
	{
		int pi = find_nearest_player();
		if (pi >= 0) {
			object *pobj = &Objects[Players[pi].objnum];
			s_player_path.target_objnum = Players[pi].objnum;
			compute_path(&s_player_path, my_seg, pobj->segnum);
		} else if (Control_center_destroyed) {
			int exit_seg = find_exit_segment();
			if (exit_seg >= 0) {
				s_player_path.target_objnum = -1;
				compute_path(&s_player_path, my_seg, exit_seg);
			} else {
				s_player_path.count = 0;
				s_player_path.target_objnum = -1;
			}
		} else {
			s_player_path.count = 0;
			s_player_path.target_objnum = -1;
		}
	}

#ifdef DXX_BUILD_DESCENT_II
	/* guidebot path -- show whenever companion robot exists (even if caged) */
	if (Buddy_objnum >= 0 && Buddy_objnum <= Highest_object_index &&
	    Objects[Buddy_objnum].type == OBJ_ROBOT) {
		s_buddy_path.target_objnum = Buddy_objnum;
		compute_path(&s_buddy_path, my_seg, Objects[Buddy_objnum].segnum);
	} else {
		s_buddy_path.count = 0;
		s_buddy_path.target_objnum = -1;
	}
#endif
}

/* -- drawing --------------------------------------------------------- */

/* Draw a 3D path, clipping each line segment independently against
 * a keep-out sphere (3x ship radius) centered on the player ship.
 * For each segment A->B:
 *   both outside sphere: draw full segment
 *   A inside, B outside: clip A to sphere boundary, draw clipped->B
 *   A outside, B inside: clip B to sphere boundary, draw A->clipped
 *   both inside: skip */
static void draw_path_lines(const indicator_path *path, int color)
{
	int i;
	fix keepout_r = ConsoleObject->size * KEEPOUT_RADIUS_MULT;

	if (path->count < 2)
		return;

	gr_setcolor(color);
	gr_settransblend(LINE_FADE_LEVEL, GR_BLEND_ADDITIVE_A);

	for (i = 0; i < path->count - 1; i++) {
		const vms_vector *a = &path->segs[i].point;
		const vms_vector *b = &path->segs[i + 1].point;
		int seg_a = path->segs[i].segnum;
		int seg_b = path->segs[i + 1].segnum;
		fix da, db;
		int a_in, b_in;

		/* only draw if at least one endpoint's segment is visible */
		if (!seg_is_visible(seg_a) && !seg_is_visible(seg_b))
			continue;

		da = vm_vec_dist_quick(&ConsoleObject->pos, (vms_vector *) a);
		db = vm_vec_dist_quick(&ConsoleObject->pos, (vms_vector *) b);
		a_in = (da < keepout_r);
		b_in = (db < keepout_r);

		if (a_in && b_in)
			continue; /* both inside sphere */

		if (!a_in && !b_in) {
			/* both outside -- draw full segment */
			g3s_point pa, pb;
			g3_rotate_point(&pa, a);
			g3_rotate_point(&pb, b);
			g3_draw_line(&pa, &pb);
		} else {
			/* one inside, one outside -- clip to sphere boundary */
			fix t_den = db - da;
			if (t_den == 0)
				continue;
			/* t is the fractional position along A->B where the sphere
			 * boundary is crossed: dist(lerp(A,B,t)) == keepout_r */
			fix t = fixdiv(keepout_r - da, t_den);
			vms_vector delta, clipped;
			g3s_point p_out, p_clip;
			vm_vec_sub(&delta, b, a);
			vm_vec_scale_add(&clipped, a, &delta, t);
			g3_rotate_point(&p_clip, &clipped);
			if (a_in) {
				/* A inside, B outside: draw clipped->B */
				g3_rotate_point(&p_out, b);
				g3_draw_line(&p_clip, &p_out);
			} else {
				/* A outside, B inside: draw A->clipped */
				g3_rotate_point(&p_out, a);
				g3_draw_line(&p_out, &p_clip);
			}
		}
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
	int is_coop = coop_qol_active();

#ifdef DXX_BUILD_DESCENT_II
	/* buddy path line works in single player and coop */
	int show_buddy = Buddy_objnum >= 0 && Buddy_objnum <= Highest_object_index &&
	                 Objects[Buddy_objnum].type == OBJ_ROBOT;
#else
	int show_buddy = 0;
#endif

	if (!is_coop && !show_buddy)
		return;

	/* throttled path update */
	if (s_frame_counter <= 0) {
		if (is_coop)
			update_paths();
#ifdef DXX_BUILD_DESCENT_II
		else if (show_buddy) {
			/* single-player: only update buddy path */
			int my_seg = ConsoleObject->segnum;
			s_buddy_path.target_objnum = Buddy_objnum;
			compute_path(&s_buddy_path, my_seg, Objects[Buddy_objnum].segnum);
			s_player_path.count = 0;
		}
#endif
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

	color_green = BM_XRGB(10, 31, 10);
	color_blue = BM_XRGB(10, 10, 31);

	/* Skip path line when the target is directly visible on screen */
	if (!target_is_on_screen(s_player_path.target_objnum))
		draw_path_lines(&s_player_path, color_green);

#ifdef DXX_BUILD_DESCENT_II
	if (!target_is_on_screen(s_buddy_path.target_objnum))
		draw_path_lines(&s_buddy_path, color_blue);
#endif
}

#endif /* __ANDROID__ */
