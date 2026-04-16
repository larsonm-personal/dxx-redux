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
	int sx, sy;   /* screen position (center of polygon) */
	int is_hires; /* 1 = hires PNG, 0 = base game */
	int seg;
	int side;
	int face;
	char name[24]; /* truncated texture name */
};

extern struct debug_tex_label g_debug_tex_labels[DEBUG_TEX_MAX_LABELS];
extern int g_debug_tex_label_count;
extern volatile int g_debug_tex_overlay_active;

#define METL154_DEBUG_NONE          0
#define METL154_DEBUG_OVERLAY_ALPHA 1
#define METL154_DEBUG_OVERLAY_RGB   2

#define METL154_EXPERIMENT_DEFAULT      0
#define METL154_EXPERIMENT_KTX2_NOMIP   1
#define METL154_EXPERIMENT_RGBA         2
#define METL154_EXPERIMENT_RGBA_NOMIP   3
#define METL154_EXPERIMENT_STOCK        4
#define METL154_EXPERIMENT_ALPHA_RAW    5
#define METL154_EXPERIMENT_COVER_SKIP   6
#define METL154_EXPERIMENT_COVER_SKIP2  7
#define METL154_EXPERIMENT_OVERLAY_ONLY 8

struct android_draw_face_context {
	int valid;
	int seg;
	int side;
	int face;
	int child;
	int side_type;
	int nv;
	int wid_flags;
	int tmap1;
	int tmap2;
};

#define DEBUG_TEX_LABEL_SET_FACE(lbl, ctxptr)                              \
	do {                                                                   \
		(lbl)->seg = ((ctxptr) && (ctxptr)->valid) ? (ctxptr)->seg : -1;   \
		(lbl)->side = ((ctxptr) && (ctxptr)->valid) ? (ctxptr)->side : -1; \
		(lbl)->face = ((ctxptr) && (ctxptr)->valid) ? (ctxptr)->face : -1; \
	} while (0)

extern volatile int g_metl154_debug_mode;
extern volatile int g_metl154_experiment_mode;
extern volatile int g_metl154_experiment_pending_apply;
extern volatile int g_metl154_snapshot_pending;
extern volatile int g_metl154_snapshot_request_frame;
extern volatile int g_metl154_render_pass;
extern volatile int g_metl154_frame_id;
extern volatile int g_metl154_draw_seq;
extern struct android_draw_face_context g_android_draw_face_ctx;

#endif /* ANDROID */
#endif /* DEBUG_TEX_OVERLAY_H */
