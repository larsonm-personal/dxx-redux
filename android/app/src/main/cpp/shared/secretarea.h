#ifndef _SECRETAREA_H
#define _SECRETAREA_H

#include "level_metadata_scan.h"
#include "guidebot_route_decision.h"
#include "route_snapshot_c.h"
#include "route_planner_c.h"
#include "route_analysis_cache.h"
#include "secret_area_scan.h"
#include "rewind_file.h"

#define LEVEL_METADATA_READINESS_CALCULATING 0
#define LEVEL_METADATA_READINESS_NEXT_READY  1
#define LEVEL_METADATA_READINESS_COMPLETE    2
#define LEVEL_METADATA_READINESS_PARTIAL     3
#define LEVEL_METADATA_READINESS_FAILED      4

#define LEVEL_METADATA_ROUTE_PROVENANCE_NONE              0
#define LEVEL_METADATA_ROUTE_PROVENANCE_CERTIFIER         1
#define LEVEL_METADATA_ROUTE_PROVENANCE_FULL_PLANNER      3
#define LEVEL_METADATA_ROUTE_PROVENANCE_COMPILED_SELECTOR 4

typedef struct level_metadata_live_work_summary {
	unsigned int full_plan_calls;
	unsigned int blocked_full_plan_calls;
	unsigned int deferred_refreshes;
	unsigned int retained_incumbents;
	unsigned int deferred_without_incumbent;
	unsigned int route_ticks;
	unsigned int deferred_ticks;
	unsigned int completed_ticks;
	unsigned int tick_overruns;
	unsigned int compiled_selector_calls;
	unsigned int compiled_selector_successes;
	unsigned int compiled_selector_failures;
	unsigned int compiled_selector_max_evaluated_actions;
	unsigned int pending_event_mask;
	int pending;
	int reachability_cursor;
	int firing_candidate_cursor;
	int firing_candidate_pass;
	int unexplored_candidate_cursor;
	unsigned long long last_tick_us;
	unsigned long long max_tick_us;
	unsigned long long last_refresh_us;
	unsigned long long max_refresh_us;
	unsigned int refresh_overruns;
} level_metadata_live_work_summary;

void secret_area_rescan_current_level(void);
void secret_area_prepare_current_level(void);
int level_metadata_try_load_pending_cache(void);
void level_metadata_note_background_result(int success);
int level_metadata_get_route_readiness(void);
const char *level_metadata_route_readiness_name(int readiness);
void level_metadata_rescan_current_level(void);
void level_metadata_rescan_current_level_from_object(int objnum);
void level_metadata_rescan_route_from_object(int objnum);
void level_metadata_rescan_route_to_segment_from_object(int objnum, int target_seg);
int level_metadata_rescan_unexplored_route_from_object(int objnum, level_metadata_unexplored_route *result);
int level_metadata_wall_shootable_from_position(int seg, const int from_pos[3], int wall_num);
int level_metadata_wall_potentially_shootable_from_position(int seg, const int from_pos[3], int wall_num);
int level_metadata_target_visible_from_position(int seg, const int from_pos[3], int target_seg, const int target_pos[3]);
int level_metadata_get_visibility_cache_summary(level_metadata_visibility_cache_summary *summary);
int level_metadata_get_route_start_objnum(void);
int level_metadata_get_route_start_seg(void);
const secret_area_state *secret_area_get_state(void);
const level_metadata_state *level_metadata_get_state(void);
const level_metadata_state *level_metadata_get_canonical_state(void);
int level_metadata_get_canonical_route_plan_summary(route_planner_plan_summary *summary);
int level_metadata_get_route_analysis_cache_summary(route_analysis_cache_summary *summary);
int level_metadata_get_live_work_summary(
    level_metadata_live_work_summary *summary);
int level_metadata_live_route_work_pending(void);
void level_metadata_invalidate_live_route_work(void);
void level_metadata_set_live_work_pending_event_mask(
    unsigned int event_mask);
const level_metadata_state *level_metadata_get_live_route_state(void);
int level_metadata_get_live_route_plan_summary(route_planner_plan_summary *summary);
int level_metadata_get_live_route_decision(guidebot_route_decision *decision);
int level_metadata_get_live_route_provenance(void);
const char *level_metadata_route_provenance_name(int provenance);
void level_metadata_set_route_shadow_enabled(int enabled);
int level_metadata_get_route_shadow_summary(
    guidebot_route_shadow_summary *summary);
int level_metadata_get_canonical_route_snapshot(route_snapshot_summary *summary);
int level_metadata_get_live_route_snapshot(route_snapshot_summary *summary);
int level_metadata_route_audit_domain(
    int start_objnum, int domain, unsigned int *work_units);
int level_metadata_validate_live_route_certificate(
    int start_objnum, unsigned int *work_units);
int level_metadata_prepare_guidebot_path_view(int start_objnum);
int level_metadata_get_exit_route_step_current(
    int start_objnum,
    level_metadata_route_step *step,
    int *step_index,
    int *target_segment);
int level_metadata_guidebot_side_passable_current(int segment, int side);
int level_metadata_guidebot_route_frontier_current(
    int start_segment,
    int goal_segment,
    int max_depth,
    int avoid_from,
    int avoid_to,
    int avoid_from2,
    int avoid_to2);
int level_metadata_guidebot_route_deferred_countdown_frontier_current(
    int start_segment,
    int goal_segment,
    int max_depth,
    int avoid_from,
    int avoid_to,
    int avoid_from2,
    int avoid_to2);
int secret_area_note_segment_entered(int segnum);
void secret_area_restore_saved_found(int saved_total, const unsigned char *found, int found_capacity, const unsigned char *visited, int visited_count);
void secret_area_restore_found_from_automap(const unsigned char *visited, int visited_count);
void secret_area_write_runtime_state(rewind_file *fp);
void secret_area_read_runtime_state(rewind_file *fp, int swap);
int secret_area_get_reveal_unfound(void);
void secret_area_set_reveal_unfound(int reveal);
int level_metadata_get_objective_mode(void);
const char *level_metadata_objective_mode_name(int mode);
void level_metadata_set_objective_mode(int mode);
void level_metadata_cycle_objective_mode(void);
void level_metadata_set_analysis_fvi_limit(unsigned int limit);
unsigned int level_metadata_get_analysis_fvi_count(void);
void level_metadata_set_persistent_cache_enabled(int enabled);
void level_metadata_set_defer_guidebot_accessibility(int defer);
int level_metadata_get_switch_projectile_radius(void);
void level_metadata_set_switch_projectile_radius_override(int radius);
void level_metadata_choose_level_display_name(
    const char *level_file, const char *current_level_name,
    char *display_name, int display_name_capacity);
unsigned int level_metadata_get_visibility_checkpoint_sequence(void);
unsigned int level_metadata_get_route_revision(void);

#endif
