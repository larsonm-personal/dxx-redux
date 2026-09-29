#ifndef DXX_BRIEFING_CANVAS_H
#define DXX_BRIEFING_CANVAS_H

#include "gr.h"

/* Both 320x200 and 640x480 briefing art was authored for a 4:3 display
 * sc_aspect is the renderer's pixel aspect correction, not the display ratio
 * Keep it unchanged so 3D robot subcanvases retain the same projection */
static inline void briefing_init_canvas(grs_canvas *canvas)
{
	const fix aspect = grd_curscreen->sc_aspect > 0 ? grd_curscreen->sc_aspect : F1_0;
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
}

#endif
