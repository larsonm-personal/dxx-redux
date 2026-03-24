/*
 * fingerprint_gen.c -- Generate Chromaprint fingerprints from audio data.
 *
 * Feeds decoded PCM into Chromaprint to produce raw and base64-encoded
 * fingerprints.  Provides convenience wrappers for file-based and
 * CD-sector-based inputs.
 */

#include "fingerprint_gen.h"
#include "pcm_decoders.h"

#include <stdlib.h>
#include <string.h>
#include <android/log.h>
#include <chromaprint.h>

#define LOG_TAG   "fingerprint_gen"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* Chromaprint wants at most ~120s of audio; clamp to avoid excess work */
#define MAX_FINGERPRINT_SAMPLES (120 * 44100 * 2) /* 120s, stereo, 44100 Hz */

int fingerprint_from_pcm(const int16_t *samples, int total_samples,
                         int sample_rate, int channels,
                         fingerprint_result_t *out)
{
	if (!samples || total_samples <= 0 || !out) return -1;

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

	int feed_samples = total_samples;
	if (feed_samples > MAX_FINGERPRINT_SAMPLES)
		feed_samples = MAX_FINGERPRINT_SAMPLES;

	if (!chromaprint_feed(ctx, samples, feed_samples)) {
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

	/* Duration */
	out->duration_ms = (int) ((long long) total_samples * 1000LL /
	                          ((long long) sample_rate * channels));

	chromaprint_free(ctx);
	return 0;
}

int fingerprint_from_audio_file(const char *path, fingerprint_result_t *out)
{
	if (!path || !out) return -1;

	pcm_decode_result_t pcm = { 0 };
	if (pcm_decode_file(path, &pcm) != 0) {
		LOGE("Failed to decode audio: %s", path);
		return -1;
	}

	int rc = fingerprint_from_pcm(pcm.pcm_data, pcm.total_samples,
	                              pcm.sample_rate, pcm.channels, out);
	pcm_decode_free(&pcm);
	return rc;
}

int fingerprint_from_cd_sectors(const uint8_t *sector_data, int audio_sector_count,
                                fingerprint_result_t *out)
{
	if (!sector_data || audio_sector_count <= 0 || !out) return -1;

	pcm_decode_result_t pcm = { 0 };
	if (pcm_decode_cd_sectors_buf(sector_data, audio_sector_count, &pcm) != 0) {
		LOGE("Failed to decode CD sectors");
		return -1;
	}

	int rc = fingerprint_from_pcm(pcm.pcm_data, pcm.total_samples,
	                              pcm.sample_rate, pcm.channels, out);
	pcm_decode_free(&pcm);
	return rc;
}

void fingerprint_free(fingerprint_result_t *fp)
{
	if (!fp) return;
	free(fp->raw_fp);
	free(fp->encoded);
	memset(fp, 0, sizeof(*fp));
}
