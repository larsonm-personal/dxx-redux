#include "sound_trace_fingerprint.h"
#include <stdio.h>
#include <string.h>

static int expect(int condition, const char *message)
{
	if (condition)
		return 0;
	fprintf(stderr, "%s\n", message);
	return 1;
}

int main(void)
{
	unsigned char live[] = "hello";
	unsigned char mixed[] = { 0, 3, 8, 19, 32, 127 };
	sound_trace_fingerprint loaded = sound_trace_fingerprint_bytes(live, 5);
	sound_trace_fingerprint cached_input = loaded;
	sound_trace_fingerprint cached_output = sound_trace_fingerprint_bytes(mixed, sizeof(mixed));
	sound_trace_fingerprint changed;
	int failures = 0;
	failures += expect(loaded.valid && loaded.hash == UINT64_C(0xa430d84680aabd0b),
	                   "fingerprint must match the published FNV-1a 64-bit hello vector");
	live[2] ^= 1;
	changed = sound_trace_fingerprint_bytes(live, 5);
	failures += expect(sound_trace_fingerprint_match(changed, loaded) == 0,
	                   "changed live sample must differ from the load snapshot");
	failures += expect(sound_trace_fingerprint_match(changed, cached_input) == 0 &&
	                       sound_trace_fingerprint_match(cached_output,
	                           sound_trace_fingerprint_bytes(mixed, sizeof(mixed))) == 1,
	                   "stale mixer input must be distinguishable from damaged mixer output");
	mixed[3] ^= 1;
	failures += expect(sound_trace_fingerprint_match(cached_output,
	                       sound_trace_fingerprint_bytes(mixed, sizeof(mixed))) == 0,
	                   "changed converted bytes must differ from the conversion snapshot");
	memcpy(live, "hello", 5);
	failures += expect(sound_trace_fingerprint_match(loaded, sound_trace_fingerprint_bytes(live, 5)) == 1,
	                   "restored bytes must match independently of pointer identity");
	failures += expect(sound_trace_fingerprint_match(loaded, sound_trace_fingerprint_bytes(live, 4)) == 0,
	                   "length changes must not compare equal");
	failures += expect(sound_trace_fingerprint_match(loaded, sound_trace_fingerprint_bytes(NULL, 5)) == -1 &&
	                       !sound_trace_fingerprint_bytes((void *) -1, 5).valid &&
	                       !sound_trace_fingerprint_bytes(live, 0).valid &&
	                       !sound_trace_fingerprint_bytes(live, SOUND_TRACE_MAX_BYTES + 1u).valid,
	                   "missing, sentinel, empty and excessive buffers must remain unknown without reads");
	if (!failures)
		puts("Sound trace fingerprint tests passed");
	return failures != 0;
}
