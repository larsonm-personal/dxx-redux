/*
 *
 * Header for escort.c
 *
 */

#ifndef _ESCORT_H
#define _ESCORT_H

struct input_demo_checkpoint_escort_state;
struct input_demo_checkpoint_thief_state;

#define GUIDEBOT_NAME_LEN 9
extern void change_guidebot_name(void);
extern void do_escort_menu(void);
extern void detect_escort_goal_accomplished(int index);
extern void invalidate_escort_goal(void);
extern void escort_note_player_key_flags(int old_flags, int new_flags);
extern void escort_note_player_key_flags_for_player(int pnum, int old_flags, int new_flags);
extern void escort_note_boss_teleported(int objnum);
extern void set_escort_special_goal(int key);
extern void escort_resume_default_goal(void);
extern void escort_find_secret_goal(void);
extern void escort_find_unexplored_goal(void);
extern int escort_get_secret_goal_display_index(void);
extern int escort_get_secret_goal_seg(void);
extern int escort_get_secret_goal_side(void);
#ifdef __ANDROID__
#ifdef INTROSPECT_ON
typedef struct escort_path_parity_result {
	int valid;
	int match;
	int start_seg;
	int goal_seg;
	int ordinary_result;
	int route_result;
	int ordinary_length;
	int route_length;
	int first_mismatch;
	int ai_state_match;
	int restored_state_match;
	unsigned int ordinary_rng_state;
	unsigned int route_rng_state;
	unsigned int ordinary_rng_calls;
	unsigned int route_rng_calls;
} escort_path_parity_result;
#endif

extern int escort_get_route_goal_active(void);
extern int escort_get_route_goal_seg(void);
extern int escort_get_route_goal_side(void);
extern int escort_get_route_goal_wall(void);
extern int escort_get_route_goal_trigger(void);
extern int escort_get_route_goal_objective_kind(void);
extern int escort_get_route_goal_activation_kind(void);
extern int escort_get_route_goal_objective_seg(void);
extern int escort_get_route_goal_objective_side(void);
extern int escort_get_route_goal_objective_wall(void);
extern int escort_get_route_goal_objective_trigger(void);
extern int escort_get_route_goal_guidance_mode(void);
extern int escort_get_route_goal_guidance_seg(void);
extern int escort_get_route_goal_guidance_side(void);
extern int escort_get_route_goal_path_endpoint_seg(void);
extern int escort_get_route_goal_target_pos(int pos[3]);
extern int escort_get_route_goal_path_endpoint_pos(int pos[3]);
extern int escort_get_route_goal_path_pending(void);
extern const char *escort_get_route_goal_instruction(void);
extern int escort_get_route_target_mode(void);
extern const char *escort_get_route_target_mode_name(void);
extern const char *escort_get_route_last_replan_reason(void);
extern unsigned int escort_get_route_metadata_rescan_count(void);
extern int escort_get_route_cache_improvement_pending(void);
extern unsigned int escort_get_route_guidance_full_search_count(void);
extern unsigned int escort_get_route_ignored_nonowner_key_change_count(void);
extern unsigned int escort_get_route_boss_move_invalidation_count(void);
extern unsigned int escort_get_route_wall_generation(void);
extern unsigned int escort_get_route_trigger_generation(void);
extern unsigned int escort_get_route_object_generation(void);
extern unsigned int escort_get_route_reactor_generation(void);
extern unsigned int escort_get_route_automap_generation(void);
extern unsigned int escort_get_route_pending_event_mask(void);
extern unsigned int escort_get_route_event_notification_count(void);
extern unsigned int escort_get_route_event_coalesced_rescan_count(void);
extern unsigned int escort_get_route_ignored_nonowner_event_count(void);
extern void escort_restore_route_target_mode(int target_mode);
extern int escort_get_unexplored_component_size(void);
extern int escort_get_unexplored_target_seg(void);
extern int escort_get_unexplored_waypoint_seg(void);
extern int escort_get_unexplored_direct_reachable(void);
extern int escort_get_unexplored_target_visited(void);
extern const char *escort_get_route_goal_label(void);
extern const char *escort_get_route_goal_guidance_mode_name(void);
extern int escort_get_route_next_waypoint_pending(void);
extern void escort_route_monitor_completion(void);
extern void escort_route_notify_wall_changed(int wall_num);
#ifdef __ANDROID__
extern void escort_notify_blastable_wall_destroyed(void);
#endif
extern void escort_route_notify_trigger_changed(int trigger_num);
extern void escort_route_notify_object_changed(int objnum);
extern void escort_route_notify_reactor_changed(void);
extern void escort_route_notify_automap_changed(int segnum);
#ifdef INTROSPECT_ON
extern int escort_debug_compare_route_path(void);
extern void escort_get_path_parity_result(escort_path_parity_result *result);
#endif
#endif
extern void input_demo_apply_recorded_guidebot_goal(int special_key, int from_menu);
extern void input_demo_apply_recorded_guidebot_find_secret(void);
extern void input_demo_apply_recorded_guidebot_find_unexplored(void);
extern void escort_rebuild_runtime_state_after_restore(void);
extern void escort_spawn_at_player(void);
extern void escort_warp_to_player(void);
extern void escort_get_input_demo_checkpoint_state(struct input_demo_checkpoint_escort_state *escort_state);
extern void escort_get_input_demo_checkpoint_thief_state(struct input_demo_checkpoint_thief_state *thief_state);
extern void thief_apply_network_mode(struct object *objp, int mode, int victim_pnum, int prepare_path);
extern void thief_prepare_for_local_control(struct object *objp);
extern int Buddy_objnum, Buddy_allowed_to_talk;

#ifdef NETWORK
extern int Escort_owner_player;
unsigned int escort_get_owner_generation(void);
void multi_send_escort_owner(int owner_pnum);
void multi_do_escort_owner(const ubyte *buf, int authenticated_sender);
void escort_transfer_ownership_on_disconnect(int gone_pnum);
void escort_release_control(void);
#endif

#endif // _ESCORT_H
