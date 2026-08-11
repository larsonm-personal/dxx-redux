#include "android_audio_format.h"

#include <limits.h>
#include <stdio.h>

static int fail(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

static int expect_format(uint16_t format, uint32_t bits, uint32_t byte_order,
                         uint8_t silence, uint32_t channels, uint32_t bytes)
{
	struct android_audio_format description;

	return android_audio_validate_spec(format, channels, 22050, 4096, bytes,
	                                   &description) &&
	       description.sample_bits == bits &&
	       description.container_bits == bits &&
	       description.byte_order == byte_order &&
	       description.silence == silence;
}

int main(void)
{
	struct android_audio_format description;

	if (!expect_format(ANDROID_AUDIO_SDL_U8, 8, ANDROID_AUDIO_LITTLE_ENDIAN,
	                   0x80, 1, 4096) ||
	    !expect_format(ANDROID_AUDIO_SDL_U8, 8, ANDROID_AUDIO_LITTLE_ENDIAN,
	                   0x80, 2, 8192) ||
	    !expect_format(ANDROID_AUDIO_SDL_S16LE, 16, ANDROID_AUDIO_LITTLE_ENDIAN,
	                   0, 1, 8192) ||
	    !expect_format(ANDROID_AUDIO_SDL_S16BE, 16, ANDROID_AUDIO_BIG_ENDIAN,
	                   0, 2, 16384))
		return fail("supported SDL format mapping failed");
	if (android_audio_describe_format(ANDROID_AUDIO_SDL_S8, &description) ||
	    android_audio_describe_format(ANDROID_AUDIO_SDL_U16LE, &description) ||
	    android_audio_describe_format(ANDROID_AUDIO_SDL_U16BE, &description) ||
	    android_audio_describe_format(0xffffu, &description))
		return fail("unsupported SDL format was admitted");
	if (android_audio_validate_spec(ANDROID_AUDIO_SDL_U8, 0, 22050, 4096, 4096,
	                                &description) ||
	    android_audio_validate_spec(ANDROID_AUDIO_SDL_U8, 3, 22050, 4096, 12288,
	                                &description) ||
	    android_audio_validate_spec(ANDROID_AUDIO_SDL_U8, 1, 0, 4096, 4096,
	                                &description) ||
	    android_audio_validate_spec(ANDROID_AUDIO_SDL_U8, 1, INT_MAX, 4096, 4096,
	                                &description) ||
	    android_audio_validate_spec(ANDROID_AUDIO_SDL_U8, 1, 22050, 0, 0,
	                                &description) ||
	    android_audio_validate_spec(ANDROID_AUDIO_SDL_U8, 1, 22050, 4096, 8192,
	                                &description))
		return fail("invalid SDL audio geometry was admitted");
	puts("PASS");
	return 0;
}
