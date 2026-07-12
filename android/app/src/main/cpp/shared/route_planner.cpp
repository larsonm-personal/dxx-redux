#include "route_planner.h"
#include "route_planner_c.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>

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

} // namespace

route_search_result search_routes(
    const route_snapshot &snapshot,
    const route_query &query,
    bool optimistic)
{
	route_search_result result;
	result.start_segment = snapshot.state.start_segment;
	const int count = static_cast<int>(snapshot.topology.segments.size());
	result.nodes.resize(count);
	if (!valid_segment(snapshot, result.start_segment)) {
		result.problem = "route start segment is invalid";
		return result;
	}
	const auto &start_center = snapshot.topology.segments[result.start_segment].center;
	const route_position &start = query.start.valid ? query.start : snapshot.state.start_position;
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
			const auto edge = evaluate_route_edge(snapshot, query, current, side);
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
		for (int optimistic = 0; optimistic <= 1; ++optimistic) {
			std::vector<level_metadata_route_search_node> legacy(view->num_segments);
			if (level_metadata_scan_route_search_shadow(
			        view, optimistic, legacy.data(), static_cast<int>(legacy.size())) != view->num_segments) {
				copy_problem(problem, problem_capacity,
				             "legacy route search comparison failed");
				return 0;
			}
			const auto shared = dxx_route::search_routes(snapshot, query, optimistic != 0);
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
		return 1;
	} catch (const std::exception &error) {
		copy_problem(problem, problem_capacity, error.what());
	} catch (...) {
		copy_problem(problem, problem_capacity,
		             "unknown route planner comparison failure");
	}
	return 0;
}
