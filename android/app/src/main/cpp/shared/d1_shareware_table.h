/* Shared D1 BITMAPS.TBL/BIN input, independent of either engine's live tables */
#ifndef DXX_D1_SHAREWARE_TABLE_H
#define DXX_D1_SHAREWARE_TABLE_H

#include <stddef.h>
#include <string.h>

/* read_byte returns 0..255, -1 at EOF, or -2 on an I/O error */
typedef struct d1_shareware_table_reader {
	int (*read_byte)(void *source);
	void *source;
	unsigned line;
	int encoded;
	int pending;
	const char *error;
} d1_shareware_table_reader;

static inline void d1_shareware_table_init(d1_shareware_table_reader *reader,
                                           int (*read_byte)(void *), void *source, int encoded)
{
	reader->read_byte = read_byte;
	reader->source = source;
	reader->line = 0;
	reader->encoded = encoded;
	reader->pending = -1;
	reader->error = NULL;
}

static inline unsigned char d1_shareware_table_decode(unsigned char value)
{
	value = (unsigned char) ((value << 1) | (value >> 7));
	value ^= 0xd3;
	return (unsigned char) ((value << 1) | (value >> 7));
}

/* Returns one physical line without its terminator, zero at EOF, -1 on error */
static inline int d1_shareware_table_physical_line(d1_shareware_table_reader *reader,
                                                   char *output, size_t capacity)
{
	size_t length = 0;
	int value;
	if (!capacity) {
		reader->error = "No space for table line";
		return -1;
	}
	output[0] = 0;
	for (;;) {
		value = reader->pending;
		reader->pending = -1;
		if (value == -1)
			value = reader->read_byte(reader->source);
		if (value < -1) {
			reader->error = "Cannot read table";
			return -1;
		}
		if (value == -1 && length == 0)
			return 0;
		if (value == -1 || value == '\n' || value == '\r') {
			if (value == '\r') {
				reader->pending = reader->read_byte(reader->source);
				if (reader->pending == '\n')
					reader->pending = -1;
			}
			++reader->line;
			return 1;
		}
		if (reader->encoded)
			value = d1_shareware_table_decode((unsigned char) value);
		if (!value) {
			reader->error = "Embedded NUL in table";
			return -1;
		}
		if (length == capacity - 1) {
			reader->error = "Table line exceeds buffer";
			return -1;
		}
		output[length++] = (char) value;
		output[length] = 0;
	}
}

/* Read a logical line, joining plain-text continuations and removing comments */
static inline int d1_shareware_table_next(d1_shareware_table_reader *reader,
                                          char *output, size_t capacity)
{
	int result = d1_shareware_table_physical_line(reader, output, capacity);
	size_t length;
	char *comment;
	if (result <= 0)
		return result;
	while (!reader->encoded && (length = strlen(output)) != 0 && output[length - 1] == '\\') {
		output[length - 1] = ' ';
		result = d1_shareware_table_physical_line(reader, output + length, capacity - length);
		if (result <= 0) {
			if (!result)
				reader->error = "Unfinished table continuation";
			return -1;
		}
	}
	comment = strchr(output, ';');
	if (comment)
		*comment = 0;
	return 1;
}

#endif
