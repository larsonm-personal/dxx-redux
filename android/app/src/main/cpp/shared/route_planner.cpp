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
	route_heap heap(result.nodes, true);
	heap.push(result.start_segment);
	while (!heap.empty()) {
		const int current = heap.pop();
		closed[current] = 1;
		for (int side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			const int child = snapshot.topology.segments[current].sides[side].child;
			if (!valid_segment(snapshot, child) || closed[child])
				continue;
			const auto edge = evaluate_route_edge(
			    snapshot, query, progress, current, side);
			if (edge.legacy_cost == LEVEL_METADATA_ROUTE_EDGE_BLOCKED ||
			    (!optimistic && edge.legacy_cost == LEVEL_METADATA_ROUTE_EDGE_PROGRESS))
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
			if (progress > node.progress_weight ||
			    (progress == node.progress_weight && distance >= node.distance))
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
			result.first_obstruction = edge;
			break;
		}
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
				legacy_progress.avoided_triggers[trigger] =
				    progress.avoided_triggers[trigger];
			}
			for (int wall = 0;
			     wall < static_cast<int>(progress.opened_hidden_walls.size()); ++wall)
				legacy_progress.opened_hidden_walls[wall] =
				    progress.opened_hidden_walls[wall];
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
