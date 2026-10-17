#include "d1_custom_validation.h"

#include "bounded_rle.h"

#include <limits.h>
#include <string.h>

#define D1_CUSTOM_MAX_FILE_BYTES  (64u * 1024u * 1024u)
#define D1_CUSTOM_MAX_IMAGE_BYTES (64u * 1024u * 1024u)
#define D1_CUSTOM_MAX_TOTAL_BYTES (128u * 1024u * 1024u)
#define D1_CUSTOM_PIG_HEADER      17u
#define D1_CUSTOM_POG_HEADER      18u
#define D1_CUSTOM_SOUND_HEADER    20u
#define D1_CUSTOM_ROBOT_RECORD    480u
#define D1_CUSTOM_MODEL_RECORD    734u
#define D1_CUSTOM_MODEL_DEPTH     256

typedef struct d1_custom_cursor {
	const uint8_t *data;
	size_t size;
	size_t position;
} d1_custom_cursor;

static int d1_custom_span(size_t size, size_t offset, size_t count)
{
	return offset <= size && count <= size - offset;
}

static int d1_custom_take(d1_custom_cursor *cursor, size_t count, const uint8_t **result)
{
	if (!cursor || !d1_custom_span(cursor->size, cursor->position, count)) return 0;
	if (result) *result = cursor->data + cursor->position;
	cursor->position += count;
	return 1;
}

static uint16_t d1_custom_u16(const uint8_t *data)
{
	return (uint16_t) (data[0] | ((uint16_t) data[1] << 8));
}

static int16_t d1_custom_i16(const uint8_t *data)
{
	return (int16_t) d1_custom_u16(data);
}

static uint32_t d1_custom_u32(const uint8_t *data)
{
	return (uint32_t) data[0] | ((uint32_t) data[1] << 8) |
	       ((uint32_t) data[2] << 16) | ((uint32_t) data[3] << 24);
}

static int32_t d1_custom_i32(const uint8_t *data)
{
	return (int32_t) d1_custom_u32(data);
}

static int d1_custom_read_i32(d1_custom_cursor *cursor, int *value)
{
	const uint8_t *data;
	if (!value || !d1_custom_take(cursor, 4, &data)) return 0;
	*value = d1_custom_i32(data);
	return 1;
}

static int d1_custom_checked_add(size_t left, size_t right, size_t *result)
{
	if (!result || right > SIZE_MAX - left) return 0;
	*result = left + right;
	return 1;
}

static int d1_custom_checked_mul(size_t left, size_t right, size_t *result)
{
	if (!result || (left && right > SIZE_MAX / left)) return 0;
	*result = left * right;
	return 1;
}

static int d1_custom_valid_lookup(int index, int maximum)
{
	return index == -1 || (index > 0 && index < maximum);
}

static void d1_custom_bitmap_name(char name[15], const uint8_t *header, unsigned int abm_flag)
{
	unsigned int frame;
	memcpy(name, header, 8);
	name[8] = 0;
	if (!(header[8] & abm_flag)) return;
	frame = header[8] & 63u;
	name[8] = '#';
	if (frame >= 10) {
		name[9] = (char) ('0' + frame / 10);
		name[10] = (char) ('0' + frame % 10);
		name[11] = 0;
	} else {
		name[9] = (char) ('0' + frame);
		name[10] = 0;
	}
}

static int d1_custom_validate_bitmap(const uint8_t *data, size_t size, size_t offset,
                                     int width, int height, unsigned int flags,
                                     const d1_custom_validation_limits *limits, size_t *total_bytes)
{
	size_t data_size;
	if (width <= 0 || height <= 0 || width > 4096 || height > 4096 ||
	    !d1_custom_checked_mul((size_t) width, (size_t) height, &data_size) ||
	    data_size > D1_CUSTOM_MAX_IMAGE_BYTES || offset >= size)
		return 0;
	if (flags & limits->rle_flag) {
		int row_size_bytes = flags & limits->rle_big_flag ? 2 : 1;
		if (!d1_custom_span(size, offset, 4)) return 0;
		data_size = d1_custom_u32(data + offset);
		if (data_size < 4 || data_size > D1_CUSTOM_MAX_IMAGE_BYTES ||
		    !d1_custom_span(size, offset, data_size) ||
		    !bounded_rle_validate_bitmap(data + offset, data_size, width, height, row_size_bytes))
			return 0;
	} else if (!d1_custom_span(size, offset, data_size))
		return 0;
	if (data_size > D1_CUSTOM_MAX_TOTAL_BYTES - *total_bytes) return 0;
	*total_bytes += data_size;
	return 1;
}

int d1_custom_validate_pigpog(const uint8_t *data, size_t size,
                              const d1_custom_validation_limits *limits,
                              d1_custom_name_lookup bitmap_lookup, d1_custom_name_lookup sound_lookup,
                              void *lookup_context)
{
	d1_custom_cursor cursor;
	size_t bitmap_bytes, data_start, directory_start, sound_bytes, total_bytes = 0;
	int first, second, bitmap_count, sound_count = 0, pog = 0, has_indices = 0, i;
	const uint8_t *indices = NULL;
	if (!data || !limits || size < 8 || size > D1_CUSTOM_MAX_FILE_BYTES ||
	    limits->max_bitmaps < 2 || limits->max_sounds < 1) return 0;
	cursor.data = data;
	cursor.size = size;
	cursor.position = 0;
	if (!d1_custom_read_i32(&cursor, &first) || !d1_custom_read_i32(&cursor, &second)) return 0;
	if (first == 0x47495050 || first == 0x474f5044) {
		pog = 1;
		has_indices = first == 0x474f5044;
		if (second != (has_indices ? 1 : 2) || !d1_custom_read_i32(&cursor, &bitmap_count)) return 0;
	} else if (first >= 0 && first < limits->max_bitmaps && second >= 0 && second < limits->max_sounds) {
		bitmap_count = first;
		sound_count = second;
	} else {
		if (first < 8 || !d1_custom_span(size, (size_t) first, 8)) return 0;
		cursor.position = (size_t) first;
		if (!d1_custom_read_i32(&cursor, &bitmap_count) || !d1_custom_read_i32(&cursor, &sound_count)) return 0;
	}
	if (bitmap_count < 0 || bitmap_count >= limits->max_bitmaps ||
	    sound_count < 0 || sound_count >= limits->max_sounds) return 0;
	directory_start = cursor.position;
	if (!d1_custom_checked_mul((size_t) bitmap_count, pog ? D1_CUSTOM_POG_HEADER : D1_CUSTOM_PIG_HEADER, &bitmap_bytes) ||
	    !d1_custom_checked_mul((size_t) sound_count, D1_CUSTOM_SOUND_HEADER, &sound_bytes) ||
	    (has_indices && !d1_custom_take(&cursor, (size_t) bitmap_count * 2, &indices)) ||
	    !d1_custom_checked_add(cursor.position, bitmap_bytes, &data_start) ||
	    !d1_custom_checked_add(data_start, sound_bytes, &data_start) || data_start > size)
		return 0;
	for (i = 0; i < bitmap_count; ++i) {
		const uint8_t *header;
		char name[15];
		size_t absolute;
		int index = -1, offset, width, height;
		unsigned int flags;
		if (!d1_custom_take(&cursor, pog ? D1_CUSTOM_POG_HEADER : D1_CUSTOM_PIG_HEADER, &header)) return 0;
		d1_custom_bitmap_name(name, header, 64);
		if (has_indices)
			index = d1_custom_u16(indices + (size_t) i * 2);
		else if (bitmap_lookup)
			index = bitmap_lookup(name, lookup_context);
		if (!d1_custom_valid_lookup(index, limits->max_bitmaps)) return 0;
		if (pog) {
			width = header[9] + ((header[11] & 15) << 8);
			height = header[10] + ((header[11] >> 4) << 8);
			flags = header[12];
			offset = d1_custom_i32(header + 14);
		} else {
			width = header[9] + ((header[8] & 128) ? 256 : 0);
			height = header[10];
			flags = header[11];
			offset = d1_custom_i32(header + 13);
		}
		if (offset < 0 || !d1_custom_checked_add(data_start, (size_t) offset, &absolute) ||
		    !d1_custom_validate_bitmap(data, size, absolute, width, height, flags, limits, &total_bytes))
			return 0;
	}
	for (i = 0; i < sound_count; ++i) {
		const uint8_t *header;
		char name[9];
		size_t absolute;
		int index = -1, length, offset;
		if (!d1_custom_take(&cursor, D1_CUSTOM_SOUND_HEADER, &header)) return 0;
		memcpy(name, header, 8);
		name[8] = 0;
		if (sound_lookup) index = sound_lookup(name, lookup_context);
		if (index != -1 && (index < 0 || index >= limits->max_sounds)) return 0;
		length = d1_custom_i32(header + 8);
		offset = d1_custom_i32(header + 16);
		if (length <= 0 || offset < 0 || !d1_custom_checked_add(data_start, (size_t) offset, &absolute) ||
		    !d1_custom_span(size, absolute, (size_t) length) ||
		    (size_t) length > D1_CUSTOM_MAX_TOTAL_BYTES - total_bytes)
			return 0;
		total_bytes += (size_t) length;
	}
	(void) directory_start;
	return 1;
}

static int d1_custom_model_word(const uint8_t *data, size_t size, size_t offset, uint16_t *value)
{
	if (!value || !d1_custom_span(size, offset, 2)) return 0;
	*value = d1_custom_u16(data + offset);
	return 1;
}

static int d1_custom_validate_model_inner(const uint8_t *data, size_t size, size_t offset,
                                          int depth, size_t *steps, int texture_count)
{
	size_t cursor = offset;
	if (!data || !steps || depth > D1_CUSTOM_MODEL_DEPTH || offset >= size) return 0;
	while (cursor < size) {
		uint16_t opcode, count, branch, texture;
		size_t record_size;
		if (!*steps || !d1_custom_model_word(data, size, cursor, &opcode)) return 0;
		--*steps;
		switch (opcode) {
			case 0: return 1;
			case 1:
				if (!d1_custom_model_word(data, size, cursor + 2, &count)) return 0;
				record_size = 4 + (size_t) count * 12;
				break;
			case 7:
				if (!d1_custom_model_word(data, size, cursor + 2, &count)) return 0;
				record_size = 8 + (size_t) count * 12;
				break;
			case 2:
				if (!d1_custom_model_word(data, size, cursor + 2, &count)) return 0;
				record_size = 30 + (size_t) ((count & ~1u) + 1u) * 2;
				break;
			case 3:
				if (!d1_custom_model_word(data, size, cursor + 2, &count) ||
				    !d1_custom_model_word(data, size, cursor + 28, &texture) || texture >= texture_count)
					return 0;
				record_size = 30 + (size_t) ((count & ~1u) + 1u) * 2 + (size_t) count * 12;
				break;
			case 4:
				if (!d1_custom_span(size, cursor, 32) ||
				    !d1_custom_model_word(data, size, cursor + 28, &branch) || !branch || branch >= size - cursor ||
				    !d1_custom_validate_model_inner(data, size, cursor + branch, depth + 1, steps, texture_count) ||
				    !d1_custom_model_word(data, size, cursor + 30, &branch) || !branch || branch >= size - cursor ||
				    !d1_custom_validate_model_inner(data, size, cursor + branch, depth + 1, steps, texture_count))
					return 0;
				record_size = 32;
				break;
			case 5:
				if (!d1_custom_model_word(data, size, cursor + 2, &texture) || texture >= texture_count) return 0;
				record_size = 36;
				break;
			case 6:
				if (!d1_custom_span(size, cursor, 20) ||
				    !d1_custom_model_word(data, size, cursor + 16, &branch) || !branch || branch >= size - cursor ||
				    !d1_custom_validate_model_inner(data, size, cursor + branch, depth + 1, steps, texture_count))
					return 0;
				record_size = 20;
				break;
			case 8: record_size = 4; break;
			default: return 0;
		}
		if (!d1_custom_span(size, cursor, record_size)) return 0;
		cursor += record_size;
	}
	return 0;
}

static int d1_custom_validate_model(const uint8_t *data, size_t size, size_t offset, int texture_count)
{
	size_t steps;
	if (!size || texture_count < 0 || size > (SIZE_MAX - 1) / 32) return 0;
	steps = size * 32 + 1;
	return d1_custom_validate_model_inner(data, size, offset, 0, &steps, texture_count);
}

static int d1_custom_read_count(d1_custom_cursor *cursor, int maximum, int *count)
{
	return d1_custom_read_i32(cursor, count) && *count >= 0 && *count <= maximum;
}

int d1_custom_validate_hx1(const uint8_t *data, size_t size,
                           const d1_custom_validation_limits *limits)
{
	d1_custom_cursor cursor;
	size_t total_model_bytes = 0;
	int count, i, index;
	if (!data || !limits || size < 8 || size > D1_CUSTOM_MAX_FILE_BYTES) return 0;
	cursor.data = data;
	cursor.size = size;
	cursor.position = 0;
	if (!d1_custom_read_i32(&cursor, &index) || index != 0x21584d48 ||
	    !d1_custom_read_i32(&cursor, &index) || index != 1) return 0;
	if (!d1_custom_read_count(&cursor, limits->max_robots, &count)) return 0;
	for (i = 0; i < count; ++i) {
		const uint8_t *record;
		int joint;
		if (!d1_custom_read_i32(&cursor, &index) || index < 0 || index >= limits->max_robots ||
		    !d1_custom_take(&cursor, D1_CUSTOM_ROBOT_RECORD, &record) ||
		    d1_custom_i32(record) < 0 || d1_custom_i32(record) >= limits->max_models ||
		    record[118] > 8)
			return 0;
		for (joint = 0; joint < 45; ++joint) {
			int joints = d1_custom_i16(record + 296 + joint * 4);
			int offset = d1_custom_i16(record + 298 + joint * 4);
			if (joints < 0 || offset < 0 || joints > limits->max_joints - offset) return 0;
		}
	}
	if (!d1_custom_read_count(&cursor, limits->max_joints, &count)) return 0;
	for (i = 0; i < count; ++i) {
		const uint8_t *record;
		if (!d1_custom_read_i32(&cursor, &index) || index < 0 || index >= limits->max_joints ||
		    !d1_custom_take(&cursor, 8, &record) || d1_custom_i16(record) < 0 ||
		    d1_custom_i16(record) >= limits->max_submodels)
			return 0;
	}
	if (!d1_custom_read_count(&cursor, limits->max_models, &count)) return 0;
	for (i = 0; i < count; ++i) {
		const uint8_t *model, *model_data, *trailing;
		int data_size, first_texture, model_count, texture_count, submodel;
		if (!d1_custom_read_i32(&cursor, &index) || index < 0 || index >= limits->max_models ||
		    !d1_custom_take(&cursor, D1_CUSTOM_MODEL_RECORD, &model)) return 0;
		model_count = d1_custom_i32(model);
		data_size = d1_custom_i32(model + 4);
		texture_count = model[730];
		first_texture = d1_custom_i16(model + 731);
		if (model_count <= 0 || model_count > limits->max_submodels || data_size <= 0 ||
		    (size_t) data_size > D1_CUSTOM_MAX_IMAGE_BYTES || first_texture < 0 ||
		    texture_count > limits->max_object_bitmaps - first_texture ||
		    !d1_custom_take(&cursor, (size_t) data_size, &model_data) ||
		    !d1_custom_take(&cursor, 8, &trailing) ||
		    (size_t) data_size > D1_CUSTOM_MAX_TOTAL_BYTES - total_model_bytes)
			return 0;
		total_model_bytes += (size_t) data_size;
		for (submodel = 0; submodel < model_count; ++submodel) {
			int offset = d1_custom_i32(model + 12 + submodel * 4);
			if (offset < 0 || !d1_custom_validate_model(model_data, (size_t) data_size,
			                                            (size_t) offset, texture_count))
				return 0;
		}
		if ((d1_custom_i32(trailing) != -1 &&
		     (d1_custom_i32(trailing) < 0 || d1_custom_i32(trailing) >= limits->max_models)) ||
		    (d1_custom_i32(trailing + 4) != -1 &&
		     (d1_custom_i32(trailing + 4) < 0 || d1_custom_i32(trailing + 4) >= limits->max_models)))
			return 0;
	}
	if (!d1_custom_read_count(&cursor, limits->max_object_bitmaps, &count)) return 0;
	for (i = 0; i < count; ++i) {
		const uint8_t *bitmap;
		if (!d1_custom_read_i32(&cursor, &index) || index < 0 || index >= limits->max_object_bitmaps ||
		    !d1_custom_take(&cursor, 2, &bitmap) || d1_custom_u16(bitmap) >= limits->max_bitmaps)
			return 0;
	}
	return cursor.position == size;
}
