/* debug_tex_overlay.h -- per-texture debug label overlay
 *
 * During 3D rendering, g3_draw_tmap accumulates screen positions and
 * texture names into g_debug_tex_labels[].  After 3D rendering (in 2D
 * mode), the labels are drawn as colored text centered on each polygon.
 * Green = hires PNG replacement, yellow = base game texture.
 *
 * Toggle via automation "set_debug" or adb broadcast GAME_COMMAND. */

#ifndef DEBUG_TEX_OVERLAY_H
#define DEBUG_TEX_OVERLAY_H

#ifdef ANDROID

#define DEBUG_TEX_MAX_LABELS 256

struct debug_tex_label {
	int sx, sy;    /* screen position (center of polygon) */
	int is_hires;  /* 1 = hires PNG, 0 = base game */
	char name[24]; /* truncated texture name */
};

extern struct debug_tex_label g_debug_tex_labels[DEBUG_TEX_MAX_LABELS];
extern int g_debug_tex_label_count;
extern volatile int g_debug_tex_overlay_active;

#endif /* ANDROID */
#endif /* DEBUG_TEX_OVERLAY_H */
