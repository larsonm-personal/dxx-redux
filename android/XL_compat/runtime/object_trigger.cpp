#include "object_trigger.h"
#include "preview.h"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "gameseq.h"
#include "gameseg.h"
#include "segment.h"
#include "switch.h"
#include "wall.h"
#include "robot.h"
#include "rng_shared.h"
#include "boss_health_shared.h"
int xl_object_classic_action(const trigger *action);
int xl_object_master_action(const trigger *action, int object_index);
int xl_object_master_supported(const trigger *action);
}

namespace
{
struct ObjectAction {
	trigger action{};
	int authored_owner = -1, owner = -1, signature = -1, destination = 0;
	bool operated = false;
};
std::vector<ObjectAction> actions, authored;
constexpr int flags_mask = 159;

int opposite(int type)
{
	static constexpr int pairs[] = { 1, 0, 2, 3, 4, 6, 5, 8, 7, 10, 9, 11, 13, 12, 14 };
	return type <= 14 ? pairs[type] : type == 32 ? 33
	                              : type == 33   ? 32
	                                             : type;
}

// Match BuildObjTriggerRef's owner/type quicksort, including equal-key swaps
void sort_actions(int left, int right)
{
	int l = left, r = right;
	const auto median = actions[(l + r) / 2];
	const auto less = [](const ObjectAction &a, const ObjectAction &b) {
		return a.authored_owner < b.authored_owner || (a.authored_owner == b.authored_owner && a.action.type < b.action.type);
	};
	do {
		while (less(actions[l], median)) ++l;
		while (less(median, actions[r])) --r;
		if (l <= r) {
			if (l < r) std::swap(actions[l], actions[r]);
			++l;
			--r;
		}
	} while (l <= r);
	if (l < right) sort_actions(l, right);
	if (left < r) sort_actions(left, r);
}

bool bound(const ObjectAction &entry)
{
	return entry.owner >= 0 && entry.owner <= Highest_object_index &&
	       xl_wall_teleport_actor(entry.owner) && Objects[entry.owner].signature == entry.signature;
}

void operate(ObjectAction &entry)
{
	auto &action = entry.action;
	if (!bound(entry) || (action.flags & TF_DISABLED)) return;
	entry.operated = true;
	if (action.type == 14) xl_object_teleport_action(&action, entry.owner, &entry.destination);
	else if (action.type == 31) xl_object_master_action(&action, entry.owner);
	else if (action.type == 32 || action.type == 33) {
		for (int link = 0; link < action.num_links; ++link) {
			const auto wall = Segments[action.seg[link]].sides[action.side[link]].wall_num;
			if (wall < 0 || wall >= Num_walls) continue;
			const auto index = Walls[wall].trigger;
			if (index >= Num_triggers) continue;
			auto &target = Triggers[index];
			if (action.type == 32) {
				if (target.type == 31 && target.value > 0) --target.value;
				if (target.type != 31 || target.value <= 0) target.flags &= ~TF_DISABLED;
				if ((action.flags & (8 | TF_DISABLED)) == 8) entry.operated = false;
			} else {
				target.flags |= TF_DISABLED;
				if (target.type == 31) target.value = static_cast<std::int32_t>(std::uint32_t(target.value) + 1u);
			}
		}
	} else xl_object_classic_action(&action);
	if (action.flags & TF_ONE_SHOT) action.flags |= TF_DISABLED;
	if (action.flags & 16) action.type = opposite(action.type);
}

float remaining_shields(const object &object)
{
	const auto &robot = Robot_info[object.id];
	fix maximum = robot.strength;
	if (robot.thief || robot.companion) maximum = d2_thief_or_companion_health_maximum(maximum, Current_level_num, Difficulty_level, robot.companion);
	else if (robot.boss_flag) maximum = boss_health_maximum_for_difficulty(maximum, Difficulty_level);
	if (maximum <= 0) return 1.0f;
	return std::clamp(float(object.shields) / float(maximum), 0.0f, 1.0f);
}
} // namespace

extern "C" int xl_object_teleport_action(const trigger *action, int object_index, int *destination)
{
	if (!xl_level_extensions_active() || !action || !destination || !xl_wall_teleport_actor(object_index) || action->num_links < 0 || action->num_links > MAX_WALLS_PER_LINK) return 1;
	for (int i = 0; i < action->num_links; ++i)
		if (action->seg[i] < 0 || action->seg[i] > Highest_segment_index) return 1;
	if (!action->num_links) return 0;
	int segment = action->seg[0];
	if (action->num_links > 1) {
		bool distinct = false;
		for (int i = 1; i < action->num_links; ++i) distinct |= action->seg[i] != segment;
		if (!distinct) return 1;
		do {
			segment = action->seg[d_rand() % action->num_links];
		} while (segment == *destination);
		*destination = segment;
	}
	auto &object = Objects[object_index];
	if (object.segnum != segment) {
		compute_segment_center(&object.pos, &Segments[segment]);
		obj_relink(object_index, segment);
	}
	return 0;
}

void xl_object_trigger_load(const xl_compat::Inventory &level)
{
	xl_object_trigger_clear();
	if (level.object_trigger_records.size() > 255) throw std::runtime_error("object trigger capacity exceeds 255 inspected records");
	for (const auto &record : level.object_trigger_records) {
		const auto &source = record.action;
		if ((source.type > 14 && (source.type < 31 || source.type > 33)) || (source.flags & ~flags_mask) || source.targets.size() > MAX_WALLS_PER_LINK)
			throw std::runtime_error("object trigger action or flags are not implemented");
		ObjectAction entry;
		entry.authored_owner = entry.owner = record.object;
		if (entry.owner >= 0) {
			if (entry.owner > Highest_object_index || Objects[entry.owner].type == OBJ_NONE)
				throw std::runtime_error("object trigger owner is missing after level loading");
			entry.signature = Objects[entry.owner].signature;
		}
		entry.action.type = source.type;
		entry.action.flags = source.flags;
		entry.action.value = source.type == 31 ? 0 : source.value;
		entry.action.time = -1;
		entry.action.num_links = static_cast<short>(source.targets.size());
		for (std::size_t link = 0; link < source.targets.size(); ++link) {
			entry.action.seg[link] = source.targets[link][0];
			entry.action.side[link] = source.targets[link][1];
		}
		actions.push_back(entry);
	}
	if (!actions.empty()) sort_actions(0, static_cast<int>(actions.size()) - 1);
	authored = actions;
}

extern "C" void xl_object_trigger_clear(void)
{
	actions.clear();
	authored.clear();
}

extern "C" void xl_object_trigger_event(int object_index, int damage)
{
	if (!xl_level_extensions_active() || object_index < 0 || object_index > Highest_object_index) return;
	const auto &object = Objects[object_index];
	if (object.type != OBJ_ROBOT && object.type != OBJ_DEBRIS && object.type != OBJ_CNTRLCEN && object.type != OBJ_HOSTAGE && object.type != OBJ_POWERUP) return;
	if (damage && object.type != OBJ_ROBOT) return;
	for (auto &entry : actions) {
		if (entry.owner != object_index) continue;
		if (!bound(entry)) {
			entry.owner = -1;
			continue;
		}
		auto &action = entry.action;
		if (bool(damage) == (action.type == 14)) {
			bool fire = true;
			if (damage) {
				const auto value = action.value >= F1_0 ? action.value / F1_0 : action.value;
				const auto threshold = 10 - std::int64_t(value);
				fire = threshold < 10 && static_cast<int>(remaining_shields(object) * 100) <= threshold * 10;
				if (fire && !(action.flags & 8)) action.value = 0;
			}
			if (fire) operate(entry);
		}
		// Removal consumes the binding even for disabled or damage-only actions
		if (!damage) entry.owner = -1;
	}
}

extern "C" void xl_object_trigger_frame(void)
{
	if (!xl_level_extensions_active()) return;
	for (auto &entry : actions)
		if ((entry.action.flags & 128) && entry.owner >= 0 && !entry.operated) {
			operate(entry);
			entry.action.flags |= TF_DISABLED;
		}
}

extern "C" int xl_object_trigger_count(void)
{
	return xl_level_extensions_active() ? static_cast<int>(actions.size()) : 0;
}

extern "C" int xl_object_trigger_state_get(int index, xl_object_trigger_state *result)
{
	if (!result || index < 0 || index >= xl_object_trigger_count()) return 0;
	auto &entry = actions[index];
	if (entry.owner >= 0 && !bound(entry)) entry.owner = -1;
	*result = { entry.owner, entry.signature, entry.action.type, entry.action.flags, entry.action.value, entry.operated ? 1 : 0, entry.destination };
	return 1;
}

extern "C" int xl_object_trigger_restore(const xl_object_trigger_state *states, int count, int apply)
{
	if (count != xl_object_trigger_count() || (!states && count)) return 0;
	for (int i = 0; i < count; ++i) {
		const auto &state = states[i];
		const auto &original = authored[i];
		if (original.action.type == 31 && !xl_object_master_supported(&original.action)) return 0;
		if (state.owner != -1 && state.owner != original.authored_owner) return 0;
		if (state.signature < (original.authored_owner < 0 ? -1 : 0) || (original.authored_owner < 0 && state.signature != -1)) return 0;
		if (state.owner >= 0 && (state.owner > Highest_object_index || Objects[state.owner].type == OBJ_NONE || Objects[state.owner].signature != state.signature)) return 0;
		if (state.operated < 0 || state.operated > 1 || (state.type != original.action.type && (!(original.action.flags & 16) || state.type != opposite(original.action.type)))) return 0;
		if ((state.flags & ~TF_DISABLED) != (original.action.flags & ~TF_DISABLED) || (state.flags & ~flags_mask)) return 0;
		if ((original.action.flags & TF_DISABLED) && !(state.flags & TF_DISABLED)) return 0;
		if (state.value != original.action.value && (state.value != 0 || original.action.type != 14 || (original.action.flags & 8))) return 0;
		if (state.destination != 0) {
			bool found = false;
			for (int link = 0; link < original.action.num_links; ++link) found |= original.action.seg[link] == state.destination;
			if (original.action.type != 14 || !found) return 0;
		}
	}
	if (apply)
		for (int i = 0; i < count; ++i) {
			const auto &state = states[i];
			auto &entry = actions[i];
			entry.owner = state.owner;
			entry.signature = state.signature;
			entry.action.type = state.type;
			entry.action.flags = state.flags;
			entry.action.value = state.value;
			entry.operated = state.operated != 0;
			entry.destination = state.destination;
		}
	return 1;
}
