/* Included by guidebot_route.c: read engine state without polling it from JNI */
#include "guidebot_info_overlay.h"

static int Guidebot_info_visible;
static struct {
	char text[80];
	fix64 time;
	int error;
	unsigned repeats;
} Guidebot_info_history[3];
static int Guidebot_info_count;
static int Guidebot_info_mode = -1;
static int Guidebot_info_stalled;
static int Guidebot_info_readiness = -1;
static char Guidebot_info_goal[80];

void guidebot_info_set_visible(int visible)
{
	__atomic_store_n(&Guidebot_info_visible, !!visible, __ATOMIC_RELAXED);
}

int guidebot_info_visible(void)
{
	return __atomic_load_n(&Guidebot_info_visible, __ATOMIC_RELAXED);
}

void guidebot_info_reset(void)
{
	Guidebot_info_count = 0;
	Guidebot_info_mode = -1;
	Guidebot_info_stalled = 0;
	Guidebot_info_readiness = -1;
	Guidebot_info_goal[0] = 0;
}

void guidebot_info_event(const char *text, int error)
{
	if (!text || !text[0])
		return;
	if (Guidebot_info_count && !strncmp(Guidebot_info_history[0].text, text,
	                                    sizeof(Guidebot_info_history[0].text) - 1)) {
		++Guidebot_info_history[0].repeats;
		Guidebot_info_history[0].time = GameTime64;
		return;
	}
	memmove(&Guidebot_info_history[1], &Guidebot_info_history[0],
	        sizeof(Guidebot_info_history[0]) * 2);
	snprintf(Guidebot_info_history[0].text, sizeof(Guidebot_info_history[0].text), "%s", text);
	Guidebot_info_history[0].time = GameTime64;
	Guidebot_info_history[0].error = error;
	Guidebot_info_history[0].repeats = 1;
	if (Guidebot_info_count < 3)
		++Guidebot_info_count;
}

int guidebot_info_history_count(void)
{
	return Guidebot_info_count;
}

const char *guidebot_info_status(void)
{
	return Guidebot_info_mode == 0 ? "inactive" : Guidebot_info_mode == 1 ? "remote owner"
	                                          : Guidebot_info_mode == 2   ? "HIGH-LEVEL"
	                                          : Guidebot_info_mode == 3   ? "BASE routing"
	                                                                      : "follow / idle";
}

void guidebot_info_draw(void)
{
	const int live = escort_buddy_is_active() && Buddy_allowed_to_talk;
	const object *obj = live ? &Objects[Buddy_objnum] : NULL;
	const ai_static *aip = obj ? &obj->ctype.ai_info : NULL;
	const ai_local *ailp = obj ? &Ai_local_info[Buddy_objnum] : NULL;
	const int readiness = level_metadata_get_route_readiness();
	int remote = 0;
	int mode, stalled, i, x, y, line, width = 0;
	char rows[6][112];
	int colors[6];
	const int normal = BM_XRGB(24, 24, 24);
	const int red = BM_XRGB(31, 5, 5);
	const int green = BM_XRGB(5, 31, 8);
	grs_font *saved_font;
	float saved_scale_x, saved_scale_y;
	const char *status;
#ifdef NETWORK
	remote = (Game_mode & GM_MULTI_COOP) && Escort_owner_player != Player_num;
#endif
	mode = !live ? 0 : remote                      ? 1
	               : Escort_route_goal.active      ? 2
	               : ailp->mode == AIM_GOTO_OBJECT ? 3
	                                               : 4;
	status = mode == 0 ? "inactive" : mode == 1 ? "remote owner"
	                              : mode == 2   ? "HIGH-LEVEL"
	                              : mode == 3   ? "BASE routing"
	                                            : "follow / idle";
	if (readiness != Guidebot_info_readiness) {
		char event[80];
		snprintf(event, sizeof(event), "Metadata: %s", level_metadata_route_readiness_name(readiness));
		guidebot_info_event(event, readiness == LEVEL_METADATA_READINESS_FAILED);
		Guidebot_info_readiness = readiness;
	}
	if (live && !remote && Escort_route_goal.active &&
	    strncmp(Guidebot_info_goal, escort_route_goal_label(), sizeof(Guidebot_info_goal) - 1)) {
		snprintf(Guidebot_info_goal, sizeof(Guidebot_info_goal), "%s", escort_route_goal_label());
		guidebot_info_event(Guidebot_info_goal, 0);
	}
	if (mode != Guidebot_info_mode) {
		guidebot_info_event(status, mode == 3 ? 1 : mode == 2 ? -1
		                                                      : 0);
		Guidebot_info_mode = mode;
	}
	stalled = live && !remote &&
	          ailp->mode == AIM_GOTO_OBJECT &&
	          escort_get_navigation_liveness_age() >= 3 * F1_0;
	if (stalled != Guidebot_info_stalled) {
		guidebot_info_event(stalled ? "No movement for 3s (possible stall)" : "Movement resumed", stalled);
		Guidebot_info_stalled = stalled;
	}
	if (!guidebot_info_visible())
		return;
	snprintf(rows[0], sizeof(rows[0]), "GB %s | metadata %s", status,
	         level_metadata_route_readiness_name(readiness));
	colors[0] = mode == 3 || readiness == LEVEL_METADATA_READINESS_FAILED ? red : mode == 2 ? green
	                                                                                        : normal;
	snprintf(rows[1], sizeof(rows[1]), "Goal: %.64s", mode == 1 ? "diagnostics on owner" : Escort_route_goal.active ? escort_route_goal_label()
	                                                                                                                : "no high-level objective");
	colors[1] = normal;
	if (obj && !remote)
		snprintf(rows[2], sizeof(rows[2]), "S%d>%d P%d/%d still:%ds retry:%d",
		         obj->segnum, Escort_route_goal.active ? Escort_route_goal.target_seg : ailp->goal_segment,
		         aip->path_length ? aip->cur_path_index + 1 : 0, aip->path_length,
		         (int) (escort_get_navigation_liveness_age() / F1_0), ailp->consecutive_retries);
	else
		snprintf(rows[2], sizeof(rows[2]), "%s", live ? "Route state is local to the owner" : "Guide-Bot docked, absent or unreleased");
	colors[2] = stalled || (ailp && !remote && ailp->consecutive_retries) ? red : normal;
	for (i = 0; i < Guidebot_info_count; ++i) {
		const int age = (int) ((GameTime64 - Guidebot_info_history[i].time) / F1_0);
		snprintf(rows[3 + i], sizeof(rows[3 + i]), "%ds %.72s", age < 0 ? 0 : age, Guidebot_info_history[i].text);
		if (Guidebot_info_history[i].repeats > 1) {
			const size_t len = strlen(rows[3 + i]);
			snprintf(rows[3 + i] + len, sizeof(rows[3 + i]) - len, " x%u", Guidebot_info_history[i].repeats);
		}
		colors[3 + i] = Guidebot_info_history[i].error > 0 ? red : Guidebot_info_history[i].error < 0 ? green
		                                                                                              : normal;
	}
	saved_font = grd_curcanv->cv_font;
	gr_set_curfont(GAME_FONT);
	saved_scale_x = FNTScaleX;
	saved_scale_y = FNTScaleY;
	FNTScaleX *= 0.75f;
	FNTScaleY *= 0.75f;
	line = LINE_SPACING;
	x = FSPACX(3);
	y = line * 2 + FSPACY(3);
	for (i = 0; i < 3 + Guidebot_info_count; ++i) {
		int w, h;
		gr_get_string_drawn_size(rows[i], &w, &h);
		/* Keep the panel below half the viewport width, also in portrait */
		while (w > grd_curcanv->cv_bitmap.bm_w * 0.48f && strlen(rows[i]) > 4) {
			const size_t len = strlen(rows[i]);
			rows[i][len - 1] = 0;
			rows[i][len - 2] = '.';
			rows[i][len - 3] = '.';
			rows[i][len - 4] = '.';
			gr_get_string_drawn_size(rows[i], &w, &h);
		}
		if (w > width) width = w;
	}
	gr_setcolor(BM_XRGB(0, 0, 0));
	gr_settransblend(7, GR_BLEND_NORMAL);
	gr_rect(x - 2, y - 2, x + width + 2, y + line * (3 + Guidebot_info_count));
	gr_settransblend(GR_FADE_OFF, GR_BLEND_NORMAL);
	for (i = 0; i < 3 + Guidebot_info_count; ++i) {
		gr_set_fontcolor(colors[i], -1);
		gr_string(x, y + line * i, rows[i]);
	}
	FNTScaleX = saved_scale_x;
	FNTScaleY = saved_scale_y;
	gr_set_curfont(saved_font);
}
