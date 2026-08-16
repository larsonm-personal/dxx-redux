#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hmp.h"
#include "midi_metadata.h"

void *test_hmp_calloc(size_t count, size_t size) { return calloc(count, size); }
void *test_hmp_malloc(size_t size) { return malloc(size); }
void *test_hmp_realloc(void *ptr, size_t size) { return realloc(ptr, size); }
void test_hmp_free(void *ptr) { free(ptr); }

void hmp_close(hmp_file *hmp)
{
	int i;
	if (!hmp) return;
	for (i = 0; i < HMP_TRACKS; ++i) free(hmp->trks[i].data);
	free(hmp);
}

typedef struct byte_buffer {
	unsigned char data[4096];
	size_t length;
} byte_buffer;

static void put_byte(byte_buffer *buffer, unsigned int value)
{
	buffer->data[buffer->length++] = (unsigned char) value;
}

static void put_bytes(byte_buffer *buffer, const void *data, size_t length)
{
	memcpy(buffer->data + buffer->length, data, length);
	buffer->length += length;
}

static void put_be16(byte_buffer *buffer, unsigned int value)
{
	put_byte(buffer, value >> 8);
	put_byte(buffer, value);
}

static void put_be32(byte_buffer *buffer, uint32_t value)
{
	put_byte(buffer, value >> 24);
	put_byte(buffer, value >> 16);
	put_byte(buffer, value >> 8);
	put_byte(buffer, value);
}

static void put_le32(unsigned char *target, uint32_t value)
{
	target[0] = (unsigned char) value;
	target[1] = (unsigned char) (value >> 8);
	target[2] = (unsigned char) (value >> 16);
	target[3] = (unsigned char) (value >> 24);
}

static size_t begin_track(byte_buffer *buffer)
{
	put_bytes(buffer, "MTrk", 4);
	put_be32(buffer, 0);
	return buffer->length;
}

static void finish_track(byte_buffer *buffer, size_t start)
{
	uint32_t length = (uint32_t) (buffer->length - start);
	buffer->data[start - 4] = (unsigned char) (length >> 24);
	buffer->data[start - 3] = (unsigned char) (length >> 16);
	buffer->data[start - 2] = (unsigned char) (length >> 8);
	buffer->data[start - 1] = (unsigned char) length;
}

static void put_text(byte_buffer *buffer, unsigned int type, const char *text)
{
	size_t length = strlen(text);
	put_byte(buffer, 0);
	put_byte(buffer, 0xff);
	put_byte(buffer, type);
	put_byte(buffer, (unsigned int) length);
	put_bytes(buffer, text, length);
}

static void put_end(byte_buffer *buffer)
{
	static const unsigned char end[] = { 0, 0xff, 0x2f, 0 };
	put_bytes(buffer, end, sizeof(end));
}

static void begin_midi(byte_buffer *buffer, unsigned int tracks)
{
	memset(buffer, 0, sizeof(*buffer));
	put_bytes(buffer, "MThd", 4);
	put_be32(buffer, 6);
	put_be16(buffer, 1);
	put_be16(buffer, tracks);
	put_be16(buffer, 480);
}

static int test_obsidian_style(void)
{
	byte_buffer buffer;
	midi_metadata metadata;
	size_t track;
	char *json;
	begin_midi(&buffer, 3);
	track = begin_track(&buffer);
	put_text(&buffer, 3, "untitled");
	put_end(&buffer);
	finish_track(&buffer, track);
	track = begin_track(&buffer);
	put_text(&buffer, 3, "bass 1");
	put_byte(&buffer, 0); put_byte(&buffer, 0x90); put_byte(&buffer, 60); put_byte(&buffer, 100);
	put_end(&buffer);
	finish_track(&buffer, track);
	track = begin_track(&buffer);
	put_text(&buffer, 1, "-------------------------");
	put_text(&buffer, 1, "By Doug Hale");
	put_text(&buffer, 1, "3.4.99");
	put_text(&buffer, 1, "anarchy@example.test");
	put_text(&buffer, 1, "Hotshot");
	put_end(&buffer);
	finish_track(&buffer, track);
	midi_metadata_init(&metadata);
	if (midi_metadata_parse(buffer.data, buffer.length, 0, &metadata) != MIDI_METADATA_OK ||
	    strcmp(metadata.title, "Hotshot") || strcmp(metadata.composer, "Doug Hale") ||
	    strcmp(metadata.display_name, "Hotshot (Doug Hale)") || metadata.event_count != 7) {
		fprintf(stderr, "Obsidian-style inference failed: title='%s' composer='%s' display='%s' events=%u\n",
		        metadata.title, metadata.composer, metadata.display_name, metadata.event_count);
		midi_metadata_free(&metadata);
		return 0;
	}
	json = midi_metadata_to_json(&metadata, "game01.mid", 1);
	if (!json || !strstr(json, "\"inherited_from_midi\":true") ||
	    !strstr(json, "Hotshot (Doug Hale)")) {
		fprintf(stderr, "metadata JSON failed\n");
		free(json);
		midi_metadata_free(&metadata);
		return 0;
	}
	free(json);
	midi_metadata_free(&metadata);
	return 1;
}

static int test_copyright_and_cp1252(void)
{
	byte_buffer buffer;
	midi_metadata metadata;
	size_t track;
	static const unsigned char copyright_text[] = {
		'C','o','p','y','r','i','g','h','t',' ',0xa9,' ','1','9','9','8',' ',
		'b','y',' ','V','e','r','r','a','n',' ','E','v','e','n','t','i','d','e'
	};
	begin_midi(&buffer, 1);
	track = begin_track(&buffer);
	put_text(&buffer, 3, "untitled");
	put_byte(&buffer, 0); put_byte(&buffer, 0xff); put_byte(&buffer, 2);
	put_byte(&buffer, sizeof(copyright_text));
	put_bytes(&buffer, copyright_text, sizeof(copyright_text));
	put_end(&buffer);
	finish_track(&buffer, track);
	midi_metadata_init(&metadata);
	if (midi_metadata_parse(buffer.data, buffer.length, 0, &metadata) != MIDI_METADATA_OK ||
	    strcmp(metadata.composer, "Verran Eventide") ||
	    strcmp(metadata.display_name, "Untitled (Verran Eventide)") ||
	    !strstr(metadata.events[1].text, "\xc2\xa9")) {
		fprintf(stderr, "copyright inference or CP1252 decode failed\n");
		midi_metadata_free(&metadata);
		return 0;
	}
	midi_metadata_free(&metadata);
	return 1;
}

static int test_hmp_metadata(void)
{
	static const unsigned char first_track[] = { 0x80, 0xff, 0x2f, 0 };
	static const unsigned char text_track[] = {
		0x80, 0xff, 0x03, 0x08, 'H','M','P',' ','S','o','n','g',
		0x80, 0xff, 0x2f, 0
	};
	const size_t total = 0x308 + 12 + sizeof(first_track) + 12 + sizeof(text_track);
	unsigned char *hmp = (unsigned char *) calloc(1, total);
	midi_metadata metadata;
	size_t offset = 0x308;
	if (!hmp) return 0;
	memcpy(hmp, "HMIMIDIP", 8);
	put_le32(hmp + 0x30, 2);
	put_le32(hmp + 0x38, 120);
	put_le32(hmp + offset + 4, 12 + sizeof(first_track));
	memcpy(hmp + offset + 12, first_track, sizeof(first_track));
	offset += 12 + sizeof(first_track);
	put_le32(hmp + offset + 4, 12 + sizeof(text_track));
	memcpy(hmp + offset + 12, text_track, sizeof(text_track));
	midi_metadata_init(&metadata);
	if (midi_metadata_parse(hmp, total, 1, &metadata) != MIDI_METADATA_OK ||
	    strcmp(metadata.title, "HMP Song")) {
		fprintf(stderr, "HMP metadata conversion failed\n");
		midi_metadata_free(&metadata);
		free(hmp);
		return 0;
	}
	midi_metadata_free(&metadata);
	free(hmp);
	return 1;
}

static int test_rejections(void)
{
	static const unsigned char truncated[] = {
		'M','T','h','d',0,0,0,6,0,1,0,1,1,0xe0,
		'M','T','r','k',0,0,0,5,0,0xff,3,8,'x'
	};
	midi_metadata metadata;
	midi_metadata_init(&metadata);
	if (midi_metadata_parse(truncated, sizeof(truncated), 0, &metadata) !=
	    MIDI_METADATA_INVALID) {
		fprintf(stderr, "truncated MIDI accepted\n");
		midi_metadata_free(&metadata);
		return 0;
	}
	midi_metadata_free(&metadata);
	return 1;
}

int main(void)
{
	if (!test_obsidian_style() || !test_copyright_and_cp1252() ||
	    !test_hmp_metadata() || !test_rejections())
		return 1;
	printf("MIDI metadata tests passed\n");
	return 0;
}
