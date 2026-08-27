#ifndef DXX_GUIDEBOT_ROUTE_CERTIFIER_H
#define DXX_GUIDEBOT_ROUTE_CERTIFIER_H

#include "guidebot_route_decision.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct guidebot_route_certifier_workspace {
	unsigned char reachable[LEVEL_METADATA_MAX_SEGMENTS];
	int queue[LEVEL_METADATA_MAX_SEGMENTS];
	int strategic_distance[LEVEL_METADATA_MAX_SEGMENTS];
	int physical_distance[LEVEL_METADATA_MAX_SEGMENTS];
} guidebot_route_certifier_workspace;

enum guidebot_route_certifier_rejection {
	GUIDEBOT_ROUTE_CERTIFIER_REJECTION_NONE = 0,
	GUIDEBOT_ROUTE_CERTIFIER_REJECTION_INVALID_TARGET = 1,
	GUIDEBOT_ROUTE_CERTIFIER_REJECTION_UNREACHABLE_TARGET = 2
};

typedef struct guidebot_route_certifier_summary {
	int selected_step;
	int selected_segment;
	int blocking_step;
	int blocking_segment;
	int blocking_reason;
	int used_prepared_fallback;
	unsigned long long required_steps_low;
	unsigned int visited_segments;
	unsigned int evaluated_edges;
	unsigned int evaluated_actions;
	unsigned int rejected_actions;
} guidebot_route_certifier_summary;

int guidebot_route_side_passable_current(
    const level_metadata_scan_view *view,
    int segment,
    int side);
int guidebot_route_side_progress_reachable_current(
    const level_metadata_scan_view *view,
    int segment,
    int side);
int guidebot_route_best_physical_frontier(
    const level_metadata_scan_view *view,
    int start_segment,
    int goal_segment,
    int max_depth,
    int avoid_from,
    int avoid_to,
    int avoid_from2,
    int avoid_to2,
    guidebot_route_certifier_workspace *workspace);
int guidebot_route_best_deferred_countdown_frontier(
    const level_metadata_scan_view *view,
    int start_segment,
    int goal_segment,
    int max_depth,
    int avoid_from,
    int avoid_to,
    int avoid_from2,
    int avoid_to2,
    guidebot_route_certifier_workspace *workspace);
int guidebot_route_select_exit_step_current_state(
    const level_metadata_scan_view *view,
    const level_metadata_state *state,
    level_metadata_route_step *selected_step,
    int *selected_index,
    int *selected_segment);
int guidebot_route_certify_current_state(
    const level_metadata_scan_view *view,
    const level_metadata_state *prepared_state,
    const route_planner_plan_summary *prepared_plan,
    guidebot_route_certifier_workspace *workspace,
    level_metadata_state *live_state,
    route_planner_plan_summary *live_plan,
    guidebot_route_validity_certificate *certificate,
    guidebot_route_certifier_summary *certifier_summary);

#ifdef __cplusplus
}
#endif

#endif
