#include "rgba8888.h"

#include <stdio.h>
#include <string.h>

struct rgba8888_case {
	const char *name;
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint8_t alpha;
};

int main(void)
{
	static const struct rgba8888_case cases[] = {
		{ "red", 0xff, 0x00, 0x00, 0xff },
		{ "blue", 0x00, 0x00, 0xff, 0xff },
		{ "mixed", 0x12, 0x34, 0x56, 0x78 },
		{ "transparent", 0xa5, 0x5a, 0xc3, 0x00 },
		{ "gray", 0x7f, 0x7f, 0x7f, 0xff },
	};
	int failures = 0;

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		uint8_t actual[6];
		const uint8_t expected[] = {
			0xcc,
			cases[i].red,
			cases[i].green,
			cases[i].blue,
			cases[i].alpha,
			0xcc,
		};

		memset(actual, 0xcc, sizeof(actual));
		rgba8888_store(actual + 1, cases[i].red, cases[i].green, cases[i].blue,
		               cases[i].alpha);
		if (memcmp(actual, expected, sizeof(actual)) != 0) {
			fprintf(stderr, "%s RGBA packing failed\n", cases[i].name);
			failures++;
		}
	}

	if (failures)
		return 1;
	printf("RGBA8888 byte packing tests passed\n");
	return 0;
}
