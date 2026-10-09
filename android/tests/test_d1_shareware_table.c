#include <stdio.h>
#include <string.h>
#include "d1_shareware_table.h"

#define CHECK(x)                                                      \
	do {                                                              \
		if (!(x)) {                                                   \
			fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #x); \
			return 1;                                                 \
		}                                                             \
	} while (0)

typedef struct memory_source {
	const unsigned char *bytes;
	size_t size, position;
} memory_source;

static int memory_byte(void *context)
{
	memory_source *source = (memory_source *) context;
	return source->position == source->size ? -1 : source->bytes[source->position++];
}

static int file_byte(void *context)
{
	FILE *file = (FILE *) context;
	int value = fgetc(file);
	return value == EOF ? (ferror(file) ? -2 : -1) : value;
}

static void prepare(d1_shareware_table_reader *reader, memory_source *source,
                    const char *text, int encoded)
{
	source->bytes = (const unsigned char *) text;
	source->size = strlen(text);
	source->position = 0;
	d1_shareware_table_init(reader, memory_byte, source, encoded);
}

int main(int argc, char **argv)
{
	d1_shareware_table_reader reader;
	memory_source source;
	char line[600];
	int result;
	prepare(&reader, &source, "\r\n$ROBOT one.pof ; comment\r@$ROBOT two.pof\nlast", 0);
	CHECK(d1_shareware_table_next(&reader, line, sizeof(line)) == 1 && !line[0]);
	CHECK(d1_shareware_table_next(&reader, line, sizeof(line)) == 1 && !strcmp(line, "$ROBOT one.pof "));
	CHECK(d1_shareware_table_next(&reader, line, sizeof(line)) == 1 && !strcmp(line, "@$ROBOT two.pof"));
	CHECK(d1_shareware_table_next(&reader, line, sizeof(line)) == 1 && !strcmp(line, "last"));
	CHECK(reader.line == 4);
	CHECK(d1_shareware_table_next(&reader, line, sizeof(line)) == 0);
	prepare(&reader, &source, "abc\\\n def\n", 0);
	CHECK(d1_shareware_table_next(&reader, line, sizeof(line)) == 1 && !strcmp(line, "abc  def"));
	CHECK(reader.line == 2);
	prepare(&reader, &source, "abc\\", 0);
	CHECK(d1_shareware_table_next(&reader, line, sizeof(line)) == -1 && reader.error);
	prepare(&reader, &source, "abcd\n", 0);
	CHECK(d1_shareware_table_next(&reader, line, 4) == -1 && reader.error);
	prepare(&reader, &source, "abc\n", 0);
	CHECK(d1_shareware_table_next(&reader, line, 4) == 1 && !strcmp(line, "abc"));
	prepare(&reader, &source, "abc\\\ndef\n", 0);
	CHECK(d1_shareware_table_next(&reader, line, 6) == -1 && reader.error);
	prepare(&reader, &source, "a", 0);
	source.size = 2; /* Include the terminating NUL as malformed source input */
	CHECK(d1_shareware_table_next(&reader, line, sizeof(line)) == -1 && reader.error);
	/* Fixed encoded bytes for ABC, independent of a test-side encoder */
	prepare(&reader, &source, "\xb9\x79\x39\n", 1);
	CHECK(d1_shareware_table_next(&reader, line, sizeof(line)) == 1 && !strcmp(line, "ABC"));
	if (argc == 2) {
		FILE *file = fopen(argv[1], "rb");
		unsigned hash = 2166136261u;
		CHECK(file != NULL);
		d1_shareware_table_init(&reader, file_byte, file, 1);
		while ((result = d1_shareware_table_next(&reader, line, sizeof(line))) > 0) {
			const unsigned char *p = (const unsigned char *) line;
			while (*p)
				hash = (hash ^ *p++) * 16777619u;
			hash = (hash ^ '\n') * 16777619u;
		}
		fclose(file);
		CHECK(result == 0);
		printf("Fixture lines=%u normalized_fnv1a=%08x\n", reader.line, hash);
		/* Independently decoded authentic DOS 1.4 and Test Flight BITMAPS.BIN */
		CHECK(reader.line == 806 && hash == 0x346bcd94u);
	}
	puts("PASS: shared D1 table input");
	return 0;
}
