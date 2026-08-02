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

typedef struct fingerprint_stream fingerprint_stream_t;

/* Incrementally fingerprint complete PCM input without retaining it all. */
fingerprint_stream_t *fingerprint_stream_new(int sample_rate, int channels);
int fingerprint_stream_feed(fingerprint_stream_t *stream,
                            const int16_t *samples, size_t frames);
int fingerprint_stream_finish(fingerprint_stream_t *stream,
                              fingerprint_result_t *out);
void fingerprint_stream_free(fingerprint_stream_t *stream);

/*
 * Generate a fingerprint from interleaved 16-bit PCM data.
 * Returns 0 on success, -1 on failure.
 */
int fingerprint_from_pcm(const int16_t *samples, size_t total_frames,
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
 * sector_data: all raw 2352-byte sectors in the track.
 * Returns 0 on success, -1 on failure.
 */
int fingerprint_from_cd_sectors(const uint8_t *sector_data, int sector_count,
                                fingerprint_result_t *out);

#define FINGERPRINT_CD_SECTORS_PER_SECOND 75

/*
 * Free all heap allocations in a fingerprint_result_t.
 */
void fingerprint_free(fingerprint_result_t *fp);

#ifdef __cplusplus
}
#endif

#endif /* FINGERPRINT_GEN_H */
