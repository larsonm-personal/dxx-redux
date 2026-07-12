#ifndef DXX_ROUTE_PLANNER_C_H
#define DXX_ROUTE_PLANNER_C_H

#include "level_metadata_scan.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct route_planner_shadow_summary {
	int compared_node_count;
	int mismatch_count;
	int first_mismatch_optimistic;
	int first_mismatch_segment;
	int first_legacy_reachable;
	int first_shared_reachable;
	int first_legacy_progress_weight;
	int first_shared_progress_weight;
	int first_legacy_parent_segment;
	int first_shared_parent_segment;
	int first_legacy_parent_side;
	int first_shared_parent_side;
	double first_legacy_distance;
	double first_shared_distance;
} route_planner_shadow_summary;

int route_planner_compare_view(
    const level_metadata_scan_view *view,
    route_planner_shadow_summary *summary,
    char *problem,
    int problem_capacity);

#ifdef __cplusplus
}
#endif

#endif
