#ifndef DXX_ENDLEVEL_VALIDATION_H
#define DXX_ENDLEVEL_VALIDATION_H

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "physfsx.h"
#include "text.h"
#include "segment.h"
#include "gameseg.h"

#define ENDLEVEL_GRID_MAX_SIZE 64

typedef struct endlevel_description {
	char terrain[256], heightmap[256], satellite[256];
	int exit_x, exit_y, heading, satellite_heading, satellite_pitch;
	int satellite_size, station_heading, station_pitch;
} endlevel_description;

/* Range checks precede conversion to the engine's fixed-point values */
static inline int endlevel_number(const char **text, int minimum, int maximum, int *value)
{
	char *end;
	long number;
	errno = 0;
	number = strtol(*text, &end, 10);
	if (end == *text || errno == ERANGE || number < minimum || number > maximum) return 0;
	while (isspace((unsigned char) *end)) ++end;
	*text = end;
	*value = (int) number;
	return 1;
}

static inline int endlevel_pair(const char *text, int minimum, int maximum, int *a, int *b)
{
	if (!endlevel_number(&text, minimum, maximum, a) || *text++ != ',') return 0;
	return endlevel_number(&text, minimum, maximum, b) && !*text;
}

/* A malformed optional presentation must never leave partially parsed engine state */
static inline int endlevel_read_description(PHYSFS_file *file, int binary, endlevel_description *data)
{
	char line[256];
	int field = 0;
	memset(data, 0, sizeof(*data));
	while (PHYSFSX_fgets(line, sizeof(line), file)) {
		char *begin, *end;
		const char *number;
		if (strlen(line) == sizeof(line) - 1) return 0;
		if (binary) decode_text_line(line);
		if ((end = strchr(line, ';'))) *end = 0;
		begin = line;
		while (isspace((unsigned char) *begin)) ++begin;
		end = begin + strlen(begin);
		while (end > begin && isspace((unsigned char) end[-1])) *--end = 0;
		if (!*begin) continue;
		number = begin;
		switch (field++) {
			case 0: strcpy(data->terrain, begin); break;
			case 1: strcpy(data->heightmap, begin); break;
			case 2:
				if (!endlevel_pair(begin, 0, ENDLEVEL_GRID_MAX_SIZE - 1, &data->exit_x, &data->exit_y)) return 0;
				break;
			case 3:
				if (!endlevel_number(&number, -32767, 32767, &data->heading) || *number) return 0;
				break;
			case 4: strcpy(data->satellite, begin); break;
			case 5:
				if (!endlevel_pair(begin, -32767, 32767, &data->satellite_heading, &data->satellite_pitch)) return 0;
				break;
			case 6:
				if (!endlevel_number(&number, 1, 32767, &data->satellite_size) || *number) return 0;
				break;
			case 7:
				if (!endlevel_pair(begin, -32767, 32767, &data->station_heading, &data->station_pitch)) return 0;
				break;
			default: return 0;
		}
	}
	return field == 8;
}

typedef struct endlevel_route {
	int exit_segment, exit_side, transition_segment, segments;
} endlevel_route;

static inline int endlevel_entry_side(int segment, int previous)
{
	int side;
	for (side = 5; side >= 0; --side)
		if (Segments[segment].children[side] == previous) return side;
	return -1;
}

/* Bounded traversal shared by gameplay and metadata, including direct exterior exits */
static inline const char *endlevel_validate_route(int start, int side, endlevel_route *route)
{
	int previous = start, segment, count, first_side = side;
	if (start < 0 || start > Highest_segment_index || side < 0 || side >= 6) return "invalid_exit";
	segment = Segments[start].children[side];
	for (count = 0; count <= Highest_segment_index + 1; ++count) {
		int entry;
		if (segment == -2) {
			int i, next = Segments[start].children[first_side], old = start;
			route->exit_segment = previous;
			route->exit_side = side;
			route->segments = count;
			route->transition_segment = count ? next : start;
			for (i = 0; i < count / 3; ++i) {
				entry = endlevel_entry_side(next, old);
				old = next;
				next = Segments[next].children[(int) Side_opposite[entry]];
				route->transition_segment = next;
			}
			return NULL;
		}
		if (segment < 0 || segment > Highest_segment_index) return "invalid_exit";
		entry = endlevel_entry_side(segment, previous);
		if (entry < 0 || entry >= 6) return "disconnected_tunnel";
		side = Side_opposite[entry];
		previous = segment;
		segment = Segments[segment].children[side];
	}
	return "cyclic_tunnel";
}

#endif
