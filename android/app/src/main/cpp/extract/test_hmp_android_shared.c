#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hmp.h"
#include "hmp_android_shared.h"

static int allocation_count;
static int fail_allocation = -1;
static size_t realloc_calls, realloc_bytes;
static int should_fail_allocation(void)
{
	return fail_allocation >= 0 && allocation_count++ == fail_allocation;
}

void *test_hmp_calloc(size_t count, size_t size)
{
	if (should_fail_allocation())
		return NULL;
	return calloc(count, size);
}

void *test_hmp_malloc(size_t size)
{
	if (should_fail_allocation())
		return NULL;
	return malloc(size);
}

void *test_hmp_realloc(void *ptr, size_t size)
{
	++realloc_calls;
	realloc_bytes += size;
	if (should_fail_allocation())
		return NULL;
	return realloc(ptr, size);
}

void test_hmp_free(void *ptr)
{
	free(ptr);
}

void hmp_close(hmp_file *hmp)
{
	int i;

	if (!hmp)
		return;
	for (i = 0; i < HMP_TRACKS; i++)
		free(hmp->trks[i].data);
	free(hmp);
}

static void write_le32(unsigned char *p, uint32_t value)
{
	p[0] = (unsigned char) value;
	p[1] = (unsigned char) (value >> 8);
	p[2] = (unsigned char) (value >> 16);
	p[3] = (unsigned char) (value >> 24);
}

static unsigned char *make_hmp(const unsigned char *track, size_t track_len,
                               uint32_t num_tracks, size_t *hmp_len)
{
	static const unsigned char first_track[] = { 0x80, 0xff, 0x2f, 0 };
	size_t i, offset = 0x308;
	unsigned char *hmp;

	*hmp_len = offset + num_tracks * 12 + sizeof(first_track);
	if (num_tracks > 1)
		*hmp_len += track_len + (num_tracks - 2) * sizeof(first_track);
	hmp = calloc(1, *hmp_len);
	if (!hmp)
		return NULL;
	memcpy(hmp, "HMIMIDIP", 8);
	write_le32(hmp + 0x30, num_tracks);
	write_le32(hmp + 0x38, 120);
	for (i = 0; i < num_tracks; i++) {
		const unsigned char *payload = first_track;
		size_t payload_len = sizeof(first_track);

		if (i == 1) {
			payload = track;
			payload_len = track_len;
		}
		write_le32(hmp + offset + 4, (uint32_t) (12 + payload_len));
		memcpy(hmp + offset + 12, payload, payload_len);
		offset += 12 + payload_len;
	}
	return hmp;
}

static int convert(const unsigned char *hmp, size_t hmp_len,
                   unsigned char **midi, int *midi_len)
{
	return hmp2mid_mem(hmp, (int) hmp_len, midi, midi_len);
}

static int expect_track_result(const char *name, const unsigned char *track,
                               size_t track_len, int expected)
{
	size_t hmp_len;
	unsigned char *hmp = make_hmp(track, track_len, 2, &hmp_len);
	unsigned char *midi = (unsigned char *) (uintptr_t) 1;
	int midi_len = -1;
	int result;

	if (!hmp)
		return 0;
	allocation_count = 0;
	fail_allocation = -1;
	result = convert(hmp, hmp_len, &midi, &midi_len);
	free(hmp);
	if (result != expected || (!expected && (midi || midi_len != 0))) {
		fprintf(stderr, "%s: result=%d midi=%p len=%d\n",
		        name, result, (void *) midi, midi_len);
		if (result)
			free(midi);
		return 0;
	}
	free(midi);
	return 1;
}

#define EXPECT_REJECT(name, ...)                                 \
	do {                                                         \
		const unsigned char data[] = { __VA_ARGS__ };            \
		if (!expect_track_result((name), data, sizeof(data), 0)) \
			return 1;                                            \
	} while (0)

/* Model the game's copying debug allocator with a bound on bytes requested
 * Growing output per event made preparation quadratic in song length */
static int test_large_song_growth(void)
{
	enum { NOTES = 5000 };
	unsigned char *track = malloc(NOTES * 8 + 4), *hmp;
	size_t hmp_len;
	int mode, i;
	if (!track) return 0;
	for (i = 0; i < NOTES; ++i) {
		const unsigned char note[] = { 0x80, 0x90, 60, 64, 0x81, 0x80, 60, 0 };
		memcpy(track + i * 8, note, sizeof(note));
	}
	memcpy(track + NOTES * 8, "\x81\xff\x2f\0", 4);
	hmp = make_hmp(track, NOTES * 8 + 4, 2, &hmp_len);
	free(track);
	if (!hmp) return 0;
	for (mode = 0; mode < 3; ++mode) {
		int failure;
		/* Includes failure at every buffer growth, not just the first allocation */
		for (failure = -1; failure < 64; ++failure) {
			unsigned char *midi = NULL;
			int midi_len = 0, ok;
			struct hmp_playback_info info;
			allocation_count = 0;
			fail_allocation = failure;
			realloc_calls = realloc_bytes = 0;
			ok = mode == 0 ? convert(hmp, hmp_len, &midi, &midi_len)
			               : hmp2mid_playback_device_mem(hmp, (int) hmp_len, 1, mode == 2, &midi, &midi_len, &info);
			if ((!ok && (failure < 0 || midi || midi_len)) ||
			    (ok && (midi_len < NOTES * 6 || realloc_calls > 32 || realloc_bytes > (size_t) midi_len * 4))) {
				fprintf(stderr, "large HMP mode=%d failure=%d ok=%d len=%d reallocs=%zu bytes=%zu\n",
				        mode, failure, ok, midi_len, realloc_calls, realloc_bytes);
				free(midi);
				free(hmp);
				return 0;
			}
			free(midi);
			if (ok && failure >= 0) break;
		}
		if (failure == 64) { free(hmp); return 0; }
	}
	free(hmp);
	fail_allocation = -1;
	return 1;
}

int main(void)
{
	static const unsigned char valid[] = {
		0x80, 0x90, 60, 64, 0x80, 0xff, 0x2f, 0
	};
	static const unsigned char expected_midi[] = {
		'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 2, 0, 0xc0,
		'M', 'T', 'r', 'k', 0, 0, 0, 11, 0, 0xff, 0x51, 3,
		0x18, 0x80, 0, 0, 0xff, 0x2f, 0,
		'M', 'T', 'r', 'k', 0, 0, 0, 8,
		0, 0x90, 60, 64, 0, 0xff, 0x2f, 0
	};
	size_t hmp_len;
	unsigned char *hmp, *midi;
	int midi_len, failure;
	if (!test_large_song_growth()) return 1;

	if (!expect_track_result("valid", valid, sizeof(valid), 1) ||
	    !expect_track_result("empty", valid, 0, 0))
		return 1;
	hmp = make_hmp(valid, sizeof(valid), 2, &hmp_len);
	midi = NULL;
	midi_len = 0;
	if (!hmp || !convert(hmp, hmp_len, &midi, &midi_len) ||
	    midi_len != (int) sizeof(expected_midi) ||
	    memcmp(midi, expected_midi, sizeof(expected_midi)) != 0) {
		fprintf(stderr, "public wrapper emitted unexpected MIDI bytes\n");
		free(midi);
		free(hmp);
		return 1;
	}
	free(midi);
	free(hmp);
	EXPECT_REJECT("event missing after delta", 0x80);
	EXPECT_REJECT("unterminated delta", 0x00);
	EXPECT_REJECT("oversized delta", 0, 0, 0, 0, 0x80);
	EXPECT_REJECT("note off missing payload", 0x80, 0x80);
	EXPECT_REJECT("note off missing second payload", 0x80, 0x80, 1);
	EXPECT_REJECT("note on missing payload", 0x80, 0x90);
	EXPECT_REJECT("poly pressure missing payload", 0x80, 0xa0, 1);
	EXPECT_REJECT("control change missing payload", 0x80, 0xb0, 1);
	EXPECT_REJECT("program change missing payload", 0x80, 0xc0);
	EXPECT_REJECT("channel pressure missing payload", 0x80, 0xd0);
	EXPECT_REJECT("pitch bend missing payload", 0x80, 0xe0, 1);
	EXPECT_REJECT("meta missing type", 0x80, 0xff);
	EXPECT_REJECT("meta missing length", 0x80, 0xff, 1);
	EXPECT_REJECT("meta unterminated length", 0x80, 0xff, 1, 0x80);
	EXPECT_REJECT("meta payload truncated", 0x80, 0xff, 1, 2, 1);
	EXPECT_REJECT("meta oversized length", 0x80, 0xff, 1,
	              0xff, 0xff, 0xff, 0x7f);
	EXPECT_REJECT("missing end of track", 0x80, 0x90, 60, 64);
	EXPECT_REJECT("trailing data after end", 0x80, 0xff, 0x2f, 0, 0);
	EXPECT_REJECT("nonzero end length", 0x80, 0xff, 0x2f, 1, 0);

	hmp = make_hmp(valid, sizeof(valid), HMP_TRACKS, &hmp_len);
	midi = NULL;
	midi_len = 0;
	if (!hmp || !convert(hmp, hmp_len, &midi, &midi_len)) {
		fprintf(stderr, "maximum track count rejected\n");
		free(hmp);
		return 1;
	}
	free(midi);
	free(hmp);

	hmp = make_hmp(valid, sizeof(valid), 0, &hmp_len);
	midi = (unsigned char *) (uintptr_t) 1;
	midi_len = -1;
	if (!hmp || convert(hmp, hmp_len, &midi, &midi_len) ||
	    midi || midi_len != 0) {
		fprintf(stderr, "zero track count accepted\n");
		free(hmp);
		return 1;
	}
	free(hmp);

	hmp = make_hmp(valid, sizeof(valid), 2, &hmp_len);
	if (!hmp)
		return 1;
	write_le32(hmp + 0x308 + 4, UINT32_MAX);
	midi = (unsigned char *) (uintptr_t) 1;
	midi_len = -1;
	if (convert(hmp, hmp_len, &midi, &midi_len) || midi || midi_len != 0) {
		fprintf(stderr, "maximum declared track length accepted\n");
		free(hmp);
		return 1;
	}
	free(hmp);

	hmp = make_hmp(valid, sizeof(valid), 2, &hmp_len);
	if (!hmp)
		return 1;
	write_le32(hmp + 0x308 + 4, 11);
	midi = (unsigned char *) (uintptr_t) 1;
	midi_len = -1;
	if (convert(hmp, hmp_len, &midi, &midi_len) || midi || midi_len != 0) {
		fprintf(stderr, "undersized declared track length accepted\n");
		free(hmp);
		return 1;
	}
	free(hmp);

	hmp = make_hmp(valid, sizeof(valid), 2, &hmp_len);
	if (!hmp)
		return 1;
	for (failure = 0; failure < 64; failure++) {
		allocation_count = 0;
		fail_allocation = failure;
		midi = (unsigned char *) (uintptr_t) 1;
		midi_len = -1;
		if (convert(hmp, hmp_len, &midi, &midi_len)) {
			free(midi);
			break;
		}
		if (midi || midi_len != 0) {
			fprintf(stderr, "allocation failure %d published output\n", failure);
			free(hmp);
			return 1;
		}
	}
	free(hmp);
	if (failure == 64) {
		fprintf(stderr, "allocation failure sweep never reached success\n");
		return 1;
	}

	/* Playback conversion must also publish no partial output on allocation failure */
	{
		const unsigned char playable[] = { 0x80, 0x90, 60, 64, 0x81, 0xff, 0x2f, 0 };
		struct hmp_playback_info info;
		fail_allocation = -1;
		hmp = make_hmp(playable, sizeof(playable), 2, &hmp_len);
		if (!hmp) return 1;
		for (failure = 0; failure < 256; failure++) {
			allocation_count = 0;
			fail_allocation = failure;
			midi = (unsigned char *) (uintptr_t) 1;
			midi_len = -1;
			if (hmp2mid_playback_mem(hmp, (int) hmp_len, 1, &midi, &midi_len, &info)) {
				free(midi);
				break;
			}
			if (midi || midi_len || info.end_ms || info.repeat_ms) {
				fprintf(stderr, "playback allocation failure %d published output\n", failure);
				free(hmp);
				return 1;
			}
		}
		free(hmp);
		if (failure == 256) return 1;
	}
	printf("HMP Android shared conversion tests passed\n");
	return 0;
}
