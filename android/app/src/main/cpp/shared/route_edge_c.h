#ifndef DXX_ROUTE_EDGE_C_H
#define DXX_ROUTE_EDGE_C_H

#include "level_metadata_scan.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct route_edge_shadow_summary {
	int compared_edge_count;
	int mismatch_count;
	int first_mismatch_segment;
	int first_mismatch_side;
	int first_legacy_cost;
	int first_shared_cost;
} route_edge_shadow_summary;

int route_edge_compare_view(
    const level_metadata_scan_view *view,
    route_edge_shadow_summary *summary,
    char *problem,
    int problem_capacity);

#ifdef __cplusplus
}
#endif

#endif
