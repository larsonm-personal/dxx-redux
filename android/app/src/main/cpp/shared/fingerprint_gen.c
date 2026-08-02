/*
 * fingerprint_gen.c -- Generate Chromaprint fingerprints from audio data.
 *
 * Feeds decoded PCM into Chromaprint to produce raw and base64-encoded
 * fingerprints.  Provides convenience wrappers for file-based and
 * CD-sector-based inputs.
 */

#include "fingerprint_gen.h"
#include "pcm_decoders.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <chromaprint.h>

#ifdef ANDROID
#include <android/log.h>
#define LOG_TAG   "fingerprint_gen"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) ((void) 0)
#define LOGE(...) ((void) 0)
#endif

struct fingerprint_stream {
	ChromaprintContext *context;
	int channels;
	int sample_rate;
	size_t total_frames;
};

static int fingerprint_duration_ms(size_t total_frames, int sample_rate,
                                   int *duration_ms)
{
	size_t duration_seconds;
	size_t duration_remainder;
	size_t duration;

	if (total_frames == 0 || sample_rate <= 0 || !duration_ms)
		return -1;
	duration_seconds = total_frames / (size_t) sample_rate;
	duration_remainder = total_frames % (size_t) sample_rate;
	if (duration_seconds > (size_t) INT_MAX / 1000)
		return -1;
	duration = duration_seconds * 1000 +
	           duration_remainder * 1000 / (size_t) sample_rate;
	if (duration > INT_MAX)
		return -1;
	*duration_ms = (int) duration;
	return 0;
}

fingerprint_stream_t *fingerprint_stream_new(int sample_rate, int channels)
{
	fingerprint_stream_t *stream;

	if (sample_rate <= 0 || channels <= 0)
		return NULL;
	stream = (fingerprint_stream_t *) calloc(1, sizeof(*stream));
	if (!stream)
		return NULL;
	stream->context = chromaprint_new(CHROMAPRINT_ALGORITHM_DEFAULT);
	if (!stream->context ||
	    !chromaprint_start(stream->context, sample_rate, channels)) {
		fingerprint_stream_free(stream);
		return NULL;
	}
	stream->channels = channels;
	stream->sample_rate = sample_rate;
	return stream;
}

int fingerprint_stream_feed(fingerprint_stream_t *stream,
                            const int16_t *samples, size_t frames)
{
	size_t frame_offset = 0;
	size_t max_frames;

	if (!stream || !stream->context || !samples || frames == 0 ||
	    frames > SIZE_MAX / (size_t) stream->channels ||
	    stream->total_frames > SIZE_MAX - frames)
		return -1;
	max_frames = (size_t) INT_MAX / (size_t) stream->channels;
	if (max_frames == 0)
		return -1;
	while (frame_offset < frames) {
		size_t remaining = frames - frame_offset;
		size_t chunk_frames = remaining < max_frames ? remaining : max_frames;
		int sample_count = (int) (chunk_frames * (size_t) stream->channels);
		if (!chromaprint_feed(
		        stream->context,
		        samples + frame_offset * (size_t) stream->channels,
		        sample_count)) {
			LOGE("chromaprint_feed failed");
			return -1;
		}
		frame_offset += chunk_frames;
	}
	stream->total_frames += frames;
	return 0;
}

int fingerprint_stream_finish(fingerprint_stream_t *stream,
                              fingerprint_result_t *out)
{
	uint32_t *raw = NULL;
	char *encoded = NULL;
	int raw_len = 0;
	int duration_ms;

	if (!stream || !stream->context || !out ||
	    fingerprint_duration_ms(stream->total_frames, stream->sample_rate,
	                            &duration_ms) != 0)
		return -1;
	memset(out, 0, sizeof(*out));
	out->duration_ms = duration_ms;
	if (!chromaprint_finish(stream->context)) {
		LOGE("chromaprint_finish failed");
		return -1;
	}
	if (!chromaprint_get_raw_fingerprint(stream->context, &raw, &raw_len) ||
	    !raw) {
		LOGE("chromaprint_get_raw_fingerprint failed");
		return -1;
	}
	out->raw_fp = (uint32_t *) malloc((size_t) raw_len * sizeof(uint32_t));
	if (!out->raw_fp) {
		chromaprint_dealloc(raw);
		return -1;
	}
	memcpy(out->raw_fp, raw, (size_t) raw_len * sizeof(uint32_t));
	out->fp_len = raw_len;
	chromaprint_dealloc(raw);
	if (chromaprint_get_fingerprint(stream->context, &encoded) && encoded) {
		out->encoded = strdup(encoded);
		chromaprint_dealloc(encoded);
	}
	if (!out->encoded) {
		fingerprint_free(out);
		return -1;
	}
	return 0;
}

void fingerprint_stream_free(fingerprint_stream_t *stream)
{
	if (!stream)
		return;
	if (stream->context)
		chromaprint_free(stream->context);
	free(stream);
}

int fingerprint_from_pcm(const int16_t *samples, size_t total_frames,
                         int sample_rate, int channels,
                         fingerprint_result_t *out)
{
	fingerprint_stream_t *stream;
	int result;

	if (!samples || total_frames == 0 || !out)
		return -1;
	stream = fingerprint_stream_new(sample_rate, channels);
	if (!stream)
		return -1;
	result = fingerprint_stream_feed(stream, samples, total_frames);
	if (result == 0)
		result = fingerprint_stream_finish(stream, out);
	fingerprint_stream_free(stream);
	return result;
}

int fingerprint_from_audio_file(const char *path, fingerprint_result_t *out)
{
	pcm_decode_result_t pcm = { 0 };
	int result;

	if (!path || !out)
		return -1;
	if (pcm_decode_file(path, &pcm) != 0) {
		LOGE("Failed to decode audio: %s", path);
		return -1;
	}
	result = fingerprint_from_pcm(pcm.pcm_data, pcm.pcm_samples,
	                              pcm.sample_rate, pcm.channels, out);
	pcm_decode_free(&pcm);
	return result;
}

int fingerprint_from_cd_sectors(const uint8_t *sector_data, int sector_count,
                                fingerprint_result_t *out)
{
	if (!sector_data || sector_count <= 0 || !out ||
	    (size_t) sector_count > SIZE_MAX / 588)
		return -1;
	return fingerprint_from_pcm((const int16_t *) sector_data,
	                            (size_t) sector_count * 588, 44100, 2, out);
}

void fingerprint_free(fingerprint_result_t *fp)
{
	if (!fp) return;
	free(fp->raw_fp);
	free(fp->encoded);
	memset(fp, 0, sizeof(*fp));
}
