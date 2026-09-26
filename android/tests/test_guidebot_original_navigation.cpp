#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
extern "C" {
#include "inferno.h"
#include "ai.h"
#include "escort.h"
#include "guidebot_route_internal.h"
#include "guidebot_redux_reference.h"
#include "game.h"
#include "gameseg.h"
#include "gameseq.h"
#include "key.h"
#include "maths.h"
#include "robot.h"
#include "wall.h"
#include "cntrlcen.h"
#include "state.h"
#include "physics.h"
#include "physfsx.h"
#include "playsave.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"
#include "input_demo_start.h"
extern int escort_set_goal_object(void), find_exit_segment(void);
extern int time_to_visit_player(object *, ai_local *, ai_static *);
extern void ai_path_set_orient_and_vel(object *, vms_vector *, int, vms_vector *);
extern int Looking_for_marker, Buddy_messages_suppressed;
extern int Last_buddy_polish_path_tick, Escort_kill_object, Last_buddy_key;
extern fix64 Buddy_last_seen_player, Buddy_last_player_path_created, Last_buddy_message_time;
extern fix64 Last_come_back_message_time, Buddy_sorry_time;
}
using json = nlohmann::ordered_json;
static json Results;
static int Checks, Failures;
static void check(const std::string &name, bool ok)
{
	++Checks;
	if (!ok) {
		++Failures;
		Results["failures"].push_back(name);
		std::fprintf(stderr, "ORIGINAL FAIL %s\n", name.c_str());
	}
}
// Both branches start with identical routing globals, allocator, object and RNG
// Presentation clocks are restored but deliberately excluded from comparison
struct Snapshot {
	object bot;
	ai_local local;
	std::vector<point_seg> points;
	int count, goal, special, index, kill, marker, lastkey, polish, suppressed;
	fix64 seen, path, created, message, comeback, sorry;
	unsigned rng, calls;
	explicit Snapshot(object *b) : bot(*b), local(Ai_local_info[b - Objects]), points(Point_segs, Point_segs + MAX_POINT_SEGS)
	{
		count = int(Point_segs_free_ptr - Point_segs);
		goal = Escort_goal_object;
		special = Escort_special_goal;
		index = Escort_goal_index;
		kill = Escort_kill_object;
		marker = Looking_for_marker;
		lastkey = Last_buddy_key;
		polish = Last_buddy_polish_path_tick;
		suppressed = Buddy_messages_suppressed;
		seen = Buddy_last_seen_player;
		path = Buddy_last_player_path_created;
		created = Escort_last_path_created;
		message = Last_buddy_message_time;
		comeback = Last_come_back_message_time;
		sorry = Buddy_sorry_time;
		d_rand_get_state(&rng);
		calls = d_rand_get_call_count();
	}
	void restore(object *b) const
	{
		if (b->segnum != bot.segnum) obj_relink(b - Objects, bot.segnum);
		*b = bot;
		Ai_local_info[b - Objects] = local;
		std::copy(points.begin(), points.end(), Point_segs);
		Point_segs_free_ptr = Point_segs + count;
		Escort_goal_object = goal;
		Escort_special_goal = special;
		Escort_goal_index = index;
		Escort_kill_object = kill;
		Looking_for_marker = marker;
		Last_buddy_key = lastkey;
		Last_buddy_polish_path_tick = polish;
		Buddy_messages_suppressed = suppressed;
		Buddy_last_seen_player = seen;
		Buddy_last_player_path_created = path;
		Escort_last_path_created = created;
		Last_buddy_message_time = message;
		Last_come_back_message_time = comeback;
		Buddy_sorry_time = sorry;
		d_rand_set_state(rng);
		d_rand_set_call_count(calls);
	}
	bool same(const Snapshot &b) const
	{
		return !std::memcmp(&bot.ctype.ai_info, &b.bot.ctype.ai_info, sizeof(ai_static)) &&
		       !std::memcmp(&bot.mtype.phys_info, &b.bot.mtype.phys_info, sizeof(physics_info)) &&
		       !std::memcmp(&bot.orient, &b.bot.orient, sizeof(bot.orient)) &&
		       !std::memcmp(&bot.pos, &b.bot.pos, sizeof(bot.pos)) && bot.segnum == b.bot.segnum &&
		       !std::memcmp(&local, &b.local, sizeof(local)) && count == b.count &&
		       !std::memcmp(points.data(), b.points.data(), count * sizeof(point_seg)) &&
		       goal == b.goal && special == b.special && index == b.index && kill == b.kill && marker == b.marker &&
		       lastkey == b.lastkey && polish == b.polish && seen == b.seen && path == b.path && created == b.created &&
		       rng == b.rng && calls == b.calls;
	}
};
static void compare(object *bot, const std::string &name, const std::function<void(bool)> &f)
{
	Snapshot start(bot);
	f(true);
	Snapshot reference(bot);
	start.restore(bot);
	f(false);
	Snapshot actual(bot);
	check(name, actual.same(reference));
	if (!actual.same(reference)) {
		Results["differences"][name] = { { "actual", { actual.goal, actual.special, actual.index, actual.count, actual.bot.ctype.ai_info.path_length, actual.local.mode, actual.calls } },
			                             { "reference", { reference.goal, reference.special, reference.index, reference.count, reference.bot.ctype.ai_info.path_length, reference.local.mode, reference.calls } } };
	}
	start.restore(bot);
}
static void place(object *o, int seg)
{
	obj_relink(o - Objects, seg);
	compute_segment_center(&o->pos, &Segments[seg]);
}
int test_guidebot_live_navigation(const char *output, const char *)
{
	FrameTime = F1_0 / 60;
	GameTime64 = 20 * F1_0;
	d_tick_count = 300;
	guidebot_routing_set_mode(GUIDEBOT_ROUTING_ORIGINAL);
	escort_spawn_at_player();
	if (Buddy_objnum < 0 || Buddy_objnum > Highest_object_index) return 1;
	object *bot = &Objects[Buddy_objnum];
	Buddy_allowed_to_talk = 1;
	Buddy_messages_suppressed = 1;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (i != Buddy_objnum && Objects[i].type == OBJ_ROBOT) obj_delete(i);
	auto &a = bot->ctype.ai_info;
	auto &l = Ai_local_info[Buddy_objnum];
	place(bot, ConsoleObject->segnum);
	Believed_player_seg = ConsoleObject->segnum;
	Believed_player_pos = ConsoleObject->pos;
	const auto player_flags = Players[Player_num].flags;
	const auto object_flags = ConsoleObject->flags;
	const int destroyed = Control_center_destroyed;
	for (int flags = 0; flags < 8; ++flags)
		for (int dead = 0; dead < 2; ++dead) {
			Players[Player_num].flags = (flags << 1);
			Control_center_destroyed = dead;
			for (int object_keys = 0; object_keys < 8; ++object_keys) {
				ConsoleObject->flags = object_keys << 1;
				Escort_special_goal = -1;
				check("selector_" + std::to_string(flags) + "_" + std::to_string(dead) + "_" + std::to_string(object_keys), escort_set_goal_object() == redux_escort_set_goal_object());
			}
		}
	Players[Player_num].flags = player_flags;
	ConsoleObject->flags = object_flags;
	Control_center_destroyed = destroyed;
	check("exit", find_exit_segment() == redux_find_exit_segment());
	for (int mode : { AIM_GOTO_PLAYER, AIM_GOTO_OBJECT }) {
		l.mode = mode;
		for (int seg = 0; seg <= Highest_segment_index; ++seg)
			for (int side = 0; side < 6; ++side)
				check("door_" + std::to_string(mode) + "_" + std::to_string(seg) + "_" + std::to_string(side), ai_door_is_openable(bot, &Segments[seg], side) == redux_ai_door_is_openable(bot, &Segments[seg], side));
	}
	for (int seen : { 0, 4 * F1_0, 4 * F1_0 + 1, 8 * F1_0 })
		for (int recent : { 0, F1_0, F1_0 + 1 })
			for (int mode : { AIM_GOTO_PLAYER, AIM_GOTO_OBJECT })
				for (int cursor : { 0, 4, 5, 9 }) {
					Buddy_last_seen_player = GameTime64 - seen;
					Buddy_last_player_path_created = GameTime64 - recent;
					l.mode = mode;
					a.path_length = 10;
					a.cur_path_index = cursor;
					check("return_cadence", time_to_visit_player(bot, &l, &a) == redux_time_to_visit_player(bot, &l, &a));
				}
	ai_reset_all_paths();
	for (int seed = 1; seed <= 4; ++seed) {
		d_srand(seed);
		d_rand_reset_call_count();
		for (int target = 0; target <= Highest_segment_index; target += 17) {
			compare(bot, "path_" + std::to_string(seed) + "_" + std::to_string(target), [&](bool ref) {
				if (ref) redux_create_path_to_segment(bot, target, 40, 1);
				else create_path_to_segment(bot, target, 40, 1);
			});
		}
	}
	// Commands and automatic goals include missing/unreachable targets and scram
	const int keys[] = { KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9 };
	for (int key : keys) {
		compare(bot, "command_" + std::to_string(key), [&](bool ref) {if(ref) redux_set_escort_special_goal(key); else set_escort_special_goal(key); });
	}
	for (int goal = ESCORT_GOAL_BLUE_KEY; goal <= ESCORT_GOAL_SCRAM; ++goal) {
		if (goal == ESCORT_GOAL_BOSS) continue;
		Escort_special_goal = -1;
		Escort_goal_object = goal;
		Looking_for_marker = -1;
		compare(bot, "goal_path_" + std::to_string(goal), [&](bool ref) {if(ref) redux_escort_create_path_to_goal(bot); else escort_create_path_to_goal(bot); });
	}
	Escort_special_goal = -1;
	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
	for (int mode : { AIM_GOTO_PLAYER, AIM_GOTO_OBJECT, AIM_WANDER })
		for (int visible = 0; visible <= 2; ++visible)
			for (int age : { 0, 6 }) {
				ai_reset_all_paths();
				redux_create_n_segment_path(bot, 8, -1);
				l.mode = mode;
				a.cur_path_index = 0;
				Buddy_last_seen_player = GameTime64 - age * F1_0;
				Buddy_last_player_path_created = 0;
				Escort_last_path_created = 0;
				compare(bot, "frame_" + std::to_string(mode) + "_" + std::to_string(visible) + "_" + std::to_string(age), [&](bool ref) {if(ref) redux_do_escort_frame(bot,MIN_ESCORT_DISTANCE-1,visible); else do_escort_frame(bot,MIN_ESCORT_DISTANCE-1,visible); });
				compare(bot, "follow_" + std::to_string(mode) + "_" + std::to_string(visible) + "_" + std::to_string(age), [&](bool ref) {if(ref) redux_ai_follow_path(bot,visible,visible,nullptr); else ai_follow_path(bot,visible,visible,nullptr); });
			}
	vms_vector target = bot->pos;
	target.x += 30 * F1_0;
	target.y += 10 * F1_0;
	for (int fps : { 30, 60, 120 }) {
		FrameTime = F1_0 / fps;
		compare(bot, "steering_" + std::to_string(fps), [&](bool ref) {if(ref) redux_ai_path_set_orient_and_vel(bot,&target,2,nullptr); else ai_path_set_orient_and_vel(bot,&target,2,nullptr); });
	}
	check("original_has_no_planner_goal", !Escort_route_goal.active && escort_route_next_goal() == ESCORT_GOAL_UNSPECIFIED);
	// Exercise successive decisions on a moving bot using real level geometry
	FrameTime = F1_0 / 60;
	ai_reset_all_paths();
	Escort_special_goal = -1;
	Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
	l.mode = AIM_GOTO_PLAYER;
	Buddy_last_seen_player = GameTime64;
	for (int frame = 0; frame < 600; ++frame) {
		GameTime64 += FrameTime;
		if (frame % 3 == 0) ++d_tick_count;
		const fix distance = vm_vec_dist_quick(&bot->pos, &ConsoleObject->pos);
		compare(bot, "moving_frame_" + std::to_string(frame), [&](bool ref) {
			if (ref) {
				redux_do_escort_frame(bot, distance, 2);
				redux_ai_follow_path(bot, 2, 2, nullptr);
			} else {
				do_escort_frame(bot, distance, 2);
				ai_follow_path(bot, 2, 2, nullptr);
			}
		});
		do_escort_frame(bot, distance, 2);
		ai_follow_path(bot, 2, 2, nullptr);
		do_physics_sim(bot);
	}
	escort_find_secret_goal();
	escort_find_unexplored_goal();
	check("enhanced_only_commands_rejected", !Escort_route_goal.active);
	// Real native save/restore, with the default deliberately opposed to the save
	for (int mode : { GUIDEBOT_ROUTING_ORIGINAL, GUIDEBOT_ROUTING_ENHANCED }) {
		guidebot_routing_set_mode(mode);
		char name[] = "guidebot-mode.sav", desc[] = "Guidebot mode test";
		stop_time();
		check("save_mode_" + std::to_string(mode), state_save_all_sub(name, desc) != 0);
		guidebot_routing_set_default(1 - mode);
		guidebot_routing_set_mode(1 - mode);
		check("restore_mode_" + std::to_string(mode), state_restore_all_sub(name, 0) != 0 && guidebot_routing_mode() == mode);
	}
	// Replay both real startup paths with a default opposed to recorded mode
	const std::string replay_path = std::string(output) + ".dximdemo";
	for (int checkpoint : { 0, 1 })
		for (int mode : { GUIDEBOT_ROUTING_ORIGINAL, GUIDEBOT_ROUTING_ENHANCED }) {
			const std::string label = "replay_" + std::to_string(checkpoint) + "_" + std::to_string(mode);
			input_demo_recorder_settings settings;
			input_demo_recorder_settings_clear(&settings);
			settings.game = INPUT_DEMO_GAME_D2;
			settings.mission = "d2";
			settings.level = Current_level_num;
			settings.difficulty = Difficulty_level;
			settings.rng_mode = "lcg_state";
			settings.has_player_cfg = 1;
			settings.player_cfg.guidebot_routing_mode = mode;
			settings.player_cfg.primary_order_count = sizeof(PlayerCfg.PrimaryOrder);
			settings.player_cfg.secondary_order_count = sizeof(PlayerCfg.SecondaryOrder);
			std::memcpy(settings.player_cfg.primary_order, PlayerCfg.PrimaryOrder, sizeof(PlayerCfg.PrimaryOrder));
			std::memcpy(settings.player_cfg.secondary_order, PlayerCfg.SecondaryOrder, sizeof(PlayerCfg.SecondaryOrder));
			std::vector<uint8_t> checkpoint_bytes;
			if (checkpoint) {
				// Metadata governs replay even if checkpoint or launcher disagrees
				guidebot_routing_set_mode(1 - mode);
				char name[] = "guidebot-replay.sav", desc[] = "Guidebot replay test";
				stop_time();
				check(label + "_save", state_save_all_sub(name, desc) != 0);
				PHYSFS_file *saved = PHYSFS_openRead(name);
				if (!saved) {
					check(label + "_read", false);
					continue;
				}
				checkpoint_bytes.resize(static_cast<size_t>(PHYSFS_fileLength(saved)));
				PHYSFS_readBytes(saved, checkpoint_bytes.data(), checkpoint_bytes.size());
				PHYSFS_close(saved);
				settings.checkpoint_save_name = name;
				settings.checkpoint_data = checkpoint_bytes.data();
				settings.checkpoint_size = checkpoint_bytes.size();
				settings.has_checkpoint_start_gt = 1;
				settings.checkpoint_start_gt = GameTime64;
			}
			char error[512] = {};
			// checkpoint_save_name must outlive settings until recorder_start
			if (checkpoint) settings.checkpoint_save_name = "guidebot-replay.sav";
			bool ok = input_demo_recorder_start(&settings, error, sizeof(error)) != 0;
			input_demo_control_state controls = {};
			input_demo_control_pulse pulse = {};
			input_demo_result frame = {};
			if (ok) ok = input_demo_recorder_capture_frame(F1_0 / 60, &controls, &pulse, 1, 0, 0, &frame, nullptr, error, sizeof(error)) != 0;
			if (ok) ok = input_demo_recorder_flush(replay_path.c_str(), error, sizeof(error)) != 0;
			check(label + "_record", ok);
			if (!ok) {
				Results["replay_error"] = error;
				input_demo_recorder_cancel();
				continue;
			}
			guidebot_routing_set_default(1 - mode);
			guidebot_routing_set_mode(1 - mode);
			ok = input_demo_load_replay_from_path(replay_path.c_str(), error, sizeof(error)) != 0;
			if (ok) ok = input_demo_start_loaded_replay() == 0;
			check(label + "_start", ok && guidebot_routing_mode() == mode);
			if (!ok) Results["replay_error"] = error;
			input_demo_replay_unload();
		}
	Results["checks"] = Checks;
	Results["passed"] = Failures == 0;
	std::ofstream file(output);
	file << Results.dump(2) << '\n';
	std::fprintf(stderr, "ORIGINAL %d checks, %d failures\n", Checks, Failures);
	return file.good() && !Failures ? 0 : 1;
}
