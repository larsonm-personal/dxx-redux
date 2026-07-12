#include "android_menu_scale.h"

#include <string.h>

#include "console.h"
#include "gamefont.h"

#ifdef OGL
#include "ogl_init.h"
#endif

/* Single tuning point for Android menu fill */
static const float k_target_fill = 0.85f;
static const float k_min_scale = 1.05f;
static const float k_kconfig_max_scale = 3.5f;
static const int k_crop_pad = 15;
static const int k_blit_tile_size = 1024;

extern int g_menu_scale_active;
extern int g_menu_scale_src_x, g_menu_scale_src_y;
extern int g_menu_scale_src_w, g_menu_scale_src_h;
extern int g_menu_scale_dst_x, g_menu_scale_dst_y;
extern int g_menu_scale_dst_w, g_menu_scale_dst_h;

static android_menu_scale_result g_last_result;

static void clear_result(android_menu_scale_result *result)
{
	if (result)
		memset(result, 0, sizeof(*result));
}

static int clamp_source_box(android_menu_scale_rect *source, int screen_w, int screen_h)
{
	if (source->x < 0) {
		source->w += source->x;
		source->x = 0;
	}
	if (source->y < 0) {
		source->h += source->y;
		source->y = 0;
	}
	if (source->x + source->w > screen_w)
		source->w = screen_w - source->x;
	if (source->y + source->h > screen_h)
		source->h = screen_h - source->y;
	return source->w > 0 && source->h > 0;
}

static int compute_destination(android_menu_scale_result *result, int screen_w,
                               int screen_h, float scale)
{
	result->dst.w = (int) (result->src.w * scale);
	result->dst.h = (int) (result->src.h * scale);
	if (result->dst.w <= 0 || result->dst.h <= 0)
		return 0;
	result->dst.x = (screen_w - result->dst.w) / 2;
	result->dst.y = (screen_h - result->dst.h) / 2;
	result->scale = scale;
	result->render_w = result->src.w;
	result->render_h = result->src.h;
	result->render_scale = 1.0f;
	result->active = 1;
	return 1;
}

int android_menu_scale_round_coord(int value, float scale)
{
	return (int) (value * scale + 0.5f);
}

void android_menu_scale_items(newmenu_item *dst, const newmenu_item *src,
                              int count, float scale)
{
	int i;

	for (i = 0; i < count; i++) {
		dst[i] = src[i];
		dst[i].x = android_menu_scale_round_coord(src[i].x, scale);
		dst[i].y = android_menu_scale_round_coord(src[i].y, scale);
		dst[i].w = android_menu_scale_round_coord(src[i].w, scale);
		dst[i].h = android_menu_scale_round_coord(src[i].h, scale);
		dst[i].right_offset = android_menu_scale_round_coord(src[i].right_offset, scale);
	}
}

float android_menu_scale_get_target_fill(void)
{
	return k_target_fill;
}

int android_menu_scale_compute_cropped(int source_x, int source_y, int source_w,
                                       int source_h, int screen_w, int screen_h, int border_x, int border_y,
                                       android_menu_scale_result *result)
{
	int crop_left, crop_top, crop_right, crop_bottom;
	float scale_x, scale_y, scale;

	clear_result(result);
	if (!result || source_w <= 0 || source_h <= 0 || screen_w <= 0 || screen_h <= 0)
		return 0;

	result->box.x = source_x;
	result->box.y = source_y;
	result->box.w = source_w;
	result->box.h = source_h;
	if (!clamp_source_box(&result->box, screen_w, screen_h))
		return 0;

	crop_left = border_x - k_crop_pad;
	crop_top = border_y - k_crop_pad;
	if (crop_left < 0)
		crop_left = 0;
	if (crop_top < 0)
		crop_top = 0;
	crop_right = crop_left;
	crop_bottom = crop_top;

	if (crop_left + crop_right >= result->box.w) {
		crop_left = 0;
		crop_right = 0;
	}
	if (crop_top + crop_bottom >= result->box.h) {
		crop_top = 0;
		crop_bottom = 0;
	}

	result->crop_left = crop_left;
	result->crop_top = crop_top;
	result->src = result->box;

	scale_x = k_target_fill * screen_w / result->box.w;
	scale_y = k_target_fill * screen_h / result->box.h;
	scale = scale_x < scale_y ? scale_x : scale_y;
	if (scale <= k_min_scale)
		return 0;

	return compute_destination(result, screen_w, screen_h, scale);
}

int android_menu_scale_compute_kconfig(int source_x, int source_y, int source_w,
                                       int source_h, int screen_w, int screen_h, int *scroll_y,
                                       android_menu_scale_result *result)
{
	float scale;
	int full_dst_w;
	int full_dst_h;
	int viewport_h;

	clear_result(result);
	if (!result || source_w <= 0 || source_h <= 0 || screen_w <= 0 || screen_h <= 0)
		return 0;

	scale = k_target_fill * screen_w / source_w;
	if (scale > k_kconfig_max_scale)
		scale = k_kconfig_max_scale;
	if (scale <= k_min_scale)
		return 0;

	result->box.x = source_x;
	result->box.y = source_y;
	result->box.w = source_w;
	result->box.h = source_h;
	if (!clamp_source_box(&result->box, screen_w, screen_h))
		return 0;

	result->src = result->box;
	full_dst_w = (int) (result->box.w * scale);
	full_dst_h = (int) (result->box.h * scale);
	if (full_dst_w <= 0 || full_dst_h <= 0)
		return 0;

	viewport_h = (int) (k_target_fill * screen_h);
	if (viewport_h <= 0 || viewport_h > screen_h)
		viewport_h = screen_h;

	if (full_dst_h > viewport_h) {
		int max_scroll;
		int current_scroll = scroll_y ? *scroll_y : 0;

		result->src.h = (int) (viewport_h / scale);
		if (result->src.h <= 0)
			return 0;
		if (result->src.h > result->box.h)
			result->src.h = result->box.h;
		max_scroll = result->box.h - result->src.h;
		if (current_scroll < 0)
			current_scroll = 0;
		if (current_scroll > max_scroll)
			current_scroll = max_scroll;
		if (scroll_y)
			*scroll_y = current_scroll;

		result->src.y = result->box.y + current_scroll;
		result->dst.x = (screen_w - full_dst_w) / 2;
		result->dst.y = (screen_h - viewport_h) / 2;
		result->dst.w = full_dst_w;
		result->dst.h = viewport_h;
		result->scale = scale;
		result->render_w = full_dst_w;
		result->render_h = full_dst_h;
		result->render_scale = scale;
		result->active = 1;
		return 1;
	}

	if (scroll_y)
		*scroll_y = 0;
	return compute_destination(result, screen_w, screen_h, scale);
}

void android_menu_scale_publish(const android_menu_scale_result *result)
{
	if (!result || !result->active) {
		android_menu_scale_clear();
		return;
	}
	g_menu_scale_src_x = result->src.x;
	g_menu_scale_src_y = result->src.y;
	g_menu_scale_src_w = result->src.w;
	g_menu_scale_src_h = result->src.h;
	g_menu_scale_dst_x = result->dst.x;
	g_menu_scale_dst_y = result->dst.y;
	g_menu_scale_dst_w = result->dst.w;
	g_menu_scale_dst_h = result->dst.h;
	g_menu_scale_active = 1;
	g_last_result = *result;
}

void android_menu_scale_clear(void)
{
	g_menu_scale_active = 0;
	g_menu_scale_src_x = g_menu_scale_src_y = 0;
	g_menu_scale_src_w = g_menu_scale_src_h = 0;
	g_menu_scale_dst_x = g_menu_scale_dst_y = 0;
	g_menu_scale_dst_w = g_menu_scale_dst_h = 0;
	clear_result(&g_last_result);
}

int android_menu_scale_get_state(android_menu_scale_result *result)
{
	if (!result)
		return 0;
	*result = g_last_result;
	result->active = g_menu_scale_active;
	result->src.x = g_menu_scale_src_x;
	result->src.y = g_menu_scale_src_y;
	result->src.w = g_menu_scale_src_w;
	result->src.h = g_menu_scale_src_h;
	result->dst.x = g_menu_scale_dst_x;
	result->dst.y = g_menu_scale_dst_y;
	result->dst.w = g_menu_scale_dst_w;
	result->dst.h = g_menu_scale_dst_h;
	return 1;
}

int android_menu_scale_begin_scaled_draw(float scale, android_menu_scale_draw_state *state)
{
	if (!state || !grd_curscreen || scale <= 1.0f)
		return 0;

	state->screen_w = grd_curscreen->sc_w;
	state->screen_h = grd_curscreen->sc_h;
	state->fnt_scale_x = FNTScaleX;
	state->fnt_scale_y = FNTScaleY;

	grd_curscreen->sc_w = (unsigned int) (state->screen_w * scale + 0.5f);
	grd_curscreen->sc_h = (unsigned int) (state->screen_h * scale + 0.5f);
	FNTScaleX = state->fnt_scale_x * scale;
	FNTScaleY = state->fnt_scale_y * scale;
	return 1;
}

void android_menu_scale_end_scaled_draw(const android_menu_scale_draw_state *state)
{
	if (!state || !grd_curscreen)
		return;

	grd_curscreen->sc_w = state->screen_w;
	grd_curscreen->sc_h = state->screen_h;
	FNTScaleX = state->fnt_scale_x;
	FNTScaleY = state->fnt_scale_y;
}

static void scale_line_masked(unsigned char *in, unsigned char *out, int ilen, int olen)
{
	int a = olen / ilen, b = olen % ilen;
	int c = 0, i;
	unsigned char *end = out + olen;

	while (out < end) {
		i = a;
		c += b;
		if (c >= ilen) {
			c -= ilen;
			goto inside_m;
		}
		while (--i >= 0) {
inside_m:
			if (*in != 255)
				*out = *in;
			out++;
		}
		in++;
	}
}

static void bitmap_scale_to_masked(grs_bitmap *src, grs_bitmap *dst)
{
	unsigned char *s = src->bm_data;
	unsigned char *d = dst->bm_data;
	int h = src->bm_h;
	int a = dst->bm_h / h, b = dst->bm_h % h;
	int c = 0, i, y;

	for (y = 0; y < h; y++) {
		i = a;
		c += b;
		if (c >= h) {
			c -= h;
			goto inside2;
		}
		while (--i >= 0) {
inside2:
			scale_line_masked(s, d, src->bm_w, dst->bm_w);
			d += dst->bm_rowsize;
		}
		s += src->bm_rowsize;
	}
}

void android_menu_scale_blit_bitmap(grs_bitmap *bitmap,
                                    const android_menu_scale_result *result, int masked)
{
	if (!bitmap || !result || !result->active)
		return;

#ifdef OGL
	{
		grs_bitmap scaled;
		grs_canvas *save_canvas = grd_curcanv;
		grs_bitmap *target_bitmap = &grd_curscreen->sc_canvas.cv_bitmap;
		int old_flags;
		gr_init_bitmap_alloc(&scaled, BM_LINEAR, 0, 0, result->dst.w,
		                     result->dst.h, result->dst.w);
		if (masked)
			memset(scaled.bm_data, TRANSPARENCY_COLOR, result->dst.w * result->dst.h);
		if (masked)
			bitmap_scale_to_masked(bitmap, &scaled);
		else
			gr_bitmap_scale_to(bitmap, &scaled);
		old_flags = scaled.bm_flags;
		if (masked)
			scaled.bm_flags |= BM_FLAG_TRANSPARENT;
		if (target_bitmap && target_bitmap->bm_type == BM_OGL) {
#ifdef ANDROID
			ogl_android_prepare_overlay_blit();
#endif
			ogl_ubitblt_i(result->dst.w, result->dst.h, result->dst.x,
			              result->dst.y, result->dst.w, result->dst.h,
			              0, 0, &scaled, target_bitmap, 1);
		} else {
			gr_set_current_canvas(NULL);
			gr_bitmap(result->dst.x, result->dst.y, &scaled);
			gr_set_current_canvas(save_canvas);
		}
		scaled.bm_flags = old_flags;
		gr_free_bitmap_data(&scaled);
	}
#else
	{
		grs_canvas *sub = gr_create_sub_canvas(&grd_curscreen->sc_canvas,
		                                       result->dst.x, result->dst.y,
		                                       result->dst.w, result->dst.h);
		if (masked)
			bitmap_scale_to_masked(bitmap, &sub->cv_bitmap);
		else
			gr_bitmap_scale_to(bitmap, &sub->cv_bitmap);
		gr_free_sub_canvas(sub);
	}
#endif
}

void android_menu_scale_blit_bitmap_region(grs_bitmap *bitmap,
                                           const android_menu_scale_result *result,
                                           int source_y)
{
	int copy_h;

	if (!bitmap || !result || !result->active)
		return;
	if (result->dst.w <= 0 || result->dst.h <= 0 || source_y < 0)
		return;
	if (source_y >= bitmap->bm_h)
		return;

	copy_h = result->dst.h;
	if (source_y + copy_h > bitmap->bm_h)
		copy_h = bitmap->bm_h - source_y;
	if (copy_h <= 0)
		return;

#ifdef OGL
	{
		grs_bitmap *target_bitmap = &grd_curscreen->sc_canvas.cv_bitmap;
		if (target_bitmap && target_bitmap->bm_type == BM_OGL) {
#ifdef ANDROID
			ogl_android_prepare_overlay_blit();
#endif
			int tile_y;

			for (tile_y = 0; tile_y < copy_h; tile_y += k_blit_tile_size) {
				int tile_h = copy_h - tile_y;
				int tile_x;

				if (tile_h > k_blit_tile_size)
					tile_h = k_blit_tile_size;
				for (tile_x = 0; tile_x < result->dst.w; tile_x += k_blit_tile_size) {
					int tile_w = result->dst.w - tile_x;

					if (tile_w > k_blit_tile_size)
						tile_w = k_blit_tile_size;
					ogl_ubitblt_i(tile_w, tile_h, result->dst.x + tile_x,
					              result->dst.y + tile_y, tile_w, tile_h,
					              tile_x, source_y + tile_y, bitmap,
					              target_bitmap, 0);
				}
			}
		} else {
			grs_canvas *save_canvas = grd_curcanv;
			gr_set_current_canvas(NULL);
			gr_bm_ubitblt(result->dst.w, copy_h, result->dst.x,
			              result->dst.y, 0, source_y, bitmap,
			              &grd_curscreen->sc_canvas.cv_bitmap);
			gr_set_current_canvas(save_canvas);
		}
	}
#else
	gr_bm_ubitblt(result->dst.w, copy_h, result->dst.x,
	              result->dst.y, 0, source_y, bitmap,
	              &grd_curscreen->sc_canvas.cv_bitmap);
#endif
}

void android_menu_scale_blit_source_region(grs_bitmap *bitmap,
                                           const android_menu_scale_result *result, int masked)
{
	int row;
	grs_bitmap cropped;

	if (!bitmap || !result || !result->active)
		return;

	gr_init_bitmap_alloc(&cropped, BM_LINEAR, 0, 0, result->src.w,
	                     result->src.h, result->src.w);
	for (row = 0; row < result->src.h; row++)
		memcpy(cropped.bm_data + row * result->src.w,
		       bitmap->bm_data + (result->src.y + row) * bitmap->bm_rowsize +
		           result->src.x,
		       result->src.w);
	android_menu_scale_blit_bitmap(&cropped, result, masked);
	gr_free_bitmap_data(&cropped);
}
