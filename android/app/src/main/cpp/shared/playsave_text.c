#include "playsave_text.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "playsave_transaction.h"

#define PLAYSAVE_TEXT_MAX_SIZE (16u * 1024u * 1024u)

struct text_buffer {
	unsigned char *data;
	size_t size;
	size_t capacity;
};

static int line_equals(const unsigned char *line, size_t length,
                       const char *expected)
{
	size_t expected_length = strlen(expected);
	size_t i;

	while (length && (line[length - 1] == '\n' || line[length - 1] == '\r'))
		length--;
	if (length != expected_length)
		return 0;
	for (i = 0; i < length; i++)
		if (tolower(line[i]) != tolower((unsigned char) expected[i]))
			return 0;
	return 1;
}

static int line_starts_with(const unsigned char *line, size_t length,
                            const char *prefix)
{
	size_t prefix_length = strlen(prefix);
	size_t i;

	if (length < prefix_length)
		return 0;
	for (i = 0; i < prefix_length; i++)
		if (tolower(line[i]) != tolower((unsigned char) prefix[i]))
			return 0;
	return 1;
}

static int append(struct text_buffer *buffer, const void *data, size_t size)
{
	size_t capacity;
	unsigned char *resized;

	if (size > PLAYSAVE_TEXT_MAX_SIZE - buffer->size)
		return 0;
	if (buffer->size + size > buffer->capacity) {
		capacity = buffer->capacity ? buffer->capacity : 1024;
		while (capacity < buffer->size + size) {
			if (capacity > PLAYSAVE_TEXT_MAX_SIZE / 2) {
				capacity = PLAYSAVE_TEXT_MAX_SIZE;
				break;
			}
			capacity *= 2;
		}
		resized = realloc(buffer->data, capacity);
		if (!resized)
			return 0;
		buffer->data = resized;
		buffer->capacity = capacity;
	}
	memcpy(buffer->data + buffer->size, data, size);
	buffer->size += size;
	return 1;
}

static int append_missing_entries(struct text_buffer *output,
                                  const struct playsave_text_entry *entries, size_t count,
                                  const unsigned char *written)
{
	size_t i;

	for (i = 0; i < count; i++)
		if (!written[i] && !append(output, entries[i].line,
		                           strlen(entries[i].line)))
			return 0;
	return 1;
}

static int append_new_section(struct text_buffer *output,
                              const char *section_header, const struct playsave_text_entry *entries,
                              size_t count)
{
	size_t i;

	if (!append(output, section_header, strlen(section_header)) ||
	    !append(output, "\n", 1))
		return 0;
	for (i = 0; i < count; i++)
		if (!append(output, entries[i].line, strlen(entries[i].line)))
			return 0;
	return append(output, "[end]\n", 6);
}

static int read_file(const char *path, unsigned char **data, size_t *size)
{
	FILE *f = fopen(path, "rb");
	long length;

	*data = NULL;
	*size = 0;
	if (!f)
		return 1;
	if (fseek(f, 0, SEEK_END) || (length = ftell(f)) < 0 ||
	    (unsigned long) length > PLAYSAVE_TEXT_MAX_SIZE ||
	    fseek(f, 0, SEEK_SET)) {
		fclose(f);
		return 0;
	}
	if (length) {
		*data = malloc((size_t) length);
		if (!*data || fread(*data, 1, (size_t) length, f) != (size_t) length) {
			free(*data);
			*data = NULL;
			fclose(f);
			return 0;
		}
	}
	*size = (size_t) length;
	fclose(f);
	return 1;
}

int playsave_text_update_section(const char *path, const char *options_header,
                                 const char *section_header, const struct playsave_text_entry *entries,
                                 size_t entry_count)
{
	unsigned char *input = NULL;
	unsigned char *written = NULL;
	struct text_buffer output = { 0 };
	size_t input_size, position = 0, last_end = SIZE_MAX;
	int section_exists = 0;
	int in_section = 0;
	int inserted = 0;
	int ok = 0;

	if (!path || !options_header || !section_header ||
	    (!entries && entry_count) || entry_count > 64 ||
	    !read_file(path, &input, &input_size))
		goto cleanup;
	written = calloc(entry_count ? entry_count : 1, 1);
	if (!written)
		goto cleanup;

	while (position < input_size) {
		size_t start = position;
		while (position < input_size && input[position++] != '\n') {}
		if (line_equals(input + start, position - start, section_header))
			section_exists = 1;
		if (line_equals(input + start, position - start, "[end]"))
			last_end = start;
	}
	position = 0;
	if (!input_size) {
		size_t header_size = strlen(options_header);
		if (!append(&output, options_header, header_size) ||
		    (header_size && options_header[header_size - 1] != '\n' &&
		     !append(&output, "\n", 1)))
			goto cleanup;
	}

	while (position < input_size) {
		size_t start = position;
		size_t line_size;
		size_t i;
		while (position < input_size && input[position++] != '\n') {}
		line_size = position - start;
		if (!section_exists && !inserted && start == last_end) {
			if (!append_new_section(&output, section_header, entries, entry_count))
				goto cleanup;
			inserted = 1;
		}
		if (!in_section &&
		    line_equals(input + start, line_size, section_header)) {
			in_section = 1;
			if (!append(&output, input + start, line_size))
				goto cleanup;
			continue;
		}
		if (in_section && line_equals(input + start, line_size, "[end]")) {
			if (!append_missing_entries(&output, entries, entry_count, written) ||
			    !append(&output, input + start, line_size))
				goto cleanup;
			in_section = 0;
			continue;
		}
		if (in_section) {
			for (i = 0; i < entry_count; i++) {
				if (!line_starts_with(input + start, line_size, entries[i].key))
					continue;
				if (!written[i] &&
				    !append(&output, entries[i].line, strlen(entries[i].line)))
					goto cleanup;
				written[i] = 1;
				break;
			}
			if (i < entry_count)
				continue;
		}
		if (!append(&output, input + start, line_size))
			goto cleanup;
	}
	if (in_section &&
	    (!append_missing_entries(&output, entries, entry_count, written) ||
	     !append(&output, "[end]\n", 6)))
		goto cleanup;
	if (!section_exists && !inserted) {
		if (output.size && output.data[output.size - 1] != '\n' &&
		    !append(&output, "\n", 1))
			goto cleanup;
		if (!append_new_section(&output, section_header, entries, entry_count))
			goto cleanup;
	}
	ok = playsave_atomic_replace_file(path, output.data, output.size);

cleanup:
	free(input);
	free(written);
	free(output.data);
	return ok;
}
