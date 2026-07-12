#ifndef DXX_ROUTE_SNAPSHOT_C_H
#define DXX_ROUTE_SNAPSHOT_C_H

#include "level_metadata_scan.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct route_snapshot_summary {
	unsigned long long topology_hash;
	unsigned long long state_hash;
	int segment_count;
	int wall_count;
	int start_segment;
	int key_mask;
	int control_center_destroyed;
} route_snapshot_summary;

int route_snapshot_build_summary(const level_metadata_scan_view *view,
                                 route_snapshot_summary *summary,
                                 char *problem,
                                 int problem_capacity);

#ifdef __cplusplus
}
#endif

#endif
