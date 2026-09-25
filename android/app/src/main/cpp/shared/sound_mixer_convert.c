#include "sound_mixer_convert.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

int sound_mixer_convert(const Uint8 *input, size_t length, int source_rate,
                        int output_rate, Uint16 format, int channels,
                        Uint8 **output, Uint32 *output_length)
{
	SDL_AudioCVT cvt;
	uint64_t frames, i;
	if (!output || !output_length)
		return 0;
	*output = NULL;
	*output_length = 0;
	if (!input || !length || length > INT_MAX || source_rate <= 0 || output_rate <= 0 || channels <= 0)
		return 0;
	frames = ((uint64_t) length * output_rate + source_rate / 2) / source_rate;
	if (!frames)
		frames = 1;
	/* SDL 1.x rounds rate ratios to powers of two on this conversion path
	 * Resample explicitly; let SDL handle only sample format and channels
	 * Sample-and-hold preserves the original integer-ratio conversion */
	if (SDL_BuildAudioCVT(&cvt, AUDIO_U8, 1, output_rate, format, channels, output_rate) < 0 ||
	    cvt.len_mult <= 0 || frames > (uint64_t) INT_MAX / cvt.len_mult)
		return 0;
	cvt.buf = malloc((size_t) frames * cvt.len_mult);
	if (!cvt.buf)
		return 0;
	cvt.len = (int) frames;
	for (i = 0; i < frames; ++i) {
		uint64_t source = i * source_rate / output_rate;
		if (source >= length)
			source = length - 1;
		cvt.buf[i] = input[source];
	}
	if (SDL_ConvertAudio(&cvt) < 0) {
		free(cvt.buf);
		return 0;
	}
	*output = cvt.buf;
	*output_length = (Uint32) cvt.len_cvt;
	return 1;
}
