#ifndef DXX_IFF_SAFE_ENGINE_ADAPTER_H
#define DXX_IFF_SAFE_ENGINE_ADAPTER_H

#include "iff_safe_decode.h"

extern ubyte iff_transparent_color;
extern ubyte iff_has_transparency;

static int iff_safe_engine_error(iff_safe_result result)
{
	if (result == IFF_SAFE_NO_MEMORY)
		return IFF_NO_MEM;
	if (result == IFF_SAFE_NOT_IFF)
		return IFF_NOT_IFF;
	if (result == IFF_SAFE_WRONG_FORM)
		return IFF_UNKNOWN_FORM;
	if (result == IFF_SAFE_FORM_ANIM)
		return IFF_FORM_ANIM;
	if (result == IFF_SAFE_FORM_BITMAP)
		return IFF_FORM_BITMAP;
	if (result == IFF_SAFE_TOO_MANY_FRAMES)
		return IFF_TOO_MANY_BMS;
	return result == IFF_SAFE_OK ? IFF_NO_ERROR : IFF_CORRUPT;
}

static int iff_safe_engine_read_file(char *filename, ubyte **data, size_t *size)
{
	PHYSFS_file *file = PHYSFSX_openReadBuffered(filename);
	PHYSFS_sint64 length;
	if (!file)
		return IFF_NO_FILE;
	length = PHYSFS_fileLength(file);
	if (length < 0 || length > IFF_SAFE_MAX_FILE_BYTES) {
		PHYSFS_close(file);
		return IFF_CORRUPT;
	}
	*data = d_malloc((size_t) length);
	if (!*data) {
		PHYSFS_close(file);
		return IFF_NO_MEM;
	}
	if (PHYSFS_read(file, *data, 1, (PHYSFS_uint32) length) != length) {
		d_free(*data);
		*data = NULL;
		PHYSFS_close(file);
		return IFF_READ_ERROR;
	}
	PHYSFS_close(file);
	*size = (size_t) length;
	return IFF_NO_ERROR;
}

static int iff_safe_engine_publish(const iff_safe_image *image, grs_bitmap *bm, int bitmap_type,
                                   ubyte *palette)
{
	ubyte *pixels;
	if (bitmap_type != BM_LINEAR && bitmap_type != BM_RGB15)
		return IFF_BAD_BM_TYPE;
	if (bitmap_type == BM_RGB15) {
		unsigned short *rgb15 = d_malloc(image->width * image->height * sizeof(*rgb15));
		if (!rgb15)
			return IFF_NO_MEM;
		for (size_t i = 0; i < image->width * image->height; ++i) {
			const size_t color = (size_t) image->pixels[i] * 3;
			rgb15[i] = (unsigned short) ((((image->palette[color] / 2) & 31) << 10) |
			                             (((image->palette[color + 1] / 2) & 31) << 5) |
			                             ((image->palette[color + 2] / 2) & 31));
		}
		pixels = (ubyte *) rgb15;
		gr_init_bitmap(bm, BM_RGB15, 0, 0, (int) image->width, (int) image->height,
		               (int) (image->width * sizeof(*rgb15)), pixels);
	} else {
		pixels = d_malloc(image->width * image->height);
		if (!pixels)
			return IFF_NO_MEM;
		memcpy(pixels, image->pixels, image->width * image->height);
		gr_init_bitmap(bm, BM_LINEAR, 0, 0, (int) image->width, (int) image->height, (int) image->width, pixels);
	}
	if (palette)
		memcpy(palette, image->palette, sizeof(image->palette));
	iff_transparent_color = image->transparent_color;
	iff_has_transparency = image->has_transparency;
	return IFF_NO_ERROR;
}

static int iff_safe_engine_read_bitmap(char *filename, grs_bitmap *bitmap, int bitmap_type, ubyte *palette)
{
	ubyte *data = NULL;
	size_t size = 0;
	iff_safe_image image;
	int result = iff_safe_engine_read_file(filename, &data, &size);
	if (result != IFF_NO_ERROR)
		return result;
	result = iff_safe_engine_error(iff_safe_decode_bitmap(data, size, &image));
	d_free(data);
	if (result != IFF_NO_ERROR)
		return result;
	result = iff_safe_engine_publish(&image, bitmap, bitmap_type, palette);
	iff_safe_free_image(&image);
	return result;
}

static int iff_safe_engine_read_into_bitmap(char *filename, grs_bitmap *bitmap, sbyte *palette)
{
	ubyte *data = NULL;
	size_t size = 0;
	iff_safe_image image;
	int result = iff_safe_engine_read_file(filename, &data, &size);
	if (result != IFF_NO_ERROR)
		return result;
	result = iff_safe_engine_error(iff_safe_decode_bitmap(data, size, &image));
	d_free(data);
	if (result != IFF_NO_ERROR)
		return result;
	if (!bitmap->bm_data || bitmap->bm_w != image.width || bitmap->bm_h != image.height ||
	    (bitmap->bm_type == BM_LINEAR && bitmap->bm_rowsize < image.width)) {
		iff_safe_free_image(&image);
		return IFF_BM_MISMATCH;
	}
	for (size_t y = 0; y < image.height; ++y) {
		if (bitmap->bm_type == BM_LINEAR)
			memcpy(bitmap->bm_data + y * bitmap->bm_rowsize, image.pixels + y * image.width, image.width);
		else
			for (size_t x = 0; x < image.width; ++x)
				gr_bm_pixel(bitmap, (int) x, (int) y, image.pixels[y * image.width + x]);
	}
	if (palette)
		memcpy(palette, image.palette, sizeof(image.palette));
	iff_transparent_color = image.transparent_color;
	iff_has_transparency = image.has_transparency;
	iff_safe_free_image(&image);
	return IFF_NO_ERROR;
}

static int iff_safe_engine_read_anim(char *filename, grs_bitmap **bitmap_list, int max_bitmaps,
                                     int *bitmap_count, ubyte *palette)
{
	ubyte *data = NULL;
	size_t size = 0, count = 0, published = 0;
	iff_safe_image *frames;
	int result;
	*bitmap_count = 0;
	if (max_bitmaps <= 0 || max_bitmaps > 1024)
		return IFF_TOO_MANY_BMS;
	result = iff_safe_engine_read_file(filename, &data, &size);
	if (result != IFF_NO_ERROR)
		return result;
	frames = (iff_safe_image *) calloc((size_t) max_bitmaps, sizeof(*frames));
	if (!frames) {
		d_free(data);
		return IFF_NO_MEM;
	}
	result = iff_safe_engine_error(iff_safe_decode_anim(data, size, frames, (size_t) max_bitmaps, &count));
	d_free(data);
	if (result == IFF_NO_ERROR)
		for (; published < count; ++published) {
			bitmap_list[published] = d_malloc(sizeof(*bitmap_list[published]));
			if (!bitmap_list[published]) {
				result = IFF_NO_MEM;
				break;
			}
			memset(bitmap_list[published], 0, sizeof(*bitmap_list[published]));
			result = iff_safe_engine_publish(frames + published, bitmap_list[published], BM_LINEAR,
			                                 published ? NULL : palette);
			if (result != IFF_NO_ERROR) {
				d_free(bitmap_list[published]);
				bitmap_list[published] = NULL;
				break;
			}
		}
	iff_safe_free_frames(frames, count);
	free(frames);
	if (result != IFF_NO_ERROR) {
		while (published--) {
			gr_free_bitmap_data(bitmap_list[published]);
			d_free(bitmap_list[published]);
			bitmap_list[published] = NULL;
		}
		return result;
	}
	*bitmap_count = (int) count;
	return IFF_NO_ERROR;
}

#endif
