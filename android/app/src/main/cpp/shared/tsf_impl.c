/* TinySoundFont + TinyMidiLoader — shared library implementation.
 * Built as libtsf.so, linked by both game .so files via arch_sdl.
 * Consumer (digi_tsf_music.c) includes headers without IMPLEMENTATION defines. */

#define TSF_IMPLEMENTATION
#define TSF_NO_STDIO
#include "tsf.h"

#define TML_IMPLEMENTATION
#define TML_NO_STDIO
#include "tml.h"

#include "music_soundfont.h"

tsf *music_soundfont_load_memory(const void *data, size_t size)
{
	size_t samples = music_soundfont_validate(data, size);
	tsf *synth;
	int p, r;
	if (!samples) return NULL;
	synth = tsf_load_memory(data, (int) size);
	if (!synth) return NULL;
	for (p = 0; p < synth->presetNum; ++p) {
		for (r = 0; r < synth->presets[p].regionNum; ++r) {
			const struct tsf_region *region = &synth->presets[p].regions[r];
			/* A sample-start offset can begin partway through a loop (GeneralUser's
			 * Soundtrack preset does this). The loop may wrap before that initial
			 * offset, provided all resolved positions stay inside the sample buffer */
			if (region->offset >= region->end || region->end > samples ||
			    (region->loop_mode && (region->loop_start >= region->loop_end || region->loop_end >= region->end))) {
				tsf_close(synth);
				return NULL;
			}
		}
	}
	return synth;
}
