#ifndef MUSIC_SOUNDFONT_H
#define MUSIC_SOUNDFONT_H

#include <stddef.h>
#include "tsf.h"

/* Keep the import limit synchronized with SoundfontStore.MAX_BYTES */
#define MUSIC_SOUNDFONT_MAX_BYTES (64u * 1024u * 1024u)

#ifdef __cplusplus
extern "C" {
#endif

struct AAssetManager;

/* Empty path selects bundled gm.sf2; a nonempty path never silently falls back */
tsf *music_soundfont_load(struct AAssetManager *assets, const char *path);
/* Validate SF2 table bounds before entering the pinned synth's parser */
size_t music_soundfont_validate(const void *data, size_t size);
/* Implemented alongside TSF so resolved sample regions can also be checked */
tsf *music_soundfont_load_memory(const void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif
