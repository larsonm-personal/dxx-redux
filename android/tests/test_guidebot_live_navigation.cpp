#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <nlohmann/json.hpp>

extern "C" {
#include "inferno.h"
#include "ai.h"
#include "escort.h"
#include "game.h"
#include "gameseg.h"
#include "gameseq.h"
#include "guidebot_route_internal.h"
#include "key.h"
#include "maths.h"
#include "physics.h"
#include "powerup.h"
#include "robot.h"
#include "secretarea.h"
#include "switch.h"
#include "wall.h"
extern void escort_create_path_to_goal(object *objp);
extern fix64 Buddy_last_seen_player, Buddy_last_player_path_created;
extern int time_to_visit_player(object *objp, ai_local *ailp, ai_static *aip);
extern int Buddy_messages_suppressed;
extern void GameProcessFrame(void);
}

static nlohmann::ordered_json Results;
static bool Passed = true;

static void check(const char *name, bool passed)
{
	Results[name] = passed;
	Passed = Passed && passed;
	std::fprintf(stderr, "ESCORT-TEST %s %s\n", passed ? "PASS" : "FAIL", name);
}

static void place(object *objp, int segment)
{
	compute_segment_center(&objp->pos, &Segments[segment]);
	obj_relink(objp - Objects, segment);
	vm_vec_zero(&objp->mtype.phys_info.velocity);
	vm_vec_zero(&objp->mtype.phys_info.thrust);
	vm_vec_zero(&objp->mtype.phys_info.rotvel);
}

// Exercise actual path-end control flow with a retained strategic goal
static void endpoint_isolation(object *bot)
{
	const int id = bot->id;
	const object saved = *bot;
	const ai_local saved_local = Ai_local_info[bot - Objects];
	for (int companion = 0; companion <= 1; ++companion) {
		const int old_companion = Robot_info[id].companion;
		Robot_info[id].companion = companion;
		object baseline = {};
		ai_local baseline_local = {};
		for (int active = 0; active <= 1; ++active) {
			*bot = saved;
			Ai_local_info[bot - Objects] = saved_local;
			auto &ai = bot->ctype.ai_info;
			auto &local = Ai_local_info[bot - Objects];
			local.mode = AIM_GOTO_PLAYER;
			ai.behavior = AIB_NORMAL;
			ai.hide_index = 0;
			ai.path_length = 3;
			ai.cur_path_index = 2;
			ai.PATH_DIR = 1;
			for (int i = 0; i < 3; ++i) {
				Point_segs[i].segnum = bot->segnum;
				Point_segs[i].point = bot->pos;
				Point_segs[i].point.x += (i - 2) * 10 * F1_0;
			}
			Point_segs_free_ptr = Point_segs + 3;
			Escort_route_goal.active = active;
			d_srand(1);
			ai_follow_path(bot, 2, 2, nullptr);
			if (!active) {
				baseline = *bot;
				baseline_local = local;
			} else {
				check(companion ? "return_endpoint_matches_legacy" : "other_robot_endpoint_matches_legacy",
				      !std::memcmp(&baseline.ctype.ai_info, &ai, sizeof(ai)) &&
				          !std::memcmp(&baseline.mtype.phys_info, &bot->mtype.phys_info, sizeof(bot->mtype.phys_info)) &&
				          baseline_local.mode == local.mode);
			}
		}
		Robot_info[id].companion = old_companion;
	}
	*bot = saved;
	Ai_local_info[bot - Objects] = saved_local;
	ai_reset_all_paths();
}

static void hostage_request(object *bot)
{
	set_escort_special_goal(KEY_6);
	Escort_goal_object = ESCORT_GOAL_HOSTAGE;
	escort_create_path_to_goal(bot);
	int hostages = 0;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_HOSTAGE) ++hostages;
	check("hostages_command_retained", hostages && Escort_special_goal == ESCORT_GOAL_HOSTAGE);
	check("hostages_has_physical_path", bot->ctype.ai_info.path_length > 0);
	Results["hostage_guidance"] = escort_get_route_goal_instruction();
	Results["hostage_objective_kind"] = Escort_route_goal.objective_kind;
	Results["hostage_objective_segment"] = Escort_route_goal.objective_seg;
	Results["hostage_path_endpoint"] = Point_segs[bot->ctype.ai_info.hide_index + bot->ctype.ai_info.path_length - 1].segnum;
	check("hostages_selects_prerequisite", Escort_route_goal.active && Escort_route_goal.objective_trigger >= 0);
	int saved_trigger_flags[MAX_TRIGGERS];
	for (int i = 0; i < Num_triggers; ++i) {
		saved_trigger_flags[i] = Triggers[i].flags;
		Triggers[i].flags |= TF_DISABLED;
	}
	escort_route_note_replan("test_blocked_hostage_access");
	escort_create_path_to_goal(bot);
	check("hostages_without_prerequisite_keeps_frontier", Escort_special_goal == ESCORT_GOAL_HOSTAGE &&
	    Escort_route_goal.active && Escort_route_goal.guidance_mode == ESCORT_ROUTE_GUIDANCE_NEAREST_PROGRESS_POINT &&
	    bot->ctype.ai_info.path_length > 0);
	for (int i = 0; i < Num_triggers; ++i)
		Triggers[i].flags = saved_trigger_flags[i];
	escort_route_note_replan("test_restore_hostage_access");
	escort_create_path_to_goal(bot);
	for (int i = 0; i <= Highest_object_index; ++i) {
		if (Objects[i].type == OBJ_POWERUP) {
			const int saved_id = Objects[i].id;
			Objects[i].id = POW_KEY_RED;
			detect_escort_goal_accomplished(i);
			Objects[i].id = saved_id;
			check("prerequisite_pickup_keeps_hostages_request", Escort_special_goal == ESCORT_GOAL_HOSTAGE);
			break;
		}
	}
	for (int step = 0; step < 12 && Escort_route_goal.active; ++step) {
		const int trigger = Escort_route_goal.objective_trigger;
		if (trigger < 0 || trigger >= Num_triggers) break;
		Results["hostage_prerequisite_triggers"].push_back(trigger);
		// Script the player's switch action; normal wall animation completes it
		check_trigger_sub(trigger, Player_num, 1);
		for (int frame = 0; frame < 120; ++frame) {
			GameTime64 += FrameTime;
			wall_frame_process();
			triggers_frame_process();
		}
		GameTime64 += 5 * F1_0;
		escort_create_path_to_goal(bot);
	}
	check("hostages_resume_ordinary_guidance_after_access", !Escort_route_goal.active &&
	    Escort_special_goal == ESCORT_GOAL_HOSTAGE && Escort_goal_index >= 0 &&
	    Objects[Escort_goal_index].type == OBJ_HOSTAGE);
	const int target = Escort_goal_index;
	if (target >= 0 && target <= Highest_object_index && Objects[target].type == OBJ_HOSTAGE) {
		detect_escort_goal_accomplished(target);
		check("hostage_rescue_completes_request", Escort_special_goal == -1);
	}
	// With no remaining hostages the ordinary no-object response still applies
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_HOSTAGE) Objects[i].type = OBJ_NONE;
	set_escort_special_goal(KEY_0);
	set_escort_special_goal(KEY_6);
	Escort_goal_object = ESCORT_GOAL_HOSTAGE;
	escort_create_path_to_goal(bot);
	check("no_hostages_clears_request", Escort_special_goal == -1 && !Escort_route_goal.active);
}

static void triggered_grate(object *bot)
{
	for (int w = 0; w < Num_walls; ++w) {
		wall &barrier = Walls[w];
		if (barrier.type != WALL_CLOSED || barrier.segnum < 0 || barrier.sidenum < 0 ||
		    Segments[barrier.segnum].children[barrier.sidenum] < 0)
			continue;
		const wall saved = barrier;
		barrier.controlling_trigger = 0;
		barrier.clip_num = -1;
		barrier.keys = KEY_NONE;
		barrier.flags &= ~WALL_BUDDY_PROOF;
		Ai_local_info[bot - Objects].mode = AIM_GOTO_OBJECT;
		check("trigger_does_not_make_grate_openable", !ai_door_is_openable(bot, &Segments[barrier.segnum], barrier.sidenum));
		barrier = saved;
		return;
	}
	check("triggered_grate_fixture_exists", false);
}

static void grate_detour(object *bot, int moving_target)
{
	int start = -1, goal = -1;
	for (int w = 0; w < Num_walls; ++w) {
		const auto &wall = Walls[w];
		if (wall.type != WALL_CLOSED || wall.segnum < 0 || wall.sidenum < 0)
			continue;
		const int child = Segments[wall.segnum].children[wall.sidenum];
		if (child < 0 || !(WALL_IS_DOORWAY(&Segments[wall.segnum], wall.sidenum) & WID_RENDPAST_FLAG))
			continue;
		place(bot, wall.segnum);
		Ai_local_info[bot - Objects].mode = AIM_GOTO_PLAYER;
		ai_reset_all_paths();
		create_path_to_segment(bot, child, 100, 1);
		const auto &ai = bot->ctype.ai_info;
		if (ai.path_length > 3 && ai.path_length < 90 &&
		    Point_segs[ai.hide_index + ai.path_length - 1].segnum == child) {
			start = wall.segnum;
			goal = child;
			break;
		}
	}
	check("found_grate_with_detour", start >= 0);
	if (start < 0) return;
	Results["grate_start"] = start;
	Results["grate_destination"] = goal;
	place(ConsoleObject, goal);
	place(bot, start);
	Escort_route_goal.active = 1;
	Escort_route_goal.target_seg = goal;
	Escort_special_goal = -1;
	Escort_goal_object = ESCORT_GOAL_EXIT;
	Buddy_last_seen_player = GameTime64 - 5 * F1_0;
	Buddy_last_player_path_created = GameTime64 - 2 * F1_0;
	Ai_local_info[bot - Objects].mode = AIM_GOTO_PLAYER;
	bool moved_away = false;
	const fix initial_distance = vm_vec_dist(&bot->pos, &ConsoleObject->pos);
	auto rejoined = [&]() {
		return vm_vec_dist(&bot->pos, &ConsoleObject->pos) < MIN_ESCORT_DISTANCE &&
		       guidebot_route_waypoint_leg_clear(bot, &bot->pos, bot->segnum, &ConsoleObject->pos);
	};
	int frames = 0;
	for (; frames < 60 * 45 && !rejoined(); ++frames) {
		FrameTime = F1_0 / 60;
		GameTime64 += FrameTime;
		// The player remains independent; exercise the normal escort decision
		// and follower, then let native physics decide all segment crossings
		Believed_player_pos = ConsoleObject->pos;
		Believed_player_seg = ConsoleObject->segnum;
		const fix distance = vm_vec_dist(&bot->pos, &ConsoleObject->pos);
		moved_away = moved_away || distance > initial_distance + F1_0;
		GameProcessFrame();
		if (frames % 3 == 0) ++d_tick_count;
	}
	Results["grate_frames"] = frames;
	Results["grate_final_segment"] = bot->segnum;
	Results["grate_final_distance"] = vm_vec_dist(&bot->pos, &ConsoleObject->pos) / F1_0;
	Results["grate_final_clear"] = guidebot_route_waypoint_leg_clear(bot, &bot->pos, bot->segnum, &ConsoleObject->pos);
	Results["grate_final_mode"] = Ai_local_info[bot - Objects].mode;
	check("return_routes_around_grate", rejoined());
	check("detour_can_move_away_from_player", moved_away);
	check("moving_player_target_valid", moving_target >= 0 && moving_target <= Highest_segment_index);
	if (moving_target < 0 || moving_target > Highest_segment_index) return;
	place(ConsoleObject, moving_target);
	for (frames = 0; frames < 60 * 120 && !rejoined(); ++frames) {
		GameTime64 += FrameTime;
		Believed_player_pos = ConsoleObject->pos;
		Believed_player_seg = ConsoleObject->segnum;
		GameProcessFrame();
		if (frames % 3 == 0) ++d_tick_count;
	}
	Results["moving_player_return_frames"] = frames;
	Results["moving_return_segment"] = bot->segnum;
	Results["moving_return_mode"] = Ai_local_info[bot - Objects].mode;
	Results["moving_return_distance"] = vm_vec_dist(&bot->pos, &ConsoleObject->pos) / F1_0;
	Results["moving_return_clear"] = guidebot_route_waypoint_leg_clear(bot, &bot->pos, bot->segnum, &ConsoleObject->pos);
	Results["moving_return_path_index"] = bot->ctype.ai_info.cur_path_index;
	Results["moving_return_path_length"] = bot->ctype.ai_info.path_length;
	Results["moving_return_since_seen"] = (GameTime64 - Buddy_last_seen_player) / F1_0;
	check("return_repaths_to_moving_player", rejoined());
}

int test_guidebot_live_navigation(const char *output, const char *return_target)
{
	FrameTime = F1_0 / 60;
	GameTime64 = 20 * F1_0;
	Buddy_messages_suppressed = 1;
	escort_spawn_at_player();
	if (Buddy_objnum < 0 || Buddy_objnum > Highest_object_index) return 1;
	object *bot = &Objects[Buddy_objnum];
	place(bot, ConsoleObject->segnum);
	// Isolate navigation from combat and stationary unprocessed robot bodies
	for (int i = 0; i <= Highest_object_index; ++i)
		if (i != Buddy_objnum && Objects[i].type == OBJ_ROBOT) obj_delete(i);
	Buddy_allowed_to_talk = 1;
	endpoint_isolation(bot);
	triggered_grate(bot);
	if (Current_level_num == 11)
		grate_detour(bot, return_target ? std::atoi(return_target) : 35);
	else
		hostage_request(bot);
	Results["passed"] = Passed;
	std::ofstream file(output);
	file << Results.dump(2) << '\n';
	return file.good() && Passed ? 0 : 1;
}
