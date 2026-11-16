#include "texture_pixels.h"
#include "pog_resources.h"
#include "../xl_rgba_pixels.h"
extern "C" {
#include "inferno.h"
#include "gr.h"
#include "piggy.h"
#include "textures.h"
#include "rle.h"
#include "segment.h"
#include "wall.h"
}

extern "C" int xl_texture_pixel_transparency(int texture, int orientation, int32_t u, int32_t v)
{
	if (texture < 0 || texture >= NumTextures || Textures[texture].index >= Num_bitmap_files) return 0;
	PIGGY_PAGE_IN(Textures[texture]);
	auto *bitmap = &GameBitmaps[Textures[texture].index];
	if (!bitmap->bm_data || bitmap->bm_w <= 0 || bitmap->bm_h <= 0) return 0;
	if (bitmap->bm_flags & BM_FLAG_RLE) bitmap = rle_expand_texture(bitmap);
	int32_t x = u, y = v;
	// Unsigned subtraction preserves the source's 32-bit fixed-point wrap
	switch (orientation & 3) {
		case 1:
			x = int32_t(65536u - uint32_t(v));
			y = u;
			break;
		case 2:
			x = int32_t(65536u - uint32_t(u));
			y = int32_t(65536u - uint32_t(v));
			break;
		case 3:
			x = v;
			y = int32_t(65536u - uint32_t(u));
			break;
	}
	if (bitmap->bm_type == BM_RGBA) {
		xl_pog_pixel_view view{ bitmap->bm_data, unsigned(bitmap->bm_w), unsigned(bitmap->bm_h), unsigned(bitmap->bm_rowsize), unsigned(bitmap->bm_rgb_opaque != 0) };
		xl_pog_physics_view(Textures[texture].index, &view);
		const auto column = dxx_xl_texture_coordinate(x, view.width);
		const auto row = dxx_xl_texture_coordinate(y, view.height);
		return view.rgb_opaque ? 0 : dxx_xl_rgba_transparency(view.pixels + row * view.row_size + column * 4);
	}
	const auto column = dxx_xl_texture_coordinate(x, unsigned(bitmap->bm_w));
	const auto row = dxx_xl_texture_coordinate(y, unsigned(bitmap->bm_h));
	const auto color = bitmap->bm_data[row * bitmap->bm_rowsize + column];
	return color == 254 ? -1 : color == TRANSPARENCY_COLOR ? 1
	                                                       : 0;
}

extern "C" int xl_side_pixel_transparency(int base, int overlay, int32_t u, int32_t v)
{
	if (overlay & 0x3fff) {
		const auto transparency = xl_texture_pixel_transparency(overlay & 0x3fff, (overlay >> 14) & 3, u, v);
		if (transparency < 0) return 1;
		if (!transparency) return 0;
	}
	return xl_texture_pixel_transparency(base, 0, u, v) != 0;
}

extern "C" int xl_side_texture_transparency(int segment, int side)
{
	if (segment < 0 || segment >= Num_segments || side < 0 || side >= 6) return 0;
	const auto &surface = Segments[segment].sides[side];
	if (surface.tmap_num2 & 0x3fff) {
		const auto flags = piggy_bitmap_get_flags(&GameBitmaps[Textures[surface.tmap_num2 & 0x3fff].index]);
		if (flags & BM_FLAG_SUPER_TRANSPARENT) return 1;
		if (!(flags & BM_FLAG_TRANSPARENT)) return 0;
	}
	const auto flags = piggy_bitmap_get_flags(&GameBitmaps[Textures[surface.tmap_num].index]);
	if (flags & (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT)) return 1;
	if (surface.wall_num >= 0 && surface.wall_num < Num_walls) {
		const auto cloak = Walls[surface.wall_num].cloak_value;
		if (cloak && cloak < 31) return 1;
	}
	return 0;
}
