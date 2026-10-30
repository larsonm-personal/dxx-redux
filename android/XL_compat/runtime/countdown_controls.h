#ifndef DXX_XL_COUNTDOWN_CONTROLS_H
#define DXX_XL_COUNTDOWN_CONTROLS_H
// Shared controlled engine events, never autonomous navigation
#include "reactor_countdown.h"
#include "../countdown.h"
#include "preview.h"
#include "object_trigger.h"
#include "trigger_state.h"
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
extern "C" {
#include "inferno.h"
#include "cntrlcen.h"
#include "game.h"
#include "gameseq.h"
#include "object.h"
#include "player.h"
#include "switch.h"
#include "wall.h"
#include "segment.h"
#include "rng_shared.h"
void do_countdown_frame(void);
void special_reactor_stuff(void);
void obj_delete_all_that_should_be_dead(void);
extern int Countdown_seconds_left, Total_countdown_time;
}
namespace xl_countdown_controls
{
inline void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
inline xl_countdown_state snapshot()
{
	xl_countdown_state state{};
	xl_countdown_get(&state);
	return state;
}
inline bool equal(const xl_countdown_state &a, const xl_countdown_state &b)
{
	return a.destroyed == b.destroyed && a.timer == b.timer && a.seconds == b.seconds && a.total == b.total && a.pending == b.pending && a.phase == b.phase && a.fraction == b.fraction;
}
inline void fixture()
{
	require(xl_level_extensions_active() && Num_triggers == 8 && xl_object_trigger_count() == 2 && Triggers[0].type == 25 && Triggers[0].time == 99 && Triggers[1].time == 1000 && Triggers[0].num_links == 1 && Triggers[0].seg[0] == -300 && Triggers[0].side[0] == 42,
	        "Countdown controls require the labeled derived fixture");
}
inline int fire(int index)
{
	xl_wall_trigger_reset();
	return check_trigger_sub(index, Player_num, 1);
}
inline void step(int time)
{
	const auto previous = FrameTime;
	FrameTime = time;
	GameTime64 += time;
	do_countdown_frame();
	FrameTime = previous;
}
inline void events()
{
	fixture();
	const bool autoplay_ran = Triggers[5].flags & TF_DISABLED;
	const auto before_autoplay = snapshot();
	xl_wall_trigger_frame();
	xl_object_trigger_state autoplay{};
	require(Control_center_destroyed && Countdown_timer > 0 && Countdown_timer <= 22 * F1_0 && Total_countdown_time == 22 && (Triggers[5].flags & TF_DISABLED) &&
	            xl_object_trigger_state_get(1, &autoplay) && autoplay.operated && (autoplay.flags & TF_DISABLED),
	        "Countdown wall/object autoplay lost authored seconds");
	// Android reaches this control after ordinary gameplay frames have elapsed
	require(autoplay_ran ? equal(before_autoplay, snapshot()) : Countdown_timer == 22 * F1_0, "Countdown autoplay repeated or did not initialize exactly");
	const auto seconds = Countdown_seconds_left;
	xl_wall_trigger_reset();
	const auto &front = Walls[Num_walls - 2];
	check_trigger(&Segments[front.segnum], short(front.sidenum), short(Players[Player_num].objnum), 0);
	require(Countdown_timer == 99 * F1_0 && Total_countdown_time == 99 && Countdown_seconds_left == seconds, "Countdown trigger used value or reset displayed seconds");
	require(fire(1) == 0 && Countdown_timer == 1000 * F1_0 && fire(1) == 1, "Long countdown was delayed or one-shot repeated");
	for (int difficulty = 0; difficulty < 5; ++difficulty) {
		const auto original_difficulty = Difficulty_level, base = Base_control_center_explosion_time;
		Difficulty_level = difficulty;
		for (int custom : { 30, 60 }) {
			Base_control_center_explosion_time = custom;
			for (int index : { 2, 3 }) require(fire(index) == 0 && Total_countdown_time == xl_compat::countdown_total(Triggers[index].time, custom, difficulty) && Countdown_timer == Total_countdown_time * F1_0, "Default/custom countdown differs by difficulty");
		}
		Difficulty_level = original_difficulty;
		Base_control_center_explosion_time = base;
	}
	const int master_result = fire(4);
	if (master_result || Countdown_timer != 99 * F1_0)
		throw std::runtime_error("Countdown master result=" + std::to_string(master_result) + " timer=" + std::to_string(Countdown_timer) + " flags=" + std::to_string(Triggers[4].flags) + " links=" + std::to_string(Triggers[4].num_links) + " wall=" + std::to_string(Walls[Num_walls - 2].trigger));
	const auto before = snapshot();
	require(fire(6) == 1 && equal(before, snapshot()), "Disabled countdown changed live state");
	auto bad = Triggers[0];
	bad.time = 32768;
	require(!xl_countdown_action(&bad) && equal(before, snapshot()), "Overflow countdown partially applied");
	int owner = -1;
	for (int i = 0; i < xl_object_trigger_count(); ++i) {
		xl_object_trigger_state state{};
		require(xl_object_trigger_state_get(i, &state), "Missing countdown object binding");
		if (!(state.flags & 128)) owner = state.owner;
	}
	require(owner >= 0 && Objects[owner].type == OBJ_POWERUP, "Countdown removal owner missing");
	Objects[owner].flags |= OF_SHOULD_BE_DEAD;
	obj_delete_all_that_should_be_dead();
	require(Objects[owner].type == OBJ_NONE && Countdown_timer == 120 * F1_0 && Total_countdown_time == 120, "Countdown object duration was overwritten as a generic event time");
}
inline void frames()
{
	fixture();
	require(!game_is_time_paused(), "Countdown frame controls require running simulation");
	xl_countdown_clear();
	require(fire(7) == 0 && Countdown_timer == 14 * F1_0, "Cannot start warning countdown");
	const auto old13 = xl_countdown_sound_queries(113), old_start = xl_countdown_sound_queries(114), old_siren = xl_countdown_sound_queries(32), old_zero = xl_countdown_sound_queries(100), old_explosion = xl_countdown_sound_queries(33);
	const auto initial_rotation = ConsoleObject->mtype.phys_info.rotvel;
	step(F1_0 / 100);
	auto state = snapshot();
	require(state.timer == 14 * F1_0 && state.pending == F1_0 / 100 && state.phase == 9 && state.fraction > 0, "Countdown did not accumulate the sub-tick frame");
	const auto pending = state.pending;
	require(fire(0) == 0 && snapshot().pending == pending, "Countdown retrigger erased source pending time");
	step(F1_0 / 50);
	require(Countdown_timer == 99 * F1_0 - F1_0 / 100 - F1_0 / 50 && snapshot().pending == 0 && snapshot().phase == 0,
	        "Countdown tick failed to consume all pending time or retained cadence overshoot");
	xl_countdown_clear();
	require(fire(7) == 0, "Cannot reset warning sequence");
	for (int i = 0; i < 61; ++i) step(F1_0 / 4);
	require(Countdown_timer == -5 * F1_0 / 4 && Countdown_seconds_left == -1 && snapshot().total == 14 && PaletteBlueAdd == 20,
	        "Countdown displayed seconds, duration or post-zero flash differs");
	require(xl_countdown_sound_queries(113) == old13 + 1 && xl_countdown_sound_queries(114) == old_start + 1 &&
	            xl_countdown_sound_queries(32) > old_siren && xl_countdown_sound_queries(100) == old_zero + 1 && xl_countdown_sound_queries(33) == old_explosion + 1,
	        "Countdown skipped or repeated warning/crossing sounds");
	require(ConsoleObject->mtype.phys_info.rotvel.x != initial_rotation.x || ConsoleObject->mtype.phys_info.rotvel.z != initial_rotation.z, "Countdown never rocked the live player object");
	Player_is_dead = 1;
	const auto dead = snapshot();
	step(F1_0 / 100);
	Player_is_dead = 0;
	require(Countdown_timer == dead.timer && snapshot().pending == 0, "Dead player countdown advanced or retained pending time");
	require(fire(0) == 0, "Cannot reset post-zero countdown");
	reset_palette_add();
}
inline xl_countdown_state prepare()
{
	fixture();
	xl_countdown_clear();
	require(fire(0) == 0, "Cannot prepare countdown continuation");
	step(F1_0 / 100);
	const int timer = Countdown_timer;
	special_reactor_stuff();
	const int bonus = Base_control_center_explosion_time + (4 - Difficulty_level) * Base_control_center_explosion_time / 4;
	require(Countdown_timer == timer + bonus * F1_0 && Total_countdown_time == (Countdown_timer >> 16) + 2,
	        "Special reactor robot did not extend the live countdown");
	const auto state = snapshot();
	require(state.pending == F1_0 / 100 && state.phase == 9 && state.fraction > 0, "Countdown continuation lacks partial cadence");
	return state;
}
inline void continuation(const xl_countdown_state &saved)
{
	require(equal(snapshot(), saved), "Countdown restore lost total, displayed seconds, pending time or fractional cadence");
	// The control is called while paused by ordinary save/rewind owners
	const bool paused = game_is_time_paused();
	step(F1_0 / 50);
	require(Countdown_timer == (paused ? saved.timer : saved.timer - saved.pending - F1_0 / 50) && snapshot().pending == 0 && snapshot().phase == 0, "Saved countdown continuation used Redux or discarded pending time");
}
} // namespace xl_countdown_controls
#endif
