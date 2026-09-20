#ifndef MUSIC_WAV_DECODE_H
#define MUSIC_WAV_DECODE_H

#include <SDL.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "music_decode_limits.h"
#include "pcm_decoders.h"

/* Use SDL's WAV decoder and the same owned 16-bit PCM contract as other music */
static inline int music_decode_wav(const void *data, size_t size, pcm_decode_result_t *out)
{
	SDL_AudioSpec spec;
	SDL_AudioCVT conversion;
	SDL_RWops *stream;
	Uint8 *samples = NULL;
	Uint32 length = 0;
	size_t frame_bytes, frames;
	int result = PCM_DECODE_ERROR;

	if (!out) return PCM_DECODE_ERROR;
	memset(out, 0, sizeof(*out));
	if (!data || !size || size > MUSIC_PCM_ENCODED_MAX_BYTES || size > INT_MAX)
		return PCM_DECODE_ERROR;
	stream = SDL_RWFromConstMem(data, (int) size);
	if (!stream || !SDL_LoadWAV_RW(stream, 1, &spec, &samples, &length))
		return PCM_DECODE_ERROR;
	if (spec.channels != 1 && spec.channels != 2) {
		result = PCM_DECODE_UNSUPPORTED_CHANNELS;
		goto done;
	}
	frame_bytes = (size_t) spec.channels * (spec.format & 0xff) / 8;
	if (!frame_bytes || !length || length % frame_bytes || spec.freq <= 0 || length > INT_MAX)
		goto done;
	frames = length / frame_bytes;
	if (!music_pcm_budget_allows(size, frames, spec.channels)) {
		result = PCM_DECODE_BUDGET_EXCEEDED;
		goto done;
	}
	memset(&conversion, 0, sizeof(conversion));
	if (SDL_BuildAudioCVT(&conversion, spec.format, spec.channels, spec.freq,
	                      AUDIO_S16SYS, spec.channels, spec.freq) < 0)
		goto done;
	if (conversion.len_mult <= 0 ||
	    (size_t) length > MUSIC_PCM_DECODED_MAX_BYTES / (size_t) conversion.len_mult) {
		result = PCM_DECODE_BUDGET_EXCEEDED;
		goto done;
	}
	conversion.buf = (Uint8 *) malloc((size_t) length * conversion.len_mult);
	if (!conversion.buf) goto done;
	memcpy(conversion.buf, samples, length);
	conversion.len = (int) length;
	if (SDL_ConvertAudio(&conversion) < 0 ||
	    conversion.len_cvt != (int) (frames * spec.channels * sizeof(int16_t))) {
		free(conversion.buf);
		goto done;
	}
	out->pcm_data = (int16_t *) conversion.buf;
	out->sample_rate = spec.freq;
	out->channels = spec.channels;
	out->total_samples = out->pcm_samples = frames;
	result = PCM_DECODE_OK;
done:
	SDL_FreeWAV(samples);
	return result;
}

#endif
