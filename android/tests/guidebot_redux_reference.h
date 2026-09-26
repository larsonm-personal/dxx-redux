/* Test-only pinned Redux declarations; see guidebot_redux_reference.c */
#pragma once
#include "ai.h"
int redux_segment_is_reachable(int curseg, int sidenum);
void redux_create_bfs_list(int start_seg, short bfs_list[], int *length, int max_segs);
int redux_exists_in_mine_2(int segnum, int objtype, int objid, int special);
int redux_exists_in_mine(int start_seg, int objtype, int objid, int special);
int redux_find_exit_segment(void);
int redux_get_boss_id(void);
void redux_escort_create_path_to_goal(object *objp);
int redux_escort_set_goal_object(void);
int redux_time_to_visit_player(object *objp, ai_local *ailp, ai_static *aip);
void redux_do_escort_frame(object *objp, fix dist_to_player, int player_visibility);
int redux_marker_exists_in_mine(int id);
void redux_set_escort_special_goal(int special_key);
void redux_detect_escort_goal_accomplished(int index);
void redux_create_random_xlate(sbyte *xt);
void redux_insert_center_points(point_seg *psegs, int *num_points);
void redux_move_towards_outside(point_seg *psegs, int *num_points, object *objp, int rand_flag);
int redux_create_path_points(object *objp, int start_seg, int end_seg, point_seg *psegs, short *num_points, int max_depth, int random_flag, int safety_flag, int avoid_seg);
int redux_polish_path(object *objp, point_seg *psegs, int num_points);
void redux_create_path_to_player(object *objp, int max_length, int safety_flag);
void redux_create_path_to_segment(object *objp, int goalseg, int max_length, int safety_flag);
void redux_create_path_to_station(object *objp, int max_length);
void redux_create_n_segment_path(object *objp, int path_length, int avoid_seg);
void redux_move_object_to_goal(object *objp, vms_vector *goal_point, int goal_seg);
void redux_ai_follow_path(object *objp, int player_visibility, int previous_visibility, vms_vector *vec_to_player);
void redux_ai_path_set_orient_and_vel(object *objp, vms_vector *goal_point, int player_visibility, vms_vector *vec_to_player);
int redux_ai_door_is_openable(object *objp, segment *segp, int sidenum);
