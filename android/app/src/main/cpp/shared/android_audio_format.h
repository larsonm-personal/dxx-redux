#ifndef ANDROID_AUDIO_FORMAT_H
#define ANDROID_AUDIO_FORMAT_H

#include <stdint.h>

#define ANDROID_AUDIO_SDL_U8    0x0008u
#define ANDROID_AUDIO_SDL_S8    0x8008u
#define ANDROID_AUDIO_SDL_U16LE 0x0010u
#define ANDROID_AUDIO_SDL_S16LE 0x8010u
#define ANDROID_AUDIO_SDL_U16BE 0x1010u
#define ANDROID_AUDIO_SDL_S16BE 0x9010u

#define ANDROID_AUDIO_LITTLE_ENDIAN 1u
#define ANDROID_AUDIO_BIG_ENDIAN    2u

struct android_audio_format {
	uint32_t sample_bits;
	uint32_t container_bits;
	uint32_t byte_order;
	uint8_t silence;
};

static inline int android_audio_describe_format(uint16_t sdl_format,
                                                struct android_audio_format *description)
{
	struct android_audio_format result;

	if (!description)
		return 0;
	switch (sdl_format) {
		case ANDROID_AUDIO_SDL_U8:
			result.sample_bits = 8;
			result.container_bits = 8;
			result.byte_order = ANDROID_AUDIO_LITTLE_ENDIAN;
			result.silence = 0x80;
			break;
		case ANDROID_AUDIO_SDL_S16LE:
			result.sample_bits = 16;
			result.container_bits = 16;
			result.byte_order = ANDROID_AUDIO_LITTLE_ENDIAN;
			result.silence = 0;
			break;
		case ANDROID_AUDIO_SDL_S16BE:
			result.sample_bits = 16;
			result.container_bits = 16;
			result.byte_order = ANDROID_AUDIO_BIG_ENDIAN;
			result.silence = 0;
			break;
		default:
			return 0;
	}
	*description = result;
	return 1;
}

static inline int android_audio_validate_spec(uint16_t sdl_format,
                                              uint32_t channels, int frequency, uint32_t frames, uint32_t buffer_bytes,
                                              struct android_audio_format *description)
{
	struct android_audio_format result;
	uint64_t expected_bytes;

	if (!android_audio_describe_format(sdl_format, &result) ||
	    (channels != 1 && channels != 2) || frequency <= 0 ||
	    (uint64_t) frequency * 1000u > UINT32_MAX || !frames)
		return 0;
	expected_bytes = (uint64_t) (result.container_bits / 8u) * channels * frames;
	if (!expected_bytes || expected_bytes > UINT32_MAX ||
	    buffer_bytes != expected_bytes)
		return 0;
	if (description)
		*description = result;
	return 1;
}

#endif
