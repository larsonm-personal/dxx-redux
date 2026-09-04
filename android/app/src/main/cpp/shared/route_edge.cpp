#include "route_edge.h"

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
		    !route_trigger_opens_path(snapshot.topology.triggers[trigger].kind) ||
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

bool side_has_active_fired_opener(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int segment,
    int side)
{
	if (!valid_segment(snapshot, segment) || side < 0 ||
	    side >= LEVEL_METADATA_MAX_SIDES)
		return false;
	for (const int source_wall :
	     snapshot.topology.segments[segment].sides[side].opener_walls) {
		if (!valid_wall(snapshot, source_wall))
			continue;
		const int trigger = snapshot.state.walls[source_wall].trigger;
		if (valid_trigger(snapshot, trigger) &&
		    !snapshot.state.triggers[trigger].disabled &&
		    route_trigger_opens_path(snapshot.topology.triggers[trigger].kind) &&
		    state_flag(progress.fired_triggers, trigger))
			return true;
	}
	return false;
}

bool edge_has_active_fired_opener(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int segment,
    int side,
    int child,
    int reverse_side)
{
	return side_has_active_fired_opener(
	           snapshot, progress, segment, side) ||
	       side_has_active_fired_opener(
	           snapshot, progress, child, reverse_side);
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
	result.progress_cost = LEVEL_METADATA_ROUTE_EDGE_PASSABLE;
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
	result.progress_cost = LEVEL_METADATA_ROUTE_EDGE_PROGRESS;
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

bool route_trigger_opens_path(route_trigger_kind kind)
{
	return kind == route_trigger_kind::open_door ||
	       kind == route_trigger_kind::toggle_door ||
	       kind == route_trigger_kind::open_wall ||
	       kind == route_trigger_kind::illusory_wall ||
	       kind == route_trigger_kind::illusion_off ||
	       kind == route_trigger_kind::unlock_door;
}

bool route_trigger_changes_navigation(route_trigger_kind kind)
{
	return route_trigger_opens_path(kind) ||
	       kind == route_trigger_kind::close_door ||
	       kind == route_trigger_kind::toggle_door ||
	       kind == route_trigger_kind::illusion_on ||
	       kind == route_trigger_kind::lock_door ||
	       kind == route_trigger_kind::close_wall;
}

route_wall_kind route_progress_wall_kind(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int wall)
{
	if (!valid_wall(snapshot, wall))
		return route_wall_kind::none;
	return wall < static_cast<int>(progress.wall_kinds.size())
	           ? progress.wall_kinds[wall]
	           : snapshot.state.walls[wall].kind;
}

bool route_progress_wall_locked(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int wall)
{
	if (!valid_wall(snapshot, wall))
		return false;
	return wall < static_cast<int>(progress.wall_locked.size())
	           ? progress.wall_locked[wall] != 0
	           : snapshot.state.walls[wall].locked;
}

bool route_progress_wall_opened(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int wall)
{
	if (!valid_wall(snapshot, wall))
		return false;
	return wall < static_cast<int>(progress.wall_opened.size())
	           ? progress.wall_opened[wall] != 0
	           : snapshot.state.walls[wall].opened;
}

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
	progress.consumed_one_shot_triggers.resize(snapshot.state.triggers.size());
	progress.trigger_in_progress.resize(snapshot.state.triggers.size());
	progress.avoided_triggers.resize(snapshot.state.triggers.size());
	for (const auto &wall : snapshot.state.walls) {
		progress.wall_kinds.push_back(wall.kind);
		progress.wall_locked.push_back(wall.locked ? 1 : 0);
		progress.wall_opened.push_back(wall.opened ? 1 : 0);
	}
	progress.opened_hidden_walls.resize(snapshot.state.walls.size());
	progress.destroyed_blastable_walls.resize(snapshot.state.walls.size());
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
	const int wall = topology_side.wall;
	const bool effective_wall_state =
	    valid_wall(snapshot, wall) &&
	    (state.wall_state_authoritative ||
	     snapshot.topology.walls[wall].shootable_trigger);
	const auto wall_kind = effective_wall_state
	                           ? route_progress_wall_kind(snapshot, state, wall)
	                       : valid_wall(snapshot, wall)
	                           ? snapshot.state.walls[wall].kind
	                           : route_wall_kind::none;
	const bool wall_locked =
	    effective_wall_state
	        ? route_progress_wall_locked(snapshot, state, wall)
	        : valid_wall(snapshot, wall) && snapshot.state.walls[wall].locked;
	const bool wall_opened =
	    effective_wall_state
	        ? route_progress_wall_opened(snapshot, state, wall)
	        : valid_wall(snapshot, wall) && snapshot.state.walls[wall].opened;
	const bool wall_changed =
	    effective_wall_state &&
	    (wall_kind != snapshot.state.walls[wall].kind ||
	     wall_locked != snapshot.state.walls[wall].locked ||
	     wall_opened != snapshot.state.walls[wall].opened);
	if (state_side.exit_trigger)
		return blocked(route_edge_blocker::exit);
	/* A shared auto-closing door can be open because it was entered through
	 * the opposite face while this face remains locked.  Treating that transient
	 * flyable bit as permanent lets the plan strand itself after the door closes. */
	if (state_side.flyable && !wall_changed &&
	    !(wall_kind == route_wall_kind::door && wall_locked))
		return passable(topology_side.wall);
	const auto &reverse_state_side =
	    snapshot.state.segments[child].sides[reverse_side];
	if (state.control_center_destroyed &&
	    (state_side.control_center_link ||
	     reverse_state_side.control_center_link))
		return passable(topology_side.wall);
	/* The fired-opener index is retained for legacy snapshots that do not carry
	 * effective wall state. It must never override a modeled close or lock. */
	if (!wall_changed && edge_has_active_fired_opener(
	                         snapshot, state, segment, side, child, reverse_side))
		return passable(topology_side.wall);
	if (!valid_wall(snapshot, wall))
		return passable();
	const auto &wall_state = snapshot.state.walls[wall];
	if (wall_opened || wall_kind == route_wall_kind::open ||
	    wall_kind == route_wall_kind::illusion)
		return passable(wall);
	const bool hidden_door = wall_state.kind == route_wall_kind::door &&
	                         wall_state.hidden && !wall_locked &&
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
	if (wall_kind == route_wall_kind::blastable &&
	    state_flag(state.destroyed_blastable_walls, wall))
		return passable(wall);
	if (wall_kind == route_wall_kind::blastable)
		return passable(wall, route_required_action::destroy_blastable_wall);
	const int key_mask = state.key_mask;
	if (wall_kind == route_wall_kind::door &&
	    key_allowed(wall_state.key, key_mask) && !wall_locked)
		return passable(wall);
	if (trigger >= 0 && !state_flag(state.avoided_triggers, trigger))
		return progress(route_edge_blocker::trigger,
		                route_required_action::activate_trigger,
		                wall, trigger);
	if (wall_kind == route_wall_kind::door && wall_locked)
		return blocked(route_edge_blocker::locked_door, wall);
	if (wall_kind == route_wall_kind::door &&
	    key_allowed(wall_state.key, key_mask))
		return passable(wall);
	if (wall_kind == route_wall_kind::door &&
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
