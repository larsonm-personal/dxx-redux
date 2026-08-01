/*
 * fingerprint_gen.h -- Generate Chromaprint fingerprints from audio data.
 *
 * Wraps the Chromaprint C API into convenience functions that accept
 * pre-decoded PCM or audio file paths.
 */

#ifndef FINGERPRINT_GEN_H
#define FINGERPRINT_GEN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint32_t *raw_fp; /* raw fingerprint array (caller must free via fingerprint_free) */
	int fp_len;       /* number of uint32's in raw_fp */
	int duration_ms;  /* total duration in milliseconds */
	char *encoded;    /* base64-encoded fingerprint string (caller frees via fingerprint_free) */
} fingerprint_result_t;

/*
 * Generate a fingerprint from interleaved 16-bit PCM data.
 * Returns 0 on success, -1 on failure.
 */
int fingerprint_from_pcm(const int16_t *samples, size_t total_frames,
                         int sample_rate, int channels,
                         fingerprint_result_t *out);

/* Calculate the exact fpcalc-compatible 120-second feed size and full duration. */
int fingerprint_pcm_window(size_t total_frames, int sample_rate, int channels,
                           int *feed_samples, int *duration_ms);

/* Fingerprint a retained PCM prefix while reporting the full source duration. */
int fingerprint_from_pcm_prefix(const int16_t *samples, size_t available_frames,
                                size_t total_frames, int sample_rate, int channels,
                                fingerprint_result_t *out);

/*
 * Generate a fingerprint from a decoded audio file (mp3/ogg/flac).
 * Uses pcm_decoders to decode the file first.
 * Returns 0 on success, -1 on failure.
 */
int fingerprint_from_audio_file(const char *path, fingerprint_result_t *out);

/*
 * Generate a fingerprint from raw CD-DA sectors.
 * sector_data: a retained prefix of raw 2352-byte sectors.
 * Returns 0 on success, -1 on failure.
 */
int fingerprint_from_cd_sectors(const uint8_t *sector_data, int prefix_sector_count,
                                int audio_sector_count,
                                fingerprint_result_t *out);

#define FINGERPRINT_CD_SECTORS_PER_SECOND 75
#define FINGERPRINT_MAX_SECONDS           120
#define FINGERPRINT_MAX_CD_SECTORS \
	(FINGERPRINT_CD_SECTORS_PER_SECOND * FINGERPRINT_MAX_SECONDS)

/*
 * Free all heap allocations in a fingerprint_result_t.
 */
void fingerprint_free(fingerprint_result_t *fp);

#ifdef __cplusplus
}
#endif

#endif /* FINGERPRINT_GEN_H */
