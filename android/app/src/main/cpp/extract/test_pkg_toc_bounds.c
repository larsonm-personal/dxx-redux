#include "pkg_reader.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

static int failures;

#define CHECK(condition)                                            \
	do {                                                            \
		if (!(condition)) {                                         \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			        #condition);                                    \
			failures++;                                             \
		}                                                           \
	} while (0)

static void test_xar_toc_bounds(void)
{
	const uint64_t header_size = 28;
	const uint64_t small_compressed = 64;

	CHECK(pkg_test_validate_xar_toc(28, 1, small_compressed, 128,
	                                header_size + small_compressed) == 1);
	CHECK(pkg_test_validate_xar_toc(28, 1, PKG_MAX_TOC_BYTES,
	                                PKG_MAX_TOC_BYTES,
	                                header_size + PKG_MAX_TOC_BYTES) == 1);

	CHECK(pkg_test_validate_xar_toc(27, 1, 64, 128, 92) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 2, 64, 128, 92) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, 0, 128, 92) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, 64, 0, 92) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, 64, 128, 27) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, 64, 128, 91) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, PKG_MAX_TOC_BYTES + 1, 128,
	                                UINT64_MAX) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, 64, PKG_MAX_TOC_BYTES + 1,
	                                UINT64_MAX) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, INT_MAX, 128, UINT64_MAX) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, UINT32_MAX, UINT32_MAX,
	                                UINT64_MAX) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, (uint64_t) SIZE_MAX,
	                                (uint64_t) SIZE_MAX, UINT64_MAX) == 0);
	CHECK(pkg_test_validate_xar_toc(28, 1, UINT64_MAX, UINT64_MAX,
	                                UINT64_MAX) == 0);
}

static void test_xar_toc_decompression(void)
{
	static const uint8_t xml[] = "<xar><toc><file/></toc></xar>";
	uLong compressed_capacity = compressBound(sizeof(xml) - 1);
	uint8_t *compressed = (uint8_t *) malloc((size_t) compressed_capacity + 1);
	uint8_t output[sizeof(xml)];
	uLong compressed_size = compressed_capacity;

	CHECK(compressed != NULL);
	if (!compressed) return;
	CHECK(compress2(compressed, &compressed_size, xml, sizeof(xml) - 1,
	                Z_BEST_SPEED) == Z_OK);

	memset(output, 0, sizeof(output));
	CHECK(pkg_test_decompress_toc(compressed, (size_t) compressed_size, output,
	                              sizeof(xml) - 1) == 0);
	CHECK(memcmp(output, xml, sizeof(xml) - 1) == 0);

	CHECK(pkg_test_decompress_toc(compressed, (size_t) compressed_size, output,
	                              sizeof(xml) - 2) == -1);
	CHECK(pkg_test_decompress_toc(compressed, (size_t) compressed_size, output,
	                              sizeof(xml)) == -1);
	CHECK(pkg_test_decompress_toc(compressed, (size_t) compressed_size - 1,
	                              output, sizeof(xml) - 1) == -1);

	compressed[compressed_size] = 0;
	CHECK(pkg_test_decompress_toc(compressed, (size_t) compressed_size + 1,
	                              output, sizeof(xml) - 1) == -1);

	free(compressed);
}

int main(void)
{
	test_xar_toc_bounds();
	test_xar_toc_decompression();
	if (failures) {
		fprintf(stderr, "%d PKG TOC bound test(s) failed\n", failures);
		return 1;
	}
	puts("PKG TOC bound tests passed");
	return 0;
}
