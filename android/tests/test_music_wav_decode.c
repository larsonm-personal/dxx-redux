#include "music_wav_decode.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

#ifdef main
#undef main
#endif

static void put16(unsigned char *p, unsigned value)
{
	p[0] = (unsigned char) value;
	p[1] = (unsigned char) (value >> 8);
}

static void put32(unsigned char *p, unsigned value)
{
	put16(p, value);
	put16(p + 2, value >> 16);
}

int main(void)
{
	unsigned char wav[52];
	pcm_decode_result_t result;
	int channels, bits;
	for (channels = 1; channels <= 2; ++channels) {
		for (bits = 8; bits <= 16; bits += 8) {
			unsigned bytes = (unsigned) (2 * channels * bits / 8);
			memset(wav, 0, sizeof(wav));
			memcpy(wav, "RIFF", 4); put32(wav + 4, 36 + bytes);
			memcpy(wav + 8, "WAVEfmt ", 8); put32(wav + 16, 16);
			put16(wav + 20, 1); put16(wav + 22, (unsigned) channels);
			put32(wav + 24, 22050); put32(wav + 28, (unsigned) (22050 * channels * bits / 8));
			put16(wav + 32, (unsigned) (channels * bits / 8)); put16(wav + 34, (unsigned) bits);
			memcpy(wav + 36, "data", 4); put32(wav + 40, bytes);
			memset(wav + 44, bits == 8 ? 128 : 0, bytes);
			if (bits == 8) {
				wav[44] = 0;
				wav[44 + bytes - 1] = 255;
			} else {
				put16(wav + 44, 0x8000);
				put16(wav + 44 + bytes - 2, 0x7fff);
			}
			assert(music_decode_wav(wav, 44 + bytes, &result) == PCM_DECODE_OK);
			assert(result.channels == channels && result.sample_rate == 22050);
			assert(result.total_samples == 2 && result.pcm_samples == 2);
			assert(result.pcm_data[0] == -32768);
			assert(result.pcm_data[2 * channels - 1] == (bits == 8 ? 32512 : 32767));
			free(result.pcm_data);
		}
	}
	assert(music_decode_wav("invalid", 7, &result) == PCM_DECODE_ERROR);
	assert(result.pcm_data == NULL);
	assert(music_decode_wav(NULL, 52, &result) == PCM_DECODE_ERROR);
	assert(music_decode_wav(wav, MUSIC_PCM_ENCODED_MAX_BYTES + 1u, &result) == PCM_DECODE_ERROR);
	return 0;
}
