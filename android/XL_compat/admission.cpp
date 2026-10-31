#include "admission.h"
#include "capacity.h"
#include "producer_layout.h"
#include "fog.h"
#include "texture_change.h"
#include "countdown.h"
#include "ambient_sound.h"
#include "static_explosion.h"
#include "named_sound.h"
#include "particle_admission.h"
#include "skybox_render.h"
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
		// Wide flags stay source-owned; modern doors consume bit 8, higher bits are reserved
		if (modern_runtime_layout(level) && issue.feature == "wall_flags") continue;
		if (issue.feature == "effect_particles") continue;
		if (issue.feature == "effect_object_types" || issue.feature == "effect_sounds" ||
		    issue.feature == "effect_trigger_target" || issue.feature == "object_effect_trigger_target") continue;
		if (level.level_version == 17 && level.game_version == 36 && issue.feature == "segment_function" && issue.detail == "2") continue;
		if (modern_runtime_layout(level) && issue.feature == "segment_function_v21" && issue.detail == "2") continue;
		if (modern_runtime_layout(level) && issue.feature == "segment_function_v21" && issue.detail == "11") continue;
		if (issue.feature == "segment_function" && issue.detail == "11") continue;
		if (issue.feature == "segment_function_v21" && issue.detail == "9") continue;
		if (modern_runtime_layout(level) && issue.feature == "segment_function_v21" && issue.detail == "10") continue;
		if ((issue.feature == "trigger_type" || issue.feature == "object_trigger_type") && issue.detail == "15") continue;
		if ((issue.feature == "trigger_type" || issue.feature == "object_trigger_type") && issue.detail == "19") continue;
		if ((issue.feature == "trigger_type" || issue.feature == "object_trigger_type") && issue.detail == "25") continue;
		if ((issue.feature == "trigger_type" || issue.feature == "object_trigger_type") && issue.detail == "28") continue;
		if ((issue.feature == "trigger_type" || issue.feature == "object_trigger_type") && issue.detail == "29") continue;
		if ((issue.feature == "trigger_type" || issue.feature == "object_trigger_type") && issue.detail == "30") continue;
		if ((issue.feature == "trigger_type" || issue.feature == "object_trigger_type") && issue.detail == "34") continue;
		if ((issue.feature == "trigger_type" || issue.feature == "object_trigger_type") && issue.detail == "36") continue;
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
	for (std::size_t segment = 0; segment < level.segments.size(); ++segment)
		if (level.segments[segment].function == 10)
			for (int side = 0; side < 6; ++side)
				if (skybox_colored_only_boundary(level, int(segment), side))
					result.push_back({ "skybox colored boundary", int(segment), "untextured fog boundary on side " + std::to_string(side) + " requires source draw-state qualification" });
	for (unsigned index = 0; index < level.objects.size(); ++index) {
		const auto &object = level.objects[index];
		if (object.render == 11) {
			try {
				(void) prepare_authored_particle(level, object);
			} catch (const std::exception &error) {
				result.push_back({ "effect_particles", int(index), error.what() });
			}
		}
		if (object.render == 13) {
			try {
				if (!ambient_sound_shape(object)) throw std::runtime_error("requires a stationary, uncontrolled sound effect object");
				const auto sound = decode_ambient_sound(object);
				if (sound.volume < 0 || sound.filename.empty()) throw std::runtime_error("requires a nonnegative playback volume and a nonempty filename");
				(void) named_sound_filename(sound.filename + ".wav");
			} catch (const std::exception &error) {
				result.push_back({ "effect_sounds", int(index), error.what() });
			}
		}
		if (object.type == 19 && !static_explosion_shape(object))
			result.push_back({ "static_explosion", int(index), "requires a stationary single-player object with powerup control and vclip rendering (control 13, movement 0, render 5)" });
		if ((object.type == 18 || object.type == 20) && !ambient_sound_shape(object) && !authored_particle_shape(object))
			result.push_back({ "effect_object_types", int(index), "object type " + std::to_string(object.type) + " requires unsupported effect behavior" });
	}
	auto limit = [&](bool exceeded, const char *feature, const char *detail) {
		if (exceeded) result.push_back({ feature, -1, detail });
	};
	limit(!((level.level_version == 15 && (level.game_version == 33 || level.game_version == 34)) || (level.level_version == 17 && level.game_version == 36) || modern_runtime_layout(level)), "serialization", "requires audited level/game-data version 15/33, 15/34, 17/36, 23/40, 25/40 or 27/40");
	limit(level.level_version >= 27 && volumetric_fog_required(level) && level.fog_settings.size() != 4, "authored fog", "requires four complete authored color/distance records and a capable OpenGL renderer");
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
	limit(countdown_total(0, level.countdown, 0) < 0, "reactor countdown", "difficulty-adjusted duration must fit 0..32767 whole seconds");
	const auto countdown_action = [&](const Trigger &action, int index, const char *kind) {
		if (action.type == 25 && countdown_total(action.time, level.countdown, 0) < 0)
			result.push_back({ kind, index, "countdown duration must fit 0..32767 whole seconds; action time is seconds, not delay milliseconds" });
	};
	for (std::size_t i = 0; i < level.triggers.size(); ++i) countdown_action(level.triggers[i], int(i), "trigger countdown");
	for (std::size_t i = 0; i < level.object_trigger_records.size(); ++i) countdown_action(level.object_trigger_records[i].action, int(i), "object trigger countdown");
	const auto seismic_action = [&](const Trigger &action, int index, const char *kind) {
		if (action.type == 36 && action.time > 32767)
			result.push_back({ kind, index, "mine-shake duration must fit 0..32767 whole seconds; negative time selects ten seconds, not a randomized delay" });
	};
	for (std::size_t i = 0; i < level.triggers.size(); ++i) seismic_action(level.triggers[i], int(i), "trigger seismic");
	for (std::size_t i = 0; i < level.object_trigger_records.size(); ++i) seismic_action(level.object_trigger_records[i].action, int(i), "object trigger seismic");
	const auto texture_action = [&](const Trigger &action, int index, const char *kind) {
		if (action.type != 19 || action.targets.empty()) return;
		const auto change = texture_change(action.value);
		if (change.base >= 910 || change.overlay >= 910)
			result.push_back({ kind, index, "texture change references an unsupported base/overlay ID " + std::to_string(change.base) + "/" + std::to_string(change.overlay) });
	};
	for (std::size_t i = 0; i < level.triggers.size(); ++i) texture_action(level.triggers[i], int(i), "trigger texture");
	for (std::size_t i = 0; i < level.object_trigger_records.size(); ++i) texture_action(level.object_trigger_records[i].action, int(i), "object trigger texture");
	limit(level.matcens.size() > 20, "generator capacity", "more than 20 robot centers");
	limit(level.equipment.size() > 128, "equipment capacity", "more than 128 equipment makers");
	limit(!level.equipment.empty() && !modern_runtime_layout(level), "equipment layout", "equipment makers require an audited modern 23/40, 25/40 or 27/40 profile");
	for (std::size_t row = 0; row < level.equipment.size(); ++row)
		for (unsigned id = 0; id < 50; ++id)
			if ((std::uint32_t(level.equipment[row][id / 32]) & (std::uint32_t(1) << (id % 32))) &&
			    (id == 7 || id == 8 || id == 9 || id == 24 || id == 26 || id == 27 || id >= 46))
				result.push_back({ "equipment item", static_cast<int>(row), "powerup " + std::to_string(id) + " requires unsupported multiplayer, addon or release pickup behavior" });
	const auto producers = producer_layout(level);
	limit(producers.robots.size() != level.matcens.size(), "generator assignment", "robotmaker segments and disk rows differ after source initialization");
	limit(producers.equipment.size() != level.equipment.size(), "equipment assignment", "equipment segments and disk rows differ after source initialization");
	int stations = 0;
	for (std::size_t i = 0; i < level.segments.size(); ++i) {
		const int function = producer_function(level.segments[i]);
		if ((function >= 1 && function <= 4) || (modern_runtime_layout(level) && function == 11)) ++stations;
	}
	limit(stations > 128, "generator assignment", "special-segment station links exceed the native signed-byte range");
	for (std::size_t i = 0; i < level.flicker_lights.size(); ++i)
		if (level.flicker_lights[i][4] < 0) result.push_back({ "flicker delay", static_cast<int>(i), "negative delay cannot advance the authored flicker clock" });
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
				// Source DoLink continues after ToggleWall ignores an absent wall
				if (action.type == 0 && wall == -1) continue;
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
