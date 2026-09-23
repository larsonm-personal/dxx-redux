/* Host integration of the shipping converter, timeline and both renderers */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "music_synth_hmp.h"
#include "hmp.h"
#include "hmp_android_shared.h"
#include "hog_midi_catalog.h"
#include "midi_seek_timeline.h"
#include "tml.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x)                                                 \
	do {                                                         \
		if (!(x)) {                                              \
			fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); \
			exit(1);                                             \
		}                                                        \
	} while (0)

static FILE *trace_file;
static uint64_t trace_frame;
void music_synth_test_trace(int chip, uint16_t reg, uint8_t value)
{
	if (trace_file) CHECK(fprintf(trace_file, "%llu %u %u\n", (unsigned long long) trace_frame,
		                          (unsigned) (chip * 512 + reg), (unsigned) value) > 0);
}
void *test_hmp_malloc(size_t n)
{
	return malloc(n);
}
void *test_hmp_calloc(size_t n, size_t s)
{
	return calloc(n, s);
}
void *test_hmp_realloc(void *p, size_t n)
{
	return realloc(p, n);
}
void test_hmp_free(void *p)
{
	free(p);
}
void hmp_close(hmp_file *hmp)
{
	for (int i = 0; i < HMP_TRACKS; ++i) free(hmp->trks[i].data);
	free(hmp);
}

static int accept_all(const char *name)
{
	(void) name;
	return 1;
}
static unsigned char *read_hog(void *context, const char *name, size_t *size)
{
	struct hog_midi_catalog catalog;
	unsigned char *data = NULL;
	int length = 0;
	CHECK(hog_catalog_load_filtered(context, &catalog, accept_all) == HOG_MIDI_CATALOG_OK);
	for (size_t i = 0; i < catalog.count; ++i) {
		if (!hog_catalog_strcasecmp(catalog.entries[i].name, name)) {
			CHECK(hog_midi_catalog_read(context, &catalog, i, &data, &length));
			break;
		}
	}
	hog_midi_catalog_free(&catalog);
	*size = (size_t) length;
	return data;
}

static unsigned char *read_fixture(void *context, const char *name, size_t *size)
{
	static const char sng[] = "test.hmp melodic.bnk drums.bnk\n";
	unsigned char *data;
	if (!strcmp(name, "dxx-r.sng")) {
		*size = sizeof(sng) - 1;
		data = malloc(*size);
		memcpy(data, sng, *size);
		return data;
	}
	if (strcmp(name, "melodic.bnk") && strcmp(name, "drums.bnk")) return NULL;
	*size = 28 + 128 * 12 + 128 * 30;
	data = calloc(1, *size);
	CHECK(data);
	const char *signature = context ? context : "ADLIB-";
	int rhythm = !strcmp(signature, "RHYTHM");
	memcpy(data + 2, rhythm ? "ADLIB-" : signature, 6);
	data[8] = data[10] = 128;
	data[12] = 28;
	data[16] = 28;
	data[17] = 6; // 28 + 128*12
	for (int i = 0; i < 128; ++i) {
		unsigned char *record = data + 28 + 128 * 12 + i * 30;
		record[0] = rhythm ? 1 : 0;
		record[1] = rhythm ? 8 : 0;
		data[28 + i * 12] = (unsigned char) i;
		data[30 + i * 12] = 60;
		for (int op = 0; op < 2; ++op) {
			unsigned char *p = record + 2 + op * 13;
			p[1] = 1;
			p[3] = 15;
			p[4] = 4;
			p[5] = 1;
			p[6] = 2;
			p[7] = 8;
			p[8] = op ? 0 : 20;
		}
	}
	return data;
}

static unsigned int event_time(const void *event)
{
	return ((const tml_message *) event)->time;
}
static const void *event_next(const void *event)
{
	return ((const tml_message *) event)->next;
}
static void dispatch(void *context, const void *event)
{
	music_synth *s = context;
	const tml_message *m = event;
	switch (m->type) {
		case TML_NOTE_ON: music_synth_channel_note_on(s, m->channel, m->key, m->velocity / 127.0f); break;
		case TML_NOTE_OFF: music_synth_channel_note_off(s, m->channel, m->key); break;
		case TML_PROGRAM_CHANGE: music_synth_channel_set_presetnumber(s, m->channel, m->program, m->channel == 9); break;
		case TML_CONTROL_CHANGE: music_synth_hmp_control(s, m->channel, m->control, m->control_value); break;
		case TML_PITCH_BEND: music_synth_channel_set_pitchwheel(s, m->channel, m->pitch_bend); break;
	}
}
static void render(void *s, short *out, int frames)
{
	music_synth_render_short(s, out, frames, 0);
	trace_frame += (unsigned) frames;
}
static const struct midi_seek_timeline_ops ops = { event_time, event_next, dispatch, render };

static unsigned long long checksum(const short *pcm, size_t samples)
{
	unsigned long long hash = 14695981039346656037ull;
	for (size_t i = 0; i < samples; ++i) {
		hash ^= (unsigned short) pcm[i] & 255u;
		hash *= 1099511628211ull;
		hash ^= (unsigned short) pcm[i] >> 8;
		hash *= 1099511628211ull;
	}
	return hash;
}

static void write_le(FILE *file, unsigned value, int bytes)
{
	for (int i = 0; i < bytes; ++i) CHECK(fputc((value >> (8 * i)) & 255, file) != EOF);
}

/* Export the same cold-start HMP path used by previews, including fallback */
static int render_comparison(int argc, char **argv)
{
	CHECK(!strcmp(argv[6], "sf2") || !strcmp(argv[6], "ymfm"));
	music_synth *s = music_synth_load(NULL, argv[2], !strcmp(argv[6], "ymfm"));
	CHECK(s);
	size_t size = 0;
	unsigned char *hmp = read_hog(argv[3], argv[4], &size), *midi = NULL;
	int midi_size = 0;
	struct hmp_playback_info info;
	CHECK(hmp);
	music_synth_prepare(s, argv[4], read_hog, argv[3]);
	CHECK(music_synth_convert_hmp(s, hmp, (int) size, !strcmp(argv[1], "--render-repeat"), &midi, &midi_size, &info));
	tml_message *messages = tml_load_memory(midi, midi_size);
	CHECK(messages);
	if (argc == 9) {
		trace_file = fopen(argv[8], "wb");
		CHECK(trace_file);
		trace_frame = 0;
	}
	music_synth_reset(s);
	struct hmp_tsf_state initial = { 0 };
	music_synth_hmp_begin(s, &initial);
	music_synth_set_output(s, TSF_STEREO_INTERLEAVED, 48000, -10);
	music_synth_set_max_voices(s, 48);
	int seconds = argc >= 8 ? atoi(argv[7]) : 20;
	CHECK(seconds > 0 && seconds <= 120);
	const int frames = seconds * 48000;
	short *pcm = malloc((size_t) frames * 2 * sizeof(short));
	CHECK(pcm);
	struct midi_seek_timeline timeline;
	midi_seek_timeline_init(&timeline, messages, 48000, 2, s, &ops);
	int rendered = midi_seek_timeline_render(&timeline, pcm, frames);
	CHECK(rendered >= 0 && rendered <= frames);
	if (rendered < frames) render(s, pcm + rendered * 2, frames - rendered);
	if (trace_file) {
		CHECK(!fclose(trace_file));
		trace_file = NULL;
	}
	FILE *file = fopen(argv[5], "wb");
	CHECK(file);
	CHECK(fwrite("RIFF", 1, 4, file) == 4);
	write_le(file, 36 + frames * 4, 4);
	CHECK(fwrite("WAVEfmt ", 1, 8, file) == 8);
	write_le(file, 16, 4);
	write_le(file, 1, 2);
	write_le(file, 2, 2);
	write_le(file, 48000, 4);
	write_le(file, 48000 * 4, 4);
	write_le(file, 4, 2);
	write_le(file, 16, 2);
	CHECK(fwrite("data", 1, 4, file) == 4);
	write_le(file, frames * 4, 4);
	for (int i = 0; i < frames * 2; ++i) write_le(file, (unsigned short) pcm[i], 2);
	CHECK(!fclose(file));
	printf("selected=%s actual=%s song=%s pcm_fnv=%016llx\n", argv[6], music_synth_is_fm(s) ? "ymfm" : "sf2", argv[4], checksum(pcm, frames * 2));
	free(pcm);
	tml_free(messages);
	free(midi);
	free(hmp);
	music_synth_close(s);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc >= 7 && argc <= 9 && (!strcmp(argv[1], "--render") || !strcmp(argv[1], "--render-repeat"))) return render_comparison(argc, argv);
	CHECK(argc == 2 || argc == 5 || (argc == 6 && !strcmp(argv[5], "--expect-sf2")));
	music_synth *s = music_synth_load(NULL, argv[1], 1);
	CHECK(s);
	CHECK(music_synth_prepare(s, "test.hmp", read_fixture, NULL));
	music_synth_reset(s);
	music_synth_set_output(s, TSF_STEREO_INTERLEAVED, 48000, -10);
	short block[9600];
	music_synth_channel_note_on(s, 0, 60, 1);
	music_synth_channel_midi_control(s, 0, 64, 127);
	music_synth_channel_note_off(s, 0, 60);
	CHECK(music_synth_active_voice_count(s) == 1);
	render(s, block, 4800);
	int peak = 0;
	for (int i = 0; i < 9600; ++i)
		if (abs(block[i]) > peak) peak = abs(block[i]);
	CHECK(peak > 100 && peak < 32767);
	music_synth_channel_midi_control(s, 0, 64, 0);
	CHECK(music_synth_active_voice_count(s) == 0);
	music_synth_channel_note_on(s, 0, 60, 1);
	music_synth_channel_midi_control(s, 0, 123, 0);
	CHECK(music_synth_active_voice_count(s) == 0);
	music_synth_channel_midi_control(s, 0, 120, 0);
	for (int i = 0; i < 20; ++i) render(s, block, 4800);
	for (int i = 0; i < 9600; ++i) CHECK(abs(block[i]) <= 1);
	// Both D2 signatures and the rhythm flag resolve through the real FM loader
	const char *variants[] = { "AMLIB-", "ANLIB-", "RHYTHM" };
	for (size_t i = 0; i < sizeof(variants) / sizeof(variants[0]); ++i) {
		CHECK(music_synth_prepare(s, "test.hmp", read_fixture, (void *) variants[i]));
		music_synth_reset(s);
		music_synth_channel_note_on(s, 9, 60, 1);
		render(s, block, 4800);
		int audible = 0;
		for (int j = 0; j < 9600; ++j) audible |= block[j] != 0;
		CHECK(audible);
	}
	CHECK(!music_synth_prepare(s, "test.hmp", read_fixture, "BADLIB"));
	CHECK(!music_synth_is_fm(s));
	CHECK(!music_synth_prepare(s, "standalone.mid", NULL, NULL));
	if (argc >= 5) {
		size_t size = 0;
		unsigned char *hmp = read_hog(argv[2], argv[3], &size), *midi = NULL;
		int midi_len = 0;
		struct hmp_playback_info info;
		CHECK(hmp && music_synth_prepare(s, argv[3], read_hog, argv[2]));
		CHECK(music_synth_convert_hmp(s, hmp, (int) size, 1, &midi, &midi_len, &info));
		int expected_fm = argc == 5;
		CHECK(music_synth_is_fm(s) == expected_fm);
		if (!expected_fm) {
			unsigned char *gm = NULL;
			int gm_size = 0;
			struct hmp_playback_info gm_info;
			CHECK(hmp2mid_playback_mem(hmp, (int) size, 1, &gm, &gm_size, &gm_info));
			CHECK(gm_size == midi_len && !memcmp(gm, midi, midi_len));
			free(gm);
			puts("Unavailable FM arrangement: exact GM fallback conversion");
		}
		free(hmp);
		FILE *file = fopen(argv[4], "wb");
		CHECK(file && fwrite(midi, 1, midi_len, file) == (size_t) midi_len && !fclose(file));
		tml_message *messages = tml_load_memory(midi, midi_len);
		CHECK(messages);
		const size_t frames = 20 * 48000;
		short *pcm = malloc(frames * 2 * sizeof(short));
		CHECK(pcm);
		unsigned long long hashes[2];
		for (int pass = 0; pass < (expected_fm ? 2 : 1); ++pass) {
			struct midi_seek_timeline timeline;
			music_synth_reset(s);
			music_synth_set_output(s, TSF_STEREO_INTERLEAVED, 48000, -10);
			midi_seek_timeline_init(&timeline, messages, 48000, 2, s, &ops);
			CHECK(midi_seek_timeline_render(&timeline, pcm, (int) frames));
			hashes[pass] = checksum(pcm, frames * 2);
		}
		if (expected_fm) CHECK(hashes[0] == hashes[1]);
		peak = 0;
		for (size_t i = 0; i < frames * 2; ++i)
			if (abs(pcm[i]) > peak) peak = abs(pcm[i]);
		fprintf(stderr, "Song render peak=%d fm=%d\n", peak, expected_fm);
		CHECK(peak > 100);
		if (expected_fm) CHECK(peak < 32767);
		if (expected_fm) {
			struct midi_seek_timeline timeline;
			tml_message *repeat = messages;
			while (repeat && repeat->time < (unsigned int) info.repeat_ms) repeat = repeat->next;
			CHECK(repeat);
			music_synth_reset(s);
			midi_seek_timeline_init(&timeline, messages, 48000, 2, s, &ops);
			CHECK(midi_seek_timeline_set_range(&timeline, repeat, info.repeat_ms, info.end_ms));
			int wrapped = 0, after_peak = 0;
			uint64_t total = midi_seek_timeline_frame_for_ms(info.end_ms + 5000, 48000);
			for (uint64_t cursor = 0; cursor < total; cursor += 4800) {
				uint64_t before = timeline.frame;
				CHECK(midi_seek_timeline_render(&timeline, block, 4800) == 4800);
				if (timeline.frame < before) wrapped = 1;
				for (int i = 0; i < 9600; ++i) {
					CHECK(abs(block[i]) < 32767);
					if (wrapped && abs(block[i]) > after_peak) after_peak = abs(block[i]);
				}
			}
			CHECK(wrapped && after_peak > 100);
			puts("Full song repeat: wrapped, audible, unclipped");
		}
		printf("song=%s filtered=%d end_ms=%.3f peak=%d pcm_fnv=%016llx reset=%s\n", argv[3], info.filtered_tracks, info.end_ms, peak, hashes[0], expected_fm ? "exact" : "not-tested-SF2");
		free(pcm);
		free(midi);
		tml_free(messages);
	}
	music_synth_close(s);
	puts("music_synth integration passed");
	return 0;
}
