#include <stdio.h>		// for printf()
#include <stdlib.h>		// for rand() and qsort()
#include <string.h>		// for memset()

#include "window.h"
#include "inferno.h"
#include "console.h"
#include "fix.h"
#include "vecmat.h"
#include "gr.h"
#include "3d.h"
#include "palette.h"
#include "timer.h"

#include "object.h"
#include "dxxerror.h"
#include "ai.h"
#include "robot.h"
#include "fvi.h"
#include "physics.h"
#include "wall.h"
#include "player.h"
#include "fireball.h"
#include "game.h"
#include "powerup.h"
#include "cntrlcen.h"
#include "gauges.h"
#include "key.h"
#include "fuelcen.h"
#include "sounds.h"
#include "screens.h"
#include "text.h"
#include "gamefont.h"
#include "newmenu.h"
#include "playsave.h"
#include "gameseq.h"
#include "automap.h"
#include "laser.h"
#include "escort.h"
#include "thief_network_policy.h"
#include "escort_exit_policy.h"
#include "escort_goal_policy.h"
#include "escort_owner_policy.h"
#include "secretarea.h"
#include "collide.h"
#include "maths.h"
#include "switch.h"
#include "input_demo_hooks.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"

#ifdef NETWORK
#include "multi.h"
#include "multibot.h"
#endif

#ifdef __ANDROID__
#include <android/log.h>
#include "android_log.h"
#include "android_menu_scale.h"
#include "coop/coop_powerup_duplication.h"
#include "endlevel.h"
#define ESCORT_DIAG(fmt, ...) __android_log_print(ANDROID_LOG_INFO, "DXX-ESCORT", fmt, ##__VA_ARGS__)
#define ANDROID_JOY_BUTTON_A 0
#define ANDROID_JOY_BUTTON_B 1
#define ANDROID_DPAD_UP_BUTTON 22
#define ANDROID_DPAD_DOWN_BUTTON 23
#define ANDROID_DPAD_LEFT_BUTTON 24
#define ANDROID_DPAD_RIGHT_BUTTON 25
#else
#define ESCORT_DIAG(fmt, ...) ((void)0)
#endif

#ifdef EDITOR
#include "editor/editor.h"
#endif

#include "guidebot_route_internal.h"

extern fix64 Buddy_last_seen_player, Buddy_last_player_path_created;
#ifdef __ANDROID__
escort_route_goal Escort_route_goal;
escort_unexplored_route_target Escort_unexplored_route_target;
int Escort_route_target_mode = ESCORT_ROUTE_TARGET_END_OF_LEVEL;
int Escort_route_target_mode_restore_pending;
int Escort_route_metadata_dirty = 1;
int Escort_route_cache_improvement_pending;
unsigned int Escort_route_seen_revision;
unsigned int Escort_route_metadata_rescan_count;
unsigned int Escort_route_guidance_full_search_count;
unsigned int Escort_route_ignored_nonowner_key_change_count;
unsigned int Escort_route_boss_move_invalidation_count;
unsigned int Escort_route_wall_generation;
unsigned int Escort_route_trigger_generation;
unsigned int Escort_route_object_generation;
unsigned int Escort_route_reactor_generation;
unsigned int Escort_route_automap_generation;
unsigned int Escort_route_pending_event_mask;
unsigned int Escort_route_pending_audit_mask;
unsigned int Escort_route_event_notification_count;
unsigned int Escort_route_notification_coalesced_count;
unsigned int Escort_route_redundant_dirty_domain_count;
unsigned int Escort_route_event_coalesced_rescan_count;
unsigned int Escort_route_publish_latency_sample_count;
unsigned int Escort_route_publish_latency_last_ticks;
unsigned int Escort_route_publish_latency_max_ticks;
fix64 Escort_route_first_dirty_notification_time;
int Escort_route_dirty_notification_time_valid;
unsigned int Escort_route_ignored_nonowner_event_count;
unsigned int Escort_route_audit_check_count;
unsigned int Escort_route_audit_discovery_count;
unsigned int Escort_route_audit_only_discovery_count;
unsigned int Escort_route_audit_work_total;
unsigned int Escort_route_audit_work_max;
unsigned int Escort_route_certificate_check_count;
unsigned int Escort_route_certificate_failure_count;
unsigned int Escort_route_certificate_work_total;
unsigned int Escort_route_certificate_work_max;
unsigned int Escort_route_path_retained_count;
unsigned int Escort_route_path_replaced_count;
unsigned int Escort_route_invalid_path_stopped_count;
unsigned int Escort_route_audit_domain_cursor;
fix64 Escort_route_audit_next_time;
fix64 Escort_route_completion_check_time;
const char *Escort_route_last_replan_reason = "level_start";
static fix64 Escort_nav_trace_next_time;
static vms_vector Escort_nav_trace_last_pos;
static int Escort_nav_trace_last_signature = -1;
static int Escort_nav_trace_last_goal = ESCORT_GOAL_UNSPECIFIED;
static int Escort_nav_trace_last_path_index = -1;
static int Escort_nav_trace_stall_samples;
int Escort_route_avoid_from_seg = -1;
int Escort_route_avoid_seg = -1;
int Escort_route_avoid_from_seg2 = -1;
int Escort_route_avoid_seg2 = -1;
int Escort_route_avoid_trigger = -1;
int Escort_route_avoid_wall = -1;
fix64 Escort_route_progress_next_time;
int Escort_route_progress_signature = -1;
int Escort_route_progress_seg = -1;
int Escort_route_progress_path_index = -1;
int Escort_route_progress_target_seg = -1;
int Escort_route_progress_stall_samples;
unsigned int Escort_route_stall_recovery_count;
#ifdef INTROSPECT_ON
int Escort_route_notifications_suppressed;
int Escort_route_certificate_checks_suppressed;
static escort_path_parity_result Escort_path_parity_result;
static point_seg Escort_path_parity_ordinary[MAX_SEGMENTS * 2];
static point_seg Escort_path_parity_route[MAX_SEGMENTS * 2];
static int Escort_path_parity_saved_path_lengths[MAX_OBJECTS];
#endif

void escort_unexplored_route_target_clear(escort_unexplored_route_target *target)
{
	if (!target)
		return;
	memset(target, 0, sizeof(*target));
	target->target_seg = -1;
	target->waypoint_seg = -1;
}

void escort_trace_path(const char *reason, object *objp, ai_local *ailp,
                              ai_static *aip, int goal_seg)
{
	char segments[512];
	int i;
	int offset = 0;

	if (!debug_log_enabled[DLOG_GUIDEBOT])
		return;
	segments[0] = 0;
	if (aip->hide_index >= 0)
		for (i = 0; i < aip->path_length && i < 32; ++i) {
			const int written = snprintf(
			    segments + offset, sizeof(segments) - offset,
			    "%s%d", i ? "," : "", Point_segs[aip->hide_index + i].segnum);
			if (written < 0 || written >= (int) sizeof(segments) - offset)
				break;
			offset += written;
		}
	debug_log(DLOG_GUIDEBOT,
	          "path reason=%s obj=%d seg=%d goal=%d special=%d goal_index=%d "
	          "route_active=%d route_target=%d mode=%d len=%d index=%d dir=%d "
	          "hide=%d segments=[%s]%s",
	          reason, (int) (objp - Objects), objp->segnum, goal_seg,
	          Escort_special_goal, Escort_goal_index, Escort_route_goal.active,
	          Escort_route_goal.target_seg, ailp->mode, aip->path_length,
	          aip->cur_path_index, aip->PATH_DIR, aip->hide_index, segments,
	          aip->path_length > 32 ? " truncated" : "");
}

void escort_trace_navigation(object *objp, ai_local *ailp, ai_static *aip,
                                    fix dist_to_player, int player_visibility)
{
	int path_point_index = -1;
	int path_seg = -1;
	fix target_distance = -1;
	fix movement = -1;
	fix velocity;
	fix rotvel;

	if (!debug_log_enabled[DLOG_GUIDEBOT])
		return;
	if (Escort_special_goal == -1 && !Escort_route_goal.active &&
	    ailp->mode != AIM_GOTO_OBJECT && ailp->mode != AIM_GOTO_PLAYER)
		return;
	if (GameTime64 < Escort_nav_trace_next_time &&
	    Escort_nav_trace_next_time - GameTime64 < F1_0 * 2)
		return;
	Escort_nav_trace_next_time = GameTime64 + F1_0;

	if (aip->hide_index >= 0 && aip->cur_path_index >= 0 &&
	    aip->cur_path_index < aip->path_length) {
		path_point_index = aip->hide_index + aip->cur_path_index;
		path_seg = Point_segs[path_point_index].segnum;
		target_distance = vm_vec_dist_quick(
		    &objp->pos, &Point_segs[path_point_index].point);
	}
	if (Escort_nav_trace_last_signature == objp->signature)
		movement = vm_vec_dist_quick(&objp->pos, &Escort_nav_trace_last_pos);
	velocity = vm_vec_mag_quick(&objp->mtype.phys_info.velocity);
	rotvel = vm_vec_mag_quick(&objp->mtype.phys_info.rotvel);

	if (Escort_nav_trace_last_signature == objp->signature &&
	    Escort_nav_trace_last_goal == Escort_goal_object &&
	    Escort_nav_trace_last_path_index == aip->cur_path_index &&
	    movement >= 0 && movement < F1_0 / 2 &&
	    ailp->mode == AIM_GOTO_OBJECT)
		Escort_nav_trace_stall_samples++;
	else
		Escort_nav_trace_stall_samples = 0;

	debug_log(DLOG_GUIDEBOT,
	          "nav t=%lld obj=%d seg=%d pos=(%d,%d,%d) move=%d vel=%d rotvel=%d "
	          "mode=%d goal=%d special=%d goal_index=%d route_active=%d "
	          "route_target=%d path_len=%d path_index=%d path_dir=%d path_seg=%d "
	          "target_dist=%d retries=%d consecutive_retries=%d player_dist=%d "
	          "visible=%d stall_samples=%d",
	          (long long) GameTime64, (int) (objp - Objects), objp->segnum,
	          objp->pos.x, objp->pos.y, objp->pos.z, movement, velocity, rotvel,
	          ailp->mode, Escort_goal_object, Escort_special_goal,
	          Escort_goal_index, Escort_route_goal.active,
	          Escort_route_goal.target_seg, aip->path_length,
	          aip->cur_path_index, aip->PATH_DIR, path_seg, target_distance,
	          ailp->retry_count, ailp->consecutive_retries, dist_to_player,
	          player_visibility, Escort_nav_trace_stall_samples);
	if (Escort_nav_trace_stall_samples == 3 ||
	    (Escort_nav_trace_stall_samples > 3 &&
	     Escort_nav_trace_stall_samples % 5 == 0))
		debug_log(DLOG_GUIDEBOT,
		          "suspected_spin obj=%d seg=%d goal=%d special=%d path_len=%d "
		          "path_index=%d path_seg=%d target_dist=%d move=%d vel=%d "
		          "rotvel=%d samples=%d",
		          (int) (objp - Objects), objp->segnum, Escort_goal_object,
		          Escort_special_goal, aip->path_length, aip->cur_path_index,
		          path_seg, target_distance, movement, velocity, rotvel,
		          Escort_nav_trace_stall_samples);

	Escort_nav_trace_last_pos = objp->pos;
	Escort_nav_trace_last_signature = objp->signature;
	Escort_nav_trace_last_goal = Escort_goal_object;
	Escort_nav_trace_last_path_index = aip->cur_path_index;
}

void escort_trace_navigation_reset(const char *reason, object *objp,
                                   ai_local *ailp, ai_static *aip)
{
	if (!debug_log_enabled[DLOG_GUIDEBOT])
		return;
	debug_log(DLOG_GUIDEBOT,
	          "navigation_reset reason=%s obj=%d seg=%d player_seg=%d "
	          "mode=%d goal=%d special=%d goal_index=%d route_active=%d "
	          "route_target=%d path_len=%d path_index=%d path_dir=%d hide=%d "
	          "since_seen=%lld since_player_path=%lld retries=%d "
	          "consecutive_retries=%d",
	          reason, (int) (objp - Objects), objp->segnum,
	          ConsoleObject ? ConsoleObject->segnum : -1, ailp->mode,
	          Escort_goal_object, Escort_special_goal, Escort_goal_index,
	          Escort_route_goal.active, Escort_route_goal.target_seg,
	          aip->path_length, aip->cur_path_index, aip->PATH_DIR,
	          aip->hide_index,
	          (long long) (GameTime64 - Buddy_last_seen_player),
	          (long long) (GameTime64 - Buddy_last_player_path_created),
	          ailp->retry_count, ailp->consecutive_retries);
}

void escort_route_set_target_mode(int target_mode)
{
	Escort_route_target_mode = target_mode;
	if (target_mode != ESCORT_ROUTE_TARGET_UNEXPLORED)
		escort_unexplored_route_target_clear(&Escort_unexplored_route_target);
}

void escort_route_note_replan(const char *reason)
{
	Escort_route_metadata_dirty = 1;
	Escort_route_last_replan_reason = reason && reason[0] ? reason : "unknown";
}

void escort_route_consume_pending_events(void)
{
	if (!Escort_route_pending_event_mask)
		return;
	Escort_route_pending_event_mask = 0;
	Escort_route_pending_audit_mask = 0;
}

unsigned int escort_route_next_event_generation(unsigned int generation)
{
	generation++;
	return generation ? generation : 1;
}

unsigned int escort_route_count_event_domains(unsigned int event_mask)
{
	unsigned int count = 0;

	while (event_mask) {
		count += event_mask & 1u;
		event_mask >>= 1;
	}
	return count;
}

void escort_route_enqueue_event_mask(
    unsigned int event_mask,
    int notification,
    int audit)
{
	const unsigned int redundant_mask =
	    Escort_route_pending_event_mask & event_mask;

	if (!event_mask)
		return;
	if (notification && redundant_mask)
		Escort_route_notification_coalesced_count++;
	if (notification && !Escort_route_dirty_notification_time_valid) {
		Escort_route_first_dirty_notification_time = GameTime64;
		Escort_route_dirty_notification_time_valid = 1;
	}
	Escort_route_redundant_dirty_domain_count +=
	    escort_route_count_event_domains(redundant_mask);
	if (audit) {
		Escort_route_pending_audit_mask |= event_mask;
		if (!redundant_mask)
			Escort_route_audit_only_discovery_count++;
	}
	Escort_route_pending_event_mask |= event_mask;
	Escort_route_metadata_dirty = 1;
}

int escort_route_has_local_authority(void)
{
#ifdef NETWORK
	if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player != Player_num)
		return 0;
#endif
	return 1;
}

void escort_route_record_event(
    unsigned int event_mask,
    unsigned int *generation,
    int matches_objective)
{
	int local_authority;

#ifdef INTROSPECT_ON
	if (Escort_route_notifications_suppressed)
		return;
#endif
	if (generation)
		*generation = escort_route_next_event_generation(*generation);
	Escort_route_event_notification_count++;
	local_authority = escort_route_has_local_authority();
	if (!local_authority)
		Escort_route_ignored_nonowner_event_count++;
	if (!escort_route_event_should_dirty(
	        local_authority,
	        Escort_route_goal.active ||
	            (Escort_route_target_mode == ESCORT_ROUTE_TARGET_UNEXPLORED &&
	             Escort_unexplored_route_target.active),
	        Escort_route_target_mode == ESCORT_ROUTE_TARGET_UNEXPLORED,
	        event_mask,
	        matches_objective))
		return;
	escort_route_enqueue_event_mask(event_mask, 1, 0);
}

unsigned int escort_route_audit_event_mask(int domain)
{
	switch (domain) {
		case ROUTE_SNAPSHOT_DOMAIN_PROGRESSION:
			return ESCORT_ROUTE_EVENT_OBJECT |
			       ESCORT_ROUTE_EVENT_REACTOR;
		case ROUTE_SNAPSHOT_DOMAIN_NAVIGATION:
			return ESCORT_ROUTE_EVENT_WALL;
		case ROUTE_SNAPSHOT_DOMAIN_TRIGGERS:
			return ESCORT_ROUTE_EVENT_TRIGGER;
		case ROUTE_SNAPSHOT_DOMAIN_PROGRESSION_OBJECTS:
			return ESCORT_ROUTE_EVENT_OBJECT;
		case ROUTE_SNAPSHOT_DOMAIN_AUTOMAP:
			return ESCORT_ROUTE_EVENT_AUTOMAP;
		default: return 0;
	}
}

unsigned int escort_route_certificate_event_mask(void)
{
	switch (Escort_route_goal.objective_kind) {
		case LEVEL_METADATA_ROUTE_TRIGGER:
			return ESCORT_ROUTE_EVENT_TRIGGER |
			       ESCORT_ROUTE_EVENT_WALL;
		case LEVEL_METADATA_ROUTE_HIDDEN_DOOR:
		case LEVEL_METADATA_ROUTE_BLASTABLE_WALL:
			return ESCORT_ROUTE_EVENT_WALL;
		case LEVEL_METADATA_ROUTE_KEY:
		case LEVEL_METADATA_ROUTE_BOSS:
			return ESCORT_ROUTE_EVENT_OBJECT;
		case LEVEL_METADATA_ROUTE_REACTOR:
			return ESCORT_ROUTE_EVENT_OBJECT |
			       ESCORT_ROUTE_EVENT_REACTOR;
		case LEVEL_METADATA_ROUTE_EXIT:
			return ESCORT_ROUTE_EVENT_REACTOR |
			       ESCORT_ROUTE_EVENT_WALL;
		default: return ESCORT_ROUTE_EVENT_WALL;
	}
}

void escort_route_run_staggered_audit(void)
{
	static const unsigned char domains[] = {
		ROUTE_SNAPSHOT_DOMAIN_PROGRESSION,
		ROUTE_SNAPSHOT_DOMAIN_NAVIGATION,
		ROUTE_SNAPSHOT_DOMAIN_TRIGGERS,
		ROUTE_SNAPSHOT_DOMAIN_PROGRESSION_OBJECTS,
		ROUTE_SNAPSHOT_DOMAIN_AUTOMAP
	};
	unsigned int work_units = 0;
	unsigned int event_mask;
	int domain;
	int changed;

	if (GameTime64 < Escort_route_audit_next_time &&
	    Escort_route_audit_next_time - GameTime64 < F1_0 * 2)
		return;
	Escort_route_audit_next_time = GameTime64 + F1_0 / 4;
	domain = domains[Escort_route_audit_domain_cursor %
	                 (sizeof(domains) / sizeof(domains[0]))];
	Escort_route_audit_domain_cursor++;
	if (domain == ROUTE_SNAPSHOT_DOMAIN_AUTOMAP &&
	    Escort_route_target_mode != ESCORT_ROUTE_TARGET_UNEXPLORED)
		return;
	changed = level_metadata_route_audit_domain(
	    Buddy_objnum, domain, &work_units);
	if (changed < 0)
		return;
	Escort_route_audit_check_count++;
	Escort_route_audit_work_total += work_units;
	if (work_units > Escort_route_audit_work_max)
		Escort_route_audit_work_max = work_units;
	if (!changed)
		return;
	event_mask = escort_route_audit_event_mask(domain);
	escort_route_enqueue_event_mask(event_mask, 0, 1);
	Escort_route_audit_discovery_count++;
}

void escort_route_sync_target_mode(void)
{
#ifdef NETWORK
	if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player == Player_num)
		multi_send_escort_owner(Escort_owner_player);
#endif
}

void escort_route_clear_goal(void)
{
	memset(&Escort_route_goal, 0, sizeof(Escort_route_goal));
	if (Escort_route_cache_improvement_pending) {
		Escort_route_cache_improvement_pending = 0;
		Escort_route_metadata_dirty = 1;
		Escort_route_last_replan_reason = "cache_ready";
	}
	Escort_route_goal.target_seg = -1;
	Escort_route_goal.target_side = -1;
	Escort_route_goal.target_wall = -1;
	Escort_route_goal.trigger_num = -1;
	Escort_route_goal.objective_kind = -1;
	Escort_route_goal.activation_kind = LEVEL_METADATA_ROUTE_ACTIVATION_NONE;
	Escort_route_goal.objective_seg = -1;
	Escort_route_goal.objective_side = -1;
	Escort_route_goal.objective_wall = -1;
	Escort_route_goal.objective_trigger = -1;
	Escort_route_goal.objective_object = -1;
	Escort_route_goal.objective_key_index = -1;
	Escort_route_goal.guidance_mode = ESCORT_ROUTE_GUIDANCE_NONE;
	Escort_route_goal.guidance_seg = -1;
	Escort_route_goal.guidance_side = -1;
	Escort_route_goal.target_pos_valid = 0;
	Escort_route_goal.path_endpoint_seg = -1;
	Escort_route_goal.path_endpoint_pos_valid = 0;
}

const char *escort_route_goal_label(void)
{
	return Escort_route_goal.label[0] ? Escort_route_goal.label : "route objective";
}

const char *escort_get_route_goal_instruction(void)
{
	if (Escort_route_goal.objective_kind == ESCORT_ROUTE_OBJECTIVE_UNEXPLORED)
		return "unexplored";
	switch (Escort_route_goal.activation_kind) {
		case LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH:
			return "follow me, then shoot the marked switch";
		case LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER:
			return "follow me, then fly through the marked opening";
		case LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER:
			return "follow me, then pass through the marked trigger";
		case LEVEL_METADATA_ROUTE_ACTIVATION_OPEN_HIDDEN_DOOR:
			return "follow me, then open the marked hidden door";
		case LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_REACTOR:
			return "destroy the reactor";
		case LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BOSS:
			return "destroy the boss";
		case LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT:
			return "enter the exit";
		case LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BLASTABLE_WALL:
			return "shoot the blastable wall";
		default:
			return escort_route_goal_label();
	}
}

int escort_route_shared_next_goal(int set_goal, int *selected_index);
void escort_route_refresh_metadata(void);
void escort_route_monitor_completion(void);
int escort_valid_segment(int segnum);

int escort_get_route_goal_active(void)
{
	return Escort_route_goal.active;
}

int escort_get_route_goal_seg(void)
{
	return Escort_route_goal.active ? Escort_route_goal.target_seg : -1;
}

int escort_get_route_goal_side(void)
{
	return Escort_route_goal.active ? Escort_route_goal.target_side : -1;
}

int escort_get_route_goal_wall(void)
{
	return Escort_route_goal.active ? Escort_route_goal.target_wall : -1;
}

int escort_get_route_goal_trigger(void)
{
	return Escort_route_goal.active ? Escort_route_goal.trigger_num : -1;
}

int escort_get_route_goal_objective_kind(void)
{
	return Escort_route_goal.active ? Escort_route_goal.objective_kind : -1;
}

int escort_get_route_goal_activation_kind(void)
{
	return Escort_route_goal.active ? Escort_route_goal.activation_kind : LEVEL_METADATA_ROUTE_ACTIVATION_NONE;
}

int escort_get_route_goal_objective_seg(void)
{
	return Escort_route_goal.active ? Escort_route_goal.objective_seg : -1;
}

int escort_get_route_goal_objective_side(void)
{
	return Escort_route_goal.active ? Escort_route_goal.objective_side : -1;
}

int escort_get_route_goal_objective_wall(void)
{
	return Escort_route_goal.active ? Escort_route_goal.objective_wall : -1;
}

int escort_get_route_goal_objective_trigger(void)
{
	return Escort_route_goal.active ? Escort_route_goal.objective_trigger : -1;
}

int escort_get_route_goal_objective_object(void)
{
	return Escort_route_goal.active ? Escort_route_goal.objective_object : -1;
}

int escort_get_route_goal_objective_key_index(void)
{
	return Escort_route_goal.active ? Escort_route_goal.objective_key_index : -1;
}

int escort_get_route_goal_guidance_mode(void)
{
	return Escort_route_goal.active ? Escort_route_goal.guidance_mode : ESCORT_ROUTE_GUIDANCE_NONE;
}

int escort_get_route_goal_guidance_seg(void)
{
	return Escort_route_goal.active ? Escort_route_goal.guidance_seg : -1;
}

int escort_get_route_goal_guidance_side(void)
{
	return Escort_route_goal.active ? Escort_route_goal.guidance_side : -1;
}

int escort_get_route_goal_path_endpoint_seg(void)
{
	return Escort_route_goal.active ? Escort_route_goal.path_endpoint_seg : -1;
}

int escort_get_route_goal_target_pos(int pos[3])
{
	if (!pos || !Escort_route_goal.active || !Escort_route_goal.target_pos_valid)
		return 0;
	pos[0] = Escort_route_goal.target_pos.x;
	pos[1] = Escort_route_goal.target_pos.y;
	pos[2] = Escort_route_goal.target_pos.z;
	return 1;
}

int escort_get_route_goal_path_endpoint_pos(int pos[3])
{
	if (!pos || !Escort_route_goal.active ||
	    !Escort_route_goal.path_endpoint_pos_valid)
		return 0;
	pos[0] = Escort_route_goal.path_endpoint_pos.x;
	pos[1] = Escort_route_goal.path_endpoint_pos.y;
	pos[2] = Escort_route_goal.path_endpoint_pos.z;
	return 1;
}

#ifdef INTROSPECT_ON
static int escort_path_points_match(const point_seg *ordinary, int ordinary_length,
	const point_seg *route, int route_length, int *first_mismatch)
{
	int i;

	*first_mismatch = -1;
	if (ordinary_length != route_length) {
		*first_mismatch = ordinary_length < route_length ? ordinary_length : route_length;
		return 0;
	}
	for (i = 0; i < ordinary_length; ++i) {
		if (ordinary[i].segnum == route[i].segnum &&
		    ordinary[i].point.x == route[i].point.x &&
		    ordinary[i].point.y == route[i].point.y &&
		    ordinary[i].point.z == route[i].point.z)
			continue;
		*first_mismatch = i;
		return 0;
	}
	return 1;
}

static void escort_path_parity_capture_robot_paths(void)
{
	int i;

	for (i = 0; i <= Highest_object_index; ++i)
		Escort_path_parity_saved_path_lengths[i] =
		    Objects[i].type == OBJ_ROBOT && Objects[i].control_type == CT_AI ?
		        Objects[i].ctype.ai_info.path_length : -1;
}

static void escort_path_parity_restore_robot_paths(void)
{
	int i;

	for (i = 0; i <= Highest_object_index; ++i)
		if (Escort_path_parity_saved_path_lengths[i] >= 0 &&
		    Objects[i].type == OBJ_ROBOT && Objects[i].control_type == CT_AI)
			Objects[i].ctype.ai_info.path_length =
			    Escort_path_parity_saved_path_lengths[i];
}

int escort_debug_compare_route_path(void)
{
	object *objp;
	ai_static *aip;
	ai_local *ailp;
	ai_static saved_aip;
	ai_local saved_ailp;
	escort_route_goal saved_route_goal;
	unsigned int rng_before, rng_calls_before;
	unsigned int rng_restored;
	unsigned int ordinary_rng_calls_after, route_rng_calls_after;
	short ordinary_length = 0, route_length = 0;
	int ordinary_mode_after, ordinary_cur_path_after, ordinary_path_length_after;
	int ordinary_hide_index_after, ordinary_path_dir_after, ordinary_goal_segment_after;
	int route_mode_after, route_cur_path_after, route_path_length_after;
	int route_hide_index_after, route_path_dir_after, route_goal_segment_after;
	int points_match;

	memset(&Escort_path_parity_result, 0, sizeof(Escort_path_parity_result));
	Escort_path_parity_result.first_mismatch = -1;
	if (!Escort_route_goal.active || !escort_is_companion_object(Buddy_objnum) ||
	    !escort_valid_segment(Escort_route_goal.target_seg) ||
	    !d_rand_get_state(&rng_before))
		return 0;

	objp = &Objects[Buddy_objnum];
	aip = &objp->ctype.ai_info;
	ailp = &Ai_local_info[Buddy_objnum];
	saved_aip = *aip;
	saved_ailp = *ailp;
	saved_route_goal = Escort_route_goal;
	escort_path_parity_capture_robot_paths();
	rng_calls_before = d_rand_get_call_count();
	ailp->goal_segment = saved_route_goal.target_seg;

	escort_route_clear_goal();
	Escort_path_parity_result.ordinary_result = create_path_points(
	    objp, objp->segnum, saved_route_goal.target_seg,
	    Escort_path_parity_ordinary, &ordinary_length,
	    Max_escort_length, 1, 1, -1);
	d_rand_get_state(&Escort_path_parity_result.ordinary_rng_state);
	ordinary_rng_calls_after = d_rand_get_call_count();
	ordinary_mode_after = ailp->mode;
	ordinary_cur_path_after = aip->cur_path_index;
	ordinary_path_length_after = aip->path_length;
	ordinary_hide_index_after = aip->hide_index;
	ordinary_path_dir_after = aip->PATH_DIR;
	ordinary_goal_segment_after = ailp->goal_segment;

	d_rand_set_state(rng_before);
	d_rand_set_call_count(rng_calls_before);
	escort_path_parity_restore_robot_paths();
	*aip = saved_aip;
	*ailp = saved_ailp;
	ailp->goal_segment = saved_route_goal.target_seg;
	Escort_route_goal = saved_route_goal;
	Escort_path_parity_result.route_result = create_guidebot_route_path_points(
	    objp, objp->segnum, saved_route_goal.target_seg,
	    Escort_path_parity_route, &route_length,
	    Max_escort_length, 1, 1);
	d_rand_get_state(&Escort_path_parity_result.route_rng_state);
	route_rng_calls_after = d_rand_get_call_count();
	route_mode_after = ailp->mode;
	route_cur_path_after = aip->cur_path_index;
	route_path_length_after = aip->path_length;
	route_hide_index_after = aip->hide_index;
	route_path_dir_after = aip->PATH_DIR;
	route_goal_segment_after = ailp->goal_segment;

	d_rand_set_state(rng_before);
	d_rand_set_call_count(rng_calls_before);
	escort_path_parity_restore_robot_paths();
	*aip = saved_aip;
	*ailp = saved_ailp;
	Escort_route_goal = saved_route_goal;

	Escort_path_parity_result.valid = 1;
	Escort_path_parity_result.start_seg = objp->segnum;
	Escort_path_parity_result.goal_seg = saved_route_goal.target_seg;
	Escort_path_parity_result.ordinary_length = ordinary_length;
	Escort_path_parity_result.route_length = route_length;
	Escort_path_parity_result.ordinary_rng_calls = ordinary_rng_calls_after - rng_calls_before;
	Escort_path_parity_result.route_rng_calls = route_rng_calls_after - rng_calls_before;
	Escort_path_parity_result.ai_state_match =
	    ordinary_mode_after == route_mode_after &&
	    ordinary_cur_path_after == route_cur_path_after &&
	    ordinary_path_length_after == route_path_length_after &&
	    ordinary_hide_index_after == route_hide_index_after &&
	    ordinary_path_dir_after == route_path_dir_after &&
	    ordinary_goal_segment_after == route_goal_segment_after;
	d_rand_get_state(&rng_restored);
	Escort_path_parity_result.restored_state_match =
	    aip->cur_path_index == saved_aip.cur_path_index &&
	    aip->path_length == saved_aip.path_length &&
	    aip->hide_index == saved_aip.hide_index &&
	    aip->PATH_DIR == saved_aip.PATH_DIR &&
	    ailp->mode == saved_ailp.mode &&
	    ailp->goal_segment == saved_ailp.goal_segment &&
	    Escort_route_goal.active == saved_route_goal.active &&
	    Escort_route_goal.target_seg == saved_route_goal.target_seg &&
	    Escort_route_goal.objective_kind == saved_route_goal.objective_kind &&
	    Escort_route_goal.objective_wall == saved_route_goal.objective_wall &&
	    rng_restored == rng_before &&
	    d_rand_get_call_count() == rng_calls_before;
	points_match = escort_path_points_match(
	    Escort_path_parity_ordinary, ordinary_length,
	    Escort_path_parity_route, route_length,
	    &Escort_path_parity_result.first_mismatch);
	Escort_path_parity_result.match =
	    points_match && Escort_path_parity_result.ai_state_match &&
	    Escort_path_parity_result.restored_state_match &&
	    Escort_path_parity_result.ordinary_result == Escort_path_parity_result.route_result &&
	    Escort_path_parity_result.ordinary_rng_state == Escort_path_parity_result.route_rng_state &&
	    Escort_path_parity_result.ordinary_rng_calls == Escort_path_parity_result.route_rng_calls;
	return Escort_path_parity_result.match;
}

void escort_get_path_parity_result(escort_path_parity_result *result)
{
	if (result)
		*result = Escort_path_parity_result;
}
#endif

int escort_get_route_target_mode(void)
{
	return Escort_route_target_mode;
}

const char *escort_get_route_last_replan_reason(void)
{
	return Escort_route_last_replan_reason;
}

unsigned int escort_get_route_metadata_rescan_count(void)
{
	return Escort_route_metadata_rescan_count;
}

unsigned int escort_get_route_guidance_full_search_count(void)
{
	return Escort_route_guidance_full_search_count;
}

int escort_get_route_avoid_seg(void)
{
	return Escort_route_avoid_seg;
}

int escort_get_route_avoid_seg2(void)
{
	return Escort_route_avoid_seg2;
}

unsigned int escort_get_route_stall_recovery_count(void)
{
	return Escort_route_stall_recovery_count;
}

unsigned int escort_get_route_ignored_nonowner_key_change_count(void)
{
	return Escort_route_ignored_nonowner_key_change_count;
}

unsigned int escort_get_route_boss_move_invalidation_count(void)
{
	return Escort_route_boss_move_invalidation_count;
}

unsigned int escort_get_route_wall_generation(void)
{
	return Escort_route_wall_generation;
}

unsigned int escort_get_route_trigger_generation(void)
{
	return Escort_route_trigger_generation;
}

unsigned int escort_get_route_object_generation(void)
{
	return Escort_route_object_generation;
}

unsigned int escort_get_route_reactor_generation(void)
{
	return Escort_route_reactor_generation;
}

unsigned int escort_get_route_automap_generation(void)
{
	return Escort_route_automap_generation;
}

unsigned int escort_get_route_pending_event_mask(void)
{
	return Escort_route_pending_event_mask;
}

unsigned int escort_get_route_event_notification_count(void)
{
	return Escort_route_event_notification_count;
}

unsigned int escort_get_route_notification_coalesced_count(void)
{
	return Escort_route_notification_coalesced_count;
}

unsigned int escort_get_route_redundant_dirty_domain_count(void)
{
	return Escort_route_redundant_dirty_domain_count;
}

unsigned int escort_get_route_event_coalesced_rescan_count(void)
{
	return Escort_route_event_coalesced_rescan_count;
}

unsigned int escort_get_route_publish_latency_sample_count(void)
{
	return Escort_route_publish_latency_sample_count;
}

unsigned int escort_get_route_publish_latency_last_ticks(void)
{
	return Escort_route_publish_latency_last_ticks;
}

unsigned int escort_get_route_publish_latency_max_ticks(void)
{
	return Escort_route_publish_latency_max_ticks;
}

unsigned int escort_get_route_ignored_nonowner_event_count(void)
{
	return Escort_route_ignored_nonowner_event_count;
}

unsigned int escort_get_route_pending_audit_mask(void)
{
	return Escort_route_pending_audit_mask;
}

unsigned int escort_get_route_audit_check_count(void)
{
	return Escort_route_audit_check_count;
}

unsigned int escort_get_route_audit_discovery_count(void)
{
	return Escort_route_audit_discovery_count;
}

unsigned int escort_get_route_audit_only_discovery_count(void)
{
	return Escort_route_audit_only_discovery_count;
}

unsigned int escort_get_route_audit_work_total(void)
{
	return Escort_route_audit_work_total;
}

unsigned int escort_get_route_audit_work_max(void)
{
	return Escort_route_audit_work_max;
}

unsigned int escort_get_route_certificate_check_count(void)
{
	return Escort_route_certificate_check_count;
}

unsigned int escort_get_route_certificate_failure_count(void)
{
	return Escort_route_certificate_failure_count;
}

unsigned int escort_get_route_certificate_work_total(void)
{
	return Escort_route_certificate_work_total;
}

unsigned int escort_get_route_certificate_work_max(void)
{
	return Escort_route_certificate_work_max;
}

unsigned int escort_get_route_path_retained_count(void)
{
	return Escort_route_path_retained_count;
}

unsigned int escort_get_route_path_replaced_count(void)
{
	return Escort_route_path_replaced_count;
}

unsigned int escort_get_route_invalid_path_stopped_count(void)
{
	return Escort_route_invalid_path_stopped_count;
}

#ifdef INTROSPECT_ON
void escort_set_route_notifications_suppressed(int suppressed)
{
	Escort_route_notifications_suppressed = suppressed != 0;
}

int escort_get_route_notifications_suppressed(void)
{
	return Escort_route_notifications_suppressed;
}

void escort_reset_route_efficiency_counters(void)
{
	Escort_route_metadata_rescan_count = 0;
	Escort_route_guidance_full_search_count = 0;
	Escort_route_event_notification_count = 0;
	Escort_route_notification_coalesced_count = 0;
	Escort_route_redundant_dirty_domain_count = 0;
	Escort_route_event_coalesced_rescan_count = 0;
	Escort_route_publish_latency_sample_count = 0;
	Escort_route_publish_latency_last_ticks = 0;
	Escort_route_publish_latency_max_ticks = 0;
	Escort_route_first_dirty_notification_time = 0;
	Escort_route_dirty_notification_time_valid = 0;
	Escort_route_audit_check_count = 0;
	Escort_route_audit_discovery_count = 0;
	Escort_route_audit_only_discovery_count = 0;
	Escort_route_audit_work_total = 0;
	Escort_route_audit_work_max = 0;
	Escort_route_certificate_check_count = 0;
	Escort_route_certificate_failure_count = 0;
	Escort_route_certificate_work_total = 0;
	Escort_route_certificate_work_max = 0;
	Escort_route_path_retained_count = 0;
	Escort_route_path_replaced_count = 0;
	Escort_route_invalid_path_stopped_count = 0;
}

void escort_set_route_certificate_checks_suppressed(int suppressed)
{
	Escort_route_certificate_checks_suppressed = suppressed != 0;
}

int escort_get_route_certificate_checks_suppressed(void)
{
	return Escort_route_certificate_checks_suppressed;
}
#endif

void escort_restore_route_target_mode(int target_mode)
{
	escort_route_set_target_mode(
	    target_mode == ESCORT_ROUTE_TARGET_UNEXPLORED ?
	        ESCORT_ROUTE_TARGET_UNEXPLORED :
	        ESCORT_ROUTE_TARGET_END_OF_LEVEL);
	Escort_route_target_mode_restore_pending = 1;
}

const char *escort_get_route_target_mode_name(void)
{
	return Escort_route_target_mode == ESCORT_ROUTE_TARGET_UNEXPLORED ? "unexplored" : "end_of_level";
}

int escort_get_unexplored_component_size(void)
{
	return Escort_unexplored_route_target.active ? Escort_unexplored_route_target.component_size : 0;
}

int escort_get_unexplored_target_seg(void)
{
	return Escort_unexplored_route_target.active ? Escort_unexplored_route_target.target_seg : -1;
}

int escort_get_unexplored_waypoint_seg(void)
{
	return Escort_unexplored_route_target.active ? Escort_unexplored_route_target.waypoint_seg : -1;
}

int escort_get_unexplored_direct_reachable(void)
{
	return Escort_unexplored_route_target.active ? Escort_unexplored_route_target.direct_reachable : 0;
}

int escort_get_unexplored_target_visited(void)
{
	return Escort_unexplored_route_target.active &&
	       escort_valid_segment(Escort_unexplored_route_target.target_seg) &&
	       Automap_visited[Escort_unexplored_route_target.target_seg];
}

const char *escort_get_route_goal_label(void)
{
	return Escort_route_goal.active ? escort_route_goal_label() : "";
}

const char *escort_get_route_goal_guidance_mode_name(void)
{
	if (!Escort_route_goal.active)
		return "";
	switch (Escort_route_goal.guidance_mode) {
		case ESCORT_ROUTE_GUIDANCE_REACH_OBJECTIVE:
			return "reach_objective";
		case ESCORT_ROUTE_GUIDANCE_REACH_HIDDEN_DOOR:
			return "reach_hidden_door";
		case ESCORT_ROUTE_GUIDANCE_REACH_FIRING_POSITION:
			return "reach_firing_position";
		case ESCORT_ROUTE_GUIDANCE_NEAREST_PROGRESS_POINT:
			return "nearest_progress_point";
		default:
			return "none";
	}
}

int escort_valid_segment(int segnum)
{
	return segnum >= 0 && segnum <= Highest_segment_index;
}

static int escort_valid_wall(int wall_num)
{
	return wall_num >= 0 && wall_num < Num_walls;
}

int escort_route_key_powerup_id(int key_index)
{
	switch (key_index) {
		case 0:
			return POW_KEY_BLUE;
		case 1:
			return POW_KEY_RED;
		case 2:
			return POW_KEY_GOLD;
		default:
			return -1;
	}
}

int escort_route_key_flag(int key_index)
{
	switch (key_index) {
		case 0:
			return PLAYER_FLAGS_BLUE_KEY;
		case 1:
			return PLAYER_FLAGS_RED_KEY;
		case 2:
			return PLAYER_FLAGS_GOLD_KEY;
		default:
			return 0;
	}
}

void escort_route_notify_wall_changed(int wall_num)
{
	int matches_objective = Escort_route_goal.active && wall_num >= 0 &&
	                        (wall_num == Escort_route_goal.objective_wall ||
	                         wall_num == Escort_route_goal.target_wall);
	escort_route_record_event(
	    ESCORT_ROUTE_EVENT_WALL,
	    &Escort_route_wall_generation,
	    matches_objective);
}

void escort_route_notify_trigger_changed(int trigger_num)
{
	int matches_objective;

#ifdef INTROSPECT_ON
	if (Escort_route_notifications_suppressed)
		return;
#endif
	matches_objective = Escort_route_goal.active && trigger_num >= 0 &&
	                    (trigger_num == Escort_route_goal.objective_trigger ||
	                     trigger_num == Escort_route_goal.trigger_num);
	escort_route_record_event(
	    ESCORT_ROUTE_EVENT_TRIGGER,
	    &Escort_route_trigger_generation,
	    matches_objective);
}

void escort_route_notify_object_changed(int objnum)
{
	int matches_objective = 0;

	if (Escort_route_goal.active && objnum >= 0 && objnum <= Highest_object_index) {
		object *objp = &Objects[objnum];
		if (objnum == Escort_route_goal.objective_object)
			matches_objective = 1;
		else if (Escort_route_goal.objective_kind == LEVEL_METADATA_ROUTE_KEY) {
			int powerup_id = escort_route_key_powerup_id(
			    Escort_route_goal.objective_key_index);
			matches_objective = powerup_id >= 0 &&
			                    ((objp->type == OBJ_POWERUP && objp->id == powerup_id) ||
			                     (objp->type == OBJ_ROBOT &&
			                      objp->contains_type == OBJ_POWERUP &&
			                      objp->contains_id == powerup_id &&
			                      objp->contains_count > 0));
		} else if (Escort_route_goal.objective_kind == LEVEL_METADATA_ROUTE_BOSS)
			matches_objective = objp->type == OBJ_ROBOT &&
			                    Robot_info[objp->id].boss_flag;
		else if (Escort_route_goal.objective_kind == LEVEL_METADATA_ROUTE_REACTOR)
			matches_objective = objp->type == OBJ_CNTRLCEN;
	}
	escort_route_record_event(
	    ESCORT_ROUTE_EVENT_OBJECT,
	    &Escort_route_object_generation,
	    matches_objective);
}

void escort_route_notify_reactor_changed(void)
{
	int matches_objective = Escort_route_goal.active &&
	                        (Escort_route_goal.objective_kind == LEVEL_METADATA_ROUTE_REACTOR ||
	                         Escort_route_goal.objective_kind == LEVEL_METADATA_ROUTE_BOSS ||
	                         Escort_route_goal.objective_kind == LEVEL_METADATA_ROUTE_EXIT);
	escort_route_record_event(
	    ESCORT_ROUTE_EVENT_REACTOR,
	    &Escort_route_reactor_generation,
	    matches_objective);
}

void escort_route_notify_automap_changed(int segnum)
{
	(void) segnum;
	escort_route_record_event(
	    ESCORT_ROUTE_EVENT_AUTOMAP,
	    &Escort_route_automap_generation,
	    0);
}

int escort_route_key_goal(int key_index)
{
	return key_index == 0 ? ESCORT_GOAL_BLUE_KEY :
	       key_index == 2 ? ESCORT_GOAL_GOLD_KEY :
	                        ESCORT_GOAL_RED_KEY;
}

int escort_route_goal_object_for_step(const level_metadata_route_step *step)
{
	if (!step)
		return ESCORT_GOAL_EXIT;
	switch (step->kind) {
		case LEVEL_METADATA_ROUTE_KEY:
			return escort_route_key_goal(step->key_index);
		case LEVEL_METADATA_ROUTE_REACTOR:
			return ESCORT_GOAL_CONTROLCEN;
		case LEVEL_METADATA_ROUTE_BOSS:
			return ESCORT_GOAL_BOSS;
		case LEVEL_METADATA_ROUTE_EXIT:
			return ESCORT_GOAL_EXIT;
		case LEVEL_METADATA_ROUTE_UNEXPLORED:
			return ESCORT_GOAL_EXIT;
		default:
			return ESCORT_GOAL_EXIT;
	}
}

int escort_route_step_guidance_mode(const level_metadata_route_step *step)
{
	if (!step)
		return ESCORT_ROUTE_GUIDANCE_NONE;
	switch (step->kind) {
		case LEVEL_METADATA_ROUTE_TRIGGER:
			switch (step->activation_kind) {
				case LEVEL_METADATA_ROUTE_ACTIVATION_UNRESOLVED_TRIGGER:
					return ESCORT_ROUTE_GUIDANCE_NEAREST_PROGRESS_POINT;
				case LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH:
					return ESCORT_ROUTE_GUIDANCE_REACH_FIRING_POSITION;
				case LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER:
				case LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER:
					return ESCORT_ROUTE_GUIDANCE_REACH_OBJECTIVE;
				default:
					return ESCORT_ROUTE_GUIDANCE_REACH_FIRING_POSITION;
			}
		case LEVEL_METADATA_ROUTE_HIDDEN_DOOR:
			return ESCORT_ROUTE_GUIDANCE_REACH_HIDDEN_DOOR;
		case LEVEL_METADATA_ROUTE_BLASTABLE_WALL:
			return ESCORT_ROUTE_GUIDANCE_REACH_FIRING_POSITION;
		case LEVEL_METADATA_ROUTE_KEY:
		case LEVEL_METADATA_ROUTE_REACTOR:
		case LEVEL_METADATA_ROUTE_BOSS:
		case LEVEL_METADATA_ROUTE_EXIT:
		case LEVEL_METADATA_ROUTE_UNEXPLORED:
			return ESCORT_ROUTE_GUIDANCE_REACH_OBJECTIVE;
		default:
			return ESCORT_ROUTE_GUIDANCE_NONE;
	}
}

int escort_route_step_is_targetable(const level_metadata_route_step *step)
{
	if (!step)
		return 0;
	switch (step->kind) {
		case LEVEL_METADATA_ROUTE_KEY:
		case LEVEL_METADATA_ROUTE_TRIGGER:
		case LEVEL_METADATA_ROUTE_HIDDEN_DOOR:
		case LEVEL_METADATA_ROUTE_BLASTABLE_WALL:
		case LEVEL_METADATA_ROUTE_REACTOR:
		case LEVEL_METADATA_ROUTE_BOSS:
		case LEVEL_METADATA_ROUTE_EXIT:
		case LEVEL_METADATA_ROUTE_UNEXPLORED:
			return escort_valid_segment(step->seg) || escort_valid_wall(step->wall_num);
		default:
			return 0;
	}
}

void escort_route_set_step_goal(const level_metadata_route_step *step, int guidance_mode, int path_terminal_seg)
{
	int target_seg = escort_valid_segment(path_terminal_seg) ? path_terminal_seg : step->seg;
	if (Escort_route_avoid_seg >= 0 &&
	    (Escort_route_avoid_trigger != step->trigger_num ||
	     Escort_route_avoid_wall != step->wall_num)) {
		Escort_route_avoid_from_seg = -1;
		Escort_route_avoid_seg = -1;
		Escort_route_avoid_from_seg2 = -1;
		Escort_route_avoid_seg2 = -1;
		Escort_route_avoid_trigger = -1;
		Escort_route_avoid_wall = -1;
	}

	if (step->activation_kind == LEVEL_METADATA_ROUTE_ACTIVATION_UNRESOLVED_TRIGGER &&
	    escort_valid_segment(step->seg))
		target_seg = step->seg;
	else if ((step->activation_kind == LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER ||
	     step->activation_kind == LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER) &&
	    escort_valid_segment(step->seg))
		target_seg = step->seg;
	int target_side = target_seg == step->seg ? step->side : -1;

	escort_route_clear_goal();
	Escort_route_goal.active = 1;
	Escort_route_goal.target_seg = target_seg;
	Escort_route_goal.target_side = target_side;
	Escort_route_goal.target_wall = step->wall_num;
	Escort_route_goal.trigger_num = step->trigger_num;
	Escort_route_goal.objective_kind = step->kind;
	Escort_route_goal.activation_kind = step->activation_kind;
	Escort_route_goal.objective_seg = step->seg;
	Escort_route_goal.objective_side = step->side;
	Escort_route_goal.objective_wall = step->wall_num;
	Escort_route_goal.objective_trigger = step->trigger_num;
	Escort_route_goal.objective_object = step->key_carrier_objnum;
	Escort_route_goal.objective_key_index = step->key_index;
	Escort_route_goal.guidance_mode = guidance_mode;
	Escort_route_goal.guidance_seg = target_seg;
	Escort_route_goal.guidance_side = target_side;
	if (guidance_mode == ESCORT_ROUTE_GUIDANCE_REACH_FIRING_POSITION &&
	    step->activation_pos_valid && target_seg == path_terminal_seg) {
		Escort_route_goal.target_pos_valid = 1;
		Escort_route_goal.target_pos.x = step->activation_pos[0];
		Escort_route_goal.target_pos.y = step->activation_pos[1];
		Escort_route_goal.target_pos.z = step->activation_pos[2];
	}
	if (step->kind == LEVEL_METADATA_ROUTE_UNEXPLORED) {
		Escort_route_goal.objective_kind = ESCORT_ROUTE_OBJECTIVE_UNEXPLORED;
		if (Escort_unexplored_route_target.active)
			Escort_route_goal.objective_seg = Escort_unexplored_route_target.target_seg;
	}
	if (escort_valid_wall(step->wall_num)) {
		Escort_route_goal.objective_seg = Walls[step->wall_num].segnum;
		Escort_route_goal.objective_side = Walls[step->wall_num].sidenum;
	}
	snprintf(Escort_route_goal.label, sizeof(Escort_route_goal.label), "%s",
	         step->label[0] ? step->label : "route objective");
}

void escort_route_set_frontier_goal(int target_seg)
{
	escort_route_clear_goal();
	Escort_route_goal.active = 1;
	Escort_route_goal.target_seg = target_seg;
	Escort_route_goal.guidance_mode = ESCORT_ROUTE_GUIDANCE_NEAREST_PROGRESS_POINT;
	Escort_route_goal.guidance_seg = target_seg;
	if (Escort_route_target_mode == ESCORT_ROUTE_TARGET_UNEXPLORED)
		Escort_route_goal.objective_kind = ESCORT_ROUTE_OBJECTIVE_UNEXPLORED;
	snprintf(Escort_route_goal.label, sizeof(Escort_route_goal.label), "%s",
	         "closest reachable route point");
}

int escort_route_shared_next_goal(int set_goal, int *selected_index)
{
	const level_metadata_state *metadata = level_metadata_get_live_route_state();
	route_planner_plan_summary summary;
	const level_metadata_route_step *step;
	int guidance_mode;
	int index;

	if (selected_index)
		*selected_index = -1;
	if (Escort_route_target_mode == ESCORT_ROUTE_TARGET_UNEXPLORED &&
	    !Escort_unexplored_route_target.active) {
		if (set_goal)
			escort_route_clear_goal();
		return ESCORT_GOAL_UNSPECIFIED;
	}
	if (!metadata || !level_metadata_get_live_route_plan_summary(&summary)) {
		if (set_goal)
			escort_route_clear_goal();
		return ESCORT_GOAL_UNSPECIFIED;
	}
	index = summary.first_pending_step;
	if (index < 0 || index >= metadata->route_step_count ||
	    index >= LEVEL_METADATA_MAX_ROUTE_STEPS) {
		if (!escort_valid_segment(summary.partial_frontier_segment)) {
			if (set_goal)
				escort_route_clear_goal();
			return ESCORT_GOAL_UNSPECIFIED;
		}
		if (set_goal)
			escort_route_set_frontier_goal(summary.partial_frontier_segment);
		return ESCORT_GOAL_EXIT;
	}
	step = &metadata->route_steps[index];
	if (!escort_route_step_is_targetable(step)) {
		if (set_goal)
			escort_route_clear_goal();
		return ESCORT_GOAL_UNSPECIFIED;
	}
	guidance_mode = escort_route_step_guidance_mode(step);
	if (guidance_mode == ESCORT_ROUTE_GUIDANCE_NONE) {
		if (set_goal)
			escort_route_clear_goal();
		if (selected_index)
			*selected_index = index;
		return ESCORT_GOAL_UNSPECIFIED;
	}
	if (set_goal)
		escort_route_set_step_goal(
		    step, guidance_mode,
		    escort_valid_segment(summary.first_pending_path_terminal_segment) ?
		        summary.first_pending_path_terminal_segment :
		        summary.partial_frontier_segment);
	if (selected_index)
		*selected_index = index;
	return escort_route_goal_object_for_step(step);
}

fix64 Escort_route_cache_poll_time;
int Escort_route_logged_readiness = -1;

int escort_route_metadata_pending(void)
{
	return level_metadata_get_route_readiness() ==
	       LEVEL_METADATA_READINESS_CALCULATING;
}

int escort_route_next_waypoint_pending(void)
{
	const int readiness = level_metadata_get_route_readiness();

	if (readiness == LEVEL_METADATA_READINESS_CALCULATING)
		return 1;
	return readiness == LEVEL_METADATA_READINESS_PARTIAL &&
	       escort_route_shared_next_goal(0, NULL) == ESCORT_GOAL_UNSPECIFIED;
}

int escort_get_route_next_waypoint_pending(void)
{
	return escort_route_next_waypoint_pending();
}

int escort_get_route_cache_improvement_pending(void)
{
	return Escort_route_cache_improvement_pending;
}

void escort_route_poll_pending_cache(void)
{
	const int readiness = level_metadata_get_route_readiness();
	const fix64 now = timer_query();
	const unsigned int revision = level_metadata_get_route_revision();
	if (revision != Escort_route_seen_revision) {
		Escort_route_seen_revision = revision;
		if (Escort_route_goal.active)
			Escort_route_cache_improvement_pending = 1;
		else
			Escort_route_metadata_dirty = 1;
	}
	if (readiness != Escort_route_logged_readiness) {
		debug_log(DLOG_GAME, "Guide-Bot route metadata readiness=%s",
		          level_metadata_route_readiness_name(readiness));
		Escort_route_logged_readiness = readiness;
	}
	if (readiness == LEVEL_METADATA_READINESS_COMPLETE ||
	    readiness == LEVEL_METADATA_READINESS_FAILED)
		return;
#ifdef NETWORK
	if (!escort_route_cache_poll_allowed(
	        (Game_mode & GM_MULTI) != 0,
	        (Game_mode & GM_MULTI_COOP) != 0))
		return;
#endif
	if (input_demo_recorder_is_active() || input_demo_replay_is_loaded())
		return;
	if (now < Escort_route_cache_poll_time &&
	    Escort_route_cache_poll_time - now < 2 * F1_0)
		return;
	Escort_route_cache_poll_time = now + F1_0;
	if (level_metadata_try_load_pending_cache()) {
		const int loaded_readiness = level_metadata_get_route_readiness();
		debug_log(DLOG_GAME, "Guide-Bot loaded route metadata readiness=%s",
		          level_metadata_route_readiness_name(loaded_readiness));
		Escort_route_logged_readiness = loaded_readiness;
		if (Escort_route_goal.active)
			Escort_route_cache_improvement_pending = 1;
		else
			Escort_route_metadata_dirty = 1;
	}
}

int escort_route_next_goal(void)
{
	return escort_route_shared_next_goal(1, NULL);
}

void escort_route_refresh_metadata(void)
{
#ifdef NETWORK
	if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player != Player_num)
		return;
#endif
	escort_route_poll_pending_cache();
	if (escort_route_metadata_pending())
		return;
	if (Escort_route_target_mode == ESCORT_ROUTE_TARGET_UNEXPLORED &&
	    Escort_unexplored_route_target.active &&
	    escort_valid_segment(Escort_unexplored_route_target.target_seg) &&
	    Automap_visited[Escort_unexplored_route_target.target_seg])
		Escort_route_metadata_dirty = 1;
	if (!Escort_route_metadata_dirty)
		return;
	Escort_route_metadata_dirty = 0;
	escort_route_consume_pending_events();
	Escort_route_metadata_rescan_count++;
	if (Escort_route_target_mode == ESCORT_ROUTE_TARGET_UNEXPLORED &&
	    escort_is_companion_object(Buddy_objnum)) {
		level_metadata_unexplored_route route;

		if (level_metadata_rescan_unexplored_route_from_object(Buddy_objnum, &route)) {
			escort_unexplored_route_target_clear(&Escort_unexplored_route_target);
			Escort_unexplored_route_target.active = 1;
			Escort_unexplored_route_target.component_size = route.component_size;
			Escort_unexplored_route_target.target_seg = route.target_seg;
			Escort_unexplored_route_target.waypoint_seg = route.waypoint_seg;
			Escort_unexplored_route_target.direct_reachable = route.direct_reachable;
			ESCORT_DIAG("unexplored target component=%d target=%d waypoint=%d direct=%d",
			            route.component_size,
			            route.target_seg,
			            route.waypoint_seg,
			            route.direct_reachable);
			return;
		}
		escort_unexplored_route_target_clear(&Escort_unexplored_route_target);
		return;
	}
	if (escort_is_companion_object(Buddy_objnum))
		level_metadata_rescan_route_from_object(Buddy_objnum);
	else
		level_metadata_rescan_current_level();
}

void escort_route_stop_invalid_path(void)
{
	object *objp;
	ai_local *ailp;
	ai_static *aip;

	if (!escort_is_companion_object(Buddy_objnum))
		return;
	objp = &Objects[Buddy_objnum];
	ailp = &Ai_local_info[Buddy_objnum];
	aip = &objp->ctype.ai_info;
	if (ailp->mode != AIM_GOTO_OBJECT)
		return;
	ailp->mode = AIM_GOTO_PLAYER;
	create_path_to_player(objp, Max_escort_length, 1);
	if (aip->path_length > 3)
		aip->path_length = polish_path(
		    objp, &Point_segs[aip->hide_index], aip->path_length);
	Buddy_last_player_path_created = GameTime64;
	Escort_last_path_created = GameTime64;
}

void escort_route_monitor_completion(void)
{
	escort_route_goal previous_goal;
	guidebot_route_decision previous_decision;
	guidebot_route_decision next_decision;
	int previous_decision_valid;
	int previous_goal_object;
	int certificate_valid = -1;
	int certificate_failed = 0;
	int has_active_goal;

#ifdef NETWORK
	if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player != Player_num)
		return;
#endif
	escort_route_poll_pending_cache();
	if (!escort_route_has_local_authority())
		return;
	has_active_goal = Escort_route_goal.active &&
	                  Escort_route_goal.objective_kind >= 0;
	previous_goal = Escort_route_goal;
	previous_goal_object = Escort_goal_object;
	previous_decision_valid =
	    level_metadata_get_live_route_decision(&previous_decision);
	if (GameTime64 >= Escort_route_completion_check_time &&
	    GameTime64 - Escort_route_completion_check_time < F1_0 / 4)
		return;
	Escort_route_completion_check_time = GameTime64;
#ifdef INTROSPECT_ON
	if (!Escort_route_certificate_checks_suppressed)
#else
	if (1)
#endif
	{
		unsigned int work_units = 0;
		int valid = level_metadata_validate_live_route_certificate(
		    Buddy_objnum, &work_units);
		certificate_valid = valid;

		if (valid >= 0) {
			Escort_route_certificate_check_count++;
			Escort_route_certificate_work_total += work_units;
			if (work_units > Escort_route_certificate_work_max)
				Escort_route_certificate_work_max = work_units;
		}
		if (valid == 0) {
			unsigned int event_mask =
			    escort_route_certificate_event_mask();

			escort_route_enqueue_event_mask(event_mask, 0, 0);
			Escort_route_certificate_failure_count++;
			certificate_failed = 1;
		}
	}
	if (has_active_goal && !Escort_route_pending_event_mask)
		escort_route_run_staggered_audit();
	if (Escort_route_pending_event_mask || Escort_route_metadata_dirty) {
		unsigned int pending_events = Escort_route_pending_event_mask;
		unsigned int audit_events = Escort_route_pending_audit_mask;
		int previous_certificate_valid =
		    certificate_valid == 1 && previous_decision_valid &&
		    previous_decision.certificate.status ==
		        GUIDEBOT_ROUTE_CERTIFICATE_VALID;
		escort_route_consume_pending_events();
		if (pending_events)
			escort_route_note_replan(
			    certificate_failed ?
			        "state_certificate" :
			    audit_events == pending_events ?
			        "state_audit" :
			    pending_events == ESCORT_ROUTE_EVENT_AUTOMAP ?
			        "automap_exploration" :
			        "world_state_event");
		escort_route_refresh_metadata();
		Escort_route_event_coalesced_rescan_count++;
		if (has_active_goal) {
			int next_decision_valid =
			    0;
			int adoption_action =
			    0;

			(void) escort_route_next_goal();
			next_decision_valid =
			    level_metadata_get_live_route_decision(&next_decision);
			adoption_action =
			    guidebot_route_decision_adoption_action(
			        previous_decision_valid, &previous_decision,
			        previous_certificate_valid,
			        next_decision_valid, &next_decision);
#ifdef __ANDROID__
			if (debug_log_enabled[DLOG_GUIDEBOT])
				debug_log(
				    DLOG_GUIDEBOT,
				    "route_adoption action=%d previous_valid=%d "
				    "previous_certificate=%d next_valid=%d "
				    "same_guidance=%d previous_target=%d next_target=%d "
				    "reason=%s",
				    adoption_action, previous_decision_valid,
				    previous_certificate_valid, next_decision_valid,
				    previous_decision_valid && next_decision_valid &&
				        guidebot_route_decision_guidance_equal(
				            &previous_decision, &next_decision),
				    previous_decision_valid
				        ? previous_decision.objective_segment
				        : -1,
				    next_decision_valid ? next_decision.objective_segment : -1,
				    Escort_route_last_replan_reason);
#endif

			if (adoption_action ==
			    GUIDEBOT_ROUTE_ADOPTION_RETAIN_PATH) {
				if (!Escort_route_goal.active ||
				    (next_decision_valid &&
				     next_decision.status ==
				         GUIDEBOT_ROUTE_DECISION_CALCULATING))
					Escort_route_goal = previous_goal;
				else {
					Escort_route_goal.path_endpoint_seg =
					    previous_goal.path_endpoint_seg;
					Escort_route_goal.path_endpoint_pos_valid =
					    previous_goal.path_endpoint_pos_valid;
					Escort_route_goal.path_endpoint_pos =
					    previous_goal.path_endpoint_pos;
				}
				Escort_goal_object = previous_goal_object;
				Escort_route_path_retained_count++;
			} else if (adoption_action ==
			           GUIDEBOT_ROUTE_ADOPTION_REPLACE_PATH) {
				Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
				Escort_route_path_replaced_count++;
			} else {
				Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
				escort_route_clear_goal();
				escort_route_stop_invalid_path();
				Escort_route_invalid_path_stopped_count++;
			}
		}
		if (Escort_route_dirty_notification_time_valid &&
		    !Escort_route_metadata_dirty) {
			fix64 latency =
			    GameTime64 - Escort_route_first_dirty_notification_time;

			if (latency < 0)
				latency = 0;
			if (latency > 0xffffffffLL)
				latency = 0xffffffffLL;
			Escort_route_publish_latency_last_ticks =
			    (unsigned int) latency;
			if (Escort_route_publish_latency_last_ticks >
			    Escort_route_publish_latency_max_ticks)
				Escort_route_publish_latency_max_ticks =
				    Escort_route_publish_latency_last_ticks;
			Escort_route_publish_latency_sample_count++;
			Escort_route_dirty_notification_time_valid = 0;
		}
		return;
	}
}

void escort_route_note_path_endpoint(object *objp)
{
	ai_static *aip;
	point_seg *endpoint;

	if (!Escort_route_goal.active || !objp)
		return;
	aip = &objp->ctype.ai_info;
	Escort_route_goal.path_endpoint_seg = -1;
	Escort_route_goal.path_endpoint_pos_valid = 0;
	if (aip->hide_index >= 0 && aip->path_length > 0) {
		endpoint = &Point_segs[aip->hide_index + aip->path_length - 1];
		Escort_route_goal.path_endpoint_seg = endpoint->segnum;
		Escort_route_goal.path_endpoint_pos = endpoint->point;
		Escort_route_goal.path_endpoint_pos_valid = 1;
	}
	else if (escort_valid_segment(Escort_route_goal.target_seg))
		Escort_route_goal.path_endpoint_seg = Escort_route_goal.target_seg;
}

void escort_route_apply_target_pos(object *objp)
{
	ai_static *aip;
	point_seg *endpoint;

	if (!Escort_route_goal.active || !Escort_route_goal.target_pos_valid || !objp)
		return;
	aip = &objp->ctype.ai_info;
	if (aip->hide_index < 0 || aip->path_length <= 0)
		return;
	endpoint = &Point_segs[aip->hide_index + aip->path_length - 1];
	if (endpoint->segnum != Escort_route_goal.target_seg ||
	    find_point_seg(&Escort_route_goal.target_pos, endpoint->segnum) !=
	        endpoint->segnum)
		return;
	endpoint->point = Escort_route_goal.target_pos;
}

#endif
