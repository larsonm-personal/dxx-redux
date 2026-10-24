#include "preview.h"
#include "gr.h"
#include "gamefont.h"

static uint64_t indicator_draws;

uint64_t xl_menu_indicator_draws(void)
{
	return indicator_draws;
}

void xl_draw_menu_indicator(void)
{
	grs_canvas *saved_canvas;
	grs_font *saved_font;
	int saved_fg, saved_bg, width, height, average;
	if (!xl_runtime_active()) return;
	saved_canvas = grd_curcanv;
	gr_set_current_canvas(NULL);
	saved_font = grd_curcanv->cv_font;
	saved_fg = grd_curcanv->cv_font_fg_color;
	saved_bg = grd_curcanv->cv_font_bg_color;
	gr_set_curfont(GAME_FONT);
	gr_set_fontcolor(BM_XRGB(31, 31, 31), -1);
	gr_get_string_size("XL", &width, &height, &average);
	gr_string(grd_curcanv->cv_bitmap.bm_w - width - (int) FSPACX(8), (int) FSPACY(8), "XL");
	++indicator_draws;
	gr_set_curfont(saved_font);
	gr_set_fontcolor(saved_fg, saved_bg);
	gr_set_current_canvas(saved_canvas);
}
