#include "utf8_codec.h"

int dxx_utf16_to_utf8(const uint16_t *input, size_t input_units,
                      char *output, size_t output_size, size_t *output_bytes,
                      int reject_null)
{
	size_t i = 0, written = 0;
	if ((!input && input_units) || !output || !output_bytes) return 0;
	while (i < input_units) {
		uint32_t codepoint = input[i++];
		if (!codepoint && reject_null) return 0;
		if (codepoint >= 0xD800u && codepoint <= 0xDBFFu) {
			uint32_t low;
			if (i >= input_units) return 0;
			low = input[i++];
			if (low < 0xDC00u || low > 0xDFFFu) return 0;
			codepoint = 0x10000u + ((codepoint - 0xD800u) << 10) + (low - 0xDC00u);
		} else if (codepoint >= 0xDC00u && codepoint <= 0xDFFFu) {
			return 0;
		}
		if (codepoint <= 0x7Fu) {
			if (written + 1u > output_size) return 0;
			output[written++] = (char) codepoint;
		} else if (codepoint <= 0x7FFu) {
			if (written + 2u > output_size) return 0;
			output[written++] = (char) (0xC0u | (codepoint >> 6));
			output[written++] = (char) (0x80u | (codepoint & 0x3Fu));
		} else if (codepoint <= 0xFFFFu) {
			if (written + 3u > output_size) return 0;
			output[written++] = (char) (0xE0u | (codepoint >> 12));
			output[written++] = (char) (0x80u | ((codepoint >> 6) & 0x3Fu));
			output[written++] = (char) (0x80u | (codepoint & 0x3Fu));
		} else {
			if (written + 4u > output_size) return 0;
			output[written++] = (char) (0xF0u | (codepoint >> 18));
			output[written++] = (char) (0x80u | ((codepoint >> 12) & 0x3Fu));
			output[written++] = (char) (0x80u | ((codepoint >> 6) & 0x3Fu));
			output[written++] = (char) (0x80u | (codepoint & 0x3Fu));
		}
	}
	*output_bytes = written;
	return 1;
}

int dxx_utf8_to_utf16(const char *input, size_t input_bytes,
                      uint16_t *output, size_t output_units, size_t *written_units)
{
	size_t i = 0, written = 0;
	if ((!input && input_bytes) || !output || !written_units) return 0;
	while (i < input_bytes) {
		uint32_t codepoint;
		unsigned char first = (unsigned char) input[i++];
		if (first <= 0x7Fu) {
			codepoint = first;
		} else if (first >= 0xC2u && first <= 0xDFu && i < input_bytes &&
		           ((unsigned char) input[i] & 0xC0u) == 0x80u) {
			codepoint = ((uint32_t) (first & 0x1Fu) << 6) |
			            ((unsigned char) input[i++] & 0x3Fu);
		} else if (first >= 0xE0u && first <= 0xEFu && i + 1u < input_bytes &&
		           ((unsigned char) input[i] & 0xC0u) == 0x80u &&
		           ((unsigned char) input[i + 1u] & 0xC0u) == 0x80u) {
			unsigned char second = (unsigned char) input[i];
			if ((first == 0xE0u && second < 0xA0u) ||
			    (first == 0xEDu && second >= 0xA0u))
				return 0;
			codepoint = ((uint32_t) (first & 0x0Fu) << 12) |
			            ((uint32_t) (second & 0x3Fu) << 6) |
			            ((unsigned char) input[i + 1u] & 0x3Fu);
			i += 2u;
		} else if (first >= 0xF0u && first <= 0xF4u && i + 2u < input_bytes &&
		           ((unsigned char) input[i] & 0xC0u) == 0x80u &&
		           ((unsigned char) input[i + 1u] & 0xC0u) == 0x80u &&
		           ((unsigned char) input[i + 2u] & 0xC0u) == 0x80u) {
			unsigned char second = (unsigned char) input[i];
			if ((first == 0xF0u && second < 0x90u) ||
			    (first == 0xF4u && second >= 0x90u))
				return 0;
			codepoint = ((uint32_t) (first & 0x07u) << 18) |
			            ((uint32_t) (second & 0x3Fu) << 12) |
			            ((uint32_t) ((unsigned char) input[i + 1u] & 0x3Fu) << 6) |
			            ((unsigned char) input[i + 2u] & 0x3Fu);
			i += 3u;
		} else {
			return 0;
		}
		if (codepoint <= 0xFFFFu) {
			if (written >= output_units) return 0;
			output[written++] = (uint16_t) codepoint;
		} else {
			if (written + 2u > output_units) return 0;
			codepoint -= 0x10000u;
			output[written++] = (uint16_t) (0xD800u + (codepoint >> 10));
			output[written++] = (uint16_t) (0xDC00u + (codepoint & 0x3FFu));
		}
	}
	*written_units = written;
	return 1;
}
