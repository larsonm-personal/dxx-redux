#ifdef ANDROID

#include <android/log.h>
#include <string.h>

#include "3d.h"
#include "debug_tex_overlay.h"
#include "gr.h"
#include "ogl_init.h"
#include "piggy.h"
#include "strutil.h"

#include "android_texture_debug.h"

struct debug_tex_label g_debug_tex_labels[DEBUG_TEX_MAX_LABELS];
int g_debug_tex_label_count = 0;
volatile int g_debug_tex_overlay_active = 0;
float g_font_rgb_override[3] = { -1.f, -1.f, -1.f };
int g_ogl_render_context = 0;
static char g_android_texture_debug_target_name[ANDROID_TEXTURE_DEBUG_TARGET_NAME_MAX] = { 0 };
volatile int g_android_texture_debug_target_mode = ANDROID_TEXTURE_DEBUG_TARGET_CROSSHAIR;

static void android_texture_debug_copy_string(char *dst, int dst_size,
                                              const char *src)
{
	int i;

	if (!dst || dst_size <= 0)
		return;
	if (!src)
		src = "";
	for (i = 0; i + 1 < dst_size && src[i]; i++)
		dst[i] = src[i];
	dst[i] = '\0';
}

void android_texture_debug_set_target(const char *value)
{
	int mode = ANDROID_TEXTURE_DEBUG_TARGET_CROSSHAIR;
	char next_name[ANDROID_TEXTURE_DEBUG_TARGET_NAME_MAX] = { 0 };

	if (!value || !value[0] || !d_stricmp(value, "crosshair") || !d_stricmp(value, "auto")) {
		mode = ANDROID_TEXTURE_DEBUG_TARGET_CROSSHAIR;
	} else if (!d_stricmp(value, "none") || !d_stricmp(value, "off") || !d_stricmp(value, "disabled")) {
		mode = ANDROID_TEXTURE_DEBUG_TARGET_NONE;
	} else {
		mode = ANDROID_TEXTURE_DEBUG_TARGET_NAME;
		android_texture_debug_copy_string(next_name, sizeof(next_name), value);
	}

	android_texture_debug_copy_string(g_android_texture_debug_target_name,
	                                  sizeof(g_android_texture_debug_target_name), next_name);
	__sync_synchronize();
	g_android_texture_debug_target_mode = mode;
}

const char *android_texture_debug_get_target_display(void)
{
	switch ((int) g_android_texture_debug_target_mode) {
		case ANDROID_TEXTURE_DEBUG_TARGET_NAME:
			return g_android_texture_debug_target_name;
		case ANDROID_TEXTURE_DEBUG_TARGET_CROSSHAIR:
			return "crosshair";
		default:
			return "";
	}
}

int android_texture_debug_target_is_crosshair(void)
{
	return (int) g_android_texture_debug_target_mode == ANDROID_TEXTURE_DEBUG_TARGET_CROSSHAIR;
}

int android_texture_debug_matches_target_name(const char *bitmapname)
{
	if ((int) g_android_texture_debug_target_mode != ANDROID_TEXTURE_DEBUG_TARGET_NAME)
		return 0;
	if (!bitmapname || !g_android_texture_debug_target_name[0])
		return 0;
	return !d_stricmp(bitmapname, g_android_texture_debug_target_name);
}

int android_texture_debug_get_label_anchor(const g3s_point *const *pointlist,
                                           int nv, int *sx, int *sy)
{
	fix cx = 0, cy = 0, cz = 0;
	int i;
	int sw = grd_curcanv->cv_bitmap.bm_w;
	int sh = grd_curcanv->cv_bitmap.bm_h;
	int checkmuldiv(fix * r, fix a, fix b, fix c);
	extern fix Canv_w2, Canv_h2;
	if (!pointlist || nv < 3 || !sx || !sy)
		return 0;
	for (i = 0; i < nv; i++) {
		cx += pointlist[i]->p3_vec.x / nv;
		cy += pointlist[i]->p3_vec.y / nv;
		cz += pointlist[i]->p3_vec.z / nv;
	}
	if (cz <= F1_0 / 4)
		return 0;
	{
		fix sx_fix, sy_fix;

		if (!checkmuldiv(&sx_fix, cx, Canv_w2, cz) || !checkmuldiv(&sy_fix, cy, Canv_h2, cz))
			return 0;
		*sx = f2i(Canv_w2 + sx_fix);
		*sy = f2i(Canv_h2 - sy_fix);
	}

	if (*sx < 0 || *sx >= sw || *sy < 0 || *sy >= sh)
		return 0;

	return 1;
}

static void android_texture_debug_append_label(int sx, int sy,
                                               grs_bitmap *bm,
                                               const char *bitmapname)
{
	struct debug_tex_label *label;

	if (!bitmapname || g_debug_tex_label_count >= DEBUG_TEX_MAX_LABELS)
		return;

	label = &g_debug_tex_labels[g_debug_tex_label_count];
	label->sx = sx;
	label->sy = sy;
	label->is_hires = (bm && bm->gltexture && bm->gltexture->is_png) ? 1 : 0;
	label->anchor_group = 0;
	label->anchor_samples = 1;
	DEBUG_TEX_LABEL_SET_FACE(label, &g_android_draw_face_ctx);
	strncpy(label->name, bitmapname, sizeof(label->name) - 1);
	label->name[sizeof(label->name) - 1] = '\0';
	g_debug_tex_label_count++;
}

void android_texture_debug_add_overlay_label(const g3s_point *const *pointlist,
                                             int nv, grs_bitmap *bm,
                                             int y_offset)
{
	const char *bitmapname;
	int sx, sy;

	if (!g_debug_tex_overlay_active || g_debug_tex_label_count >= DEBUG_TEX_MAX_LABELS || !bm)
		return;

	bitmapname = piggy_game_bitmap_name(bm);
	if (!bitmapname)
		return;
	if (!android_texture_debug_get_label_anchor(pointlist, nv, &sx, &sy))
		return;

	android_texture_debug_append_label(sx, sy + y_offset, bm, bitmapname);
}

void android_texture_debug_add_joined_labels(const g3s_point *const *pointlist,
                                             int nv, grs_bitmap *bmbot,
                                             grs_bitmap *bmovl)
{
	const char *botname = piggy_game_bitmap_name(bmbot);
	const char *ovlname = piggy_game_bitmap_name(bmovl);
	int sx, sy;

	if (!g_debug_tex_overlay_active || nv < 3 || (!botname && !ovlname))
		return;
	if (!android_texture_debug_get_label_anchor(pointlist, nv, &sx, &sy))
		return;

	if (botname)
		android_texture_debug_append_label(sx, sy, bmbot, botname);
	if (ovlname)
		android_texture_debug_append_label(sx, sy + 10, bmovl, ovlname);
}

void android_texture_debug_log_render_bind(int *render_log_count,
                                           grs_bitmap *bm)
{
	const char *bitmapname;
	GLint min_filter = 0;

	if (!render_log_count || *render_log_count >= 5 || !bm || !bm->gltexture)
		return;

	bitmapname = piggy_game_bitmap_name(bm);
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &min_filter);
	__android_log_print(ANDROID_LOG_INFO, "DXX-TEX",
	                    "3D render bind #%d: %s handle=%u is_png=%d w=%d h=%d u=%.3f v=%.3f min_filter=0x%x",
	                    *render_log_count, bitmapname ? bitmapname : "?",
	                    bm->gltexture->handle, bm->gltexture->is_png,
	                    bm->gltexture->w, bm->gltexture->h,
	                    bm->gltexture->u, bm->gltexture->v,
	                    min_filter);
	(*render_log_count)++;
}

#endif /* ANDROID */
