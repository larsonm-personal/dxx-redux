/*
 * fingerprint_gen.h -- Generate Chromaprint fingerprints from audio data.
 *
 * Wraps the Chromaprint C API into convenience functions that accept
 * pre-decoded PCM or audio file paths.
 */

#ifndef FINGERPRINT_GEN_H
#define FINGERPRINT_GEN_H

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
int fingerprint_from_pcm(const int16_t *samples, int total_samples,
                         int sample_rate, int channels,
                         fingerprint_result_t *out);

/*
 * Generate a fingerprint from a decoded audio file (mp3/ogg/flac).
 * Uses pcm_decoders to decode the file first.
 * Returns 0 on success, -1 on failure.
 */
int fingerprint_from_audio_file(const char *path, fingerprint_result_t *out);

/*
 * Generate a fingerprint from raw CD-DA sectors.
 * sector_data: raw 2352-byte sectors, audio_sector_count: number of sectors.
 * Returns 0 on success, -1 on failure.
 */
int fingerprint_from_cd_sectors(const uint8_t *sector_data, int audio_sector_count,
                                fingerprint_result_t *out);

/*
 * Free all heap allocations in a fingerprint_result_t.
 */
void fingerprint_free(fingerprint_result_t *fp);

#ifdef __cplusplus
}
#endif

#endif /* FINGERPRINT_GEN_H */
