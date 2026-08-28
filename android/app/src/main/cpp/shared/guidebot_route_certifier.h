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
	int firing_cache_valid;
	int firing_cache_num_segments;
	int firing_cache_num_walls;
	int firing_cache_trigger;
	int firing_cache_wall;
	int firing_cache_aim[3];
	int firing_cache_segment;
	int firing_cache_path_segment_count;
	int firing_cache_position[3];
	int firing_cache_shot_quality;
	int firing_cache_incidence_cosine;
	int job_active;
	int job_start_segment;
	int job_num_segments;
	int reach_head;
	int reach_tail;
	int reach_side;
	int reach_complete;
	unsigned int job_visited_segments;
	unsigned int job_evaluated_edges;
	unsigned int job_evaluated_firing_positions;
	int firing_search_active;
	int firing_search_pass;
	int firing_search_segment;
	int firing_search_detailed_count;
	int firing_search_selection_segment;
	int firing_search_selection_complete;
	int firing_search_detailed_index;
	int firing_search_detailed_sample;
	int firing_search_detailed_segments[8];
	long double firing_search_detailed_scores[8];
	int firing_search_original_segment;
	int firing_search_original_position[3];
	int firing_search_best_segment;
	int firing_search_best_path_distance;
	int firing_search_best_position[3];
	int firing_search_best_quality;
	int firing_search_best_incidence;
	long double firing_search_best_score;
	int firing_frontier_active;
	int firing_frontier_phase;
	int firing_frontier_goal_segment;
	int firing_frontier_init_segment;
	int firing_frontier_head;
	int firing_frontier_tail;
	int firing_frontier_side;
	int firing_frontier_scan_segment;
	int firing_frontier_best_segment;
	int firing_frontier_best_remaining;
	int firing_frontier_best_incidence;
	int unexplored_active;
	int unexplored_init_segment;
	int unexplored_scan_segment;
	int unexplored_component_head;
	int unexplored_component_tail;
	int unexplored_component_side;
	int unexplored_component_size;
	int unexplored_component_target;
	int unexplored_component_distance;
	int unexplored_best_size;
	int unexplored_best_target;
	int unexplored_best_distance;
} guidebot_route_certifier_workspace;

typedef unsigned long long (*guidebot_route_certifier_clock)(void *user);

typedef struct guidebot_route_certifier_budget {
	guidebot_route_certifier_clock clock_us;
	void *clock_user;
	unsigned long long deadline_us;
	unsigned int work_limit;
} guidebot_route_certifier_budget;

#define GUIDEBOT_ROUTE_CERTIFIER_INVALID 0
#define GUIDEBOT_ROUTE_CERTIFIER_VALID   1
#define GUIDEBOT_ROUTE_CERTIFIER_PENDING 2

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
	unsigned int evaluated_firing_positions;
	int reranked_firing_position;
	int firing_cache_hit;
	int approximate_firing_position;
	int steep_firing_position;
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
int guidebot_route_select_compiled_current_state(
    const level_metadata_scan_view *view,
    const level_metadata_state *compiled_state,
    const route_planner_plan_summary *compiled_plan,
    level_metadata_state *live_state,
    route_planner_plan_summary *live_plan,
    guidebot_route_validity_certificate *certificate,
    guidebot_route_certifier_summary *summary);
int guidebot_route_certify_current_state(
    const level_metadata_scan_view *view,
    const level_metadata_state *prepared_state,
    const route_planner_plan_summary *prepared_plan,
    guidebot_route_certifier_workspace *workspace,
    level_metadata_state *live_state,
    route_planner_plan_summary *live_plan,
    guidebot_route_validity_certificate *certificate,
    guidebot_route_certifier_summary *certifier_summary);
int guidebot_route_certify_current_state_budgeted(
    const level_metadata_scan_view *view,
    const level_metadata_state *prepared_state,
    const route_planner_plan_summary *prepared_plan,
    guidebot_route_certifier_workspace *workspace,
    level_metadata_state *live_state,
    route_planner_plan_summary *live_plan,
    guidebot_route_validity_certificate *certificate,
    guidebot_route_certifier_summary *certifier_summary,
    const guidebot_route_certifier_budget *budget);
int guidebot_route_find_unexplored_budgeted(
    const level_metadata_scan_view *view,
    guidebot_route_certifier_workspace *workspace,
    level_metadata_unexplored_route *result,
    const guidebot_route_certifier_budget *budget);
void guidebot_route_certifier_reset_job(
    guidebot_route_certifier_workspace *workspace);

#ifdef __cplusplus
}
#endif

#endif
