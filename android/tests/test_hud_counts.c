/* Exercise the shared HUD renderer with engine state and captured text/colors */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "gamefont.h"
#include "gr.h"
#include "hud_counts_shared.h"
#include "hudmsg.h"
#include "multi.h"
#include "object.h"
#include "player.h"
#include "secretarea.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

int Game_mode, N_players, Highest_object_index;
#ifdef DXX_BUILD_DESCENT_II
player Players[MAX_PLAYERS + 4];
#else
player Players[MAX_PLAYERS];
#endif
object Objects[MAX_OBJECTS];
coop_player_kill_stats Coop_kill_stats[MAX_PLAYERS];
int Color_0_31_0 = -1;
float FNTScaleX = 1, FNTScaleY = 1;
grs_font *Gamefonts[MAX_FONTS];
grs_canvas *grd_curcanv;

static char hostage_text[80], red_text[24], robot_text[32];
static int current_color, hostage_y;
static int secret_total;

void gr_set_curfont(grs_font *font) { grd_curcanv->cv_font = font; }
void gr_set_fontcolor(int fg, int bg) { (void)bg; current_color = fg; }
int gr_find_closest_color(int r, int g, int b) { (void)b; return r ? 1 : g ? 2 : 0; }
void gr_get_string_drawn_size(const char *text, int *w, int *h)
{
	*w = (int)strlen(text) * 7;
	*h = 5;
}
int gr_string(int x, int y, const char *text)
{
	(void)x;
	if (strncmp(text, "robots: ", 8) == 0)
		snprintf(robot_text, sizeof(robot_text), "%s", text);
	if (strncmp(text, "hostages: ", 10) == 0)
		hostage_y = y;
	if (y == hostage_y) {
		CHECK(strlen(hostage_text) + strlen(text) < sizeof(hostage_text));
		strcat(hostage_text, text);
		if (current_color == BM_XRGB(31, 0, 0)) {
			CHECK(strlen(red_text) + strlen(text) < sizeof(red_text));
			strcat(red_text, text);
		}
	}
	return 0;
}
int HUD_message_area_intersects(int x, int y, int w, int h)
{
	(void)x; (void)y; (void)w; (void)h;
	return 0;
}
const secret_area_state *secret_area_get_state(void) { return NULL; }
int secret_area_total(const secret_area_state *state) { (void)state; return secret_total; }
int secret_area_found_count(const secret_area_state *state) { (void)state; return 0; }
static int right_inset(int y, int h) { (void)y; (void)h; return 0; }

static void check_hostages(int pnum, const char *text, const char *red)
{
	hostage_text[0] = red_text[0] = 0;
	hostage_y = -1;
	hud_counts_draw(pnum, 0, 0, 0, right_inset);
	CHECK(strcmp(hostage_text, text) == 0);
	CHECK(strcmp(red_text, red) == 0);
	CHECK(hud_counts_get_debug_state()->hostages.drawn);
}

static void check_secret_robots(const char *text)
{
	const hud_counts_debug_state *counts;
	robot_text[0] = 0;
	hostage_y = -1;
	hud_counts_draw(0, 0, 0, 1, right_inset);
	counts = hud_counts_get_debug_state();
	CHECK(strcmp(robot_text, text) == 0);
	CHECK(counts->robots.drawn);
	CHECK(!counts->hostages.present);
	CHECK(counts->secrets.drawn == (secret_total > 0));
	if (secret_total > 0)
		CHECK(counts->secrets.y >= counts->robots.y + counts->robots.h);
}

int main(void)
{
	grs_font font = {0};
	grs_canvas canvas = {0};
	font.ft_w = 7;
	font.ft_h = 5;
	Gamefonts[GFONT_SMALL] = &font;
	canvas.cv_bitmap.bm_w = 640;
	grd_curcanv = &canvas;
	N_players = 2;
	Players[0].connected = Players[1].connected = CONNECT_PLAYING;
	Players[0].hostages_level = Players[1].hostages_level = 3;
	Highest_object_index = 1;
	Objects[0].type = Objects[1].type = OBJ_HOSTAGE;
	Players[0].hostages_on_board = 1;
	check_hostages(0, "hostages: 1/3", "");

	/* A teammate picks up a hostage: both players see the team count in green */
	Game_mode = GM_MULTI | GM_MULTI_COOP;
	Objects[1].type = OBJ_NONE;
	Players[1].hostages_on_board = 1;
	check_hostages(0, "hostages: 2/3", "");
	check_hostages(1, "hostages: 2/3", "");

	/* An escaped teammate still carries rescued hostages */
	Players[1].connected = CONNECT_END_MENU;
	check_hostages(0, "hostages: 2/3", "");
	Players[1].connected = CONNECT_PLAYING;

	/* Actual loss on a teammate's death is red, survivors remain green */
	Players[1].hostages_on_board = 0;
	check_hostages(0, "hostages: 1/1/3", "1");
	check_hostages(1, "hostages: 1/1/3", "1");

	/* Stale inventory in disconnected or unused slots must not hide losses */
	Players[1].hostages_on_board = 1;
	Players[1].connected = CONNECT_DISCONNECTED;
	Players[2].hostages_on_board = 99;
	Players[2].connected = CONNECT_PLAYING;
	check_hostages(0, "hostages: 1/1/3", "1");

	/* Single player continues to use only the local inventory */
	Game_mode = 0;
	Players[1].connected = CONNECT_PLAYING;
	check_hostages(0, "hostages: 1/1/3", "1");
	Players[0].hostages_on_board = 0;
	check_hostages(0, "hostages: 2/0/3", "2");
	/* Secret levels show robot progress even without any secret-area row */
	Players[0].num_robots_level = 10;
	Players[0].num_kills_level = 3;
	check_secret_robots("robots: 3/10");
	secret_total = 2;
	check_secret_robots("robots: 3/10");

	/* Co-op secret levels use team kills, just like ordinary mines */
	Game_mode = GM_MULTI | GM_MULTI_COOP;
	Coop_kill_stats[0].robots_killed = 2;
	Coop_kill_stats[1].robots_killed = 5;
	check_secret_robots("robots: 7/10");
	secret_total = 0;
	check_secret_robots("robots: 7/10");
	puts("Robot and hostage HUD counts passed");
	return 0;
}
