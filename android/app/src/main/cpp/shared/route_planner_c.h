#ifndef DXX_ROUTE_PLANNER_C_H
#define DXX_ROUTE_PLANNER_C_H

#include "level_metadata_scan.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif

typedef struct route_planner_shadow_summary {
	int compared_progress_state_count;
	int compared_node_count;
	int mismatch_count;
	int first_mismatch_progress_state;
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
	int compared_target_count;
	int target_mismatch_count;
	int first_target_category;
	int first_target_index;
	int first_legacy_target_count;
	int first_shared_target_count;
	int first_legacy_target_segment;
	int first_shared_target_segment;
	int first_legacy_target_pos[3];
	int first_shared_target_pos[3];
	int compared_target_selection_count;
	int target_selection_mismatch_count;
	int first_selection_progress_state;
	int first_legacy_selection_index;
	int first_shared_selection_index;
	int first_legacy_selection_progress_weight;
	int first_shared_selection_progress_weight;
	double first_legacy_selection_distance;
	double first_shared_selection_distance;
	int compared_key_selection_count;
	int key_selection_mismatch_count;
	int first_key_selection_progress_state;
	int first_key_selection_key;
	int first_legacy_key_selection_index;
	int first_shared_key_selection_index;
	int first_legacy_key_selection_progress_weight;
	int first_shared_key_selection_progress_weight;
	double first_legacy_key_selection_distance;
	double first_shared_key_selection_distance;
	int compared_trigger_source_edge_count;
	int compared_trigger_source_count;
	int trigger_source_mismatch_count;
	int first_trigger_source_progress_state;
	int first_trigger_source_segment;
	int first_trigger_source_side;
	int first_trigger_source_index;
	int first_legacy_trigger_source_count;
	int first_shared_trigger_source_count;
	int first_legacy_trigger_source_wall;
	int first_shared_trigger_source_wall;
	int first_legacy_trigger_source_trigger;
	int first_shared_trigger_source_trigger;
	int first_legacy_trigger_source_segment;
	int first_shared_trigger_source_segment;
	int first_legacy_trigger_source_side;
	int first_shared_trigger_source_side;
	int first_legacy_trigger_source_pos[3];
	int first_shared_trigger_source_pos[3];
} route_planner_shadow_summary;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

int route_planner_compare_view(
    const level_metadata_scan_view *view,
    route_planner_shadow_summary *summary,
    char *problem,
    int problem_capacity);

#ifdef __cplusplus
}
#endif

#endif
