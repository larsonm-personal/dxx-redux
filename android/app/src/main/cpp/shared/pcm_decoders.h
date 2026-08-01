/*
 * pcm_decoders.h -- Unified API for decoding MP3/OGG/FLAC to raw PCM.
 * Uses single-file decoders: minimp3, stb_vorbis, dr_flac.
 * All decoders are public domain / MIT-0.
 */

#ifndef PCM_DECODERS_H
#define PCM_DECODERS_H

#include <stdint.h>
#include <stddef.h>

/* Decoded PCM output.  Caller must free pcm_data with pcm_decode_free(). */
typedef struct {
	int16_t *pcm_data;    /* interleaved 16-bit samples */
	int sample_rate;      /* e.g. 44100 */
	int channels;         /* 1 or 2 */
	size_t total_samples; /* per-channel sample count */
	size_t pcm_samples;   /* per-channel sample count retained in pcm_data */
} pcm_decode_result_t;

/* Decode an audio file to 16-bit PCM.
 * Supported: .mp3, .ogg, .flac (detected by extension).
 * Returns 0 on success, -1 on failure. */
int pcm_decode_file(const char *path, pcm_decode_result_t *out);

/* Decode no more than max_seconds while retaining the full source duration. */
int pcm_decode_file_prefix(const char *path, size_t max_seconds,
                           pcm_decode_result_t *out);

/* Decode audio from an in-memory buffer to 16-bit PCM.
 * ext must include the dot, e.g. ".mp3", ".ogg", ".flac".
 * Returns 0 on success, -1 on failure. */
int pcm_decode_memory(const void *data, size_t size, const char *ext,
                      pcm_decode_result_t *out);
int pcm_decode_memory_prefix(const void *data, size_t size, const char *ext,
                             size_t max_seconds, pcm_decode_result_t *out);

/* Free PCM data allocated by pcm_decode_file() or pcm_decode_memory(). */
void pcm_decode_free(pcm_decode_result_t *r);

/* Decode raw CD-DA sectors (2352 bytes each, 16-bit stereo 44100 Hz)
 * from an open file descriptor.  Reads start_sector..start_sector+num_sectors-1.
 * Output is pre-filled: sample_rate=44100, channels=2.
 * Returns 0 on success, -1 on failure. */
int pcm_decode_cd_sectors(int fd, long start_sector, long num_sectors,
                          pcm_decode_result_t *out);

/* Decode pre-read CD-DA sectors from a memory buffer.
 * sector_data: raw 2352-byte sectors already in memory.
 * Returns 0 on success, -1 on failure. */
int pcm_decode_cd_sectors_buf(const uint8_t *sector_data, int num_sectors,
                              pcm_decode_result_t *out);

#endif /* PCM_DECODERS_H */
