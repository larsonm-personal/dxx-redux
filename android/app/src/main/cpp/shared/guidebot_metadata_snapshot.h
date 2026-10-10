#ifndef GUIDEBOT_METADATA_SNAPSHOT_H
#define GUIDEBOT_METADATA_SNAPSHOT_H

#include "secretarea.h"
#include "guidebot_route_certifier.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Owned, pointer-free process-local state, never a disk/network representation */
typedef struct guidebot_metadata_snapshot {
#define METADATA_STRUCT(type, name, serializer) type name;
#define METADATA_VALUE(type, name, kind)        type name;
#include "guidebot_metadata_save_members.h"
#undef METADATA_STRUCT
#undef METADATA_VALUE
} guidebot_metadata_snapshot;

/* Capture is game-thread only; codec operations use only their arguments */
void level_metadata_capture_snapshot(guidebot_metadata_snapshot *snapshot);
size_t guidebot_metadata_snapshot_encoded_size(void);
int guidebot_metadata_snapshot_encode(const guidebot_metadata_snapshot *snapshot,
                                      void *data, size_t size, int64_t epoch);
/* Rejected input leaves the destination unchanged, including on truncation */
int guidebot_metadata_snapshot_decode(guidebot_metadata_snapshot *snapshot,
                                      const void *data, size_t size, int64_t epoch);

#ifdef __cplusplus
}
#endif
#endif
