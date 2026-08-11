#ifndef CLASSIC_DEMO_WALL_VALIDATION_H
#define CLASSIC_DEMO_WALL_VALIDATION_H

#include <stdint.h>

#define CLASSIC_DEMO_WALL_RECORD_BYTES 7u

static inline int classic_demo_wall_records_fit(int wall_count,
                                                int max_walls, int64_t remaining_bytes)
{
	return wall_count >= 0 && wall_count <= max_walls && remaining_bytes >= 0 &&
	       (uint64_t) wall_count <= (uint64_t) remaining_bytes / CLASSIC_DEMO_WALL_RECORD_BYTES;
}

#endif
