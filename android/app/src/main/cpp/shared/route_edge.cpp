#include "route_edge.h"
#include "route_edge_c.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

namespace dxx_route
{
namespace
{

bool valid_segment(const route_snapshot &snapshot, int segment)
{
	return segment >= 0 &&
	       segment < static_cast<int>(snapshot.topology.segments.size());
}

bool valid_wall(const route_snapshot &snapshot, int wall)
{
	return wall >= 0 && wall < static_cast<int>(snapshot.state.walls.size());
}

bool valid_trigger(const route_snapshot &snapshot, int trigger)
{
	return trigger >= 0 &&
	       trigger < static_cast<int>(snapshot.topology.triggers.size()) &&
	       trigger < static_cast<int>(snapshot.state.triggers.size());
}

bool trigger_makes_progress(route_trigger_kind kind)
{
	return kind == route_trigger_kind::open_door ||
	       kind == route_trigger_kind::open_wall ||
	       kind == route_trigger_kind::illusory_wall ||
	       kind == route_trigger_kind::illusion_off ||
	       kind == route_trigger_kind::unlock_door;
}

bool state_flag(const std::vector<unsigned char> &values, int index)
{
	return index >= 0 && index < static_cast<int>(values.size()) &&
	       values[index] != 0;
}

int side_progress_trigger(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int segment,
    int side)
{
	if (!valid_segment(snapshot, segment) || side < 0 ||
	    side >= LEVEL_METADATA_MAX_SIDES)
		return -1;
	const auto &topology_side = snapshot.topology.segments[segment].sides[side];
	for (const int source_wall : topology_side.opener_walls) {
		if (!valid_wall(snapshot, source_wall))
			continue;
		const int trigger = snapshot.state.walls[source_wall].trigger;
		if (!valid_trigger(snapshot, trigger) ||
		    snapshot.state.triggers[trigger].disabled ||
		    !trigger_makes_progress(snapshot.topology.triggers[trigger].kind) ||
		    state_flag(progress.fired_triggers, trigger))
			continue;
		return trigger;
	}
	return -1;
}

int edge_progress_trigger(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int segment,
    int side,
    int child,
    int reverse_side)
{
	const int direct = side_progress_trigger(snapshot, progress, segment, side);
	return direct >= 0 ? direct : side_progress_trigger(snapshot, progress, child, reverse_side);
}

bool side_has_fired_trigger(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int segment,
    int side)
{
	if (!valid_segment(snapshot, segment) || side < 0 ||
	    side >= LEVEL_METADATA_MAX_SIDES)
		return false;
	for (const int source_wall : snapshot.topology.segments[segment].sides[side].opener_walls) {
		if (!valid_wall(snapshot, source_wall))
			continue;
		const int trigger = snapshot.state.walls[source_wall].trigger;
		if (valid_trigger(snapshot, trigger) &&
		    !snapshot.state.triggers[trigger].disabled &&
		    trigger_makes_progress(snapshot.topology.triggers[trigger].kind) &&
		    state_flag(progress.fired_triggers, trigger))
			return true;
	}
	return false;
}

bool edge_has_fired_trigger(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int segment,
    int side,
    int child,
    int reverse_side)
{
	return side_has_fired_trigger(snapshot, progress, segment, side) ||
	       side_has_fired_trigger(snapshot, progress, child, reverse_side);
}

bool key_allowed(route_key_requirement key, int key_mask)
{
	switch (key) {
		case route_key_requirement::none: return true;
		case route_key_requirement::blue:
			return (key_mask & LEVEL_METADATA_KEY_MASK_BLUE) != 0;
		case route_key_requirement::red:
			return (key_mask & LEVEL_METADATA_KEY_MASK_RED) != 0;
		case route_key_requirement::gold:
			return (key_mask & LEVEL_METADATA_KEY_MASK_GOLD) != 0;
		default: return false;
	}
}

int key_bit(route_key_requirement key)
{
	switch (key) {
		case route_key_requirement::blue: return LEVEL_METADATA_KEY_MASK_BLUE;
		case route_key_requirement::red: return LEVEL_METADATA_KEY_MASK_RED;
		case route_key_requirement::gold: return LEVEL_METADATA_KEY_MASK_GOLD;
		default: return 0;
	}
}

route_edge_decision passable(
    int wall = -1,
    route_required_action action = route_required_action::none)
{
	route_edge_decision result;
	result.legacy_cost = LEVEL_METADATA_ROUTE_EDGE_PASSABLE;
	result.blocker = route_edge_blocker::none;
	result.action = action;
	result.wall = wall;
	return result;
}

route_edge_decision progress(
    route_edge_blocker blocker,
    route_required_action action,
    int wall,
    int trigger = -1,
    route_key_requirement key = route_key_requirement::none)
{
	route_edge_decision result;
	result.legacy_cost = LEVEL_METADATA_ROUTE_EDGE_PROGRESS;
	result.blocker = blocker;
	result.action = action;
	result.wall = wall;
	result.trigger = trigger;
	result.key = key;
	return result;
}

route_edge_decision blocked(route_edge_blocker blocker, int wall = -1)
{
	route_edge_decision result;
	result.blocker = blocker;
	result.wall = wall;
	return result;
}

} // namespace

route_edge_decision evaluate_route_edge(
    const route_snapshot &snapshot,
    const route_query &query,
    int segment,
    int side)
{
	route_progress_state progress;
	progress.current_segment = snapshot.state.start_segment;
	progress.current_position = query.start.valid ? query.start : snapshot.state.start_position;
	progress.key_mask = query.progression.key_mask;
	progress.control_center_destroyed = snapshot.state.control_center_destroyed;
	progress.fired_triggers.resize(snapshot.state.triggers.size());
	progress.avoided_triggers.resize(snapshot.state.triggers.size());
	progress.opened_hidden_walls.resize(snapshot.state.walls.size());
	return evaluate_route_edge(snapshot, query, progress, segment, side);
}

route_edge_decision evaluate_route_edge(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &state,
    int segment,
    int side)
{
	return evaluate_route_edge(
	    snapshot, query, state, route_key_requirement::none, segment, side);
}

route_edge_decision evaluate_route_edge(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &state,
    route_key_requirement forbidden_missing_key,
    int segment,
    int side)
{
	if (!valid_segment(snapshot, segment) || side < 0 ||
	    side >= LEVEL_METADATA_MAX_SIDES)
		return blocked(route_edge_blocker::invalid_topology);
	const auto &topology_side = snapshot.topology.segments[segment].sides[side];
	const int child = topology_side.child;
	const int reverse_side = topology_side.reverse_side;
	if (!valid_segment(snapshot, child) || reverse_side < 0 ||
	    reverse_side >= LEVEL_METADATA_MAX_SIDES ||
	    snapshot.topology.segments[child].sides[reverse_side].child != segment)
		return blocked(route_edge_blocker::invalid_topology);
	const auto &state_side = snapshot.state.segments[segment].sides[side];
	if (state_side.exit_trigger)
		return blocked(route_edge_blocker::exit);
	if (state_side.flyable)
		return passable(topology_side.wall);
	const auto &reverse_state_side =
	    snapshot.state.segments[child].sides[reverse_side];
	if (state.control_center_destroyed &&
	    (state_side.control_center_link ||
	     reverse_state_side.control_center_link))
		return passable(topology_side.wall);
	if (edge_has_fired_trigger(
	        snapshot, state, segment, side, child, reverse_side))
		return passable(topology_side.wall);
	const int wall = topology_side.wall;
	if (!valid_wall(snapshot, wall))
		return passable();
	const auto &wall_state = snapshot.state.walls[wall];
	if (wall_state.opened)
		return passable(wall);
	const bool hidden_door = wall_state.kind == route_wall_kind::door &&
	                         wall_state.hidden && !wall_state.locked &&
	                         wall_state.key == route_key_requirement::none;
	if (hidden_door && state_flag(state.opened_hidden_walls, wall))
		return passable(wall);
	if (hidden_door)
		return progress(route_edge_blocker::hidden_door,
		                route_required_action::open_hidden_door, wall);
	const int trigger = edge_progress_trigger(
	    snapshot, state, segment, side, child, reverse_side);
	if (state_side.hard_blocked) {
		if (trigger >= 0 && !state_flag(state.avoided_triggers, trigger))
			return progress(route_edge_blocker::trigger,
			                route_required_action::activate_trigger,
			                wall, trigger);
		auto result = blocked(route_edge_blocker::hard_blocked, wall);
		if (query.navigator.companion)
			result.action = route_required_action::wait_for_player;
		return result;
	}
	if (wall_state.kind == route_wall_kind::open ||
	    wall_state.kind == route_wall_kind::illusion)
		return passable(wall);
	if (wall_state.kind == route_wall_kind::blastable)
		return passable(wall, route_required_action::destroy_blastable_wall);
	const int key_mask = state.key_mask;
	if (wall_state.kind == route_wall_kind::door &&
	    key_allowed(wall_state.key, key_mask) && !wall_state.locked)
		return passable(wall);
	if (trigger >= 0 && !state_flag(state.avoided_triggers, trigger))
		return progress(route_edge_blocker::trigger,
		                route_required_action::activate_trigger,
		                wall, trigger);
	if (wall_state.kind == route_wall_kind::door && wall_state.locked)
		return blocked(route_edge_blocker::locked_door, wall);
	if (wall_state.kind == route_wall_kind::door &&
	    key_allowed(wall_state.key, key_mask))
		return passable(wall);
	if (wall_state.kind == route_wall_kind::door &&
	    wall_state.key != route_key_requirement::none &&
	    wall_state.key != route_key_requirement::unknown &&
	    wall_state.key != forbidden_missing_key &&
	    ((state.avoided_key_mask | state.key_in_progress) &
	     key_bit(wall_state.key)) == 0)
		return progress(route_edge_blocker::missing_key,
		                route_required_action::acquire_key,
		                wall, -1, wall_state.key);
	return blocked(route_edge_blocker::closed_wall, wall);
}

} // namespace dxx_route

namespace
{

void copy_problem(char *out, int capacity, const char *problem)
{
	if (out && capacity > 0)
		std::snprintf(out, static_cast<std::size_t>(capacity), "%s",
		              problem ? problem : "");
}

} // namespace

extern "C" int route_edge_compare_view(
    const level_metadata_scan_view *view,
    route_edge_shadow_summary *summary,
    char *problem,
    int problem_capacity)
{
	if (summary) {
		std::memset(summary, 0, sizeof(*summary));
		summary->first_mismatch_segment = -1;
		summary->first_mismatch_side = -1;
	}
	copy_problem(problem, problem_capacity, "");
	if (!view || !summary) {
		copy_problem(problem, problem_capacity,
		             "route edge comparison requires input and output");
		return 0;
	}
	try {
		dxx_route::route_snapshot snapshot;
		std::string detail;
		if (!dxx_route::build_route_snapshot(*view, snapshot, &detail)) {
			copy_problem(problem, problem_capacity, detail.c_str());
			return 0;
		}
		dxx_route::route_query query;
		query.start = snapshot.state.start_position;
		query.progression.key_mask = snapshot.state.key_mask;
		query.navigator.companion = false;
		for (int segment = 0; segment < view->num_segments; ++segment) {
			for (int side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
				const int legacy = level_metadata_scan_route_edge_cost(
				    view, segment, side);
				const int shared = dxx_route::evaluate_route_edge(
				                       snapshot, query, segment, side)
				                       .legacy_cost;
				summary->compared_edge_count++;
				if (legacy == shared)
					continue;
				if (summary->mismatch_count == 0) {
					summary->first_mismatch_segment = segment;
					summary->first_mismatch_side = side;
					summary->first_legacy_cost = legacy;
					summary->first_shared_cost = shared;
				}
				summary->mismatch_count++;
			}
		}
		return 1;
	} catch (const std::exception &error) {
		copy_problem(problem, problem_capacity, error.what());
	} catch (...) {
		copy_problem(problem, problem_capacity,
		             "unknown route edge comparison failure");
	}
	return 0;
}
