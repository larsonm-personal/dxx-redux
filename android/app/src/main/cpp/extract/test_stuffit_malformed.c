#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "stuffit_extract.h"

#define TEST_ARCHIVE_SIZE 100u

static void put_be16(unsigned char *p, unsigned int value)
{
	p[0] = (unsigned char) (value >> 8);
	p[1] = (unsigned char) value;
}

static void put_be32(unsigned char *p, unsigned int value)
{
	p[0] = (unsigned char) (value >> 24);
	p[1] = (unsigned char) (value >> 16);
	p[2] = (unsigned char) (value >> 8);
	p[3] = (unsigned char) value;
}

static void make_sit5_header(unsigned char *archive, unsigned int first_offset)
{
	static const unsigned char signature[] =
	    "StuffIt (c)1997-\377\377\377\377 Aladdin Systems, Inc., "
	    "http://www.aladdinsys.com/StuffIt/\r\n";
	unsigned int i;

	memset(archive, 0, TEST_ARCHIVE_SIZE);
	for (i = 0; i < sizeof(signature) - 1u; i++) {
		if (signature[i] != 0xffu)
			archive[i] = signature[i];
	}
	archive[i + 2u] = 5u;
	put_be16(archive + 92u, 1u);
	put_be32(archive + 94u, first_offset);
}

static unsigned char *allocate_guarded_archive(void **allocation, size_t *allocation_size)
{
#ifdef _WIN32
	SYSTEM_INFO system_info;
	unsigned char *region;
	DWORD old_protection;

	GetSystemInfo(&system_info);
	*allocation_size = (size_t) system_info.dwPageSize * 2u;
	region = (unsigned char *) VirtualAlloc(NULL, *allocation_size,
	                                        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!region || !VirtualProtect(region + system_info.dwPageSize,
	                               system_info.dwPageSize, PAGE_NOACCESS,
	                               &old_protection)) {
		if (region)
			VirtualFree(region, 0, MEM_RELEASE);
		return NULL;
	}
	*allocation = region;
	return region + system_info.dwPageSize - TEST_ARCHIVE_SIZE;
#elif defined(MAP_ANONYMOUS) || defined(MAP_ANON)
	long page_size = sysconf(_SC_PAGESIZE);
	unsigned char *region;
	int flags = MAP_PRIVATE;

#ifdef MAP_ANONYMOUS
	flags |= MAP_ANONYMOUS;
#else
	flags |= MAP_ANON;
#endif
	if (page_size <= 0)
		return NULL;
	*allocation_size = (size_t) page_size * 2u;
	region = (unsigned char *) mmap(NULL, *allocation_size, PROT_READ | PROT_WRITE,
	                                flags, -1, 0);
	if (region == MAP_FAILED ||
	    mprotect(region + page_size, (size_t) page_size, PROT_NONE) != 0) {
		if (region != MAP_FAILED)
			munmap(region, *allocation_size);
		return NULL;
	}
	*allocation = region;
	return region + page_size - TEST_ARCHIVE_SIZE;
#else
	*allocation_size = TEST_ARCHIVE_SIZE;
	*allocation = malloc(*allocation_size);
	return (unsigned char *) *allocation;
#endif
}

static void free_guarded_archive(void *allocation, size_t allocation_size)
{
#ifdef _WIN32
	(void) allocation_size;
	VirtualFree(allocation, 0, MEM_RELEASE);
#elif defined(MAP_ANONYMOUS) || defined(MAP_ANON)
	munmap(allocation, allocation_size);
#else
	(void) allocation_size;
	free(allocation);
#endif
}

int main(void)
{
	void *allocation = NULL;
	size_t allocation_size = 0;
	unsigned char *archive = allocate_guarded_archive(&allocation, &allocation_size);
	unsigned int first_offset;
	int failures = 0;

	if (!archive) {
		fprintf(stderr, "failed to allocate guarded SIT5 fixture\n");
		return 1;
	}
	for (first_offset = TEST_ARCHIVE_SIZE - 48u;
	     first_offset < TEST_ARCHIVE_SIZE; first_offset++) {
		make_sit5_header(archive, first_offset);
		if (stuffit_test_sit5_list_entries(archive, TEST_ARCHIVE_SIZE) != -1) {
			fprintf(stderr, "accepted truncated SIT5 header at offset %u\n", first_offset);
			failures++;
		}
	}
	free_guarded_archive(allocation, allocation_size);
	if (failures != 0)
		return 1;
	printf("SIT5 malformed header tests passed\n");
	return 0;
}
