#ifndef DXX_LEVEL_SECTION_IO_H
#define DXX_LEVEL_SECTION_IO_H

#include <physfs.h>
#include "dxxerror.h"

static inline int level_count_is_valid(int count, int minimum, int maximum)
{
	return minimum >= 0 && maximum >= minimum && count >= minimum && count <= maximum;
}

static inline int level_index_is_valid(int index, int count)
{
	return index >= 0 && index < count;
}

static inline int level_optional_index_is_valid(int index, int count)
{
	return index == -1 || level_index_is_valid(index, count);
}

static inline int level_index_range_is_valid(int index, int count, int total)
{
	return index >= 0 && count >= 0 && index <= total && count <= total - index;
}

static inline int level_normalize_legacy_signed_byte_count(int count)
{
	/* Original consumers used signed loop bounds, so negative serialized counts were empty */
	return count < 0 ? 0 : count;
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

typedef struct level_link_repair_report {
	int count;
	unsigned normalized_count;
	unsigned ignored_invalid_links;
} level_link_repair_report;

static inline level_link_repair_report level_repair_legacy_links(int count, int maximum,
                                                                 short *segments, short *sides,
                                                                 int segment_count, int side_count)
{
	level_link_repair_report report = { 0, 0, 0 };
	int readable_count;
	int write_index = 0;
	int i;

	if (!segments || !sides || maximum < 0 || segment_count < 1 || side_count < 1) {
		report.normalized_count = 1;
		return report;
	}
	if (count < 0) {
		report.normalized_count = 1;
		return report;
	}
	readable_count = count;
	if (readable_count > maximum) {
		readable_count = maximum;
		report.normalized_count = 1;
	}
	for (i = 0; i < readable_count; ++i) {
		if (segments[i] < 0 || segments[i] >= segment_count ||
		    sides[i] < 0 || sides[i] >= side_count) {
			++report.ignored_invalid_links;
			continue;
		}
		segments[write_index] = segments[i];
		sides[write_index] = sides[i];
		++write_index;
	}
	report.count = write_index;
	return report;
}

enum {
	LEVEL_COMPILED_VERTEX_SERIALIZED_SIZE = 12,
	LEVEL_OBJECT_MINIMUM_SERIALIZED_SIZE = 79,
	LEVEL_WALL_V16_SERIALIZED_SIZE = 9,
	LEVEL_WALL_V19_SERIALIZED_SIZE = 21,
	LEVEL_WALL_SERIALIZED_SIZE = 24,
	LEVEL_TRIGGER_V29_SERIALIZED_SIZE = 54,
	LEVEL_TRIGGER_SERIALIZED_SIZE = 52,
	LEVEL_REACTOR_TRIGGER_SERIALIZED_SIZE = 42,
	LEVEL_MATCEN_V25_SERIALIZED_SIZE = 16,
	LEVEL_MATCEN_SERIALIZED_SIZE = 20,
	LEVEL_DL_INDEX_SERIALIZED_SIZE = 6,
	LEVEL_DELTA_LIGHT_SERIALIZED_SIZE = 8,
	LEVEL_FLICKERING_LIGHT_SERIALIZED_SIZE = 16
};

static inline int level_envelope_is_valid(PHYSFS_file *file, int signature,
                                          int expected_signature, int version,
                                          int minimum_version, int maximum_version,
                                          int mine_offset, int game_offset)
{
	PHYSFS_sint64 length;

	if (!file || signature != expected_signature || version < minimum_version ||
	    version > maximum_version || mine_offset < 0 || game_offset < 0)
		return 0;
	length = PHYSFS_fileLength(file);
	return length > 0 && mine_offset < length && game_offset < length;
}

static inline PHYSFS_uint64 level_compiled_mine_minimum_segment_size(int new_file_format, int gamesave_version)
{
	PHYSFS_uint64 size = 8 * 2; /* vertex indices */

	if (new_file_format)
		size += 2; /* child and wall masks */
	else {
		size += (6 * 2) + 6; /* children and wall numbers */
		if (gamesave_version == 5 || gamesave_version <= 1)
			size += 4; /* special, matcen, and value */
	}
	if (gamesave_version <= 5)
		size += 2; /* compressed static light */
	else
		size += 8; /* trailing segment2 record */
	return size;
}

static inline int level_minimum_span_is_valid(PHYSFS_file *file,
                                              int first_count, PHYSFS_uint64 first_size,
                                              int second_count, PHYSFS_uint64 second_size,
                                              const char *name)
{
	const PHYSFS_uint64 maximum = (PHYSFS_uint64) -1;
	PHYSFS_uint64 required;
	PHYSFS_uint64 second_required;
	PHYSFS_sint64 position;
	PHYSFS_sint64 length;

	if (!file || first_count < 0 || second_count < 0 ||
	    (first_size && (PHYSFS_uint64) first_count > maximum / first_size) ||
	    (second_size && (PHYSFS_uint64) second_count > maximum / second_size)) {
		Warning("Invalid level %s minimum span", name);
		return 0;
	}
	required = (PHYSFS_uint64) first_count * first_size;
	second_required = (PHYSFS_uint64) second_count * second_size;
	if (second_required > maximum - required) {
		Warning("Invalid level %s minimum span", name);
		return 0;
	}
	required += second_required;
	position = PHYSFS_tell(file);
	length = PHYSFS_fileLength(file);
	if (position < 0 || length < position || required > (PHYSFS_uint64) (length - position)) {
		Warning("Truncated level %s minimum span=%llu remaining=%lld", name,
		        (unsigned long long) required,
		        length >= position && position >= 0 ? (long long) (length - position) : -1LL);
		return 0;
	}
	return 1;
}

/* Serialized sections need not be adjacent, or in decoding order */
static inline int level_section_seek(PHYSFS_file *file, int offset, int count, int maximum,
                                     PHYSFS_uint64 record_size, const char *name)
{
	const PHYSFS_uint64 maximum_size = (PHYSFS_uint64) -1;
	PHYSFS_uint64 required;
	PHYSFS_sint64 length;

	if (!level_count_is_valid(count, 0, maximum)) {
		Warning("Invalid level %s section count=%d maximum=%d", name, count, maximum);
		return 0;
	}
	if (!count)
		return 1;
	if (!file || offset < 0 || record_size == 0 ||
	    (PHYSFS_uint64) count > maximum_size / record_size) {
		Warning("Invalid level %s section offset=%d count=%d size=%llu", name, offset, count,
		        (unsigned long long) record_size);
		return 0;
	}
	length = PHYSFS_fileLength(file);
	if (length < 0) {
		Warning("Invalid level %s section file length", name);
		return 0;
	}
	required = (PHYSFS_uint64) count * record_size;
	if ((PHYSFS_uint64) offset > (PHYSFS_uint64) length || required > (PHYSFS_uint64) length - (PHYSFS_uint64) offset ||
	    !PHYSFS_seek(file, offset)) {
		Warning("Invalid level %s section span offset=%d count=%d size=%llu length=%lld", name,
		        offset, count, (unsigned long long) record_size, (long long) length);
		return 0;
	}
	return 1;
}

#endif
