/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Escort robot behavior.
 *
 */

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
#include "escort_exit_policy.h"
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
#include "android_menu_scale.h"
extern volatile int g_guidebot_navigate_nearest_point;
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

extern void multi_send_stolen_items();
void say_escort_goal(int goal_num);
struct escort_menu;
static void show_escort_menu(struct escort_menu *menu);
extern fix64 Buddy_last_seen_player, Buddy_last_player_path_created;


static const char *const Escort_goal_text[MAX_ESCORT_GOALS] = {
	"BLUE KEY",
	"YELLOW KEY",
	"RED KEY",
	"REACTOR",
	"EXIT",
	"ENERGY",
	"ENERGYCEN",
	"SHIELD",
	"POWERUP",
	"ROBOT",
	"HOSTAGES",
	"SPEW",
	"SCRAM",
	"EXIT",
	"BOSS",
	"MARKER 1",
	"MARKER 2",
	"MARKER 3",
	"MARKER 4",
	"MARKER 5",
	"MARKER 6",
	"MARKER 7",
	"MARKER 8",
	"MARKER 9",
	"SECRET",
// -- too much work -- 	"KAMIKAZE  "
};

int	Max_escort_length = 200;
int	Escort_kill_object = -1;
ubyte Stolen_items[MAX_STOLEN_ITEMS];
int	Stolen_item_index;
fix64	Escort_last_path_created = 0;
int	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED, Escort_special_goal = -1, Escort_goal_index = -1, Buddy_messages_suppressed = 0;
static int Escort_goal_secret_seg = -1;
static int Escort_goal_secret_side = -1;
fix64	Buddy_sorry_time;
int	Buddy_objnum, Buddy_allowed_to_talk;
int	Looking_for_marker;
int	Last_buddy_key;
#ifdef NETWORK
int	Escort_owner_player = -1;
static unsigned int Escort_owner_generation;
static int Escort_network_target_mode;
static void escort_apply_multiplayer_owner(int new_owner, int target_mode);
static int escort_owner_candidate_eligible(int pnum);
#endif

fix64	Last_buddy_message_time;

#define ESCORT_KEY_FLAGS (PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_GOLD_KEY | PLAYER_FLAGS_RED_KEY)

static int escort_is_companion_object(int objnum)
{
	if (objnum < 0 || objnum > Highest_object_index)
		return 0;
	if (Objects[objnum].type != OBJ_ROBOT)
		return 0;
	return Robot_info[Objects[objnum].id].companion;
}

static int escort_find_companion_object(void)
{
	int i;

	for (i = 0; i <= Highest_object_index; i++)
		if (escort_is_companion_object(i))
			return i;

	return -1;
}

static int escort_refresh_buddy_objnum(void)
{
	if (!escort_is_companion_object(Buddy_objnum))
		Buddy_objnum = escort_find_companion_object();
	return Buddy_objnum != -1;
}

static int escort_reactor_exists(void);
static int escort_goal_command_allowed(void);
int find_exit_segment(void);

#ifdef __ANDROID__
enum escort_route_guidance_mode {
	ESCORT_ROUTE_GUIDANCE_NONE = 0,
	ESCORT_ROUTE_GUIDANCE_REACH_OBJECTIVE = 1,
	ESCORT_ROUTE_GUIDANCE_REACH_HIDDEN_DOOR = 2,
	ESCORT_ROUTE_GUIDANCE_REACH_FIRING_POSITION = 3,
	ESCORT_ROUTE_GUIDANCE_NEAREST_PROGRESS_POINT = 4
};

#define ESCORT_ROUTE_OBJECTIVE_UNEXPLORED 1000

enum escort_route_target_mode {
	ESCORT_ROUTE_TARGET_END_OF_LEVEL = 0,
	ESCORT_ROUTE_TARGET_UNEXPLORED = 1
};

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
	int guidance_mode;
	int guidance_seg;
	int guidance_side;
	int path_endpoint_seg;
	char label[LEVEL_METADATA_ROUTE_LABEL_LEN];
} escort_route_goal;

static escort_route_goal Escort_route_goal;
static escort_unexplored_route_target Escort_unexplored_route_target;
static int Escort_route_target_mode = ESCORT_ROUTE_TARGET_END_OF_LEVEL;
static int Escort_route_target_mode_restore_pending;
static int Escort_route_metadata_dirty = 1;
static unsigned int Escort_route_metadata_rescan_count;
static unsigned int Escort_route_guidance_full_search_count;
static unsigned int Escort_route_selector_compare_count;
static unsigned int Escort_route_selector_mismatch_count;
static int Escort_route_selector_shared_index = -1;
static int Escort_route_selector_legacy_index = -1;
static int Escort_route_selector_mismatch_shared_index = -1;
static int Escort_route_selector_mismatch_legacy_index = -1;
static int Escort_route_selector_mismatch_shared_goal = ESCORT_GOAL_UNSPECIFIED;
static int Escort_route_selector_mismatch_legacy_goal = ESCORT_GOAL_UNSPECIFIED;
static const char *Escort_route_last_replan_reason = "level_start";

static void escort_unexplored_route_target_clear(escort_unexplored_route_target *target)
{
	if (!target)
		return;
	memset(target, 0, sizeof(*target));
	target->target_seg = -1;
	target->waypoint_seg = -1;
}

static void escort_route_set_target_mode(int target_mode)
{
	Escort_route_target_mode = target_mode;
	if (target_mode != ESCORT_ROUTE_TARGET_UNEXPLORED)
		escort_unexplored_route_target_clear(&Escort_unexplored_route_target);
}

static void escort_route_note_replan(const char *reason)
{
	Escort_route_metadata_dirty = 1;
	Escort_route_last_replan_reason = reason && reason[0] ? reason : "unknown";
}

static void escort_route_sync_target_mode(void)
{
#ifdef NETWORK
	if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player == Player_num)
		multi_send_escort_owner(Escort_owner_player);
#endif
}

static void escort_route_clear_goal(void)
{
	memset(&Escort_route_goal, 0, sizeof(Escort_route_goal));
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
	Escort_route_goal.guidance_mode = ESCORT_ROUTE_GUIDANCE_NONE;
	Escort_route_goal.guidance_seg = -1;
	Escort_route_goal.guidance_side = -1;
	Escort_route_goal.path_endpoint_seg = -1;
}

static const char *escort_route_goal_label(void)
{
	return Escort_route_goal.label[0] ? Escort_route_goal.label : "route objective";
}

static const char *escort_route_goal_instruction(void)
{
	if (Escort_route_goal.objective_kind == ESCORT_ROUTE_OBJECTIVE_UNEXPLORED)
		return "unexplored";
	switch (Escort_route_goal.activation_kind) {
		case LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH:
			return "go here and shoot this switch";
		case LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER:
			return "fly through this trigger";
		case LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER:
			return "pass through this trigger";
		case LEVEL_METADATA_ROUTE_ACTIVATION_OPEN_HIDDEN_DOOR:
			return "open this hidden door";
		case LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_REACTOR:
			return "destroy the reactor";
		case LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BOSS:
			return "destroy the boss";
		case LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT:
			return "enter the exit";
		default:
			return escort_route_goal_label();
	}
}

#ifdef __ANDROID__
static int escort_start_default_goal_now(void);
#endif
static int escort_route_legacy_next_goal_for_key_flags(int key_flags, int set_goal, int *selected_index);
static int escort_route_shared_next_goal(int set_goal, int *selected_index);
static void escort_route_refresh_metadata(void);
static int escort_route_step_reachable(const level_metadata_route_step *step, int guidance_mode);
static object *escort_route_reference_object(void);
static int escort_key_owner_player(void);
static int escort_owned_key_flags(void);
static int escort_key_exists(int powerup_id);
static int escort_boss_exists(void);

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

unsigned int escort_get_route_selector_compare_count(void)
{
	return Escort_route_selector_compare_count;
}

unsigned int escort_get_route_selector_mismatch_count(void)
{
	return Escort_route_selector_mismatch_count;
}

int escort_get_route_selector_shared_index(void)
{
	return Escort_route_selector_shared_index;
}

int escort_get_route_selector_legacy_index(void)
{
	return Escort_route_selector_legacy_index;
}

int escort_get_route_selector_mismatch_shared_index(void)
{
	return Escort_route_selector_mismatch_shared_index;
}

int escort_get_route_selector_mismatch_legacy_index(void)
{
	return Escort_route_selector_mismatch_legacy_index;
}

int escort_get_route_selector_mismatch_shared_goal(void)
{
	return Escort_route_selector_mismatch_shared_goal;
}

int escort_get_route_selector_mismatch_legacy_goal(void)
{
	return Escort_route_selector_mismatch_legacy_goal;
}

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

void escort_route_step_analysis_clear(escort_route_step_analysis *analysis)
{
	if (!analysis)
		return;
	memset(analysis, 0, sizeof(*analysis));
	analysis->index = -1;
	analysis->kind = -1;
	analysis->activation_kind = LEVEL_METADATA_ROUTE_ACTIVATION_NONE;
	analysis->satisfied_reason = ESCORT_ROUTE_STEP_REASON_NONE;
	analysis->reachable = -1;
	analysis->guidance_mode = ESCORT_ROUTE_GUIDANCE_NONE;
	analysis->key_index = -1;
	analysis->key_owned = -1;
	analysis->key_exists = -1;
	analysis->trigger_num = -1;
	analysis->trigger_flags = -1;
	analysis->trigger_disabled = -1;
	analysis->linked_wall_count = 0;
	analysis->linked_walls_passable = -1;
	analysis->first_blocking_link = -1;
	analysis->first_blocking_seg = -1;
	analysis->first_blocking_side = -1;
	analysis->first_blocking_wall = -1;
}

void escort_route_link_analysis_clear(escort_route_link_analysis *analysis)
{
	if (!analysis)
		return;
	memset(analysis, 0, sizeof(*analysis));
	analysis->index = -1;
	analysis->seg = -1;
	analysis->side = -1;
	analysis->wall = -1;
}

const char *escort_route_step_satisfied_reason_name(int reason)
{
	switch (reason) {
		case ESCORT_ROUTE_STEP_REASON_START:
			return "start";
		case ESCORT_ROUTE_STEP_REASON_KEY_OWNED:
			return "key_owned";
		case ESCORT_ROUTE_STEP_REASON_KEY_MISSING:
			return "key_missing";
		case ESCORT_ROUTE_STEP_REASON_TRIGGER_DISABLED:
			return "trigger_disabled";
		case ESCORT_ROUTE_STEP_REASON_LINKS_PASSABLE:
			return "links_passable";
		case ESCORT_ROUTE_STEP_REASON_LINKS_BLOCKED:
			return "links_blocked";
		case ESCORT_ROUTE_STEP_REASON_LATER_REACHABLE:
			return "later_reachable";
		case ESCORT_ROUTE_STEP_REASON_REACTOR_DESTROYED:
			return "reactor_destroyed";
		case ESCORT_ROUTE_STEP_REASON_REACTOR_ALIVE:
			return "reactor_alive";
		case ESCORT_ROUTE_STEP_REASON_BOSS_DESTROYED:
			return "boss_destroyed";
		case ESCORT_ROUTE_STEP_REASON_BOSS_ALIVE:
			return "boss_alive";
		case ESCORT_ROUTE_STEP_REASON_EXIT_PENDING:
			return "exit_pending";
		case ESCORT_ROUTE_STEP_REASON_NOT_APPLICABLE:
			return "not_applicable";
		case ESCORT_ROUTE_STEP_REASON_INVALID:
			return "invalid";
		default:
			return "none";
	}
}

static int escort_valid_segment(int segnum)
{
	return segnum >= 0 && segnum <= Highest_segment_index;
}

static int escort_valid_side(int sidenum)
{
	return sidenum >= 0 && sidenum < MAX_SIDES_PER_SEGMENT;
}

static int escort_valid_wall(int wall_num)
{
	return wall_num >= 0 && wall_num < Num_walls;
}

static int escort_route_link_passable(int segnum, int sidenum)
{
	object *reference_obj;

	if (segnum < 0 || segnum > Highest_segment_index ||
	    sidenum < 0 || sidenum >= MAX_SIDES_PER_SEGMENT)
		return 1;
	reference_obj = escort_route_reference_object();
	if (!reference_obj)
		return 0;
	return level_metadata_route_edge_cost_from_object(
	           reference_obj - Objects, segnum, sidenum) ==
	       LEVEL_METADATA_ROUTE_EDGE_PASSABLE;
}

static int escort_route_key_flag(int key_index)
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

static int escort_route_key_powerup_id(int key_index)
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

static int escort_route_key_goal(int key_index)
{
	return key_index == 0 ? ESCORT_GOAL_BLUE_KEY :
	       key_index == 2 ? ESCORT_GOAL_GOLD_KEY :
	                        ESCORT_GOAL_RED_KEY;
}

static int escort_route_goal_object_for_step(const level_metadata_route_step *step)
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
		case LEVEL_METADATA_ROUTE_HOSTAGE:
			return ESCORT_GOAL_HOSTAGE;
		case LEVEL_METADATA_ROUTE_UNEXPLORED:
			return ESCORT_GOAL_EXIT;
		default:
			return ESCORT_GOAL_EXIT;
	}
}

static int escort_route_step_guidance_mode(const level_metadata_route_step *step)
{
	if (!step)
		return ESCORT_ROUTE_GUIDANCE_NONE;
	switch (step->kind) {
		case LEVEL_METADATA_ROUTE_TRIGGER:
			switch (step->activation_kind) {
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
		case LEVEL_METADATA_ROUTE_KEY:
		case LEVEL_METADATA_ROUTE_REACTOR:
		case LEVEL_METADATA_ROUTE_BOSS:
		case LEVEL_METADATA_ROUTE_EXIT:
		case LEVEL_METADATA_ROUTE_HOSTAGE:
		case LEVEL_METADATA_ROUTE_UNEXPLORED:
			return ESCORT_ROUTE_GUIDANCE_REACH_OBJECTIVE;
		default:
			return ESCORT_ROUTE_GUIDANCE_NONE;
	}
}

static int escort_route_step_link_count(const level_metadata_route_step *step)
{
	if (!step)
		return 0;
	if (step->opened_link_count > 0)
		return step->opened_link_count < LEVEL_METADATA_MAX_ROUTE_LINKS ?
		       step->opened_link_count :
		       LEVEL_METADATA_MAX_ROUTE_LINKS;
	return step->side >= 0 ? 1 : 0;
}

static int escort_route_step_link_at(const level_metadata_route_step *step, int link_index, int *seg, int *side, int *wall)
{
	int link_count;

	if (seg)
		*seg = -1;
	if (side)
		*side = -1;
	if (wall)
		*wall = -1;
	link_count = escort_route_step_link_count(step);
	if (!step || link_index < 0 || link_index >= link_count)
		return 0;
	if (step->opened_link_count > 0) {
		if (seg)
			*seg = step->opened_link_seg[link_index];
		if (side)
			*side = step->opened_link_side[link_index];
		if (wall)
			*wall = step->opened_link_wall[link_index];
		return 1;
	}
	if (seg)
		*seg = step->seg;
	if (side)
		*side = step->side;
	if (wall)
		*wall = step->wall_num;
	return 1;
}

static int escort_route_step_links_passable(const level_metadata_route_step *step, escort_route_step_analysis *analysis)
{
	int link_count = escort_route_step_link_count(step);
	int link;

	if (analysis) {
		analysis->linked_wall_count = link_count;
		analysis->linked_walls_passable = 1;
	}
	for (link = 0; link < link_count; link++) {
		int seg, side, wall;
		int passable;
		escort_route_step_link_at(step, link, &seg, &side, &wall);
		passable = escort_route_link_passable(seg, side);
		if (passable)
			continue;
		if (analysis) {
			analysis->linked_walls_passable = 0;
			analysis->first_blocking_link = link;
			analysis->first_blocking_seg = seg;
			analysis->first_blocking_side = side;
			analysis->first_blocking_wall = wall;
		}
		return 0;
	}
	return 1;
}

static int escort_route_step_is_targetable(const level_metadata_route_step *step)
{
	if (!step)
		return 0;
	switch (step->kind) {
		case LEVEL_METADATA_ROUTE_KEY:
		case LEVEL_METADATA_ROUTE_TRIGGER:
		case LEVEL_METADATA_ROUTE_HIDDEN_DOOR:
		case LEVEL_METADATA_ROUTE_REACTOR:
		case LEVEL_METADATA_ROUTE_BOSS:
		case LEVEL_METADATA_ROUTE_EXIT:
		case LEVEL_METADATA_ROUTE_HOSTAGE:
		case LEVEL_METADATA_ROUTE_UNEXPLORED:
			return escort_valid_segment(step->seg) || escort_valid_wall(step->wall_num);
		default:
			return 0;
	}
}

static void escort_route_analyze_step_for_key_flags(
    const level_metadata_route_step *step,
    int step_index,
    int key_flags,
    int calculate_reachability,
    escort_route_step_analysis *analysis)
{
	int key_flag;
	int key_powerup;

	escort_route_step_analysis_clear(analysis);
	if (!analysis)
		return;
	analysis->index = step_index;
	if (!step) {
		analysis->satisfied = 1;
		analysis->satisfied_reason = ESCORT_ROUTE_STEP_REASON_INVALID;
		return;
	}
	analysis->valid = 1;
	analysis->kind = step->kind;
	analysis->activation_kind = step->activation_kind;
	analysis->key_index = step->key_index;
	analysis->trigger_num = step->trigger_num;
	analysis->guidance_mode = escort_route_step_guidance_mode(step);
	if (calculate_reachability)
		analysis->reachable = escort_route_step_reachable(step, analysis->guidance_mode);

	switch (step->kind) {
		case LEVEL_METADATA_ROUTE_START:
			analysis->satisfied = 1;
			analysis->satisfied_reason = ESCORT_ROUTE_STEP_REASON_START;
			break;
		case LEVEL_METADATA_ROUTE_KEY:
			key_flag = escort_route_key_flag(step->key_index);
			key_powerup = escort_route_key_powerup_id(step->key_index);
			analysis->key_owned = key_flag ? ((key_flags & key_flag) != 0) : 1;
			analysis->key_exists = key_powerup >= 0 ? escort_key_exists(key_powerup) : 0;
			analysis->satisfied = analysis->key_owned;
			analysis->satisfied_reason = analysis->satisfied ?
			                              ESCORT_ROUTE_STEP_REASON_KEY_OWNED :
			                              ESCORT_ROUTE_STEP_REASON_KEY_MISSING;
			break;
		case LEVEL_METADATA_ROUTE_TRIGGER:
			if (step->trigger_num < 0 || step->trigger_num >= Num_triggers) {
				analysis->satisfied = 1;
				analysis->satisfied_reason = ESCORT_ROUTE_STEP_REASON_INVALID;
				break;
			}
			analysis->trigger_flags = Triggers[step->trigger_num].flags;
			analysis->trigger_disabled = (Triggers[step->trigger_num].flags & TF_DISABLED) != 0;
			analysis->satisfied = escort_route_step_links_passable(step, analysis);
			if (analysis->satisfied) {
				analysis->satisfied_reason = ESCORT_ROUTE_STEP_REASON_LINKS_PASSABLE;
			} else {
				analysis->satisfied_reason = ESCORT_ROUTE_STEP_REASON_LINKS_BLOCKED;
			}
			break;
		case LEVEL_METADATA_ROUTE_HIDDEN_DOOR:
			analysis->satisfied = escort_route_step_links_passable(step, analysis);
			analysis->satisfied_reason = analysis->satisfied ?
			                              ESCORT_ROUTE_STEP_REASON_LINKS_PASSABLE :
			                              ESCORT_ROUTE_STEP_REASON_LINKS_BLOCKED;
			break;
		case LEVEL_METADATA_ROUTE_REACTOR:
			analysis->satisfied = Control_center_destroyed != 0 || !escort_reactor_exists();
			analysis->satisfied_reason = analysis->satisfied ?
			                              ESCORT_ROUTE_STEP_REASON_REACTOR_DESTROYED :
			                              ESCORT_ROUTE_STEP_REASON_REACTOR_ALIVE;
			break;
		case LEVEL_METADATA_ROUTE_BOSS:
			analysis->satisfied = Control_center_destroyed != 0 || !escort_boss_exists();
			analysis->satisfied_reason = analysis->satisfied ?
			                              ESCORT_ROUTE_STEP_REASON_BOSS_DESTROYED :
			                              ESCORT_ROUTE_STEP_REASON_BOSS_ALIVE;
			break;
		case LEVEL_METADATA_ROUTE_EXIT:
			analysis->satisfied = 0;
			analysis->satisfied_reason = ESCORT_ROUTE_STEP_REASON_EXIT_PENDING;
			break;
		case LEVEL_METADATA_ROUTE_UNEXPLORED:
			analysis->satisfied = escort_valid_segment(step->seg) && Automap_visited[step->seg] != 0;
			analysis->satisfied_reason = analysis->satisfied ?
			                              ESCORT_ROUTE_STEP_REASON_LATER_REACHABLE :
			                              ESCORT_ROUTE_STEP_REASON_NOT_APPLICABLE;
			break;
		default:
			analysis->satisfied = 0;
			analysis->satisfied_reason = ESCORT_ROUTE_STEP_REASON_NOT_APPLICABLE;
			break;
	}
}

static void escort_route_set_step_goal(const level_metadata_route_step *step, int guidance_mode)
{
	escort_route_goal previous = Escort_route_goal;
	int objective_kind = step->kind == LEVEL_METADATA_ROUTE_UNEXPLORED ?
	                     ESCORT_ROUTE_OBJECTIVE_UNEXPLORED : step->kind;
	int objective_seg = escort_valid_wall(step->wall_num) ? Walls[step->wall_num].segnum :
	                    step->kind == LEVEL_METADATA_ROUTE_UNEXPLORED && Escort_unexplored_route_target.active ?
	                    Escort_unexplored_route_target.target_seg : step->seg;
	int preserve_firing_position = previous.active &&
	                               previous.objective_kind == objective_kind &&
	                               previous.objective_seg == objective_seg &&
	                               previous.activation_kind == step->activation_kind &&
	                               previous.objective_wall == step->wall_num &&
	                               previous.objective_trigger == step->trigger_num &&
	                               previous.guidance_mode == ESCORT_ROUTE_GUIDANCE_REACH_FIRING_POSITION &&
	                               escort_valid_segment(previous.guidance_seg);

	escort_route_clear_goal();
	Escort_route_goal.active = 1;
	Escort_route_goal.target_seg = step->seg;
	Escort_route_goal.target_side = step->side;
	Escort_route_goal.target_wall = step->wall_num;
	Escort_route_goal.trigger_num = step->trigger_num;
	Escort_route_goal.objective_kind = step->kind;
	Escort_route_goal.activation_kind = step->activation_kind;
	Escort_route_goal.objective_seg = step->seg;
	Escort_route_goal.objective_side = step->side;
	Escort_route_goal.objective_wall = step->wall_num;
	Escort_route_goal.objective_trigger = step->trigger_num;
	Escort_route_goal.guidance_mode = guidance_mode;
	Escort_route_goal.guidance_seg = step->seg;
	Escort_route_goal.guidance_side = step->side;
	if (step->kind == LEVEL_METADATA_ROUTE_UNEXPLORED) {
		Escort_route_goal.objective_kind = ESCORT_ROUTE_OBJECTIVE_UNEXPLORED;
		if (Escort_unexplored_route_target.active)
			Escort_route_goal.objective_seg = Escort_unexplored_route_target.target_seg;
	}
	if (escort_valid_wall(step->wall_num)) {
		Escort_route_goal.objective_seg = Walls[step->wall_num].segnum;
		Escort_route_goal.objective_side = Walls[step->wall_num].sidenum;
	}
	if (preserve_firing_position) {
		Escort_route_goal.target_seg = previous.guidance_seg;
		Escort_route_goal.target_side = previous.guidance_side;
		Escort_route_goal.guidance_mode = previous.guidance_mode;
		Escort_route_goal.guidance_seg = previous.guidance_seg;
		Escort_route_goal.guidance_side = previous.guidance_side;
	}
	snprintf(Escort_route_goal.label, sizeof(Escort_route_goal.label), "%s",
	         step->label[0] ? step->label : "route objective");
}

static int escort_route_legacy_next_goal_for_key_flags(int key_flags, int set_goal, int *selected_index)
{
	const level_metadata_state *metadata = level_metadata_get_live_route_state();
	int i;
	int unexplored_mode = Escort_route_target_mode == ESCORT_ROUTE_TARGET_UNEXPLORED;

	if (selected_index)
		*selected_index = -1;
	if (unexplored_mode && !Escort_unexplored_route_target.active) {
		if (set_goal)
			escort_route_clear_goal();
		return ESCORT_GOAL_UNSPECIFIED;
	}
	if (!metadata || metadata->route_step_count <= 0) {
		if (set_goal)
			escort_route_clear_goal();
		return ESCORT_GOAL_UNSPECIFIED;
	}
	for (i = 0; i < metadata->route_step_count && i < LEVEL_METADATA_MAX_ROUTE_STEPS; i++) {
		const level_metadata_route_step *step = &metadata->route_steps[i];
		escort_route_step_analysis analysis;

		escort_route_analyze_step_for_key_flags(step, i, key_flags, 0, &analysis);
		switch (step->kind) {
			case LEVEL_METADATA_ROUTE_START:
				break;
			case LEVEL_METADATA_ROUTE_KEY:
				if (!analysis.satisfied) {
					if (set_goal && escort_valid_segment(step->seg))
						escort_route_set_step_goal(step, analysis.guidance_mode);
					if (selected_index)
						*selected_index = i;
					return escort_route_key_goal(step->key_index);
				}
				break;
			case LEVEL_METADATA_ROUTE_TRIGGER:
				if (!analysis.satisfied && escort_valid_segment(step->seg)) {
					if (set_goal)
						escort_route_set_step_goal(step, analysis.guidance_mode);
					if (selected_index)
						*selected_index = i;
					return escort_route_goal_object_for_step(step);
				}
				break;
			case LEVEL_METADATA_ROUTE_HIDDEN_DOOR:
			case LEVEL_METADATA_ROUTE_REACTOR:
			case LEVEL_METADATA_ROUTE_BOSS:
			case LEVEL_METADATA_ROUTE_EXIT:
			case LEVEL_METADATA_ROUTE_HOSTAGE:
			case LEVEL_METADATA_ROUTE_UNEXPLORED:
				if (!analysis.satisfied && escort_route_step_is_targetable(step)) {
					int guidance_mode = analysis.guidance_mode;

					if (set_goal)
						escort_route_set_step_goal(step, guidance_mode);
					if (selected_index)
						*selected_index = i;
					return escort_route_goal_object_for_step(step);
				}
				break;
			default:
				if (set_goal)
					escort_route_clear_goal();
				return ESCORT_GOAL_UNSPECIFIED;
		}
	}
	if (set_goal)
		escort_route_clear_goal();
	return ESCORT_GOAL_UNSPECIFIED;
}

static int escort_route_shared_next_goal(int set_goal, int *selected_index)
{
	const level_metadata_state *metadata = level_metadata_get_live_route_state();
	route_planner_plan_summary summary;
	const level_metadata_route_step *step;
	int guidance_mode;
	int index;

	if (selected_index)
		*selected_index = -1;
	if (!metadata || !level_metadata_get_live_route_plan_summary(&summary)) {
		if (set_goal)
			escort_route_clear_goal();
		return ESCORT_GOAL_UNSPECIFIED;
	}
	index = summary.first_pending_step;
	if (index < 0 || index >= metadata->route_step_count ||
	    index >= LEVEL_METADATA_MAX_ROUTE_STEPS) {
		if (set_goal)
			escort_route_clear_goal();
		return ESCORT_GOAL_UNSPECIFIED;
	}
	step = &metadata->route_steps[index];
	if (!escort_route_step_is_targetable(step)) {
		if (set_goal)
			escort_route_clear_goal();
		return ESCORT_GOAL_UNSPECIFIED;
	}
	guidance_mode = escort_route_step_guidance_mode(step);
	if (set_goal)
		escort_route_set_step_goal(step, guidance_mode);
	if (selected_index)
		*selected_index = index;
	return escort_route_goal_object_for_step(step);
}

static int escort_route_next_goal(int key_flags)
{
	const level_metadata_state *metadata;
	escort_route_step_analysis analysis;
	int shared_index = -1;
	int legacy_index = -1;
	int shared_goal;
	int legacy_goal;

	escort_route_shared_next_goal(0, &shared_index);
	metadata = level_metadata_get_live_route_state();
	if (metadata && shared_index >= 0 &&
	    shared_index < metadata->route_step_count) {
		escort_route_analyze_step_for_key_flags(
		    &metadata->route_steps[shared_index], shared_index, key_flags, 0,
		    &analysis);
		if (analysis.satisfied) {
			escort_route_note_replan("shared_waypoint_satisfied");
			escort_route_refresh_metadata();
		}
	}
	shared_goal = escort_route_shared_next_goal(1, &shared_index);
	legacy_goal = escort_route_legacy_next_goal_for_key_flags(
	    key_flags, 0, &legacy_index);
	Escort_route_selector_compare_count++;
	Escort_route_selector_shared_index = shared_index;
	Escort_route_selector_legacy_index = legacy_index;
	if (shared_index != legacy_index || shared_goal != legacy_goal) {
		Escort_route_selector_mismatch_count++;
		Escort_route_selector_mismatch_shared_index = shared_index;
		Escort_route_selector_mismatch_legacy_index = legacy_index;
		Escort_route_selector_mismatch_shared_goal = shared_goal;
		Escort_route_selector_mismatch_legacy_goal = legacy_goal;
		ESCORT_DIAG(
		    "route selector mismatch shared=%d/%d legacy=%d/%d",
		    shared_index, shared_goal, legacy_index, legacy_goal);
	}
	return shared_goal;
}

static void escort_route_refresh_metadata(void)
{
#ifdef NETWORK
	if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player != Player_num)
		return;
#endif
	if (Escort_route_target_mode == ESCORT_ROUTE_TARGET_UNEXPLORED &&
	    Escort_unexplored_route_target.active &&
	    escort_valid_segment(Escort_unexplored_route_target.target_seg) &&
	    Automap_visited[Escort_unexplored_route_target.target_seg])
		Escort_route_metadata_dirty = 1;
	if (!Escort_route_metadata_dirty)
		return;
	Escort_route_metadata_dirty = 0;
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

int escort_route_analyze_step(int step_index, escort_route_step_analysis *analysis)
{
	const level_metadata_state *metadata = level_metadata_get_live_route_state();
	route_planner_plan_summary summary;
	int selected_index = -1;
	int key_flags;
	int count;

	escort_route_step_analysis_clear(analysis);
	if (!analysis || !metadata)
		return 0;
	count = metadata->route_step_count;
	if (count > LEVEL_METADATA_MAX_ROUTE_STEPS)
		count = LEVEL_METADATA_MAX_ROUTE_STEPS;
	if (step_index < 0 || step_index >= count)
		return 0;
	key_flags = escort_owned_key_flags();
	if (level_metadata_get_live_route_plan_summary(&summary))
		selected_index = summary.first_pending_step;
	escort_route_analyze_step_for_key_flags(&metadata->route_steps[step_index], step_index, key_flags, 1, analysis);
	analysis->selected_next = step_index == selected_index;
	return 1;
}

int escort_route_analyze_step_link(int step_index, int link_index, escort_route_link_analysis *analysis)
{
	const level_metadata_state *metadata = level_metadata_get_live_route_state();
	const level_metadata_route_step *step;
	int count;

	escort_route_link_analysis_clear(analysis);
	if (!analysis || !metadata)
		return 0;
	count = metadata->route_step_count;
	if (count > LEVEL_METADATA_MAX_ROUTE_STEPS)
		count = LEVEL_METADATA_MAX_ROUTE_STEPS;
	if (step_index < 0 || step_index >= count)
		return 0;
	step = &metadata->route_steps[step_index];
	if (!escort_route_step_link_at(step, link_index, &analysis->seg, &analysis->side, &analysis->wall))
		return 0;
	analysis->valid = 1;
	analysis->index = link_index;
	analysis->passable = escort_route_link_passable(analysis->seg, analysis->side);
	return 1;
}

static int escort_live_side_cost(object *objp, int segnum, int sidenum)
{
	if (!objp || !escort_valid_segment(segnum) || !escort_valid_side(sidenum))
		return -1;
	return level_metadata_route_edge_cost_from_object(
	           objp - Objects, segnum, sidenum) ==
	               LEVEL_METADATA_ROUTE_EDGE_PASSABLE ?
	           0 : -1;
}

static int escort_route_can_see_wall_from_pos(int from_seg, const vms_vector *from_pos, int wall_num)
{
	int pos[3];

	if (!escort_valid_segment(from_seg) || !from_pos || !escort_valid_wall(wall_num))
		return 0;
	pos[0] = from_pos->x;
	pos[1] = from_pos->y;
	pos[2] = from_pos->z;
	return level_metadata_wall_visible_from_position(from_seg, pos, wall_num);
}

static int escort_route_can_see_pos_from_pos(int from_seg, const vms_vector *from_pos, const vms_vector *target)
{
	int from[3];
	int to[3];

	if (!escort_valid_segment(from_seg) || !from_pos || !target)
		return 0;
	from[0] = from_pos->x;
	from[1] = from_pos->y;
	from[2] = from_pos->z;
	to[0] = target->x;
	to[1] = target->y;
	to[2] = target->z;
	return level_metadata_target_visible_from_position(
	    from_seg, from, -1, to);
}

static int escort_route_segment_can_see_wall(int segnum, int wall_num)
{
	static const int sample_weights[] = { 3, 7, 15 };
	vms_vector center, side_center, candidate;
	int edge, sample_index, sidenum, vertex_index;

	if (!escort_valid_segment(segnum) || !escort_valid_wall(wall_num))
		return 0;
	compute_segment_center(&center, &Segments[segnum]);
	if (escort_route_can_see_wall_from_pos(segnum, &center, wall_num))
		return 1;
	for (sidenum = 0; sidenum < MAX_SIDES_PER_SEGMENT; sidenum++) {
		compute_center_point_on_side(&side_center, &Segments[segnum], sidenum);
		for (sample_index = 0; sample_index < (int) (sizeof(sample_weights) / sizeof(sample_weights[0])); ++sample_index) {
			int weight = sample_weights[sample_index];
			candidate.x = (center.x + side_center.x * weight) / (weight + 1);
			candidate.y = (center.y + side_center.y * weight) / (weight + 1);
			candidate.z = (center.z + side_center.z * weight) / (weight + 1);
			if (escort_route_can_see_wall_from_pos(segnum, &candidate, wall_num))
				return 1;
		}
	}
	for (vertex_index = 0; vertex_index < MAX_VERTICES_PER_SEGMENT; vertex_index++) {
		const vms_vector *vertex = &Vertices[Segments[segnum].verts[vertex_index]];
		for (sample_index = 0; sample_index < (int) (sizeof(sample_weights) / sizeof(sample_weights[0])); ++sample_index) {
			int weight = sample_weights[sample_index];
			candidate.x = (center.x + vertex->x * weight) / (weight + 1);
			candidate.y = (center.y + vertex->y * weight) / (weight + 1);
			candidate.z = (center.z + vertex->z * weight) / (weight + 1);
			if (escort_route_can_see_wall_from_pos(segnum, &candidate, wall_num))
				return 1;
		}
	}
	for (sidenum = 0; sidenum < MAX_SIDES_PER_SEGMENT; sidenum++) {
		for (edge = 0; edge < 4; edge++) {
			const vms_vector *first = &Vertices[Segments[segnum].verts[Side_to_verts[sidenum][edge]]];
			const vms_vector *second = &Vertices[Segments[segnum].verts[Side_to_verts[sidenum][(edge + 1) % 4]]];
			vms_vector edge_midpoint;
			edge_midpoint.x = (first->x + second->x) / 2;
			edge_midpoint.y = (first->y + second->y) / 2;
			edge_midpoint.z = (first->z + second->z) / 2;
			for (sample_index = 0; sample_index < (int) (sizeof(sample_weights) / sizeof(sample_weights[0])); ++sample_index) {
				int weight = sample_weights[sample_index];
				candidate.x = (center.x + edge_midpoint.x * weight) / (weight + 1);
				candidate.y = (center.y + edge_midpoint.y * weight) / (weight + 1);
				candidate.z = (center.z + edge_midpoint.z * weight) / (weight + 1);
				if (escort_route_can_see_wall_from_pos(segnum, &candidate, wall_num))
					return 1;
			}
		}
	}
	return 0;
}

static int escort_route_segment_can_see_pos(int segnum, const vms_vector *target)
{
	static const int sample_weights[] = { 3, 7, 15 };
	vms_vector center, side_center, candidate;
	int edge, sample_index, sidenum, vertex_index;

	if (!escort_valid_segment(segnum) || !target)
		return 0;
	compute_segment_center(&center, &Segments[segnum]);
	if (escort_route_can_see_pos_from_pos(segnum, &center, target))
		return 1;
	for (sidenum = 0; sidenum < MAX_SIDES_PER_SEGMENT; sidenum++) {
		compute_center_point_on_side(&side_center, &Segments[segnum], sidenum);
		for (sample_index = 0; sample_index < (int) (sizeof(sample_weights) / sizeof(sample_weights[0])); ++sample_index) {
			int weight = sample_weights[sample_index];
			candidate.x = (center.x + side_center.x * weight) / (weight + 1);
			candidate.y = (center.y + side_center.y * weight) / (weight + 1);
			candidate.z = (center.z + side_center.z * weight) / (weight + 1);
			if (escort_route_can_see_pos_from_pos(segnum, &candidate, target))
				return 1;
		}
	}
	for (vertex_index = 0; vertex_index < MAX_VERTICES_PER_SEGMENT; vertex_index++) {
		const vms_vector *vertex = &Vertices[Segments[segnum].verts[vertex_index]];
		for (sample_index = 0; sample_index < (int) (sizeof(sample_weights) / sizeof(sample_weights[0])); ++sample_index) {
			int weight = sample_weights[sample_index];
			candidate.x = (center.x + vertex->x * weight) / (weight + 1);
			candidate.y = (center.y + vertex->y * weight) / (weight + 1);
			candidate.z = (center.z + vertex->z * weight) / (weight + 1);
			if (escort_route_can_see_pos_from_pos(segnum, &candidate, target))
				return 1;
		}
	}
	for (sidenum = 0; sidenum < MAX_SIDES_PER_SEGMENT; sidenum++) {
		for (edge = 0; edge < 4; edge++) {
			const vms_vector *first = &Vertices[Segments[segnum].verts[Side_to_verts[sidenum][edge]]];
			const vms_vector *second = &Vertices[Segments[segnum].verts[Side_to_verts[sidenum][(edge + 1) % 4]]];
			vms_vector edge_midpoint;
			edge_midpoint.x = (first->x + second->x) / 2;
			edge_midpoint.y = (first->y + second->y) / 2;
			edge_midpoint.z = (first->z + second->z) / 2;
			for (sample_index = 0; sample_index < (int) (sizeof(sample_weights) / sizeof(sample_weights[0])); ++sample_index) {
				int weight = sample_weights[sample_index];
				candidate.x = (center.x + edge_midpoint.x * weight) / (weight + 1);
				candidate.y = (center.y + edge_midpoint.y * weight) / (weight + 1);
				candidate.z = (center.z + edge_midpoint.z * weight) / (weight + 1);
				if (escort_route_can_see_pos_from_pos(segnum, &candidate, target))
					return 1;
			}
		}
	}
	return 0;
}

static int escort_route_find_nearest_visible_wall_segment(object *objp, int wall_num)
{
	int reachable_dist[MAX_SEGMENTS];
	int queue[MAX_SEGMENTS * MAX_SIDES_PER_SEGMENT];
	int qhead, qtail;
	int i, sidenum;
	int start_seg;

	if (!objp || !escort_valid_wall(wall_num))
		return -1;
	start_seg = objp->segnum;
	if (!escort_valid_segment(start_seg))
		return -1;
	for (i = 0; i < MAX_SEGMENTS; i++)
		reachable_dist[i] = -1;
	qhead = qtail = 0;
	queue[qtail++] = start_seg;
	reachable_dist[start_seg] = 0;
	while (qhead < qtail) {
		int segnum = queue[qhead++];

		if (reachable_dist[segnum] <= Max_escort_length &&
		    escort_route_segment_can_see_wall(segnum, wall_num))
			return segnum;
		for (sidenum = 0; sidenum < MAX_SIDES_PER_SEGMENT; sidenum++) {
			int child_segnum;

			if (escort_live_side_cost(objp, segnum, sidenum) != 0)
				continue;
			child_segnum = Segments[segnum].children[sidenum];
			if (escort_valid_segment(child_segnum) && reachable_dist[child_segnum] < 0) {
				reachable_dist[child_segnum] = reachable_dist[segnum] + 1;
				if (qtail < MAX_SEGMENTS * MAX_SIDES_PER_SEGMENT)
					queue[qtail++] = child_segnum;
			}
		}
	}
	return -1;
}

static int escort_route_object_matches_kind(int objnum, int objective_kind)
{
	object *objp;

	if (objnum < 0 || objnum > Highest_object_index)
		return 0;
	objp = &Objects[objnum];
	if (objp->flags & OF_SHOULD_BE_DEAD)
		return 0;
	if (objective_kind == LEVEL_METADATA_ROUTE_REACTOR)
		return objp->type == OBJ_CNTRLCEN;
	if (objective_kind == LEVEL_METADATA_ROUTE_BOSS)
		return objp->type == OBJ_ROBOT && Robot_info[objp->id].boss_flag;
	return 0;
}

static int escort_route_find_object_target(int objective_kind, int preferred_seg, vms_vector *target, int *target_seg)
{
	int objnum;

	if (target_seg)
		*target_seg = -1;
	for (objnum = 0; objnum <= Highest_object_index; objnum++) {
		if (!escort_route_object_matches_kind(objnum, objective_kind) ||
		    (escort_valid_segment(preferred_seg) && Objects[objnum].segnum != preferred_seg))
			continue;
		if (target)
			*target = Objects[objnum].pos;
		if (target_seg)
			*target_seg = Objects[objnum].segnum;
		return 1;
	}
	if (objective_kind == LEVEL_METADATA_ROUTE_REACTOR &&
	    escort_valid_segment(preferred_seg) &&
	    Segment2s[preferred_seg].special == SEGMENT_IS_CONTROLCEN) {
		if (target)
			compute_segment_center(target, &Segments[preferred_seg]);
		if (target_seg)
			*target_seg = preferred_seg;
		return 1;
	}
	for (objnum = 0; objnum <= Highest_object_index; objnum++) {
		if (!escort_route_object_matches_kind(objnum, objective_kind))
			continue;
		if (target)
			*target = Objects[objnum].pos;
		if (target_seg)
			*target_seg = Objects[objnum].segnum;
		return 1;
	}
	return 0;
}

static int escort_route_find_nearest_visible_object_segment(object *objp, int objective_kind, int objective_seg)
{
	int reachable_dist[MAX_SEGMENTS];
	int queue[MAX_SEGMENTS * MAX_SIDES_PER_SEGMENT];
	int qhead, qtail;
	int i, sidenum;
	int start_seg;
	vms_vector target;

	if (!objp || !escort_route_find_object_target(objective_kind, objective_seg, &target, NULL))
		return -1;
	start_seg = objp->segnum;
	if (!escort_valid_segment(start_seg))
		return -1;
	for (i = 0; i < MAX_SEGMENTS; i++)
		reachable_dist[i] = -1;
	qhead = qtail = 0;
	queue[qtail++] = start_seg;
	reachable_dist[start_seg] = 0;
	while (qhead < qtail) {
		int segnum = queue[qhead++];

		if (reachable_dist[segnum] <= Max_escort_length &&
		    escort_route_segment_can_see_pos(segnum, &target))
			return segnum;
		for (sidenum = 0; sidenum < MAX_SIDES_PER_SEGMENT; sidenum++) {
			int child_segnum;

			if (escort_live_side_cost(objp, segnum, sidenum) != 0)
				continue;
			child_segnum = Segments[segnum].children[sidenum];
			if (escort_valid_segment(child_segnum) && reachable_dist[child_segnum] < 0) {
				reachable_dist[child_segnum] = reachable_dist[segnum] + 1;
				if (qtail < MAX_SEGMENTS * MAX_SIDES_PER_SEGMENT)
					queue[qtail++] = child_segnum;
			}
		}
	}
	return -1;
}

static void escort_route_set_guidance_segment(int target_seg, int target_side, int guidance_mode)
{
	if (!Escort_route_goal.active || !escort_valid_segment(target_seg))
		return;
	Escort_route_goal.target_seg = target_seg;
	Escort_route_goal.target_side = target_side;
	Escort_route_goal.guidance_seg = target_seg;
	Escort_route_goal.guidance_side = target_side;
	Escort_route_goal.guidance_mode = guidance_mode;
}

static void escort_route_refresh_guidance_target(object *objp)
{
	int guidance_mode;
	int visible_seg;
	vms_vector target;

	if (!Escort_route_goal.active)
		return;
	Escort_route_goal.path_endpoint_seg = -1;
	if (Escort_route_goal.objective_kind == LEVEL_METADATA_ROUTE_TRIGGER &&
	    Escort_route_goal.activation_kind == LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH &&
	    escort_valid_wall(Escort_route_goal.objective_wall) &&
	    escort_valid_segment(Escort_route_goal.target_seg) &&
	    escort_route_segment_can_see_wall(Escort_route_goal.target_seg, Escort_route_goal.objective_wall)) {
		escort_route_set_guidance_segment(
		    Escort_route_goal.target_seg, -1, ESCORT_ROUTE_GUIDANCE_REACH_FIRING_POSITION);
		return;
	}
	if ((Escort_route_goal.objective_kind == LEVEL_METADATA_ROUTE_REACTOR ||
	     Escort_route_goal.objective_kind == LEVEL_METADATA_ROUTE_BOSS) &&
	    escort_valid_segment(Escort_route_goal.target_seg) &&
	    escort_route_find_object_target(
	        Escort_route_goal.objective_kind, Escort_route_goal.objective_seg, &target, NULL) &&
	    escort_route_segment_can_see_pos(Escort_route_goal.target_seg, &target)) {
		guidance_mode = Escort_route_goal.target_seg == Escort_route_goal.objective_seg ?
		                ESCORT_ROUTE_GUIDANCE_REACH_OBJECTIVE :
		                ESCORT_ROUTE_GUIDANCE_REACH_FIRING_POSITION;
		escort_route_set_guidance_segment(
		    Escort_route_goal.target_seg,
		    guidance_mode == ESCORT_ROUTE_GUIDANCE_REACH_OBJECTIVE ? Escort_route_goal.objective_side : -1,
		    guidance_mode);
		return;
	}
	if (Escort_route_goal.objective_kind != LEVEL_METADATA_ROUTE_TRIGGER ||
	    Escort_route_goal.activation_kind != LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH ||
	    !escort_valid_wall(Escort_route_goal.objective_wall)) {
		if (Escort_route_goal.objective_kind != LEVEL_METADATA_ROUTE_REACTOR &&
		    Escort_route_goal.objective_kind != LEVEL_METADATA_ROUTE_BOSS)
			return;
		Escort_route_guidance_full_search_count++;
		visible_seg = escort_route_find_nearest_visible_object_segment(
		    objp,
		    Escort_route_goal.objective_kind,
		    Escort_route_goal.objective_seg);
		if (!escort_valid_segment(visible_seg))
			return;
		guidance_mode = visible_seg == Escort_route_goal.objective_seg ?
		                ESCORT_ROUTE_GUIDANCE_REACH_OBJECTIVE :
		                ESCORT_ROUTE_GUIDANCE_REACH_FIRING_POSITION;
		escort_route_set_guidance_segment(
		    visible_seg,
		    visible_seg == Escort_route_goal.objective_seg ? Escort_route_goal.objective_side : -1,
		    guidance_mode);
		ESCORT_DIAG("route object guidance kind=%d objective=%d target=%d",
		            Escort_route_goal.objective_kind,
		            Escort_route_goal.objective_seg,
		            visible_seg);
	} else {
		Escort_route_guidance_full_search_count++;
		visible_seg = escort_route_find_nearest_visible_wall_segment(objp, Escort_route_goal.objective_wall);
		if (!escort_valid_segment(visible_seg))
			return;
		escort_route_set_guidance_segment(
		    visible_seg,
		    visible_seg == Escort_route_goal.objective_seg ? Escort_route_goal.objective_side : -1,
		    ESCORT_ROUTE_GUIDANCE_REACH_FIRING_POSITION);
		ESCORT_DIAG("route trigger guidance trigger=%d wall=%d objective=%d:%d target=%d",
		            Escort_route_goal.objective_trigger,
		            Escort_route_goal.objective_wall,
		            Escort_route_goal.objective_seg,
		            Escort_route_goal.objective_side,
		            visible_seg);
	}
}

static void escort_route_note_path_endpoint(object *objp)
{
	ai_static *aip;

	if (!Escort_route_goal.active || !objp)
		return;
	aip = &objp->ctype.ai_info;
	Escort_route_goal.path_endpoint_seg = -1;
	if (aip->hide_index >= 0 && aip->path_length > 0)
		Escort_route_goal.path_endpoint_seg = Point_segs[aip->hide_index + aip->path_length - 1].segnum;
	else if (escort_valid_segment(Escort_route_goal.target_seg))
		Escort_route_goal.path_endpoint_seg = Escort_route_goal.target_seg;
}

static int escort_optimistic_side_cost(object *objp, int segnum, int sidenum)
{
	int edge_cost;

	if (!objp || !escort_valid_segment(segnum) || !escort_valid_side(sidenum))
		return -1;
	edge_cost = level_metadata_route_edge_cost_from_object(objp - Objects, segnum, sidenum);
	return edge_cost == LEVEL_METADATA_ROUTE_EDGE_PASSABLE ? 0 :
	       edge_cost == LEVEL_METADATA_ROUTE_EDGE_PROGRESS ? 1 : -1;
}

static int escort_find_nearest_reachable_goal_segment(object *objp, int goal_seg)
{
	int reachable_dist[MAX_SEGMENTS], optimistic_dist[MAX_SEGMENTS], optimistic_cost[MAX_SEGMENTS];
	int queue[MAX_SEGMENTS * MAX_SIDES_PER_SEGMENT];
	int qhead, qtail;
	int i, sidenum;
	int start_seg = objp->segnum;
	int best_seg = -1, best_dist = 32767, best_cost = 32767, best_reachable_dist = -1;

	if ((goal_seg < 0) || (goal_seg > Highest_segment_index) ||
	    (start_seg < 0) || (start_seg > Highest_segment_index))
		return -1;

	for (i = 0; i < MAX_SEGMENTS; i++) {
		reachable_dist[i] = -1;
		optimistic_dist[i] = -1;
		optimistic_cost[i] = 32767;
	}

	qhead = qtail = 0;
	queue[qtail++] = start_seg;
	reachable_dist[start_seg] = 0;
	while (qhead < qtail) {
		int segnum = queue[qhead++];

		for (sidenum = 0; sidenum < MAX_SIDES_PER_SEGMENT; sidenum++) {
			int child_segnum;

			if (escort_live_side_cost(objp, segnum, sidenum) != 0)
				continue;

			child_segnum = Segments[segnum].children[sidenum];
			if ((child_segnum >= 0) && (child_segnum <= Highest_segment_index) &&
			    (reachable_dist[child_segnum] < 0)) {
				reachable_dist[child_segnum] = reachable_dist[segnum] + 1;
				if (qtail < MAX_SEGMENTS * MAX_SIDES_PER_SEGMENT)
					queue[qtail++] = child_segnum;
			}
		}
	}

	qhead = qtail = 0;
	queue[qtail++] = goal_seg;
	optimistic_dist[goal_seg] = 0;
	optimistic_cost[goal_seg] = 0;
	while (qhead < qtail) {
		int segnum = queue[qhead++];

		for (sidenum = 0; sidenum < MAX_SIDES_PER_SEGMENT; sidenum++) {
			int child_segnum = Segments[segnum].children[sidenum];
			int edge_cost, next_dist, next_cost, reverse_side;

			if ((child_segnum < 0) || (child_segnum > Highest_segment_index))
				continue;

			reverse_side = find_connect_side(&Segments[segnum], &Segments[child_segnum]);
			if (!escort_valid_side(reverse_side))
				continue;
			edge_cost = escort_optimistic_side_cost(objp, child_segnum, reverse_side);
			if (edge_cost < 0)
				continue;

			next_dist = optimistic_dist[segnum] + 1;
			next_cost = optimistic_cost[segnum] + edge_cost;
			if ((optimistic_dist[child_segnum] < 0) ||
			    (next_dist < optimistic_dist[child_segnum]) ||
			    ((next_dist == optimistic_dist[child_segnum]) && (next_cost < optimistic_cost[child_segnum]))) {
				optimistic_dist[child_segnum] = next_dist;
				optimistic_cost[child_segnum] = next_cost;
				if (qtail < MAX_SEGMENTS * MAX_SIDES_PER_SEGMENT)
					queue[qtail++] = child_segnum;
			}
		}
	}

	for (i = 0; i <= Highest_segment_index; i++) {
		if ((reachable_dist[i] <= 0) || (reachable_dist[i] > Max_escort_length) || (optimistic_dist[i] < 0))
			continue;

		if ((optimistic_dist[i] < best_dist) ||
		    ((optimistic_dist[i] == best_dist) && (optimistic_cost[i] < best_cost)) ||
		    ((optimistic_dist[i] == best_dist) && (optimistic_cost[i] == best_cost) && (reachable_dist[i] > best_reachable_dist))) {
			best_seg = i;
			best_dist = optimistic_dist[i];
			best_cost = optimistic_cost[i];
			best_reachable_dist = reachable_dist[i];
		}
	}

	ESCORT_DIAG("nearest guidebot fallback start=%d goal=%d target=%d dist=%d cost=%d reachable=%d",
	            start_seg, goal_seg, best_seg, best_dist, best_cost, best_reachable_dist);
	return best_seg;
}

static int escort_create_path_to_nearest_point(object *objp, int goal_seg)
{
	int target_seg = escort_find_nearest_reachable_goal_segment(objp, goal_seg);
	ai_static *aip = &objp->ctype.ai_info;

	if (target_seg < 0)
		return 0;

	create_path_to_segment_metadata_route(objp, target_seg, Max_escort_length, 1);
	if (aip->path_length > 3)
		aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);

	if ((aip->path_length <= 1) ||
	    (Point_segs[aip->hide_index + aip->path_length - 1].segnum != target_seg)) {
		input_demo_log_escort_path_state("escort_create_path_to_goal nearest_point_failed", objp);
		return 0;
	}

	if (Escort_route_goal.active)
		escort_route_set_guidance_segment(target_seg, -1, ESCORT_ROUTE_GUIDANCE_NEAREST_PROGRESS_POINT);
	input_demo_log_escort_path_state("escort_create_path_to_goal nearest_point", objp);
	return 1;
}

static int escort_should_use_nearest_point_fallback(int using_route_goal)
{
	if (g_guidebot_navigate_nearest_point)
		return 1;
	if (using_route_goal)
		return 1;
	return Escort_goal_object == ESCORT_GOAL_BOSS ||
	       Escort_goal_object == ESCORT_GOAL_CONTROLCEN ||
	       Escort_goal_object == ESCORT_GOAL_EXIT ||
	       Escort_goal_object == ESCORT_GOAL_EXIT2;
}

static void escort_report_nearest_point_fallback(int using_route_goal)
{
	Last_buddy_message_time = 0;
	if (using_route_goal)
		buddy_message("Can't reach next: %s, navigating as close as possible.", escort_route_goal_label());
	else if ((Escort_goal_object == ESCORT_GOAL_EXIT) || (Escort_goal_object == ESCORT_GOAL_EXIT2))
		buddy_message("Can't reach exit, navigating as close as possible.");
	else
		buddy_message("Can't reach %s, navigating as close as possible.", Escort_goal_text[Escort_goal_object-1]);
}

static int escort_try_nearest_point_fallback(object *objp, int goal_seg, int using_route_goal)
{
	if (!escort_should_use_nearest_point_fallback(using_route_goal))
		return 0;
	if (!escort_create_path_to_nearest_point(objp, goal_seg))
		return 0;
	escort_report_nearest_point_fallback(using_route_goal);
	return 1;
}

static object *escort_route_reference_object(void)
{
	if (escort_is_companion_object(Buddy_objnum))
		return &Objects[Buddy_objnum];
	if (ConsoleObject)
		return ConsoleObject;
	return NULL;
}

static int escort_route_segment_reachable_from(object *objp, int goal_seg)
{
	int reachable[MAX_SEGMENTS];
	int queue[MAX_SEGMENTS * MAX_SIDES_PER_SEGMENT];
	int qhead, qtail;
	int i;

	if (!objp || !escort_valid_segment(objp->segnum) || !escort_valid_segment(goal_seg))
		return -1;
	if (objp->segnum == goal_seg)
		return 1;
	for (i = 0; i < MAX_SEGMENTS; i++)
		reachable[i] = 0;
	qhead = qtail = 0;
	queue[qtail++] = objp->segnum;
	reachable[objp->segnum] = 1;
	while (qhead < qtail) {
		int segnum = queue[qhead++];
		int sidenum;

		for (sidenum = 0; sidenum < MAX_SIDES_PER_SEGMENT; sidenum++) {
			int child_segnum;
			if (escort_live_side_cost(objp, segnum, sidenum) != 0)
				continue;
			child_segnum = Segments[segnum].children[sidenum];
			if (!escort_valid_segment(child_segnum) || reachable[child_segnum])
				continue;
			if (child_segnum == goal_seg)
				return 1;
			reachable[child_segnum] = 1;
			if (qtail < MAX_SEGMENTS * MAX_SIDES_PER_SEGMENT)
				queue[qtail++] = child_segnum;
		}
	}
	return 0;
}

static int escort_route_step_reachable(const level_metadata_route_step *step, int guidance_mode)
{
	object *objp = escort_route_reference_object();
	int target_seg;

	if (!step || !objp)
		return -1;
	target_seg = step->seg;
	if (step->kind == LEVEL_METADATA_ROUTE_TRIGGER &&
	    guidance_mode == ESCORT_ROUTE_GUIDANCE_REACH_FIRING_POSITION &&
	    escort_valid_wall(step->wall_num)) {
		int visible_seg = escort_route_find_nearest_visible_wall_segment(objp, step->wall_num);
		if (escort_valid_segment(visible_seg))
			target_seg = visible_seg;
	} else if (step->kind == LEVEL_METADATA_ROUTE_REACTOR ||
	           step->kind == LEVEL_METADATA_ROUTE_BOSS) {
		int visible_seg = escort_route_find_nearest_visible_object_segment(objp, step->kind, step->seg);
		if (escort_valid_segment(visible_seg))
			target_seg = visible_seg;
	}
	if (!escort_valid_segment(target_seg) && escort_valid_wall(step->wall_num))
		target_seg = Walls[step->wall_num].segnum;
	return escort_route_segment_reachable_from(objp, target_seg);
}
#endif

#define STOLEN_ITEM_NONE 255
#define STOLEN_ITEM_PROXIMITY_MINE 254
#define STOLEN_ITEM_SMART_MINE 253

static int thief_full_drop_enabled(void)
{
#ifdef NETWORK
	return (Game_mode & GM_MULTI) && Netgame.FullDeathSpew;
#else
	return 0;
#endif
}

static int thief_find_stolen_item_slot(void)
{
	int i;

	if (!thief_full_drop_enabled())
		return Stolen_item_index;

	for (i=0; i<MAX_STOLEN_ITEMS; i++) {
		int slot = (Stolen_item_index+i) % MAX_STOLEN_ITEMS;
		if (Stolen_items[slot] == STOLEN_ITEM_NONE)
			return slot;
	}
	for (i=0; i<MAX_STOLEN_ITEMS; i++) {
		int slot = (Stolen_item_index+i) % MAX_STOLEN_ITEMS;
		if ((Stolen_items[slot] == POW_SHIELD_BOOST) || (Stolen_items[slot] == POW_ENERGY))
			return slot;
	}

	return -1;
}

static int thief_store_stolen_item(ubyte item)
{
	int slot = thief_find_stolen_item_slot();

	if (slot < 0)
		return 0;

	Stolen_items[slot] = item;
	Stolen_item_index = slot;
	return 1;
}

static void thief_drop_stolen_mine(object *objp, int weapon_id)
{
	int newseg;
	vms_vector randvec, tvec;

	make_random_vector(&randvec);
	vm_vec_add(&tvec, &objp->pos, &randvec);
	newseg = find_point_seg(&tvec, objp->segnum);
	if (newseg == -1) {
		tvec = objp->pos;
		newseg = objp->segnum;
	}
	Laser_create_new(&randvec, &tvec, newseg, objp - Objects, weapon_id, 0);
}

void init_buddy_for_level(void)
{
	input_demo_reset_escort_state_probes();

	Buddy_allowed_to_talk = 0;
	Buddy_objnum = -1;
	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
	Escort_special_goal = -1;
	Escort_goal_index = -1;
	Escort_goal_secret_seg = -1;
	Escort_goal_secret_side = -1;
#ifdef __ANDROID__
	escort_route_clear_goal();
	escort_route_set_target_mode(ESCORT_ROUTE_TARGET_END_OF_LEVEL);
	Escort_route_metadata_rescan_count = 0;
	Escort_route_guidance_full_search_count = 0;
	Escort_route_selector_compare_count = 0;
	Escort_route_selector_mismatch_count = 0;
	Escort_route_selector_shared_index = -1;
	Escort_route_selector_legacy_index = -1;
	Escort_route_selector_mismatch_shared_index = -1;
	Escort_route_selector_mismatch_legacy_index = -1;
	Escort_route_selector_mismatch_shared_goal = ESCORT_GOAL_UNSPECIFIED;
	Escort_route_selector_mismatch_legacy_goal = ESCORT_GOAL_UNSPECIFIED;
	escort_route_note_replan("level_start");
	Escort_route_target_mode_restore_pending = 0;
#endif
	Buddy_messages_suppressed = 0;
#ifdef NETWORK
	Escort_owner_player = -1;
	Escort_owner_generation = 0;
	Escort_network_target_mode = 0;
#endif

	Buddy_objnum = escort_find_companion_object();

	Buddy_sorry_time = -F1_0;

	Looking_for_marker = -1;
	Last_buddy_key = -1;
}

void escort_spawn_at_player(void)
{
	object *buddy_objp;
	int old_buddy_objnum = Buddy_objnum;

	if (Game_mode & GM_MULTI) {
#ifdef NETWORK
		if (!(Game_mode & GM_MULTI_COOP)) {
			HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot in Multiplayer!");
			return;
		}
		if (Escort_owner_player != -1 && Escort_owner_player != Player_num) {
			HUD_init_message_literal(HM_DEFAULT, "Guide-Bot is controlled by another player");
			return;
		}
#else
		HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot in Multiplayer!");
		return;
#endif
	}

	if (!escort_refresh_buddy_objnum()) {
#ifdef NETWORK
		if (Game_mode & GM_MULTI_COOP) {
			HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot present in mine!");
			return;
		}
#endif
		create_buddy_bot();
		Buddy_objnum = escort_find_companion_object();
		if (Buddy_objnum == -1) {
			HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot type available");
			return;
		}
	}

	buddy_objp = &Objects[Buddy_objnum];
	buddy_objp->last_pos = ConsoleObject->pos;
	buddy_objp->pos = ConsoleObject->pos;
	buddy_objp->orient = ConsoleObject->orient;
	obj_relink(Buddy_objnum, ConsoleObject->segnum);
	vm_vec_zero(&buddy_objp->mtype.phys_info.velocity);
	vm_vec_zero(&buddy_objp->mtype.phys_info.thrust);
	vm_vec_zero(&buddy_objp->mtype.phys_info.rotvel);
	vm_vec_zero(&buddy_objp->mtype.phys_info.rotthrust);
	init_ai_object(Buddy_objnum, buddy_objp->ctype.ai_info.behavior, -1);
	Buddy_allowed_to_talk = 1;
	Buddy_last_seen_player = GameTime64;
	Buddy_last_player_path_created = GameTime64;
	Escort_last_path_created = GameTime64;
	Escort_special_goal = -1;
	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
	Escort_goal_index = -1;
	Escort_goal_secret_seg = -1;
	Escort_goal_secret_side = -1;
#ifdef __ANDROID__
	escort_route_set_target_mode(ESCORT_ROUTE_TARGET_END_OF_LEVEL);
	escort_route_clear_goal();
	escort_route_note_replan("guidebot_spawned");
#endif
#ifdef NETWORK
	if (Game_mode & GM_MULTI_COOP) {
		int requested_owner = Player_num;

		if (multi_i_am_master() && !escort_owner_candidate_eligible(requested_owner)) {
			for (requested_owner = 0; requested_owner < N_players; ++requested_owner)
				if (escort_owner_candidate_eligible(requested_owner))
					break;
			if (requested_owner >= N_players)
				requested_owner = -1;
		}
		multi_send_escort_owner(requested_owner);
	}
#endif
	HUD_init_message(HM_DEFAULT, old_buddy_objnum == -1 ? "%s deployed" : "%s released", PlayerCfg.GuidebotName);
}

static int escort_try_warp_position(object *buddy_objp, const vms_vector *candidate, int start_seg, vms_vector *warp_pos, int *warp_segnum)
{
	int candidate_segnum;
	int old_segnum;
	vms_vector old_pos;

	candidate_segnum = find_point_seg(candidate, start_seg);
	if (candidate_segnum < 0)
		return 0;

	old_pos = buddy_objp->pos;
	old_segnum = buddy_objp->segnum;
	buddy_objp->pos = *candidate;
	buddy_objp->segnum = candidate_segnum;
	if (object_intersects_wall(buddy_objp)) {
		buddy_objp->pos = old_pos;
		buddy_objp->segnum = old_segnum;
		return 0;
	}

	buddy_objp->pos = old_pos;
	buddy_objp->segnum = old_segnum;
	*warp_pos = *candidate;
	*warp_segnum = candidate_segnum;
	return 1;
}

static int escort_try_warp_offset(object *buddy_objp, const vms_vector *dir, fix offset, vms_vector *warp_pos, int *warp_segnum)
{
	vms_vector candidate;

	vm_vec_scale_add(&candidate, &ConsoleObject->pos, dir, offset);
	return escort_try_warp_position(buddy_objp, &candidate, ConsoleObject->segnum, warp_pos, warp_segnum);
}

static int escort_find_warp_to_player_position(object *buddy_objp, vms_vector *warp_pos, int *warp_segnum)
{
	fix offset;
	vms_vector candidate;

	offset = ConsoleObject->size + buddy_objp->size + F1_0 / 2;
	if (escort_try_warp_offset(buddy_objp, &ConsoleObject->orient.fvec, -offset, warp_pos, warp_segnum))
		return 1;
	if (escort_try_warp_offset(buddy_objp, &ConsoleObject->orient.rvec, offset, warp_pos, warp_segnum))
		return 1;
	if (escort_try_warp_offset(buddy_objp, &ConsoleObject->orient.rvec, -offset, warp_pos, warp_segnum))
		return 1;
	if (escort_try_warp_offset(buddy_objp, &ConsoleObject->orient.uvec, offset, warp_pos, warp_segnum))
		return 1;
	if (escort_try_warp_offset(buddy_objp, &ConsoleObject->orient.uvec, -offset, warp_pos, warp_segnum))
		return 1;
	if (escort_try_warp_offset(buddy_objp, &ConsoleObject->orient.fvec, offset, warp_pos, warp_segnum))
		return 1;

	compute_segment_center(&candidate, &Segments[ConsoleObject->segnum]);
	if (escort_try_warp_position(buddy_objp, &candidate, ConsoleObject->segnum, warp_pos, warp_segnum))
		return 1;

	candidate = ConsoleObject->pos;
	return escort_try_warp_position(buddy_objp, &candidate, ConsoleObject->segnum, warp_pos, warp_segnum);
}

void escort_warp_to_player(void)
{
	object *buddy_objp;
	vms_vector warp_pos;
	int warp_segnum;
#ifdef NETWORK
	sbyte remote_owner;
#endif

	if (!ConsoleObject)
		return;

	if (Game_mode & GM_MULTI) {
#ifdef NETWORK
		if (!(Game_mode & GM_MULTI_COOP)) {
			HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot in Multiplayer!");
			return;
		}
		if (Escort_owner_player != -1 && Escort_owner_player != Player_num) {
			HUD_init_message_literal(HM_DEFAULT, "Guide-Bot is controlled by another player");
			return;
		}
#else
		HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot in Multiplayer!");
		return;
#endif
	}

	if (!escort_refresh_buddy_objnum()) {
		HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot in mine.");
		return;
	}
	if (!escort_goal_command_allowed())
		return;

	buddy_objp = &Objects[Buddy_objnum];
#ifdef NETWORK
	if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player == -1) {
		multi_send_escort_owner(Player_num);
	}
	if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player != Player_num) {
		HUD_init_message_literal(HM_DEFAULT, "Guide-Bot control requested");
		return;
	}
#endif

	if (!escort_find_warp_to_player_position(buddy_objp, &warp_pos, &warp_segnum)) {
		HUD_init_message(HM_DEFAULT, "No clear space to warp %s", PlayerCfg.GuidebotName);
		return;
	}

#ifdef NETWORK
	remote_owner = buddy_objp->ctype.ai_info.REMOTE_OWNER;
#endif
	buddy_objp->last_pos = warp_pos;
	buddy_objp->pos = warp_pos;
	buddy_objp->orient = ConsoleObject->orient;
	obj_relink(Buddy_objnum, warp_segnum);
	vm_vec_zero(&buddy_objp->mtype.phys_info.velocity);
	vm_vec_zero(&buddy_objp->mtype.phys_info.thrust);
	vm_vec_zero(&buddy_objp->mtype.phys_info.rotvel);
	vm_vec_zero(&buddy_objp->mtype.phys_info.rotthrust);
	init_ai_object(Buddy_objnum, buddy_objp->ctype.ai_info.behavior, -1);
#ifdef NETWORK
	buddy_objp->ctype.ai_info.REMOTE_OWNER = remote_owner;
#endif
	buddy_objp->ctype.ai_info.hide_index = -1;
	buddy_objp->ctype.ai_info.path_length = 0;
	buddy_objp->ctype.ai_info.cur_path_index = 0;
	Ai_local_info[Buddy_objnum].mode = AIM_GOTO_PLAYER;
	Buddy_allowed_to_talk = 1;
	Buddy_last_seen_player = GameTime64;
	Buddy_last_player_path_created = GameTime64 - F1_0 * 2;
	Escort_last_path_created = GameTime64 - F1_0 * 2;
	HUD_init_message(HM_DEFAULT, "%s warped to you", PlayerCfg.GuidebotName);
}

//	-----------------------------------------------------------------------------
//	See if segment from curseg through sidenum is reachable.
//	Return true if it is reachable, else return false.
int segment_is_reachable(int curseg, int sidenum)
{
	int		wall_num, rval;
	segment	*segp = &Segments[curseg];

	if (!IS_CHILD(segp->children[sidenum]))
		return 0;

	wall_num = segp->sides[sidenum].wall_num;

	//	If no wall, then it is reachable
	if (wall_num == -1)
		return 1;

	rval = ai_door_is_openable(NULL, segp, sidenum);

	return rval;

// -- MK, 10/17/95 -- 
// -- MK, 10/17/95 -- 	//	Hmm, a closed wall.  I think this mean not reachable.
// -- MK, 10/17/95 -- 	if (Walls[wall_num].type == WALL_CLOSED)
// -- MK, 10/17/95 -- 		return 0;
// -- MK, 10/17/95 -- 
// -- MK, 10/17/95 -- 	if (Walls[wall_num].type == WALL_DOOR) {
// -- MK, 10/17/95 -- 		if (Walls[wall_num].keys == KEY_NONE) {
// -- MK, 10/17/95 -- 			return 1;		//	@MK, 10/17/95: Be consistent with ai_door_is_openable
// -- MK, 10/17/95 -- // -- 			if (Walls[wall_num].flags & WALL_DOOR_LOCKED)
// -- MK, 10/17/95 -- // -- 				return 0;
// -- MK, 10/17/95 -- // -- 			else
// -- MK, 10/17/95 -- // -- 				return 1;
// -- MK, 10/17/95 -- 		} else if (Walls[wall_num].keys == KEY_BLUE)
// -- MK, 10/17/95 -- 			return (Players[Player_num].flags & PLAYER_FLAGS_BLUE_KEY);
// -- MK, 10/17/95 -- 		else if (Walls[wall_num].keys == KEY_GOLD)
// -- MK, 10/17/95 -- 			return (Players[Player_num].flags & PLAYER_FLAGS_GOLD_KEY);
// -- MK, 10/17/95 -- 		else if (Walls[wall_num].keys == KEY_RED)
// -- MK, 10/17/95 -- 			return (Players[Player_num].flags & PLAYER_FLAGS_RED_KEY);
// -- MK, 10/17/95 -- 		else
// -- MK, 10/17/95 -- 			Int3();	//	Impossible!  Doesn't have no key, but doesn't have any key!
// -- MK, 10/17/95 -- 	} else
// -- MK, 10/17/95 -- 		return 1;
// -- MK, 10/17/95 -- 
// -- MK, 10/17/95 -- 	Int3();	//	Hmm, thought 'if' above had to return!
// -- MK, 10/17/95 -- 	return 0;

}


//	-----------------------------------------------------------------------------
//	Create a breadth-first list of segments reachable from current segment.
//	max_segs is maximum number of segments to search.  Use MAX_SEGMENTS to search all.
//	On exit, *length <= max_segs.
//	Input:
//		start_seg
//	Output:
//		bfs_list:	array of shorts, each reachable segment.  Includes start segment.
//		length:		number of elements in bfs_list
void create_bfs_list(int start_seg, short bfs_list[], int *length, int max_segs)
{
	int	head, tail;
	sbyte   visited[MAX_SEGMENTS];

	for (unsigned s=0; s<sizeof(visited)/sizeof(visited[0]); s++)
		visited[s] = 0;

	head = 0;
	tail = 0;

	bfs_list[head++] = start_seg;
	visited[start_seg] = 1;

	while ((head != tail) && (head < max_segs)) {
		int		i;
		int		curseg;
		segment	*cursegp;

		curseg = bfs_list[tail++];
		cursegp = &Segments[curseg];

		for (i=0; i<MAX_SIDES_PER_SEGMENT; i++) {
			int	connected_seg;

			connected_seg = cursegp->children[i];

			if (IS_CHILD(connected_seg) && (visited[connected_seg] == 0)) {
				if (segment_is_reachable(curseg, i)) {
					bfs_list[head++] = connected_seg;
					if (head >= max_segs)
						break;
					visited[connected_seg] = 1;
					Assert(head < MAX_SEGMENTS);
				}
			}
		}
	}

	*length = head;
	
}

//	-----------------------------------------------------------------------------
//	Return true if ok for buddy to talk, else return false.
//	Buddy is allowed to talk if the segment he is in does not contain a blastable wall that has not been blasted
//	AND he has never yet, since being initialized for level, been allowed to talk.
int ok_for_buddy_to_talk(void)
{
	int		i;
	segment	*segp;

	if (!escort_refresh_buddy_objnum())
		return 0;

	if (Buddy_allowed_to_talk)
		return 1;

	segp = &Segments[Objects[Buddy_objnum].segnum];

	for (i=0; i<MAX_SIDES_PER_SEGMENT; i++) {
		int	wall_num = segp->sides[i].wall_num;

		if (wall_num != -1) {
			if ((Walls[wall_num].type == WALL_BLASTABLE) && !(Walls[wall_num].flags & WALL_BLASTED))
				return 0;
		}

		//	Check one level deeper.
		if (IS_CHILD(segp->children[i])) {
			int		j;
			segment	*csegp = &Segments[segp->children[i]];

			for (j=0; j<MAX_SIDES_PER_SEGMENT; j++) {
				int	wall2 = csegp->sides[j].wall_num;

				if (wall2 != -1) {
					if ((Walls[wall2].type == WALL_BLASTABLE) && !(Walls[wall2].flags & WALL_BLASTED))
						return 0;
				}
			}
		}
	}

	Buddy_allowed_to_talk = 1;

#ifdef NETWORK
	/* android port: only the master simulates passive cage release while the
	 * guidebot is unowned, so initial assignment cannot race across peers. */
	if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player == -1 && multi_i_am_master()) {
		int owner_pnum;
		for (owner_pnum = 0; owner_pnum < N_players; owner_pnum++)
			if (escort_owner_candidate_eligible(owner_pnum)) {
				multi_send_escort_owner(owner_pnum);
				ESCORT_DIAG("initial ownership assigned to player %d", owner_pnum);
				break;
			}
	}
#endif

	return 1;
}

//	--------------------------------------------------------------------------------------------
void detect_escort_goal_accomplished(int index)
{
	int	i,j;
	int	detected = 0;

	if (!Buddy_allowed_to_talk)
		return;

	//	If goal is to go away, how can it be achieved?
	if (Escort_special_goal == ESCORT_GOAL_SCRAM)
		return;

	if (Escort_special_goal == ESCORT_GOAL_SECRET)
		return;

//	See if goal found was a key.  Need to handle default goals differently.
//	Note, no buddy_met_goal sound when blow up reactor or exit.  Not great, but ok
//	since for reactor, noisy, for exit, buddy is disappearing.
#ifdef __ANDROID__
if ((Escort_special_goal == -1) && !Escort_route_goal.active && (Escort_goal_index == index)) {
#else
if ((Escort_special_goal == -1) && (Escort_goal_index == index)) {
#endif
	detected = 1;
	goto dega_ok;
}

if ((Escort_goal_index <= ESCORT_GOAL_RED_KEY) && (index >= 0)) {
	if (Objects[index].type == OBJ_POWERUP)  {
		if (Objects[index].id == POW_KEY_BLUE) {
			if (Escort_goal_index == ESCORT_GOAL_BLUE_KEY) {
				detected = 1;
				goto dega_ok;
			}
		} else if (Objects[index].id == POW_KEY_GOLD) {
			if (Escort_goal_index == ESCORT_GOAL_GOLD_KEY) {
				detected = 1;
				goto dega_ok;
			}
		} else if (Objects[index].id == POW_KEY_RED) {
			if (Escort_goal_index == ESCORT_GOAL_RED_KEY) {
				detected = 1;
				goto dega_ok;
			}
		}
	}
}
	if (Escort_special_goal != -1)
	{
		if (Escort_special_goal == ESCORT_GOAL_ENERGYCEN) {
			if (index == -4)
				detected = 1;
			else {
				for (i=0; i<MAX_SIDES_PER_SEGMENT; i++)
					if (Segments[index].children[i] == Escort_goal_index) {
						detected = 1;
						goto dega_ok;
					} else {
						for (j=0; j<MAX_SIDES_PER_SEGMENT; j++)
							if (Segments[i].children[j] == Escort_goal_index) {
								detected = 1;
								goto dega_ok;
							}
					}
			}
		} else if ((Objects[index].type == OBJ_POWERUP) && (Escort_special_goal == ESCORT_GOAL_POWERUP))
			detected = 1;	//	Any type of powerup picked up will do.
		else if ((Objects[index].type == Objects[Escort_goal_index].type) && (Objects[index].id == Objects[Escort_goal_index].id)) {
			//	Note: This will help a little bit in making the buddy believe a goal is satisfied.  Won't work for a general goal like "find any powerup"
			// because of the insistence of both type and id matching.
			detected = 1;
		}
	}

dega_ok: ;
	if (detected && ok_for_buddy_to_talk()) {
		digi_play_sample_once(SOUND_BUDDY_MET_GOAL, F1_0);
		Escort_goal_index = -1;
		Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
		Escort_special_goal = -1;
		Looking_for_marker = -1;
	}

}

void change_guidebot_name()
{
	newmenu_item m[2];
	char text[GUIDEBOT_NAME_LEN+1]="";
	int item;

	strcpy(text,PlayerCfg.GuidebotName);

	m[0].type=NM_TYPE_INPUT; m[0].text_len = GUIDEBOT_NAME_LEN; m[0].text = text;
	m[1].type=NM_TYPE_MENU; m[1].text = TXT_OK;
	item = newmenu_do( NULL, "Enter Guide-bot name:", 2, m, NULL, NULL );

	if (item != -1) {
		strcpy(PlayerCfg.GuidebotName,text);
		strcpy(PlayerCfg.GuidebotNameReal,text);
		write_player_file();
	}
}

//	-----------------------------------------------------------------------------
void buddy_message(char * format, ... )
{
	if (Buddy_messages_suppressed)
		return;

#ifdef NETWORK
	if ((Game_mode & GM_MULTI) && !(Game_mode & GM_MULTI_COOP))
		return;
	if ((Game_mode & GM_MULTI_COOP) && Escort_owner_player != Player_num)
		return;
#endif

	if (Last_buddy_message_time + F1_0 < GameTime64) {
		if (ok_for_buddy_to_talk()) {
			char	gb_str[16], new_format[128];
			va_list	args;
			int t;

			va_start(args, format );
			vsprintf(new_format, format, args);
			va_end(args);

			gb_str[0] = CC_COLOR;
			gb_str[1] = BM_XRGB(28, 0, 0);
			strcpy(&gb_str[2], PlayerCfg.GuidebotName);
			t = strlen(gb_str);
			gb_str[t] = ':';
			gb_str[t+1] = CC_COLOR;
			gb_str[t+2] = BM_XRGB(0, 31, 0);
			gb_str[t+3] = 0;

			HUD_init_message(HM_DEFAULT, "%s %s", gb_str, new_format);

			Last_buddy_message_time = GameTime64;
		}
	}

}

//	-----------------------------------------------------------------------------
void thief_message(char * format, ... )
{

	char	gb_str[16], new_format[128];
	va_list	args;

	va_start(args, format );
	vsprintf(new_format, format, args);
	va_end(args);

	gb_str[0] = 1;
	gb_str[1] = BM_XRGB(28, 0, 0);
	strcpy(&gb_str[2], "THIEF:");
	gb_str[8] = 1;
	gb_str[9] = BM_XRGB(0, 31, 0);
	gb_str[10] = 0;

	HUD_init_message(HM_DEFAULT, "%s %s", gb_str, new_format);

}

//	-----------------------------------------------------------------------------
//	Return true if marker #id has been placed.
int marker_exists_in_mine(int id)
{
	int	i;

	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type == OBJ_MARKER)
			if (Objects[i].id == id)
				return 1;

	return 0;
}

typedef struct escort_secret_goal_info {
	int display_index;
	int seg;
	int side;
	int bfs_rank;
} escort_secret_goal_info;

static void escort_clear_secret_goal(void);
static int escort_find_nearest_unfound_secret_entrance(int start_seg,
                                                       int skip_display_index,
                                                       escort_secret_goal_info *goal);
static void escort_report_secret_goal_failure(int goal_index);

//	-----------------------------------------------------------------------------
void set_escort_special_goal(int special_key)
{
	int marker_key;

	if (!escort_goal_command_allowed())
		return;
	escort_clear_secret_goal();
#ifdef __ANDROID__
	escort_route_set_target_mode(ESCORT_ROUTE_TARGET_END_OF_LEVEL);
	escort_route_clear_goal();
	escort_route_note_replan("goal_command");
	escort_route_sync_target_mode();
#endif

	special_key = special_key & (~KEY_SHIFTED);

	marker_key = special_key;
	
	if (Last_buddy_key == special_key)
	{
		if ((Looking_for_marker == -1) && (special_key != KEY_0)) {
			if (marker_exists_in_mine(marker_key - KEY_1))
				Looking_for_marker = marker_key - KEY_1;
			else {
				Last_buddy_message_time = 0;	//	Force this message to get through.
				buddy_message("Marker %i not placed.", marker_key - KEY_1 + 1);
				Looking_for_marker = -1;
			}
		} else {
			Looking_for_marker = -1;
		}
	}

	Last_buddy_key = special_key;

	if (special_key == KEY_0)
		Looking_for_marker = -1;
		
	if ( Looking_for_marker != -1 ) {
		Escort_special_goal = ESCORT_GOAL_MARKER1 + marker_key - KEY_1;
	} else {
		switch (special_key) {
			case KEY_1:	Escort_special_goal = ESCORT_GOAL_ENERGY;			break;
			case KEY_2:	Escort_special_goal = ESCORT_GOAL_ENERGYCEN;		break;
			case KEY_3:	Escort_special_goal = ESCORT_GOAL_SHIELD;			break;
			case KEY_4:	Escort_special_goal = ESCORT_GOAL_POWERUP;		break;
			case KEY_5:	Escort_special_goal = ESCORT_GOAL_ROBOT;			break;
			case KEY_6:	Escort_special_goal = ESCORT_GOAL_HOSTAGE;		break;
			case KEY_7:	Escort_special_goal = ESCORT_GOAL_SCRAM;			break;
			case KEY_8:	Escort_special_goal = ESCORT_GOAL_PLAYER_SPEW;	break;
			case KEY_9:	Escort_special_goal = ESCORT_GOAL_EXIT;			break;
			case KEY_0:
				Escort_special_goal = -1;
				Escort_goal_index = -1;
				Escort_last_path_created = 0;
				break;
			default:
				Int3();		//	Oops, called with illegal key value.
		}
	}

	Last_buddy_message_time = GameTime64 - 2*F1_0;	//	Allow next message to come through.

	say_escort_goal(Escort_special_goal);
	// -- Escort_goal_object = escort_set_goal_object();

	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
}

void escort_resume_default_goal(void)
{
	Looking_for_marker = -1;
	Last_buddy_key = -1;
	set_escort_special_goal(KEY_0);
	Last_buddy_key = -1;
#ifdef __ANDROID__
	escort_start_default_goal_now();
#endif
}

void escort_find_secret_goal(void)
{
	escort_secret_goal_info goal;
	int skip_display_index = -1;
	int goal_index;

	if (!escort_goal_command_allowed())
		return;

#ifdef __ANDROID__
	escort_route_set_target_mode(ESCORT_ROUTE_TARGET_END_OF_LEVEL);
	escort_route_clear_goal();
	escort_route_note_replan("secret_command");
	escort_route_sync_target_mode();
#endif
	if ((Escort_special_goal == ESCORT_GOAL_SECRET || Escort_goal_object == ESCORT_GOAL_SECRET) &&
	    Escort_goal_index > 0)
		skip_display_index = Escort_goal_index;
	goal_index = escort_find_nearest_unfound_secret_entrance(Objects[Buddy_objnum].segnum, skip_display_index, &goal);
	if (goal_index == -2 && skip_display_index > 0)
		goal_index = escort_find_nearest_unfound_secret_entrance(Objects[Buddy_objnum].segnum, -1, &goal);
	if (goal_index < 0) {
		escort_report_secret_goal_failure(goal_index);
		Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
		Escort_special_goal = -1;
		Escort_goal_index = goal_index;
		escort_clear_secret_goal();
		return;
	}

	Looking_for_marker = -1;
	Last_buddy_key = -1;
	Escort_special_goal = ESCORT_GOAL_SECRET;
	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
	Escort_goal_index = goal.display_index;
	Escort_goal_secret_seg = goal.seg;
	Escort_goal_secret_side = goal.side;
	Last_buddy_message_time = GameTime64 - 2*F1_0;
	say_escort_goal(ESCORT_GOAL_SECRET);
}

void escort_find_unexplored_goal(void)
{
#ifdef __ANDROID__
	if (!escort_goal_command_allowed())
		return;

	Looking_for_marker = -1;
	Last_buddy_key = -1;
	Escort_special_goal = -1;
	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
	Escort_goal_index = -1;
	escort_clear_secret_goal();
	escort_route_set_target_mode(ESCORT_ROUTE_TARGET_UNEXPLORED);
	escort_route_clear_goal();
	escort_route_note_replan("unexplored_command");
	Last_buddy_message_time = GameTime64 - 2*F1_0;
	if (escort_start_default_goal_now()) {
		escort_route_sync_target_mode();
		return;
	}

	Last_buddy_message_time = 0;
	buddy_message("No reachable unexplored area found.");
	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
	Escort_goal_index = -1;
	escort_route_set_target_mode(ESCORT_ROUTE_TARGET_END_OF_LEVEL);
	escort_route_clear_goal();
	escort_route_sync_target_mode();
#endif
}

void input_demo_apply_recorded_guidebot_goal(int special_key, int from_menu)
{
	if (from_menu) {
		Looking_for_marker = -1;
		Last_buddy_key = -1;
		set_escort_special_goal(special_key);
		Last_buddy_key = -1;
		return;
	}
	set_escort_special_goal(special_key);
}

void input_demo_apply_recorded_guidebot_find_secret(void)
{
	escort_find_secret_goal();
}

void input_demo_apply_recorded_guidebot_find_unexplored(void)
{
	escort_find_unexplored_goal();
}

//	-----------------------------------------------------------------------------
//	Return id of boss.
int get_boss_id(void)
{
	int	i;

	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type == OBJ_ROBOT)
			if (Robot_info[Objects[i].id].boss_flag)
				return Objects[i].id;

	return -1;
}

//	-----------------------------------------------------------------------------
//	Return object index if object of objtype, objid exists in mine, else return -1
//	"special" is used to find objects spewed by player which is hacked into flags field of powerup.
int exists_in_mine_2(int segnum, int objtype, int objid, int special)
{
	if (Segments[segnum].objects != -1) {
		int		objnum = Segments[segnum].objects;

		while (objnum != -1) {
			object	*curobjp = &Objects[objnum];

			if (special == ESCORT_GOAL_PLAYER_SPEW) {
				if (curobjp->flags & OF_PLAYER_DROPPED)
					return objnum;
			}

			if (curobjp->type == objtype) {
				//	Don't find escort robots if looking for robot!
				if ((curobjp->type == OBJ_ROBOT) && (Robot_info[curobjp->id].companion))
					;
				else if (objid == -1) {
					if ((objtype == OBJ_POWERUP) && (curobjp->id != POW_KEY_BLUE) && (curobjp->id != POW_KEY_GOLD) && (curobjp->id != POW_KEY_RED))
						return objnum;
					else
						return objnum;
				} else if (curobjp->id == objid)
					return objnum;
			}

			if (objtype == OBJ_POWERUP)
				if (curobjp->contains_count)
					if (curobjp->contains_type == OBJ_POWERUP)
						if (curobjp->contains_id == objid)
							return objnum;

			objnum = curobjp->next;
		}
	}

	return -1;
}

//	-----------------------------------------------------------------------------
//	Return nearest object of interest.
//	If special == ESCORT_GOAL_PLAYER_SPEW, then looking for any object spewed by player.
//	-1 means object does not exist in mine.
//	-2 means object does exist in mine, but buddy-bot can't reach it (eg, behind triggered wall)
int exists_in_mine(int start_seg, int objtype, int objid, int special)
{
	int	segindex, segnum;
	short	bfs_list[MAX_SEGMENTS];
	int	length;

	create_bfs_list(start_seg, bfs_list, &length, MAX_SEGMENTS);

	if (objtype == FUELCEN_CHECK) {
		for (segindex=0; segindex<length; segindex++) {
			segnum = bfs_list[segindex];
			if (Segment2s[segnum].special == SEGMENT_IS_FUELCEN)
				return segnum;
		}
	} else {
		for (segindex=0; segindex<length; segindex++) {
			int	objnum;

			segnum = bfs_list[segindex];

			objnum = exists_in_mine_2(segnum, objtype, objid, special);
			if (objnum != -1)
				return objnum;

		}
	}

	//	Couldn't find what we're looking for by looking at connectivity.
	//	See if it's in the mine.  It could be hidden behind a trigger or switch
	//	which the buddybot doesn't understand.
	if (objtype == FUELCEN_CHECK) {
		for (segnum=0; segnum<=Highest_segment_index; segnum++)
			if (Segment2s[segnum].special == SEGMENT_IS_FUELCEN)
				return -2;
	} else {
		for (segnum=0; segnum<=Highest_segment_index; segnum++) {
			int	objnum;

			objnum = exists_in_mine_2(segnum, objtype, objid, special);
			if (objnum != -1)
				return -2;
		}
	}

	return -1;
}

#ifdef __ANDROID__
static int escort_find_unreachable_goal_segment(int escort_goal_object)
{
	int i;

	switch (escort_goal_object) {
		case ESCORT_GOAL_BOSS:
			for (i=0; i<=Highest_object_index; i++)
				if (Objects[i].type == OBJ_ROBOT &&
				    Robot_info[Objects[i].id].boss_flag &&
				    !(Objects[i].flags & OF_SHOULD_BE_DEAD) &&
				    escort_valid_segment(Objects[i].segnum))
					return Objects[i].segnum;
			break;
		case ESCORT_GOAL_CONTROLCEN:
			for (i=0; i<=Highest_object_index; i++)
				if (Objects[i].type == OBJ_CNTRLCEN &&
				    !(Objects[i].flags & OF_SHOULD_BE_DEAD) &&
				    escort_valid_segment(Objects[i].segnum))
					return Objects[i].segnum;
			for (i=0; i<=Highest_segment_index; i++)
				if (escort_valid_segment(i) && Segment2s[i].special == SEGMENT_IS_CONTROLCEN)
					return i;
			break;
		case ESCORT_GOAL_EXIT:
		case ESCORT_GOAL_EXIT2:
			return find_exit_segment();
	}
	return -1;
}
#endif

static void escort_clear_secret_goal(void)
{
	Escort_goal_secret_seg = -1;
	Escort_goal_secret_side = -1;
}

int escort_get_secret_goal_display_index(void)
{
	return (Escort_goal_object == ESCORT_GOAL_SECRET) ? Escort_goal_index : -1;
}

int escort_get_secret_goal_seg(void)
{
	return (Escort_goal_object == ESCORT_GOAL_SECRET) ? Escort_goal_secret_seg : -1;
}

int escort_get_secret_goal_side(void)
{
	return (Escort_goal_object == ESCORT_GOAL_SECRET) ? Escort_goal_secret_side : -1;
}

static int escort_secret_goal_is_better(const escort_secret_goal_info *candidate,
                                        const escort_secret_goal_info *best)
{
	if (best->display_index < 0)
		return 1;
	if (candidate->bfs_rank != best->bfs_rank)
		return candidate->bfs_rank < best->bfs_rank;
	if (candidate->display_index != best->display_index)
		return candidate->display_index < best->display_index;
	if (candidate->seg != best->seg)
		return candidate->seg < best->seg;
	return candidate->side < best->side;
}

static int escort_find_nearest_unfound_secret_entrance(int start_seg,
                                                       int skip_display_index,
                                                       escort_secret_goal_info *goal)
{
	const secret_area_state *state = secret_area_get_state();
	escort_secret_goal_info best;
	short bfs_list[MAX_SEGMENTS];
	static int bfs_rank[MAX_SEGMENTS];
	int length;
	int total;
	int have_unfound = 0;
	int i;

	if (goal)
		memset(goal, 0, sizeof(*goal));
	if (!state || !state->enabled)
		return -1;
	total = secret_area_total(state);
	if (total <= 0)
		return -1;
	if (secret_area_found_count(state) >= total)
		return -4;
	if ((start_seg < 0) || (start_seg > Highest_segment_index))
		return -2;

	for (i=0; i<MAX_SEGMENTS; i++)
		bfs_rank[i] = -1;
	create_bfs_list(start_seg, bfs_list, &length, MAX_SEGMENTS);
	for (i=0; i<length; i++)
		if ((bfs_list[i] >= 0) && (bfs_list[i] < MAX_SEGMENTS))
			bfs_rank[bfs_list[i]] = i;

	best.display_index = -1;
	best.seg = -1;
	best.side = -1;
	best.bfs_rank = MAX_SEGMENTS;
	for (i=0; i<total; i++) {
		const secret_area_entry *secret = &state->secrets[i];
		int e;

		if (state->found[i])
			continue;
		have_unfound = 1;
		if (secret->display_index == skip_display_index)
			continue;
		for (e=0; e<secret->entrance_count; e++) {
			const secret_area_entrance *entrance = &secret->entrances[e];
			escort_secret_goal_info candidate;

			if ((entrance->seg < 0) || (entrance->seg > Highest_segment_index) ||
			    (entrance->seg >= MAX_SEGMENTS) || (bfs_rank[entrance->seg] < 0))
				continue;
			candidate.display_index = secret->display_index;
			candidate.seg = entrance->seg;
			candidate.side = entrance->side;
			candidate.bfs_rank = bfs_rank[entrance->seg];
			if (escort_secret_goal_is_better(&candidate, &best))
				best = candidate;
		}
	}

	if (best.display_index < 0)
		return have_unfound ? -2 : -4;
	if (goal)
		*goal = best;
	return best.display_index;
}

static void escort_report_secret_goal_failure(int goal_index)
{
	Last_buddy_message_time = 0;
	if (goal_index == -4)
		buddy_message("All found.");
	else if (goal_index == -2)
		buddy_message("Can't reach any secrets.");
	else
		buddy_message("No secrets in mine.");
}

static int escort_goal_command_allowed(void)
{
	Buddy_messages_suppressed = 0;

	if (!Buddy_allowed_to_talk) {
		ok_for_buddy_to_talk();
		if (!Buddy_allowed_to_talk) {
			int	i;

			for (i=0; i<=Highest_object_index; i++)
				if ((Objects[i].type == OBJ_ROBOT) && Robot_info[Objects[i].id].companion) {
					HUD_init_message(HM_DEFAULT, "%s has not been released.",PlayerCfg.GuidebotName);
					break;
				}
			if (i == Highest_object_index+1)
				HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot in mine.");

			return 0;
		}
	}

	return 1;
}

static int escort_owned_key_flags(void)
{
	return Players[escort_key_owner_player()].flags & ESCORT_KEY_FLAGS;
}

static int escort_key_owner_player(void)
{
#ifdef NETWORK
	if ((Game_mode & GM_MULTI_COOP) &&
	    Escort_owner_player >= 0 && Escort_owner_player < MAX_PLAYERS &&
	    Players[Escort_owner_player].connected == CONNECT_PLAYING)
		return Escort_owner_player;
#endif
	return Player_num;
}

static int escort_key_exists(int powerup_id)
{
	return exists_in_mine(ConsoleObject->segnum, OBJ_POWERUP, powerup_id, -1) != -1;
}

static int escort_reactor_exists(void)
{
	int i;

	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type == OBJ_CNTRLCEN && !(Objects[i].flags & OF_SHOULD_BE_DEAD))
			return 1;

	for (i=0; i<=Highest_segment_index; i++)
		if (Segment2s[i].special == SEGMENT_IS_CONTROLCEN)
			return 1;

	return 0;
}

static int escort_boss_exists(void)
{
	int i;

	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type == OBJ_ROBOT &&
		    Robot_info[Objects[i].id].boss_flag &&
		    !(Objects[i].flags & OF_SHOULD_BE_DEAD))
			return 1;

	return 0;
}

static int side_has_exit_trigger(int seg, int side)
{
	int wall_num;
	int trigger_num;

	if (seg < 0 || seg > Highest_segment_index || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return 0;
	wall_num = Segments[seg].sides[side].wall_num;
	if (wall_num < 0 || wall_num >= Num_walls)
		return 0;
	trigger_num = Walls[wall_num].trigger;
	if (trigger_num < 0 || trigger_num >= Num_triggers)
		return 0;
	return Triggers[trigger_num].type == TT_EXIT;
}

//	-----------------------------------------------------------------------------
//	Return true if it happened, else return false.
int find_exit_segment(void)
{
	int i, j;
	int external_segment = -1;
	int trigger_segment = -1;

	/* Classic D2 routes the Guide-Bot to the external flythrough endpoint. */
	for (i = 0; i <= Highest_segment_index && external_segment == -1; ++i)
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; ++j)
			if (Segments[i].children[j] == -2) {
				external_segment = i;
				break;
			}

	/* Trigger-only community levels have no external endpoint to select. */
	for (i = 0; i <= Highest_segment_index && trigger_segment == -1; ++i)
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; ++j)
			if (side_has_exit_trigger(i, j)) {
				trigger_segment = i;
				break;
			}

	return escort_exit_segment_preferred(external_segment, trigger_segment);
}

#define	BUDDY_MARKER_TEXT_LEN	25

//	-----------------------------------------------------------------------------
void say_escort_goal(int goal_num)
{
	if (Player_is_dead)
		return;

	switch (goal_num) {
		case ESCORT_GOAL_BLUE_KEY:		buddy_message("Finding BLUE KEY");			break;
		case ESCORT_GOAL_GOLD_KEY:		buddy_message("Finding YELLOW KEY");		break;
		case ESCORT_GOAL_RED_KEY:		buddy_message("Finding RED KEY");			break;
		case ESCORT_GOAL_CONTROLCEN:	buddy_message("Finding REACTOR");			break;
		case ESCORT_GOAL_EXIT:			buddy_message("Finding EXIT");				break;
		case ESCORT_GOAL_ENERGY:		buddy_message("Finding ENERGY");				break;
		case ESCORT_GOAL_ENERGYCEN:	buddy_message("Finding ENERGY CENTER");	break;
		case ESCORT_GOAL_SHIELD:		buddy_message("Finding a SHIELD");			break;
		case ESCORT_GOAL_POWERUP:		buddy_message("Finding a POWERUP");			break;
		case ESCORT_GOAL_ROBOT:			buddy_message("Finding a ROBOT");			break;
		case ESCORT_GOAL_HOSTAGE:		buddy_message("Finding a HOSTAGE");			break;
		case ESCORT_GOAL_SCRAM:			buddy_message("Staying away...");			break;
		case ESCORT_GOAL_BOSS:			buddy_message("Finding BOSS robot");		break;
		case ESCORT_GOAL_PLAYER_SPEW:	buddy_message("Finding your powerups");	break;
		case ESCORT_GOAL_SECRET:
			if (Escort_goal_index > 0)
				buddy_message("Finding secret %i", Escort_goal_index);
			else
				buddy_message("Finding secret");
			break;
		case ESCORT_GOAL_MARKER1:
		case ESCORT_GOAL_MARKER2:
		case ESCORT_GOAL_MARKER3:
		case ESCORT_GOAL_MARKER4:
		case ESCORT_GOAL_MARKER5:
		case ESCORT_GOAL_MARKER6:
		case ESCORT_GOAL_MARKER7:
		case ESCORT_GOAL_MARKER8:
		case ESCORT_GOAL_MARKER9:
			{ char marker_text[BUDDY_MARKER_TEXT_LEN];
			strncpy(marker_text, MarkerMessage[goal_num-ESCORT_GOAL_MARKER1], BUDDY_MARKER_TEXT_LEN-1);
			marker_text[BUDDY_MARKER_TEXT_LEN-1] = 0;
			buddy_message("Finding marker %i: '%s'", goal_num-ESCORT_GOAL_MARKER1+1, marker_text);
			break;
			}
	}
}

//	-----------------------------------------------------------------------------
void escort_create_path_to_goal(object *objp)
{
	int	goal_seg = -1;
	int			objnum = objp-Objects;
	ai_static	*aip = &objp->ctype.ai_info;
	ai_local		*ailp = &Ai_local_info[objnum];
	int used_nearest_point = 0;
#ifdef __ANDROID__
	int using_route_goal = 0;
#endif

	input_demo_log_escort_goal_probe("entry", objp, ailp, aip, -1, -1);

	if (Escort_special_goal != -1)
		Escort_goal_object = Escort_special_goal;

	Escort_kill_object = -1;

	if (Looking_for_marker != -1) {

		Escort_goal_index = exists_in_mine(objp->segnum, OBJ_MARKER, Escort_goal_object-ESCORT_GOAL_MARKER1, -1);
		if (Escort_goal_index > -1)
			goal_seg = Objects[Escort_goal_index].segnum;
	} else {
#ifdef __ANDROID__
		if (Escort_route_goal.active) {
			escort_route_refresh_guidance_target(objp);
			goal_seg = Escort_route_goal.target_seg;
			Escort_goal_index = goal_seg;
			using_route_goal = 1;
		} else
#endif
		{
			switch (Escort_goal_object) {
				case ESCORT_GOAL_BLUE_KEY:
					Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_KEY_BLUE, -1);
					if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
					break;
				case ESCORT_GOAL_GOLD_KEY:
					Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_KEY_GOLD, -1);
					if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
					break;
				case ESCORT_GOAL_RED_KEY:
					Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_KEY_RED, -1);
					if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
					break;
				case ESCORT_GOAL_CONTROLCEN:
					Escort_goal_index = exists_in_mine(objp->segnum, OBJ_CNTRLCEN, -1, -1);
					if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
					break;
				case ESCORT_GOAL_EXIT:
				case ESCORT_GOAL_EXIT2:
					goal_seg = find_exit_segment();
					Escort_goal_index = goal_seg;
					break;
				case ESCORT_GOAL_ENERGY:
					Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_ENERGY, -1);
					if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
					break;
				case ESCORT_GOAL_ENERGYCEN:
					goal_seg = exists_in_mine(objp->segnum, FUELCEN_CHECK, -1, -1);
					Escort_goal_index = goal_seg;
					break;
				case ESCORT_GOAL_SHIELD:
					Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, POW_SHIELD_BOOST, -1);
					if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
					break;
				case ESCORT_GOAL_POWERUP:
					Escort_goal_index = exists_in_mine(objp->segnum, OBJ_POWERUP, -1, -1);
					if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
					break;
				case ESCORT_GOAL_ROBOT:
					Escort_goal_index = exists_in_mine(objp->segnum, OBJ_ROBOT, -1, -1);
					if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
					break;
				case ESCORT_GOAL_HOSTAGE:
					Escort_goal_index = exists_in_mine(objp->segnum, OBJ_HOSTAGE, -1, -1);
					if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
					break;
				case ESCORT_GOAL_PLAYER_SPEW:
					Escort_goal_index = exists_in_mine(objp->segnum, -1, -1, ESCORT_GOAL_PLAYER_SPEW);
					if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
					break;
				case ESCORT_GOAL_SECRET: {
					escort_secret_goal_info secret_goal;

					Escort_goal_index = escort_find_nearest_unfound_secret_entrance(objp->segnum, -1, &secret_goal);
					if (Escort_goal_index > -1) {
						goal_seg = secret_goal.seg;
						Escort_goal_secret_seg = secret_goal.seg;
						Escort_goal_secret_side = secret_goal.side;
					} else {
						escort_clear_secret_goal();
					}
					break;
				}
				case ESCORT_GOAL_SCRAM:
					goal_seg = -3;		//	Kinda a hack.
					Escort_goal_index = goal_seg;
					break;
				case ESCORT_GOAL_BOSS: {
					int	boss_id;

					boss_id = get_boss_id();
					Assert(boss_id != -1);
					Escort_goal_index = exists_in_mine(objp->segnum, OBJ_ROBOT, boss_id, -1);
					if (Escort_goal_index > -1) goal_seg = Objects[Escort_goal_index].segnum;
					break;
				}
				default:
					Int3();	//	Oops, Illegal value in Escort_goal_object.
					goal_seg = 0;
					break;
			}
		}
	}

	input_demo_log_escort_goal_probe("resolved", objp, ailp, aip, goal_seg,
		Escort_goal_index);

	if ((Escort_goal_index < 0) && (Escort_goal_index != -3)) {	//	I apologize for this statement -- MK, 09/22/95
#ifdef __ANDROID__
		if (Escort_goal_index == -2) {
			goal_seg = escort_find_unreachable_goal_segment(Escort_goal_object);
			if (escort_valid_segment(goal_seg) &&
			    escort_try_nearest_point_fallback(objp, goal_seg, using_route_goal)) {
				used_nearest_point = 1;
				goto escort_goal_path_ready;
			}
		}
#endif
		if (Escort_goal_object == ESCORT_GOAL_SECRET) {
			escort_report_secret_goal_failure(Escort_goal_index);
			Looking_for_marker = -1;
		} else if (Escort_goal_index == -1) {
			Last_buddy_message_time = 0;	//	Force this message to get through.
			buddy_message("No %s in mine.", Escort_goal_text[Escort_goal_object-1]);
			Looking_for_marker = -1;
		} else if (Escort_goal_index == -2) {
			Last_buddy_message_time = 0;	//	Force this message to get through.
			buddy_message("Can't reach %s.", Escort_goal_text[Escort_goal_object-1]);
			Looking_for_marker = -1;
		} else
			Int3();

		Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
		Escort_special_goal = -1;
		escort_clear_secret_goal();
	} else {
		if (goal_seg == -3) {
			// SIM RNG: this changes the live scram path the guidebot follows
			create_n_segment_path(objp, 16 + d_rand() * 16, -1);
			aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
			input_demo_log_escort_path_state("escort_create_path_to_goal scram", objp);
		} else {
#ifdef __ANDROID__
			if (using_route_goal)
				create_path_to_segment_metadata_route(objp, goal_seg, Max_escort_length, 1);
			else
#endif
				create_path_to_segment(objp, goal_seg, Max_escort_length, 1);	//	MK!: Last parm (safety_flag) used to be 1!!
			if (aip->path_length > 3)
				aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
			input_demo_log_escort_path_state("escort_create_path_to_goal to_segment", objp);
			if ((aip->path_length > 0) && (Point_segs[aip->hide_index + aip->path_length - 1].segnum != goal_seg)) {
#ifdef __ANDROID__
				if (escort_try_nearest_point_fallback(objp, goal_seg, using_route_goal)) {
					used_nearest_point = 1;
				} else
#endif
				{
					fix	dist_to_player;
					Last_buddy_message_time = 0;	//	Force this message to get through.
#ifdef __ANDROID__
					if (using_route_goal)
						buddy_message("Can't reach next: %s.", escort_route_goal_label());
					else
#endif
					if (Escort_goal_object == ESCORT_GOAL_SECRET)
						buddy_message("Can't reach any secrets.");
					else
						buddy_message("Can't reach %s.", Escort_goal_text[Escort_goal_object-1]);
					Looking_for_marker = -1;
					Escort_goal_object = ESCORT_GOAL_SCRAM;
					escort_clear_secret_goal();
					dist_to_player = find_connected_distance(&objp->pos, objp->segnum, &Believed_player_pos, Believed_player_seg, 100, WID_FLY_FLAG);
					if (dist_to_player > MIN_ESCORT_DISTANCE)
						create_path_to_player(objp, Max_escort_length, 1);	//	MK!: Last parm used to be 1!
					else {
						// SIM RNG: this changes the live fallback scram path the guidebot follows
						create_n_segment_path(objp, 8 + d_rand() * 8, -1);
						aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
						input_demo_log_escort_path_state("escort_create_path_to_goal fallback_scram", objp);
					}
				}
			}
#ifdef __ANDROID__
			else if ((aip->path_length == 0) && escort_try_nearest_point_fallback(objp, goal_seg, using_route_goal)) {
				used_nearest_point = 1;
			}
#endif
		}

escort_goal_path_ready:
#ifdef __ANDROID__
		if (using_route_goal)
			escort_route_note_path_endpoint(objp);
#endif
		ailp->mode = AIM_GOTO_OBJECT;

		if (!used_nearest_point) {
#ifdef __ANDROID__
			if (using_route_goal)
				buddy_message("Finding NEXT: %s", escort_route_goal_instruction());
			else
#endif
			say_escort_goal(Escort_goal_object);
		}
	}

}

//	-----------------------------------------------------------------------------
//	Escort robot chooses goal object based on owned keys, location.
//	Returns goal object.
int escort_set_goal_object(void)
{
	int key_flags;
#ifdef __ANDROID__
	int route_goal;
#endif

	if (Escort_special_goal != -1) {
#ifdef __ANDROID__
		escort_route_clear_goal();
#endif
		return ESCORT_GOAL_UNSPECIFIED;
	}

	key_flags = escort_owned_key_flags();
#ifdef __ANDROID__
	escort_route_refresh_metadata();
	route_goal = escort_route_next_goal(key_flags);
	if (route_goal != ESCORT_GOAL_UNSPECIFIED)
		return route_goal;
	if (Escort_route_target_mode == ESCORT_ROUTE_TARGET_UNEXPLORED)
		return ESCORT_GOAL_UNSPECIFIED;
#endif
	if ((key_flags & PLAYER_FLAGS_RED_KEY) == 0) {
		if ((key_flags & (PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_GOLD_KEY)) == 0) {
			if (escort_key_exists(POW_KEY_BLUE))
				return ESCORT_GOAL_BLUE_KEY;
			if (escort_key_exists(POW_KEY_GOLD))
				return ESCORT_GOAL_GOLD_KEY;
		} else if ((key_flags & PLAYER_FLAGS_GOLD_KEY) == 0) {
			if (escort_key_exists(POW_KEY_GOLD))
				return ESCORT_GOAL_GOLD_KEY;
		}
		if (escort_key_exists(POW_KEY_RED))
			return ESCORT_GOAL_RED_KEY;
	}

	if (Control_center_destroyed == 0) {
		if (Num_boss_teleport_segs)
			return ESCORT_GOAL_BOSS;
		else if (escort_reactor_exists())
			return ESCORT_GOAL_CONTROLCEN;
	}

	return (Control_center_destroyed || find_exit_segment() != -1) ? ESCORT_GOAL_EXIT : ESCORT_GOAL_CONTROLCEN;
}

#ifdef __ANDROID__
static int escort_start_default_goal_now(void)
{
	object *objp;
	ai_static *aip;
	ai_local *ailp;

	if (!escort_goal_command_allowed() || !escort_is_companion_object(Buddy_objnum))
		return 0;
	objp = &Objects[Buddy_objnum];
	aip = &objp->ctype.ai_info;
	ailp = &Ai_local_info[Buddy_objnum];
	Escort_goal_object = escort_set_goal_object();
	if (Escort_goal_object == ESCORT_GOAL_UNSPECIFIED)
		return 0;
	ailp->mode = AIM_GOTO_OBJECT;
	escort_create_path_to_goal(objp);
	if (aip->path_length > 3)
		aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
	Escort_last_path_created = GameTime64;
	Buddy_last_player_path_created = GameTime64;
	return 1;
}
#endif

#define	MAX_ESCORT_TIME_AWAY		(F1_0*4)

fix64	Buddy_last_seen_player = 0, Buddy_last_player_path_created;

#ifdef __ANDROID__
static int escort_route_goal_has_pending_path_for(const ai_local *ailp, const ai_static *aip)
{
	int next_path_index;

	if (!Escort_route_goal.active || !ailp || !aip || ailp->mode != AIM_GOTO_OBJECT || aip->path_length <= 1)
		return 0;
	next_path_index = aip->cur_path_index + aip->PATH_DIR;
	return next_path_index >= 0 && next_path_index < aip->path_length;
}

int escort_get_route_goal_path_pending(void)
{
	if (!escort_is_companion_object(Buddy_objnum))
		return 0;
	return escort_route_goal_has_pending_path_for(
	    &Ai_local_info[Buddy_objnum], &Objects[Buddy_objnum].ctype.ai_info);
}
#endif

//	-----------------------------------------------------------------------------
int time_to_visit_player(object *objp, ai_local *ailp, ai_static *aip)
{
	//	Note: This one has highest priority because, even if already going towards player,
	//	might be necessary to create a new path, as player can move.
	if (GameTime64 - Buddy_last_seen_player > MAX_ESCORT_TIME_AWAY) {
		if (GameTime64 - Buddy_last_player_path_created > F1_0) {
#ifdef __ANDROID__
			if (!escort_route_goal_has_pending_path_for(ailp, aip))
				return 1;
#else
			return 1;
#endif
		}
	}

	if (ailp->mode == AIM_GOTO_PLAYER)
		return 0;

	if (objp->segnum == ConsoleObject->segnum)
		return 0;

	if (aip->cur_path_index < aip->path_length/2)
		return 0;
	
	return 1;
}

int	Buddy_objnum;
fix64	Last_come_back_message_time = 0;

fix64	Buddy_last_missile_time;
extern fix64	Re_init_thief_time;

void escort_get_input_demo_checkpoint_state(input_demo_checkpoint_escort_state *escort_state)
{
	if (!escort_state)
		return;

	input_demo_checkpoint_escort_state_clear(escort_state);
	escort_state->valid = 1;
	escort_state->buddy_allowed_to_talk = Buddy_allowed_to_talk;
	escort_state->buddy_last_seen_player = Buddy_last_seen_player;
	escort_state->buddy_last_player_path_created = Buddy_last_player_path_created;
	escort_state->escort_kill_object = Escort_kill_object;
	escort_state->escort_last_path_created = Escort_last_path_created;
	escort_state->escort_goal_object = Escort_goal_object;
	escort_state->escort_special_goal = Escort_special_goal;
	escort_state->escort_goal_index = Escort_goal_index;
	escort_state->buddy_messages_suppressed = Buddy_messages_suppressed;
	escort_state->buddy_sorry_time = Buddy_sorry_time;
	escort_state->looking_for_marker = Looking_for_marker;
	escort_state->last_buddy_key = Last_buddy_key;
	escort_state->last_buddy_message_time = Last_buddy_message_time;
	escort_state->last_come_back_message_time = Last_come_back_message_time;
	escort_state->buddy_last_missile_time = Buddy_last_missile_time;
#ifdef NETWORK
	escort_state->escort_owner_player = Escort_owner_player;
#endif
}

void escort_get_input_demo_checkpoint_thief_state(input_demo_checkpoint_thief_state *thief_state)
{
	if (!thief_state)
		return;

	input_demo_checkpoint_thief_state_clear(thief_state);
	thief_state->valid = 1;
	thief_state->stolen_item_index = Stolen_item_index;
	thief_state->re_init_thief_time = Re_init_thief_time;
	thief_state->last_thief_hit_time = Last_thief_hit_time;
}

#ifdef NETWORK
static void escort_validate_owner_after_restore(void)
{
	if (!(Game_mode & GM_MULTI_COOP))
		return;
	if ((Escort_owner_player < -1) ||
	    ((Escort_owner_player >= 0) && !escort_owner_candidate_eligible(Escort_owner_player)))
		Escort_owner_player = -1;
}

static void escort_restore_companion_robot_control(void)
{
	if (!(Game_mode & GM_MULTI_COOP))
		return;
	escort_validate_owner_after_restore();
	if (!escort_refresh_buddy_objnum())
		return;
	multi_restore_companion_robot_control(Buddy_objnum, Escort_owner_player);
}
#endif

void escort_rebuild_runtime_state_after_restore(void)
{
	ai_local *ailp = NULL;
	object *buddy_objp = NULL;
	input_demo_checkpoint_escort_state checkpoint_escort_state;
	input_demo_checkpoint_thief_state checkpoint_thief_state;
	int have_checkpoint_escort_state;
	int have_checkpoint_thief_state;
	fix64 raw_time_player_seen;
	fix64 raw_escort_last_path_created;
	int i;
#ifdef __ANDROID__
	int preserve_route_target_mode = Escort_route_target_mode_restore_pending;
	Escort_route_target_mode_restore_pending = 0;
	escort_route_note_replan("save_restore");
#endif

	input_demo_reset_escort_state_probes();
	input_demo_checkpoint_escort_state_clear(&checkpoint_escort_state);
	input_demo_checkpoint_thief_state_clear(&checkpoint_thief_state);
	have_checkpoint_escort_state = input_demo_replay_get_checkpoint_escort_state(&checkpoint_escort_state);
	have_checkpoint_thief_state = input_demo_replay_get_checkpoint_thief_state(&checkpoint_thief_state);

	Buddy_objnum = -1;
	Buddy_last_seen_player = 0;
	Buddy_last_player_path_created = 0;
	Last_come_back_message_time = 0;
	Buddy_last_missile_time = 0;

	for (i = 0; i <= Highest_object_index; i++)
		if ((Objects[i].type == OBJ_ROBOT) && Robot_info[Objects[i].id].companion) {
			Buddy_objnum = i;
			break;
		}

	if (Buddy_objnum != -1) {
		buddy_objp = &Objects[Buddy_objnum];
		ailp = &Ai_local_info[Buddy_objnum];
	}

	if (have_checkpoint_thief_state) {
		Stolen_item_index = checkpoint_thief_state.stolen_item_index;
		Re_init_thief_time = checkpoint_thief_state.re_init_thief_time;
		Last_thief_hit_time = checkpoint_thief_state.last_thief_hit_time;
	}

	if (have_checkpoint_escort_state) {
		Buddy_allowed_to_talk = checkpoint_escort_state.buddy_allowed_to_talk;
		Buddy_last_seen_player = checkpoint_escort_state.buddy_last_seen_player;
		Buddy_last_player_path_created = checkpoint_escort_state.buddy_last_player_path_created;
		if (checkpoint_escort_state.escort_kill_object != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Escort_kill_object = checkpoint_escort_state.escort_kill_object;
		Escort_last_path_created = checkpoint_escort_state.escort_last_path_created;
		if (checkpoint_escort_state.escort_goal_object != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Escort_goal_object = checkpoint_escort_state.escort_goal_object;
		if (checkpoint_escort_state.escort_special_goal != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Escort_special_goal = checkpoint_escort_state.escort_special_goal;
		if (checkpoint_escort_state.escort_goal_index != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Escort_goal_index = checkpoint_escort_state.escort_goal_index;
		if (checkpoint_escort_state.buddy_messages_suppressed != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Buddy_messages_suppressed = checkpoint_escort_state.buddy_messages_suppressed;
		if (checkpoint_escort_state.buddy_sorry_time != INPUT_DEMO_CHECKPOINT_ESCORT_I64_UNSET)
			Buddy_sorry_time = checkpoint_escort_state.buddy_sorry_time;
		if (checkpoint_escort_state.looking_for_marker != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Looking_for_marker = checkpoint_escort_state.looking_for_marker;
		if (checkpoint_escort_state.last_buddy_key != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Last_buddy_key = checkpoint_escort_state.last_buddy_key;
		if (checkpoint_escort_state.last_buddy_message_time != INPUT_DEMO_CHECKPOINT_ESCORT_I64_UNSET)
			Last_buddy_message_time = checkpoint_escort_state.last_buddy_message_time;
		Last_come_back_message_time = checkpoint_escort_state.last_come_back_message_time;
		Buddy_last_missile_time = checkpoint_escort_state.buddy_last_missile_time;
	#ifdef NETWORK
		if (checkpoint_escort_state.escort_owner_player != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			Escort_owner_player = checkpoint_escort_state.escort_owner_player;
	#endif
		input_demo_log_escort_restore_checkpoint(buddy_objp, ailp,
			have_checkpoint_thief_state, Buddy_messages_suppressed,
			Buddy_sorry_time, Looking_for_marker, Last_buddy_key,
			Last_buddy_message_time, Last_come_back_message_time,
			Buddy_last_missile_time, Re_init_thief_time,
			Last_thief_hit_time);
#ifdef NETWORK
		escort_restore_companion_robot_control();
#endif
		return;
	}

	if (Buddy_objnum == -1)
		return;

	raw_time_player_seen = ailp->time_player_seen;
	raw_escort_last_path_created = Escort_last_path_created;
#ifdef NETWORK
	escort_validate_owner_after_restore();
#endif
	Buddy_allowed_to_talk = 0;
	ok_for_buddy_to_talk();
	Buddy_last_seen_player = ailp->time_player_seen;
	if (Buddy_last_seen_player > GameTime64)
		Buddy_last_seen_player = GameTime64;
	if ((Buddy_last_seen_player < GameTime64 - MAX_ESCORT_TIME_AWAY) && ailp->previous_visibility)
		Buddy_last_seen_player = GameTime64;
	if (Buddy_last_seen_player < 0)
		Buddy_last_seen_player = 0;

	Buddy_last_player_path_created = Escort_last_path_created;
	if ((ailp->mode == AIM_GOTO_PLAYER) && (Buddy_last_player_path_created < GameTime64 - F1_0))
		Buddy_last_player_path_created = GameTime64;
	if (Buddy_last_player_path_created > GameTime64)
		Buddy_last_player_path_created = GameTime64;
	if (Buddy_last_player_path_created < 0)
		Buddy_last_player_path_created = 0;

	Last_come_back_message_time = Buddy_last_player_path_created;
	if (!input_demo_replay_is_loaded()) {
		Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
		Escort_special_goal = -1;
		Escort_goal_index = -1;
		escort_clear_secret_goal();
#ifdef __ANDROID__
		if (!preserve_route_target_mode)
			escort_route_set_target_mode(ESCORT_ROUTE_TARGET_END_OF_LEVEL);
		escort_route_clear_goal();
#endif
	}
#ifdef NETWORK
	escort_restore_companion_robot_control();
#endif
	input_demo_log_escort_restore_normalization(buddy_objp, ailp, raw_time_player_seen, raw_escort_last_path_created);
	input_demo_log_escort_restore_state(buddy_objp, ailp);
}

//	-----------------------------------------------------------------------------
void bash_buddy_weapon_info(int weapon_objnum)
{
	object	*objp = &Objects[weapon_objnum];

	objp->ctype.laser_info.parent_num = ConsoleObject-Objects;
	objp->ctype.laser_info.parent_type = OBJ_PLAYER;
	objp->ctype.laser_info.parent_signature = ConsoleObject->signature;
}

//	-----------------------------------------------------------------------------
int maybe_buddy_fire_mega(int objnum)
{
	object	*objp = &Objects[objnum];
	object	*buddy_objp = &Objects[Buddy_objnum];
	fix		dist, dot;
	vms_vector	vec_to_robot;
	int		weapon_objnum;

	vm_vec_sub(&vec_to_robot, &buddy_objp->pos, &objp->pos);
	dist = vm_vec_normalize_quick(&vec_to_robot);

	if (dist > F1_0*100)
		return 0;

	dot = vm_vec_dot(&vec_to_robot, &buddy_objp->orient.fvec);

	if (dot < F1_0/2)
		return 0;

	if (!object_to_object_visibility(buddy_objp, objp, FQ_TRANSWALL))
		return 0;

	if (Weapon_info[MEGA_ID].render_type == 0) {
		con_printf(CON_VERBOSE, "Buddy can't fire mega (shareware)\n");
		buddy_message("CLICK!");
		return 0;
	}

	buddy_message("GAHOOGA!");

	weapon_objnum = Laser_create_new_easy( &buddy_objp->orient.fvec, &buddy_objp->pos, objnum, MEGA_ID, 1);

	if (weapon_objnum != -1)
		bash_buddy_weapon_info(weapon_objnum);

	return 1;
}

//-----------------------------------------------------------------------------
int maybe_buddy_fire_smart(int objnum)
{
	object	*objp = &Objects[objnum];
	object	*buddy_objp = &Objects[Buddy_objnum];
	fix		dist;
	int		weapon_objnum;

	dist = vm_vec_dist_quick(&buddy_objp->pos, &objp->pos);

	if (dist > F1_0*80)
		return 0;

	if (!object_to_object_visibility(buddy_objp, objp, FQ_TRANSWALL))
		return 0;

	buddy_message("WHAMMO!");

	weapon_objnum = Laser_create_new_easy( &buddy_objp->orient.fvec, &buddy_objp->pos, objnum, SMART_ID, 1);

	if (weapon_objnum != -1)
		bash_buddy_weapon_info(weapon_objnum);

	return 1;
}

//	-----------------------------------------------------------------------------
void do_buddy_dude_stuff(void)
{
	int	i;

	if (!ok_for_buddy_to_talk())
		return;

	if (Buddy_last_missile_time > GameTime64)
		Buddy_last_missile_time = 0;

	if (Buddy_last_missile_time + F1_0*2 < GameTime64) {
		//	See if a robot potentially in view cone
		for (i=0; i<=Highest_object_index; i++)
			if ((Objects[i].type == OBJ_ROBOT) && !Robot_info[Objects[i].id].companion)
				if (maybe_buddy_fire_mega(i)) {
					Buddy_last_missile_time = GameTime64;
					return;
				}

		//	See if a robot near enough that buddy should fire smart missile
		for (i=0; i<=Highest_object_index; i++)
			if ((Objects[i].type == OBJ_ROBOT) && !Robot_info[Objects[i].id].companion)
				if (maybe_buddy_fire_smart(i)) {
					Buddy_last_missile_time = GameTime64;
					return;
				}

	}
}

//	-----------------------------------------------------------------------------
//	Called every frame (or something).
void do_escort_frame(object *objp, fix dist_to_player, int player_visibility)
{
	int			objnum = objp-Objects;
	ai_static	*aip = &objp->ctype.ai_info;
	ai_local		*ailp = &Ai_local_info[objnum];
	unsigned int replay_rng_state = 0;
	unsigned int replay_rng_call_count = 0;
	int replay_should_visit_player = 0;
	fix64 replay_since_seen = 0;
	fix64 replay_since_player_path = 0;
	int replay_visit_away_gate = 0;
	int replay_visit_recent_path_gate = 0;
	int replay_visit_goto_player_gate = 0;
	int replay_visit_same_seg_gate = 0;
	int replay_visit_early_path_gate = 0;
	int replay_player_seg = -1;
	int replay_believed_seg = -1;
	int replay_state_probe_active = input_demo_trace_escort_active() &&
		Robot_info[objp->id].companion;
	int replay_rng_probe_active = input_demo_trace_escort_active() &&
		Robot_info[objp->id].companion && d_rand_get_state(&replay_rng_state);
	if (replay_rng_probe_active)
		replay_rng_call_count = d_rand_get_call_count();

	Buddy_objnum = objp-Objects;

	if (player_visibility) {
		Buddy_last_seen_player = GameTime64;
		if (Players[Player_num].flags & PLAYER_FLAGS_HEADLIGHT_ON)	//	DAMN! MK, stupid bug, fixed 12/08/95, changed PLAYER_FLAGS_HEADLIGHT to PLAYER_FLAGS_HEADLIGHT_ON
			if (f2i(Players[Player_num].energy) < 40)
				if ((f2i(Players[Player_num].energy)/2) & 2)
					if (!Player_is_dead)
						buddy_message("Hey, your headlight's on!");

	}

	if (cheats.buddyangry)
		do_buddy_dude_stuff();

	if (Buddy_sorry_time + F1_0 > GameTime64) {
		Last_buddy_message_time = 0;	//	Force this message to get through.
		if (Buddy_sorry_time < GameTime64 + F1_0*2)
			buddy_message("Oops, sorry 'bout that...");
		Buddy_sorry_time = -F1_0*2;
	}

	//	If buddy not allowed to talk, then he is locked in his room.  Make him mostly do nothing unless you're nearby.
	if (!Buddy_allowed_to_talk)
		if (dist_to_player > F1_0*100)
			aip->SKIP_AI_COUNT = (F1_0/4)/FrameTime;

	//	AIM_WANDER has been co-opted for buddy behavior (didn't want to modify aistruct.h)
	//	It means the object has been told to get lost and has come to the end of its path.
	//	If the player is now visible, then create a path.
	if (ailp->mode == AIM_WANDER)
		if (player_visibility) {
			// SIM RNG: this changes the live rejoin path the guidebot follows
			create_n_segment_path(objp, 16 + d_rand() * 16, -1);
			aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
			if (replay_rng_probe_active)
				input_demo_log_escort_rng_progress("after AIM_WANDER create_n_segment_path", &replay_rng_state, &replay_rng_call_count);
		}

	if (Escort_special_goal == ESCORT_GOAL_SCRAM) {
		if (player_visibility)
			if (Escort_last_path_created + F1_0*3 < GameTime64) {
				// SIM RNG: this changes the live scram path the guidebot follows
				create_n_segment_path(objp, 10 + d_rand() * 16, ConsoleObject->segnum);
				Escort_last_path_created = GameTime64;
				if (replay_rng_probe_active)
					input_demo_log_escort_rng_progress("after ESCORT_GOAL_SCRAM create_n_segment_path", &replay_rng_state, &replay_rng_call_count);
			}

		return;
	}

	//	Force checking for new goal every 5 seconds, and create new path, if necessary.
	if (((Escort_special_goal != ESCORT_GOAL_SCRAM) && ((Escort_last_path_created + F1_0*5) < GameTime64)) ||
		((Escort_special_goal == ESCORT_GOAL_SCRAM) && ((Escort_last_path_created + F1_0*15) < GameTime64))) {
		if (replay_state_probe_active)
			input_demo_log_escort_goal_reset();
		Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
		Escort_last_path_created = GameTime64;
	}
	if (replay_state_probe_active) {
		replay_player_seg = ConsoleObject->segnum;
		replay_believed_seg = Believed_player_seg;
		input_demo_log_escort_segment_change(objp, ailp, aip, replay_player_seg, replay_believed_seg);
		replay_since_seen = GameTime64 - Buddy_last_seen_player;
		replay_since_player_path = GameTime64 - Buddy_last_player_path_created;
		replay_visit_away_gate = replay_since_seen > MAX_ESCORT_TIME_AWAY;
		replay_visit_recent_path_gate = replay_since_player_path <= F1_0;
		replay_visit_goto_player_gate = ailp->mode == AIM_GOTO_PLAYER;
		replay_visit_same_seg_gate = objp->segnum == replay_player_seg;
		replay_visit_early_path_gate = aip->cur_path_index < aip->path_length/2;
		replay_should_visit_player = (Escort_special_goal != ESCORT_GOAL_SCRAM) && time_to_visit_player(objp, ailp, aip);
		input_demo_log_escort_visit_change(objp, ailp, aip, replay_player_seg, replay_believed_seg,
			replay_visit_away_gate, replay_visit_recent_path_gate, replay_visit_goto_player_gate,
			replay_visit_same_seg_gate, replay_visit_early_path_gate, replay_should_visit_player);
	}
	if (replay_state_probe_active)
		input_demo_log_escort_state(objp, ailp, aip, dist_to_player, player_visibility,
			replay_should_visit_player, replay_since_seen, replay_since_player_path,
			replay_visit_away_gate, replay_visit_recent_path_gate,
			replay_visit_goto_player_gate, replay_visit_same_seg_gate,
			replay_visit_early_path_gate);

	if ((Escort_special_goal != ESCORT_GOAL_SCRAM) && (replay_state_probe_active ? replay_should_visit_player : time_to_visit_player(objp, ailp, aip))) {
		int	max_len;

		Buddy_last_player_path_created = GameTime64;
		ailp->mode = AIM_GOTO_PLAYER;
		if (!player_visibility) {
			if ((Last_come_back_message_time + F1_0 < GameTime64) || (Last_come_back_message_time > GameTime64)) {
				buddy_message("Coming back to get you.");
				Last_come_back_message_time = GameTime64;
			}
		}
		//	No point in Buddy creating very long path if he's not allowed to talk.  Really kills framerate.
		max_len = Max_escort_length;
		if (!Buddy_allowed_to_talk)
			max_len = 3;
		create_path_to_player(objp, max_len, 1);	//	MK!: Last parm used to be 1!
		aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
		input_demo_log_escort_path_state("time_to_visit_player final", objp);
		if (replay_rng_probe_active)
			input_demo_log_escort_rng_progress("after time_to_visit_player create_path_to_player", &replay_rng_state, &replay_rng_call_count);
		ailp->mode = AIM_GOTO_PLAYER;
	} else if (GameTime64 - Buddy_last_seen_player > MAX_ESCORT_TIME_AWAY) {
		//	This is to prevent buddy from looking for a goal, which he will do because we only allow path creation once/second.
		return;
	} else if ((ailp->mode == AIM_GOTO_PLAYER) &&
		(aip->cur_path_index >= aip->path_length/2) &&
		(dist_to_player < MIN_ESCORT_DISTANCE - F1_0/4)) {
		Escort_goal_object = escort_set_goal_object();
		ailp->mode = AIM_GOTO_OBJECT;		//	May look stupid to be before path creation, but ai_door_is_openable uses mode to determine what doors can be got through
		escort_create_path_to_goal(objp);
		if (replay_rng_probe_active)
			input_demo_log_escort_rng_progress("after AIM_GOTO_PLAYER escort_create_path_to_goal", &replay_rng_state, &replay_rng_call_count);
		aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
			input_demo_log_escort_path_state("AIM_GOTO_PLAYER final", objp);
		if (aip->path_length < 3) {
			create_n_segment_path(objp, 5, Believed_player_seg);
				input_demo_log_escort_path_state("AIM_GOTO_PLAYER fallback", objp);
			if (replay_rng_probe_active)
				input_demo_log_escort_rng_progress("after AIM_GOTO_PLAYER fallback create_n_segment_path", &replay_rng_state, &replay_rng_call_count);
		}
		ailp->mode = AIM_GOTO_OBJECT;
	} else if (Escort_goal_object == ESCORT_GOAL_UNSPECIFIED) {
		if ((ailp->mode != AIM_GOTO_PLAYER) ||
			((aip->cur_path_index >= aip->path_length/2) &&
			 (dist_to_player < MIN_ESCORT_DISTANCE - F1_0/4))) {
			Escort_goal_object = escort_set_goal_object();
			ailp->mode = AIM_GOTO_OBJECT;		//	May look stupid to be before path creation, but ai_door_is_openable uses mode to determine what doors can be got through
			escort_create_path_to_goal(objp);
			if (replay_rng_probe_active)
				input_demo_log_escort_rng_progress("after unspecified escort_create_path_to_goal", &replay_rng_state, &replay_rng_call_count);
			aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
			input_demo_log_escort_path_state("unspecified goal final", objp);
			if (aip->path_length < 3) {
				create_n_segment_path(objp, 5, Believed_player_seg);
				input_demo_log_escort_path_state("unspecified fallback", objp);
				if (replay_rng_probe_active)
					input_demo_log_escort_rng_progress("after unspecified fallback create_n_segment_path", &replay_rng_state, &replay_rng_call_count);
			}
			ailp->mode = AIM_GOTO_OBJECT;
		}
	}

}

void invalidate_escort_goal(void)
{
	Escort_goal_object = -1;
#ifdef __ANDROID__
	escort_route_clear_goal();
	escort_route_note_replan("goal_invalidated");
#endif
}

void escort_note_player_key_flags(int old_flags, int new_flags)
{
	if ((old_flags ^ new_flags) & ESCORT_KEY_FLAGS) {
		invalidate_escort_goal();
#ifdef __ANDROID__
		escort_route_note_replan("key_change");
#endif
	}
}

//	-------------------------------------------------------------------------------------------------
void do_snipe_frame(object *objp, fix dist_to_player, int player_visibility, vms_vector *vec_to_player)
{
	int			objnum = objp-Objects;
	ai_local		*ailp = &Ai_local_info[objnum];
	fix			connected_distance;
	int			replay_snipe_probe_active = input_demo_trace_snipe_detail_active(objp);

	if (replay_snipe_probe_active)
		input_demo_log_snipe_detail_probe(1,
			"entry", objp, ailp, player_visibility, dist_to_player);

	if (dist_to_player > F1_0*500)
		return;

	switch (ailp->mode) {
		case AIM_SNIPE_WAIT:
			if ((dist_to_player > F1_0*50) && (ailp->next_action_time > 0))
				return;

			ailp->next_action_time = SNIPE_WAIT_TIME;

			connected_distance = find_connected_distance(&objp->pos, objp->segnum, &Believed_player_pos, Believed_player_seg, 30, WID_FLY_FLAG);
			if (connected_distance < F1_0*500) {
				create_path_to_player(objp, 30, 1);
				ailp->mode = AIM_SNIPE_ATTACK;
				ailp->next_action_time = SNIPE_ATTACK_TIME;	//	have up to 10 seconds to find player.
			}
			break;

		case AIM_SNIPE_RETREAT:
		case AIM_SNIPE_RETREAT_BACKWARDS:
			if (ailp->next_action_time < 0) {
				ailp->mode = AIM_SNIPE_WAIT;
				ailp->next_action_time = SNIPE_WAIT_TIME;
			} else if ((player_visibility == 0) || (ailp->next_action_time > SNIPE_ABORT_RETREAT_TIME)) {
				ai_follow_path(objp, player_visibility, player_visibility, vec_to_player);
				ailp->mode = AIM_SNIPE_RETREAT_BACKWARDS;
			} else {
				ailp->mode = AIM_SNIPE_FIRE;
				ailp->next_action_time = SNIPE_FIRE_TIME/2;
			}
			break;

		case AIM_SNIPE_ATTACK:
			if (ailp->next_action_time < 0) {
				ailp->mode = AIM_SNIPE_RETREAT;
				ailp->next_action_time = SNIPE_WAIT_TIME;
			} else {
				ai_follow_path(objp, player_visibility, player_visibility, vec_to_player);
				if (player_visibility) {
					ailp->mode = AIM_SNIPE_FIRE;
					ailp->next_action_time = SNIPE_FIRE_TIME;
				} else
					ailp->mode = AIM_SNIPE_ATTACK;
			}
			break;

		case AIM_SNIPE_FIRE:
			if (ailp->next_action_time < 0) {
				ai_static	*aip = &objp->ctype.ai_info;
				// SIM RNG: these rolls set the live retreat path and retreat mode
				create_n_segment_path(objp, 10 + d_rand()/2048, ConsoleObject->segnum);
				aip->path_length = polish_path(objp, &Point_segs[aip->hide_index], aip->path_length);
				if (d_rand() < 8192)
					ailp->mode = AIM_SNIPE_RETREAT_BACKWARDS;
				else
					ailp->mode = AIM_SNIPE_RETREAT;
				ailp->next_action_time = SNIPE_RETREAT_TIME;
			} else {
			}
			break;

		default:
			Int3();	//	Oops, illegal mode for snipe behavior.
			ailp->mode = AIM_SNIPE_ATTACK;
			ailp->next_action_time = F1_0;
			break;
	}

	if (replay_snipe_probe_active)
		input_demo_log_snipe_detail_probe(0,
			"exit", objp, ailp, player_visibility, dist_to_player);

}

#define	THIEF_DEPTH	20

extern int pick_connected_segment(object *objp, int max_depth);

//	------------------------------------------------------------------------------------------------------
//	Choose segment to recreate thief in.
int choose_thief_recreation_segment(void)
{
	int	segnum = -1;
	int	cur_drop_depth;

	cur_drop_depth = THIEF_DEPTH;

	while ((segnum == -1) && (cur_drop_depth > THIEF_DEPTH/2)) {
		segnum = pick_connected_segment(&Objects[Players[Player_num].objnum], cur_drop_depth);
		if (Segment2s[segnum].special == SEGMENT_IS_CONTROLCEN)
			segnum = -1;
		cur_drop_depth--;
	}

	if (segnum == -1) {
		// SIM RNG: this picks the thief's live respawn segment if no connected path works
		return (d_rand() * Highest_segment_index) >> 15;
	} else
		return segnum;

}

extern object * create_morph_robot( segment *segp, vms_vector *object_pos, int object_id);

fix64	Re_init_thief_time = 0x3f000000;

//	----------------------------------------------------------------------
void recreate_thief(object *objp)
{
	int			segnum;
	vms_vector	center_point;
	object		*new_obj;

	segnum = choose_thief_recreation_segment();
	compute_segment_center(&center_point, &Segments[segnum]);

	new_obj = create_morph_robot( &Segments[segnum], &center_point, objp->id);
	init_ai_object(new_obj-Objects, AIB_SNIPE, -1);
	Re_init_thief_time = GameTime64 + F1_0*10;		//	In 10 seconds, re-initialize thief.
}

//	----------------------------------------------------------------------------
#define	THIEF_ATTACK_TIME		(F1_0*10)

fix	Thief_wait_times[NDL] = {F1_0*30, F1_0*25, F1_0*20, F1_0*15, F1_0*10};

//	-------------------------------------------------------------------------------------------------
void do_thief_frame(object *objp, fix dist_to_player, int player_visibility, vms_vector *vec_to_player)
{
	int			objnum = objp-Objects;
	ai_local		*ailp = &Ai_local_info[objnum];
	fix			connected_distance;
	int			replay_thief_probe_active = input_demo_trace_thief_detail_active(objp);

	if ((Current_level_num < 0) && (Re_init_thief_time < GameTime64)) {
		if (Re_init_thief_time > GameTime64 - F1_0*2)
			init_thief_for_level();
		Re_init_thief_time = 0x3f000000;
	}

	if ((dist_to_player > F1_0*500) && (ailp->next_action_time > 0))
		return;

	if (replay_thief_probe_active)
		input_demo_log_thief_detail_probe(1,
			"entry", objp, ailp, player_visibility, dist_to_player);

	if (Player_is_dead)
		ailp->mode = AIM_THIEF_RETREAT;

	switch (ailp->mode) {
		case AIM_THIEF_WAIT:
			if (ailp->player_awareness_type >= PA_PLAYER_COLLISION) {
				ailp->player_awareness_type = 0;
				create_path_to_player(objp, 30, 1);
				ailp->mode = AIM_THIEF_ATTACK;
				ailp->next_action_time = THIEF_ATTACK_TIME/2;
				return;
			} else if (player_visibility) {
				create_n_segment_path(objp, 15, ConsoleObject->segnum);
				ailp->mode = AIM_THIEF_RETREAT;
				return;
			}

			if ((dist_to_player > F1_0*50) && (ailp->next_action_time > 0))
				return;

			ailp->next_action_time = Thief_wait_times[Difficulty_level]/2;

			connected_distance = find_connected_distance(&objp->pos, objp->segnum, &Believed_player_pos, Believed_player_seg, 30, WID_FLY_FLAG);
			if (connected_distance < F1_0*500) {
				create_path_to_player(objp, 30, 1);
				ailp->mode = AIM_THIEF_ATTACK;
				ailp->next_action_time = THIEF_ATTACK_TIME;	//	have up to 10 seconds to find player.
			}

			break;

		case AIM_THIEF_RETREAT:
			if (ailp->next_action_time < 0) {
				ailp->mode = AIM_THIEF_WAIT;
				ailp->next_action_time = Thief_wait_times[Difficulty_level];
			} else if ((dist_to_player < F1_0*100) || player_visibility || (ailp->player_awareness_type >= PA_PLAYER_COLLISION)) {
				ai_follow_path(objp, player_visibility, player_visibility, vec_to_player);
				if ((dist_to_player < F1_0*100) || (ailp->player_awareness_type >= PA_PLAYER_COLLISION)) {
					ai_static	*aip = &objp->ctype.ai_info;
					if (((aip->cur_path_index <=1) && (aip->PATH_DIR == -1)) || ((aip->cur_path_index >= aip->path_length-1) && (aip->PATH_DIR == 1))) {
						ailp->player_awareness_type = 0;
						create_n_segment_path(objp, 10, ConsoleObject->segnum);

						//	If path is real short, try again, allowing to go through player's segment
						if (aip->path_length < 4) {
							create_n_segment_path(objp, 10, -1);
						} else if (objp->shields* 4 < Robot_info[objp->id].strength) {
							//	If robot really low on hits, will run through player with even longer path
							if (aip->path_length < 8) {
								create_n_segment_path(objp, 10, -1);
							}
						}

						ailp->mode = AIM_THIEF_RETREAT;
					}
				} else
					ailp->mode = AIM_THIEF_RETREAT;

			}

			break;

		//	This means the thief goes from wherever he is to the player.
		//	Note: When thief successfully steals something, his action time is forced negative and his mode is changed
		//			to retreat to get him out of attack mode.
		case AIM_THIEF_ATTACK:
			if (ailp->player_awareness_type >= PA_PLAYER_COLLISION) {
				ailp->player_awareness_type = 0;
				// SIM RNG: this decides whether the thief breaks off into a retreat path
				if (d_rand() > 8192) {
					create_n_segment_path(objp, 10, ConsoleObject->segnum);
					Ai_local_info[objp-Objects].next_action_time = Thief_wait_times[Difficulty_level]/2;
					Ai_local_info[objp-Objects].mode = AIM_THIEF_RETREAT;
				}
			} else if (ailp->next_action_time < 0) {
				//	This forces him to create a new path every second.
				ailp->next_action_time = F1_0;
				create_path_to_player(objp, 100, 0);
				ailp->mode = AIM_THIEF_ATTACK;
			} else {
				if (player_visibility && (dist_to_player < F1_0*100)) {
					//	If the player is close to looking at the thief, thief shall run away.
					//	No more stupid thief trying to sneak up on you when you're looking right at him!
					if (dist_to_player > F1_0*60) {
						fix	dot = vm_vec_dot(vec_to_player, &ConsoleObject->orient.fvec);
						if (dot < -F1_0/2) {	//	Looking at least towards thief, so thief will run!
							create_n_segment_path(objp, 10, ConsoleObject->segnum);
							Ai_local_info[objp-Objects].next_action_time = Thief_wait_times[Difficulty_level]/2;
							Ai_local_info[objp-Objects].mode = AIM_THIEF_RETREAT;
						}
					} 
					ai_turn_towards_vector(vec_to_player, objp, F1_0/4);
					move_towards_player(objp, vec_to_player);
				} else {
					ai_static	*aip = &objp->ctype.ai_info;
					//	If path length == 0, then he will keep trying to create path, but he is probably stuck in his closet.
					if ((aip->path_length > 1) || ((d_tick_count & 0x0f) == 0)) {
						ai_follow_path(objp, player_visibility, player_visibility, vec_to_player);
						ailp->mode = AIM_THIEF_ATTACK;
					}
				}
			}
			break;

		default:
			ailp->mode = AIM_THIEF_ATTACK;
			ailp->next_action_time = F1_0;
			break;
	}

	if (replay_thief_probe_active)
		input_demo_log_thief_detail_probe(0,
			"exit", objp, ailp, player_visibility, dist_to_player);

}

//	----------------------------------------------------------------------------
//	Return true if this item (whose presence is indicated by Players[player_num].flags) gets stolen.
int maybe_steal_flag_item(int player_num, int flagval)
{
	if (Players[player_num].flags & flagval) {
		// SIM RNG: this decides whether the thief removes a real flag-backed inventory item
		if (d_rand() < THIEF_PROBABILITY) {
			int	powerup_index=-1;
			switch (flagval) {
				case PLAYER_FLAGS_INVULNERABLE:
					powerup_index = POW_INVULNERABILITY;
					thief_message("Invulnerability stolen!");
					break;
				case PLAYER_FLAGS_CLOAKED:
					powerup_index = POW_CLOAK;
					thief_message("Cloak stolen!");
					break;
				case PLAYER_FLAGS_MAP_ALL:
					powerup_index = POW_FULL_MAP;
					thief_message("Full map stolen!");
					break;
				case PLAYER_FLAGS_QUAD_LASERS:
					powerup_index = POW_QUAD_FIRE;
					thief_message("Quad lasers stolen!");
					break;
				case PLAYER_FLAGS_AFTERBURNER:
					powerup_index = POW_AFTERBURNER;
					thief_message("Afterburner stolen!");
					break;
// --				case PLAYER_FLAGS_AMMO_RACK:
// --					powerup_index = POW_AMMO_RACK;
// --					thief_message("Ammo Rack stolen!");
// --					break;
				case PLAYER_FLAGS_CONVERTER:
					powerup_index = POW_CONVERTER;
					thief_message("Converter stolen!");
					break;
				case PLAYER_FLAGS_HEADLIGHT:
					powerup_index = POW_HEADLIGHT;
					thief_message("Headlight stolen!");
					break;
			}
			Assert(powerup_index != -1);
			if (!thief_store_stolen_item((ubyte)powerup_index))
				return 0;
			Players[player_num].flags &= (~flagval);
			if (flagval == PLAYER_FLAGS_HEADLIGHT)
				Players[player_num].flags &= ~PLAYER_FLAGS_HEADLIGHT_ON;

			digi_play_sample_once(SOUND_WEAPON_STOLEN, F1_0);
			return 1;
		}
	}

	return 0;
}

//	----------------------------------------------------------------------------
int maybe_steal_secondary_weapon(int player_num, int weapon_num)
{
	if ((Players[player_num].secondary_weapon_flags & HAS_FLAG(weapon_num)) && Players[player_num].secondary_ammo[weapon_num])
		// SIM RNG: these rolls decide whether the thief removes real secondary inventory
		if (d_rand() < THIEF_PROBABILITY) {
			int stolen_item = -1;

			if ((weapon_num == PROXIMITY_INDEX) && !thief_full_drop_enabled())
				if (d_rand() > 8192)		//	Come in groups of 4, only add 1/4 of time.
					return 0;

			if (thief_full_drop_enabled()) {
				if (weapon_num == PROXIMITY_INDEX)
					stolen_item = STOLEN_ITEM_PROXIMITY_MINE;
				else if (weapon_num == SMART_MINE_INDEX)
					stolen_item = STOLEN_ITEM_SMART_MINE;
			}
			if ((weapon_num != PROXIMITY_INDEX) && (weapon_num != SMART_MINE_INDEX))
				stolen_item = Secondary_weapon_to_powerup[weapon_num];

			if ((stolen_item != -1) && !thief_store_stolen_item((ubyte)stolen_item))
				return 0;

			Players[player_num].secondary_ammo[weapon_num]--;

			thief_message("%s stolen!", SECONDARY_WEAPON_NAMES(weapon_num));		//	Danger! Danger! Use of literal!  Danger!
			if (Players[Player_num].secondary_ammo[weapon_num] == 0)
				auto_select_weapon(1);

			// -- compress_stolen_items();
			digi_play_sample_once(SOUND_WEAPON_STOLEN, F1_0);
			return 1;
		}

	return 0;
}

//	----------------------------------------------------------------------------
int maybe_steal_primary_weapon(int player_num, int weapon_num)
{
	if ((Players[player_num].primary_weapon_flags & HAS_FLAG(weapon_num)) && Players[player_num].primary_ammo[weapon_num]) {
		// SIM RNG: this decides whether the thief removes a real primary weapon or laser level
		if (d_rand() < THIEF_PROBABILITY) {
			if (weapon_num == 0) {
				if (Players[player_num].laser_level > 0) {
					ubyte stolen_item;

					if (Players[player_num].laser_level > 3) {
						stolen_item = POW_SUPER_LASER;
					} else {
						stolen_item = Primary_weapon_to_powerup[weapon_num];
					}
					if (!thief_store_stolen_item(stolen_item))
						return 0;
					thief_message("%s level decreased!", PRIMARY_WEAPON_NAMES(weapon_num));		//	Danger! Danger! Use of literal!  Danger!
					Players[player_num].laser_level--;
					digi_play_sample_once(SOUND_WEAPON_STOLEN, F1_0);
					return 1;
				}
			} else if (Players[player_num].primary_weapon_flags & (1 << weapon_num)) {
				if (!thief_store_stolen_item(Primary_weapon_to_powerup[weapon_num]))
					return 0;
				Players[player_num].primary_weapon_flags &= ~(1 << weapon_num);

				thief_message("%s stolen!", PRIMARY_WEAPON_NAMES(weapon_num));		//	Danger! Danger! Use of literal!  Danger!
				auto_select_weapon(0);
				digi_play_sample_once(SOUND_WEAPON_STOLEN, F1_0);
				return 1;
			}
		}
	}

	return 0;
}



//	----------------------------------------------------------------------------
//	Called for a thief-type robot.
//	If a item successfully stolen, returns true, else returns false.
//	If a wapon successfully stolen, do everything, removing it from player,
//	updating Stolen_items information, deselecting, etc.
int attempt_to_steal_item_3(object *objp, int player_num)
{
	int	i;

	if (Ai_local_info[objp-Objects].mode != AIM_THIEF_ATTACK)
		return 0;

	//	First, try to steal equipped items.

	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_INVULNERABLE))
		return 1;

	//	If primary weapon = laser, first try to rip away those nasty quad lasers!
	if (Players[Player_num].primary_weapon == 0)
		if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_QUAD_LASERS))
			return 1;

	//	Makes it more likely to steal primary than secondary.
	for (i=0; i<2; i++)
		if (maybe_steal_primary_weapon(player_num, Players[Player_num].primary_weapon))
			return 1;

	if (maybe_steal_secondary_weapon(player_num, Players[Player_num].secondary_weapon))
		return 1;

	//	See what the player has and try to snag something.
	//	Try best things first.
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_INVULNERABLE))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_CLOAKED))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_QUAD_LASERS))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_AFTERBURNER))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_CONVERTER))
		return 1;
// --	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_AMMO_RACK))	//	Can't steal because what if have too many items, say 15 homing missiles?
// --		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_HEADLIGHT))
		return 1;
	if (maybe_steal_flag_item(player_num, PLAYER_FLAGS_MAP_ALL))
		return 1;

	for (i=MAX_SECONDARY_WEAPONS-1; i>=0; i--) {
		if (maybe_steal_primary_weapon(player_num, i))
			return 1;
		if (maybe_steal_secondary_weapon(player_num, i))
			return 1;
	}

	return 0;
}

//	----------------------------------------------------------------------------
int attempt_to_steal_item_2(object *objp, int player_num)
{
	int	rval;

	rval = attempt_to_steal_item_3(objp, player_num);

	if (rval) {
		Stolen_item_index = (Stolen_item_index+1) % MAX_STOLEN_ITEMS;
		// SIM RNG: this decides whether a successful theft consumes an extra stolen-item slot
		if (d_rand() > 20000)	//	Occasionally, boost the value again
			Stolen_item_index = (Stolen_item_index+1) % MAX_STOLEN_ITEMS;
	}

	return rval;
}

//	----------------------------------------------------------------------------
//	Called for a thief-type robot.
//	If a item successfully stolen, returns true, else returns false.
//	If a wapon successfully stolen, do everything, removing it from player,
//	updating Stolen_items information, deselecting, etc.
int attempt_to_steal_item(object *objp, int player_num)
{
	int	i;
	int	rval = 0;

	if (objp->ctype.ai_info.dying_start_time)
		return 0;

	rval += attempt_to_steal_item_2(objp, player_num);

	for (i=0; i<3; i++) {
		// SIM RNG: this decides whether the thief steals additional real items in the same attack
		if (!rval || (d_rand() < 11000)) {	//	about 1/3 of time, steal another item
			rval += attempt_to_steal_item_2(objp, player_num);
		} else
			break;
	}
	create_n_segment_path(objp, 10, ConsoleObject->segnum);
	Ai_local_info[objp-Objects].next_action_time = Thief_wait_times[Difficulty_level]/2;
	Ai_local_info[objp-Objects].mode = AIM_THIEF_RETREAT;
	if (rval) {
		PALETTE_FLASH_ADD(30, 15, -20);
		update_laser_weapon_info();
//		digi_link_sound_to_pos( SOUND_NASTY_ROBOT_HIT_1, objp->segnum, 0, &objp->pos, 0 , DEFAULT_ROBOT_SOUND_VOLUME);
//	I removed this to make the "steal sound" more obvious -AP
#ifdef NETWORK
                if (Game_mode & GM_NETWORK)
                 multi_send_stolen_items();
#endif
	}
	return rval;
}

// --------------------------------------------------------------------------------------------------------------
//	Indicate no items have been stolen.
void init_thief_for_level(void)
{
	int	i;

	for (i=0; i<MAX_STOLEN_ITEMS; i++)
		Stolen_items[i] = 255;

	Assert (MAX_STOLEN_ITEMS >= 3*2);	//	Oops!  Loop below will overwrite memory!
  
   if (!(Game_mode & GM_MULTI) || (Game_mode & GM_MULTI_COOP))
		for (i=0; i<3; i++) {
			Stolen_items[2*i] = POW_SHIELD_BOOST;
			Stolen_items[2*i+1] = POW_ENERGY;
		}

	Stolen_item_index = 0;
}

// --------------------------------------------------------------------------------------------------------------
void drop_stolen_items(object *objp)
{
	int	i;

	for (i=0; i<MAX_STOLEN_ITEMS; i++) {
		if (Stolen_items[i] == STOLEN_ITEM_PROXIMITY_MINE)
			thief_drop_stolen_mine(objp, PROXIMITY_ID);
		else if (Stolen_items[i] == STOLEN_ITEM_SMART_MINE)
			thief_drop_stolen_mine(objp, SUPERPROX_ID);
		else if (Stolen_items[i] != STOLEN_ITEM_NONE)
			drop_powerup(OBJ_POWERUP, Stolen_items[i], 1, &objp->mtype.phys_info.velocity, &objp->pos, objp->segnum);
		Stolen_items[i] = STOLEN_ITEM_NONE;
	}
	Stolen_item_index = 0;

}

// --------------------------------------------------------------------------------------------------------------
#define ESCORT_MENU_ITEM_COUNT 11

typedef struct escort_menu
{
	char	goal_str[32];
	char	message_action[16];
	int	selected_item;
	int	multiplayer_passthrough;
} escort_menu;

static int escort_menu_key_for_item(int item)
{
	switch (item) {
		case 0: return KEY_0;
		case 1: return KEY_1;
		case 2: return KEY_2;
		case 3: return KEY_3;
		case 4: return KEY_4;
		case 5: return KEY_5;
		case 6: return KEY_6;
		case 7: return KEY_7;
		case 8: return KEY_8;
		case 9: return KEY_9;
		case 10: return KEY_T;
		default: return KEY_ESC;
	}
}

static void escort_menu_item_text(escort_menu *menu, int item, char *buf, size_t bufsz)
{
	switch (item) {
		case 0:
			snprintf(buf, bufsz, "0.  Next Goal: %s", menu->goal_str);
			break;
		case 1:
			snprintf(buf, bufsz, "1.  Find Energy Powerup");
			break;
		case 2:
			snprintf(buf, bufsz, "2.  Find Energy Center");
			break;
		case 3:
			snprintf(buf, bufsz, "3.  Find Shield Powerup");
			break;
		case 4:
			snprintf(buf, bufsz, "4.  Find Any Powerup");
			break;
		case 5:
			snprintf(buf, bufsz, "5.  Find a Robot");
			break;
		case 6:
			snprintf(buf, bufsz, "6.  Find a Hostage");
			break;
		case 7:
			snprintf(buf, bufsz, "7.  Stay Away From Me");
			break;
		case 8:
			snprintf(buf, bufsz, "8.  Find My Powerups");
			break;
		case 9:
			snprintf(buf, bufsz, "9.  Find the exit");
			break;
		case 10:
			snprintf(buf, bufsz, "T.  %s Messages", menu->message_action);
			break;
		default:
			buf[0] = '\0';
			break;
	}
}

static void escort_menu_move_selection(escort_menu *menu, int delta)
{
	menu->selected_item = (menu->selected_item + delta + ESCORT_MENU_ITEM_COUNT) % ESCORT_MENU_ITEM_COUNT;
}

static int escort_menu_activate_key(window *wind, int key)
{
	char error[256] = "";

	switch (key) {
		case KEY_0:
		case KEY_1:
		case KEY_2:
		case KEY_3:
		case KEY_4:
		case KEY_5:
		case KEY_6:
		case KEY_7:
		case KEY_8:
		case KEY_9:
			if (input_demo_recorder_is_active() &&
			    !input_demo_recorder_stage_direct_command_guidebot_goal(key, 1, error, sizeof(error)) &&
			    error[0])
				con_printf(CON_NORMAL, "Input demo recorder guidebot goal event failed: %s\n", error);
			Looking_for_marker = -1;
			Last_buddy_key = -1;
			set_escort_special_goal(key);
			Last_buddy_key = -1;
			window_close(wind);
			return 1;

		case KEY_T: {
			char	msg[32];
			int	temp;

			temp = !Buddy_messages_suppressed;

			if (temp)
				strcpy(msg, "suppressed");
			else
				strcpy(msg, "enabled");

			Buddy_messages_suppressed = 1;
			buddy_message("Messages %s.", msg);

			Buddy_messages_suppressed = temp;

			window_close(wind);
			return 1;
		}

		default:
			break;
	}

	return 0;
}

int escort_menu_keycommand(window *wind, d_event *event, escort_menu *menu)
{
	int	key;

	key = event_key_get(event);

	switch (key) {
		case KEY_0:
		case KEY_1:
		case KEY_2:
		case KEY_3:
		case KEY_4:
		case KEY_5:
		case KEY_6:
		case KEY_7:
		case KEY_8:
		case KEY_9:
		case KEY_T:
			return escort_menu_activate_key(wind, key);

		case KEY_UP:
			escort_menu_move_selection(menu, -1);
			return 1;

		case KEY_DOWN:
			escort_menu_move_selection(menu, 1);
			return 1;

		case KEY_ENTER:
			return escort_menu_activate_key(wind, escort_menu_key_for_item(menu->selected_item));

		case KEY_ESC:
		case KEY_F4 + KEY_SHIFTED:
			window_close(wind);
			return 1;

		default:
			break;
	}

	return 0;
}

#ifdef __ANDROID__
static int escort_menu_joystick_button_down(window *wind, d_event *event, escort_menu *menu)
{
	switch (event_joystick_get_button(event)) {
		case ANDROID_DPAD_UP_BUTTON:
			escort_menu_move_selection(menu, -1);
			return 1;

		case ANDROID_DPAD_DOWN_BUTTON:
			escort_menu_move_selection(menu, 1);
			return 1;

		case ANDROID_DPAD_LEFT_BUTTON:
		case ANDROID_DPAD_RIGHT_BUTTON:
			return 1;

		case ANDROID_JOY_BUTTON_A:
			return escort_menu_activate_key(wind, escort_menu_key_for_item(menu->selected_item));

		case ANDROID_JOY_BUTTON_B:
			window_close(wind);
			return 1;

		default:
			break;
	}

	return 0;
}
#endif

int escort_menu_handler(window *wind, d_event *event, escort_menu *menu)
{
	switch (event->type)
	{
		case EVENT_WINDOW_ACTIVATED:
			game_flush_inputs();
			break;
			
		case EVENT_KEY_COMMAND:
			return escort_menu_keycommand(wind, event, menu);

#ifdef __ANDROID__
		case EVENT_JOYSTICK_BUTTON_DOWN:
			return escort_menu_joystick_button_down(wind, event, menu);
#endif
			
		case EVENT_IDLE:
			if (menu->multiplayer_passthrough)
				return 0;
			timer_delay2(50);
			break;
			
		case EVENT_WINDOW_DRAW:
			show_escort_menu(menu);
			break;
			
		case EVENT_WINDOW_CLOSE:
		#ifdef __ANDROID__
			android_menu_scale_clear();
		#endif
			d_free(menu);
			return 0;	// continue closing
			break;
			
		default:
			return 0;
			break;
	}

	return 1;
}

void do_escort_menu(void)
{
	int	next_goal;
	escort_menu *menu;
	window *wind;

	if (Game_mode & GM_MULTI) {
#ifdef NETWORK
		if (!(Game_mode & GM_MULTI_COOP)) {
			HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot in Multiplayer!");
			return;
		}
		if (Escort_owner_player != Player_num) {
			HUD_init_message_literal(HM_DEFAULT, "Guide-Bot is controlled by another player");
			return;
		}
#else
		HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot in Multiplayer!");
		return;
#endif
	}

	if (!escort_refresh_buddy_objnum()) {
		HUD_init_message_literal(HM_DEFAULT, "No Guide-Bot present in mine!");

		#if 0	//ndef NDEBUG	// Just use HELPVISHNU!!
		//	If no buddy bot, create one!
		HUD_init_message(HM_DEFAULT, "Debug Version: Creating Guide-Bot!");
		create_buddy_bot();
		#else
		return;
		#endif
	}

	ok_for_buddy_to_talk();	//	Needed here or we might not know buddy can talk when he can.

	if (!Buddy_allowed_to_talk) {
		HUD_init_message(HM_DEFAULT, "%s has not been released",PlayerCfg.GuidebotName);
		return;
	}

	MALLOC(menu, escort_menu, 1);
	if (!menu)
		return;
	menu->selected_item = 0;
	menu->multiplayer_passthrough = (Game_mode & GM_MULTI) != 0;
	sprintf(menu->goal_str, "ERROR");
	sprintf(menu->message_action, "Suppress");
	
	//	This prevents the buddy from coming back if you've told him to scram.
	//	If we don't set next_goal, we get garbage there.
	if (Escort_special_goal == ESCORT_GOAL_SCRAM) {
		Escort_special_goal = -1;	//	Else setting next goal might fail.
		next_goal = escort_set_goal_object();
		Escort_special_goal = ESCORT_GOAL_SCRAM;
	} else {
		Escort_special_goal = -1;	//	Else setting next goal might fail.
		next_goal = escort_set_goal_object();
	}

	switch (next_goal) {
		case ESCORT_GOAL_UNSPECIFIED:
			Int3();
			break;
			
		case ESCORT_GOAL_BLUE_KEY:
			sprintf(menu->goal_str, "blue key");
			break;
		case ESCORT_GOAL_GOLD_KEY:
			sprintf(menu->goal_str, "yellow key");
			break;
		case ESCORT_GOAL_RED_KEY:
			sprintf(menu->goal_str, "red key");
			break;
		case ESCORT_GOAL_CONTROLCEN:
			sprintf(menu->goal_str, "reactor");
			break;
		case ESCORT_GOAL_BOSS:
			sprintf(menu->goal_str, "boss");
			break;
		case ESCORT_GOAL_EXIT:
#ifdef __ANDROID__
			if (Escort_route_goal.active) {
				snprintf(menu->goal_str, sizeof(menu->goal_str), "next: %s", escort_route_goal_label());
				break;
			}
#endif
			sprintf(menu->goal_str, "exit");
			break;
		case ESCORT_GOAL_MARKER1:
		case ESCORT_GOAL_MARKER2:
		case ESCORT_GOAL_MARKER3:
		case ESCORT_GOAL_MARKER4:
		case ESCORT_GOAL_MARKER5:
		case ESCORT_GOAL_MARKER6:
		case ESCORT_GOAL_MARKER7:
		case ESCORT_GOAL_MARKER8:
		case ESCORT_GOAL_MARKER9:
			sprintf(menu->goal_str, "marker %i", next_goal-ESCORT_GOAL_MARKER1+1);
			break;
		default:
			Int3();
			break;

	}
			
	if (!Buddy_messages_suppressed)
		sprintf(menu->message_action, "Suppress");
	else
		sprintf(menu->message_action, "Enable");

	// Just make it the full screen size and let show_escort_menu figure it out
	wind = window_create(&grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT, (int (*)(window *, d_event *, void *))escort_menu_handler, menu);
	if (!wind)
	{
		d_free(menu);
		return;
	}
	if (menu->multiplayer_passthrough)
		window_set_modal(wind, 0);
}

//	-------------------------------------------------------------------------------
//	Show the Buddy menu!
static void escort_menu_draw_contents(escort_menu *menu, const char *title,
	char rows[ESCORT_MENU_ITEM_COUNT][80], int x, int y, int content_w,
	int title_h)
{
	int i, item_y;

	gr_set_fontcolor(BM_XRGB(0, 28, 0), -1);
	gr_ustring(x, y, title);
	item_y = y + title_h + LINE_SPACING;
	for (i = 0; i < ESCORT_MENU_ITEM_COUNT; i++) {
		if (i == menu->selected_item) {
			gr_setcolor(BM_XRGB(0, 28, 0));
			gr_rect(x - FSPACX(2), item_y - FSPACY(1), x + content_w + FSPACX(2), item_y + LINE_SPACING - FSPACY(1));
			gr_set_fontcolor(BM_XRGB(0, 0, 0), -1);
		} else {
			gr_set_fontcolor(BM_XRGB(0, 28, 0), -1);
		}
		gr_ustring(x, item_y, rows[i]);
		item_y += LINE_SPACING;
	}
}

#ifdef __ANDROID__
static int escort_menu_draw_scaled(escort_menu *menu, const char *title,
	char rows[ESCORT_MENU_ITEM_COUNT][80], int source_x, int source_y,
	int box_w, int box_h, int content_w, int title_h)
{
	android_menu_scale_result menu_scale;
	grs_bitmap source_bitmap;
	grs_canvas source_canvas;
	grs_canvas *save_canvas = grd_curcanv;

	if (!android_menu_scale_compute_cropped(source_x, source_y, box_w, box_h,
		SWIDTH, SHEIGHT, BORDERX, BORDERY, &menu_scale))
		return 0;

	gr_init_bitmap_alloc(&source_bitmap, BM_LINEAR, 0, 0, box_w, box_h, box_w);
	gr_init_canvas(&source_canvas, source_bitmap.bm_data, BM_LINEAR, box_w, box_h);
	gr_set_current_canvas(&source_canvas);
	nm_draw_background(0, 0, box_w, box_h);
	escort_menu_draw_contents(menu, title, rows, BORDERX, BORDERY, content_w, title_h);
	gr_set_current_canvas(save_canvas);
	android_menu_scale_blit_bitmap(&source_bitmap, &menu_scale, 0);
	android_menu_scale_publish(&menu_scale);
	gr_free_bitmap_data(&source_bitmap);
	return 1;
}
#endif

static void show_escort_menu(escort_menu *menu)
{	
	char rows[ESCORT_MENU_ITEM_COUNT][80];
	const char *title = "Select Guide-Bot Command:";
	int i, w, h, aw, title_w, title_h, content_w, content_h;
	int x, y, box_w, box_h;


	gr_set_current_canvas(NULL);

	gr_set_curfont( GAME_FONT );

	gr_get_string_size(title, &title_w, &title_h, &aw);
	content_w = title_w;
	for (i = 0; i < ESCORT_MENU_ITEM_COUNT; i++) {
		escort_menu_item_text(menu, i, rows[i], sizeof(rows[i]));
		gr_get_string_size(rows[i], &w, &h, &aw);
		if (w > content_w)
			content_w = w;
	}
	content_h = title_h + LINE_SPACING * (ESCORT_MENU_ITEM_COUNT + 2);
	box_w = content_w + BORDERX * 2;
	box_h = content_h + BORDERY * 2;

	x = (SWIDTH - box_w) / 2 + BORDERX;
	y = (SHEIGHT - box_h) / 2 + BORDERY;

#ifdef __ANDROID__
	if (!escort_menu_draw_scaled(menu, title, rows, x - BORDERX, y - BORDERY,
		box_w, box_h, content_w, title_h))
		android_menu_scale_clear();
	else {
		reset_cockpit();
		return;
	}
#endif

	nm_draw_background(x - BORDERX, y - BORDERY, x + content_w + BORDERX, y + content_h + BORDERY);
	escort_menu_draw_contents(menu, title, rows, x, y, content_w, title_h);

	reset_cockpit();
}

#ifdef NETWORK
// android port: multiplayer coop guidebot support
enum {
	ESCORT_OWNER_PACKET_STATE = 0,
	ESCORT_OWNER_PACKET_REQUEST = 1
};

static int escort_route_target_mode_for_network(void)
{
#ifdef __ANDROID__
	return Escort_route_target_mode;
#else
	return Escort_network_target_mode;
#endif
}

static int escort_route_target_mode_valid(int target_mode)
{
	return target_mode == 0 || target_mode == 1;
}

static int escort_owner_candidate_eligible(int pnum)
{
	if (pnum < 0 || pnum >= N_players || pnum >= MAX_PLAYERS)
		return 0;
	return escort_owner_slot_eligible(
	    pnum,
	    N_players,
	    Players[pnum].connected == CONNECT_PLAYING,
	    Netgame.host_is_obs != 0);
}

static void escort_reset_navigation_for_owner(int new_owner)
{
	object *buddy_objp;
	ai_static *aip;
	ai_local *ailp;

	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
	Escort_special_goal = -1;
	Escort_goal_index = -1;
	escort_clear_secret_goal();
#ifdef __ANDROID__
	escort_route_clear_goal();
	escort_unexplored_route_target_clear(&Escort_unexplored_route_target);
	escort_route_note_replan("owner_handoff");
#endif
	if (!escort_refresh_buddy_objnum())
		return;

	buddy_objp = &Objects[Buddy_objnum];
	aip = &buddy_objp->ctype.ai_info;
	ailp = &Ai_local_info[Buddy_objnum];
	aip->hide_index = -1;
	aip->path_length = 0;
	aip->cur_path_index = 0;
	aip->PATH_DIR = 1;
	ailp->mode = AIM_GOTO_PLAYER;
	Buddy_last_seen_player = GameTime64;
	Buddy_last_player_path_created = GameTime64;
	Escort_last_path_created = GameTime64;

	if (new_owner == Player_num && ConsoleObject) {
		create_path_to_player(buddy_objp, Max_escort_length, 1);
		if (aip->path_length > 3)
			aip->path_length = polish_path(buddy_objp, &Point_segs[aip->hide_index], aip->path_length);
		ailp->mode = AIM_GOTO_PLAYER;
	}
}

static void escort_apply_multiplayer_owner(int new_owner, int target_mode)
{
	int old_owner = Escort_owner_player;
	int owner_changed = old_owner != new_owner;
#ifdef __ANDROID__
	int mode_changed;
#endif

	if (!escort_route_target_mode_valid(target_mode))
		target_mode = 0;
	Escort_network_target_mode = target_mode;
#ifdef __ANDROID__
	mode_changed = Escort_route_target_mode != target_mode;

	escort_route_set_target_mode(target_mode);
	if (mode_changed && !owner_changed && new_owner != Player_num) {
		escort_route_clear_goal();
		escort_unexplored_route_target_clear(&Escort_unexplored_route_target);
		escort_route_note_replan("owner_mode_sync");
	}
#else
	(void) target_mode;
#endif

	Escort_owner_player = new_owner;
	if (escort_refresh_buddy_objnum()) {
		if (new_owner >= 0)
			Buddy_allowed_to_talk = 1;
		multi_restore_companion_robot_control(Buddy_objnum, new_owner);
	}

	if (!owner_changed)
		return;
	escort_reset_navigation_for_owner(new_owner);
	ESCORT_DIAG("ownership applied: old=%d new=%d mode=%d", old_owner, new_owner, target_mode);
	if (new_owner == Player_num)
		HUD_init_message_literal(HM_DEFAULT, "Guide-Bot: you have control");
	else if (new_owner >= 0)
		HUD_init_message(HM_DEFAULT, "Guide-Bot: %s has control", Players[new_owner].callsign);
	else
		HUD_init_message_literal(HM_DEFAULT, "Guide-Bot has no owner");
}

static void escort_send_owner_packet(int packet_kind, int owner_pnum, int target_mode, unsigned int generation)
{
	multibuf[0] = MULTI_ESCORT_OWNER;
	multibuf[1] = (ubyte)Player_num;
	multibuf[2] = (ubyte)(sbyte)owner_pnum;
	multibuf[3] = (ubyte)target_mode;
	multibuf[4] = (ubyte)packet_kind;
	multibuf[5] = (ubyte)generation;
	multibuf[6] = (ubyte)(generation >> 8);
	multibuf[7] = (ubyte)(generation >> 16);
	multibuf[8] = (ubyte)(generation >> 24);
	multi_send_data(multibuf, 9, 2);
}

static unsigned int escort_next_owner_generation(void)
{
	Escort_owner_generation++;
	if (Escort_owner_generation == 0)
		Escort_owner_generation = 1;
	return Escort_owner_generation;
}

unsigned int escort_get_owner_generation(void)
{
	return Escort_owner_generation;
}

void multi_send_escort_owner(int owner_pnum)
{
	int target_mode = escort_route_target_mode_for_network();

	if (!(Game_mode & GM_MULTI_COOP))
		return;
	if (owner_pnum != -1 && !escort_owner_candidate_eligible(owner_pnum))
		return;
	if (multi_i_am_master()) {
		unsigned int generation = escort_next_owner_generation();
		escort_apply_multiplayer_owner(owner_pnum, target_mode);
		escort_send_owner_packet(ESCORT_OWNER_PACKET_STATE, owner_pnum, target_mode, generation);
	} else {
		escort_send_owner_packet(ESCORT_OWNER_PACKET_REQUEST, owner_pnum, target_mode, Escort_owner_generation);
	}
}

void multi_do_escort_owner(const ubyte *buf, int authenticated_sender)
{
	int claimed_sender = (int)buf[1];
	int sender;
	int new_owner = (int)(sbyte)buf[2];
	int target_mode = (int)buf[3];
	int packet_kind = (int)buf[4];
	unsigned int generation = (unsigned int)buf[5] |
	                          ((unsigned int)buf[6] << 8) |
	                          ((unsigned int)buf[7] << 16) |
	                          ((unsigned int)buf[8] << 24);

	if (!escort_owner_packet_sender_valid(claimed_sender, authenticated_sender)) {
		ESCORT_DIAG("ownership packet rejected: claimed_sender=%d authenticated_sender=%d",
		            claimed_sender,
		            authenticated_sender);
		return;
	}
	sender = authenticated_sender;
	if (sender < 0 || sender >= N_players || sender >= MAX_PLAYERS)
		return;
	if (new_owner != -1 && !escort_owner_candidate_eligible(new_owner))
		return;
	if (!escort_route_target_mode_valid(target_mode))
		return;

	if (packet_kind == ESCORT_OWNER_PACKET_REQUEST) {
		if (!multi_i_am_master())
			return;
		if (!escort_owner_request_allowed(Escort_owner_player,
		                                  sender,
		                                  new_owner,
		                                  escort_owner_candidate_eligible(sender),
		                                  new_owner == -1 || escort_owner_candidate_eligible(new_owner),
		                                  Escort_owner_generation,
		                                  generation))
			return;
		generation = escort_next_owner_generation();
		escort_apply_multiplayer_owner(new_owner, target_mode);
		escort_send_owner_packet(ESCORT_OWNER_PACKET_STATE, new_owner, target_mode, generation);
		return;
	}
	if (packet_kind != ESCORT_OWNER_PACKET_STATE || sender != multi_who_is_master())
		return;
	if (generation <= Escort_owner_generation)
		return;
	Escort_owner_generation = generation;
	escort_apply_multiplayer_owner(new_owner, target_mode);
}

void escort_transfer_ownership_on_disconnect(int gone_pnum)
{
	int new_owner = -1;
	int i;

	if (!multi_i_am_master() || Escort_owner_player != gone_pnum)
		return;

	// Pick lowest-numbered connected player as new owner
	for (i = 0; i < N_players; i++) {
		if (i == gone_pnum)
			continue;
		if (escort_owner_candidate_eligible(i)) {
			new_owner = i;
			break;
		}
	}

	ESCORT_DIAG("transfer_ownership: gone=%d new_owner=%d", gone_pnum, new_owner);
	multi_send_escort_owner(new_owner);
}

void escort_release_control(void)
{
	int offset, candidate;
	int new_owner = -1;

	if (Escort_owner_player != Player_num)
		return;
	if (!(Game_mode & GM_MULTI_COOP))
		return;

	for (offset = 1; offset < N_players; offset++) {
		candidate = (Player_num + offset) % N_players;
		if (escort_owner_candidate_eligible(candidate)) {
			new_owner = candidate;
			break;
		}
	}
	if (new_owner < 0)
		return;

	multi_send_escort_owner(new_owner);
	HUD_init_message_literal(HM_DEFAULT, "Guide-Bot control released");
}
#endif
