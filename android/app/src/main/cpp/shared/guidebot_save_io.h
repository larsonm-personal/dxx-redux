#ifndef DXX_GUIDEBOT_SAVE_IO_H
#define DXX_GUIDEBOT_SAVE_IO_H

#include "rewind_file.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define GUIDEBOT_RUNTIME_SAVE_VERSION 41

/* Fields are explicitly encoded, never native structs or pointers
 * The validation pass consumes the same schema without mutating live state */
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
typedef struct guidebot_save_stream {
	rewind_file *file;
	int writing;
	int apply;
	int ok;
	int64_t epoch;
	size_t bytes;
} guidebot_save_stream;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

enum guidebot_save_kind { GB_SIGNED,
	                      GB_UNSIGNED,
	                      GB_CLOCK,
	                      GB_DOUBLE,
	                      GB_LONG_DOUBLE,
	                      GB_BYTES };

static inline void guidebot_save_bytes(guidebot_save_stream *s, void *data, size_t size)
{
	if (!s->ok) return;
	s->bytes += size;
	if (s->writing == 2) return;
#if REWIND_FILE_USES_WRAPPER
	s->ok = (s->writing ? rewind_file_write(s->file, data, 1, (PHYSFS_uint32) size) : rewind_file_read(s->file, data, 1, (PHYSFS_uint32) size)) == (PHYSFS_sint64) size;
#else
	s->ok = (s->writing ? PHYSFS_writeBytes(s->file, data, size) : PHYSFS_readBytes(s->file, data, size)) == (PHYSFS_sint64) size;
#endif
}

static inline void guidebot_save_field(guidebot_save_stream *s, void *field, size_t width, size_t count, int kind)
{
	size_t index;
	/* Framing needs the encoded width, not value conversion or floating-point formatting */
	if (s->writing == 2) {
		if (s->ok) s->bytes += count * (kind == GB_BYTES ? 1 : kind == GB_LONG_DOUBLE ? 64
			                                                                          : 8);
		return;
	}
	for (index = 0; index < count && s->ok; ++index) {
		unsigned char *value = (unsigned char *) field + index * width;
		unsigned char bytes[64] = { 0 };
		uint64_t bits = 0;
		size_t i;
		if (kind == GB_BYTES) {
			if (s->writing) bytes[0] = *value;
			guidebot_save_bytes(s, bytes, 1);
			if (!s->writing && s->apply && s->ok) *value = bytes[0];
			continue;
		}
		if (kind == GB_LONG_DOUBLE) {
			long double number;
			char *end;
			if (s->writing) {
				memcpy(&number, value, sizeof(number));
				snprintf((char *) bytes, sizeof(bytes), "%La", number);
			}
			guidebot_save_bytes(s, bytes, sizeof(bytes));
			if (!s->writing && s->ok) {
				if (bytes[sizeof(bytes) - 1]) {
					s->ok = 0;
					return;
				}
				number = strtold((char *) bytes, &end);
				if (end == (char *) bytes || *end) {
					s->ok = 0;
					return;
				}
				if (s->apply) memcpy(value, &number, sizeof(number));
			}
			continue;
		}
		if (s->writing) {
			if (width == 4) {
				uint32_t word;
				memcpy(&word, value, 4);
				bits = kind == GB_SIGNED ? (uint64_t) (int64_t) (int32_t) word : word;
			} else if (width == 8) memcpy(&bits, value, 8);
			else {
				s->ok = 0;
				return;
			}
			if (kind == GB_CLOCK) bits -= (uint64_t) s->epoch;
			for (i = 0; i < 8; ++i) bytes[i] = (unsigned char) (bits >> (i * 8));
		}
		guidebot_save_bytes(s, bytes, 8);
		if (!s->writing && s->ok) {
			for (i = 0; i < 8; ++i) bits |= (uint64_t) bytes[i] << (i * 8);
			if (width == 4 && kind == GB_SIGNED && (int64_t) bits != (int32_t) bits) s->ok = 0;
			if (width == 4 && kind == GB_UNSIGNED && bits > UINT32_MAX) s->ok = 0;
			if (kind == GB_CLOCK) bits += (uint64_t) s->epoch;
			if (s->apply && s->ok) {
				if (width == 4) {
					uint32_t word = (uint32_t) bits;
					memcpy(value, &word, 4);
				} else if (width == 8) memcpy(value, &bits, 8);
				else s->ok = 0;
			}
		}
	}
}
/* Bounded counters and indices are checked even in the non-applying pass */
static inline void guidebot_save_bounded(guidebot_save_stream *s, void *field, int kind, int64_t minimum, int64_t maximum)
{
	uint32_t word;
	int64_t value;
	guidebot_save_stream decoded = *s;
	memcpy(&word, field, sizeof(word));
	value = kind == GB_SIGNED ? (int64_t) (int32_t) word : word;
	decoded.apply = 1;
	guidebot_save_field(&decoded, &value, sizeof(value), 1, GB_SIGNED);
	s->ok = decoded.ok && value >= minimum && value <= maximum;
	s->bytes = decoded.bytes;
	if (!s->writing && s->apply && s->ok) {
		word = (uint32_t) value;
		memcpy(field, &word, sizeof(word));
	}
}

static inline void guidebot_save_text(guidebot_save_stream *s, char *text, size_t size)
{
	char buffer[256];
	if (size > sizeof(buffer)) {
		s->ok = 0;
		return;
	}
	if (s->writing) memcpy(buffer, text, size);
	guidebot_save_bytes(s, buffer, size);
	if (!s->ok) return;
	if (!memchr(buffer, 0, size)) {
		s->ok = 0;
		return;
	}
	if (!s->writing && s->apply) memcpy(text, buffer, size);
}
#define GB_LIMIT(s, value, kind, minimum, maximum) guidebot_save_bounded(s, &(value), kind, minimum, maximum)
#define GB_TEXT(s, value)                          guidebot_save_text(s, value, sizeof(value))
#define GB_FIELD(s, value, kind)                   guidebot_save_field(s, &(value), sizeof(value), 1, kind)
#define GB_ARRAY(s, value, kind)                   guidebot_save_field(s, value, sizeof((value)[0]), sizeof(value) / sizeof((value)[0]), kind)

int escort_save_runtime(guidebot_save_stream *s);
void escort_route_save_runtime(guidebot_save_stream *s);
void level_metadata_save_runtime(guidebot_save_stream *s);

#endif
