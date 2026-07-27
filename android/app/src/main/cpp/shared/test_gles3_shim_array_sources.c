#include "gles3_shim_array_sources.h"

#include <stdio.h>

static int failures;

#define CHECK(condition, message)                   \
	do {                                            \
		if (!(condition)) {                         \
			fprintf(stderr, "FAIL: %s\n", message); \
			++failures;                             \
		}                                           \
	} while (0)

static void test_client_arrays(void)
{
	const struct gles3_shim_array_source sources[] = {
		{ 1, 0 },
		{ 1, 0 },
		{ 1, 0 },
		{ 1, 0 }
	};
	unsigned int buffer = 99;
	int kind = gles3_shim_choose_array_source(sources, 4, &buffer);

	CHECK(kind == GLES3_SHIM_ARRAY_SOURCE_CLIENT, "client arrays use streaming");
	CHECK(buffer == 0, "client arrays select no caller buffer");
}

static void test_interleaved_vbo_offsets(void)
{
	const struct gles3_shim_array_source sources[] = {
		{ 1, 17 },
		{ 0, 91 },
		{ 1, 17 },
		{ 0, 0 }
	};
	unsigned int buffer = 0;
	int kind = gles3_shim_choose_array_source(sources, 4, &buffer);

	CHECK(kind == GLES3_SHIM_ARRAY_SOURCE_BUFFER,
	      "zero and nonzero offsets from one VBO use the buffer path");
	CHECK(buffer == 17, "the caller VBO is preserved");
}

static void test_incompatible_sources(void)
{
	const struct gles3_shim_array_source mixed[] = {
		{ 1, 17 },
		{ 1, 0 }
	};
	const struct gles3_shim_array_source different_buffers[] = {
		{ 1, 17 },
		{ 1, 18 }
	};
	unsigned int buffer = 0;

	CHECK(gles3_shim_choose_array_source(mixed, 2, &buffer) ==
	          GLES3_SHIM_ARRAY_SOURCE_REJECT,
	      "mixed client and VBO arrays are rejected");
	CHECK(gles3_shim_choose_array_source(different_buffers, 2, &buffer) ==
	          GLES3_SHIM_ARRAY_SOURCE_REJECT,
	      "different VBO bindings are rejected");
}

int main(void)
{
	test_client_arrays();
	test_interleaved_vbo_offsets();
	test_incompatible_sources();
	if (failures) {
		fprintf(stderr, "%d GLES shim array-source test(s) failed\n", failures);
		return 1;
	}
	puts("GLES shim array-source tests passed");
	return 0;
}
