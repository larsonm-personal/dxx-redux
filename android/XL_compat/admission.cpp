#include "admission.h"
#include <algorithm>
#include <sstream>

namespace xl_compat
{
std::vector<Issue> runtime_blockers(const Inventory &level, bool experimental_water)
{
	std::vector<Issue> result;
	for (const auto &issue : level.unsupported) {
		if (issue.feature == "object_triggers") continue;
		if (issue.feature == "object_trigger_type" && (issue.detail == "14" || issue.detail == "32" || issue.detail == "33")) continue;
		if (issue.feature == "trigger_type" && (issue.detail == "14" || issue.detail == "31" || issue.detail == "32" || issue.detail == "33")) continue;
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
	limit(!((level.level_version == 15 && level.game_version == 33) || (level.level_version == 17 && level.game_version == 36) || (level.level_version == 23 && level.game_version == 40)), "serialization", "requires audited level/game-data version 15/33, 17/36 or 23/40");
	limit(level.walls.size() > 255, "wall capacity", "more than 255 walls");
	limit(level.objects.size() > 1000, "object capacity", "more than 1000 objects");
	limit(level.segments.size() > 9000 || level.vertices.size() > 36000, "geometry capacity", "more than 9000 segments or 36000 vertices");
	limit(level.triggers.size() > 100, "trigger capacity", "more than 100 triggers");
	limit(level.matcens.size() > 20, "generator capacity", "more than 20 robot centers");
	const bool modern_water = level.level_version >= 21 && std::any_of(level.segments.begin(), level.segments.end(), [](const Segment &segment) { return (segment.properties & segment_water) != 0; });
	const bool fluid_actors = !level.matcens.empty() || std::any_of(level.objects.begin(), level.objects.end(), [](const Object &object) { return object.type == 2 || (object.contains_count > 0 && object.contains_type == 2); });
	limit(modern_water && fluid_actors, "water actors", "water with robots or robot generators is not qualified; robot-free singleplayer water is supported");
	limit(level.light_indices.size() > 500 || level.delta_lights.size() > 32000, "lighting capacity", "more than 500 light indices or 32000 delta lights");
	// Translation splits counts into signed-byte chunks, preserving zero-count records
	std::size_t translated_indices = 0;
	for (const auto &index : level.light_indices)
		translated_indices += index[2] ? (index[2] + 126) / 127 : 1;
	limit(translated_indices > 500, "changing-light capacity", "lossless translation requires more than 500 light indices");
	limit(level.flicker_lights.size() > 100, "flicker capacity", "more than 100 flickering lights");
	limit(level.object_triggers < 0 || static_cast<std::size_t>(level.object_triggers) != level.object_trigger_records.size(), "object triggers", "declared records and inspected owners differ");
	limit(level.object_trigger_records.size() > 255, "object trigger capacity", "more than 255 object-attached actions");
	for (std::size_t i = 0; i < level.object_trigger_records.size(); ++i) {
		const auto &record = level.object_trigger_records[i];
		const auto &action = record.action;
		const auto block = [&](bool condition, const char *detail) { if (condition) result.push_back({ "object triggers", static_cast<int>(i), detail }); };
		block(action.type == 31, "object master child context requires further reference/runtime qualification");
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
