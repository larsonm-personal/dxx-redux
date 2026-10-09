#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "d1_shareware_sound.h"
#include "d1_shareware_model.h"

#define CHECK(x)                                                      \
	do {                                                              \
		if (!(x)) {                                                   \
			fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #x); \
			return 1;                                                 \
		}                                                             \
	} while (0)
#ifdef D1_SOUND_REFERENCE
void sound_decompress(unsigned char *data, int size, unsigned char *output);
#endif

int main(int argc, char **argv)
{
	unsigned char input[] = { 0x77, 0x77, 0xff }, output[8];
	memset(output, 0x6d, sizeof(output));
	CHECK(d1_shareware_sound_decode(input, 3, output, 5));
	CHECK(output[5] == 0x6d);
	CHECK(!d1_shareware_sound_decode(input, 3, output, 7));
	CHECK(!d1_shareware_sound_decode(input, 3, output, 4));
	CHECK(d1_shareware_sound_decode(input, 0, output, 0));
	if (argc == 2) {
		FILE *file = fopen(argv[1], "rb");
		unsigned char *pig;
		long size;
		unsigned bitmaps, sounds, hash = 2166136261u, odd = 0;
		size_t headers, data_start, i, decoded = 0;
		CHECK(file != NULL && fseek(file, 0, SEEK_END) == 0);
		size = ftell(file);
		CHECK(size > 8 && size < 16 * 1024 * 1024 && fseek(file, 0, SEEK_SET) == 0);
		pig = (unsigned char *) malloc(size);
		CHECK(pig != NULL && fread(pig, 1, size, file) == (size_t) size);
		fclose(file);
		bitmaps = d1_pof_u32(pig);
		sounds = d1_pof_u32(pig + 4);
		CHECK(bitmaps == 1152 && sounds == 70);
		headers = 8 + bitmaps * 17;
		data_start = headers + sounds * 20;
		CHECK(data_start <= (size_t) size);
		for (i = 0; i < sounds; ++i) {
			const unsigned char *header = pig + headers + i * 20;
			size_t length = d1_pof_u32(header + 8), encoded = d1_pof_u32(header + 12);
			size_t offset = d1_pof_u32(header + 16), j;
			unsigned char *samples;
			CHECK(length < 1024 * 1024 && data_start + offset <= (size_t) size && encoded <= (size_t) size - data_start - offset);
			CHECK(encoded == length / 2 + length % 2);
			samples = (unsigned char *) malloc(length + 2);
			CHECK(samples != NULL);
			memset(samples, 0x6d, length + 2);
#ifdef D1_SOUND_REFERENCE
			sound_decompress(pig + data_start + offset, (int) encoded, samples);
#else
			CHECK(d1_shareware_sound_decode(pig + data_start + offset, encoded, samples, length));
			CHECK(samples[length] == 0x6d && samples[length + 1] == 0x6d);
#endif
			for (j = 0; j < length; ++j) hash = (hash ^ samples[j]) * 16777619u;
			odd += length % 2;
			decoded += length;
			free(samples);
		}
		free(pig);
		printf("Fixture sounds=%u odd=%u decoded=%zu fnv1a=%08x\n", sounds, odd, decoded, hash);
		/* Baseline from the original native D1 decoder on both authentic sources */
		CHECK(odd == 35 && decoded == 827545 && hash == 0x770dec7eu);
	}
	puts("PASS: bounded shared D1 sound decoder");
	return 0;
}
