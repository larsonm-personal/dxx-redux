#include "music_decode_limits.h"

#include <limits.h>
#include <stdio.h>

static int fail(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

int main(void)
{
	size_t size = 0;
	size_t stereo_frames_at_limit = MUSIC_PCM_DECODED_MAX_BYTES / (2u * sizeof(int16_t));

	if (music_encoded_size_allowed(-1, MUSIC_PCM_ENCODED_MAX_BYTES, &size) ||
	    music_encoded_size_allowed(0, MUSIC_PCM_ENCODED_MAX_BYTES, &size) ||
	    !music_encoded_size_allowed(MUSIC_PCM_ENCODED_MAX_BYTES,
	                                MUSIC_PCM_ENCODED_MAX_BYTES, &size) ||
	    size != MUSIC_PCM_ENCODED_MAX_BYTES ||
	    music_encoded_size_allowed((int64_t) MUSIC_PCM_ENCODED_MAX_BYTES + 1,
	                               MUSIC_PCM_ENCODED_MAX_BYTES, &size) ||
	    music_encoded_size_allowed(INT64_MAX, MUSIC_PCM_ENCODED_MAX_BYTES, &size))
		return fail("encoded size boundary failed");
	if (!music_pcm_budget_allows(MUSIC_PCM_ENCODED_MAX_BYTES,
	                             stereo_frames_at_limit, 2) ||
	    music_pcm_budget_allows(MUSIC_PCM_ENCODED_MAX_BYTES,
	                            stereo_frames_at_limit + 1u, 2) ||
	    music_pcm_budget_allows(1u, 513u, 1) ||
	    !music_pcm_budget_allows(1u, 512u, 1) ||
	    music_pcm_budget_allows(1024u, 1u, 3))
		return fail("decoded PCM budget boundary failed");
	puts("PASS");
	return 0;
}
