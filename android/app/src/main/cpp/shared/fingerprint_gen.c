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

int fingerprint_pcm_window(size_t total_frames, int sample_rate, int channels,
                           int *feed_samples, int *duration_ms)
{
	size_t max_frames;
	size_t feed_frames;
	size_t duration_seconds;
	size_t duration_remainder;
	size_t duration;

	if (total_frames == 0 || sample_rate <= 0 || channels <= 0 ||
	    !feed_samples || !duration_ms)
		return -1;
	if ((size_t) sample_rate > SIZE_MAX / FINGERPRINT_MAX_SECONDS)
		return -1;
	max_frames = (size_t) sample_rate * FINGERPRINT_MAX_SECONDS;
	feed_frames = total_frames < max_frames ? total_frames : max_frames;
	if (feed_frames > (size_t) INT_MAX / (size_t) channels)
		return -1;

	/* Split the duration calculation so total_frames * 1000 cannot overflow. */
	duration_seconds = total_frames / (size_t) sample_rate;
	duration_remainder = total_frames % (size_t) sample_rate;
	if (duration_seconds > (size_t) INT_MAX / 1000)
		return -1;
	duration = duration_seconds * 1000 +
	           duration_remainder * 1000 / (size_t) sample_rate;
	if (duration > INT_MAX)
		return -1;

	*feed_samples = (int) (feed_frames * (size_t) channels);
	*duration_ms = (int) duration;
	return 0;
}

int fingerprint_from_pcm_prefix(const int16_t *samples, size_t available_frames,
                                size_t total_frames, int sample_rate, int channels,
                                fingerprint_result_t *out)
{
	int feed_count;
	int duration_ms;
	size_t feed_frames;

	if (!samples || !out ||
	    fingerprint_pcm_window(total_frames, sample_rate, channels,
	                           &feed_count, &duration_ms) != 0)
		return -1;
	feed_frames = (size_t) feed_count / (size_t) channels;
	if (available_frames < feed_frames)
		return -1;

	memset(out, 0, sizeof(*out));

	ChromaprintContext *ctx = chromaprint_new(CHROMAPRINT_ALGORITHM_DEFAULT);
	if (!ctx) {
		LOGE("chromaprint_new failed");
		return -1;
	}

	if (!chromaprint_start(ctx, sample_rate, channels)) {
		LOGE("chromaprint_start failed");
		chromaprint_free(ctx);
		return -1;
	}

	if (!chromaprint_feed(ctx, samples, feed_count)) {
		LOGE("chromaprint_feed failed");
		chromaprint_free(ctx);
		return -1;
	}

	if (!chromaprint_finish(ctx)) {
		LOGE("chromaprint_finish failed");
		chromaprint_free(ctx);
		return -1;
	}

	/* Get raw fingerprint */
	uint32_t *raw = NULL;
	int raw_len = 0;
	if (!chromaprint_get_raw_fingerprint(ctx, &raw, &raw_len) || !raw) {
		LOGE("chromaprint_get_raw_fingerprint failed");
		chromaprint_free(ctx);
		return -1;
	}

	/* Copy the raw fingerprint to our own allocation */
	out->raw_fp = (uint32_t *) malloc((size_t) raw_len * sizeof(uint32_t));
	if (!out->raw_fp) {
		chromaprint_dealloc(raw);
		chromaprint_free(ctx);
		return -1;
	}
	memcpy(out->raw_fp, raw, (size_t) raw_len * sizeof(uint32_t));
	out->fp_len = raw_len;
	chromaprint_dealloc(raw);

	/* Get base64-encoded fingerprint */
	char *encoded = NULL;
	if (chromaprint_get_fingerprint(ctx, &encoded) && encoded) {
		out->encoded = strdup(encoded);
		chromaprint_dealloc(encoded);
	}

	out->duration_ms = duration_ms;

	chromaprint_free(ctx);
	return 0;
}

int fingerprint_from_pcm(const int16_t *samples, size_t total_frames,
                         int sample_rate, int channels,
                         fingerprint_result_t *out)
{
	return fingerprint_from_pcm_prefix(samples, total_frames, total_frames,
	                                   sample_rate, channels, out);
}

int fingerprint_from_audio_file(const char *path, fingerprint_result_t *out)
{
	if (!path || !out) return -1;

	pcm_decode_result_t pcm = { 0 };
	if (pcm_decode_file_prefix(path, FINGERPRINT_MAX_SECONDS, &pcm) != 0) {
		LOGE("Failed to decode audio: %s", path);
		return -1;
	}

	int rc = fingerprint_from_pcm_prefix(pcm.pcm_data, pcm.pcm_samples,
	                                     pcm.total_samples, pcm.sample_rate,
	                                     pcm.channels, out);
	pcm_decode_free(&pcm);
	return rc;
}

int fingerprint_from_cd_sectors(const uint8_t *sector_data, int prefix_sector_count,
                                int audio_sector_count,
                                fingerprint_result_t *out)
{
	size_t available_frames;
	size_t total_frames;
	int required_sectors;

	if (!sector_data || prefix_sector_count <= 0 || audio_sector_count <= 0 || !out)
		return -1;
	required_sectors = audio_sector_count < FINGERPRINT_MAX_CD_SECTORS
	                       ? audio_sector_count
	                       : FINGERPRINT_MAX_CD_SECTORS;
	if (prefix_sector_count < required_sectors ||
	    (size_t) prefix_sector_count > SIZE_MAX / 588 ||
	    (size_t) audio_sector_count > SIZE_MAX / 588)
		return -1;
	available_frames = (size_t) prefix_sector_count * 588;
	total_frames = (size_t) audio_sector_count * 588;
	return fingerprint_from_pcm_prefix((const int16_t *) sector_data,
	                                   available_frames, total_frames,
	                                   44100, 2, out);
}

void fingerprint_free(fingerprint_result_t *fp)
{
	if (!fp) return;
	free(fp->raw_fp);
	free(fp->encoded);
	memset(fp, 0, sizeof(*fp));
}
