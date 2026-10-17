#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "d2_ham_validation.h"

static void put32(unsigned char *data, size_t offset, int value)
{
	data[offset] = (unsigned char) value;
	data[offset + 1] = (unsigned char) ((unsigned) value >> 8);
	data[offset + 2] = (unsigned char) ((unsigned) value >> 16);
	data[offset + 3] = (unsigned char) ((unsigned) value >> 24);
}

static void append32(unsigned char *data, size_t *position, int value)
{
	put32(data, *position, value);
	*position += 4;
}

static void expect(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		exit(1);
	}
}

static d2_ham_validation_limits limits(void)
{
	d2_ham_validation_limits value = {
		1200, 254, 110, 110, 60, 85, 1600, 70, 50,
		200, 100, 610, 6, 7, 1800, 10
	};
	return value;
}

int main(void)
{
	d2_ham_validation_limits validation_limits = limits();
	unsigned char main_ham[256] = { 0 };
	unsigned char extra_ham[64] = { 0 };
	unsigned char hxm[64] = { 0 };
	size_t position = 0, count_offset;
	int i;

	append32(main_ham, &position, 0x214d4148);
	append32(main_ham, &position, 3);
	count_offset = position;
	for (i = 0; i < 12; ++i) append32(main_ham, &position, 0);
	position += 132;
	for (i = 0; i < 4; ++i) append32(main_ham, &position, 0);
	expect(d2_ham_validate_main(main_ham, position, &validation_limits),
	       "bounded empty main HAM is accepted");
	put32(main_ham, count_offset, INT_MAX);
	expect(!d2_ham_validate_main(main_ham, position, &validation_limits),
	       "oversized main HAM table is rejected");
	put32(main_ham, count_offset, 0);
	expect(!d2_ham_validate_main(main_ham, position - 1, &validation_limits),
	       "truncated main HAM is rejected");

	position = 0;
	append32(extra_ham, &position, 0x5848414d);
	append32(extra_ham, &position, 1);
	for (i = 0; i < 6; ++i) append32(extra_ham, &position, 0);
	expect(d2_ham_validate_extra(extra_ham, position, 2, &validation_limits,
	                             62, 66, 1145, 166, 422, 502),
	       "bounded empty extra HAM is accepted");
	put32(extra_ham, position - 4, 2);
	extra_ham[position++] = 0;
	extra_ham[position++] = 0;
	extra_ham[position++] = 1;
	extra_ham[position++] = 0;
	expect(d2_ham_validate_extra(extra_ham, position, 2, &validation_limits,
	                             62, 66, 1145, 166, 422, 502),
	       "bounded extra HAM pointer table is accepted");
	extra_ham[position++] = 0;
	extra_ham[position++] = 0;
	extra_ham[position++] = 1;
	extra_ham[position++] = 0;
	expect(d2_ham_validate_extra(extra_ham, position, 2, &validation_limits,
	                             62, 66, 1145, 166, 422, 502),
	       "legacy duplicate extra HAM pointer table is accepted");
	expect(!d2_ham_validate_extra(extra_ham, position - 1, 2, &validation_limits,
	                              62, 66, 1145, 166, 422, 502),
	       "truncated legacy extra HAM pointer table is rejected");
	extra_ham[position - 2] = 0xff;
	extra_ham[position - 1] = 0xff;
	expect(!d2_ham_validate_extra(extra_ham, position, 2, &validation_limits,
	                              62, 66, 1145, 166, 422, 502),
	       "out-of-range legacy extra HAM pointer is rejected");
	position = 32;
	put32(extra_ham, 8, -1);
	expect(!d2_ham_validate_extra(extra_ham, position, 2, &validation_limits,
	                              62, 66, 1145, 166, 422, 502),
	       "negative extra HAM count is rejected");

	position = 0;
	append32(hxm, &position, 0x21584d48);
	append32(hxm, &position, 1);
	for (i = 0; i < 5; ++i) append32(hxm, &position, 0);
	expect(d2_ham_validate_hxm(hxm, position, &validation_limits, 66, 1145, 166),
	       "bounded empty HXM is accepted");
	append32(hxm, &position, 0);
	expect(d2_ham_validate_hxm(hxm, position, &validation_limits, 66, 1145, 166),
	       "legacy trailing HXM zero word is accepted");
	append32(hxm, &position, 0);
	expect(!d2_ham_validate_hxm(hxm, position, &validation_limits, 66, 1145, 166),
	       "multiple trailing HXM words are rejected");
	position -= 4;
	put32(hxm, position - 4, 1);
	expect(!d2_ham_validate_hxm(hxm, position, &validation_limits, 66, 1145, 166),
	       "nonzero trailing HXM word is rejected");
	put32(hxm, position - 4, 0);
	put32(hxm, 8, INT_MAX);
	expect(!d2_ham_validate_hxm(hxm, position, &validation_limits, 66, 1145, 166),
	       "oversized HXM count is rejected");
	put32(hxm, 8, 0);
	put32(hxm, 4, 0);
	expect(!d2_ham_validate_hxm(hxm, position, &validation_limits, 66, 1145, 166),
	       "obsolete HXM version is rejected recoverably");

	puts("PASS: bounded D2 HAM/HXM validation");
	return 0;
}
