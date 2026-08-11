#include "bounded_music_read.h"

#include <stdio.h>
#include <string.h>

typedef struct test_reader {
	const unsigned char *source;
	size_t size;
	size_t offset;
	size_t maximum_chunk;
	int fail_after_first;
	int overreport;
} test_reader;

static int64_t test_read(void *context, unsigned char *destination, size_t maximum)
{
	test_reader *reader = (test_reader *) context;
	size_t amount;

	if (reader->overreport)
		return (int64_t) maximum + 1;
	if (reader->fail_after_first && reader->offset)
		return -1;
	if (reader->offset >= reader->size)
		return 0;
	amount = reader->size - reader->offset;
	if (amount > maximum)
		amount = maximum;
	if (amount > reader->maximum_chunk)
		amount = reader->maximum_chunk;
	memcpy(destination, reader->source + reader->offset, amount);
	reader->offset += amount;
	return (int64_t) amount;
}

static int fail(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

int main(void)
{
	static const unsigned char source[] = { 1, 2, 3, 4, 5 };
	unsigned char destination[sizeof(source)] = { 0 };
	test_reader reader = { source, sizeof(source), 0, 2, 0, 0 };

	if (!bounded_music_read_exact(test_read, &reader, destination, sizeof(destination)) ||
	    memcmp(source, destination, sizeof(source)))
		return fail("repeated short reads did not complete exactly");
	reader = (test_reader) { source, 2, 0, 2, 0, 0 };
	if (bounded_music_read_exact(test_read, &reader, destination, sizeof(destination)))
		return fail("early EOF was accepted");
	reader = (test_reader) { source, sizeof(source), 0, 2, 1, 0 };
	if (bounded_music_read_exact(test_read, &reader, destination, sizeof(destination)))
		return fail("read error was accepted");
	reader = (test_reader) { source, sizeof(source), 0, 2, 0, 1 };
	if (bounded_music_read_exact(test_read, &reader, destination, sizeof(destination)))
		return fail("reader over-report was accepted");
	puts("PASS");
	return 0;
}
