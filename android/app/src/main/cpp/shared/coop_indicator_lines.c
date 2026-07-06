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
#include "android_log.h"
#include "wall.h"
#include "switch.h"
#include "cntrlcen.h"
#include "gameseg.h"
#include "textures.h"
#ifdef OGL
#include "ogl_init.h"
#endif
#ifdef DXX_BUILD_DESCENT_II
#include "escort.h"
#endif

#include "coop_indicator_lines.h"
#include "coop_indicator_lines_math.h"

/* -- tuning ---------------------------------------------------------- */
#define PATH_UPDATE_INTERVAL 30 /* frames between path recomputation */
#define MAX_INDICATOR_DEPTH  25 /* max path segments to compute/draw */
#define LINE_HALF_WIDTH      (F1_0 / 4)
#define KEEPOUT_RADIUS_MULT  3 /* keep-out sphere = ship radius * N */

/* -- cached paths ---------------------------------------------------- */
typedef struct {
	point_seg segs[MAX_INDICATOR_DEPTH + 4];
	short count;
	int target_objnum; /* object index of target, -1=none   */
	fix alpha;         /* 0..F1_0 display alpha             */
} indicator_path;

static indicator_path s_player_path;
#ifdef DXX_BUILD_DESCENT_II
static indicator_path s_buddy_path;
#endif
static int s_frame_counter;
static int s_show_nearest_player = 1;
static int s_show_guidebot = 1;

/* -- helpers --------------------------------------------------------- */

static void draw_path_segment(const vms_vector *a, const vms_vector *b)
{
	g3s_point pa, pb;

	if (a->x == b->x && a->y == b->y && a->z == b->z)
		return;

	g3_rotate_point(&pa, a);
	g3_rotate_point(&pb, b);
	g3_draw_rod_flat(&pa, LINE_HALF_WIDTH, &pb, LINE_HALF_WIDTH);
}

/* Check whether a segment is currently in the portal-rendered visible set */
static int seg_is_visible(int segnum)
{
	int i;
	for (i = 0; i < N_render_segs; i++)
		if (Render_list[i] == segnum)
			return 1;
	return 0;
}

/* Check whether a target object is well inside the player's screen.
 * Returns 1 if the target's segment is in the render list AND the target
 * position projects to the inner viewport.  Used to suppress indicator lines
 * only once the player can comfortably see the target. */
static int target_is_deep_on_screen(int objnum)
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
	return coop_indicator_target_in_inner_screen(pt.p3_sx, pt.p3_sy,
	                                             Canvas_width, Canvas_height);
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
	if (s_show_nearest_player) {
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
	} else {
		s_player_path.count = 0;
		s_player_path.target_objnum = -1;
	}

#ifdef DXX_BUILD_DESCENT_II
	/* guidebot path -- only show after guidebot has been released */
	if (s_show_guidebot &&
	    Buddy_allowed_to_talk &&
	    Buddy_objnum >= 0 && Buddy_objnum <= Highest_object_index &&
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
	int fade_level = coop_indicator_line_fade_level(path->alpha);

	if (path->count < 2 || fade_level >= GR_FADE_OFF)
		return;

	gr_setcolor(color);
	gr_settransblend(fade_level, GR_BLEND_ADDITIVE_A);

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
			draw_path_segment(a, b);
		} else {
			/* one inside, one outside -- clip to sphere boundary */
			fix t_den = db - da;
			if (t_den == 0)
				continue;
			/* t is the fractional position along A->B where the sphere
			 * boundary is crossed: dist(lerp(A,B,t)) == keepout_r */
			fix t = fixdiv(keepout_r - da, t_den);
			vms_vector delta, clipped;
			vm_vec_sub(&delta, b, a);
			vm_vec_scale_add(&clipped, a, &delta, t);
			if (a_in) {
				/* A inside, B outside: draw clipped->B */
				draw_path_segment(&clipped, b);
			} else {
				/* A outside, B inside: draw A->clipped */
				draw_path_segment(a, &clipped);
			}
		}
	}

	gr_settransblend(GR_FADE_OFF, GR_BLEND_NORMAL);
}

/* -- coop diagnostics ------------------------------------------------ */
static int s_diag_frames;

typedef struct {
	unsigned int texture_sig;
	unsigned int segment_sig;
	unsigned int player_tex_sig;
	int paged_out;
	int gltex_ptrs;
	int gltex_handles;
	int png_handles;
	int mask_handles;
	int mipmapped;
	int invalid_tmaps;
	int first_bad_seg;
	int first_bad_side;
	int first_bad_tmap1;
	int first_bad_tmap2;
} coop_texture_diag;

static unsigned int coop_diag_mix(unsigned int hash, unsigned int value)
{
	hash ^= value;
	hash *= 16777619u;
	return hash;
}

static void coop_collect_texture_diag(coop_texture_diag *diag)
{
	unsigned int hash;
	int i, j;

	memset(diag, 0, sizeof(*diag));
	diag->first_bad_seg = -1;
	diag->first_bad_side = -1;
	diag->first_bad_tmap1 = -1;
	diag->first_bad_tmap2 = -1;

	hash = 2166136261u;
	for (i = 0; i < NumTextures; i++) {
		unsigned int bitmap_index = Textures[i].index;
		hash = coop_diag_mix(hash, (unsigned int) i);
		hash = coop_diag_mix(hash, bitmap_index);
		if (bitmap_index < MAX_BITMAP_FILES) {
			grs_bitmap *bm = &GameBitmaps[bitmap_index];
			hash = coop_diag_mix(hash, (unsigned int) bm->bm_flags);
			hash = coop_diag_mix(hash, (unsigned int) bm->bm_w);
			hash = coop_diag_mix(hash, (unsigned int) bm->bm_h);
			if (bm->bm_flags & BM_FLAG_PAGED_OUT)
				diag->paged_out++;
#ifdef OGL
			if (bm->gltexture) {
				diag->gltex_ptrs++;
				if (bm->gltexture->handle > 0)
					diag->gltex_handles++;
				if (bm->gltexture->is_png)
					diag->png_handles++;
				if (bm->gltexture->has_mipmaps)
					diag->mipmapped++;
			}
			if (bm->gltexture_mask && bm->gltexture_mask->handle > 0)
				diag->mask_handles++;
#endif
		}
	}
	diag->texture_sig = hash;

	hash = 2166136261u;
	for (i = 0; i <= Highest_segment_index; i++) {
		for (j = 0; j < 6; j++) {
			int tmap1 = Segments[i].sides[j].tmap_num;
			int tmap2 = (int) (unsigned short) Segments[i].sides[j].tmap_num2;
			int tmap2_base = tmap2 & 0x3fff;
			int invalid = (tmap1 < 0 || tmap1 >= NumTextures ||
			               (tmap2_base && tmap2_base >= NumTextures));
			hash = coop_diag_mix(hash, (unsigned int) i);
			hash = coop_diag_mix(hash, (unsigned int) j);
			hash = coop_diag_mix(hash, (unsigned int) (unsigned short) tmap1);
			hash = coop_diag_mix(hash, (unsigned int) tmap2);
			if (invalid) {
				diag->invalid_tmaps++;
				if (diag->first_bad_seg < 0) {
					diag->first_bad_seg = i;
					diag->first_bad_side = j;
					diag->first_bad_tmap1 = tmap1;
					diag->first_bad_tmap2 = tmap2;
				}
			}
		}
	}
	diag->segment_sig = hash;

	hash = 2166136261u;
	for (i = 0; i < MAX_PLAYERS; i++) {
		hash = coop_diag_mix(hash, multi_player_tex_color[i]);
		for (j = 0; j < N_PLAYER_SHIP_TEXTURES; j++)
			hash = coop_diag_mix(hash, multi_player_textures[i][j].index);
	}
	for (i = 0; i < N_players && i < MAX_PLAYERS; i++) {
		object *obj = NULL;
		hash = coop_diag_mix(hash, (unsigned int) i);
		hash = coop_diag_mix(hash, (unsigned int) Players[i].connected);
		hash = coop_diag_mix(hash, (unsigned int) Players[i].objnum);
		if (Players[i].objnum >= 0 && Players[i].objnum <= Highest_object_index)
			obj = &Objects[Players[i].objnum];
		if (obj && obj->render_type == RT_POLYOBJ) {
			hash = coop_diag_mix(hash, (unsigned int) obj->type);
			hash = coop_diag_mix(hash, (unsigned int) obj->id);
			hash = coop_diag_mix(hash, (unsigned int) obj->rtype.pobj_info.model_num);
			hash = coop_diag_mix(hash, (unsigned int) obj->rtype.pobj_info.alt_textures);
		}
	}
	diag->player_tex_sig = hash;
}

void coop_indicator_diag_trigger(void)
{
	s_diag_frames = 120; /* ~4 seconds, logged every 10th frame */
}

static void coop_indicator_diag_tick(void)
{
	extern int Player_is_dead;
	extern window *Game_wind;
	coop_texture_diag tex_diag;
	char _diag_buf[512];
	int gw_front;

	if (s_diag_frames <= 0)
		return;

	s_diag_frames--;
	if (s_diag_frames % 10 != 0)
		return;
	if (!ConsoleObject)
		return;

	gw_front = (Game_wind && Game_wind == window_get_front());
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
	debug_log(DLOG_COOP_DESYNC, "[COOP] %s", _diag_buf);

	coop_collect_texture_diag(&tex_diag);
	debug_log(DLOG_COOP_DESYNC,
	          "[COOP] texdiag[%d]: level=%d game_mode=0x%x net_mode=%u net_flags=0x%x "
	          "monitors=0x%x player=%d master=%d n_players=%d custom_tex=%d textures=%d "
	          "first_multi=%d highest_segment=%d texture_sig=%08x segment_sig=%08x "
	          "player_tex_sig=%08x paged=%d gl_ptr=%d gl_handle=%d png=%d mask=%d "
	          "mip=%d invalid_tmaps=%d first_bad=%d:%d:%d/%d",
	          s_diag_frames,
	          Current_level_num,
	          Game_mode,
	          (unsigned int) Netgame.gamemode,
	          (unsigned int) Netgame.game_flags,
	          (unsigned int) Netgame.monitor_vector,
	          Player_num,
	          multi_who_is_master(),
	          N_players,
	          (int) Netgame.AllowCustomModelsTextures,
	          NumTextures,
	          First_multi_bitmap_num,
	          Highest_segment_index,
	          tex_diag.texture_sig,
	          tex_diag.segment_sig,
	          tex_diag.player_tex_sig,
	          tex_diag.paged_out,
	          tex_diag.gltex_ptrs,
	          tex_diag.gltex_handles,
	          tex_diag.png_handles,
	          tex_diag.mask_handles,
	          tex_diag.mipmapped,
	          tex_diag.invalid_tmaps,
	          tex_diag.first_bad_seg,
	          tex_diag.first_bad_side,
	          tex_diag.first_bad_tmap1,
	          tex_diag.first_bad_tmap2);
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

void coop_indicator_lines_set_options(int show_nearest_player, int show_guidebot)
{
	s_show_nearest_player = show_nearest_player ? 1 : 0;
	s_show_guidebot = show_guidebot ? 1 : 0;
	s_frame_counter = 0;
	if (!s_show_nearest_player) {
		s_player_path.count = 0;
		s_player_path.target_objnum = -1;
		s_player_path.alpha = 0;
	}
#ifdef DXX_BUILD_DESCENT_II
	if (!s_show_guidebot) {
		s_buddy_path.count = 0;
		s_buddy_path.target_objnum = -1;
		s_buddy_path.alpha = 0;
	}
#endif
}

/* -- public API ------------------------------------------------------ */

/* 3D path lines -- call from render.c between render_mine/g3_end_frame */
void coop_indicator_lines_render(void)
{
	int color_green, color_blue;
	int is_coop = coop_qol_active();
	int want_player_line = is_coop && s_show_nearest_player;

#ifdef DXX_BUILD_DESCENT_II
	/* buddy path line works in single player and coop */
	int show_buddy = s_show_guidebot &&
	                 Buddy_allowed_to_talk &&
	                 Buddy_objnum >= 0 && Buddy_objnum <= Highest_object_index &&
	                 Objects[Buddy_objnum].type == OBJ_ROBOT;
#else
	int show_buddy = 0;
#endif

	if (!want_player_line)
		s_player_path.alpha = 0;
#ifdef DXX_BUILD_DESCENT_II
	if (!show_buddy)
		s_buddy_path.alpha = 0;
#endif

	coop_indicator_diag_tick();

	if (!want_player_line && !show_buddy)
		return;

	/* throttled path update */
	if (s_frame_counter <= 0) {
		if (want_player_line || show_buddy)
			update_paths();
		s_frame_counter = PATH_UPDATE_INTERVAL;
	}
	s_frame_counter--;

	color_green = BM_XRGB(10, 31, 10);
	color_blue = BM_XRGB(10, 10, 31);

	if (want_player_line) {
		s_player_path.alpha = coop_indicator_line_advance_alpha(
		    s_player_path.alpha,
		    s_player_path.count >= 2 &&
		        !target_is_deep_on_screen(s_player_path.target_objnum),
		    FrameTime);
		draw_path_lines(&s_player_path, color_green);
	} else {
		s_player_path.alpha = 0;
	}

#ifdef DXX_BUILD_DESCENT_II
	if (show_buddy) {
		s_buddy_path.alpha = coop_indicator_line_advance_alpha(
		    s_buddy_path.alpha,
		    s_buddy_path.count >= 2 &&
		        !target_is_deep_on_screen(s_buddy_path.target_objnum),
		    FrameTime);
		draw_path_lines(&s_buddy_path, color_blue);
	} else {
		s_buddy_path.alpha = 0;
	}
#endif
}

#endif /* __ANDROID__ */
