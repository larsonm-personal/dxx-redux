#ifndef COOP_RESTORE_HUD_H
#define COOP_RESTORE_HUD_H

#include "../multi_save_transfer_progress.h"

/* Called with GAME_FONT and the HUD canvas, like the adjacent status text */
static void coop_restore_draw_download_bar(const char *message, int y, int percent)
{
	int w, h;
	if (percent < 0) return;
	gr_get_string_drawn_size(message, &w, &h);
	const int x = (grd_curcanv->cv_bitmap.bm_w - w) / 2;
	const int top = y + h + FSPACY(1);
	const int bottom = top + FSPACY(2);
	gr_setcolor(BM_XRGB(8, 8, 8));
	gr_rect(x, top, x + w - 1, bottom);
	const int filled = w * percent / 100;
	if (filled > 0) {
		gr_setcolor(BM_XRGB(31, 24, 0));
		gr_rect(x, top, x + filled - 1, bottom);
	}
}

#endif
