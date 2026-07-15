#ifndef HUD_COUNTS_SHARED_H
#define HUD_COUNTS_SHARED_H

typedef int (*hud_counts_right_inset_fn)(int y, int h);

typedef struct hud_counts_debug_row {
	int present;
	int drawn;
	int x;
	int y;
	int w;
	int h;
} hud_counts_debug_row;

typedef struct hud_counts_debug_state {
	hud_counts_debug_row robots;
	hud_counts_debug_row hostages;
	hud_counts_debug_row secrets;
} hud_counts_debug_state;

void hud_counts_debug_reset(void);
const hud_counts_debug_state *hud_counts_get_debug_state(void);
void hud_counts_draw(int pnum, int score_added_active, int timer_active,
                     int secret_only, hud_counts_right_inset_fn right_inset);

#endif
