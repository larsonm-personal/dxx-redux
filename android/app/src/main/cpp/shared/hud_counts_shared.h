#ifndef HUD_COUNTS_SHARED_H
#define HUD_COUNTS_SHARED_H

typedef int (*hud_counts_right_inset_fn)(int y, int h);

void hud_counts_draw(int pnum, int score_added_active, int timer_active,
	int secret_only, hud_counts_right_inset_fn right_inset);

#endif
