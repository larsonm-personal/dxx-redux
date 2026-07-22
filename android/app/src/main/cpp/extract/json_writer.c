#include "json_writer.h"

#include <stdint.h>
#include <string.h>

static int valid_utf8_sequence(const unsigned char *text, size_t remaining,
                               size_t *length_out)
{
	unsigned char first = text[0];
	size_t length;
	uint32_t codepoint;
	if (first >= 0xc2 && first <= 0xdf) {
		length = 2;
		codepoint = first & 0x1f;
	} else if (first >= 0xe0 && first <= 0xef) {
		length = 3;
		codepoint = first & 0x0f;
	} else if (first >= 0xf0 && first <= 0xf4) {
		length = 4;
		codepoint = first & 0x07;
	} else {
		return 0;
	}
	if (remaining < length) return 0;
	for (size_t i = 1; i < length; i++) {
		if ((text[i] & 0xc0) != 0x80) return 0;
		codepoint = (codepoint << 6) | (text[i] & 0x3f);
	}
	if ((length == 3 && codepoint < 0x800) ||
	    (length == 4 && codepoint < 0x10000) ||
	    (codepoint >= 0xd800 && codepoint <= 0xdfff) ||
	    codepoint > 0x10ffff)
		return 0;
	*length_out = length;
	return 1;
}

int json_write_string(FILE *output, const char *value)
{
	if (!output) return -1;
	if (!value) return fputs("null", output) < 0 ? -1 : 0;
	if (fputc('"', output) == EOF) return -1;
	const unsigned char *text = (const unsigned char *) value;
	size_t remaining = strlen(value);
	while (remaining > 0) {
		unsigned char byte = *text;
		const char *escape = NULL;
		if (byte == '"') escape = "\\\"";
		else if (byte == '\\') escape = "\\\\";
		else if (byte == '\b') escape = "\\b";
		else if (byte == '\f') escape = "\\f";
		else if (byte == '\n') escape = "\\n";
		else if (byte == '\r') escape = "\\r";
		else if (byte == '\t') escape = "\\t";
		if (escape) {
			if (fputs(escape, output) < 0) return -1;
			text++;
			remaining--;
		} else if (byte < 0x20 || byte >= 0x80) {
			size_t utf8_length;
			if (byte >= 0x80 &&
			    valid_utf8_sequence(text, remaining, &utf8_length)) {
				if (fwrite(text, 1, utf8_length, output) != utf8_length) return -1;
				text += utf8_length;
				remaining -= utf8_length;
			} else {
				if (fprintf(output, "\\u%04x", (unsigned) byte) < 0) return -1;
				text++;
				remaining--;
			}
		} else {
			if (fputc(byte, output) == EOF) return -1;
			text++;
			remaining--;
		}
	}
	return fputc('"', output) == EOF ? -1 : 0;
}
