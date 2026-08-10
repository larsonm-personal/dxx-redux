#include <stdio.h>
#include <string.h>

#include "bounded_rle.h"

static void set_size(unsigned char *data, size_t size)
{
	data[0] = (unsigned char)size;
	data[1] = (unsigned char)(size >> 8);
	data[2] = (unsigned char)(size >> 16);
	data[3] = (unsigned char)(size >> 24);
}

static int expect(const char *name, int actual, int expected)
{
	if (actual == expected)
		return 1;
	fprintf(stderr, "%s: expected %d, got %d\n", name, expected, actual);
	return 0;
}

int main(void)
{
	unsigned char valid[] = {0, 0, 0, 0, 4, 4, 1, 2, 3, 0xe0, 0xe2, 9, 8, 0xe0};
	unsigned char work[sizeof(valid)];
	int ok = 1;

	set_size(valid, sizeof(valid));
	ok &= expect("valid rows", bounded_rle_validate_bitmap(valid, sizeof(valid), 3, 2, 1), 1);
	ok &= expect("truncated header", bounded_rle_validate_bitmap(valid, 3, 3, 2, 1), 0);
	ok &= expect("short row table", bounded_rle_validate_bitmap(valid, 5, 3, 2, 1), 0);

	memcpy(work, valid, sizeof(work));
	work[4] = 0;
	ok &= expect("zero row", bounded_rle_validate_bitmap(work, sizeof(work), 3, 2, 1), 0);
	memcpy(work, valid, sizeof(work));
	work[4] = 5;
	ok &= expect("oversized row", bounded_rle_validate_bitmap(work, sizeof(work), 3, 2, 1), 0);
	memcpy(work, valid, sizeof(work));
	work[9] = 4;
	ok &= expect("missing terminator", bounded_rle_validate_bitmap(work, sizeof(work), 3, 2, 1), 0);
	memcpy(work, valid, sizeof(work));
	work[6] = 0xe0;
	ok &= expect("early terminator", bounded_rle_validate_bitmap(work, sizeof(work), 3, 2, 1), 0);
	memcpy(work, valid, sizeof(work));
	work[8] = 0xe1;
	ok &= expect("truncated run", bounded_rle_validate_bitmap(work, sizeof(work), 3, 2, 1), 0);
	memcpy(work, valid, sizeof(work));
	work[6] = 1;
	work[7] = 0xe0;
	work[4] = 2;
	set_size(work, sizeof(work) - 2);
	ok &= expect("width minus one", bounded_rle_validate_bitmap(work, sizeof(work) - 2, 3, 2, 1), 0);
	memcpy(work, valid, sizeof(work));
	work[10] = 0xe4;
	ok &= expect("width plus one", bounded_rle_validate_bitmap(work, sizeof(work), 3, 2, 1), 0);
	memcpy(work, valid, sizeof(work));
	work[0]++;
	ok &= expect("declared size mismatch", bounded_rle_validate_bitmap(work, sizeof(work), 3, 2, 1), 0);
	ok &= expect("unsupported row table", bounded_rle_validate_bitmap(valid, sizeof(valid), 3, 2, 3), 0);

	return ok ? 0 : 1;
}
