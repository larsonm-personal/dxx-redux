#ifndef DXX_GUIDEBOT_ROUTE_CERTIFIER_H
#define DXX_GUIDEBOT_ROUTE_CERTIFIER_H

#include "guidebot_route_decision.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct guidebot_route_certifier_workspace {
	unsigned char reachable[LEVEL_METADATA_MAX_SEGMENTS];
	int queue[LEVEL_METADATA_MAX_SEGMENTS];
} guidebot_route_certifier_workspace;

typedef struct guidebot_route_certifier_summary {
	int selected_step;
	int selected_segment;
	int used_prepared_fallback;
	unsigned int visited_segments;
	unsigned int evaluated_edges;
	unsigned int evaluated_actions;
	unsigned int rejected_actions;
} guidebot_route_certifier_summary;

int guidebot_route_side_passable_current(
    const level_metadata_scan_view *view,
    int segment,
    int side);
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
