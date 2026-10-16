#ifndef DXX_LEVEL_SECTION_IO_H
#define DXX_LEVEL_SECTION_IO_H

#include <physfs.h>
#include "dxxerror.h"

static inline int level_count_is_valid(int count, int minimum, int maximum)
{
	return minimum >= 0 && maximum >= minimum && count >= minimum && count <= maximum;
}

static inline int level_segment_references_are_valid(const short *children, int child_count,
                                                     const int *vertices, int vertex_count, int segment_count, int loaded_vertex_count)
{
	int i;

	if (!children || !vertices || child_count < 0 || vertex_count < 0 || segment_count < 1 || loaded_vertex_count < 1)
		return 0;
	for (i = 0; i < child_count; ++i)
		if (children[i] < -2 || children[i] >= segment_count)
			return 0;
	for (i = 0; i < vertex_count; ++i)
		if (vertices[i] < 0 || vertices[i] >= loaded_vertex_count)
			return 0;
	return 1;
}

static inline int level_links_are_valid(int count, int maximum, const short *segments,
                                        const short *sides, int segment_count, int side_count)
{
	int i;

	if (!level_count_is_valid(count, 0, maximum) || !segments || !sides || segment_count < 1 || side_count < 1)
		return 0;
	for (i = 0; i < count; ++i)
		if (segments[i] < 0 || segments[i] >= segment_count || sides[i] < 0 || sides[i] >= side_count)
			return 0;
	return 1;
}

/* Serialized sections need not be adjacent, or in decoding order */
static inline int level_section_seek(PHYSFS_file *file, int offset, int count, int maximum, const char *name)
{
	if (!level_count_is_valid(count, 0, maximum)) {
		Warning("Invalid level %s section count=%d maximum=%d", name, count, maximum);
		return 0;
	}
	if (!count)
		return 1;
	if (offset < 0 || offset >= PHYSFS_fileLength(file) || !PHYSFS_seek(file, offset)) {
		Warning("Invalid level %s section offset=%d count=%d", name, offset, count);
		return 0;
	}
	return 1;
}

#endif
