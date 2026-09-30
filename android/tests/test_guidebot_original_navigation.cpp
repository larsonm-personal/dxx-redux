#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
extern "C" {
#include "inferno.h"
#include "d1_in_d2/d1_in_d2.h"
#include "ai.h"
#include "escort.h"
#include "guidebot_route_internal.h"
#include "guidebot_redux_reference.h"
#include "game.h"
#include "gameseg.h"
#include "gameseq.h"
#include "key.h"
#include "maths.h"
#include "multi.h"
#include "mission.h"
#include "laser.h"
#include "fvi.h"
#include "robot.h"
#include "wall.h"
#include "cntrlcen.h"
#include "state.h"
#include "guidebot_save_io.h"
#include "physics.h"
#include "physfsx.h"
#include "playsave.h"
#include "powerup.h"
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
		Results["differences"][name] = { { "actual", { actual.goal, actual.special, actual.index, actual.count, +actual.bot.ctype.ai_info.path_length, actual.local.mode, actual.calls } },
			                             { "reference", { reference.goal, reference.special, reference.index, reference.count, +reference.bot.ctype.ai_info.path_length, reference.local.mode, reference.calls } } };
	}
	start.restore(bot);
}
static void place(object *o, int seg)
{
	obj_relink(o - Objects, seg);
	compute_segment_center(&o->pos, &Segments[seg]);
}
// Opt-in diagnostic: ordinary save/load, with no demo checkpoint metadata
// This exercises live navigation and physics, not the entire game frame loop
static json continuity_state()
{
	const object &bot = Objects[Buddy_objnum];
	// The legacy HUD message contains palette bytes, not UTF-8 text
	const char *message = escort_goal_message() ? escort_goal_message() : "";
	const auto *message_bytes = reinterpret_cast<const unsigned char *>(message);
	unsigned rng;
	d_rand_get_state(&rng);
	return {
		{ "routing_mode", guidebot_routing_mode() },
		{ "goal", Escort_goal_object }, { "special_goal", Escort_special_goal }, { "goal_index", Escort_goal_index },
		{ "marker", Looking_for_marker }, { "last_key", Last_buddy_key },
		{ "released", Buddy_allowed_to_talk }, { "messages_suppressed", Buddy_messages_suppressed },
		{ "goal_message_bytes", std::vector<unsigned char>(message_bytes, message_bytes + std::strlen(message)) },
		{ "seen_delta", Buddy_last_seen_player - GameTime64 },
		{ "player_path_delta", Buddy_last_player_path_created - GameTime64 },
		{ "path_created_delta", Escort_last_path_created - GameTime64 },
		{ "route_target_mode", Escort_route_target_mode }, { "route_active", Escort_route_goal.active },
		{ "route_goal", { Escort_route_goal.target_seg, Escort_route_goal.objective_kind, Escort_route_goal.objective_seg,
		                  Escort_route_goal.objective_trigger, Escort_route_goal.objective_object, Escort_route_goal.guidance_mode } },
		{ "secret_goal", { escort_get_secret_goal_seg(), escort_get_secret_goal_side() } },
		{ "comeback_delta", Last_come_back_message_time - GameTime64 },
		{ "sorry_delta", Buddy_sorry_time - GameTime64 },
		{ "ai_mode", Ai_local_info[Buddy_objnum].mode },
		{ "path_length", bot.ctype.ai_info.path_length }, { "path_index", bot.ctype.ai_info.cur_path_index },
		{ "path_allocator", Point_segs_free_ptr - Point_segs },
		{ "segment", bot.segnum }, { "position", { bot.pos.x, bot.pos.y, bot.pos.z } },
		{ "velocity", { bot.mtype.phys_info.velocity.x, bot.mtype.phys_info.velocity.y, bot.mtype.phys_info.velocity.z } },
		{ "rng", rng }, { "rng_calls", d_rand_get_call_count() }
	};
}
static void continuity_step(int frame)
{
	FrameTime = F1_0 / 60;
	GameTime64 += FrameTime;
	if (frame % 3 == 0) ++d_tick_count;
	Believed_player_seg = ConsoleObject->segnum;
	Believed_player_pos = ConsoleObject->pos;
	escort_goal_message_frame();
	escort_route_monitor_completion();
	object *bot = &Objects[Buddy_objnum];
	do_escort_frame(bot, vm_vec_dist_quick(&bot->pos, &ConsoleObject->pos), 2);
	ai_follow_path(bot, 2, 2, nullptr);
	do_physics_sim(bot);
}
static bool continuity_equal(json left, json right)
{
	// Ordinary loads reset this diagnostic counter; compare actual RNG state
	left.erase("rng_calls");
	right.erase("rng_calls");
	return left == right;
}
static bool continuity_payload_validation()
{

	const json before = continuity_state();
	PHYSFS_file *file = PHYSFS_openWrite("guidebot-runtime.bin");
	if (!file) return false;
	guidebot_save_stream stream = { file, 1, 0, 1, GameTime64, 0 };
	const bool written = escort_save_runtime(&stream) && (PHYSFS_sint64)stream.bytes == PHYSFS_tell(file);

	PHYSFS_close(file);
	if (!written) return false;
	file = PHYSFS_openRead("guidebot-runtime.bin");
	if (!file) return false;

	std::vector<unsigned char> bytes((size_t)PHYSFS_fileLength(file));
	const bool read = PHYSFS_readBytes(file, bytes.data(), bytes.size()) == (PHYSFS_sint64)bytes.size();
	PHYSFS_close(file);
	if (!read || bytes.empty()) return false;
	for (int truncated : { 0, 1 }) {

		file = PHYSFS_openWrite("guidebot-runtime-probe.bin");
		if (!file) return false;
		const size_t size = bytes.size() - truncated;
		const bool copied = PHYSFS_writeBytes(file, bytes.data(), size) == (PHYSFS_sint64)size;
		PHYSFS_close(file);
		if (!copied) return false;
		file = PHYSFS_openRead("guidebot-runtime-probe.bin");
		if (!file) return false;
		stream = { file, 0, 0, 1, GameTime64, 0 };
		const bool valid = escort_save_runtime(&stream) != 0;

		PHYSFS_close(file);
		if (valid == (truncated != 0) || !continuity_equal(before, continuity_state())) return false;
	}

	return true;
}
static int audit_save_continuity(const char *output)
{
	escort_set_goal_message_persistent(1);
	json report;
	bool passed = true;
	for (int mode : { GUIDEBOT_ROUTING_ORIGINAL, GUIDEBOT_ROUTING_ENHANCED }) {
		for (int command : { KEY_7, KEY_0, KEY_9, KEY_6, -1 }) {
			if (command == -1 && mode == GUIDEBOT_ROUTING_ORIGINAL) continue;
			std::fprintf(stderr, "CONTINUITY mode=%d command=%d start\n", mode, command);
			guidebot_routing_set_default(mode);
			// Level restoration rereads the pilot configuration on desktop
			if (write_player_file() != 0) return 2;
			guidebot_routing_set_mode(mode);
			GameTime64 = 20 * F1_0;
			place(&Objects[Buddy_objnum], ConsoleObject->segnum);
			Buddy_allowed_to_talk = 1;
			Buddy_messages_suppressed = 1;
			escort_reset_routing();
			// Scram, Next, Exit and Hostages exercise classic and Enhanced guidance
			if (command == -1) escort_find_unexplored_goal();
			else set_escort_special_goal(command);
			for (int frame = 0; frame < 120; ++frame) continuity_step(frame);
			json result;

			result["before_save"] = continuity_state();
			char name[] = "guidebot-continuity.sav", desc[] = "Guidebot continuity audit";
			stop_time();
			if (!state_save_all_sub(name, desc)) return 2;
			result["after_save"] = continuity_state();
			std::vector<json> reference;
			for (int frame = 0; frame < 120; ++frame) {
				continuity_step(frame);
				reference.push_back(continuity_state());
			}

			if (!state_restore_all_sub(name, 0)) return 2;
			result["after_load"] = continuity_state();
			result["save_unchanged"] = continuity_equal(result["before_save"], result["after_save"]);
			result["restore_unchanged"] = continuity_equal(result["after_save"], result["after_load"]);
			result["first_divergent_frame"] = nullptr;
			for (int frame = 0; frame < 120; ++frame) {
				continuity_step(frame);
				const json actual = continuity_state();
				if (!continuity_equal(actual, reference[frame]) && result["first_divergent_frame"].is_null()) {
					result["first_divergent_frame"] = frame + 1;
					result["uninterrupted"] = reference[frame];
					result["resumed"] = actual;
				}
				if (frame == 119) {
					result["uninterrupted_final"] = reference[frame];
					result["resumed_final"] = actual;
				}
			}

			result["passed"] = result["save_unchanged"].get<bool>() && result["restore_unchanged"].get<bool>() && result["first_divergent_frame"].is_null();
			passed = passed && result["passed"].get<bool>();
			const char *scenario = command == KEY_7 ? "scram" : command == KEY_0 ? "next" : command == KEY_9 ? "exit" : command == KEY_6 ? "hostages" : "unexplored";
			report[mode == GUIDEBOT_ROUTING_ORIGINAL ? "Original" : "Enhanced"][scenario] = result;
		}
	}

	report["payload_validation"] = continuity_payload_validation();

	passed = passed && report["payload_validation"].get<bool>();
	report["passed"] = passed;
	std::ofstream file(output);
	try { file << report.dump(2) << '\n'; }
	catch (const std::exception &error) { std::fprintf(stderr, "CONTINUITY report: %s\n", error.what()); return 2; }
	return file.good() && passed ? 0 : 1;
}
static void audit_redux_return_events(object *bot)
{
	Snapshot start(bot);
	const int destroyed = Control_center_destroyed;
	const auto flags = Players[Player_num].flags;
	const auto object_flags = ConsoleObject->flags;
	Players[Player_num].flags = PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_GOLD_KEY | PLAYER_FLAGS_RED_KEY;
	ConsoleObject->flags = Players[Player_num].flags;
	// A distant owner remains periodically visible while the reactor countdown runs
	for (int visible : { 0, 1, 2 })
		for (int age : { 0, 3, 5, 16 }) {
			start.restore(bot);
			redux_create_n_segment_path(bot, 4, -1);
			Control_center_destroyed = 1;
			Escort_special_goal = -1;
			Escort_goal_object = ESCORT_GOAL_CONTROLCEN;
			Ai_local_info[Buddy_objnum].mode = AIM_GOTO_PLAYER;
			Buddy_last_seen_player = GameTime64 - 3 * F1_0;
			Escort_last_path_created = GameTime64 - age * F1_0;
			compare(bot, "reactor_return_" + std::to_string(visible) + "_" + std::to_string(age), [&](bool ref) {
				if (ref) redux_do_escort_frame(bot, MIN_ESCORT_DISTANCE + F1_0, visible);
				else do_escort_frame(bot, MIN_ESCORT_DISTANCE + F1_0, visible);
			});
			// Redux can retain the old return path beyond 16 seconds when sight is refreshed
			redux_do_escort_frame(bot, MIN_ESCORT_DISTANCE + F1_0, visible);
			check("redux_waits_for_rejoin", Ai_local_info[Buddy_objnum].mode == AIM_GOTO_PLAYER &&
			      Escort_goal_object != ESCORT_GOAL_EXIT);
			place(bot, find_exit_segment());
			compare(bot, "reactor_rejoin_" + std::to_string(visible) + "_" + std::to_string(age), [&](bool ref) {
				if (ref) redux_do_escort_frame(bot, MIN_ESCORT_DISTANCE - 1, 2);
				else do_escort_frame(bot, MIN_ESCORT_DISTANCE - 1, 2);
			});
		}
	const int persistent = escort_goal_message_persistent();
	Escort_special_goal = ESCORT_GOAL_SHIELD;
	Escort_goal_index = -1;
	detect_escort_goal_accomplished(-4);
	check("fuel_sentinel_does_not_index_objects", Escort_special_goal == ESCORT_GOAL_SHIELD);
	escort_set_goal_message_persistent(1);
	Ai_local_info[Buddy_objnum].mode = AIM_GOTO_PLAYER;
	Escort_special_goal = ESCORT_GOAL_SCRAM;
	Escort_last_path_created = GameTime64;
	buddy_goal_message("Staying away...");
	do_escort_frame(bot, MIN_ESCORT_DISTANCE + F1_0, 2);
	check("classic_scram_status_preserved", escort_goal_message() && std::strstr(escort_goal_message(), "Staying away"));
	Escort_special_goal = -1;
	do_escort_frame(bot, MIN_ESCORT_DISTANCE + F1_0, 2);
	check("classic_silent_return_status", escort_goal_message() && std::strstr(escort_goal_message(), "Coming back"));
	escort_set_goal_message_persistent(persistent);
	start.restore(bot);
	Control_center_destroyed = destroyed;
	Players[Player_num].flags = flags;
	ConsoleObject->flags = object_flags;
	// A valid byte-sized path must retain its beginning for midpoint and patrol rules
	ai_reset_all_paths();
	auto &a = bot->ctype.ai_info;
	a.hide_index = 0;
	a.path_length = 124;
	a.cur_path_index = 120;
	a.PATH_DIR = 1;
	Ai_local_info[Buddy_objnum].mode = AIM_GOTO_PLAYER;
	for (int i = 0; i < a.path_length; ++i) {
		Point_segs[i].segnum = bot->segnum;
		Point_segs[i].point = bot->pos;
		if (i != 120) Point_segs[i].point.x += 50 * F1_0;
	}
	Point_segs_free_ptr = Point_segs + a.path_length;
	compare(bot, "classic_long_return_prefix", [&](bool ref) {
		if (ref) redux_ai_follow_path(bot, 2, 2, nullptr);
		else ai_follow_path(bot, 2, 2, nullptr);
	});
	start.restore(bot);
}
static void audit_stale_player_return(object *bot)
{
	if (!d1_in_d2_use_d1_gameplay()) return;
	Snapshot start(bot);
	const object player = *ConsoleObject;
	const int believed_seg = Believed_player_seg;
	const auto flags = Players[Player_num].flags;
	// Keep the requested center within Classic's finite path search depth
	for (int seg = 0; seg <= Highest_segment_index; ++seg)
		if (Segment2s[seg].special == SEGMENT_IS_FUELCEN) {
			place(ConsoleObject, seg);
			break;
		}
	int stale_seg = -1;
	for (int side = 0; side < MAX_SIDES_PER_SEGMENT; ++side)
		if (WALL_IS_DOORWAY(&Segments[ConsoleObject->segnum], side) & WID_FLY_FLAG) {
			stale_seg = Segments[ConsoleObject->segnum].children[side];
			if (stale_seg >= 0) break;
		}
	check("stale_return_fixture", stale_seg >= 0);
	for (int cloaked = 0; stale_seg >= 0 && cloaked <= 1; ++cloaked) {
		start.restore(bot);
		place(bot, stale_seg);
		Believed_player_seg = stale_seg;
		Players[Player_num].flags = (flags & ~PLAYER_FLAGS_CLOAKED) | (cloaked ? PLAYER_FLAGS_CLOAKED : 0);
		ai_reset_all_paths();
		create_path_to_player(bot, Max_escort_length, 1);
		const int target = cloaked ? stale_seg : ConsoleObject->segnum;
		const auto &path = bot->ctype.ai_info;
		check("stale_return_target_" + std::to_string(cloaked), Ai_local_info[Buddy_objnum].goal_segment == target);
		check("stale_return_endpoint_" + std::to_string(cloaked), path.path_length > 0 &&
		      Point_segs[path.hide_index + path.path_length - 1].segnum == target);
		check("stale_return_preserves_enemy_memory_" + std::to_string(cloaked), Believed_player_seg == stale_seg);
		if (!cloaked) {
			Looking_for_marker = Last_buddy_key = -1;
			set_escort_special_goal(KEY_2);
			check("stale_return_accepts_energy_center", Escort_special_goal == ESCORT_GOAL_ENERGYCEN);
			place(bot, target);
			Ai_local_info[Buddy_objnum].mode = AIM_GOTO_PLAYER;
			Buddy_last_seen_player = Buddy_last_player_path_created = GameTime64;
			do_escort_frame(bot, 0, 2);
			check("stale_return_resumes_energy_center", Escort_goal_object == ESCORT_GOAL_ENERGYCEN &&
			      Escort_goal_index >= 0 && Ai_local_info[Buddy_objnum].mode == AIM_GOTO_OBJECT);
		}
	}
	start.restore(bot);
	obj_relink(ConsoleObject - Objects, player.segnum);
	*ConsoleObject = player;
	Believed_player_seg = believed_seg;
	Players[Player_num].flags = flags;
}

static void audit_reactor_room_return(object *bot)
{
	if (std::strcmp(Current_mission_filename, "descent") || Current_level_num != 2) return;
	Snapshot start(bot);
	const object player = *ConsoleObject;
	const auto flags = Players[Player_num].flags;
	const int destroyed = Control_center_destroyed, game_mode = Game_mode, owner = Escort_owner_player;
	const fix64 time = GameTime64;
	const int tick = d_tick_count;
	const fix dt = FrameTime;
	const auto believed_pos = Believed_player_pos;
	const int believed_seg = Believed_player_seg;
	for (int scenario : { 0, 1, 2, 3 }) {
		const int coop = scenario & 1, early_retreat = scenario >> 1;
		start.restore(bot);
		Game_mode = coop ? GM_NETWORK | GM_MULTI_COOP | GM_MULTI_ROBOTS : 0;
		Escort_owner_player = Player_num;
		Players[Player_num].flags = PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_GOLD_KEY | PLAYER_FLAGS_RED_KEY;
		ConsoleObject->flags = Players[Player_num].flags;
		place(bot, 355);
		place(ConsoleObject, 359);
		Believed_player_pos = ConsoleObject->pos;
		Believed_player_seg = ConsoleObject->segnum;
		GameTime64 = time;
		FrameTime = F1_0 / 60;
		ai_reset_all_paths();
		redux_create_path_to_player(bot, Max_escort_length, 1);
		Ai_local_info[Buddy_objnum].mode = AIM_GOTO_PLAYER;
		Escort_special_goal = -1;
		Escort_goal_object = ESCORT_GOAL_UNSPECIFIED;
		Escort_last_path_created = Buddy_last_seen_player = Buddy_last_player_path_created = GameTime64;
		Control_center_destroyed = 1;
		int first_exit = -1, return_frames = 0;
		// Segment centers approximate the log, not its unrecorded positions/inputs
		for (int frame = 0; frame < 960; ++frame) {
			const int seg = frame < 260 ? (early_retreat ? 350 : 358) : frame < 560 ? 350 : frame < 860 ? 348 : 370;
			if (ConsoleObject->segnum != seg) place(ConsoleObject, seg);
			Believed_player_pos = ConsoleObject->pos;
			Believed_player_seg = ConsoleObject->segnum;
			GameTime64 += FrameTime;
			if (frame % 3 == 0) ++d_tick_count;
			const fix distance = vm_vec_dist_quick(&bot->pos, &ConsoleObject->pos);
			const int visible = object_to_object_visibility(bot, ConsoleObject, FQ_TRANSWALL) ? 2 : 0;
			compare(bot, "reactor_room_" + std::to_string(scenario) + "_" + std::to_string(frame), [&](bool ref) {
				if (ref) { redux_do_escort_frame(bot, distance, visible); redux_ai_follow_path(bot, visible, visible, nullptr); }
				else { do_escort_frame(bot, distance, visible); ai_follow_path(bot, visible, visible, nullptr); }
			});
			do_escort_frame(bot, distance, visible);
			ai_follow_path(bot, visible, visible, nullptr);
			if (Escort_goal_object == ESCORT_GOAL_EXIT && first_exit < 0) first_exit = frame;
			if (Ai_local_info[Buddy_objnum].mode == AIM_GOTO_PLAYER) ++return_frames;
			do_physics_sim(bot);
		}
		Results["reactor_room_approximation"][early_retreat ? "early_retreat" : "sampled_segments"][coop ? "coop" : "single_player"] = {
			{ "frames", 960 }, { "first_exit_frame", first_exit }, { "return_frames", return_frames }
		};
	}
	start.restore(bot);
	if (ConsoleObject->segnum != player.segnum) obj_relink(ConsoleObject - Objects, player.segnum);
	*ConsoleObject = player;
	Players[Player_num].flags = flags;
	Control_center_destroyed = destroyed;
	Game_mode = game_mode;
	Escort_owner_player = owner;
	GameTime64 = time;
	d_tick_count = tick;
	FrameTime = dt;
	Believed_player_pos = believed_pos;
	Believed_player_seg = believed_seg;
}
int test_guidebot_live_navigation(const char *output, const char *audit)
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
	if (audit && !std::strcmp(audit, "save-continuity")) return audit_save_continuity(output);
	auto &a = bot->ctype.ai_info;
	auto &l = Ai_local_info[Buddy_objnum];
	place(bot, ConsoleObject->segnum);
	Believed_player_seg = ConsoleObject->segnum;
	Believed_player_pos = ConsoleObject->pos;
	const auto player_flags = Players[Player_num].flags;
	const auto object_flags = ConsoleObject->flags;
	const int destroyed = Control_center_destroyed;
	const int game_mode = Game_mode;
	const int protocol = multi_protocol;
	// Co-op keys stay in the world after pickup; goal selection must follow inventory
	Game_mode = GM_NETWORK | GM_MULTI_COOP | GM_MULTI_ROBOTS;
	multi_protocol = MULTI_PROTO_UDP;
	Players[Player_num].flags = 0;
	ConsoleObject->flags = 0;
	Escort_special_goal = -1;
	const int key_ids[] = { POW_KEY_BLUE, POW_KEY_GOLD, POW_KEY_RED };
	const int key_flags[] = { PLAYER_FLAGS_BLUE_KEY, PLAYER_FLAGS_GOLD_KEY, PLAYER_FLAGS_RED_KEY };
	const int key_goals[] = { ESCORT_GOAL_BLUE_KEY, ESCORT_GOAL_GOLD_KEY, ESCORT_GOAL_RED_KEY };
	for (int k = 0; k < 3; ++k) {
		int objnum = obj_create(OBJ_POWERUP, key_ids[k], ConsoleObject->segnum, &ConsoleObject->pos,
		                       nullptr, F1_0, CT_POWERUP, MT_NONE, RT_POWERUP);
		check("coop_key_created", objnum >= 0);
		if (objnum < 0) continue;
		check("coop_key_before_pickup_" + std::to_string(k), escort_set_goal_object() == key_goals[k]);
		const int consumed = do_powerup(&Objects[objnum]);
		check("coop_key_remains_" + std::to_string(k), !consumed && Objects[objnum].type == OBJ_POWERUP);
		check("coop_key_inventory_" + std::to_string(k), (Players[Player_num].flags & key_flags[k]) != 0);
		check("coop_key_goal_advances_" + std::to_string(k), escort_set_goal_object() != key_goals[k]);
		obj_delete(objnum);
	}
	const int owner = Escort_owner_player;
	const int remote = (Player_num + 1) % MAX_PLAYERS;
	const auto remote_flags = Players[remote].flags;
	const auto remote_connected = Players[remote].connected;
	const int blue = obj_create(OBJ_POWERUP, POW_KEY_BLUE, ConsoleObject->segnum, &ConsoleObject->pos,
	                            nullptr, F1_0, CT_POWERUP, MT_NONE, RT_POWERUP);
	check("owner_key_created", blue >= 0);
	Escort_owner_player = remote;
	Players[remote].connected = CONNECT_PLAYING;
	Players[remote].flags = 0;
	check("coop_uses_owner_inventory", escort_set_goal_object() == ESCORT_GOAL_BLUE_KEY);
	Players[remote].flags = PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_GOLD_KEY | PLAYER_FLAGS_RED_KEY;
	check("coop_owner_keys_advance_goal", escort_set_goal_object() > ESCORT_GOAL_RED_KEY);
	Players[remote].connected = CONNECT_DISCONNECTED;
	check("coop_disconnected_owner_uses_local_inventory", escort_set_goal_object() > ESCORT_GOAL_RED_KEY);
	if (blue >= 0) obj_delete(blue);
	Players[remote].flags = remote_flags;
	Players[remote].connected = remote_connected;
	Escort_owner_player = owner;
	Game_mode = game_mode;
	multi_protocol = protocol;
	for (int flags = 0; flags < 8; ++flags)
		for (int dead = 0; dead < 2; ++dead) {
			Players[Player_num].flags = (flags << 1);
			Control_center_destroyed = dead;
			Escort_special_goal = -1;
			// The frozen selector has an upstream bug: it reads inventory from object flags
			// Supply its intended input, then verify the live selector ignores object flags
			ConsoleObject->flags = flags << 1;
			const int expected = redux_escort_set_goal_object();
			for (int object_keys = 0; object_keys < 8; ++object_keys) {
				ConsoleObject->flags = object_keys << 1;
				check("selector_" + std::to_string(flags) + "_" + std::to_string(dead) + "_" + std::to_string(object_keys), escort_set_goal_object() == expected);
			}
		}
	Players[Player_num].flags = player_flags;
	ConsoleObject->flags = object_flags;
	Control_center_destroyed = destroyed;
	audit_redux_return_events(bot);
	audit_reactor_room_return(bot);
	audit_stale_player_return(bot);
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
		check("save_default_" + std::to_string(mode), write_player_file() == 0);
		guidebot_routing_set_mode(1 - mode);
		check("restore_mode_" + std::to_string(mode), state_restore_all_sub(name, 0) != 0 && guidebot_routing_mode() == 1 - mode);
	}
	// Replay both real startup paths with a default opposed to recorded mode
	const std::string replay_path = std::string(output) + ".dximdemo";
	for (int checkpoint : { 0, 1 })
		for (int mode : { GUIDEBOT_ROUTING_ORIGINAL, GUIDEBOT_ROUTING_ENHANCED }) {
			const std::string label = "replay_" + std::to_string(checkpoint) + "_" + std::to_string(mode);
			input_demo_recorder_settings settings;
			input_demo_recorder_settings_clear(&settings);
			settings.game = INPUT_DEMO_GAME_D2;
			settings.mission = Current_mission_filename;
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
