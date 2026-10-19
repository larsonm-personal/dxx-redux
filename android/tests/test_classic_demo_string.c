#include <stdio.h>
#include <string.h>

#include "classic_demo_string.h"

typedef struct test_reader {
	const unsigned char *data;
	size_t size;
	size_t position;
} test_reader;

static int test_read(void *context, void *buffer, size_t size)
{
	test_reader *reader = context;
	size_t available = reader->size - reader->position;
	size_t count = size < available ? size : available;

	if (count)
		memcpy(buffer, reader->data + reader->position, count);
	reader->position += count;
	return count == size;
}

static int check(int condition, const char *message)
{
	if (condition)
		return 1;
	fprintf(stderr, "classic demo string test failed: %s\n", message);
	return 0;
}

int main(void)
{
	static const unsigned char valid[] = { 4, 'a', 'b', 'c', 0 };
	static const unsigned char zero_length[] = { 0, 'x' };
	static const unsigned char too_long[] = { 6, 'a', 'b', 'c', 'd', 'e', 0, 'z' };
	static const unsigned char high_bit_length[] = { 255 };
	static const unsigned char unterminated[] = { 3, 'a', 'b', 'c', 'z' };
	static const unsigned char truncated_payload[] = { 4, 'a', 'b' };
	unsigned char high_bit_payload[sizeof(high_bit_length) + 255];
	char destination[4] = "old";
	unsigned int encoded_length = 99;
	test_reader reader;
	classic_demo_string_status status;

	reader = (test_reader) { valid, sizeof(valid), 0 };
	status = classic_demo_read_counted_string(test_read, &reader,
	                                          destination, sizeof(destination), &encoded_length);
	if (!check(status == CLASSIC_DEMO_STRING_OK, "valid string rejected") ||
	    !check(!strcmp(destination, "abc"), "valid string changed") ||
	    !check(reader.position == sizeof(valid), "valid string did not consume its full record") ||
	    !check(encoded_length == 4, "valid encoded length changed"))
		return 1;

	memcpy(destination, "old", sizeof(destination));
	reader = (test_reader) { zero_length, sizeof(zero_length), 0 };
	status = classic_demo_read_counted_string(test_read, &reader,
	                                          destination, sizeof(destination), &encoded_length);
	if (!check(status == CLASSIC_DEMO_STRING_INVALID_LENGTH, "zero length accepted") ||
	    !check(!strcmp(destination, "old"), "zero length mutated destination") ||
	    !check(reader.position == 1, "zero length consumed payload bytes"))
		return 1;

	reader = (test_reader) { too_long, sizeof(too_long), 0 };
	status = classic_demo_read_counted_string(test_read, &reader,
	                                          destination, sizeof(destination), &encoded_length);
	if (!check(status == CLASSIC_DEMO_STRING_TOO_LONG, "oversized string accepted") ||
	    !check(!strcmp(destination, "old"), "oversized string mutated destination") ||
	    !check(reader.position == 7, "oversized string did not consume its full record") ||
	    !check(encoded_length == 6, "oversized encoded length changed"))
		return 1;

	memset(high_bit_payload, 'x', sizeof(high_bit_payload));
	high_bit_payload[0] = high_bit_length[0];
	high_bit_payload[sizeof(high_bit_payload) - 1] = 0;
	reader = (test_reader) { high_bit_payload, sizeof(high_bit_payload), 0 };
	status = classic_demo_read_counted_string(test_read, &reader,
	                                          destination, sizeof(destination), &encoded_length);
	if (!check(status == CLASSIC_DEMO_STRING_TOO_LONG, "high-bit length accepted") ||
	    !check(reader.position == sizeof(high_bit_payload), "high-bit length was not consumed as unsigned") ||
	    !check(encoded_length == 255, "high-bit encoded length changed"))
		return 1;

	reader = (test_reader) { unterminated, sizeof(unterminated), 0 };
	status = classic_demo_read_counted_string(test_read, &reader,
	                                          destination, sizeof(destination), &encoded_length);
	if (!check(status == CLASSIC_DEMO_STRING_UNTERMINATED, "unterminated string accepted") ||
	    !check(!strcmp(destination, "old"), "unterminated string mutated destination") ||
	    !check(reader.position == 4, "unterminated string did not consume its full record"))
		return 1;

	reader = (test_reader) { NULL, 0, 0 };
	status = classic_demo_read_counted_string(test_read, &reader,
	                                          destination, sizeof(destination), &encoded_length);
	if (!check(status == CLASSIC_DEMO_STRING_TRUNCATED, "missing length accepted") ||
	    !check(reader.position == 0, "missing length changed reader position"))
		return 1;

	reader = (test_reader) { truncated_payload, sizeof(truncated_payload), 0 };
	status = classic_demo_read_counted_string(test_read, &reader,
	                                          destination, sizeof(destination), &encoded_length);
	if (!check(status == CLASSIC_DEMO_STRING_TRUNCATED, "truncated payload accepted") ||
	    !check(!strcmp(destination, "old"), "truncated payload mutated destination") ||
	    !check(reader.position == sizeof(truncated_payload), "truncated payload did not consume available bytes"))
		return 1;

	puts("classic demo string tests passed");
	return 0;
}
