#include "preview.h"
#include "speed_boost.h"
#include "spawn_points.h"
#include "level_messages.h"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <algorithm>
#include "object_trigger.h"
#include "trigger_state.h"
#include "../trigger_flags.h"
#include "../player_resources.h"
#include "save_epoch_format.h"
extern "C" {
#include "inferno.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "segment.h"
#include "switch.h"
#include "wall.h"
#include "timer.h"
#include "rng_shared.h"
#include "sounds.h"
int xl_object_classic_action(const trigger *action);
}

namespace
{
int depth = -1;
std::uint64_t exit_generation = 0;
std::array<fix64, MAX_TRIGGERS> last_activation{};
std::array<bool, MAX_TRIGGERS> activated{};
struct ScheduledTrigger {
	bool operated = false;
	fix64 started = 0;
	int delay_ms = -1, object = -1, shot = 0;
	bool attached = false;
	int signature = 0;
};
std::array<ScheduledTrigger, MAX_TRIGGERS> scheduled{};
std::array<int, MAX_TRIGGERS> object_destinations{};
#if defined(INTROSPECT_ON) || defined(DXX_XL_DEVELOPER_PROBE)
std::array<std::array<std::uint64_t, 2>, 2> cue_queries{};
#endif
int stock_bypass = -1;
constexpr int supported_flags = xl_compat::trigger_flags_mask;
bool delayed(const trigger &action)
{
	if (action.type == 29) return false;
	const auto magnitude = std::abs(std::int64_t(action.time));
	return magnitude >= 100 && magnitude <= 900000;
}
fix delay_ticks(int milliseconds)
{
	// Reference MSEC2X uses a float multiply and truncates to fixed point
	return static_cast<fix>(float(milliseconds) * (65536.0f / 1000.0f));
}
int opposite(int type)
{
	static constexpr int pairs[] = { 1, 0, 2, 3, 4, 6, 5, 8, 7, 10, 9, 11, 13, 12, 14 };
	return type <= 14 ? pairs[type] : type == 32 ? 33
	                              : type == 33   ? 32
	                                             : type;
}
struct TriggerDepth {
	TriggerDepth()
	{
		++depth;
	}
	~TriggerDepth()
	{
		--depth;
	}
};
int linked_trigger(const trigger &source, int link)
{
	const int segment = source.seg[link], side = source.side[link];
	if (segment < 0 || segment > Highest_segment_index || side < 0 || side >= 6) return -1;
	const int wall = Segments[segment].sides[side].wall_num;
	if (wall < 0 || wall >= Num_walls) return -1;
	const int target = wall_trigger_index(Walls[wall].trigger);
	return target >= 0 && target < Num_triggers ? target : -1;
}
} // namespace

extern "C" void xl_wall_trigger_reset(void)
{
	activated.fill(false);
	scheduled.fill({});
	object_destinations.fill(0);
}

extern "C" int xl_wall_trigger_stock_active(void)
{
	return stock_bypass >= 0;
}

extern "C" int xl_trigger_cue_enabled(const trigger *action, int sound)
{
	if (!xl_level_extensions_active() || !action) return 1;
	const int slot = sound == SOUND_FORCEFIELD_OFF ? 0 : sound == SOUND_WALL_REMOVED ? 1
	                                                                                 : -1;
	if (slot < 0) return 1;
	const bool enabled = xl_compat::trigger_cue_enabled(action->flags, slot ? xl_compat::TriggerCue::IllusionOff : xl_compat::TriggerCue::ForcefieldOff);
#if defined(INTROSPECT_ON) || defined(DXX_XL_DEVELOPER_PROBE)
	++cue_queries[slot][enabled ? 1 : 0];
#endif
	return enabled;
}

extern "C" std::uint64_t xl_trigger_cue_queries(int sound, int enabled)
{
#if defined(INTROSPECT_ON) || defined(DXX_XL_DEVELOPER_PROBE)
	const int slot = sound == SOUND_FORCEFIELD_OFF ? 0 : sound == SOUND_WALL_REMOVED ? 1
	                                                                                 : -1;
	if (slot >= 0 && (enabled == 0 || enabled == 1)) return cue_queries[slot][enabled];
#else
	(void) sound;
	(void) enabled;
#endif
	return 0;
}

extern "C" void xl_wall_trigger_exit(void)
{
	xl_speed_boost_stop(Players[Player_num].objnum);
	if (depth >= 0) ++exit_generation;
}

// Return -1 for ordinary engine dispatch; children retain the original actor
static int operate_wall(int index, int object_index, int shot, bool attached, int signature = -1)
{
	if (!xl_level_extensions_active() || index < 0 || index >= Num_triggers) return -1;
	if (stock_bypass == index) return -1;
	if (depth > 15) return 1;
	auto &action = Triggers[index];
	const bool chain = action.type >= 31 && action.type <= 33;
	const bool resource = xl_compat::player_resource_action(action.type);
	if (!chain && action.type > 14 && action.type != 15 && action.type != 28 && action.type != 29 && !resource) return 1;
	if ((Game_mode & GM_MULTI) || (action.flags & TF_DISABLED) || index >= MAX_TRIGGERS || object_index < 0 || object_index >= MAX_OBJECTS) return 1;
	const bool live = object_index <= Highest_object_index && (Objects[object_index].type == OBJ_PLAYER || xl_wall_teleport_actor(object_index)) && (signature < 0 || Objects[object_index].signature == signature);
	// Attached events outlive their owner; geometry work does not require an actor
	// Motion still requires the same live object, never a recycled slot
	if (!live && (!attached || signature < 0)) return 1;
	if (signature < 0) signature = Objects[object_index].signature;
	const int actor = live ? Objects[object_index].type : OBJ_NONE;
	const bool player = actor == OBJ_PLAYER;
	if (player) {
		if (object_index != Players[Player_num].objnum) return 1;
	} else if (!attached && action.type != 31 && action.type != 14 && action.type != 15) return 1;
	if ((action.flags & ~supported_flags) || action.num_links < 0 || action.num_links > MAX_WALLS_PER_LINK || (action.type == 28 && !action.num_links)) return 1;
	for (int i = 0; !resource && action.type != 29 && i < action.num_links; ++i)
		if (action.seg[i] < 0 || action.seg[i] > Highest_segment_index || action.side[i] < 0 || action.side[i] >= 6) return 1;
	TriggerDepth scope;
	const auto entry_exit = exit_generation;
	if (!depth && !attached && action.type != 14 && action.type != 15) {
		const auto now = timer_query();
		if (activated[index] && now - last_activation[index] < F1_0 * 3 / 4) return 1;
		activated[index] = true;
		last_activation[index] = now;
	}
	auto &state = scheduled[index];
	if (!state.operated) {
		state.operated = true;
		state.started = GameTime64;
		state.object = object_index;
		state.shot = shot;
		state.attached = attached;
		state.signature = signature;
		if (delayed(action)) {
			if (action.time > 0) state.delay_ms = action.time;
			else {
				const int tenth = -action.time / 10;
				state.delay_ms = tenth + tenth * (d_rand() % 10);
			}
		}
	}
	if (delayed(action) && GameTime64 - state.started < delay_ticks(state.delay_ms)) {
		activated[index] = false;
		return 1;
	}
	if (action.type == 29) {
		xl_message_action(&action);
	} else if (action.type == 28) {
		xl_spawn_action(&action);
	} else if (action.type == 15) {
		if (live) xl_speed_boost_action(&action, index, object_index, shot);
	} else if (resource) {
		xl_player_resource_trigger(&action);
	} else if (action.type == 14) {
		// A dead player's teleport skips motion but still reaches post-action flags
		if (attached) {
			if (live) xl_object_teleport_action(&action, object_index, &object_destinations[index]);
		} else xl_wall_teleport_action(index, object_index, shot);
	} else if (!chain && attached) {
		xl_object_classic_action(&action);
	} else if (!chain) {
		const int previous = stock_bypass;
		stock_bypass = index;
		check_trigger_sub(index, Player_num, shot);
		stock_bypass = previous;
		if (exit_generation != entry_exit) return 1;
	}
	for (int i = 0; chain && i < action.num_links; ++i) {
		const int target_index = linked_trigger(action, i);
		if (target_index < 0) continue;
		auto &target = Triggers[target_index];
		if (action.type == 31) {
			if (attached) operate_wall(target_index, object_index, 0, true, signature);
			else if (player) check_trigger_sub(target_index, Player_num, 0);
			else xl_wall_trigger(target_index, object_index, 0);
			if (exit_generation != entry_exit) return 1;
		} else if (action.type == 32) {
			if (target.type == 31 && target.value > 0) --target.value;
			if (target.type != 31 || target.value <= 0) target.flags &= ~TF_DISABLED;
			if ((action.flags & (8 | TF_DISABLED)) == 8) state.operated = false;
		} else {
			target.flags |= TF_DISABLED;
			if (target.type == 31) target.value = static_cast<std::int32_t>(std::uint32_t(target.value) + 1u);
		}
	}
	if (action.flags & TF_ONE_SHOT) action.flags |= TF_DISABLED;
	if (action.flags & 16) action.type = opposite(action.type);
	xl_preview_trigger_result(index, shot, 0);
	return 0;
}

extern "C" int xl_wall_trigger(int index, int object_index, int shot)
{
	return operate_wall(index, object_index, shot, false);
}

extern "C" int xl_object_master_action(const trigger *action, int object_index)
{
	if (!xl_level_extensions_active() || !action || depth > 15 || !xl_wall_teleport_actor(object_index)) return 1;
	TriggerDepth scope;
	for (int link = 0; link < action->num_links; ++link) {
		const int child = linked_trigger(*action, link);
		if (child >= 0) operate_wall(child, object_index, 0, true);
	}
	return 0;
}

extern "C" int xl_object_master_supported(const trigger *action)
{
	if (!action) return 0;
	std::array<bool, MAX_TRIGGERS> seen{};
	std::array<int, MAX_TRIGGERS> pending{};
	int count = 0;
	const auto add = [&](const trigger &master) {
		for (int link = 0; link < master.num_links; ++link) {
			const int child = linked_trigger(master, link);
			if (child >= 0 && child < MAX_TRIGGERS && !seen[child]) {
				seen[child] = true;
				pending[count++] = child;
			}
		}
	};
	add(*action);
	while (count) {
		const auto &child = Triggers[pending[--count]];
		if (child.type == 14 && child.num_links > 1) {
			bool distinct = false;
			for (int i = 1; i < child.num_links; ++i) distinct |= child.seg[i] != child.seg[0];
			if (!distinct) return 0;
		}
		if (child.type == 31) add(child);
	}
	return 1;
}

extern "C" void xl_wall_trigger_frame(void)
{
	if (!xl_level_extensions_active()) return;
	xl_message_frame();
	for (int index = 0; index < Num_triggers && index < MAX_TRIGGERS; ++index) {
		auto &action = Triggers[index];
		if (action.flags & TF_DISABLED) continue;
		auto &state = scheduled[index];
		const auto generation = exit_generation;
		if ((action.flags & 128) && !state.operated && (delayed(action) || !(action.flags & 8))) {
			xl_wall_trigger(index, Players[Player_num].objnum, 0);
			if (exit_generation != generation) return;
			if (!delayed(action)) action.flags |= TF_DISABLED;
		}
		if (state.operated && delayed(action) && GameTime64 - state.started >= delay_ticks(state.delay_ms)) {
			operate_wall(index, state.object, state.shot, state.attached, state.signature);
			if (exit_generation != generation) return;
			if (action.flags & 8) state.operated = false;
			else {
				action.time = 0;
				action.flags |= TF_DISABLED;
			}
		}
	}
	xl_object_trigger_frame();
	xl_speed_boost_frame();
}

extern "C" int xl_wall_trigger_state(int index, xl_trigger_state *result)
{
	if (!xl_level_extensions_active() || !result || index < 0 || index >= Num_triggers || index >= MAX_TRIGGERS) return 0;
	const auto &state = scheduled[index];
	*result = state.operated ? xl_trigger_state{ GameTime64 - state.started, state.delay_ms, state.object, state.shot, 1 } : xl_trigger_state{ 0, -1, -1, 0, 0 };
	result->destination = object_destinations[index];
	result->attached = state.operated && state.attached;
	result->signature = state.operated ? state.signature : 0;
	return 1;
}

extern "C" int xl_wall_trigger_restore(const xl_trigger_state *states, int count)
{
	return xl_wall_trigger_restore_mode(states, count, 1);
}

extern "C" int xl_wall_trigger_restore_mode(const xl_trigger_state *states, int count, int apply)
{
	if (!xl_level_extensions_active() || (!states && count) || count != Num_triggers || count < 0 || count > MAX_TRIGGERS) return 0;
	for (int i = 0; i < count; ++i) {
		const auto &state = states[i];
		const auto &action = Triggers[i];
		const bool resource = xl_compat::player_resource_action(action.type);
		if ((action.type > 14 && action.type != 15 && action.type != 28 && action.type != 29 && !resource && (action.type < 31 || action.type > 33)) || (action.flags & ~supported_flags) || action.num_links < 0 || action.num_links > MAX_WALLS_PER_LINK || (action.type == 28 && !action.num_links)) return 0;
		for (int target = 0; !resource && action.type != 29 && target < action.num_links; ++target)
			if (action.seg[target] < 0 || action.seg[target] > Highest_segment_index || action.side[target] < 0 || action.side[target] >= 6) return 0;
		if (state.destination < 0 || state.destination > Highest_segment_index) return 0;
		if (state.destination) {
			if (!xl_object_trigger_count() || action.type != 14) return 0;
			bool found = false;
			for (int link = 0; link < action.num_links; ++link) found |= action.seg[link] == state.destination;
			if (!found) return 0;
		}
		if (state.operated < 0 || state.operated > 1 || state.elapsed < 0 || state.elapsed > SAVE_EPOCH_MAX || state.delay_ms < -1 || state.delay_ms > 900000 || state.object < -1 || state.object >= MAX_OBJECTS || state.shot < 0 || state.shot > 1) return 0;
		if (state.attached < 0 || state.attached > 1 || state.signature < 0 || (state.attached && !xl_object_trigger_count())) return 0;
		if (!state.operated && (state.attached || state.signature)) return 0;
		if (!state.operated && (state.elapsed || state.delay_ms != -1 || state.object != -1 || state.shot)) return 0;
		if (state.operated && state.object < 0) return 0;
		if (state.operated && delayed(action)) {
			if (action.time > 0 && state.delay_ms != action.time) return 0;
			if (action.time < 0) {
				const int tenth = -action.time / 10;
				if (state.delay_ms < tenth || state.delay_ms > tenth * 10 || state.delay_ms % tenth) return 0;
			}
		}
	}
	if (!apply) return 1;
	for (int i = 0; i < count; ++i) {
		const auto &state = states[i];
		scheduled[i] = { state.operated != 0, GameTime64 - state.elapsed, state.delay_ms, state.object, state.shot, state.attached != 0, state.signature };
		object_destinations[i] = state.destination;
	}
	activated.fill(false);
	return 1;
}
