#include "route_planner.h"
#include "route_planner_c.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>

namespace dxx_route
{
namespace
{

double point_distance(const route_position &left, const route_position &right)
{
	const double dx = (static_cast<double>(left.value[0]) - right.value[0]) /
	                  LEVEL_METADATA_FIX_SCALE;
	const double dy = (static_cast<double>(left.value[1]) - right.value[1]) /
	                  LEVEL_METADATA_FIX_SCALE;
	const double dz = (static_cast<double>(left.value[2]) - right.value[2]) /
	                  LEVEL_METADATA_FIX_SCALE;
	return std::sqrt(dx * dx + dy * dy + dz * dz);
}

class route_heap
{
  public:
	route_heap(std::vector<route_search_node> &nodes, bool progress_cost)
	    : nodes_(nodes), positions_(nodes.size(), 0), progress_cost_(progress_cost)
	{
		heap_.push_back(-1);
	}

	bool empty() const
	{
		return heap_.size() == 1;
	}

	void push(int segment)
	{
		heap_.push_back(segment);
		positions_[segment] = static_cast<int>(heap_.size()) - 1;
		sift_up(positions_[segment]);
	}

	int pop()
	{
		const int result = heap_[1];
		positions_[result] = 0;
		if (heap_.size() == 2) {
			heap_.pop_back();
			return result;
		}
		heap_[1] = heap_.back();
		positions_[heap_[1]] = 1;
		heap_.pop_back();
		sift_down(1);
		return result;
	}

	void decrease(int segment)
	{
		if (positions_[segment] > 0)
			sift_up(positions_[segment]);
	}

	bool contains(int segment) const
	{
		return positions_[segment] > 0;
	}

  private:
	bool less(int left, int right) const
	{
		if (progress_cost_ &&
		    nodes_[left].progress_weight != nodes_[right].progress_weight)
			return nodes_[left].progress_weight < nodes_[right].progress_weight;
		return nodes_[left].distance < nodes_[right].distance;
	}

	void swap(int left, int right)
	{
		const int value = heap_[left];
		heap_[left] = heap_[right];
		heap_[right] = value;
		positions_[heap_[left]] = left;
		positions_[heap_[right]] = right;
	}

	void sift_up(int index)
	{
		while (index > 1) {
			const int parent = index / 2;
			if (!less(heap_[index], heap_[parent]))
				break;
			swap(parent, index);
			index = parent;
		}
	}

	void sift_down(int index)
	{
		for (;;) {
			const int left = index * 2;
			const int right = left + 1;
			int smallest = index;
			if (left < static_cast<int>(heap_.size()) &&
			    less(heap_[left], heap_[smallest]))
				smallest = left;
			if (right < static_cast<int>(heap_.size()) &&
			    less(heap_[right], heap_[smallest]))
				smallest = right;
			if (smallest == index)
				break;
			swap(index, smallest);
			index = smallest;
		}
	}

	std::vector<route_search_node> &nodes_;
	std::vector<int> heap_;
	std::vector<int> positions_;
	bool progress_cost_;
};

bool valid_segment(const route_snapshot &snapshot, int segment)
{
	return segment >= 0 &&
	       segment < static_cast<int>(snapshot.topology.segments.size());
}

bool valid_wall(const route_snapshot &snapshot, int wall)
{
	return wall >= 0 && wall < static_cast<int>(snapshot.state.walls.size()) &&
	       wall < static_cast<int>(snapshot.topology.walls.size());
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

bool trigger_source_wall_valid(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int wall)
{
	if (!valid_wall(snapshot, wall))
		return false;
	const int trigger = snapshot.state.walls[wall].trigger;
	return valid_trigger(snapshot, trigger) &&
	       !snapshot.state.triggers[trigger].disabled &&
	       route_trigger_opens_path(snapshot.topology.triggers[trigger].kind) &&
	       !state_flag(progress.fired_triggers, trigger);
}

bool side_has_trigger_source(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int segment,
    int side)
{
	if (!valid_segment(snapshot, segment) || side < 0 ||
	    side >= LEVEL_METADATA_MAX_SIDES)
		return false;
	for (const int wall : snapshot.topology.segments[segment].sides[side].opener_walls)
		if (trigger_source_wall_valid(snapshot, progress, wall))
			return true;
	return false;
}

route_position weighted_position(
    const route_position &first,
    const route_position &second,
    int second_weight)
{
	route_position result;
	if (!first.valid || !second.valid)
		return result;
	const int total = 1 + second_weight;
	for (int coordinate = 0; coordinate < 3; ++coordinate)
		result.value[coordinate] = static_cast<int>(
		    (static_cast<long long>(first.value[coordinate]) +
		     static_cast<long long>(second.value[coordinate]) * second_weight) /
		    total);
	result.valid = true;
	return result;
}

bool source_visible_from_position(
    const route_snapshot &snapshot,
    const route_trigger_source &source,
    const route_visibility_query &visibility,
    int segment,
    const route_position &position)
{
	if (valid_wall(snapshot, source.source_wall) && visibility.wall_visible)
		return visibility.wall_visible(
		    visibility.user, segment, position, source.source_wall);
	return visibility.target_visible &&
	       visibility.target_visible(
	           visibility.user, segment, position, source.source_segment,
	           source.source_position);
}

bool visible_source_position(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    const route_trigger_source &source,
    const route_visibility_query &visibility,
    int segment,
    route_position &position,
    double &extra_distance)
{
	static constexpr int sample_weights[] = { 3, 7, 15 };
	static constexpr int side_vertices[LEVEL_METADATA_MAX_SIDES][4] = {
		{ 7, 6, 2, 3 },
		{ 0, 4, 7, 3 },
		{ 0, 1, 5, 4 },
		{ 2, 6, 5, 1 },
		{ 4, 5, 6, 7 },
		{ 3, 2, 1, 0 },
	};
	if (!valid_segment(snapshot, segment))
		return false;
	const auto &topology_segment = snapshot.topology.segments[segment];
	if (segment == progress.current_segment && progress.current_position.valid &&
	    source_visible_from_position(
	        snapshot, source, visibility, segment, progress.current_position)) {
		position = progress.current_position;
		extra_distance = 0.0;
		return true;
	}
	if (!topology_segment.center.valid)
		return false;
	if (source_visible_from_position(
	        snapshot, source, visibility, segment, topology_segment.center)) {
		position = topology_segment.center;
		extra_distance = 0.0;
		return true;
	}
	for (int side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
		const auto &side_center = topology_segment.sides[side].center;
		if (!side_center.valid)
			continue;
		for (const int weight : sample_weights) {
			const auto candidate = weighted_position(
			    topology_segment.center, side_center, weight);
			if (!source_visible_from_position(
			        snapshot, source, visibility, segment, candidate))
				continue;
			position = candidate;
			extra_distance = point_distance(topology_segment.center, candidate);
			return true;
		}
	}
	for (const auto &vertex : topology_segment.vertices) {
		if (!vertex.valid)
			continue;
		for (const int weight : sample_weights) {
			const auto candidate = weighted_position(
			    topology_segment.center, vertex, weight);
			if (!source_visible_from_position(
			        snapshot, source, visibility, segment, candidate))
				continue;
			position = candidate;
			extra_distance = point_distance(topology_segment.center, candidate);
			return true;
		}
	}
	for (int side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
		for (int edge = 0; edge < 4; ++edge) {
			const auto &first =
			    topology_segment.vertices[side_vertices[side][edge]];
			const auto &second = topology_segment.vertices
			                         [side_vertices[side][(edge + 1) % 4]];
			if (!first.valid || !second.valid)
				continue;
			route_position midpoint;
			midpoint.valid = true;
			for (int coordinate = 0; coordinate < 3; ++coordinate)
				midpoint.value[coordinate] =
				    (first.value[coordinate] + second.value[coordinate]) / 2;
			for (const int weight : sample_weights) {
				const auto candidate = weighted_position(
				    topology_segment.center, midpoint, weight);
				if (!source_visible_from_position(
				        snapshot, source, visibility, segment, candidate))
					continue;
				position = candidate;
				extra_distance = point_distance(
				    topology_segment.center, candidate);
				return true;
			}
		}
	}
	return false;
}

std::vector<route_trigger_source> discover_trigger_sources_internal(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int segment,
    int side,
    bool include_in_progress)
{
	std::vector<route_trigger_source> result;
	if (!valid_segment(snapshot, segment) || side < 0 ||
	    side >= LEVEL_METADATA_MAX_SIDES)
		return result;
	const auto &requested_side = snapshot.topology.segments[segment].sides[side];
	const int child = requested_side.child;
	if (!valid_segment(snapshot, child))
		return result;
	int target_segment = segment;
	int target_side = side;
	if (!side_has_trigger_source(snapshot, progress, target_segment, target_side)) {
		target_segment = child;
		target_side = requested_side.reverse_side;
		if (!side_has_trigger_source(
		        snapshot, progress, target_segment, target_side))
			return result;
	}
	const auto &target =
	    snapshot.topology.segments[target_segment].sides[target_side];
	for (const int source_wall : target.opener_walls) {
		if (!trigger_source_wall_valid(snapshot, progress, source_wall))
			continue;
		const int trigger = snapshot.state.walls[source_wall].trigger;
		if (!include_in_progress &&
		    state_flag(progress.trigger_in_progress, trigger))
			continue;
		const auto &source_topology = snapshot.topology.walls[source_wall];
		route_position source_position = source_topology.target;
		if (!source_position.valid &&
		    valid_segment(snapshot, source_topology.segment))
			source_position =
			    snapshot.topology.segments[source_topology.segment].center;
		if (!source_position.valid)
			continue;
		route_trigger_source source;
		source.target_segment = target_segment;
		source.target_side = target_side;
		source.target_wall = target.wall;
		source.source_wall = source_wall;
		source.source_segment = source_topology.segment;
		source.source_side = source_topology.side;
		source.trigger = trigger;
		source.trigger_kind = snapshot.topology.triggers[trigger].kind;
		source.source_position = source_position;
		result.push_back(source);
	}
	return result;
}

int key_index(route_key_requirement key)
{
	switch (key) {
		case route_key_requirement::blue: return 0;
		case route_key_requirement::red: return 1;
		case route_key_requirement::gold: return 2;
		default: return -1;
	}
}

} // namespace

route_progress_state initial_route_progress_state(
    const route_snapshot &snapshot,
    const route_query &query)
{
	route_progress_state result;
	result.current_segment = snapshot.state.start_segment;
	result.current_position = query.start.valid
	                              ? query.start
	                              : snapshot.state.start_position;
	result.key_mask = query.progression.key_mask;
	result.control_center_destroyed = snapshot.state.control_center_destroyed;
	result.fired_triggers.resize(snapshot.state.triggers.size());
	result.trigger_in_progress.resize(snapshot.state.triggers.size());
	result.avoided_triggers.resize(snapshot.state.triggers.size());
	result.opened_hidden_walls.resize(snapshot.state.walls.size());
	return result;
}

bool route_progress_acquire_key(
    route_progress_state &progress,
    route_key_requirement key)
{
	const int index = key_index(key);
	if (index < 0)
		return false;
	progress.key_mask |= 1 << index;
	return true;
}

bool route_progress_fire_trigger(route_progress_state &progress, int trigger)
{
	if (trigger < 0 || trigger >= static_cast<int>(progress.fired_triggers.size()))
		return false;
	progress.fired_triggers[trigger] = 1;
	return true;
}

bool route_progress_open_hidden_wall(
    const route_snapshot &snapshot,
    route_progress_state &progress,
    int wall)
{
	if (wall < 0 || wall >= static_cast<int>(snapshot.topology.walls.size()) ||
	    wall >= static_cast<int>(progress.opened_hidden_walls.size()))
		return false;
	progress.opened_hidden_walls[wall] = 1;
	const auto &source = snapshot.topology.walls[wall];
	if (!valid_segment(snapshot, source.segment) || source.side < 0 ||
	    source.side >= LEVEL_METADATA_MAX_SIDES)
		return true;
	const auto &side = snapshot.topology.segments[source.segment].sides[source.side];
	if (!valid_segment(snapshot, side.child) || side.reverse_side < 0 ||
	    side.reverse_side >= LEVEL_METADATA_MAX_SIDES)
		return true;
	const int reverse_wall =
	    snapshot.topology.segments[side.child].sides[side.reverse_side].wall;
	if (reverse_wall >= 0 &&
	    reverse_wall < static_cast<int>(progress.opened_hidden_walls.size()))
		progress.opened_hidden_walls[reverse_wall] = 1;
	return true;
}

route_search_result search_routes(
    const route_snapshot &snapshot,
    const route_query &query,
    bool optimistic)
{
	return search_routes(
	    snapshot, query, initial_route_progress_state(snapshot, query), optimistic);
}

route_search_result search_routes(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    bool optimistic)
{
	route_search_options options;
	options.optimistic = optimistic;
	return search_routes(snapshot, query, progress, options);
}

route_search_result search_routes(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    const route_search_options &options)
{
	route_search_result result;
	result.start_segment = progress.current_segment;
	const int count = static_cast<int>(snapshot.topology.segments.size());
	result.nodes.resize(count);
	if (!valid_segment(snapshot, result.start_segment)) {
		result.problem = "route start segment is invalid";
		return result;
	}
	const auto &start_center = snapshot.topology.segments[result.start_segment].center;
	const route_position &start = progress.current_position;
	if (!start.valid || !start_center.valid) {
		result.problem = "route start position is invalid";
		return result;
	}
	const double infinity = std::numeric_limits<double>::infinity();
	const int max_progress = std::numeric_limits<int>::max();
	for (auto &node : result.nodes) {
		node.distance = infinity;
		node.progress_weight = max_progress;
	}
	std::vector<unsigned char> closed(count, 0);
	auto &start_node = result.nodes[result.start_segment];
	start_node.reachable = true;
	start_node.distance = point_distance(start, start_center);
	start_node.progress_weight = 0;
	route_heap heap(result.nodes, options.prioritize_progress);
	heap.push(result.start_segment);
	while (!heap.empty()) {
		const int current = heap.pop();
		result.visit_order.push_back(current);
		closed[current] = 1;
		for (int side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			const int child = snapshot.topology.segments[current].sides[side].child;
			if (!valid_segment(snapshot, child) || closed[child])
				continue;
			const auto edge = evaluate_route_edge(
			    snapshot, query, progress, options.forbidden_missing_key,
			    current, side);
			if (edge.legacy_cost == LEVEL_METADATA_ROUTE_EDGE_BLOCKED ||
			    (!options.optimistic &&
			     edge.legacy_cost == LEVEL_METADATA_ROUTE_EDGE_PROGRESS))
				continue;
			const auto &current_center = snapshot.topology.segments[current].center;
			const auto &child_center = snapshot.topology.segments[child].center;
			if (!current_center.valid || !child_center.valid)
				continue;
			const double distance = result.nodes[current].distance +
			                        point_distance(current_center, child_center);
			const int progress = result.nodes[current].progress_weight +
			                     (edge.legacy_cost == LEVEL_METADATA_ROUTE_EDGE_PROGRESS ? 1 : 0);
			auto &node = result.nodes[child];
			if (options.prioritize_progress &&
			    progress > node.progress_weight)
				continue;
			if ((!options.prioritize_progress ||
			     progress == node.progress_weight) &&
			    distance >= node.distance)
				continue;
			node.reachable = true;
			node.distance = distance;
			node.progress_weight = progress;
			node.parent_segment = current;
			node.parent_side = side;
			node.incoming_edge = edge;
			if (heap.contains(child))
				heap.decrease(child);
			else
				heap.push(child);
		}
	}
	return result;
}

route_path_result build_route_path(
    const route_search_result &search,
    int target_segment)
{
	route_path_result result;
	if (target_segment < 0 || target_segment >= static_cast<int>(search.nodes.size()) ||
	    !search.nodes[target_segment].reachable)
		return result;
	result.reached = true;
	result.distance = search.nodes[target_segment].distance;
	result.progress_weight = search.nodes[target_segment].progress_weight;
	std::vector<int> reverse_segments;
	std::vector<int> reverse_sides;
	for (int current = target_segment;
	     current >= 0 && current < static_cast<int>(search.nodes.size());
	     current = search.nodes[current].parent_segment) {
		reverse_segments.push_back(current);
		if (current == search.start_segment)
			break;
		reverse_sides.push_back(search.nodes[current].parent_side);
	}
	if (reverse_segments.empty() || reverse_segments.back() != search.start_segment) {
		result = route_path_result{};
		return result;
	}
	result.segments.assign(reverse_segments.rbegin(), reverse_segments.rend());
	result.sides.assign(reverse_sides.rbegin(), reverse_sides.rend());
	for (std::size_t index = 1; index < result.segments.size(); ++index) {
		const auto &edge = search.nodes[result.segments[index]].incoming_edge;
		if (edge.legacy_cost == LEVEL_METADATA_ROUTE_EDGE_PROGRESS) {
			result.has_obstruction = true;
			result.first_obstruction_segment = result.segments[index - 1];
			result.first_obstruction_side = result.sides[index - 1];
			result.first_obstruction = edge;
			break;
		}
	}
	return result;
}

route_target_selection select_route_target(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    const std::vector<route_target> &targets)
{
	route_target_selection result;
	const auto search = search_routes(snapshot, query, progress, true);
	if (!search.problem.empty())
		return result;
	double best_distance = std::numeric_limits<double>::infinity();
	int best_progress = std::numeric_limits<int>::max();
	for (int index = 0; index < static_cast<int>(targets.size()); ++index) {
		const auto &target = targets[index];
		if (!valid_segment(snapshot, target.segment) || !target.position.valid ||
		    !search.nodes[target.segment].reachable)
			continue;
		const auto &center = snapshot.topology.segments[target.segment].center;
		if (!center.valid)
			continue;
		const auto &node = search.nodes[target.segment];
		const double distance = node.distance +
		                        point_distance(center, target.position);
		if (node.progress_weight > best_progress ||
		    (node.progress_weight == best_progress && distance >= best_distance))
			continue;
		best_progress = node.progress_weight;
		best_distance = distance;
		result.selected_index = index;
	}
	if (result.selected_index < 0)
		return result;
	result.found = true;
	result.distance = best_distance;
	result.progress_weight = best_progress;
	result.path = build_route_path(
	    search, targets[result.selected_index].segment);
	result.path.distance = best_distance;
	return result;
}

route_target_selection select_key_target(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    route_key_requirement key,
    const std::vector<route_target> &targets)
{
	route_target_selection result;
	const int index = key_index(key);
	if (index < 0 ||
	    ((progress.key_mask | progress.key_in_progress) & (1 << index)) != 0)
		return result;
	route_search_options options;
	options.optimistic = true;
	options.prioritize_progress = false;
	options.forbidden_missing_key = key;
	const auto search = search_routes(snapshot, query, progress, options);
	if (!search.problem.empty())
		return result;
	double best_distance = std::numeric_limits<double>::infinity();
	for (int target_index = 0;
	     target_index < static_cast<int>(targets.size()); ++target_index) {
		const auto &target = targets[target_index];
		if (!valid_segment(snapshot, target.segment) || !target.position.valid ||
		    !search.nodes[target.segment].reachable)
			continue;
		const auto &center = snapshot.topology.segments[target.segment].center;
		if (!center.valid)
			continue;
		const double distance = search.nodes[target.segment].distance +
		                        point_distance(center, target.position);
		if (distance >= best_distance)
			continue;
		best_distance = distance;
		result.selected_index = target_index;
	}
	if (result.selected_index < 0)
		return result;
	result.found = true;
	result.distance = best_distance;
	result.progress_weight =
	    search.nodes[targets[result.selected_index].segment].progress_weight;
	result.path = build_route_path(
	    search, targets[result.selected_index].segment);
	result.path.distance = best_distance;
	return result;
}

std::vector<route_trigger_source> discover_trigger_sources(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int segment,
    int side)
{
	return discover_trigger_sources_internal(
	    snapshot, progress, segment, side, false);
}

route_trigger_path_selection select_trigger_firing_path(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    const std::vector<route_trigger_source> &sources,
    const route_visibility_query &visibility)
{
	route_trigger_path_selection result;
	const auto search = search_routes(snapshot, query, progress, false);
	if (!search.problem.empty())
		return result;
	for (const auto &source : sources) {
		if (!trigger_source_wall_valid(
		        snapshot, progress, source.source_wall) ||
		    !valid_trigger(snapshot, source.trigger) ||
		    state_flag(progress.trigger_in_progress, source.trigger) ||
		    !source.source_position.valid)
			continue;
		route_trigger_path_selection candidate;
		candidate.source = source;
		if (valid_segment(snapshot, source.source_segment) &&
		    search.nodes[source.source_segment].reachable) {
			candidate.path = build_route_path(
			    search, source.source_segment);
			candidate.path.distance += point_distance(
			    snapshot.topology.segments[source.source_segment].center,
			    source.source_position);
			candidate.terminal_segment = source.source_segment;
			candidate.terminal_position = source.source_position;
			candidate.found = true;
		} else {
			for (const int segment : search.visit_order) {
				double extra_distance = 0.0;
				route_position terminal;
				if (!visible_source_position(
				        snapshot, progress, source, visibility, segment,
				        terminal, extra_distance))
					continue;
				candidate.path = build_route_path(search, segment);
				candidate.path.distance += extra_distance;
				candidate.path.progress_weight = 0;
				candidate.terminal_segment = segment;
				candidate.terminal_position = terminal;
				candidate.found = true;
				break;
			}
		}
		if (!candidate.found ||
		    (result.found && candidate.path.distance >= result.path.distance))
			continue;
		result = std::move(candidate);
	}
	return result;
}

route_target_inventory discover_route_targets(const route_snapshot &snapshot)
{
	route_target_inventory result;
	for (std::size_t object_index = 0;
	     object_index < snapshot.state.objects.size(); ++object_index) {
		const auto &object = snapshot.state.objects[object_index];
		if (object.should_be_dead || !object.position.valid ||
		    !valid_segment(snapshot, object.segment))
			continue;
		if (object.kind == route_object_kind::powerup &&
		    object.key != route_key_requirement::none) {
			const int key = key_index(object.key);
			if (key >= 0 && key < 3 &&
			    result.keys[key].size() < LEVEL_METADATA_MAX_TARGETS) {
				route_target target;
				target.kind = route_target_kind::key;
				target.key = object.key;
				target.segment = object.segment;
				target.object = static_cast<int>(object_index);
				target.position = object.position;
				result.keys[key].push_back(target);
			}
		}
		if (object.contains_count > 0 &&
		    object.contains_key != route_key_requirement::none) {
			const int key = key_index(object.contains_key);
			if (key >= 0 && key < 3 &&
			    result.keys[key].size() < LEVEL_METADATA_MAX_TARGETS) {
				route_target target;
				target.kind = route_target_kind::key;
				target.key = object.contains_key;
				target.segment = object.segment;
				target.object = static_cast<int>(object_index);
				target.contained = true;
				target.position = object.position;
				result.keys[key].push_back(target);
			}
		}
		if (!result.reactor_found &&
		    object.kind == route_object_kind::control_center) {
			result.reactor_found = true;
			result.reactor.kind = route_target_kind::reactor;
			result.reactor.segment = object.segment;
			result.reactor.object = static_cast<int>(object_index);
			result.reactor.position = object.position;
		}
		if (!result.boss_found && object.kind == route_object_kind::robot &&
		    object.boss) {
			result.boss_found = true;
			result.boss.kind = route_target_kind::boss;
			result.boss.segment = object.segment;
			result.boss.object = static_cast<int>(object_index);
			result.boss.position = object.position;
		}
	}
	if (!result.reactor_found) {
		for (int segment = 0;
		     segment < static_cast<int>(snapshot.topology.segments.size());
		     ++segment) {
			const auto &candidate = snapshot.topology.segments[segment];
			if (!candidate.control_center || !candidate.center.valid)
				continue;
			result.reactor_found = true;
			result.reactor.kind = route_target_kind::reactor;
			result.reactor.segment = segment;
			result.reactor.position = candidate.center;
			break;
		}
	}
	for (int segment = 0;
	     segment < static_cast<int>(snapshot.topology.segments.size());
	     ++segment) {
		const auto &topology_segment = snapshot.topology.segments[segment];
		for (int side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			const auto &topology_side = topology_segment.sides[side];
			const auto &state_side = snapshot.state.segments[segment].sides[side];
			bool route_exit = topology_side.child == -2;
			if (!route_exit && state_side.exit_trigger) {
				route_exit = true;
				const int wall = topology_side.wall;
				if (wall >= 0 && wall < static_cast<int>(snapshot.state.walls.size())) {
					const int trigger = snapshot.state.walls[wall].trigger;
					if (trigger >= 0 &&
					    trigger < static_cast<int>(snapshot.topology.triggers.size()) &&
					    snapshot.topology.triggers[trigger].kind ==
					        route_trigger_kind::secret_exit)
						route_exit = false;
				}
			}
			if (!route_exit || result.exits.size() >= LEVEL_METADATA_MAX_TARGETS)
				continue;
			route_target target;
			target.kind = route_target_kind::exit;
			target.segment = segment;
			target.side = side;
			target.position = topology_side.center.valid
			                      ? topology_side.center
			                      : topology_segment.center;
			result.exits.push_back(target);
		}
	}
	return result;
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

bool distances_match(double left, double right)
{
	if (!std::isfinite(left) || !std::isfinite(right))
		return !std::isfinite(left) && !std::isfinite(right);
	return std::fabs(left - right) <= 1e-9;
}

struct view_visibility_context {
	const level_metadata_scan_view *view;
};

bool view_target_visible(
    void *user,
    int segment,
    const dxx_route::route_position &from,
    int target_segment,
    const dxx_route::route_position &target)
{
	const auto *context = static_cast<view_visibility_context *>(user);
	return context->view->target_visible_from_segment(
	           context->view->user, segment, from.value.data(), target_segment,
	           target.value.data()) != 0;
}

bool view_wall_visible(
    void *user,
    int segment,
    const dxx_route::route_position &from,
    int wall)
{
	const auto *context = static_cast<view_visibility_context *>(user);
	return context->view->wall_visible_from_segment(
	           context->view->user, segment, from.value.data(), wall) != 0;
}

} // namespace

extern "C" int route_planner_compare_view(
    const level_metadata_scan_view *view,
    route_planner_shadow_summary *summary,
    char *problem,
    int problem_capacity)
{
	if (summary) {
		std::memset(summary, 0, sizeof(*summary));
		summary->first_mismatch_segment = -1;
		summary->first_target_category = -1;
		summary->first_target_index = -1;
		summary->first_trigger_source_progress_state = -1;
		summary->first_trigger_source_segment = -1;
		summary->first_trigger_source_side = -1;
		summary->first_trigger_source_index = -1;
		summary->first_trigger_firing_path_progress_state = -1;
		summary->first_trigger_firing_path_segment = -1;
		summary->first_trigger_firing_path_side = -1;
	}
	copy_problem(problem, problem_capacity, "");
	if (!view || !summary) {
		copy_problem(problem, problem_capacity,
		             "route planner comparison requires input and output");
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
		view_visibility_context visibility_context = { view };
		dxx_route::route_visibility_query visibility;
		visibility.user = &visibility_context;
		if (view->target_visible_from_segment)
			visibility.target_visible = view_target_visible;
		if (view->wall_visible_from_segment)
			visibility.wall_visible = view_wall_visible;
		auto legacy_targets = std::unique_ptr<level_metadata_route_target_inventory_shadow>(
		    new level_metadata_route_target_inventory_shadow{});
		if (!level_metadata_scan_route_targets_shadow(view, legacy_targets.get())) {
			copy_problem(problem, problem_capacity,
			             "legacy route target comparison failed");
			return 0;
		}
		const auto shared_targets = dxx_route::discover_route_targets(snapshot);
		auto record_target_mismatch =
		    [&](int category, int index, int old_count, int new_count,
		        const level_metadata_route_target_shadow *old_target,
		        const dxx_route::route_target *new_target) {
			    summary->target_mismatch_count++;
			    if (summary->target_mismatch_count != 1)
				    return;
			    summary->first_target_category = category;
			    summary->first_target_index = index;
			    summary->first_legacy_target_count = old_count;
			    summary->first_shared_target_count = new_count;
			    summary->first_legacy_target_segment = old_target ? old_target->seg : -1;
			    summary->first_shared_target_segment = new_target ? new_target->segment : -1;
			    for (int coordinate = 0; coordinate < 3; ++coordinate) {
				    summary->first_legacy_target_pos[coordinate] = old_target ? old_target->pos[coordinate] : 0;
				    summary->first_shared_target_pos[coordinate] = new_target ? new_target->position.value[coordinate] : 0;
			    }
		    };
		auto compare_targets =
		    [&](int category, const level_metadata_route_target_shadow *old_targets,
		        int old_count, const std::vector<dxx_route::route_target> &new_targets) {
			    const int new_count = static_cast<int>(new_targets.size());
			    summary->compared_target_count++;
			    if (old_count != new_count)
				    record_target_mismatch(category, -1, old_count, new_count, nullptr, nullptr);
			    const int count = old_count < new_count ? old_count : new_count;
			    for (int index = 0; index < count; ++index) {
				    const auto &old_target = old_targets[index];
				    const auto &new_target = new_targets[index];
				    summary->compared_target_count++;
				    const bool mismatch = old_target.seg != new_target.segment ||
				                          !new_target.position.valid ||
				                          old_target.pos[0] != new_target.position.value[0] ||
				                          old_target.pos[1] != new_target.position.value[1] ||
				                          old_target.pos[2] != new_target.position.value[2];
				    if (mismatch)
					    record_target_mismatch(category, index, old_count, new_count,
					                           &old_target, &new_target);
			    }
		    };
		for (int key = 0; key < 3; ++key)
			compare_targets(key, legacy_targets->keys[key],
			                legacy_targets->key_count[key], shared_targets.keys[key]);
		std::vector<dxx_route::route_target> shared_reactor;
		if (shared_targets.reactor_found)
			shared_reactor.push_back(shared_targets.reactor);
		compare_targets(3, &legacy_targets->reactor,
		                legacy_targets->reactor_found, shared_reactor);
		std::vector<dxx_route::route_target> shared_boss;
		if (shared_targets.boss_found)
			shared_boss.push_back(shared_targets.boss);
		compare_targets(4, &legacy_targets->boss,
		                legacy_targets->boss_found, shared_boss);
		compare_targets(5, legacy_targets->exits, legacy_targets->exit_count,
		                shared_targets.exits);
		std::vector<dxx_route::route_progress_state> progress_states;
		progress_states.push_back(
		    dxx_route::initial_route_progress_state(snapshot, query));
		auto advanced = progress_states.front();
		dxx_route::route_progress_acquire_key(
		    advanced, dxx_route::route_key_requirement::blue);
		dxx_route::route_progress_acquire_key(
		    advanced, dxx_route::route_key_requirement::red);
		dxx_route::route_progress_acquire_key(
		    advanced, dxx_route::route_key_requirement::gold);
		advanced.control_center_destroyed = true;
		progress_states.push_back(advanced);
		auto opened = advanced;
		opened.fired_triggers.assign(opened.fired_triggers.size(), 1);
		opened.opened_hidden_walls.assign(
		    opened.opened_hidden_walls.size(), 1);
		progress_states.push_back(opened);
		auto avoided = progress_states.front();
		avoided.avoided_key_mask = LEVEL_METADATA_KEY_MASK_BLUE |
		                           LEVEL_METADATA_KEY_MASK_RED |
		                           LEVEL_METADATA_KEY_MASK_GOLD;
		avoided.avoided_triggers.assign(avoided.avoided_triggers.size(), 1);
		progress_states.push_back(avoided);
		summary->compared_progress_state_count =
		    static_cast<int>(progress_states.size());
		for (int state_index = 0;
		     state_index < static_cast<int>(progress_states.size()); ++state_index) {
			const auto &progress = progress_states[state_index];
			level_metadata_route_progress_shadow legacy_progress = {};
			legacy_progress.current_seg = progress.current_segment;
			for (int coordinate = 0; coordinate < 3; ++coordinate)
				legacy_progress.current_pos[coordinate] =
				    progress.current_position.value[coordinate];
			legacy_progress.key_mask = progress.key_mask;
			legacy_progress.key_in_progress = progress.key_in_progress;
			legacy_progress.avoided_key_mask = progress.avoided_key_mask;
			legacy_progress.control_center_destroyed =
			    progress.control_center_destroyed;
			for (int trigger = 0;
			     trigger < static_cast<int>(progress.fired_triggers.size()); ++trigger) {
				legacy_progress.fired_triggers[trigger] =
				    progress.fired_triggers[trigger];
				legacy_progress.trigger_in_progress[trigger] =
				    progress.trigger_in_progress[trigger];
				legacy_progress.avoided_triggers[trigger] =
				    progress.avoided_triggers[trigger];
			}
			for (int wall = 0;
			     wall < static_cast<int>(progress.opened_hidden_walls.size()); ++wall)
				legacy_progress.opened_hidden_walls[wall] =
				    progress.opened_hidden_walls[wall];
			const auto firing_search = dxx_route::search_routes(
			    snapshot, query, progress, false);
			if (!firing_search.problem.empty()) {
				copy_problem(
				    problem, problem_capacity, firing_search.problem.c_str());
				return 0;
			}
			bool compared_direct_firing = false;
			bool compared_visible_firing = false;
			for (int segment = 0; segment < view->num_segments; ++segment) {
				for (int side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
					std::array<level_metadata_route_trigger_source_shadow,
					           LEVEL_METADATA_MAX_WALLS>
					    legacy_sources = {};
					int legacy_count = 0;
					if (!level_metadata_scan_route_trigger_sources_shadow(
					        view, &legacy_progress, segment, side,
					        legacy_sources.data(),
					        static_cast<int>(legacy_sources.size()),
					        &legacy_count)) {
						copy_problem(
						    problem, problem_capacity,
						    "legacy trigger source comparison failed");
						return 0;
					}
					const auto shared_sources =
					    dxx_route::discover_trigger_sources(
					        snapshot, progress, segment, side);
					const int shared_count =
					    static_cast<int>(shared_sources.size());
					summary->compared_trigger_source_edge_count++;
					auto record_source_mismatch = [&](int index) {
						if (summary->trigger_source_mismatch_count++ != 0)
							return;
						summary->first_trigger_source_progress_state =
						    state_index;
						summary->first_trigger_source_segment = segment;
						summary->first_trigger_source_side = side;
						summary->first_trigger_source_index = index;
						summary->first_legacy_trigger_source_count =
						    legacy_count;
						summary->first_shared_trigger_source_count =
						    shared_count;
						if (index < 0 || index >= legacy_count ||
						    index >= shared_count)
							return;
						const auto &old_source = legacy_sources[index];
						const auto &new_source = shared_sources[index];
						summary->first_legacy_trigger_source_wall =
						    old_source.source_wall;
						summary->first_shared_trigger_source_wall =
						    new_source.source_wall;
						summary->first_legacy_trigger_source_trigger =
						    old_source.trigger_num;
						summary->first_shared_trigger_source_trigger =
						    new_source.trigger;
						summary->first_legacy_trigger_source_segment =
						    old_source.source_seg;
						summary->first_shared_trigger_source_segment =
						    new_source.source_segment;
						summary->first_legacy_trigger_source_side =
						    old_source.source_side;
						summary->first_shared_trigger_source_side =
						    new_source.source_side;
						for (int coordinate = 0; coordinate < 3; ++coordinate) {
							summary->first_legacy_trigger_source_pos[coordinate] =
							    old_source.source_pos[coordinate];
							summary->first_shared_trigger_source_pos[coordinate] =
							    new_source.source_position.value[coordinate];
						}
					};
					if (legacy_count != shared_count)
						record_source_mismatch(-1);
					const int source_count =
					    legacy_count < shared_count ? legacy_count : shared_count;
					for (int index = 0; index < source_count; ++index) {
						const auto &old_source = legacy_sources[index];
						const auto &new_source = shared_sources[index];
						summary->compared_trigger_source_count++;
						const bool mismatch =
						    old_source.target_seg != new_source.target_segment ||
						    old_source.target_side != new_source.target_side ||
						    old_source.target_wall != new_source.target_wall ||
						    old_source.source_wall != new_source.source_wall ||
						    old_source.source_seg != new_source.source_segment ||
						    old_source.source_side != new_source.source_side ||
						    old_source.trigger_num != new_source.trigger ||
						    !new_source.source_position.valid ||
						    old_source.source_pos[0] !=
						        new_source.source_position.value[0] ||
						    old_source.source_pos[1] !=
						        new_source.source_position.value[1] ||
						    old_source.source_pos[2] !=
						        new_source.source_position.value[2];
						if (mismatch)
							record_source_mismatch(index);
					}
					if (shared_sources.empty() ||
					    segment != shared_sources.front().target_segment ||
					    side != shared_sources.front().target_side ||
					    state_index > 1)
						continue;
					bool directly_reachable = false;
					for (const auto &source : shared_sources)
						if (source.source_segment >= 0 &&
						    source.source_segment <
						        static_cast<int>(firing_search.nodes.size()) &&
						    firing_search.nodes[source.source_segment].reachable) {
							directly_reachable = true;
							break;
						}
					if ((directly_reachable && compared_direct_firing) ||
					    (!directly_reachable && compared_visible_firing))
						continue;
					if (directly_reachable)
						compared_direct_firing = true;
					else
						compared_visible_firing = true;
					level_metadata_route_trigger_firing_path_shadow legacy_firing = {};
					if (!level_metadata_scan_route_trigger_firing_path_shadow(
					        view, &legacy_progress, segment, side,
					        &legacy_firing)) {
						copy_problem(
						    problem, problem_capacity,
						    "legacy trigger firing path comparison failed");
						return 0;
					}
					const auto shared_firing =
					    dxx_route::select_trigger_firing_path(
					        snapshot, query, progress, shared_sources,
					        visibility);
					summary->compared_trigger_firing_path_count++;
					const bool firing_mismatch =
					    legacy_firing.found !=
					        static_cast<int>(shared_firing.found) ||
					    (legacy_firing.found &&
					     (legacy_firing.source.source_wall !=
					          shared_firing.source.source_wall ||
					      legacy_firing.source.trigger_num !=
					          shared_firing.source.trigger ||
					      legacy_firing.progress_weight !=
					          shared_firing.path.progress_weight ||
					      legacy_firing.terminal_seg !=
					          shared_firing.terminal_segment ||
					      legacy_firing.terminal_pos_valid !=
					          static_cast<int>(
					              shared_firing.terminal_position.valid) ||
					      legacy_firing.terminal_pos[0] !=
					          shared_firing.terminal_position.value[0] ||
					      legacy_firing.terminal_pos[1] !=
					          shared_firing.terminal_position.value[1] ||
					      legacy_firing.terminal_pos[2] !=
					          shared_firing.terminal_position.value[2] ||
					      !distances_match(
					          legacy_firing.distance,
					          shared_firing.path.distance)));
					if (firing_mismatch &&
					    summary->trigger_firing_path_mismatch_count++ == 0) {
						summary->first_trigger_firing_path_progress_state =
						    state_index;
						summary->first_trigger_firing_path_segment = segment;
						summary->first_trigger_firing_path_side = side;
						summary->first_legacy_trigger_firing_path_found =
						    legacy_firing.found;
						summary->first_shared_trigger_firing_path_found =
						    shared_firing.found;
						summary->first_legacy_trigger_firing_path_wall =
						    legacy_firing.source.source_wall;
						summary->first_shared_trigger_firing_path_wall =
						    shared_firing.source.source_wall;
						summary->first_legacy_trigger_firing_path_trigger =
						    legacy_firing.source.trigger_num;
						summary->first_shared_trigger_firing_path_trigger =
						    shared_firing.source.trigger;
						summary->first_legacy_trigger_firing_path_terminal_segment =
						    legacy_firing.terminal_seg;
						summary->first_shared_trigger_firing_path_terminal_segment =
						    shared_firing.terminal_segment;
						summary->first_legacy_trigger_firing_path_progress_weight =
						    legacy_firing.progress_weight;
						summary->first_shared_trigger_firing_path_progress_weight =
						    shared_firing.path.progress_weight;
						for (int coordinate = 0; coordinate < 3; ++coordinate) {
							summary->first_legacy_trigger_firing_path_terminal_pos[coordinate] =
							    legacy_firing.terminal_pos[coordinate];
							summary->first_shared_trigger_firing_path_terminal_pos[coordinate] =
							    shared_firing.terminal_position.value[coordinate];
						}
						summary->first_legacy_trigger_firing_path_distance =
						    legacy_firing.distance;
						summary->first_shared_trigger_firing_path_distance =
						    shared_firing.path.distance;
					}
				}
			}
			level_metadata_route_target_selection_shadow legacy_selection = {};
			if (!level_metadata_scan_route_select_targets_shadow(
			        view, &legacy_progress, legacy_targets->exits,
			        legacy_targets->exit_count, &legacy_selection)) {
				copy_problem(problem, problem_capacity,
				             "legacy route target selection comparison failed");
				return 0;
			}
			const auto shared_selection = dxx_route::select_route_target(
			    snapshot, query, progress, shared_targets.exits);
			summary->compared_target_selection_count++;
			const bool selection_mismatch =
			    legacy_selection.selected_index != shared_selection.selected_index ||
			    (legacy_selection.selected_index >= 0 &&
			     (legacy_selection.progress_weight != shared_selection.progress_weight ||
			      !distances_match(legacy_selection.distance, shared_selection.distance)));
			if (selection_mismatch &&
			    summary->target_selection_mismatch_count++ == 0) {
				summary->first_selection_progress_state = state_index;
				summary->first_legacy_selection_index = legacy_selection.selected_index;
				summary->first_shared_selection_index = shared_selection.selected_index;
				summary->first_legacy_selection_progress_weight =
				    legacy_selection.progress_weight;
				summary->first_shared_selection_progress_weight =
				    shared_selection.progress_weight;
				summary->first_legacy_selection_distance = legacy_selection.distance;
				summary->first_shared_selection_distance = shared_selection.distance;
			}
			const dxx_route::route_key_requirement key_types[3] = {
				dxx_route::route_key_requirement::blue,
				dxx_route::route_key_requirement::red,
				dxx_route::route_key_requirement::gold,
			};
			for (int key = 0; key < 3; ++key) {
				level_metadata_route_target_selection_shadow legacy_key = {};
				if (!level_metadata_scan_route_select_key_shadow(
				        view, &legacy_progress, key, &legacy_key)) {
					copy_problem(problem, problem_capacity,
					             "legacy key target selection comparison failed");
					return 0;
				}
				const auto shared_key = dxx_route::select_key_target(
				    snapshot, query, progress, key_types[key],
				    shared_targets.keys[key]);
				summary->compared_key_selection_count++;
				const bool key_mismatch =
				    legacy_key.selected_index != shared_key.selected_index ||
				    (legacy_key.selected_index >= 0 &&
				     (legacy_key.progress_weight != shared_key.progress_weight ||
				      !distances_match(legacy_key.distance, shared_key.distance)));
				if (key_mismatch &&
				    summary->key_selection_mismatch_count++ == 0) {
					summary->first_key_selection_progress_state = state_index;
					summary->first_key_selection_key = key;
					summary->first_legacy_key_selection_index =
					    legacy_key.selected_index;
					summary->first_shared_key_selection_index =
					    shared_key.selected_index;
					summary->first_legacy_key_selection_progress_weight =
					    legacy_key.progress_weight;
					summary->first_shared_key_selection_progress_weight =
					    shared_key.progress_weight;
					summary->first_legacy_key_selection_distance =
					    legacy_key.distance;
					summary->first_shared_key_selection_distance =
					    shared_key.distance;
				}
			}
			for (int optimistic = 0; optimistic <= 1; ++optimistic) {
				std::vector<level_metadata_route_search_node> legacy(view->num_segments);
				if (level_metadata_scan_route_search_state_shadow(
				        view, &legacy_progress, optimistic, legacy.data(),
				        static_cast<int>(legacy.size())) != view->num_segments) {
					copy_problem(problem, problem_capacity,
					             "legacy route search comparison failed");
					return 0;
				}
				const auto shared = dxx_route::search_routes(
				    snapshot, query, progress, optimistic != 0);
				if (!shared.problem.empty()) {
					copy_problem(problem, problem_capacity, shared.problem.c_str());
					return 0;
				}
				for (int segment = 0; segment < view->num_segments; ++segment) {
					const auto &old_node = legacy[segment];
					const auto &new_node = shared.nodes[segment];
					const bool mismatch =
					    (old_node.reachable != static_cast<int>(new_node.reachable)) ||
					    (old_node.reachable &&
					     (old_node.progress_weight != new_node.progress_weight ||
					      old_node.parent_seg != new_node.parent_segment ||
					      old_node.parent_side != new_node.parent_side ||
					      !distances_match(old_node.distance, new_node.distance)));
					summary->compared_node_count++;
					if (!mismatch)
						continue;
					if (summary->mismatch_count++ == 0) {
						summary->first_mismatch_progress_state = state_index;
						summary->first_mismatch_optimistic = optimistic;
						summary->first_mismatch_segment = segment;
						summary->first_legacy_reachable = old_node.reachable;
						summary->first_shared_reachable = new_node.reachable;
						summary->first_legacy_progress_weight = old_node.progress_weight;
						summary->first_shared_progress_weight = new_node.progress_weight;
						summary->first_legacy_parent_segment = old_node.parent_seg;
						summary->first_shared_parent_segment = new_node.parent_segment;
						summary->first_legacy_parent_side = old_node.parent_side;
						summary->first_shared_parent_side = new_node.parent_side;
						summary->first_legacy_distance = old_node.distance;
						summary->first_shared_distance = new_node.distance;
					}
				}
			}
		}
		return 1;
	} catch (const std::exception &error) {
		copy_problem(problem, problem_capacity, error.what());
	} catch (...) {
		copy_problem(problem, problem_capacity,
		             "unknown route planner comparison failure");
	}
	return 0;
}
