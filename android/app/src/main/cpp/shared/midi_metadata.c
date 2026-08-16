#include "midi_metadata.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hmp_android_shared.h"
#include "u_mem.h"

typedef struct midi_json_buffer {
	char *data;
	size_t length;
	size_t capacity;
} midi_json_buffer;

static uint16_t read_be16(const unsigned char *p)
{
	return (uint16_t) (((uint16_t) p[0] << 8) | p[1]);
}

static uint32_t read_be32(const unsigned char *p)
{
	return ((uint32_t) p[0] << 24) | ((uint32_t) p[1] << 16) |
	       ((uint32_t) p[2] << 8) | p[3];
}

static int ascii_equal_ci(const char *a, const char *b)
{
	while (*a && *b) {
		if (tolower((unsigned char) *a) != tolower((unsigned char) *b))
			return 0;
		++a;
		++b;
	}
	return *a == *b;
}

static int ascii_starts_ci(const char *value, const char *prefix)
{
	while (*prefix) {
		if (!*value || tolower((unsigned char) *value) !=
		                   tolower((unsigned char) *prefix))
			return 0;
		++value;
		++prefix;
	}
	return 1;
}

static const char *ascii_find_ci(const char *value, const char *needle)
{
	size_t needle_length = strlen(needle);
	for (; *value; ++value) {
		size_t i;
		for (i = 0; i < needle_length; ++i)
			if (!value[i] || tolower((unsigned char) value[i]) !=
			                 tolower((unsigned char) needle[i]))
				break;
		if (i == needle_length) return value;
	}
	return NULL;
}

static int utf8_sequence_length(const unsigned char *data, size_t length,
	                            size_t offset)
{
	unsigned char first = data[offset];
	if (first <= 0x7f)
		return first ? 1 : 0;
	if (first >= 0xc2 && first <= 0xdf && offset + 1 < length &&
	    (data[offset + 1] & 0xc0) == 0x80)
		return 2;
	if (first >= 0xe0 && first <= 0xef && offset + 2 < length &&
	    (data[offset + 1] & 0xc0) == 0x80 && (data[offset + 2] & 0xc0) == 0x80 &&
	    !(first == 0xe0 && data[offset + 1] < 0xa0) &&
	    !(first == 0xed && data[offset + 1] >= 0xa0))
		return 3;
	if (first >= 0xf0 && first <= 0xf4 && offset + 3 < length &&
	    (data[offset + 1] & 0xc0) == 0x80 && (data[offset + 2] & 0xc0) == 0x80 &&
	    (data[offset + 3] & 0xc0) == 0x80 &&
	    !(first == 0xf0 && data[offset + 1] < 0x90) &&
	    !(first == 0xf4 && data[offset + 1] >= 0x90))
		return 4;
	return 0;
}

static int valid_utf8(const unsigned char *data, size_t length)
{
	size_t offset = 0;
	while (offset < length) {
		int sequence = utf8_sequence_length(data, length, offset);
		if (!sequence)
			return 0;
		offset += (size_t) sequence;
	}
	return 1;
}

static uint16_t cp1252_codepoint(unsigned char value)
{
	static const uint16_t map[32] = {
		0x20ac, 0x0081, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
		0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008d, 0x017d, 0x008f,
		0x0090, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
		0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0x009d, 0x017e, 0x0178
	};
	if (value >= 0x80 && value <= 0x9f)
		return map[value - 0x80];
	return value;
}

static size_t append_utf8_codepoint(char *output, size_t capacity,
	                                size_t written, uint16_t codepoint)
{
	if (codepoint <= 0x7f) {
		if (written + 1 >= capacity) return written;
		output[written++] = (char) codepoint;
	} else if (codepoint <= 0x7ff) {
		if (written + 2 >= capacity) return written;
		output[written++] = (char) (0xc0 | (codepoint >> 6));
		output[written++] = (char) (0x80 | (codepoint & 0x3f));
	} else {
		if (written + 3 >= capacity) return written;
		output[written++] = (char) (0xe0 | (codepoint >> 12));
		output[written++] = (char) (0x80 | ((codepoint >> 6) & 0x3f));
		output[written++] = (char) (0x80 | (codepoint & 0x3f));
	}
	return written;
}

static char *decode_text(const unsigned char *data, size_t length,
	                     unsigned int *truncated)
{
	size_t limit = length;
	size_t start, end, i, written = 0;
	char *result;
	int input_is_utf8;
	if (limit > MIDI_METADATA_MAX_EVENT_BYTES) {
		limit = MIDI_METADATA_MAX_EVENT_BYTES;
		*truncated = 1;
	}
	result = (char *) malloc(limit * 3u + 1u);
	if (!result)
		return NULL;
	input_is_utf8 = valid_utf8(data, limit);
	for (i = 0; i < limit;) {
		unsigned char value = data[i];
		if (value == '\r' || value == '\n' || value == '\t' || value == 0)
			result[written++] = ' ';
		else if (input_is_utf8) {
			int sequence = utf8_sequence_length(data, limit, i);
			memcpy(result + written, data + i, (size_t) sequence);
			written += (size_t) sequence;
			i += (size_t) sequence;
			continue;
		} else if (value >= 0x20)
			written = append_utf8_codepoint(result, limit * 3u + 1u, written,
			                                cp1252_codepoint(value));
		++i;
	}
	result[written] = '\0';
	start = 0;
	while (start < written && isspace((unsigned char) result[start])) ++start;
	end = written;
	while (end > start && isspace((unsigned char) result[end - 1])) --end;
	if (start)
		memmove(result, result + start, end - start);
	result[end - start] = '\0';
	return result;
}

static int read_vlq(const unsigned char **cursor, const unsigned char *end,
	                uint32_t *value)
{
	unsigned int count = 0;
	uint32_t result = 0;
	do {
		unsigned char byte;
		if (*cursor == end || count == 4)
			return 0;
		byte = *(*cursor)++;
		if (result > (UINT32_MAX >> 7))
			return 0;
		result = (result << 7) | (byte & 0x7f);
		++count;
		if (!(byte & 0x80)) {
			*value = result;
			return 1;
		}
	} while (1);
}

static int add_text_event(midi_metadata *metadata, unsigned int track,
	                      unsigned int type, const unsigned char *data,
	                      size_t length)
{
	char *text;
	if (metadata->event_count == MIDI_METADATA_MAX_EVENTS ||
	    metadata->retained_text_bytes >= MIDI_METADATA_MAX_TEXT_BYTES) {
		metadata->metadata_truncated = 1;
		return 1;
	}
	text = decode_text(data, length, &metadata->metadata_truncated);
	if (!text)
		return 0;
	if (!text[0]) {
		free(text);
		return 1;
	}
	if (metadata->retained_text_bytes + strlen(text) + 1u >
	    MIDI_METADATA_MAX_TEXT_BYTES) {
		metadata->metadata_truncated = 1;
		free(text);
		return 1;
	}
	metadata->events[metadata->event_count].track_index = track;
	metadata->events[metadata->event_count].type = type;
	metadata->events[metadata->event_count].text = text;
	metadata->event_count++;
	metadata->retained_text_bytes += strlen(text) + 1u;
	return 1;
}

static int parse_track(const unsigned char *data, size_t length,
	                   unsigned int track_index, midi_metadata *metadata,
	                   unsigned char *has_channel_events)
{
	const unsigned char *cursor = data;
	const unsigned char *end = data + length;
	unsigned char running_status = 0;
	while (cursor < end) {
		uint32_t ignored_delta;
		unsigned char status;
		if (!read_vlq(&cursor, end, &ignored_delta) || cursor == end)
			return 0;
		status = *cursor++;
		if (status < 0x80) {
			if (!running_status)
				return 0;
			--cursor;
			status = running_status;
		}
		if (status == 0xff) {
			uint32_t meta_length;
			unsigned int type;
			if (cursor == end)
				return 0;
			type = *cursor++;
			if (!read_vlq(&cursor, end, &meta_length) ||
			    meta_length > (uint32_t) (end - cursor))
				return 0;
			if (type >= 0x01 && type <= 0x09 &&
			    !add_text_event(metadata, track_index, type, cursor, meta_length))
				return -1;
			cursor += meta_length;
			running_status = 0;
			continue;
		}
		if (status == 0xf0 || status == 0xf7) {
			uint32_t sysex_length;
			if (!read_vlq(&cursor, end, &sysex_length) ||
			    sysex_length > (uint32_t) (end - cursor))
				return 0;
			cursor += sysex_length;
			running_status = 0;
			continue;
		}
		if (status < 0x80 || status >= 0xf0)
			return 0;
		running_status = status;
		*has_channel_events = 1;
		{
			size_t payload = (status & 0xe0) == 0xc0 ? 1u : 2u;
			if (payload > (size_t) (end - cursor))
				return 0;
			cursor += payload;
		}
	}
	return 1;
}

static int generic_title(const char *text)
{
	return ascii_equal_ci(text, "untitled") || ascii_equal_ci(text, "sequence") ||
	       ascii_equal_ci(text, "track") || ascii_equal_ci(text, "song");
}

static int plausible_title(const char *text)
{
	size_t i, length = strlen(text);
	int letters = 0;
	if (!length || length >= MIDI_METADATA_TITLE_BYTES || strchr(text, '@') ||
	    ascii_equal_ci(text, "me") ||
	    ascii_find_ci(text, "http") || ascii_find_ci(text, "copyright") ||
	    ascii_starts_ci(text, "by ") || ascii_starts_ci(text, "please ") ||
	    ascii_starts_ci(text, "thank ") || ascii_find_ci(text, "contact me"))
		return 0;
	for (i = 0; i < length; ++i)
		if (isalpha((unsigned char) text[i])) letters++;
	return letters >= 2;
}

static int useful_composer(const char *text)
{
	return text[0] && !ascii_equal_ci(text, "me") && !strchr(text, '@') &&
	       strlen(text) < MIDI_METADATA_COMPOSER_BYTES;
}

static void copy_trimmed(char *destination, size_t capacity, const char *source)
{
	size_t length = strlen(source);
	if (length >= capacity)
		length = capacity - 1u;
	while (length && ((unsigned char) source[length] & 0xc0) == 0x80)
		--length;
	memcpy(destination, source, length);
	destination[length] = '\0';
}

static void infer_summary(midi_metadata *metadata,
	                      const unsigned char *track_has_channel)
{
	const char *generic = NULL;
	unsigned int i;
	for (i = 0; i < metadata->event_count; ++i) {
		const midi_metadata_text_event *event = &metadata->events[i];
		if (event->track_index == 0 && event->type == 0x03) {
			if (!generic_title(event->text)) {
				copy_trimmed(metadata->title, sizeof(metadata->title), event->text);
				break;
			}
			generic = event->text;
		}
	}
	for (i = 0; i < metadata->event_count && !metadata->composer[0]; ++i) {
		const char *text = metadata->events[i].text;
		const char *candidate = NULL;
		if (ascii_starts_ci(text, "by "))
			candidate = text + 3;
		else if (ascii_find_ci(text, "copyright")) {
			const char *by = ascii_find_ci(text, " by ");
			if (by) candidate = by + 4;
		}
		if (candidate && useful_composer(candidate))
			copy_trimmed(metadata->composer, sizeof(metadata->composer), candidate);
	}
	if (!metadata->title[0]) {
		for (i = 0; i < metadata->event_count; ++i) {
			const midi_metadata_text_event *event = &metadata->events[i];
			if (event->track_index < metadata->track_count &&
			    !track_has_channel[event->track_index] && plausible_title(event->text) &&
			    !generic_title(event->text) &&
			    (!metadata->composer[0] || !ascii_equal_ci(event->text, metadata->composer)))
				copy_trimmed(metadata->title, sizeof(metadata->title), event->text);
		}
	}
	if (!metadata->title[0] && generic)
		copy_trimmed(metadata->title, sizeof(metadata->title), generic);
	if (metadata->title[0] && metadata->composer[0]) {
		char combined[MIDI_METADATA_TITLE_BYTES + MIDI_METADATA_COMPOSER_BYTES + 4];
		const char *display_title = generic_title(metadata->title) ? "Untitled" : metadata->title;
		snprintf(combined, sizeof(combined), "%s (%s)", display_title,
		         metadata->composer);
		if (strlen(combined) < sizeof(metadata->display_name))
			copy_trimmed(metadata->display_name, sizeof(metadata->display_name), combined);
		else {
			size_t suffix_length = strlen(metadata->composer) + 3u;
			size_t title_bytes = sizeof(metadata->display_name) - suffix_length - 4u;
			if (title_bytes > strlen(display_title)) title_bytes = strlen(display_title);
			while (title_bytes &&
			       ((unsigned char) display_title[title_bytes] & 0xc0) == 0x80)
				--title_bytes;
			snprintf(metadata->display_name, sizeof(metadata->display_name), "%.*s... (%s)",
			         (int) title_bytes, display_title, metadata->composer);
		}
	} else if (metadata->title[0] && !generic_title(metadata->title))
		copy_trimmed(metadata->display_name, sizeof(metadata->display_name), metadata->title);
	else if (metadata->composer[0])
		copy_trimmed(metadata->display_name, sizeof(metadata->display_name), metadata->composer);
}

void midi_metadata_init(midi_metadata *metadata)
{
	if (metadata)
		memset(metadata, 0, sizeof(*metadata));
}

void midi_metadata_free(midi_metadata *metadata)
{
	unsigned int i;
	if (!metadata) return;
	for (i = 0; i < metadata->event_count; ++i)
		free(metadata->events[i].text);
	free(metadata->events);
	midi_metadata_init(metadata);
}

int midi_metadata_parse(const unsigned char *data, size_t length, int is_hmp,
	                    midi_metadata *metadata)
{
	const unsigned char *midi = data;
	size_t midi_length = length, offset;
	unsigned char *converted = NULL;
	unsigned char *track_has_channel = NULL;
	unsigned int parsed_tracks = 0;
	int converted_length = 0;
	int status = MIDI_METADATA_INVALID;
	if (!metadata)
		return MIDI_METADATA_INVALID;
	midi_metadata_free(metadata);
	if (!data || length < 14 || length > MIDI_METADATA_MAX_INPUT_BYTES)
		goto done;
	if (is_hmp) {
		if (length > INT32_MAX ||
		    !hmp2mid_mem(data, (int) length, &converted, &converted_length)) {
			status = MIDI_METADATA_CONVERSION;
			goto done;
		}
		midi = converted;
		midi_length = (size_t) converted_length;
	}
	if (midi_length < 14 || memcmp(midi, "MThd", 4) || read_be32(midi + 4) < 6)
		goto done;
	if ((size_t) read_be32(midi + 4) > midi_length - 8u)
		goto done;
	metadata->smf_format = read_be16(midi + 8);
	metadata->track_count = read_be16(midi + 10);
	metadata->time_division = read_be16(midi + 12);
	if (!metadata->track_count || metadata->track_count > 1024u)
		goto done;
	metadata->events = (midi_metadata_text_event *) calloc(
	    MIDI_METADATA_MAX_EVENTS, sizeof(*metadata->events));
	track_has_channel = (unsigned char *) calloc(metadata->track_count, 1);
	if (!metadata->events || !track_has_channel) {
		status = MIDI_METADATA_ALLOCATION;
		goto done;
	}
	offset = 8u + read_be32(midi + 4);
	while (parsed_tracks < metadata->track_count) {
		uint32_t track_length;
		int parsed;
		if (offset + 8u > midi_length || memcmp(midi + offset, "MTrk", 4))
			goto done;
		track_length = read_be32(midi + offset + 4);
		offset += 8u;
		if (track_length > midi_length - offset)
			goto done;
		parsed = parse_track(midi + offset, track_length, parsed_tracks, metadata,
		                     &track_has_channel[parsed_tracks]);
		if (parsed < 0) {
			status = MIDI_METADATA_ALLOCATION;
			goto done;
		}
		if (!parsed)
			goto done;
		offset += track_length;
		++parsed_tracks;
	}
	infer_summary(metadata, track_has_channel);
	status = MIDI_METADATA_OK;

done:
	free(track_has_channel);
	if (converted) d_free(converted);
	metadata->status = status;
	if (status != MIDI_METADATA_OK) {
		unsigned int truncated = metadata->metadata_truncated;
		midi_metadata_free(metadata);
		metadata->status = status;
		metadata->metadata_truncated = truncated;
	}
	return status;
}

int midi_metadata_has_text(const midi_metadata *metadata)
{
	return metadata && metadata->status == MIDI_METADATA_OK && metadata->event_count;
}

int midi_metadata_has_useful_summary(const midi_metadata *metadata)
{
	return metadata && metadata->status == MIDI_METADATA_OK && metadata->display_name[0];
}

const char *midi_metadata_status_name(int status)
{
	switch (status) {
		case MIDI_METADATA_OK: return "ok";
		case MIDI_METADATA_LIMIT: return "limit";
		case MIDI_METADATA_ALLOCATION: return "allocation_failed";
		case MIDI_METADATA_CONVERSION: return "hmp_conversion_failed";
		default: return "invalid_midi";
	}
}

const char *midi_metadata_event_type_name(unsigned int type)
{
	switch (type) {
		case 0x01: return "Text";
		case 0x02: return "Copyright";
		case 0x03: return "Sequence/track name";
		case 0x04: return "Instrument name";
		case 0x05: return "Lyric";
		case 0x06: return "Marker";
		case 0x07: return "Cue point";
		case 0x08: return "Program name";
		case 0x09: return "Device name";
		default: return "Text";
	}
}

static int json_reserve(midi_json_buffer *buffer, size_t extra)
{
	size_t capacity;
	char *grown;
	if (extra > SIZE_MAX - buffer->length - 1u)
		return 0;
	if (buffer->length + extra + 1u <= buffer->capacity)
		return 1;
	capacity = buffer->capacity ? buffer->capacity : 1024u;
	while (capacity < buffer->length + extra + 1u) {
		if (capacity > SIZE_MAX / 2u)
			return 0;
		capacity *= 2u;
	}
	grown = (char *) realloc(buffer->data, capacity);
	if (!grown)
		return 0;
	buffer->data = grown;
	buffer->capacity = capacity;
	return 1;
}

static int json_append(midi_json_buffer *buffer, const char *text)
{
	size_t length = strlen(text);
	if (!json_reserve(buffer, length))
		return 0;
	memcpy(buffer->data + buffer->length, text, length + 1u);
	buffer->length += length;
	return 1;
}

static int json_string(midi_json_buffer *buffer, const char *text)
{
	const unsigned char *p = (const unsigned char *) (text ? text : "");
	char escape[8];
	if (!json_append(buffer, "\"")) return 0;
	for (; *p; ++p) {
		if (*p == '"' || *p == '\\') {
			escape[0] = '\\'; escape[1] = (char) *p; escape[2] = '\0';
			if (!json_append(buffer, escape)) return 0;
		} else if (*p == '\n' || *p == '\r' || *p == '\t') {
			escape[0] = '\\'; escape[1] = *p == '\n' ? 'n' : (*p == '\r' ? 'r' : 't');
			escape[2] = '\0';
			if (!json_append(buffer, escape)) return 0;
		} else if (*p < 0x20) {
			snprintf(escape, sizeof(escape), "\\u%04x", *p);
			if (!json_append(buffer, escape)) return 0;
		} else {
			char value[2] = { (char) *p, '\0' };
			if (!json_append(buffer, value)) return 0;
		}
	}
	return json_append(buffer, "\"");
}

char *midi_metadata_to_json(const midi_metadata *metadata,
	                        const char *metadata_source_filename,
	                        int inherited_from_midi)
{
	midi_json_buffer buffer = { 0 };
	char number[128];
	unsigned int i;
	if (!metadata) return NULL;
	if (!json_append(&buffer, "{\"parse_status\":")) goto fail;
	if (!json_string(&buffer, midi_metadata_status_name(metadata->status))) goto fail;
	snprintf(number, sizeof(number),
	         ",\"smf_format\":%u,\"track_count\":%u,\"time_division\":%u,",
	         metadata->smf_format, metadata->track_count, metadata->time_division);
	if (!json_append(&buffer, number) || !json_append(&buffer, "\"title\":")) goto fail;
	if (!json_string(&buffer, metadata->title) || !json_append(&buffer, ",\"composer\":")) goto fail;
	if (!json_string(&buffer, metadata->composer) || !json_append(&buffer, ",\"display_name\":")) goto fail;
	if (!json_string(&buffer, metadata->display_name) ||
	    !json_append(&buffer, ",\"metadata_source_filename\":")) goto fail;
	if (!json_string(&buffer, metadata_source_filename) ||
	    !json_append(&buffer, inherited_from_midi ?
	        ",\"inherited_from_midi\":true" : ",\"inherited_from_midi\":false")) goto fail;
	if (!json_append(&buffer, metadata->metadata_truncated ?
	        ",\"metadata_truncated\":true,\"text_events\":[" :
	        ",\"metadata_truncated\":false,\"text_events\":[")) goto fail;
	for (i = 0; i < metadata->event_count; ++i) {
		if (i && !json_append(&buffer, ",")) goto fail;
		snprintf(number, sizeof(number), "{\"track_index\":%u,\"type\":",
		         metadata->events[i].track_index);
		if (!json_append(&buffer, number) ||
		    !json_string(&buffer, midi_metadata_event_type_name(metadata->events[i].type)) ||
		    !json_append(&buffer, ",\"text\":")) goto fail;
		if (!json_string(&buffer, metadata->events[i].text) || !json_append(&buffer, "}")) goto fail;
	}
	if (!json_append(&buffer, "]}")) goto fail;
	return buffer.data;
fail:
	free(buffer.data);
	return NULL;
}
