#include "d1_pig_validation.h"

#define D1_MODEL_OP_EOF        0
#define D1_MODEL_OP_DEFPOINTS  1
#define D1_MODEL_OP_FLATPOLY   2
#define D1_MODEL_OP_TMAPPOLY   3
#define D1_MODEL_OP_SORTNORM   4
#define D1_MODEL_OP_RODBM      5
#define D1_MODEL_OP_SUBCALL    6
#define D1_MODEL_OP_DEFP_START 7
#define D1_MODEL_OP_GLOW       8
#define D1_MODEL_MAX_RECURSION 256
#define D1_VECTOR_SIZE         12
#define D1_FIX_SIZE            4

int d1_pig_validate_span(int64_t file_size, int64_t offset, int64_t size)
{
	return file_size >= 0 && offset >= 0 && size >= 0 && offset <= file_size && size <= file_size - offset;
}

int d1_pig_validate_arena(size_t remaining, int data_size, int slack)
{
	return data_size > 0 && slack >= 0 && (size_t) data_size <= remaining && (size_t) slack <= remaining - data_size;
}

int d1_pig_validate_rle(const uint8_t *data, size_t size, int width, int height, int big_rows)
{
	size_t header_size, row_offset;
	int row;

	if (!data || size < 4 || width <= 0 || height <= 0 ||
	    (size_t) height > (SIZE_MAX - 4) / (big_rows ? 2 : 1))
		return 0;
	header_size = 4 + (size_t) height * (big_rows ? 2 : 1);
	if (header_size > size ||
	    (uint32_t) data[0] + ((uint32_t) data[1] << 8) + ((uint32_t) data[2] << 16) +
	            ((uint32_t) data[3] << 24) !=
	        size)
		return 0;
	row_offset = header_size;
	for (row = 0; row < height; row++) {
		size_t cursor, row_end;
		unsigned int pixels = 0;
		unsigned int row_size = big_rows
		                            ? (unsigned int) data[4 + 2 * row] | ((unsigned int) data[5 + 2 * row] << 8)
		                            : data[4 + row];

		if (!row_size || row_size > size - row_offset)
			return 0;
		cursor = row_offset;
		row_end = row_offset + row_size;
		while (cursor < row_end) {
			unsigned int token = data[cursor++];
			unsigned int count;

			if ((token & 0xe0) != 0xe0) {
				count = 1;
			} else {
				count = token & 0x1f;
				if (!count) {
					if (cursor != row_end || pixels != (unsigned int) width)
						return 0;
					break;
				}
				if (cursor >= row_end)
					return 0;
				cursor++;
			}
			if (count > (unsigned int) width - pixels)
				return 0;
			pixels += count;
		}
		if (cursor != row_end || !row_end || data[row_end - 1] != 0xe0)
			return 0;
		row_offset = row_end;
	}
	return row_offset == size;
}

int d1_pig_measure_remapped_rle(const uint8_t *data, size_t size, int width, int height,
                                int big_rows, const uint8_t colormap[256], size_t *remapped_size)
{
	size_t input_offset, output_size;
	int row;

	if (!colormap || !remapped_size || !d1_pig_validate_rle(data, size, width, height, big_rows))
		return 0;
	input_offset = 4 + (size_t) height * (big_rows ? 2 : 1);
	output_size = input_offset;
	for (row = 0; row < height; row++) {
		size_t cursor = input_offset;
		size_t row_output_size = 0;
		unsigned int row_size = big_rows
		                            ? (unsigned int) data[4 + 2 * row] | ((unsigned int) data[5 + 2 * row] << 8)
		                            : data[4 + row];

		input_offset += row_size;
		while (cursor < input_offset) {
			unsigned int token = data[cursor++];

			if ((token & 0xe0) != 0xe0) {
				row_output_size += (colormap[token] & 0xe0) == 0xe0 ? 2 : 1;
			} else if (token & 0x1f) {
				cursor++;
				row_output_size += 2;
			} else {
				row_output_size++;
			}
		}
		if (row_output_size > (big_rows ? 65535u : 255u) || row_output_size > SIZE_MAX - output_size)
			return 0;
		output_size += row_output_size;
	}
	*remapped_size = output_size;
	return 1;
}

int d1_pig_valid_model_index(int index, int model_count)
{
	return index >= 0 && index < model_count;
}

int d1_pig_valid_optional_model_index(int index, int model_count)
{
	return index == -1 || d1_pig_valid_model_index(index, model_count);
}

static int d1_model_word_at(const uint8_t *data, size_t size, size_t offset, uint16_t *value)
{
	if (!data || !value || offset > size || size - offset < sizeof(*value))
		return 0;
	*value = (uint16_t) (data[offset] | ((uint16_t) data[offset + 1] << 8));
	return 1;
}

static int validate_d1_model_stream_inner(const uint8_t *data, size_t size, size_t offset, int depth,
                                          size_t *remaining_steps)
{
	size_t cursor = offset;

	if (depth > D1_MODEL_MAX_RECURSION || offset >= size || !remaining_steps)
		return 0;
	while (cursor < size) {
		uint16_t opcode, count, branch;
		size_t record_size;

		if (!*remaining_steps)
			return 0;
		(*remaining_steps)--;
		if (!d1_model_word_at(data, size, cursor, &opcode))
			return 0;
		switch (opcode) {
			case D1_MODEL_OP_EOF:
				return 1;
			case D1_MODEL_OP_DEFPOINTS:
				if (!d1_model_word_at(data, size, cursor + 2, &count))
					return 0;
				record_size = 4 + (size_t) count * D1_VECTOR_SIZE;
				break;
			case D1_MODEL_OP_DEFP_START:
				if (!d1_model_word_at(data, size, cursor + 2, &count))
					return 0;
				record_size = 8 + (size_t) count * D1_VECTOR_SIZE;
				break;
			case D1_MODEL_OP_FLATPOLY:
				if (!d1_model_word_at(data, size, cursor + 2, &count))
					return 0;
				record_size = 30 + (size_t) ((count & ~1) + 1) * sizeof(uint16_t);
				break;
			case D1_MODEL_OP_TMAPPOLY:
				if (!d1_model_word_at(data, size, cursor + 2, &count))
					return 0;
				record_size = 30 + (size_t) ((count & ~1) + 1) * sizeof(uint16_t) +
				              (size_t) count * 3 * D1_FIX_SIZE;
				break;
			case D1_MODEL_OP_SORTNORM:
				if (cursor > size || size - cursor < 32 ||
				    !d1_model_word_at(data, size, cursor + 28, &branch) || !branch ||
				    branch >= size - cursor ||
				    !validate_d1_model_stream_inner(data, size, cursor + branch, depth + 1,
				                                    remaining_steps) ||
				    !d1_model_word_at(data, size, cursor + 30, &branch) || !branch ||
				    branch >= size - cursor ||
				    !validate_d1_model_stream_inner(data, size, cursor + branch, depth + 1,
				                                    remaining_steps))
					return 0;
				record_size = 32;
				break;
			case D1_MODEL_OP_RODBM:
				record_size = 36;
				break;
			case D1_MODEL_OP_SUBCALL:
				if (cursor > size || size - cursor < 20 ||
				    !d1_model_word_at(data, size, cursor + 16, &branch) || !branch ||
				    branch >= size - cursor ||
				    !validate_d1_model_stream_inner(data, size, cursor + branch, depth + 1,
				                                    remaining_steps))
					return 0;
				record_size = 20;
				break;
			case D1_MODEL_OP_GLOW:
				record_size = 4;
				break;
			default:
				return 0;
		}
		if (record_size > size - cursor)
			return 0;
		cursor += record_size;
	}
	return 0;
}

int d1_pig_validate_model_stream(const uint8_t *data, size_t size, size_t offset)
{
	size_t remaining_steps;

	if (size > (SIZE_MAX - 1) / 32)
		return 0;
	remaining_steps = size * 32 + 1;
	return validate_d1_model_stream_inner(data, size, offset, 0, &remaining_steps);
}
