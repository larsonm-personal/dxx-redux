#include <stdio.h>

#include "game.h"
#include "gamefont.h"
#include "gr.h"
#include "hud_counts_shared.h"
#ifdef NETWORK
#include "multi.h"
#endif
#include "object.h"
#include "player.h"
#include "secretarea.h"

extern int Color_0_31_0;

static void hud_counts_draw_right_text(int y, const char *text,
	hud_counts_right_inset_fn right_inset)
{
	int w, h, aw;

	gr_get_string_size(text, &w, &h, &aw);
	if (Color_0_31_0 == -1)
		Color_0_31_0 = BM_XRGB(0,31,0);
	gr_set_fontcolor(Color_0_31_0, -1);
	gr_string(grd_curcanv->cv_bitmap.bm_w - right_inset(y, h) - w - FSPACX(1),
		y, text);
}

static int hud_counts_robot_kills(int pnum)
{
#ifdef NETWORK
	int i, kills = 0;

	if (Game_mode & GM_MULTI_COOP) {
		for (i = 0; i < MAX_PLAYERS; i++)
			kills += Coop_kill_stats[i].robots_killed;
		return kills;
	}
#endif

	return Players[pnum].num_kills_level;
}

static int hud_counts_remaining_hostages(void)
{
	int i, count = 0;

	for (i = 0; i <= Highest_object_index; i++)
		if (Objects[i].type == OBJ_HOSTAGE)
			count++;

	return count;
}

static void hud_counts_draw_hostages(int y, int lost, int onboard, int total,
	hud_counts_right_inset_fn right_inset)
{
	char label[20] = "hostages: ";
	char lost_str[12], suffix[24], full[48];
	int label_w, label_h, label_aw;
	int lost_w, lost_h, lost_aw;
	int full_w, full_h, full_aw;
	int x;

	if (lost <= 0) {
		snprintf(full, sizeof(full), "%s%d/%d", label, onboard, total);
		hud_counts_draw_right_text(y, full, right_inset);
		return;
	}

	snprintf(lost_str, sizeof(lost_str), "%d", lost);
	snprintf(suffix, sizeof(suffix), "/%d/%d", onboard, total);
	snprintf(full, sizeof(full), "%s%s%s", label, lost_str, suffix);
	gr_get_string_size(full, &full_w, &full_h, &full_aw);
	gr_get_string_size(label, &label_w, &label_h, &label_aw);
	gr_get_string_size(lost_str, &lost_w, &lost_h, &lost_aw);

	x = grd_curcanv->cv_bitmap.bm_w - right_inset(y, full_h) - full_w - FSPACX(1);
	if (Color_0_31_0 == -1)
		Color_0_31_0 = BM_XRGB(0,31,0);
	gr_set_fontcolor(Color_0_31_0, -1);
	gr_string(x, y, label);
	gr_set_fontcolor(BM_XRGB(31,0,0), -1);
	gr_string(x + label_w, y, lost_str);
	gr_set_fontcolor(Color_0_31_0, -1);
	gr_string(x + label_w + lost_w, y, suffix);
}

static void hud_counts_draw_secrets(int y,
	hud_counts_right_inset_fn right_inset)
{
	char secret_str[32];
	const secret_area_state *state = secret_area_get_state();
	int total = secret_area_total(state);

	if (total <= 0)
		return;
	snprintf(secret_str, sizeof(secret_str), "secrets: %d/%d",
		secret_area_found_count(state), total);
	hud_counts_draw_right_text(y, secret_str, right_inset);
}

void hud_counts_draw(int pnum, int score_added_active, int timer_active,
	int secret_only, hud_counts_right_inset_fn right_inset)
{
	char robot_str[32];
	int yline = 1;
	int remaining_hostages, lost_hostages;

	gr_set_curfont(GAME_FONT);

	if (score_added_active)
		yline++;
	if (timer_active)
		yline++;

	if (secret_only) {
		hud_counts_draw_secrets(FSPACY(1) + LINE_SPACING * yline, right_inset);
		return;
	}

	snprintf(robot_str, sizeof(robot_str), "robots: %d/%d",
		hud_counts_robot_kills(pnum), Players[pnum].num_robots_level);
	hud_counts_draw_right_text(FSPACY(1) + LINE_SPACING * yline, robot_str,
		right_inset);

	remaining_hostages = hud_counts_remaining_hostages();
	lost_hostages = Players[pnum].hostages_level -
		Players[pnum].hostages_on_board - remaining_hostages;
	if (lost_hostages < 0)
		lost_hostages = 0;
	hud_counts_draw_hostages(FSPACY(1) + LINE_SPACING * (yline + 1),
		lost_hostages, Players[pnum].hostages_on_board,
		Players[pnum].hostages_level, right_inset);
	hud_counts_draw_secrets(FSPACY(1) + LINE_SPACING * (yline + 2), right_inset);
}
