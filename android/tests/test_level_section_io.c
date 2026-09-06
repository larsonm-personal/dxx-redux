#include <stdio.h>
#include "level_section_io.h"

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "CHECK failed: %s line %d\n", #condition, __LINE__); return 1; } } while (0)

static int warnings;
void Warning(char *format, ...)
{
	(void)format;
	++warnings;
}

int main(int argc, char **argv)
{
	const unsigned char bytes[12] = {0, 0, 0x17, 0, 0, 0, 0, 0, 0x42, 0, 0, 0};
	const char *name = "test_level_section_io.bin";
	unsigned char value;
	PHYSFS_file *file;
	(void)argc;
	CHECK(PHYSFS_init(argv[0]));
	CHECK(PHYSFS_setWriteDir("."));
	CHECK(PHYSFS_addToSearchPath(".", 0));
	file = PHYSFS_openWrite(name);
	CHECK(file);
	CHECK(PHYSFS_write(file, bytes, 1, sizeof(bytes)) == sizeof(bytes));
	CHECK(PHYSFS_close(file));
	file = PHYSFS_openRead(name);
	CHECK(file);
	CHECK(level_section_seek(file, 8, 1, "walls"));
	CHECK(PHYSFS_read(file, &value, 1, 1) == 1 && value == 0x42);
	CHECK(level_section_seek(file, 2, 1, "triggers"));
	CHECK(PHYSFS_read(file, &value, 1, 1) == 1 && value == 0x17);
	CHECK(level_section_seek(file, -1, 0, "empty"));
	CHECK(PHYSFS_tell(file) == 3);
	CHECK(!level_section_seek(file, -1, 1, "negative"));
	CHECK(!level_section_seek(file, 12, 1, "eof"));
	CHECK(!level_section_seek(file, 100, 1, "past eof"));
	CHECK(!level_section_seek(file, 0, -1, "negative count"));
	CHECK(warnings == 4);
	CHECK(PHYSFS_close(file));
	CHECK(PHYSFS_delete(name));
	CHECK(PHYSFS_deinit());
	return 0;
}
