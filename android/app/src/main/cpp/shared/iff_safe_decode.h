#ifndef DXX_IFF_SAFE_DECODE_H
#define DXX_IFF_SAFE_DECODE_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define IFF_SAFE_MAX_DIMENSION   8192u
#define IFF_SAFE_MAX_PIXELS      (64u * 1024u * 1024u)
#define IFF_SAFE_MAX_ANIM_PIXELS (256u * 1024u * 1024u)
#define IFF_SAFE_MAX_FILE_BYTES  (128u * 1024u * 1024u)

typedef enum iff_safe_result {
	IFF_SAFE_OK = 0,
	IFF_SAFE_CORRUPT,
	IFF_SAFE_NO_MEMORY,
	IFF_SAFE_NOT_IFF,
	IFF_SAFE_WRONG_FORM,
	IFF_SAFE_FORM_ANIM,
	IFF_SAFE_FORM_BITMAP,
	IFF_SAFE_TOO_MANY_FRAMES
} iff_safe_result;

typedef struct iff_safe_image {
	uint8_t *pixels;
	uint8_t palette[768];
	size_t width;
	size_t height;
	uint8_t transparent_color;
	uint8_t has_transparency;
} iff_safe_image;

typedef struct iff_safe_cursor {
	const uint8_t *data;
	size_t size;
	size_t position;
} iff_safe_cursor;

static uint16_t iff_safe_be16(const uint8_t *data)
{
	return (uint16_t) (((uint16_t) data[0] << 8) | data[1]);
}

static uint32_t iff_safe_be32(const uint8_t *data)
{
	return ((uint32_t) data[0] << 24) | ((uint32_t) data[1] << 16) | ((uint32_t) data[2] << 8) | data[3];
}

static int iff_safe_tag(const uint8_t *data, const char *tag)
{
	return memcmp(data, tag, 4) == 0;
}

static int iff_safe_take(iff_safe_cursor *cursor, size_t count, const uint8_t **data)
{
	if (cursor->position > cursor->size || count > cursor->size - cursor->position)
		return 0;
	*data = cursor->data + cursor->position;
	cursor->position += count;
	return 1;
}

static void iff_safe_free_image(iff_safe_image *image)
{
	free(image->pixels);
	memset(image, 0, sizeof(*image));
}

static iff_safe_result iff_safe_allocate_image(iff_safe_image *image, size_t width, size_t height)
{
	if (!width || !height || width > IFF_SAFE_MAX_DIMENSION || height > IFF_SAFE_MAX_DIMENSION ||
	    width > IFF_SAFE_MAX_PIXELS / height)
		return IFF_SAFE_CORRUPT;
	image->pixels = (uint8_t *) malloc(width * height);
	if (!image->pixels)
		return IFF_SAFE_NO_MEMORY;
	image->width = width;
	image->height = height;
	return IFF_SAFE_OK;
}

static iff_safe_result iff_safe_copy_previous(iff_safe_image *image, const iff_safe_image *previous)
{
	iff_safe_result result;
	if (!previous || !previous->pixels)
		return IFF_SAFE_CORRUPT;
	result = iff_safe_allocate_image(image, previous->width, previous->height);
	if (result != IFF_SAFE_OK)
		return result;
	memcpy(image->pixels, previous->pixels, previous->width * previous->height);
	memcpy(image->palette, previous->palette, sizeof(image->palette));
	image->transparent_color = previous->transparent_color;
	image->has_transparency = previous->has_transparency;
	return IFF_SAFE_OK;
}

static iff_safe_result iff_safe_decode_row(iff_safe_cursor *cursor, uint8_t compression, uint8_t *row, size_t row_size)
{
	if (!compression) {
		const uint8_t *source;
		if (!iff_safe_take(cursor, row_size, &source))
			return IFF_SAFE_CORRUPT;
		memcpy(row, source, row_size);
		return IFF_SAFE_OK;
	}
	if (compression != 1)
		return IFF_SAFE_CORRUPT;
	for (size_t offset = 0; offset < row_size;) {
		const uint8_t *source;
		int8_t control;
		size_t count;
		if (!iff_safe_take(cursor, 1, &source))
			return IFF_SAFE_CORRUPT;
		control = (int8_t) *source;
		if (control == -128)
			continue;
		if (control >= 0) {
			count = (size_t) control + 1;
			if (count > row_size - offset || !iff_safe_take(cursor, count, &source))
				return IFF_SAFE_CORRUPT;
			memcpy(row + offset, source, count);
		} else {
			count = (size_t) (1 - control);
			if (count > row_size - offset || !iff_safe_take(cursor, 1, &source))
				return IFF_SAFE_CORRUPT;
			memset(row + offset, *source, count);
		}
		offset += count;
	}
	return IFF_SAFE_OK;
}

static iff_safe_result iff_safe_decode_body(const uint8_t *data, size_t size, uint32_t form_type,
                                            uint8_t planes, uint8_t masking, uint8_t compression,
                                            iff_safe_image *image)
{
	iff_safe_cursor cursor = { data, size, 0 };
	const size_t source_width = form_type == 0x50424d20u ? image->width : (image->width + 7) / 8;
	const size_t row_size = (source_width + 1) & ~(size_t) 1;
	const size_t stored_planes = form_type == 0x50424d20u ? 1 : planes;
	uint8_t *row;
	uint8_t *planar = NULL;
	iff_safe_result result = IFF_SAFE_OK;
	if (!row_size || !stored_planes || stored_planes > 8 || row_size > IFF_SAFE_MAX_DIMENSION ||
	    row_size > IFF_SAFE_MAX_PIXELS / stored_planes ||
	    row_size * stored_planes > IFF_SAFE_MAX_PIXELS / image->height)
		return IFF_SAFE_CORRUPT;
	row = (uint8_t *) malloc(row_size);
	if (!row)
		return IFF_SAFE_NO_MEMORY;
	if (form_type != 0x50424d20u) {
		planar = (uint8_t *) malloc(row_size * stored_planes * image->height);
		if (!planar) {
			free(row);
			return IFF_SAFE_NO_MEMORY;
		}
	}
	for (size_t y = 0; y < image->height && result == IFF_SAFE_OK; ++y) {
		for (size_t plane = 0; plane < stored_planes; ++plane) {
			result = iff_safe_decode_row(&cursor, compression, row, row_size);
			if (result != IFF_SAFE_OK)
				break;
			if (form_type == 0x50424d20u)
				memcpy(image->pixels + y * image->width, row, image->width);
			else
				memcpy(planar + (y * stored_planes + plane) * row_size, row, row_size);
		}
		if (result == IFF_SAFE_OK && masking == 1)
			result = iff_safe_decode_row(&cursor, compression, row, row_size);
	}
	if (result == IFF_SAFE_OK && cursor.position != cursor.size)
		result = IFF_SAFE_CORRUPT;
	if (result == IFF_SAFE_OK && planar) {
		for (size_t y = 0; y < image->height; ++y)
			for (size_t x = 0; x < image->width; ++x) {
				uint8_t pixel = 0;
				for (size_t plane = 0; plane < stored_planes; ++plane)
					if (planar[(y * stored_planes + plane) * row_size + x / 8] & (0x80u >> (x & 7)))
						pixel |= (uint8_t) (1u << plane);
				image->pixels[y * image->width + x] = pixel;
			}
	}
	free(planar);
	free(row);
	return result;
}

static iff_safe_result iff_safe_decode_delta(const uint8_t *data, size_t size, iff_safe_image *image)
{
	iff_safe_cursor cursor = { data, size, 0 };
	const uint8_t *source;
	const size_t encoded_width = (image->width + 1) & ~(size_t) 1;
	if (!image->pixels || !iff_safe_take(&cursor, 4, &source))
		return IFF_SAFE_CORRUPT;
	for (size_t y = 0; y < image->height; ++y) {
		size_t x = 0;
		uint8_t items;
		if (!iff_safe_take(&cursor, 1, &source))
			return IFF_SAFE_CORRUPT;
		items = *source;
		while (items--) {
			uint8_t code;
			size_t count;
			if (!iff_safe_take(&cursor, 1, &source))
				return IFF_SAFE_CORRUPT;
			code = *source;
			if (!code) {
				uint8_t value;
				if (!iff_safe_take(&cursor, 2, &source))
					return IFF_SAFE_CORRUPT;
				count = source[0];
				value = source[1];
				if (!count || count > encoded_width - x)
					return IFF_SAFE_CORRUPT;
				if (x < image->width) {
					const size_t visible = count < image->width - x ? count : image->width - x;
					memset(image->pixels + y * image->width + x, value, visible);
				}
			} else if (code > 0x80) {
				count = code - 0x80;
				if (count > encoded_width - x)
					return IFF_SAFE_CORRUPT;
			} else {
				count = code;
				if (count > encoded_width - x || !iff_safe_take(&cursor, count, &source))
					return IFF_SAFE_CORRUPT;
				if (x < image->width) {
					const size_t visible = count < image->width - x ? count : image->width - x;
					memcpy(image->pixels + y * image->width + x, source, visible);
				}
			}
			x += count;
		}
		if (x != encoded_width)
			return IFF_SAFE_CORRUPT;
	}
	return cursor.position == cursor.size ? IFF_SAFE_OK : IFF_SAFE_CORRUPT;
}

static iff_safe_result iff_safe_parse_image_form(const uint8_t *data, size_t size,
                                                 const iff_safe_image *previous, iff_safe_image *image)
{
	size_t form_end;
	size_t position = 12;
	uint32_t form_type;
	uint8_t planes = 0, masking = 0, compression = 0;
	int have_header = 0, have_pixels = 0, have_palette = 0, have_anhd = 0;
	iff_safe_result result = IFF_SAFE_OK;
	memset(image, 0, sizeof(*image));
	if (size < 12 || !iff_safe_tag(data, "FORM"))
		return IFF_SAFE_NOT_IFF;
	form_end = (size_t) iff_safe_be32(data + 4) + 8;
	if (form_end < 12 || form_end > size)
		return IFF_SAFE_CORRUPT;
	form_type = iff_safe_be32(data + 8);
	if (form_type == 0x414e494du)
		return IFF_SAFE_FORM_ANIM;
	if (form_type != 0x50424d20u && form_type != 0x494c424du)
		return IFF_SAFE_WRONG_FORM;
	while (position < form_end) {
		const uint8_t *chunk;
		size_t chunk_size, padded_size;
		uint32_t chunk_type;
		if (form_end - position < 8)
			goto corrupt;
		chunk_type = iff_safe_be32(data + position);
		chunk_size = iff_safe_be32(data + position + 4);
		position += 8;
		padded_size = chunk_size + (chunk_size & 1u);
		if (padded_size < chunk_size || padded_size > form_end - position)
			goto corrupt;
		chunk = data + position;
		if (chunk_type == 0x424d4844u) {
			size_t width, height;
			if (have_header || chunk_size < 20 || have_pixels || have_anhd)
				goto corrupt;
			width = iff_safe_be16(chunk);
			height = iff_safe_be16(chunk + 2);
			planes = chunk[8];
			masking = chunk[9];
			compression = chunk[10];
			if ((form_type == 0x50424d20u && planes != 8) ||
			    (form_type == 0x494c424du && (!planes || planes > 8)) || masking > 2 || compression > 1)
				goto corrupt;
			result = iff_safe_allocate_image(image, width, height);
			if (result != IFF_SAFE_OK)
				goto done;
			image->transparent_color = (uint8_t) iff_safe_be16(chunk + 12);
			image->has_transparency = masking == 2;
			have_header = 1;
		} else if (chunk_type == 0x414e4844u) {
			if (have_anhd || have_header || have_pixels)
				goto corrupt;
			result = iff_safe_copy_previous(image, previous);
			if (result != IFF_SAFE_OK)
				goto done;
			have_anhd = 1;
		} else if (chunk_type == 0x434d4150u) {
			if (have_palette || chunk_size > sizeof(image->palette) || chunk_size % 3)
				goto corrupt;
			for (size_t i = 0; i < chunk_size; ++i)
				image->palette[i] = chunk[i] >> 2;
			have_palette = 1;
		} else if (chunk_type == 0x424f4459u) {
			if (!have_header || have_pixels)
				goto corrupt;
			result = iff_safe_decode_body(chunk, chunk_size, form_type, planes, masking, compression, image);
			if (result != IFF_SAFE_OK)
				goto done;
			have_pixels = 1;
		} else if (chunk_type == 0x444c5441u) {
			if (!have_anhd || have_pixels)
				goto corrupt;
			result = iff_safe_decode_delta(chunk, chunk_size, image);
			if (result != IFF_SAFE_OK)
				goto done;
			have_pixels = 1;
		}
		position += padded_size;
	}
	if (position != form_end || !have_pixels)
		goto corrupt;
	return IFF_SAFE_OK;

corrupt:
	result = IFF_SAFE_CORRUPT;
done:
	iff_safe_free_image(image);
	return result;
}

static iff_safe_result iff_safe_decode_bitmap(const uint8_t *data, size_t size, iff_safe_image *image)
{
	if (size > IFF_SAFE_MAX_FILE_BYTES)
		return IFF_SAFE_CORRUPT;
	return iff_safe_parse_image_form(data, size, NULL, image);
}

static void iff_safe_free_frames(iff_safe_image *frames, size_t count)
{
	for (size_t i = 0; i < count; ++i)
		iff_safe_free_image(frames + i);
}

static iff_safe_result iff_safe_decode_anim(const uint8_t *data, size_t size, iff_safe_image *frames,
                                            size_t capacity, size_t *frame_count)
{
	size_t form_end, position = 12, count = 0, total_pixels = 0;
	iff_safe_result result = IFF_SAFE_OK;
	*frame_count = 0;
	if (size < 12 || size > IFF_SAFE_MAX_FILE_BYTES || !iff_safe_tag(data, "FORM"))
		return IFF_SAFE_NOT_IFF;
	form_end = (size_t) iff_safe_be32(data + 4) + 8;
	if (form_end < 12 || form_end > size)
		return IFF_SAFE_CORRUPT;
	if (iff_safe_tag(data + 8, "PBM ") || iff_safe_tag(data + 8, "ILBM"))
		return IFF_SAFE_FORM_BITMAP;
	if (!iff_safe_tag(data + 8, "ANIM"))
		return IFF_SAFE_WRONG_FORM;
	while (position < form_end) {
		size_t nested_size, padded_size;
		if (form_end - position < 8 || !iff_safe_tag(data + position, "FORM")) {
			result = IFF_SAFE_CORRUPT;
			break;
		}
		nested_size = (size_t) iff_safe_be32(data + position + 4) + 8;
		padded_size = nested_size + (nested_size & 1u);
		if (nested_size < 12 || padded_size < nested_size || padded_size > form_end - position) {
			result = IFF_SAFE_CORRUPT;
			break;
		}
		if (count == capacity) {
			result = IFF_SAFE_TOO_MANY_FRAMES;
			break;
		}
		result = iff_safe_parse_image_form(data + position, nested_size, count ? frames + count - 1 : NULL,
		                                   frames + count);
		if (result != IFF_SAFE_OK)
			break;
		if (frames[count].width > (IFF_SAFE_MAX_ANIM_PIXELS - total_pixels) / frames[count].height) {
			iff_safe_free_image(frames + count);
			result = IFF_SAFE_CORRUPT;
			break;
		}
		total_pixels += frames[count].width * frames[count].height;
		++count;
		position += padded_size;
	}
	if (result == IFF_SAFE_OK && (!count || position != form_end))
		result = IFF_SAFE_CORRUPT;
	if (result != IFF_SAFE_OK) {
		iff_safe_free_frames(frames, count);
		return result;
	}
	*frame_count = count;
	return IFF_SAFE_OK;
}

#endif
