#include "cd_read_contract.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                         \
	do {                                                                         \
		if (!(condition)) {                                                      \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
			return 1;                                                            \
		}                                                                        \
	} while (0)

int main(void)
{
	const char *path = "cd_read_contract_test.tmp";
	FILE *file = fopen(path, "wb");
	char *data = NULL;
	size_t length = 0;
	long long offset = -1;
	long long bytes = -1;

	CHECK(file != NULL);
	CHECK(fwrite("FILE sample.bin BINARY\n", 1, 23, file) == 23);
	CHECK(fclose(file) == 0);
	CHECK(cd_read_file_exact(path, 23, &data, &length));
	CHECK(length == 23 && memcmp(data, "FILE sample.bin BINARY\n", 23) == 0);
	free(data);
	CHECK(!cd_read_file_exact(path, 22, &data, &length));
	CHECK(remove(path) == 0);
	CHECK(!cd_read_file_exact(path, 23, &data, &length));

	CHECK(cd_track_span(2, 3, 5 * 2352LL, &offset, &bytes));
	CHECK(offset == 2 * 2352LL && bytes == 3 * 2352LL);
	CHECK(!cd_track_span(-1, 1, 2352, NULL, NULL));
	CHECK(!cd_track_span(0, 0, 2352, NULL, NULL));
	CHECK(!cd_track_span(0, 2, 2352, NULL, NULL));
	CHECK(!cd_track_span(INT_MAX, INT_MAX, 2352, NULL, NULL));
	CHECK(cd_track_span_with_stride(2, 3, 2048, 5 * 2048LL, &offset, &bytes));
	CHECK(offset == 2 * 2048LL && bytes == 3 * 2048LL);
	CHECK(!cd_track_span_with_stride(0, 1, 0, 2048, NULL, NULL));
	printf("CD read contract tests passed\n");
	return 0;
}
