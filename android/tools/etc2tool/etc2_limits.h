#ifndef ETC2_LIMITS_H
#define ETC2_LIMITS_H

#include <stddef.h>
#include <stdint.h>

#define ETC2TOOL_MAX_DIMENSION 2048

struct etc2tool_layout {
	int padded_width;
	int padded_height;
	int mip_count;
	size_t base_rgba_bytes;
	size_t base_float_bytes;
	size_t total_rgba_mip_bytes;
	size_t total_etc2_mip_bytes;
};

static inline int etc2tool_checked_mul(size_t left, size_t right, size_t *result)
{
	if (!result || (left && right > SIZE_MAX / left))
		return 0;
	*result = left * right;
	return 1;
}

static inline int etc2tool_pow2_dimension(int value, int *rounded)
{
	int result = 1;

	if (!rounded || value <= 0 || value > ETC2TOOL_MAX_DIMENSION)
		return 0;
	while (result < value) {
		if (result > ETC2TOOL_MAX_DIMENSION / 2)
			return 0;
		result *= 2;
	}
	if (result < 4)
		result = 4;
	*rounded = result;
	return 1;
}

static inline int etc2tool_level_sizes(int width, int height,
                                       size_t *rgba_bytes, size_t *float_bytes, size_t *etc2_bytes)
{
	size_t pixels;
	size_t components;
	size_t block_count;
	size_t blocks_w;
	size_t blocks_h;

	if (width <= 0 || height <= 0 || !rgba_bytes || !float_bytes || !etc2_bytes ||
	    !etc2tool_checked_mul((size_t) width, (size_t) height, &pixels) ||
	    !etc2tool_checked_mul(pixels, 4u, &components) ||
	    !etc2tool_checked_mul(components, sizeof(float), float_bytes))
		return 0;
	*rgba_bytes = components;
	blocks_w = ((size_t) width + 3u) / 4u;
	blocks_h = ((size_t) height + 3u) / 4u;
	return etc2tool_checked_mul(blocks_w, blocks_h, &block_count) &&
	       etc2tool_checked_mul(block_count, 16u, etc2_bytes);
}

static inline int etc2tool_image_layout(int width, int height, int channels,
                                        int generate_mips, struct etc2tool_layout *layout)
{
	struct etc2tool_layout result = { 0 };
	int mip_width;
	int mip_height;

	if (!layout || channels < 1 || channels > 4 ||
	    !etc2tool_pow2_dimension(width, &result.padded_width) ||
	    !etc2tool_pow2_dimension(height, &result.padded_height))
		return 0;
	mip_width = result.padded_width;
	mip_height = result.padded_height;
	result.mip_count = 1;
	for (;;) {
		size_t rgba_bytes;
		size_t float_bytes;
		size_t etc2_bytes;

		if (!etc2tool_level_sizes(mip_width, mip_height, &rgba_bytes, &float_bytes,
		                          &etc2_bytes) ||
		    result.total_rgba_mip_bytes > SIZE_MAX - rgba_bytes ||
		    result.total_etc2_mip_bytes > SIZE_MAX - etc2_bytes)
			return 0;
		if (result.mip_count == 1) {
			result.base_rgba_bytes = rgba_bytes;
			result.base_float_bytes = float_bytes;
		}
		result.total_rgba_mip_bytes += rgba_bytes;
		result.total_etc2_mip_bytes += etc2_bytes;
		if (!generate_mips || mip_width <= 4 || mip_height <= 4)
			break;
		mip_width /= 2;
		mip_height /= 2;
		result.mip_count++;
	}
	*layout = result;
	return 1;
}

#endif
