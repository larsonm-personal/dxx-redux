#include <stdio.h>
#include "level_section_io.h"

#define CHECK(condition)                                                         \
	do {                                                                         \
		if (!(condition)) {                                                      \
			fprintf(stderr, "CHECK failed: %s line %d\n", #condition, __LINE__); \
			return 1;                                                            \
		}                                                                        \
	} while (0)

static int warnings;
void Warning(char *format, ...)
{
	(void) format;
	++warnings;
}

int main(int argc, char **argv)
{
	const unsigned char bytes[12] = { 0, 0, 0x17, 0, 0, 0, 0, 0, 0x42, 0, 0, 0 };
	const short valid_children[6] = { -1, -2, 0, 1, -1, -1 };
	const short invalid_children[6] = { -1, -3, 0, 1, -1, -1 };
	const int valid_vertices[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
	const int invalid_vertices[8] = { 0, 1, 2, 3, 4, 5, 6, 8 };
	const short valid_segments[2] = { 0, 1 };
	const short valid_sides[2] = { 0, 5 };
	const short invalid_sides[2] = { 0, 6 };
	const char *name = "test_level_section_io.bin";
	unsigned char value;
	PHYSFS_file *file;
	(void) argc;
	CHECK(PHYSFS_init(argv[0]));
	CHECK(PHYSFS_setWriteDir("."));
	CHECK(PHYSFS_addToSearchPath(".", 0));
	file = PHYSFS_openWrite(name);
	CHECK(file);
	CHECK(PHYSFS_write(file, bytes, 1, sizeof(bytes)) == sizeof(bytes));
	CHECK(PHYSFS_close(file));
	file = PHYSFS_openRead(name);
	CHECK(file);
	CHECK(level_count_is_valid(1, 1, 2));
	CHECK(!level_count_is_valid(0, 1, 2));
	CHECK(!level_count_is_valid(3, 1, 2));
	CHECK(!level_count_is_valid(1, 2, 1));
	CHECK(level_segment_references_are_valid(valid_children, 6, valid_vertices, 8, 2, 8));
	CHECK(!level_segment_references_are_valid(invalid_children, 6, valid_vertices, 8, 2, 8));
	CHECK(!level_segment_references_are_valid(valid_children, 6, invalid_vertices, 8, 2, 8));
	CHECK(level_links_are_valid(2, 2, valid_segments, valid_sides, 2, 6));
	CHECK(!level_links_are_valid(3, 2, valid_segments, valid_sides, 2, 6));
	CHECK(!level_links_are_valid(2, 2, valid_segments, invalid_sides, 2, 6));
	CHECK(level_section_seek(file, 8, 1, 1, "walls"));
	CHECK(PHYSFS_read(file, &value, 1, 1) == 1 && value == 0x42);
	CHECK(level_section_seek(file, 2, 1, 1, "triggers"));
	CHECK(PHYSFS_read(file, &value, 1, 1) == 1 && value == 0x17);
	CHECK(level_section_seek(file, -1, 0, 1, "empty"));
	CHECK(PHYSFS_tell(file) == 3);
	CHECK(!level_section_seek(file, -1, 1, 1, "negative"));
	CHECK(!level_section_seek(file, 12, 1, 1, "eof"));
	CHECK(!level_section_seek(file, 100, 1, 1, "past eof"));
	CHECK(!level_section_seek(file, 0, -1, 1, "negative count"));
	CHECK(!level_section_seek(file, 0, 2, 1, "oversized count"));
	CHECK(warnings == 5);
	CHECK(PHYSFS_close(file));
	CHECK(PHYSFS_delete(name));
	CHECK(PHYSFS_deinit());
	return 0;
}
