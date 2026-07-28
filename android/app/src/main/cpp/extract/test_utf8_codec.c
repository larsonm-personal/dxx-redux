#include <stdio.h>
#include <string.h>

#include "../shared/utf8_codec.h"

static int failures;

#define CHECK(condition)                                                 \
	do {                                                                 \
		if (!(condition)) {                                              \
			fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
			failures++;                                                  \
		}                                                                \
	} while (0)

static void test_round_trip(void)
{
	static const uint16_t input[] = { 'A', 0x00E9u, 0xD83Du, 0xDE00u };
	static const unsigned char expected[] = {
		0x41u, 0xC3u, 0xA9u, 0xF0u, 0x9Fu, 0x98u, 0x80u
	};
	char utf8[16];
	uint16_t output[8];
	size_t bytes = 0, units = 0;
	CHECK(dxx_utf16_to_utf8(input, 4, utf8, sizeof(utf8), &bytes, 1));
	CHECK(bytes == sizeof(expected));
	CHECK(!memcmp(utf8, expected, sizeof(expected)));
	CHECK(dxx_utf8_to_utf16(utf8, bytes, output, 8, &units));
	CHECK(units == 4);
	CHECK(!memcmp(input, output, sizeof(input)));
}

static void test_embedded_null_policy(void)
{
	static const uint16_t input[] = { 'a', 0, 'b' };
	static const char utf8[] = { 'a', 0, 'b' };
	char output[8];
	uint16_t units[4];
	size_t written = 0;
	CHECK(!dxx_utf16_to_utf8(input, 3, output, sizeof(output), &written, 1));
	CHECK(dxx_utf16_to_utf8(input, 3, output, sizeof(output), &written, 0));
	CHECK(written == 3 && !memcmp(output, utf8, 3));
	CHECK(dxx_utf8_to_utf16(utf8, 3, units, 4, &written));
	CHECK(written == 3 && !memcmp(units, input, sizeof(input)));
}

static void test_invalid_utf16(void)
{
	static const uint16_t high[] = { 0xD800u };
	static const uint16_t low[] = { 0xDC00u };
	static const uint16_t pair_bad[] = { 0xD800u, 'x' };
	char output[8];
	size_t written;
	CHECK(!dxx_utf16_to_utf8(high, 1, output, sizeof(output), &written, 1));
	CHECK(!dxx_utf16_to_utf8(low, 1, output, sizeof(output), &written, 1));
	CHECK(!dxx_utf16_to_utf8(pair_bad, 2, output, sizeof(output), &written, 1));
}

static void test_invalid_utf8(void)
{
	static const unsigned char *cases[] = {
		(const unsigned char *) "\x80",
		(const unsigned char *) "\xC0\x80",
		(const unsigned char *) "\xE2\x82",
		(const unsigned char *) "\xED\xA0\x80",
		(const unsigned char *) "\xF4\x90\x80\x80",
		(const unsigned char *) "\xF0\x80\x80\x80"
	};
	static const size_t sizes[] = { 1, 2, 2, 3, 4, 4 };
	uint16_t output[8];
	size_t written;
	size_t i;
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
		CHECK(!dxx_utf8_to_utf16((const char *) cases[i], sizes[i],
		                         output, 8, &written));
}

static void test_capacity_failures(void)
{
	static const uint16_t input[] = { 0xD83Du, 0xDE00u };
	static const char utf8[] = "\xF0\x9F\x98\x80";
	char bytes[3];
	uint16_t units[1];
	size_t written;
	CHECK(!dxx_utf16_to_utf8(input, 2, bytes, sizeof(bytes), &written, 1));
	CHECK(!dxx_utf8_to_utf16(utf8, 4, units, 1, &written));
}

int main(void)
{
	test_round_trip();
	test_embedded_null_policy();
	test_invalid_utf16();
	test_invalid_utf8();
	test_capacity_failures();
	if (failures) return 1;
	puts("UTF-8 codec tests passed");
	return 0;
}
