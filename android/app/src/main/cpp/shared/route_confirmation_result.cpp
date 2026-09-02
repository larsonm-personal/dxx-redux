#include "route_confirmation_result.h"

#include <cmath>
#include <cstdio>
#include <fstream>

#include <nlohmann/json.hpp>

#include "route_confirmation.h"

extern "C" {
#include "game.h"
#include "level_metadata_scan.h"
#include "maths.h"
}

namespace
{
double fixed_seconds(int64_t ticks)
{
	const double seconds =
	    static_cast<double>(ticks) / static_cast<double>(F1_0);
	return std::round(seconds * 1000000.0) / 1000000.0;
}

nlohmann::ordered_json serialize_result(const route_confirmation_summary &summary,
                                        const char *mission, int level)
{
	nlohmann::ordered_json result;
	nlohmann::ordered_json objectives = nlohmann::ordered_json::array();
	nlohmann::ordered_json objective_seconds = nlohmann::ordered_json::array();
	result["schema"] = "dxx-guidebot-route-confirmation-v1";
	result["mission"] = mission && *mission ? mission : "d2";
	result["level"] = level;
	result["status"] = route_confirmation_status_name(summary.status);
	result["seed"] = summary.seed;
	result["fixed_hz"] = summary.fixed_hz;
	result["difficulty"] = Difficulty_level;
	result["frames"] = summary.frame_count;
	result["simulation_seconds"] = fixed_seconds(summary.elapsed_ticks);
	result["rng_start"] = {
		{ "simulation",
		  { { "state", summary.rng_start.simulation.state },
		    { "calls", summary.rng_start.simulation.call_count } } },
		{ "effects",
		  { { "state", summary.rng_start.effects.state },
		    { "calls", summary.rng_start.effects.call_count } } }
	};
	result["rng_end"] = {
		{ "simulation",
		  { { "state", summary.rng_end.simulation.state },
		    { "calls", summary.rng_end.simulation.call_count } } },
		{ "effects",
		  { { "state", summary.rng_end.effects.state },
		    { "calls", summary.rng_end.effects.call_count } } }
	};
	result["radius"] = {
		{ "player", summary.player_radius },
		{ "guidebot", summary.guidebot_radius },
		{ "effective", summary.effective_radius }
	};
	if (summary.problem[0])
		result["problem"] = summary.problem;
	for (int index = 0; index < summary.objective_count; ++index) {
		const route_confirmation_objective_result &objective =
		    summary.objectives[index];
		nlohmann::ordered_json item;
		item["route_step_index"] = objective.route_step_index;
		item["kind"] = level_metadata_route_step_kind_name(objective.kind);
		item["kind_id"] = objective.kind;
		item["activation_kind"] =
		    level_metadata_route_activation_kind_name(objective.activation_kind);
		item["activation_kind_id"] = objective.activation_kind;
		item["label"] = objective.label;
		item["frame"] = objective.completed_frame;
		item["seconds"] = fixed_seconds(objective.completed_ticks);
		objectives.push_back(item);
		objective_seconds.push_back(fixed_seconds(objective.completed_ticks));
	}
	result["objectives"] = objectives;
	if (summary.status == ROUTE_CONFIRMATION_CONFIRMED) {
		result["route_confirmation"] = {
			{ "status", "confirmed" },
			{ "generation", 1 },
			{ "seed", ROUTE_CONFIRMATION_CANONICAL_SEED },
			{ "fixed_hz", ROUTE_CONFIRMATION_FIXED_HZ },
			{ "objective_seconds", objective_seconds },
			{ "total_seconds", fixed_seconds(summary.elapsed_ticks) }
		};
	}
	return result;
}
} // namespace

extern "C" int route_confirmation_write_json(const char *path,
                                             const char *mission, int level,
                                             char *error, size_t error_size)
{
	const route_confirmation_summary *summary =
	    route_confirmation_get_summary();
	std::ofstream stream(path ? path : "");
	if (!stream) {
		if (error && error_size)
			snprintf(error, error_size, "could not open output %s",
			         path ? path : "");
		return 0;
	}
	stream << serialize_result(*summary, mission, level).dump(2) << "\n";
	if (!stream) {
		if (error && error_size)
			snprintf(error, error_size, "could not write output %s",
			         path ? path : "");
		return 0;
	}
	return 1;
}
