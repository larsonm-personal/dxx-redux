#include <stddef.h>

static void *test_realloc(void *pointer, size_t size);

#define DXX_REWIND_FILE_WRAPPER   1
#define DXX_REWIND_FILE_CORE_ONLY 1
#define REWIND_FILE_REALLOC       test_realloc
#include "rewind_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail_realloc;
static PHYSFS_sint64 g_physfs_write_result;
static int g_physfs_close_result = 1;

static void *test_realloc(void *pointer, size_t size)
{
	return g_fail_realloc ? NULL : realloc(pointer, size);
}

static int fail(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

int main(void)
{
	static const unsigned char first[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
	static const unsigned char second[] = { 9, 10, 11 };
	rewind_memory_buffer buffer = { NULL, 0, 0, 0 };
	rewind_memory_buffer replacement = { NULL, 0, 0, 0 };
	rewind_file file;
	PHYSFS_file *physical_file = (PHYSFS_file *) &file;

	g_physfs_write_result = (PHYSFS_sint64) sizeof(first) - 1;
	rewind_file_init_physfs(&file, physical_file);
	if (rewind_file_write(&file, first, 1, sizeof(first)) != sizeof(first) - 1 ||
	    rewind_file_close(&file))
		return fail("short physical write did not become sticky");
	g_physfs_write_result = (PHYSFS_sint64) sizeof(first);
	g_physfs_close_result = 0;
	rewind_file_init_physfs(&file, physical_file);
	if (rewind_file_write(&file, first, 1, sizeof(first)) != sizeof(first) ||
	    rewind_file_close(&file))
		return fail("physical close failure was not propagated");
	g_physfs_close_result = 1;

	rewind_file_init_memory_write(&file, &buffer);
	if (rewind_file_write(&file, first, 1, sizeof(first)) != sizeof(first) ||
	    !rewind_file_close(&file) || buffer.size != sizeof(first) ||
	    memcmp(buffer.data, first, sizeof(first)))
		return fail("initial memory write failed");

	rewind_file_init_memory_write(&file, &buffer);
	if (rewind_file_write(&file, second, 1, sizeof(second)) != sizeof(second) ||
	    !rewind_file_close(&file) || buffer.size != sizeof(second) ||
	    memcmp(buffer.data, second, sizeof(second)))
		return fail("smaller rewrite retained a stale high-water size");

	g_fail_realloc = 1;
	rewind_file_init_memory_write(&file, &replacement);
	if (rewind_file_write(&file, first, 1, sizeof(first)) != 0 ||
	    rewind_file_close(&file) || !replacement.error)
		return fail("allocation failure did not become sticky");
	rewind_memory_buffer_discard(&replacement);
	if (buffer.size != sizeof(second) || memcmp(buffer.data, second, sizeof(second)))
		return fail("failed staging changed the published buffer");

	g_fail_realloc = 0;
	rewind_file_init_memory_write(&file, &replacement);
	if (rewind_file_write(&file, first, 1, sizeof(first)) != sizeof(first) ||
	    !rewind_file_close(&file))
		return fail("replacement staging write failed");
	rewind_memory_buffer_replace(&buffer, &replacement);
	if (buffer.size != sizeof(first) || memcmp(buffer.data, first, sizeof(first)) ||
	    replacement.data || replacement.size || replacement.capacity || replacement.error)
		return fail("successful staging replacement was not atomic");

	rewind_memory_buffer_discard(&buffer);
	puts("PASS");
	return 0;
}

PHYSFS_sint64 PHYSFS_readBytes(PHYSFS_file *file, void *buffer, PHYSFS_uint64 length)
{
	(void) file;
	(void) buffer;
	(void) length;
	return -1;
}

PHYSFS_sint64 PHYSFS_writeBytes(PHYSFS_file *file, const void *buffer, PHYSFS_uint64 length)
{
	(void) file;
	(void) buffer;
	(void) length;
	return g_physfs_write_result;
}

int PHYSFS_close(PHYSFS_file *file)
{
	(void) file;
	return g_physfs_close_result;
}

int PHYSFS_seek(PHYSFS_file *file, PHYSFS_uint64 position)
{
	(void) file;
	(void) position;
	return 0;
}

PHYSFS_sint64 PHYSFS_tell(PHYSFS_file *file)
{
	(void) file;
	return -1;
}

PHYSFS_sint64 PHYSFS_fileLength(PHYSFS_file *file)
{
	(void) file;
	return -1;
}

int PHYSFS_eof(PHYSFS_file *file)
{
	(void) file;
	return 1;
}
