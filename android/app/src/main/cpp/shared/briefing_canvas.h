#ifndef DXX_BRIEFING_CANVAS_H
#define DXX_BRIEFING_CANVAS_H

#include "gr.h"
#include "gamefont.h"
#include "android_log.h"
#ifdef ANDROID
#include "android_surface_lifecycle.h"
#endif

typedef struct briefing_font_scale {
	float x, y;
} briefing_font_scale;

static inline briefing_font_scale briefing_begin_text(const grs_canvas *canvas)
{
	const briefing_font_scale saved = { FNTScaleX, FNTScaleY };
#if defined(OGL) || defined(ANDROID)
	const int font_x = GAME_FONT->ft_w >= 7 ? GAME_FONT->ft_w / 7 : 1;
	const int font_y = GAME_FONT->ft_h >= 5 ? GAME_FONT->ft_h / 5 : 1;
	/* Preserve the 640x480 presentation: 2x low-res or 1x high-res fonts
	 * Scale each axis with the canvas to include its physical pixel correction
	 * Use the same font units as FSPACX/FSPACY for tabs and line spacing */
	FNTScaleX = (float) canvas->cv_bitmap.bm_w / (320 * font_x);
	FNTScaleY = (float) canvas->cv_bitmap.bm_h / (240 * font_y);
#else
	/* The desktop software font renderer does not support scaling */
	(void) canvas;
#endif
	return saved;
}

static inline void briefing_end_text(briefing_font_scale saved)
{
	FNTScaleX = saved.x;
	FNTScaleY = saved.y;
}

static inline fix briefing_pixel_aspect(void)
{
#ifdef ANDROID
	/* Android stretches the render buffer to the live SurfaceView
	 * Saved desktop AspectX/AspectY can describe a different display */
	const int display_width = android_surface_get_display_width();
	const int display_height = android_surface_get_display_height();
	if (display_width > 0 && display_height > 0 && SWIDTH > 0 && SHEIGHT > 0)
		return (fix) ((SWIDTH * (long long) display_height * F1_0) /
		              (SHEIGHT * (long long) display_width));
#endif
	return grd_curscreen->sc_aspect > 0 ? grd_curscreen->sc_aspect : F1_0;
}

/* Both 320x200 and 640x480 briefing art was authored for a 4:3 display
 * Use the same pixel correction for the backdrop and 3D robot subcanvases */
static inline void briefing_init_canvas(grs_canvas *canvas)
{
	const fix aspect = briefing_pixel_aspect();
	int width = (int) ((SHEIGHT * 4LL * aspect + 3 * F1_0 / 2) / (3 * F1_0));
	int height = SHEIGHT;
	if (width > SWIDTH) {
		width = SWIDTH;
		height = (int) ((width * 3LL * F1_0 + 2LL * aspect) / (4LL * aspect));
	}
	if (width < 1) width = 1;
	if (height < 1) height = 1;
	gr_init_sub_canvas(canvas, &grd_curscreen->sc_canvas,
	                   (SWIDTH - width) / 2, (SHEIGHT - height) / 2, width, height);
	debug_log_force(DLOG_GAME, "Android briefing canvas: screen=%dx%d pixel_aspect=%d configured_pixel_aspect=%d canvas=%d,%d %dx%d",
	                SWIDTH, SHEIGHT, aspect, grd_curscreen->sc_aspect, canvas->cv_bitmap.bm_x, canvas->cv_bitmap.bm_y, width, height);
}

#endif
