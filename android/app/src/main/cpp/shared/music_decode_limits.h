#ifndef MUSIC_DECODE_LIMITS_H
#define MUSIC_DECODE_LIMITS_H

#include <stdint.h>
#include <stddef.h>

#define MUSIC_PCM_ENCODED_MAX_BYTES  (64u * 1024u * 1024u)
#define MUSIC_MIDI_ENCODED_MAX_BYTES (16u * 1024u * 1024u)
#define MUSIC_PCM_DECODED_MAX_BYTES  (128u * 1024u * 1024u)
#define MUSIC_PCM_MAX_EXPANSION      1024u

static inline int music_encoded_size_allowed(int64_t declared_size,
                                             size_t maximum, size_t *size)
{
	if (declared_size <= 0 || (uint64_t) declared_size > (uint64_t) maximum ||
	    (uint64_t) declared_size > (uint64_t) SIZE_MAX)
		return 0;
	if (size)
		*size = (size_t) declared_size;
	return 1;
}

static inline int music_pcm_budget_allows(size_t encoded_bytes,
                                          size_t frames, int channels)
{
	size_t decoded_bytes;

	if (!encoded_bytes || !frames || (channels != 1 && channels != 2) ||
	    frames > SIZE_MAX / (size_t) channels / sizeof(int16_t))
		return 0;
	decoded_bytes = frames * (size_t) channels * sizeof(int16_t);
	if (decoded_bytes > MUSIC_PCM_DECODED_MAX_BYTES)
		return 0;
	return decoded_bytes / encoded_bytes < MUSIC_PCM_MAX_EXPANSION ||
	       (decoded_bytes / encoded_bytes == MUSIC_PCM_MAX_EXPANSION &&
	        decoded_bytes % encoded_bytes == 0);
}

#endif
