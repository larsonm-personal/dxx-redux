#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "music_soundfont.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x)                                                   \
	do {                                                           \
		if (!(x)) {                                                \
			fprintf(stderr, "Failed line %d: %s\n", __LINE__, #x); \
			return 1;                                              \
		}                                                          \
	} while (0)

int main(int argc, char **argv)
{
	FILE *file;
	long size;
	unsigned char *data;
	tsf *synth;
	short pcm[4096];
	int i, p, block, audible = 0;
	CHECK(argc == 2);
	file = fopen(argv[1], "rb");
	CHECK(file && !fseek(file, 0, SEEK_END));
	size = ftell(file);
	CHECK(size > 12 && !fseek(file, 0, SEEK_SET));
	data = malloc((size_t) size);
	CHECK(data && fread(data, 1, (size_t) size, file) == (size_t) size);
	fclose(file);
	CHECK(music_soundfont_validate(data, (size_t) size));
	CHECK(!music_soundfont_validate(data, (size_t) size - 1));
	CHECK(!music_soundfont_validate(data, MUSIC_SOUNDFONT_MAX_BYTES + 1));
	data[0] = 'X';
	CHECK(!music_soundfont_load_memory(data, (size_t) size));
	data[0] = 'R';
	synth = music_soundfont_load(NULL, argv[1]);
	CHECK(synth && tsf_get_presetcount(synth) > 0);
	tsf_set_output(synth, TSF_STEREO_INTERLEAVED, 48000, -10);
	/* Exercise every preset, including banks with offset/loop generators */
	for (p = 0; p < tsf_get_presetcount(synth); ++p) {
		tsf_reset(synth);
		tsf_note_on(synth, p, 60, 1.0f);
		for (block = 0; block < 32; ++block) {
			tsf_render_short(synth, pcm, 2048, 0);
			for (i = 0; i < 4096; ++i) audible |= pcm[i] != 0;
		}
	}
	CHECK(audible);
	tsf_close(synth);
	CHECK(!music_soundfont_load(NULL, "missing-soundfont-test.sf2"));
	/* Corrupt a real preset's bag index, preserving RIFF lengths */
	for (i = 12; i < size - 70; ++i) {
		if (!memcmp(data + i, "phdr", 4)) {
			data[i + 8 + 24] = data[i + 8 + 25] = 255;
			CHECK(!music_soundfont_load_memory(data, (size_t) size));
			break;
		}
	}
	CHECK(i < size - 70);
	free(data);
	puts("PASS: SF2 loads and all presets render; missing/truncated/oversized/corrupt fonts rejected");
	return 0;
}
