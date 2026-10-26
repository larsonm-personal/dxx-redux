#include "admission.h"
#include <algorithm>
#include <sstream>

namespace xl_compat
{
std::vector<Issue> runtime_blockers(const Inventory &level, bool experimental_water)
{
	std::vector<Issue> result;
	for (const auto &issue : level.unsupported) {
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
	limit(level.object_triggers != 0, "object triggers", "object-attached triggers are not implemented");
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
