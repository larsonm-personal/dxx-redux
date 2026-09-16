/* Bounded, non-cryptographic fingerprints for diagnostic comparisons */
#ifndef SOUND_TRACE_FINGERPRINT_H
#define SOUND_TRACE_FINGERPRINT_H

#include <stddef.h>
#include <stdint.h>

#define SOUND_TRACE_MAX_BYTES (16u * 1024u * 1024u)

typedef struct sound_trace_fingerprint {
	uint64_t hash;
	size_t length;
	int valid;
} sound_trace_fingerprint;

static inline sound_trace_fingerprint sound_trace_fingerprint_bytes(const void *data, size_t length)
{
	sound_trace_fingerprint result = { 0, length, 0 };
	const unsigned char *bytes = (const unsigned char *) data;
	size_t i;
	if (!data || data == (const void *) -1 || !length || length > SOUND_TRACE_MAX_BYTES)
		return result;
	result.hash = UINT64_C(14695981039346656037);
	for (i = 0; i < length; ++i) {
		result.hash ^= bytes[i];
		result.hash *= UINT64_C(1099511628211);
	}
	result.valid = 1;
	return result;
}

/* Unknown is distinct from a mismatch */
static inline int sound_trace_fingerprint_match(sound_trace_fingerprint a, sound_trace_fingerprint b)
{
	if (!a.valid || !b.valid)
		return -1;
	return a.length == b.length && a.hash == b.hash;
}

#endif
