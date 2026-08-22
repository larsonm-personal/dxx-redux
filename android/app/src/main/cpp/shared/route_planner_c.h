#ifndef DXX_ROUTE_PLANNER_C_H
#define DXX_ROUTE_PLANNER_C_H

#include "level_metadata_scan.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif

enum route_planner_endpoint_kind {
	ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL = 0,
	ROUTE_PLANNER_ENDPOINT_UNEXPLORED = 1,
	ROUTE_PLANNER_ENDPOINT_SEGMENT = 2
};

typedef struct route_planner_plan_summary {
	int endpoint_kind;
	int route_step_count;
	int first_pending_step;
	int first_pending_path_segment_count;
	int first_pending_path_terminal_segment;
	int partial_frontier_segment;
} route_planner_plan_summary;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

int route_planner_plan_view(
    const level_metadata_scan_view *view,
    int endpoint_kind,
    int target_segment,
    level_metadata_state *state,
    level_metadata_unexplored_route *unexplored,
    route_planner_plan_summary *summary,
    char *problem,
    int problem_capacity);

int route_planner_segment_reachable_view(
    const level_metadata_scan_view *view,
    int target_segment);

#ifdef __cplusplus
}
#endif

#endif
