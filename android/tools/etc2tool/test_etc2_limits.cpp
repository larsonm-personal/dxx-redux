#include "etc2_limits.h"

#include <climits>
#include <cstdio>

static int fail(const char *message)
{
	std::fprintf(stderr, "%s\n", message);
	return 1;
}

int main()
{
	struct etc2tool_layout layout;
	size_t value;

	if (!etc2tool_image_layout(1, 1, 1, 1, &layout) ||
	    layout.padded_width != 4 || layout.padded_height != 4 ||
	    layout.mip_count != 1 || layout.base_rgba_bytes != 64 ||
	    layout.base_float_bytes != 256 || layout.total_etc2_mip_bytes != 16)
		return fail("minimum texture layout failed");
	if (!etc2tool_image_layout(1025, 1024, 4, 0, &layout) ||
	    layout.padded_width != 2048 || layout.padded_height != 1024 ||
	    layout.base_rgba_bytes != 8u * 1024u * 1024u || layout.mip_count != 1)
		return fail("power-of-two transition failed");
	if (!etc2tool_image_layout(2048, 2048, 4, 1, &layout) ||
	    layout.padded_width != 2048 || layout.padded_height != 2048 ||
	    layout.base_rgba_bytes != 16u * 1024u * 1024u ||
	    layout.base_float_bytes != 64u * 1024u * 1024u || layout.mip_count != 10)
		return fail("maximum texture layout failed");
	if (etc2tool_image_layout(0, 1, 1, 1, &layout) ||
	    etc2tool_image_layout(1, 0, 1, 1, &layout) ||
	    etc2tool_image_layout(2049, 1, 1, 1, &layout) ||
	    etc2tool_image_layout(1, 2049, 1, 1, &layout) ||
	    etc2tool_image_layout(1, 1, 0, 1, &layout) ||
	    etc2tool_image_layout(1, 1, 5, 1, &layout) ||
	    etc2tool_image_layout(32769, 8193, 1, 1, &layout))
		return fail("invalid texture layout was admitted");
	if (etc2tool_checked_mul(SIZE_MAX, 2, &value) ||
	    !etc2tool_checked_mul(SIZE_MAX, 1, &value) || value != SIZE_MAX)
		return fail("checked multiplication boundary failed");
	std::puts("PASS");
	return 0;
}
