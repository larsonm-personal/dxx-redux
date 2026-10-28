#include "admission.h"
#include "capacity.h"
#include <algorithm>
#include <cstdint>
#include <sstream>

namespace xl_compat
{
std::vector<int> object_chain_targets(const Inventory &level, const Trigger &master)
{
	std::vector<int> result;
	std::vector<bool> seen(level.triggers.size());
	std::vector<const Trigger *> pending{ &master };
	while (!pending.empty()) {
		const auto *action = pending.back();
		pending.pop_back();
		for (const auto &target : action->targets) {
			if (target[0] < 0 || static_cast<std::size_t>(target[0]) >= level.segments.size() || target[1] < 0 || target[1] >= 6) continue;
			const int wall = level.segments[target[0]].sides[target[1]].wall;
			if (wall < 0 || static_cast<std::size_t>(wall) >= level.walls.size()) continue;
			const int index = level.walls[wall].trigger;
			if (index < 0 || static_cast<std::size_t>(index) >= level.triggers.size() || seen[index]) continue;
			seen[index] = true;
			result.push_back(index);
			if (level.triggers[index].type == 31) pending.push_back(&level.triggers[index]);
		}
	}
	return result;
}

std::vector<Issue> runtime_blockers(const Inventory &level, bool experimental_water)
{
	std::vector<Issue> result;
	for (const auto &issue : level.unsupported) {
		if (level.level_version == 17 && level.game_version == 36 && issue.feature == "segment_function" && issue.detail == "2") continue;
		if (level.level_version == 23 && issue.feature == "segment_function_v21" && issue.detail == "2") continue;
		if (level.level_version == 23 && issue.feature == "segment_function_v21" && issue.detail == "11") continue;
		if (issue.feature == "segment_function" && issue.detail == "11") continue;
		if (issue.feature == "segment_function_v21" && issue.detail == "9") continue;
		if ((issue.feature == "trigger_type" || issue.feature == "object_trigger_type") && issue.detail == "15") continue;
		if ((issue.feature == "trigger_type" || issue.feature == "object_trigger_type") && issue.detail == "28") continue;
		if (issue.feature == "object_triggers") continue;
		if (issue.feature == "object_trigger_type" && (issue.detail == "14" || issue.detail == "26" || issue.detail == "31" || issue.detail == "32" || issue.detail == "33")) continue;
		if (issue.feature == "trigger_type" && (issue.detail == "14" || issue.detail == "31" || issue.detail == "32" || issue.detail == "33")) continue;
		if (issue.feature == "trigger_type" && (issue.detail == "17" || issue.detail == "18")) continue;
		if (experimental_water) {
			if (level.level_version < 21 && issue.feature == "segment_function" && issue.detail == "7") continue;
		}
		if (issue.feature != "embedded_lighting" &&
		    !(issue.feature == "segment_function" && issue.detail == "13")) result.push_back(issue);
	}
	for (const auto &issue : level.unknown)
		// These describe validation stages, not serialized unknown features
		if (issue.feature != "object_dependencies" && issue.feature != "assets" && issue.feature != "runtime_semantics")
			result.push_back(issue);
	auto limit = [&](bool exceeded, const char *feature, const char *detail) {
		if (exceeded) result.push_back({ feature, -1, detail });
	};
	limit(!((level.level_version == 15 && (level.game_version == 33 || level.game_version == 34)) || (level.level_version == 17 && level.game_version == 36) || (level.level_version == 23 && level.game_version == 40)), "serialization", "requires audited level/game-data version 15/33, 15/34, 17/36 or 23/40");
	limit(level.walls.size() > DXX_XL_MAX_WALLS, "wall capacity", "more than 2047 walls");
	for (std::size_t i = 0; i < level.walls.size(); ++i) {
		const auto &wall = level.walls[i];
		if (wall.type != 2) continue;
		if (wall.clip < 0) result.push_back({ "door animation", static_cast<int>(i), "door has no wall animation" });
		if (wall.segment < 0 || static_cast<std::size_t>(wall.segment) >= level.segments.size() || wall.side < 0 || wall.side >= 6) continue;
		const int child = level.segments[wall.segment].children[wall.side];
		if (child < 0 || static_cast<std::size_t>(child) >= level.segments.size()) continue;
		const auto &back = level.segments[child];
		for (int side = 0; side < 6; ++side) {
			if (back.children[side] != wall.segment) continue;
			const int other = back.sides[side].wall;
			if (other >= 0 && static_cast<std::size_t>(other) < level.walls.size() && level.walls[other].clip < 0)
				result.push_back({ "door animation", static_cast<int>(i), "opposite wall " + std::to_string(other) + " has no animation for paired door closing" });
			break;
		}
	}
	limit(xl_object_capacity_for_count(level.objects.size()) > DXX_XL_MAX_OBJECTS, "object capacity", ("authored objects require " + std::to_string(xl_object_capacity_for_count(level.objects.size())) + " runtime slots; supported signed-index pool is 32768").c_str());
	limit(level.segments.size() > 9000 || level.vertices.size() > 36000, "geometry capacity", "more than 9000 segments or 36000 vertices");
	limit(level.triggers.size() > DXX_XL_MAX_TRIGGERS, "trigger capacity", "more than 254 triggers");
	limit(level.matcens.size() > 20, "generator capacity", "more than 20 robot centers");
	limit(level.equipment.size() > 128, "equipment capacity", "more than 128 equipment makers");
	limit(!level.equipment.empty() && level.level_version != 23, "equipment layout", "equipment makers require the audited modern 23/40 profile");
	for (std::size_t row = 0; row < level.equipment.size(); ++row)
		for (unsigned id = 0; id < 50; ++id)
			if ((std::uint32_t(level.equipment[row][id / 32]) & (std::uint32_t(1) << (id % 32))) &&
			    (id == 7 || id == 8 || id == 9 || id == 24 || id == 26 || id == 27 || id >= 46))
				result.push_back({ "equipment item", static_cast<int>(row), "powerup " + std::to_string(id) + " requires unsupported multiplayer, addon or release pickup behavior" });
	std::vector<int> equipment_owners(level.equipment.size(), -1);
	// XL AssignProducer uses the authored segment producer index, ignoring the
	// redundant links in each disk row. Stock renumbers segments during mine load
	std::vector<int> generator_owners(level.matcens.size(), -1);
	int stations = 0;
	for (std::size_t i = 0; i < level.segments.size(); ++i) {
		const auto &segment = level.segments[i];
		if (segment.function >= 1 && segment.function <= 4) ++stations;
		if (level.level_version == 23 && segment.function == 11) {
			++stations;
			if (segment.producer < 0 || static_cast<std::size_t>(segment.producer) >= equipment_owners.size())
				result.push_back({ "equipment assignment", static_cast<int>(i), "equipment segment has no source record for its producer index" });
			else if (equipment_owners[segment.producer] >= 0)
				result.push_back({ "equipment assignment", static_cast<int>(i), "producer index is shared by multiple equipment segments" });
			else equipment_owners[segment.producer] = static_cast<int>(i);
		}
		if (segment.function != 4) continue;
		if (segment.producer < 0 || static_cast<std::size_t>(segment.producer) >= generator_owners.size())
			result.push_back({ "generator assignment", static_cast<int>(i), "robotmaker segment has no source record for its producer index" });
		else if (generator_owners[segment.producer] >= 0)
			result.push_back({ "generator assignment", static_cast<int>(i), "producer index is shared by multiple robotmaker segments" });
		else generator_owners[segment.producer] = static_cast<int>(i);
	}
	for (std::size_t i = 0; i < generator_owners.size(); ++i)
		if (generator_owners[i] < 0) result.push_back({ "generator assignment", static_cast<int>(i), "source record has no robotmaker segment with this producer index" });
	for (std::size_t i = 0; i < equipment_owners.size(); ++i)
		if (equipment_owners[i] < 0) result.push_back({ "equipment assignment", static_cast<int>(i), "source record has no equipment segment with this producer index" });
	limit(stations > 128, "generator assignment", "special-segment station links exceed the native signed-byte range");
	limit(level.light_indices.size() > 500 || level.delta_lights.size() > 32000, "lighting capacity", "more than 500 light indices or 32000 delta lights");
	// Translation splits counts into signed-byte chunks, preserving zero-count records
	std::size_t translated_indices = 0;
	for (const auto &index : level.light_indices)
		translated_indices += index[2] ? (index[2] + 126) / 127 : 1;
	limit(translated_indices > 500, "changing-light capacity", "lossless translation requires more than 500 light indices");
	limit(level.flicker_lights.size() > 100, "flicker capacity", "more than 100 flickering lights");
	limit(level.object_triggers < 0 || static_cast<std::size_t>(level.object_triggers) != level.object_trigger_records.size(), "object triggers", "declared records and inspected owners differ");
	limit(level.object_trigger_records.size() > DXX_XL_MAX_TRIGGERS, "object trigger capacity", "more than 254 object-attached actions");
	for (std::size_t i = 0; i < level.object_trigger_records.size(); ++i) {
		const auto &record = level.object_trigger_records[i];
		const auto &action = record.action;
		const auto block = [&](bool condition, const char *detail) { if (condition) result.push_back({ "object triggers", static_cast<int>(i), detail }); };
		if (action.type == 26) {
			block(record.object >= 0 && (static_cast<std::size_t>(record.object) >= level.objects.size() || level.objects[record.object].type != 2), "spawn-bot action requires a robot boss owner");
			if (!action.targets.empty()) {
				const int segment = action.targets[0][0];
				block(segment >= 0 && (static_cast<std::size_t>(segment) >= level.segments.size() || level.segments[segment].function != 4), "spawn-bot selection target must be a robotmaker segment or negative for arbitrary selection");
			}
		}
		if (action.type == 31)
			for (int target : object_chain_targets(level, action)) {
				const auto &child = level.triggers[target];
				if (child.type == 14 && child.targets.size() > 1)
					block(std::all_of(child.targets.begin(), child.targets.end(), [&](const auto &link) { return link[0] == child.targets[0][0]; }), "object master teleport child requires distinct destination segments");
			}
		if (record.object >= 0 && static_cast<std::size_t>(record.object) < level.objects.size()) {
			const int type = level.objects[record.object].type;
			block(type != 2 && type != 3 && type != 7 && type != 8 && type != 9, "owner type has no qualified singleplayer event lifecycle");
		}
		if (action.type == 14 && action.targets.size() > 1) {
			block(std::all_of(action.targets.begin(), action.targets.end(), [&](const auto &target) { return target[0] == action.targets[0][0]; }), "multiple teleport links require distinct destination segments to avoid reference retry deadlock");
		}
		if (action.type == 0 || action.type == 1 || (action.type >= 5 && action.type <= 11)) {
			for (const auto &target : action.targets) {
				if (target[0] < 0 || static_cast<std::size_t>(target[0]) >= level.segments.size() || target[1] < 0 || target[1] >= 6) {
					block(true, "invalid geometric action target");
					break;
				}
				const auto wall = level.segments[target[0]].sides[target[1]].wall;
				if (wall < 0 || static_cast<std::size_t>(wall) >= level.walls.size()) {
					block(true, "wall action target lacks a wall");
					break;
				}
			}
		}
	}
	return result;
}

std::string blocker_message(const std::vector<Issue> &blockers)
{
	std::ostringstream message;
	for (std::size_t i = 0; i < blockers.size() && i < 4; ++i) {
		if (i) message << '\n';
		message << blockers[i].feature;
		if (blockers[i].record >= 0) message << " at record " << blockers[i].record;
		message << ": " << blockers[i].detail;
	}
	if (blockers.size() > 4) message << "\n"
		                             << blockers.size() - 4 << " additional unsupported requirements";
	return message.str();
}
} // namespace xl_compat
