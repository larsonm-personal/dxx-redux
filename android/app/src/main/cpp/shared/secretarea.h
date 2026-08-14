#ifndef _SECRETAREA_H
#define _SECRETAREA_H

#include "level_metadata_scan.h"
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
int level_metadata_target_visible_from_position(int seg, const int from_pos[3], int target_seg, const int target_pos[3]);
int level_metadata_get_visibility_cache_summary(level_metadata_visibility_cache_summary *summary);
int level_metadata_get_route_start_objnum(void);
int level_metadata_get_route_start_seg(void);
const secret_area_state *secret_area_get_state(void);
const level_metadata_state *level_metadata_get_state(void);
const level_metadata_state *level_metadata_get_canonical_state(void);
int level_metadata_get_canonical_route_plan_summary(route_planner_plan_summary *summary);
int level_metadata_get_route_analysis_cache_summary(route_analysis_cache_summary *summary);
void level_metadata_mark_route_objective_completed(
    int kind, int trigger, int wall, int key_index);
const level_metadata_state *level_metadata_get_live_route_state(void);
int level_metadata_get_live_route_plan_summary(route_planner_plan_summary *summary);
int level_metadata_get_canonical_route_snapshot(route_snapshot_summary *summary);
int level_metadata_get_live_route_snapshot(route_snapshot_summary *summary);
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
unsigned int level_metadata_get_visibility_checkpoint_sequence(void);
unsigned int level_metadata_get_route_revision(void);

#endif
