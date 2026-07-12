#ifndef _SECRETAREA_H
#define _SECRETAREA_H

#include "level_metadata_scan.h"
#include "route_snapshot_c.h"
#include "route_edge_c.h"
#include "route_planner_c.h"
#include "secret_area_scan.h"

void secret_area_rescan_current_level(void);
void level_metadata_rescan_current_level(void);
void level_metadata_rescan_current_level_from_object(int objnum);
void level_metadata_rescan_route_from_object(int objnum);
void level_metadata_rescan_route_to_segment_from_object(int objnum, int target_seg);
int level_metadata_rescan_unexplored_route_from_object(int objnum, level_metadata_unexplored_route *result);
int level_metadata_route_edge_cost_from_object(int objnum, int seg, int side);
int level_metadata_wall_visible_from_position(int seg, const int from_pos[3], int wall_num);
int level_metadata_get_route_start_objnum(void);
int level_metadata_get_route_start_seg(void);
const secret_area_state *secret_area_get_state(void);
const level_metadata_state *level_metadata_get_state(void);
const level_metadata_state *level_metadata_get_canonical_state(void);
int level_metadata_get_canonical_route_snapshot(route_snapshot_summary *summary);
int level_metadata_get_live_route_snapshot(route_snapshot_summary *summary);
int level_metadata_get_route_edge_shadow(route_edge_shadow_summary *summary);
int level_metadata_get_route_planner_shadow(route_planner_shadow_summary *summary);
int secret_area_note_segment_entered(int segnum);
void secret_area_restore_saved_found(int saved_total, const unsigned char *found, int found_capacity, const unsigned char *visited, int visited_count);
void secret_area_restore_found_from_automap(const unsigned char *visited, int visited_count);
int secret_area_get_reveal_unfound(void);
void secret_area_set_reveal_unfound(int reveal);
int level_metadata_get_show_objectives(void);
void level_metadata_set_show_objectives(int show);

#endif
