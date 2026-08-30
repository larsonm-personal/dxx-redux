#ifndef DXX_MIDI_METADATA_PHYSFS_H
#define DXX_MIDI_METADATA_PHYSFS_H

#include "midi_metadata.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MIDI_METADATA_SOURCE_FILENAME_BYTES 256u

int midi_metadata_read_physfs(const char *filename, midi_metadata *metadata);
int midi_metadata_resolve_physfs(const char *filename, midi_metadata *metadata,
                                 char *source_filename, size_t source_filename_size,
                                 int *inherited_from_midi);

#ifdef __cplusplus
}
#endif

#endif
