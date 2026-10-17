#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "d1_custom_validation.h"

static void put16(unsigned char *data, size_t offset, int value)
{
	data[offset] = (unsigned char) value;
	data[offset + 1] = (unsigned char) ((unsigned) value >> 8);
}

static void put32(unsigned char *data, size_t offset, int value)
{
	data[offset] = (unsigned char) value;
	data[offset + 1] = (unsigned char) ((unsigned) value >> 8);
	data[offset + 2] = (unsigned char) ((unsigned) value >> 16);
	data[offset + 3] = (unsigned char) ((unsigned) value >> 24);
}

static void expect(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		exit(1);
	}
}

static d1_custom_validation_limits limits(void)
{
	d1_custom_validation_limits value = { 1800, 256, 30, 600, 300, 210, 10, 8, 32 };
	return value;
}

int main(void)
{
	d1_custom_validation_limits validation_limits = limits();
	unsigned char pog[36] = { 0 };
	unsigned char hx1[800] = { 0 };
	size_t position;

	put32(pog, 0, 0x474f5044);
	put32(pog, 4, 1);
	put32(pog, 8, 1);
	put16(pog, 12, 1);
	memcpy(pog + 14, "fixture", 7);
	pog[23] = 2;
	pog[24] = 2;
	put32(pog, 28, 0);
	pog[32] = 1;
	pog[33] = 2;
	pog[34] = 3;
	pog[35] = 4;
	expect(d1_custom_validate_pigpog(pog, sizeof(pog), &validation_limits, NULL, NULL, NULL),
	       "bounded raw DPOG is accepted");
	put32(pog, 8, INT_MAX);
	expect(!d1_custom_validate_pigpog(pog, sizeof(pog), &validation_limits, NULL, NULL, NULL),
	       "oversized DPOG count is rejected");
	put32(pog, 8, 1);
	put16(pog, 12, 1800);
	expect(!d1_custom_validate_pigpog(pog, sizeof(pog), &validation_limits, NULL, NULL, NULL),
	       "out-of-range replacement index is rejected");
	put16(pog, 12, 1);
	put32(pog, 28, -1);
	expect(!d1_custom_validate_pigpog(pog, sizeof(pog), &validation_limits, NULL, NULL, NULL),
	       "negative bitmap offset is rejected");
	put32(pog, 28, 0);
	expect(!d1_custom_validate_pigpog(pog, sizeof(pog) - 1, &validation_limits, NULL, NULL, NULL),
	       "truncated bitmap data is rejected");

	put32(hx1, 0, 0x21584d48);
	put32(hx1, 4, 1);
	position = 8;
	put32(hx1, position, 0);
	position += 4;
	put32(hx1, position, 0);
	position += 4;
	put32(hx1, position, 1);
	position += 4;
	put32(hx1, position, 0);
	position += 4;
	put32(hx1, position, 1);
	put32(hx1, position + 4, 2);
	put32(hx1, position + 12, 0);
	position += 734;
	put16(hx1, position, 0);
	position += 2;
	put32(hx1, position, -1);
	position += 4;
	put32(hx1, position, -1);
	position += 4;
	put32(hx1, position, 0);
	position += 4;
	expect(d1_custom_validate_hx1(hx1, position, &validation_limits), "bounded HX1 model is accepted");
	expect(!d1_custom_validate_hx1(hx1, position - 1, &validation_limits), "truncated HX1 is rejected");
	put32(hx1, 16, -1);
	expect(!d1_custom_validate_hx1(hx1, position, &validation_limits), "negative HX1 count is rejected");
	put32(hx1, 16, 1);
	put32(hx1, 20, 300);
	expect(!d1_custom_validate_hx1(hx1, position, &validation_limits), "out-of-range HX1 model index is rejected");

	puts("PASS: bounded D1 custom data validation");
	return 0;
}
