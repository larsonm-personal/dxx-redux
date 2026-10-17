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

static void put16(unsigned char *data, size_t offset, int value)
{
	data[offset] = (unsigned char) value;
	data[offset + 1] = (unsigned char) ((unsigned) value >> 8);
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

static void validate_public_demo_ham(const char *path,
                                     const d2_ham_validation_limits *validation_limits)
{
	FILE *file;
	unsigned char *data;
	long length;
	d2_ham_validation_report report;

	file = fopen(path, "rb");
	expect(file != NULL, "public D2 demo HAM opens");
	expect(!fseek(file, 0, SEEK_END), "public D2 demo HAM length is readable");
	length = ftell(file);
	expect(length == 1961015 && !fseek(file, 0, SEEK_SET),
	       "public D2 demo HAM has the pinned release length");
	data = (unsigned char *) malloc((size_t) length);
	expect(data != NULL && fread(data, 1, (size_t) length, file) == (size_t) length,
	       "public D2 demo HAM bytes are readable");
	fclose(file);
	expect(d2_ham_validate_main(data, (size_t) length, validation_limits, &report),
	       "public D2 demo HAM is accepted");
	expect(report.inert_robot_records == 46,
	       "public D2 demo HAM retains its 46 sparse robot slots");
	free(data);
}

int main(int argc, char **argv)
{
	d2_ham_validation_limits validation_limits = limits();
	d2_ham_validation_report report;
	unsigned char main_ham[256] = { 0 };
	unsigned char sparse_ham[1024] = { 0 };
	unsigned char extra_ham[64] = { 0 };
	unsigned char hxm[64] = { 0 };
	unsigned char joint_hxm[512] = { 0 };
	size_t position = 0, count_offset;
	int i;

	append32(main_ham, &position, 0x214d4148);
	append32(main_ham, &position, 3);
	count_offset = position;
	for (i = 0; i < 12; ++i) append32(main_ham, &position, 0);
	position += 132;
	for (i = 0; i < 4; ++i) append32(main_ham, &position, 0);
	expect(d2_ham_validate_main(main_ham, position, &validation_limits, &report),
	       "bounded empty main HAM is accepted");
	expect(!report.inert_robot_records && !report.normalized_empty_joint_offsets &&
	           !report.ignored_trailing_bytes,
	       "canonical main HAM has no compatibility repairs");
	put32(main_ham, count_offset, INT_MAX);
	expect(!d2_ham_validate_main(main_ham, position, &validation_limits, &report),
	       "oversized main HAM table is rejected");
	put32(main_ham, count_offset, 0);
	expect(!d2_ham_validate_main(main_ham, position - 1, &validation_limits, &report),
	       "truncated main HAM is rejected");

	position = 0;
	append32(sparse_ham, &position, 0x214d4148);
	append32(sparse_ham, &position, 3);
	for (i = 0; i < 5; ++i) append32(sparse_ham, &position, 0);
	append32(sparse_ham, &position, 1);
	put32(sparse_ham, position, -1);
	position += 480;
	for (i = 0; i < 6; ++i) append32(sparse_ham, &position, 0);
	position += 132;
	for (i = 0; i < 4; ++i) append32(sparse_ham, &position, 0);
	expect(d2_ham_validate_main(sparse_ham, position, &validation_limits, &report) &&
	           report.inert_robot_records == 1,
	       "official-demo-style inert robot slot is accepted and reported");
	put32(sparse_ham, 32, -2);
	expect(!d2_ham_validate_main(sparse_ham, position, &validation_limits, &report),
	       "robot slot model below the legacy inert sentinel is rejected");

	position = 0;
	append32(extra_ham, &position, 0x5848414d);
	append32(extra_ham, &position, 1);
	for (i = 0; i < 6; ++i) append32(extra_ham, &position, 0);
	expect(d2_ham_validate_extra(extra_ham, position, 2, &validation_limits,
	                             62, 66, 1145, 166, 422, 502, &report),
	       "bounded empty extra HAM is accepted");
	put32(extra_ham, position - 4, 2);
	extra_ham[position++] = 0;
	extra_ham[position++] = 0;
	extra_ham[position++] = 1;
	extra_ham[position++] = 0;
	expect(d2_ham_validate_extra(extra_ham, position, 2, &validation_limits,
	                             62, 66, 1145, 166, 422, 502, &report),
	       "bounded extra HAM pointer table is accepted");
	extra_ham[position++] = 0;
	extra_ham[position++] = 0;
	extra_ham[position++] = 1;
	extra_ham[position++] = 0;
	expect(d2_ham_validate_extra(extra_ham, position, 2, &validation_limits,
	                             62, 66, 1145, 166, 422, 502, &report),
	       "legacy duplicate extra HAM pointer table is accepted");
	expect(report.ignored_trailing_bytes == 4,
	       "ignored duplicate extra HAM pointer table is reported");
	expect(!d2_ham_validate_extra(extra_ham, position - 1, 2, &validation_limits,
	                              62, 66, 1145, 166, 422, 502, &report),
	       "truncated legacy extra HAM pointer table is rejected");
	extra_ham[position - 2] = 0xff;
	extra_ham[position - 1] = 0xff;
	expect(!d2_ham_validate_extra(extra_ham, position, 2, &validation_limits,
	                              62, 66, 1145, 166, 422, 502, &report),
	       "out-of-range legacy extra HAM pointer is rejected");
	position = 32;
	put32(extra_ham, 8, -1);
	expect(!d2_ham_validate_extra(extra_ham, position, 2, &validation_limits,
	                              62, 66, 1145, 166, 422, 502, &report),
	       "negative extra HAM count is rejected");

	position = 0;
	append32(hxm, &position, 0x21584d48);
	append32(hxm, &position, 1);
	for (i = 0; i < 5; ++i) append32(hxm, &position, 0);
	expect(d2_ham_validate_hxm(hxm, position, &validation_limits, 66, 1145, 166, &report),
	       "bounded empty HXM is accepted");
	append32(hxm, &position, 0);
	expect(d2_ham_validate_hxm(hxm, position, &validation_limits, 66, 1145, 166, &report),
	       "legacy trailing HXM zero word is accepted");
	append32(hxm, &position, 0);
	expect(d2_ham_validate_hxm(hxm, position, &validation_limits, 66, 1145, 166, &report) &&
	           report.ignored_trailing_bytes == 8,
	       "bounded zero-filled HXM suffix is accepted and reported");
	put32(hxm, position - 4, 1);
	expect(!d2_ham_validate_hxm(hxm, position, &validation_limits, 66, 1145, 166, &report),
	       "nonzero trailing HXM word is rejected");
	put32(hxm, position - 4, 0);
	put32(hxm, 8, INT_MAX);
	expect(!d2_ham_validate_hxm(hxm, position, &validation_limits, 66, 1145, 166, &report),
	       "oversized HXM count is rejected");
	put32(hxm, 8, 0);
	put32(hxm, 4, 0);
	expect(!d2_ham_validate_hxm(hxm, position, &validation_limits, 66, 1145, 166, &report),
	       "obsolete HXM version is rejected recoverably");

	position = 0;
	append32(joint_hxm, &position, 0x21584d48);
	append32(joint_hxm, &position, 1);
	append32(joint_hxm, &position, 1);
	append32(joint_hxm, &position, 0);
	position += 480;
	for (i = 0; i < 4; ++i) append32(joint_hxm, &position, 0);
	put16(joint_hxm, 16 + 298, -1);
	expect(d2_ham_validate_hxm(joint_hxm, position, &validation_limits, 66, 1145, 166, &report),
	       "negative offset on an empty robot joint list is ignored");
	expect(report.normalized_empty_joint_offsets == 1,
	       "normalized empty robot joint-list offset is reported");
	put16(joint_hxm, 16 + 296, 1);
	expect(!d2_ham_validate_hxm(joint_hxm, position, &validation_limits, 66, 1145, 166, &report),
	       "negative offset on a nonempty robot joint list is rejected");
	put16(joint_hxm, 16 + 298, 1145);
	expect(!d2_ham_validate_hxm(joint_hxm, position, &validation_limits, 66, 1145, 166, &report),
	       "out-of-range nonempty robot joint list is rejected");
	if (argc == 2)
		validate_public_demo_ham(argv[1], &validation_limits);

	puts("PASS: bounded D2 HAM/HXM validation");
	return 0;
}
