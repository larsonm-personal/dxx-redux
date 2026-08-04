#include "saf_io_contract.h"

#include <stdint.h>
#include <stdio.h>

#define CHECK(condition)                                                         \
	do {                                                                         \
		if (!(condition)) {                                                      \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
			return 1;                                                            \
		}                                                                        \
	} while (0)

int main(void)
{
	int64_t position = -1;
	CHECK(saf_io_resolve_seek(0, 10, &position) && position == 0);
	CHECK(saf_io_resolve_seek(10, 10, &position) && position == 10);
	CHECK(!saf_io_resolve_seek(11, 10, &position));
	CHECK(!saf_io_resolve_seek(UINT64_MAX, INT64_MAX, &position));
	CHECK(!saf_io_resolve_seek(0, -1, &position));
	CHECK(!saf_io_resolve_seek(0, 0, NULL));
	printf("SAF I/O contract tests passed\n");
	return 0;
}
