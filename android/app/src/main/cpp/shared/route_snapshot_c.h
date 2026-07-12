#ifndef DXX_ROUTE_SNAPSHOT_C_H
#define DXX_ROUTE_SNAPSHOT_C_H

#include "level_metadata_scan.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif

typedef struct route_snapshot_summary {
	unsigned long long topology_hash;
	unsigned long long state_hash;
	unsigned long long start_hash;
	unsigned long long progression_hash;
	unsigned long long navigation_hash;
	unsigned long long trigger_hash;
	unsigned long long object_hash;
	unsigned long long automap_hash;
	int segment_count;
	int wall_count;
	int trigger_count;
	int object_count;
	int start_segment;
	int key_mask;
	int control_center_destroyed;
	unsigned int topology_generation;
	unsigned int start_generation;
	unsigned int progression_generation;
	unsigned int navigation_generation;
	unsigned int trigger_generation;
	unsigned int object_generation;
	unsigned int automap_generation;
} route_snapshot_summary;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

int route_snapshot_build_summary(const level_metadata_scan_view *view,
                                 route_snapshot_summary *summary,
                                 char *problem,
                                 int problem_capacity);

#ifdef __cplusplus
}
#endif

#endif
