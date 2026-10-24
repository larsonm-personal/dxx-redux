#ifndef DXX_REDUX_MISSION_DESCRIPTOR_SAFE_H
#define DXX_REDUX_MISSION_DESCRIPTOR_SAFE_H

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Exact key matching for new descriptor dialects; legacy token rules stay unchanged */
static inline int mission_descriptor_key_is(const char *line, const char *key)
{
	if (!line || !key || !*key) return 0;
	while (*line && isspace((unsigned char) *line)) ++line;
	while (*key) {
		if (tolower((unsigned char) *line) != tolower((unsigned char) *key)) return 0;
		++line;
		++key;
	}
	while (*line && isspace((unsigned char) *line)) ++line;
	return *line == '=';
}

static inline char *mission_descriptor_value(char *line)
{
	char *value;
	if (!line || !(value = strchr(line, '='))) return NULL;
	for (++value; *value && isspace((unsigned char) *value); ++value) {}
	return *value ? value : NULL;
}

static inline int mission_descriptor_parse_count(const char *text, int minimum, int maximum, int *result)
{
	char *end;
	long value;
	if (!text || !result || minimum > maximum) return 0;
	errno = 0;
	value = strtol(text, &end, 10);
	if (text == end || errno == ERANGE || value < minimum || value > maximum) return 0;
	while (*end && isspace((unsigned char) *end)) ++end;
	if (*end && *end != ';') return 0;
	*result = (int) value;
	return 1;
}

static inline int mission_descriptor_copy_level_name(const char *line, char *output, size_t capacity)
{
	const char *begin, *end;
	size_t length;
	if (!line || !output || !capacity) return 0;
	for (begin = line; *begin && isspace((unsigned char) *begin); ++begin) {}
	for (end = begin; *end && !isspace((unsigned char) *end); ++end) {}
	length = (size_t) (end - begin);
	if (!length || length >= capacity) return 0;
	memcpy(output, begin, length);
	output[length] = 0;
	return 1;
}

static inline int mission_descriptor_parse_secret_ex(const char *line, int last_level, char *name, size_t name_capacity, int *origin, unsigned *extra_origins)
{
	const char *begin, *comma, *name_end, *cursor;
	size_t length;
	int first_origin = 0;
	unsigned parsed_origins = 0;
	if (!line || last_level < 1 || !name || !name_capacity || !origin || !(comma = strchr(line, ','))) return 0;
	for (begin = line; begin < comma && isspace((unsigned char) *begin); ++begin) {}
	for (name_end = begin; name_end < comma && !isspace((unsigned char) *name_end); ++name_end) {}
	for (cursor = name_end; cursor < comma && isspace((unsigned char) *cursor); ++cursor) {}
	length = (size_t) (name_end - begin);
	if (cursor != comma || !length || length >= name_capacity) return 0;

	cursor = comma + 1;
	for (;;) {
		char *end;
		long parsed_origin;
		while (*cursor && isspace((unsigned char) *cursor)) ++cursor;
		if (!*cursor || *cursor == ';') return 0;
		errno = 0;
		parsed_origin = strtol(cursor, &end, 10);
		if (cursor == end || errno == ERANGE || parsed_origin < 1 || parsed_origin > last_level) return 0;
		if (!first_origin) first_origin = (int) parsed_origin;
		++parsed_origins;
		cursor = end;
		while (*cursor && isspace((unsigned char) *cursor)) ++cursor;
		if (*cursor == ',') {
			++cursor;
			continue;
		}
		if (!*cursor || *cursor == ';') break;
		return 0;
	}
	memcpy(name, begin, length);
	name[length] = 0;
	*origin = first_origin;
	if (extra_origins) *extra_origins = parsed_origins - 1;
	return 1;
}

static inline int mission_descriptor_parse_secret(const char *line, int last_level, char *name, size_t name_capacity, int *origin)
{
	return mission_descriptor_parse_secret_ex(line, last_level, name, name_capacity, origin, NULL);
}

static inline int mission_descriptor_make_text_filename(const char *value, const char *extension, char *output, size_t capacity)
{
	const char *begin, *end, *dot = NULL;
	size_t extension_length, stem_length;
	if (!value || !extension || !output || !capacity || extension[0] != '.') return 0;
	for (begin = value; *begin && isspace((unsigned char) *begin); ++begin) {}
	for (end = begin; *end && !isspace((unsigned char) *end); ++end) {
		if (*end == '/' || *end == '\\' || *end == ':') return 0;
		if (*end == '.') dot = end;
	}
	if (dot) end = dot;
	stem_length = (size_t) (end - begin);
	extension_length = strlen(extension);
	if (!stem_length || stem_length + extension_length >= capacity) return 0;
	memcpy(output, begin, stem_length);
	memcpy(output + stem_length, extension, extension_length + 1);
	return 1;
}

#endif
