#ifndef D2_GUIDEBOT_ROUTE_INTERNAL_H
#define D2_GUIDEBOT_ROUTE_INTERNAL_H

enum escort_route_target_mode {
	ESCORT_ROUTE_TARGET_END_OF_LEVEL = 0,
	ESCORT_ROUTE_TARGET_UNEXPLORED = 1,
	ESCORT_ROUTE_TARGET_EXIT = 2
};

#ifdef __ANDROID__

#include "level_metadata_scan.h"
#include "vecmat.h"

enum escort_route_guidance_mode {
	ESCORT_ROUTE_GUIDANCE_NONE = 0,
	ESCORT_ROUTE_GUIDANCE_REACH_OBJECTIVE = 1,
	ESCORT_ROUTE_GUIDANCE_REACH_HIDDEN_DOOR = 2,
	ESCORT_ROUTE_GUIDANCE_REACH_FIRING_POSITION = 3,
	ESCORT_ROUTE_GUIDANCE_NEAREST_PROGRESS_POINT = 4
};

#define ESCORT_ROUTE_OBJECTIVE_UNEXPLORED 1000

typedef struct escort_unexplored_route_target {
	int active;
	int component_size;
	int target_seg;
	int waypoint_seg;
	int direct_reachable;
} escort_unexplored_route_target;

typedef struct escort_route_goal {
	int active;
	int target_seg;
	int target_side;
	int target_wall;
	int trigger_num;
	int objective_kind;
	int activation_kind;
	int objective_seg;
	int objective_side;
	int objective_wall;
	int objective_trigger;
	int objective_object;
	int objective_key_index;
	int switch_shot_quality;
	int switch_shot_incidence_cosine;
	int guidance_mode;
	int guidance_seg;
	int guidance_side;
	int frontier_player_keyed_door;
	int target_pos_valid;
	vms_vector target_pos;
	int path_endpoint_seg;
	int path_endpoint_pos_valid;
	vms_vector path_endpoint_pos;
	char label[LEVEL_METADATA_ROUTE_LABEL_LEN];
} escort_route_goal;

extern escort_route_goal Escort_route_goal;
extern escort_unexplored_route_target Escort_unexplored_route_target;
extern int Escort_route_target_mode;
extern int Escort_route_target_mode_restore_pending;
extern int Escort_route_metadata_dirty;
extern int Escort_route_cache_improvement_pending;
extern unsigned int Escort_route_seen_revision;
extern unsigned int Escort_route_metadata_rescan_count;
extern unsigned int Escort_route_guidance_full_search_count;
extern unsigned int Escort_route_ignored_nonowner_key_change_count;
extern unsigned int Escort_route_boss_move_invalidation_count;
extern unsigned int Escort_route_wall_generation;
extern unsigned int Escort_route_trigger_generation;
extern unsigned int Escort_route_object_generation;
extern unsigned int Escort_route_reactor_generation;
extern unsigned int Escort_route_automap_generation;
extern unsigned int Escort_route_pending_event_mask;
extern unsigned int Escort_route_pending_audit_mask;
extern unsigned int Escort_route_deferred_live_event_mask;
extern unsigned int Escort_route_event_notification_count;
extern unsigned int Escort_route_notification_coalesced_count;
extern unsigned int Escort_route_redundant_dirty_domain_count;
extern unsigned int Escort_route_event_coalesced_rescan_count;
extern unsigned int Escort_route_publish_latency_sample_count;
extern unsigned int Escort_route_publish_latency_last_ticks;
extern unsigned int Escort_route_publish_latency_max_ticks;
extern fix64 Escort_route_first_dirty_notification_time;
extern int Escort_route_dirty_notification_time_valid;
extern unsigned int Escort_route_ignored_nonowner_event_count;
extern unsigned int Escort_route_audit_check_count;
extern unsigned int Escort_route_audit_discovery_count;
extern unsigned int Escort_route_audit_only_discovery_count;
extern unsigned int Escort_route_audit_work_total;
extern unsigned int Escort_route_audit_work_max;
extern unsigned int Escort_route_audit_deferred_count;
extern unsigned int Escort_route_certificate_check_count;
extern unsigned int Escort_route_certificate_failure_count;
extern unsigned int Escort_route_certificate_work_total;
extern unsigned int Escort_route_certificate_work_max;
extern unsigned int Escort_route_path_retained_count;
extern unsigned int Escort_route_path_replaced_count;
extern unsigned int Escort_route_invalid_path_stopped_count;
extern unsigned int Escort_route_audit_domain_cursor;
extern fix64 Escort_route_audit_next_time;
extern fix64 Escort_route_last_audit_rescan_time;
extern int Escort_route_last_audit_rescan_time_valid;
extern fix64 Escort_route_completion_check_time;
extern const char *Escort_route_last_replan_reason;
extern int Escort_route_avoid_from_seg;
extern int Escort_route_avoid_seg;
extern int Escort_route_avoid_from_seg2;
extern int Escort_route_avoid_seg2;
extern int Escort_route_avoid_trigger;
extern int Escort_route_avoid_wall;
extern fix64 Escort_route_progress_next_time;
extern int Escort_route_progress_signature;
extern int Escort_route_progress_seg;
extern int Escort_route_progress_path_index;
extern int Escort_route_progress_target_seg;
extern int Escort_route_progress_stall_samples;
extern unsigned int Escort_route_stall_recovery_count;
extern fix64 Escort_route_cache_poll_time;
extern int Escort_route_logged_readiness;

#ifdef INTROSPECT_ON
extern int Escort_route_notifications_suppressed;
extern int Escort_route_certificate_checks_suppressed;
#endif

int escort_is_companion_object(int objnum);
void escort_unexplored_route_target_clear(escort_unexplored_route_target *target);
void escort_trace_path(const char *reason, object *objp, ai_local *ailp,
                       ai_static *aip, int goal_seg);
void escort_trace_navigation(object *objp, ai_local *ailp, ai_static *aip,
                             fix dist_to_player, int player_visibility);
void escort_trace_navigation_reset(const char *reason, object *objp,
                                   ai_local *ailp, ai_static *aip);
void escort_route_set_target_mode(int target_mode);
void escort_route_note_replan(const char *reason);
void escort_route_record_event(unsigned int event_mask,
                               unsigned int *generation,
                               int matches_objective);
void escort_route_sync_target_mode(void);
void escort_route_clear_goal(void);
const char *escort_route_goal_label(void);
int escort_valid_segment(int segnum);
int escort_route_key_flag(int key_index);
int escort_route_next_waypoint_pending(void);
int escort_route_next_goal(void);
int escort_route_adopt_exit_command(void);
int escort_route_physical_target(object *objp, int goal_seg, int max_depth);
void escort_route_refresh_metadata(void);
void escort_route_note_path_endpoint(object *objp);
void escort_route_apply_target_pos(object *objp);

#endif

#endif
