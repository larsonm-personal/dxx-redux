#include "music_track_json.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void music_track_json_write(music_track_json_writer *writer, const char *text, size_t length)
{
	if (length > SIZE_MAX - writer->length) {
		writer->failed = 1;
		return;
	}
	if (writer->buffer) {
		if (writer->capacity == 0 || writer->length >= writer->capacity ||
		    length > writer->capacity - writer->length - 1) {
			writer->failed = 1;
		} else if (!writer->failed) {
			memcpy(writer->buffer + writer->length, text, length);
		}
	}
	writer->length += length;
}

static void music_track_json_write_string(music_track_json_writer *writer, const char *text)
{
	unsigned char ch;
	if (!text)
		text = "";
	while ((ch = (unsigned char) *text++) != '\0') {
		switch (ch) {
			case '\\':
				music_track_json_write(writer, "\\\\", 2);
				break;
			case '"':
				music_track_json_write(writer, "\\\"", 2);
				break;
			case '\n':
				music_track_json_write(writer, "\\n", 2);
				break;
			case '\r':
				music_track_json_write(writer, "\\r", 2);
				break;
			case '\t':
				music_track_json_write(writer, "\\t", 2);
				break;
			default: {
				const char output = ch < 0x20 ? ' ' : (char) ch;
				music_track_json_write(writer, &output, 1);
				break;
			}
		}
	}
}

void music_track_json_begin(music_track_json_writer *writer, char *buffer, size_t capacity)
{
	writer->buffer = buffer;
	writer->capacity = capacity;
	writer->length = 0;
	writer->count = 0;
	writer->failed = 0;
	music_track_json_write(writer, "[", 1);
}

void music_track_json_add(music_track_json_writer *writer, int index, const char *name)
{
	char index_text[32];
	const int index_length = snprintf(index_text, sizeof(index_text), "%d", index);
	if (index_length < 0 || (size_t) index_length >= sizeof(index_text)) {
		writer->failed = 1;
		return;
	}
	if (writer->count++)
		music_track_json_write(writer, ",", 1);
	music_track_json_write(writer, "{\"index\":", 9);
	music_track_json_write(writer, index_text, (size_t) index_length);
	music_track_json_write(writer, ",\"name\":\"", 9);
	music_track_json_write_string(writer, name);
	music_track_json_write(writer, "\"}", 2);
}

int music_track_json_finish(music_track_json_writer *writer)
{
	music_track_json_write(writer, "]", 1);
	if (writer->length > INT_MAX)
		writer->failed = 1;
	if (writer->buffer) {
		if (writer->failed || writer->length >= writer->capacity) {
			if (writer->capacity)
				writer->buffer[0] = '\0';
			return -1;
		}
		writer->buffer[writer->length] = '\0';
	}
	return writer->failed ? -1 : (int) writer->length;
}
