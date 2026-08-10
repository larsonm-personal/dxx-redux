#include "bounded_rle.h"

#include <stdint.h>

#define BOUNDED_RLE_CODE       0xe0u
#define BOUNDED_RLE_COUNT_MASK 0x1fu

static int bounded_rle_validate_row(const unsigned char *row, size_t row_size,
                                    int width)
{
	size_t position = 0;
	int decoded = 0;

	while (position < row_size) {
		unsigned int value = row[position++];

		if ((value & BOUNDED_RLE_CODE) != BOUNDED_RLE_CODE) {
			if (decoded == width)
				return 0;
			decoded++;
			continue;
		}
		value &= BOUNDED_RLE_COUNT_MASK;
		if (!value)
			return position == row_size && decoded == width;
		if (position == row_size || value > (unsigned int) (width - decoded))
			return 0;
		position++;
		decoded += (int) value;
	}
	return 0;
}

int bounded_rle_validate_bitmap(const unsigned char *data, size_t data_size,
                                int width, int height, int row_size_bytes)
{
	size_t table_size;
	size_t position;
	uint32_t declared_size;
	int row;

	if (!data || width <= 0 || height <= 0 ||
	    (row_size_bytes != 1 && row_size_bytes != 2))
		return 0;
	if ((size_t) height > (SIZE_MAX - 4) / (size_t) row_size_bytes)
		return 0;
	table_size = (size_t) height * (size_t) row_size_bytes;
	if (data_size < 4 + table_size || data_size > UINT32_MAX)
		return 0;
	declared_size = (uint32_t) data[0] |
	                ((uint32_t) data[1] << 8) |
	                ((uint32_t) data[2] << 16) |
	                ((uint32_t) data[3] << 24);
	if (declared_size != data_size)
		return 0;
	position = 4 + table_size;
	for (row = 0; row < height; row++) {
		size_t row_size = data[4 + (size_t) row * (size_t) row_size_bytes];

		if (row_size_bytes == 2)
			row_size |= (size_t) data[5 + (size_t) row * 2] << 8;
		if (!row_size || row_size > data_size - position ||
		    !bounded_rle_validate_row(data + position, row_size, width))
			return 0;
		position += row_size;
	}
	return position == data_size;
}
