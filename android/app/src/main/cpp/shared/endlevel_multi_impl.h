/* Included by both endlevel engines after their flythrough implementation */
#include "timer.h"
#include "gamefont.h"

static fix64 Multiplayer_exit_time[MAX_PLAYERS];
static unsigned Multiplayer_exited;
#ifdef __ANDROID__
#include "coop/coop_world_visit.h"
static uint64_t Multiplayer_visit;
#endif
static int Multiplayer_track_ready;
static object Multiplayer_track_start;
static struct {
	object ship;
	fix64 elapsed;
	fix outside_time;
	int started, outside, finished, visible, label_visible;
} Multiplayer_flyouts[MAX_PLAYERS];

void endlevel_multi_reset(void)
{
#ifdef __ANDROID__
	Multiplayer_visit = coop_world_visit_current();
#endif
	Multiplayer_exited = 0;
	Multiplayer_track_ready = 0;
	memset(Multiplayer_flyouts, 0, sizeof(Multiplayer_flyouts));
}

static void endlevel_multi_check_world(void)
{
#ifdef __ANDROID__
	if (Multiplayer_visit != coop_world_visit_current()) endlevel_multi_reset();
#endif
}

void endlevel_multi_note_exit(int player, uint32_t age_ms)
{
	endlevel_multi_check_world();
	if (player < 0 || player >= MAX_PLAYERS || age_ms == UINT32_MAX) return;
	fix64 started = timer_query() - (fix64) age_ms * F1_0 / 1000;
	/* Retries/status snapshots may move a clock earlier, never restart it */
	if (!(Multiplayer_exited & (1u << player)) || started < Multiplayer_exit_time[player])
		Multiplayer_exit_time[player] = started;
	Multiplayer_exited |= 1u << player;
}

uint32_t endlevel_multi_exit_age(int player)
{
	endlevel_multi_check_world();
	if (player < 0 || player >= MAX_PLAYERS || !(Multiplayer_exited & (1u << player))) return UINT32_MAX;
	fix64 elapsed = timer_query() - Multiplayer_exit_time[player];
	if (elapsed < 0) elapsed = 0;
	uint64_t ms = (uint64_t) elapsed * 1000 / F1_0;
	return ms >= UINT32_MAX ? UINT32_MAX - 1 : (uint32_t) ms;
}

void endlevel_multi_begin(void)
{
	endlevel_multi_check_world();
	Multiplayer_track_ready = (Game_mode & GM_MULTI) != 0;
	Multiplayer_track_start = *ConsoleObject;
	memset(Multiplayer_flyouts, 0, sizeof(Multiplayer_flyouts));
}

void endlevel_multi_end(void)
{
	/* Keep exit clocks available to peers still watching this world */
	Multiplayer_track_ready = 0;
	memset(Multiplayer_flyouts, 0, sizeof(Multiplayer_flyouts));
}

int endlevel_multi_local_finished(void)
{
	return Endlevel_sequence == EL_STOPPED && Endlevel_frame.timer < 0;
}

void endlevel_multi_frame(void)
{
	if (!Multiplayer_track_ready || !(Game_mode & GM_MULTI)) return;
	for (int p = 0; p < N_players; ++p) {
		uint32_t age = endlevel_multi_exit_age(p);
		if (p == Player_num || age == UINT32_MAX || Players[p].connected == CONNECT_DISCONNECTED ||
		    Players[p].connected == CONNECT_FOUND_SECRET ||
		    (Netgame.max_numobservers && p == OBSERVER_PLAYER_ID)) continue;
		object *ship = &Multiplayer_flyouts[p].ship;
		if (!Multiplayer_flyouts[p].started) {
			*ship = Multiplayer_track_start;
			ship->id = p;
			ship->type = OBJ_PLAYER;
			ship->control_type = CT_NONE;
			ship->movement_type = MT_NONE;
			start_endlevel_flythrough(2 + p, ship, FLY_SPEED);
			do_endlevel_flythrough_step(2 + p, 0);
			Multiplayer_flyouts[p].started = 1;
		}
		/* Bound catch-up for a peer that finished long before this viewer */
		if (age > 600000) Multiplayer_flyouts[p].finished = 1;
		fix64 target = (fix64) age * F1_0 / 1000;
		while (!Multiplayer_flyouts[p].finished && Multiplayer_flyouts[p].elapsed < target) {
			fix step = (fix) min(target - Multiplayer_flyouts[p].elapsed, F1_0 / 60);
			if (Multiplayer_flyouts[p].outside) {
				vm_vec_scale_add2(&ship->pos, &ship->orient.fvec, fixmul(step, FLY_SPEED));
				Multiplayer_flyouts[p].outside_time += step;
				if (Multiplayer_flyouts[p].outside_time >= i2f(6)) Multiplayer_flyouts[p].finished = 1;
			} else {
				do_endlevel_flythrough_step(2 + p, step);
				vms_vector from_exit;
				vm_vec_sub(&from_exit, &ship->pos, &mine_side_exit_point);
				if (ship->segnum == exit_segnum && vm_vec_dot(&from_exit, &mine_exit_orient.fvec) > 0)
					Multiplayer_flyouts[p].outside = 1;
			}
			Multiplayer_flyouts[p].elapsed += step;
		}
	}
}

static int endlevel_multi_visible(int player)
{
	if (!Multiplayer_track_ready || player == Player_num || !Multiplayer_flyouts[player].started ||
	    Multiplayer_flyouts[player].finished || Players[player].connected == CONNECT_DISCONNECTED ||
	    Players[player].connected == CONNECT_FOUND_SECRET) return 0;
	if (Endlevel_sequence >= EL_OUTSIDE) return Multiplayer_flyouts[player].outside;
	if (Multiplayer_flyouts[player].outside) return 0;
	/* Presentation actors are detached from Objects; test walls without objects */
	fvi_query query;
	fvi_info hit;
	memset(&query, 0, sizeof(query));
	query.p0 = &Viewer_eye;
	query.p1 = &Multiplayer_flyouts[player].ship.pos;
	query.startseg = find_point_seg(&Viewer_eye, Viewer->segnum);
	if (query.startseg < 0) return 0;
	query.thisobjnum = -1;
	find_vector_intersection(&query, &hit);
	return hit.hit_type == HIT_NONE;
}

void endlevel_multi_render(void)
{
	for (int p = 0; p < MAX_PLAYERS; ++p)
		Multiplayer_flyouts[p].visible = Multiplayer_flyouts[p].label_visible = 0;
	if (!(Game_mode & GM_MULTI)) return;
	for (int p = 0; p < N_players; ++p) {
		if (!endlevel_multi_visible(p)) continue;
		object *ship = &Multiplayer_flyouts[p].ship;
		g3s_point point;
		g3_rotate_point(&point, &ship->pos);
		if (point.p3_codes) continue;
		g3_project_point(&point);
		if (point.p3_flags & PF_OVERFLOW) continue;
		/* Draw the model directly: these actors have no gameplay object slot */
		int objnum = Players[p].objnum;
		if (objnum < 0 || objnum > Highest_object_index) continue;
		int textures = Objects[objnum].rtype.pobj_info.alt_textures;
		g3s_lrgb light = { F1_0 * 2, F1_0 * 2, F1_0 * 2 };
		fix glow = F1_0;
		draw_polygon_model(&ship->pos, &ship->orient, ship->rtype.pobj_info.anim_angles,
		                   ship->rtype.pobj_info.model_num, 0, light, &glow,
		                   textures > 0 ? multi_player_textures[textures - 1] : NULL);
		Multiplayer_flyouts[p].visible = 1;
	}
}

void endlevel_multi_render_names(void)
{
	/* Use the HUD pass after g3_end_frame, with depth testing disabled */
	if (!(Game_mode & GM_MULTI) || !Multiplayer_track_ready) return;
	gr_set_curfont(GAME_FONT);
	for (int p = 0; p < N_players; ++p) {
		if (!Multiplayer_flyouts[p].visible) continue;
		g3s_point point;
		g3_rotate_point(&point, &Multiplayer_flyouts[p].ship.pos);
		g3_project_point(&point);
		int w, h, aw;
		const rgb *colors = Netgame.BlackAndWhitePyros ? player_rgb_alt : player_rgb;
		int color = get_color_for_player(p, 0);
		gr_get_string_size(Players[p].callsign, &w, &h, &aw);
		gr_set_fontcolor(BM_XRGB(colors[color].r, colors[color].g, colors[color].b), -1);
		fix radius = fixmuldiv(fixmul(Multiplayer_flyouts[p].ship.size, Matrix_scale.y),
		                       i2f(grd_curcanv->cv_bitmap.bm_h) / 2, point.p3_z);
		gr_string(f2i(point.p3_sx) - w / 2, f2i(point.p3_sy + radius) + FSPACY(1), Players[p].callsign);
		Multiplayer_flyouts[p].label_visible = 1;
	}
}

void endlevel_multi_get_actor(int player, endlevel_multi_actor_state *state)
{
	memset(state, 0, sizeof(*state));
	state->age_ms = endlevel_multi_exit_age(player);
	if (player < 0 || player >= MAX_PLAYERS) return;
	state->active = Multiplayer_track_ready && Multiplayer_flyouts[player].started && !Multiplayer_flyouts[player].finished &&
	                Players[player].connected != CONNECT_DISCONNECTED && Players[player].connected != CONNECT_FOUND_SECRET;
	state->outside = Multiplayer_flyouts[player].outside;
	state->finished = Multiplayer_flyouts[player].finished;
	state->visible = Multiplayer_flyouts[player].visible;
	state->label_visible = Multiplayer_flyouts[player].label_visible;
	state->segment = Multiplayer_flyouts[player].ship.segnum;
	state->position = Multiplayer_flyouts[player].ship.pos;
}
