#include "android_font_scale.h"

#include "gamefont.h"
#include "gr.h"

#define FONTSCALE_X(x) ((float)(x) * FNTScaleX)
#define FONTSCALE_Y(x) ((float)(x) * FNTScaleY)
#define BITS_TO_BYTES(x) (((x) + 7) >> 3)
#define INFONT(c) ((c) >= 0 && (c) <= grd_curcanv->cv_font->ft_maxchar - grd_curcanv->cv_font->ft_minchar)

void get_char_width(ubyte c, ubyte c2, int *width, int *spacing);
int get_centered_x(const char *s);

int android_font_scale_active(void)
{
	return FNTScaleX < 0.99f || FNTScaleX > 1.01f ||
	       FNTScaleY < 0.99f || FNTScaleY > 1.01f;
}

static void android_render_scaled_font_char(int x, int y, unsigned char *fp,
	int raw_width, int draw_width, int draw_height, int underline, int masked)
{
	grs_bitmap raw_bm, scaled_bm, *draw_bm;
	int r, i, bits_per_row;
	unsigned char fg = (unsigned char) grd_curcanv->cv_font_fg_color;
	unsigned char bg = masked ? TRANSPARENCY_COLOR :
	                   (unsigned char) grd_curcanv->cv_font_bg_color;

	if (raw_width < 1 || grd_curcanv->cv_font->ft_h < 1 ||
	    draw_width < 1 || draw_height < 1)
		return;

	bits_per_row = BITS_TO_BYTES(raw_width);
	gr_init_bitmap_alloc(&raw_bm, BM_LINEAR, 0, 0, raw_width,
	                     grd_curcanv->cv_font->ft_h, raw_width);
	for (r = 0; r < grd_curcanv->cv_font->ft_h; r++) {
		int underline_row = underline &&
			(r == grd_curcanv->cv_font->ft_baseline + 2 ||
			 r == grd_curcanv->cv_font->ft_baseline + 3);
		for (i = 0; i < raw_width; i++) {
			int bit = fp[r * bits_per_row + (i >> 3)] &
			          (0x80 >> (i & 7));
			raw_bm.bm_data[r * raw_width + i] =
				(bit || underline_row) ? fg : bg;
		}
	}

	draw_bm = &raw_bm;
	if (draw_width != raw_width ||
	    draw_height != grd_curcanv->cv_font->ft_h) {
		gr_init_bitmap_alloc(&scaled_bm, BM_LINEAR, 0, 0, draw_width,
		                     draw_height, draw_width);
		gr_bitmap_scale_to(&raw_bm, &scaled_bm);
		draw_bm = &scaled_bm;
	}

	if (masked)
		gr_bitmapm(x, y, draw_bm);
	else
		gr_bitmap(x, y, draw_bm);

	if (draw_bm == &scaled_bm)
		gr_free_bitmap_data(&scaled_bm);
	gr_free_bitmap_data(&raw_bm);
}

int android_internal_string_scaled_linear(int x, int y, const char *s, int masked)
{
	unsigned char *fp;
	const char *text_ptr, *next_row, *text_ptr1;
	int scaled_width, spacing, letter, raw_width, draw_width, draw_height;
	int underline;
	int skip_lines = 0;
	int xx, yy = y;

	next_row = s;
	while (next_row != NULL) {
		text_ptr1 = next_row;
		next_row = NULL;
		text_ptr = text_ptr1;
		xx = x;
		if (xx == 0x8000)
			xx = get_centered_x(text_ptr1);

		while (*text_ptr) {
			if (*text_ptr == '\n') {
				next_row = &text_ptr[1];
				yy += FONTSCALE_Y(grd_curcanv->cv_font->ft_h) + FSPACY(1);
				break;
			}

			if (*text_ptr == CC_COLOR) {
				grd_curcanv->cv_font_fg_color = (unsigned char) *(text_ptr + 1);
				text_ptr += 2;
				continue;
			}

			if (*text_ptr == CC_LSPACING) {
				skip_lines = *(text_ptr + 1) - '0';
				text_ptr += 2;
				continue;
			}

			underline = 0;
			if (*text_ptr == CC_UNDERLINE) {
				underline = 1;
				text_ptr++;
				if (!*text_ptr)
					break;
			}

			get_char_width(text_ptr[0], text_ptr[1], &scaled_width, &spacing);
			letter = (unsigned char) *text_ptr -
			         grd_curcanv->cv_font->ft_minchar;
			if (!INFONT(letter)) {
				if (!(grd_curcanv->cv_font->ft_flags & FT_PROPORTIONAL))
					spacing = FONTSCALE_X(grd_curcanv->cv_font->ft_w);
				xx += spacing;
				text_ptr++;
				continue;
			}

			raw_width = (grd_curcanv->cv_font->ft_flags & FT_PROPORTIONAL) ?
				grd_curcanv->cv_font->ft_widths[letter] :
				grd_curcanv->cv_font->ft_w;
			draw_width = (grd_curcanv->cv_font->ft_flags & FT_PROPORTIONAL) ?
				scaled_width : FONTSCALE_X(raw_width);
			draw_height = FONTSCALE_Y(grd_curcanv->cv_font->ft_h);
			if (draw_width < 1)
				draw_width = 1;
			if (draw_height < 1)
				draw_height = 1;
			if (!(grd_curcanv->cv_font->ft_flags & FT_PROPORTIONAL))
				spacing = draw_width;

			if (grd_curcanv->cv_font->ft_flags & FT_PROPORTIONAL)
				fp = grd_curcanv->cv_font->ft_chars[letter];
			else
				fp = grd_curcanv->cv_font->ft_data +
					letter * BITS_TO_BYTES(raw_width) *
					grd_curcanv->cv_font->ft_h;

			android_render_scaled_font_char(xx, yy, fp, raw_width, draw_width,
			                                draw_height, underline, masked);
			xx += spacing;
			text_ptr++;
		}
		yy += skip_lines;
		skip_lines = 0;
	}
	return 0;
}
