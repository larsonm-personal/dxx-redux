#include "route_planner.h"
#include "route_planner_c.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <unordered_map>

namespace dxx_route
{
namespace
{

bool valid_segment(const route_snapshot &snapshot, int segment);
bool valid_wall(const route_snapshot &snapshot, int wall);

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

bool positions_differ(
    const route_position &left,
    const route_position &right)
{
	return left.valid && right.valid && left.value != right.value;
}

bool side_normal(
    const route_snapshot &snapshot,
    int segment_number,
    int side_number,
    std::int64_t normal[3])
{
	static constexpr int side_vertices[LEVEL_METADATA_MAX_SIDES][4] = {
		{ 7, 6, 2, 3 }, { 0, 4, 7, 3 }, { 0, 1, 5, 4 }, { 2, 6, 5, 1 }, { 4, 5, 6, 7 }, { 3, 2, 1, 0 }
	};
	if (!valid_segment(snapshot, segment_number) || side_number < 0 ||
	    side_number >= LEVEL_METADATA_MAX_SIDES)
		return false;
	const auto &segment = snapshot.topology.segments[segment_number];
	const auto &first = segment.vertices[side_vertices[side_number][0]];
	const auto &second = segment.vertices[side_vertices[side_number][1]];
	const auto &third = segment.vertices[side_vertices[side_number][2]];
	const auto &fourth = segment.vertices[side_vertices[side_number][3]];
	if (!first.valid || !second.valid || !third.valid || !fourth.valid)
		return false;
	const auto calculate = [normal](
	                           const route_position &origin,
	                           const route_position &left,
	                           const route_position &right) {
		const std::int64_t a[3] = {
			static_cast<std::int64_t>(left.value[0]) - origin.value[0],
			static_cast<std::int64_t>(left.value[1]) - origin.value[1],
			static_cast<std::int64_t>(left.value[2]) - origin.value[2]
		};
		const std::int64_t b[3] = {
			static_cast<std::int64_t>(right.value[0]) - origin.value[0],
			static_cast<std::int64_t>(right.value[1]) - origin.value[1],
			static_cast<std::int64_t>(right.value[2]) - origin.value[2]
		};
		normal[0] = a[1] * b[2] - a[2] * b[1];
		normal[1] = a[2] * b[0] - a[0] * b[2];
		normal[2] = a[0] * b[1] - a[1] * b[0];
		return normal[0] != 0 || normal[1] != 0 || normal[2] != 0;
	};
	return calculate(first, second, third) ||
	       calculate(first, third, fourth);
}

route_position exit_activation_position(
    const route_snapshot &snapshot,
    const route_target &target)
{
	const auto &face = target.position;
	const auto &segment_center = snapshot.topology.segments[target.segment].center;
	if (!face.valid || !segment_center.valid)
		return segment_center.valid ? segment_center : face;
	std::int64_t normal[3] = {};
	if (!side_normal(snapshot, target.segment, target.side, normal))
		return segment_center;
	long double inward_dot = 0.0;
	long double length_squared = 0.0;
	for (int axis = 0; axis < 3; ++axis) {
		inward_dot += static_cast<long double>(normal[axis]) *
		              (segment_center.value[axis] - face.value[axis]);
		length_squared +=
		    static_cast<long double>(normal[axis]) * normal[axis];
	}
	if (inward_dot < 0.0)
		for (auto &coordinate : normal)
			coordinate = -coordinate;
	const long double length = std::sqrt(length_squared);
	if (length <= 0.0)
		return segment_center;
	route_position result = face;
	for (int axis = 0; axis < 3; ++axis) {
		const auto coordinate = static_cast<std::int64_t>(std::llround(
		    face.value[axis] + static_cast<long double>(normal[axis]) *
		                           (4 * LEVEL_METADATA_FIX_SCALE) / length));
		result.value[axis] = static_cast<int>(std::max<std::int64_t>(
		    std::numeric_limits<int>::min(),
		    std::min<std::int64_t>(coordinate, std::numeric_limits<int>::max())));
	}
	return result;
}

route_position crossing_aim_position(
    const route_snapshot &snapshot,
    const route_trigger_source &source,
    const route_position &activation_position)
{
	route_position result;
	if (!activation_position.valid ||
	    !valid_segment(snapshot, source.source_segment) ||
	    source.source_side < 0 ||
	    source.source_side >= LEVEL_METADATA_MAX_SIDES)
		return result;
	const auto &segment = snapshot.topology.segments[source.source_segment];
	const auto &side = segment.sides[source.source_side];
	if (valid_segment(snapshot, side.child)) {
		result = snapshot.topology.segments[side.child].center;
		if (positions_differ(result, activation_position))
			return result;
	}
	if (valid_wall(snapshot, source.source_wall)) {
		result = snapshot.topology.walls[source.source_wall].target;
		if (positions_differ(result, activation_position))
			return result;
	}
	std::int64_t normal[3] = {};
	if (!side_normal(
	        snapshot, source.source_segment, source.source_side, normal))
		return {};
	std::int64_t maximum = 0;
	for (const auto coordinate : normal)
		maximum = std::max(maximum, coordinate < 0 ? -coordinate : coordinate);
	const std::int64_t divisor =
	    std::max<std::int64_t>(1, maximum / (LEVEL_METADATA_FIX_SCALE * 10));
	result = activation_position;
	for (int axis = 0; axis < 3; ++axis) {
		const std::int64_t coordinate =
		    static_cast<std::int64_t>(result.value[axis]) +
		    normal[axis] / divisor;
		result.value[axis] = static_cast<int>(std::max<std::int64_t>(
		    std::numeric_limits<int>::min(),
		    std::min<std::int64_t>(coordinate, std::numeric_limits<int>::max())));
	}
	return result;
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

void report_progress(
    const route_visibility_query &visibility,
    const char *stage,
    int completed,
    int total)
{
	if (visibility.progress)
		visibility.progress(
		    visibility.progress_user, stage, completed, total);
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
	const auto &topology = snapshot.topology.walls[wall];
	if (!topology.shootable_trigger) {
		if (!valid_segment(snapshot, topology.segment) || topology.side < 0 ||
		    topology.side >= LEVEL_METADATA_MAX_SIDES)
			return false;
		const auto &side =
		    snapshot.topology.segments[topology.segment].sides[topology.side];
		if (!valid_segment(snapshot, side.child))
			return false;
	}
	const int trigger = snapshot.state.walls[wall].trigger;
	return valid_trigger(snapshot, trigger) &&
	       !snapshot.state.triggers[trigger].disabled &&
	       route_trigger_opens_path(snapshot.topology.triggers[trigger].kind) &&
	       !state_flag(progress.fired_triggers, trigger);
}

bool trigger_source_fits_navigator(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_trigger_source &source)
{
	if (!valid_wall(snapshot, source.source_wall) ||
	    snapshot.topology.walls[source.source_wall].shootable_trigger)
		return true;
	if (!valid_segment(snapshot, source.source_segment) || source.source_side < 0 ||
	    source.source_side >= LEVEL_METADATA_MAX_SIDES)
		return false;
	const int clearance = snapshot.topology.segments[source.source_segment]
	                          .sides[source.source_side]
	                          .clearance_radius;
	return query.navigator.radius <= 0 || clearance <= 0 ||
	       clearance >= query.navigator.radius;
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
	if (valid_wall(snapshot, source.source_wall) && visibility.wall_shootable)
		return visibility.wall_shootable(
		    visibility.user, segment, position, source.source_wall);
	return visibility.target_visible &&
	       visibility.target_visible(
	           visibility.user, segment, position, source.source_segment,
	           source.source_position);
}

struct visibility_sample_key {
	unsigned int cache_namespace;
	int sample_kind;
	int segment;
	int target_wall;
	int target_segment;
	std::array<int, 3> target_position;

	bool operator==(const visibility_sample_key &other) const
	{
		return cache_namespace == other.cache_namespace &&
		       sample_kind == other.sample_kind && segment == other.segment &&
		       target_wall == other.target_wall &&
		       target_segment == other.target_segment &&
		       target_position == other.target_position;
	}
};

struct visibility_sample_key_hash {
	std::size_t operator()(const visibility_sample_key &key) const
	{
		std::size_t hash = key.cache_namespace;
		auto mix = [&hash](int value) {
			hash ^= static_cast<unsigned int>(value) + 0x9e3779b9u +
			        (hash << 6) + (hash >> 2);
		};
		mix(key.sample_kind);
		mix(key.segment);
		mix(key.target_wall);
		mix(key.target_segment);
		for (const int coordinate : key.target_position)
			mix(coordinate);
		return hash;
	}
};

struct visibility_sample_result {
	bool found = false;
	route_position position;
	double extra_distance = 0.0;
};

using visibility_sample_cache = std::unordered_map<
    visibility_sample_key, visibility_sample_result,
    visibility_sample_key_hash>;

visibility_sample_key make_visibility_sample_key(
    const route_snapshot &snapshot,
    const route_trigger_source &source,
    const route_visibility_query &visibility,
    int segment,
    int sample_kind)
{
	visibility_sample_key key = {};
	key.cache_namespace = visibility.sample_cache_namespace;
	key.sample_kind = sample_kind;
	key.segment = segment;
	key.target_wall =
	    valid_wall(snapshot, source.source_wall) && visibility.wall_shootable
	        ? source.source_wall
	        : -1;
	key.target_segment = key.target_wall >= 0 ? -1 : source.source_segment;
	key.target_position = key.target_wall >= 0
	                          ? std::array<int, 3>{ { 0, 0, 0 } }
	                          : source.source_position.value;
	return key;
}

bool find_cached_visibility_sample(
    const route_snapshot &snapshot,
    const route_trigger_source &source,
    const route_visibility_query &visibility,
    int segment,
    int sample_kind,
    route_position &position,
    double &extra_distance,
    bool &found)
{
	if (!visibility.sample_cache)
		return false;
	auto &cache = *static_cast<visibility_sample_cache *>(
	    visibility.sample_cache);
	const auto item = cache.find(make_visibility_sample_key(
	    snapshot, source, visibility, segment, sample_kind));
	if (item == cache.end())
		return false;
	found = item->second.found;
	position = item->second.position;
	extra_distance = item->second.extra_distance;
	return true;
}

void store_cached_visibility_sample(
    const route_snapshot &snapshot,
    const route_trigger_source &source,
    const route_visibility_query &visibility,
    int segment,
    int sample_kind,
    const route_position &position,
    double extra_distance,
    bool found)
{
	if (!visibility.sample_cache)
		return;
	auto &cache = *static_cast<visibility_sample_cache *>(
	    visibility.sample_cache);
	visibility_sample_result result;
	result.found = found;
	result.position = position;
	result.extra_distance = extra_distance;
	cache[make_visibility_sample_key(
	    snapshot, source, visibility, segment, sample_kind)] = result;
}

bool visible_source_center_position(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    const route_trigger_source &source,
    const route_visibility_query &visibility,
    int segment,
    route_position &position,
    double &extra_distance)
{
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
	bool found = false;
	if (find_cached_visibility_sample(
	        snapshot, source, visibility, segment, 0, position,
	        extra_distance, found))
		return found;
	if (!topology_segment.center.valid)
		return false;
	found = source_visible_from_position(
	    snapshot, source, visibility, segment, topology_segment.center);
	if (found) {
		position = topology_segment.center;
		extra_distance = 0.0;
	}
	store_cached_visibility_sample(
	    snapshot, source, visibility, segment, 0, position, extra_distance,
	    found);
	return found;
}

bool visible_source_detailed_position(
    const route_snapshot &snapshot,
    const route_trigger_source &source,
    const route_visibility_query &visibility,
    int segment,
    route_position &position,
    double &extra_distance)
{
	static constexpr int sample_weights[] = { 1 };
	static constexpr int face_sample_weights[] = { 1, 3 };
	/* Each physical segment edge appears on two faces. Sample it once. */
	static constexpr int segment_edges[12][2] = {
		{ 7, 6 },
		{ 6, 2 },
		{ 2, 3 },
		{ 3, 7 },
		{ 0, 4 },
		{ 4, 7 },
		{ 3, 0 },
		{ 0, 1 },
		{ 1, 5 },
		{ 5, 4 },
		{ 6, 5 },
		{ 1, 2 },
	};
	if (!valid_segment(snapshot, segment))
		return false;
	const auto &topology_segment = snapshot.topology.segments[segment];
	bool found = false;
	if (find_cached_visibility_sample(
	        snapshot, source, visibility, segment, 1, position,
	        extra_distance, found))
		return found;
	double best_distance = std::numeric_limits<double>::infinity();
	auto consider = [&](const route_position &candidate) {
		if (!source_visible_from_position(
		        snapshot, source, visibility, segment, candidate))
			return;
		const double distance = point_distance(
		    topology_segment.center, candidate);
		if (found && distance >= best_distance)
			return;
		position = candidate;
		extra_distance = distance;
		best_distance = distance;
		found = true;
	};
	if (!topology_segment.center.valid)
		return false;
	for (int side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
		const auto &side_center = topology_segment.sides[side].center;
		if (!side_center.valid)
			continue;
		for (const int weight : face_sample_weights) {
			const auto candidate = weighted_position(
			    topology_segment.center, side_center, weight);
			consider(candidate);
		}
	}
	for (const auto &vertex : topology_segment.vertices) {
		if (!vertex.valid)
			continue;
		for (const int weight : sample_weights) {
			const auto candidate = weighted_position(
			    topology_segment.center, vertex, weight);
			consider(candidate);
		}
	}
	for (const auto &edge : segment_edges) {
		const auto &first = topology_segment.vertices[edge[0]];
		const auto &second = topology_segment.vertices[edge[1]];
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
			consider(candidate);
		}
	}
	store_cached_visibility_sample(
	    snapshot, source, visibility, segment, 1, position, extra_distance,
	    found);
	return found;
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
	return visible_source_center_position(
	           snapshot, progress, source, visibility, segment, position,
	           extra_distance) ||
	       visible_source_detailed_position(
	           snapshot, source, visibility, segment, position,
	           extra_distance);
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
	result.destroyed_blastable_walls.resize(snapshot.state.walls.size());
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

void route_progress_traverse_path(
    const route_snapshot &snapshot,
    route_progress_state &progress,
    const route_path_result &path)
{
	for (std::size_t index = 0;
	     index < path.sides.size() && index < path.segments.size(); ++index) {
		const int segment = path.segments[index];
		const int side = path.sides[index];
		if (!valid_segment(snapshot, segment) || side < 0 ||
		    side >= LEVEL_METADATA_MAX_SIDES)
			continue;
		const int wall = snapshot.topology.segments[segment].sides[side].wall;
		if (!valid_wall(snapshot, wall) ||
		    snapshot.topology.walls[wall].shootable_trigger)
			continue;
		const int trigger = snapshot.state.walls[wall].trigger;
		if (!valid_trigger(snapshot, trigger) ||
		    snapshot.state.triggers[trigger].disabled ||
		    !route_trigger_opens_path(snapshot.topology.triggers[trigger].kind))
			continue;
		route_progress_fire_trigger(progress, trigger);
	}
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

bool route_progress_destroy_blastable_wall(
    const route_snapshot &snapshot,
    route_progress_state &progress,
    int wall)
{
	if (!valid_wall(snapshot, wall) ||
	    wall >= static_cast<int>(progress.destroyed_blastable_walls.size()))
		return false;
	progress.destroyed_blastable_walls[wall] = 1;
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
	    reverse_wall < static_cast<int>(progress.destroyed_blastable_walls.size()))
		progress.destroyed_blastable_walls[reverse_wall] = 1;
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
			const int clearance =
			    snapshot.topology.segments[current].sides[side].clearance_radius;
			if (query.navigator.radius > 0 && clearance > 0 &&
			    clearance < query.navigator.radius)
				continue;
			const auto edge = evaluate_route_edge(
			    snapshot, query, progress, options.forbidden_missing_key,
			    current, side);
			if (edge.progress_cost == LEVEL_METADATA_ROUTE_EDGE_BLOCKED ||
			    (!options.optimistic &&
			     edge.progress_cost == LEVEL_METADATA_ROUTE_EDGE_PROGRESS))
				continue;
			const auto &current_center = snapshot.topology.segments[current].center;
			const auto &child_center = snapshot.topology.segments[child].center;
			if (!current_center.valid || !child_center.valid)
				continue;
			const double distance = result.nodes[current].distance +
			                        point_distance(current_center, child_center);
			const int progress = result.nodes[current].progress_weight +
			                     (edge.progress_cost == LEVEL_METADATA_ROUTE_EDGE_PROGRESS ? 1 : 0);
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
		if (edge.progress_cost == LEVEL_METADATA_ROUTE_EDGE_PROGRESS) {
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
	const int segments = static_cast<int>(search.visit_order.size());
	const int total = static_cast<int>(sources.size()) * segments * 2;
	report_progress(visibility, "route_visibility", 0, total);
	for (std::size_t source_index = 0; source_index < sources.size();
	     ++source_index) {
		const auto &source = sources[source_index];
		const int source_base = static_cast<int>(source_index) * segments * 2;
		if (!trigger_source_wall_valid(
		        snapshot, progress, source.source_wall) ||
		    !valid_trigger(snapshot, source.trigger) ||
		    state_flag(progress.trigger_in_progress, source.trigger) ||
		    !source.source_position.valid) {
			report_progress(
			    visibility, "route_visibility", source_base + segments * 2,
			    total);
			continue;
		}
		route_trigger_path_selection candidate;
		candidate.source = source;
		const bool shootable = valid_wall(snapshot, source.source_wall) &&
		                       snapshot.topology.walls[source.source_wall]
		                           .shootable_trigger;
		if (shootable) {
			auto best_distance = [&]() {
				return candidate.found
				           ? candidate.path.distance
				       : result.found ? result.path.distance
				                      : std::numeric_limits<double>::infinity();
			};
			auto accept_position = [&](int segment, const route_position &terminal,
			                           double extra_distance) {
				auto path = build_route_path(search, segment);
				path.distance += extra_distance;
				if (candidate.found &&
				    path.distance >= candidate.path.distance)
					return;
				path.progress_weight = 0;
				path.terminal_segment = segment;
				path.terminal_position = terminal;
				candidate.path = std::move(path);
				candidate.terminal_segment = segment;
				candidate.terminal_position = terminal;
				candidate.found = true;
			};
			/* Find a center-line candidate cheaply before sampling every face,
			 * vertex, and edge.  The detailed pass only examines segments whose
			 * center-path lower bound can still improve the selected route. */
			for (int index = 0; index < segments; ++index) {
				const int segment = search.visit_order[index];
				double extra_distance = 0.0;
				route_position terminal;
				if (search.nodes[segment].distance >= best_distance())
					break;
				if (visible_source_center_position(
				        snapshot, progress, source, visibility, segment,
				        terminal, extra_distance))
					accept_position(segment, terminal, extra_distance);
				if ((index & 63) == 63)
					report_progress(
					    visibility, "route_visibility",
					    source_base + index + 1, total);
			}
			report_progress(
			    visibility, "route_visibility", source_base + segments,
			    total);
			for (int index = 0; index < segments; ++index) {
				const int segment = search.visit_order[index];
				double extra_distance = 0.0;
				route_position terminal;
				if (search.nodes[segment].distance >= best_distance())
					break;
				if (visible_source_detailed_position(
				        snapshot, source, visibility, segment, terminal,
				        extra_distance))
					accept_position(segment, terminal, extra_distance);
				if ((index & 63) == 63)
					report_progress(
					    visibility, "route_visibility",
					    source_base + segments + index + 1, total);
			}
			report_progress(
			    visibility, "route_visibility", source_base + segments * 2,
			    total);
		} else if (
		    valid_segment(snapshot, source.source_segment) &&
		    search.nodes[source.source_segment].reachable) {
			candidate.path = build_route_path(
			    search, source.source_segment);
			candidate.path.distance += point_distance(
			    snapshot.topology.segments[source.source_segment].center,
			    source.source_position);
			candidate.terminal_segment = source.source_segment;
			candidate.terminal_position = source.source_position;
			candidate.path.terminal_segment = source.source_segment;
			candidate.path.terminal_position = source.source_position;
			candidate.found = true;
		}
		if (!candidate.found ||
		    (result.found && candidate.path.distance >= result.path.distance))
			continue;
		result = std::move(candidate);
	}
	report_progress(visibility, "route_visibility", total, total);
	if (result.found && visibility.wall_shootable_without_transparency &&
	    valid_wall(snapshot, result.source.source_wall))
		result.uses_transparent_surface =
		    !visibility.wall_shootable_without_transparency(
		        visibility.user, result.terminal_segment,
		        result.terminal_position, result.source.source_wall);
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

namespace
{

const char *dependency_key_name(int index)
{
	return index == 0 ? "blue" : index == 1 ? "red"
	                         : index == 2   ? "gold"
	                                        : "unknown";
}

const char *dependency_trigger_type_name(route_trigger_kind kind)
{
	switch (kind) {
		case route_trigger_kind::open_door: return "open_door";
		case route_trigger_kind::exit: return "exit";
		case route_trigger_kind::secret_exit: return "secret_exit";
		case route_trigger_kind::illusion_off: return "illusion_off";
		case route_trigger_kind::unlock_door: return "unlock_door";
		case route_trigger_kind::open_wall: return "open_wall";
		case route_trigger_kind::illusory_wall: return "illusory_wall";
		default: return "unknown";
	}
}

struct dependency_state {
	route_progress_state progress;
	std::vector<route_semantic_step> steps;
	std::string problem;
	int failed_trigger = -1;
	int failed_key = -1;
	double pending_distance = 0.0;
	route_path_result pending_path;
	int partial_frontier_segment = -1;
	std::vector<unsigned char> hidden_door_in_progress;
	std::string note;
	bool unresolved_action = false;
	std::string unresolved_action_problem;
	bool unresolved_obstruction_valid = false;
	int unresolved_obstruction_segment = -1;
	int unresolved_obstruction_side = -1;
	route_edge_decision unresolved_obstruction;
};

struct unexplored_candidate {
	int component = -1;
	int component_size = 0;
	int target_segment = -1;
	double distance = std::numeric_limits<double>::infinity();
	bool direct = false;
};

struct unexplored_inventory {
	int component_count = 0;
	std::vector<unexplored_candidate> candidates;
};

unexplored_inventory discover_unexplored_candidates(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress)
{
	unexplored_inventory result;
	const int segment_count = static_cast<int>(snapshot.topology.segments.size());
	std::vector<int> component_ids(segment_count, -1);
	std::vector<int> queue;
	queue.reserve(segment_count);
	auto optimistic_progress = progress;
	optimistic_progress.control_center_destroyed = true;
	for (int segment = 0; segment < segment_count; ++segment) {
		if (component_ids[segment] >= 0 ||
		    snapshot.state.segments[segment].explored)
			continue;
		unexplored_candidate candidate;
		candidate.component = result.component_count;
		queue.clear();
		queue.push_back(segment);
		component_ids[segment] = result.component_count;
		for (std::size_t head = 0; head < queue.size(); ++head) {
			const int current = queue[head];
			candidate.component_size++;
			for (int side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
				const int child =
				    snapshot.topology.segments[current].sides[side].child;
				if (!valid_segment(snapshot, child) ||
				    component_ids[child] >= 0 ||
				    snapshot.state.segments[child].explored)
					continue;
				const auto edge = evaluate_route_edge(
				    snapshot, query, optimistic_progress, current, side);
				if (edge.progress_cost == LEVEL_METADATA_ROUTE_EDGE_BLOCKED)
					continue;
				component_ids[child] = result.component_count;
				queue.push_back(child);
			}
		}
		result.candidates.push_back(candidate);
		result.component_count++;
	}
	if (result.component_count == 0)
		return result;
	const auto update_candidate = [&](int segment, double distance, bool direct) {
		const int component = component_ids[segment];
		if (component < 0)
			return;
		auto &candidate = result.candidates[component];
		if (candidate.target_segment >= 0 &&
		    (distance > candidate.distance ||
		     (distance == candidate.distance &&
		      segment >= candidate.target_segment)))
			return;
		candidate.target_segment = segment;
		candidate.distance = distance;
		candidate.direct = direct;
	};
	const auto direct = search_routes(snapshot, query, progress, false);
	if (direct.problem.empty())
		for (int segment = 0; segment < segment_count; ++segment)
			if (direct.nodes[segment].reachable)
				update_candidate(segment, direct.nodes[segment].distance, true);
	const auto optimistic = search_routes(
	    snapshot, query, optimistic_progress, true);
	if (optimistic.problem.empty())
		for (int segment = 0; segment < segment_count; ++segment) {
			const int component = component_ids[segment];
			if (component >= 0 &&
			    result.candidates[component].target_segment < 0 &&
			    optimistic.nodes[segment].reachable)
				update_candidate(
				    segment, optimistic.nodes[segment].distance, false);
		}
	result.candidates.erase(
	    std::remove_if(
	        result.candidates.begin(), result.candidates.end(),
	        [](const unexplored_candidate &candidate) {
		        return candidate.target_segment < 0;
	        }),
	    result.candidates.end());
	std::sort(
	    result.candidates.begin(), result.candidates.end(),
	    [](const unexplored_candidate &left,
	       const unexplored_candidate &right) {
		    if (left.component_size != right.component_size)
			    return left.component_size > right.component_size;
		    if (left.direct != right.direct)
			    return left.direct > right.direct;
		    if (left.distance != right.distance)
			    return left.distance < right.distance;
		    return left.target_segment < right.target_segment;
	    });
	return result;
}

class dependency_planner
{
  public:
	dependency_planner(
	    const route_snapshot &snapshot,
	    const route_query &query,
	    const route_progress_state &progress,
	    const route_visibility_query &visibility,
	    bool allow_unresolved_triggers = false)
	    : snapshot_(snapshot), query_(query), visibility_(visibility),
	      targets_(discover_route_targets(snapshot)),
	      allow_unresolved_triggers_(allow_unresolved_triggers)
	{
		state_.progress = progress;
		state_.hidden_door_in_progress.resize(snapshot.state.walls.size());
	}

	route_dependency_result resolve_trigger(int segment, int side)
	{
		route_dependency_result result;
		result.progress = state_.progress;
		const auto sources = discover_trigger_sources_internal(
		    snapshot_, state_.progress, segment, side, true);
		if (sources.empty())
			return result;
		result.attempted = true;
		result.resolved = fire_trigger(segment, side, 0);
		result.progress = state_.progress;
		result.steps = std::move(state_.steps);
		result.problem = std::move(state_.problem);
		result.failed_trigger = state_.failed_trigger;
		result.failed_key = state_.failed_key;
		result.pending_distance = state_.pending_distance;
		result.pending_path = std::move(state_.pending_path);
		return result;
	}

	route_plan_result plan_end_level()
	{
		if (!initialize_route())
			return finish_partial("route incomplete");
		if (!targets_.reactor_found)
			state_.note = targets_.exits.empty()
			                  ? "missing reactor"
			                  : "no reactor, exit exists";
		if (targets_.exits.empty()) {
			set_problem("missing exit");
			return finish_partial("route incomplete");
		}
		bool progressed = false;
		if (!progress_primary(progressed))
			return finish_partial("route incomplete");
		const auto selected = select_route_target(
		    snapshot_, query_, state_.progress, targets_.exits);
		if (!selected.found) {
			set_problem("exit unreachable");
			return finish_partial("route incomplete");
		}
		const auto &target = targets_.exits[selected.selected_index];
		const auto activation = exit_activation_position(snapshot_, target);
		if (!move_to_target(target.segment, activation, 0) ||
		    !append_target_step(
		        route_semantic_step_kind::exit, target, "Exit"))
			return finish_partial("route incomplete");
		return make_plan_result(route_plan_status::ok);
	}

	route_plan_result plan_segment(int segment)
	{
		if (!initialize_route())
			return finish_partial("unexplored route incomplete");
		if (!valid_segment(snapshot_, segment) ||
		    !snapshot_.topology.segments[segment].center.valid) {
			set_problem("unexplored target missing center");
			return finish_partial("unexplored route incomplete");
		}
		route_target target;
		target.segment = segment;
		target.position = snapshot_.topology.segments[segment].center;
		if (!move_to_endpoint(target) ||
		    !append_target_step(
		        route_semantic_step_kind::unexplored, target,
		        "Unexplored"))
			return finish_partial("unexplored route incomplete");
		return make_plan_result(route_plan_status::ok);
	}

	route_plan_result plan_unexplored()
	{
		if (!initialize_route())
			return finish_partial("unexplored route incomplete");
		const auto prefix = state_;
		const auto inventory = discover_unexplored_candidates(
		    snapshot_, query_, state_.progress);
		if (inventory.component_count <= 0) {
			set_problem("no unexplored area");
			return finish_partial("unexplored route incomplete");
		}
		for (const auto &candidate : inventory.candidates) {
			state_ = prefix;
			route_target target;
			target.segment = candidate.target_segment;
			target.position =
			    snapshot_.topology.segments[target.segment].center;
			if (!move_to_endpoint(target) ||
			    !append_target_step(
			        route_semantic_step_kind::unexplored, target,
			        "Unexplored"))
				continue;
			auto result = make_plan_result(route_plan_status::ok);
			result.unexplored_component_size = candidate.component_size;
			result.unexplored_target_segment = target.segment;
			result.unexplored_waypoint_segment =
			    result.steps.size() > 1 ? result.steps[1].segment
			                            : target.segment;
			result.unexplored_direct_reachable = candidate.direct;
			return result;
		}
		state_ = prefix;
		set_problem("no reachable unexplored area");
		return finish_partial("unexplored route incomplete");
	}

  private:
	void set_problem(const std::string &problem)
	{
		if (state_.problem.empty())
			state_.problem = problem;
	}

	route_plan_result make_plan_result(route_plan_status status)
	{
		route_plan_result result;
		result.status = status;
		result.progress = state_.progress;
		result.steps = std::move(state_.steps);
		result.problem = std::move(state_.problem);
		if (state_.unresolved_action) {
			if (status == route_plan_status::ok)
				result.problem.clear();
			else if (result.problem.empty())
				result.problem = state_.unresolved_action_problem.empty()
				                     ? "switch activation route unresolved"
				                     : state_.unresolved_action_problem;
		}
		result.note = std::move(state_.note);
		result.partial_frontier_segment = state_.partial_frontier_segment;
		for (const auto &step : result.steps)
			if (std::isfinite(step.distance_from_previous) &&
			    step.distance_from_previous > 0.0)
				result.travel_distance += step.distance_from_previous;
		return result;
	}

	bool initialize_route()
	{
		if (!valid_segment(snapshot_, state_.progress.current_segment) ||
		    !state_.progress.current_position.valid) {
			set_problem("missing player start");
			return false;
		}
		route_semantic_step step;
		step.kind = route_semantic_step_kind::start;
		step.segment = state_.progress.current_segment;
		step.label = "Start";
		return append_step(std::move(step));
	}

	void note_unresolved_obstruction(
	    int segment,
	    int side,
	    const route_edge_decision &obstruction)
	{
		if (state_.unresolved_obstruction_valid ||
		    (obstruction.blocker != route_edge_blocker::trigger &&
		     obstruction.blocker != route_edge_blocker::hidden_door))
			return;
		state_.unresolved_obstruction_valid = true;
		state_.unresolved_obstruction_segment = segment;
		state_.unresolved_obstruction_side = side;
		state_.unresolved_obstruction = obstruction;
	}

	route_plan_result finish_partial(const char *problem)
	{
		append_unresolved_obstruction();
		if (state_.problem.empty())
			set_problem(problem);
		return make_plan_result(
		    state_.steps.size() > 1 ? route_plan_status::partial
		                            : route_plan_status::failed);
	}

	route_path_result path_to_position(
	    int segment,
	    const route_position &position,
	    bool optimistic,
	    route_key_requirement forbidden_key = route_key_requirement::none)
	{
		route_search_options options;
		options.optimistic = optimistic;
		options.prioritize_progress =
		    forbidden_key == route_key_requirement::none;
		options.forbidden_missing_key = forbidden_key;
		const auto search = search_routes(
		    snapshot_, query_, state_.progress, options);
		if (!search.problem.empty())
			return {};
		auto path = build_route_path(search, segment);
		if (!path.reached || !position.valid || !valid_segment(snapshot_, segment) ||
		    !snapshot_.topology.segments[segment].center.valid)
			return {};
		path.distance += point_distance(
		    snapshot_.topology.segments[segment].center, position);
		path.terminal_segment = segment;
		path.terminal_position = position;
		return path;
	}

	route_path_result visible_path_to_target(const route_target &target)
	{
		const auto search = search_routes(
		    snapshot_, query_, state_.progress, false);
		if (!search.problem.empty())
			return {};
		route_trigger_source visibility_target;
		visibility_target.source_segment = target.segment;
		visibility_target.source_position = target.position;
		const int total = static_cast<int>(search.visit_order.size());
		report_progress(visibility_, "route_target_visibility", 0, total);
		for (int index = 0; index < total; ++index) {
			const int segment = search.visit_order[index];
			double extra_distance = 0.0;
			route_position terminal;
			if (!visible_source_position(
			        snapshot_, state_.progress, visibility_target,
			        visibility_, segment, terminal, extra_distance)) {
				if ((index & 63) == 63)
					report_progress(
					    visibility_, "route_target_visibility", index + 1,
					    total);
				continue;
			}
			auto path = build_route_path(search, segment);
			path.distance += extra_distance;
			path.progress_weight = 0;
			path.terminal_segment = segment;
			path.terminal_position = terminal;
			report_progress(
			    visibility_, "route_target_visibility", total, total);
			return path;
		}
		report_progress(
		    visibility_, "route_target_visibility", total, total);
		return {};
	}

	void accumulate_path(const route_path_result &path)
	{
		route_progress_traverse_path(snapshot_, state_.progress, path);
		state_.pending_distance += path.distance;
		state_.pending_path.reached = true;
		state_.pending_path.distance += path.distance;
		state_.pending_path.progress_weight += path.progress_weight;
		if (path.terminal_position.valid) {
			state_.pending_path.terminal_segment = path.terminal_segment;
			state_.pending_path.terminal_position = path.terminal_position;
		}
		if (state_.pending_path.segments.empty()) {
			state_.pending_path.segments = path.segments;
			state_.pending_path.sides = path.sides;
			return;
		}
		std::size_t segment_start = 0;
		if (!path.segments.empty() &&
		    state_.pending_path.segments.back() == path.segments.front())
			segment_start = 1;
		state_.pending_path.segments.insert(
		    state_.pending_path.segments.end(),
		    path.segments.begin() + static_cast<std::ptrdiff_t>(segment_start),
		    path.segments.end());
		state_.pending_path.sides.insert(
		    state_.pending_path.sides.end(), path.sides.begin(), path.sides.end());
	}

	void finish_step(route_semantic_step &step)
	{
		step.activation_position = state_.progress.current_position;
		step.distance_from_previous = state_.pending_distance;
		step.path = std::move(state_.pending_path);
		state_.pending_distance = 0.0;
		state_.pending_path = {};
	}

	bool append_step(route_semantic_step step)
	{
		if (state_.steps.size() >= LEVEL_METADATA_MAX_ROUTE_STEPS) {
			set_problem("route step limit");
			return false;
		}
		finish_step(step);
		if (step.activation == route_activation_kind::unresolved_trigger)
			step.activation_position = {};
		state_.steps.push_back(std::move(step));
		return true;
	}

	int reverse_wall(int wall) const
	{
		if (!valid_wall(snapshot_, wall))
			return -1;
		const auto &source = snapshot_.topology.walls[wall];
		if (!valid_segment(snapshot_, source.segment) || source.side < 0 ||
		    source.side >= LEVEL_METADATA_MAX_SIDES)
			return -1;
		const auto &side =
		    snapshot_.topology.segments[source.segment].sides[source.side];
		if (!valid_segment(snapshot_, side.child) || side.reverse_side < 0 ||
		    side.reverse_side >= LEVEL_METADATA_MAX_SIDES)
			return -1;
		return snapshot_.topology.segments[side.child]
		    .sides[side.reverse_side]
		    .wall;
	}

	void set_wall_pair(std::vector<unsigned char> &values, int wall, bool value)
	{
		if (wall >= 0 && wall < static_cast<int>(values.size()))
			values[wall] = value;
		const int reverse = reverse_wall(wall);
		if (reverse >= 0 && reverse < static_cast<int>(values.size()))
			values[reverse] = value;
	}

	bool append_hidden_door_step(int segment, int side, int wall)
	{
		route_semantic_step step;
		step.kind = route_semantic_step_kind::hidden_door;
		step.segment = segment;
		step.side = side;
		step.wall = wall;
		step.activation = route_activation_kind::open_hidden_door;
		step.label = "Open hidden door";
		if (valid_wall(snapshot_, wall)) {
			step.aim_position = snapshot_.topology.walls[wall].target;
			step.label_position = step.aim_position;
		}
		step.opened_links.push_back({ segment, side, wall });
		const int reverse = reverse_wall(wall);
		if (valid_wall(snapshot_, reverse) &&
		    step.opened_links.size() < LEVEL_METADATA_MAX_ROUTE_LINKS) {
			const auto &reverse_topology = snapshot_.topology.walls[reverse];
			step.opened_links.push_back({ reverse_topology.segment, reverse_topology.side, reverse });
		}
		return append_step(std::move(step));
	}

	bool append_blastable_wall_step(int segment, int side, int wall)
	{
		route_semantic_step step;
		step.kind = route_semantic_step_kind::blastable_wall;
		step.segment = segment;
		step.side = side;
		step.wall = wall;
		step.activation = route_activation_kind::destroy_blastable_wall;
		step.label = "Destroy blastable wall";
		if (valid_wall(snapshot_, wall)) {
			step.aim_position = snapshot_.topology.walls[wall].target;
			step.label_position = step.aim_position;
		}
		step.opened_links.push_back({ segment, side, wall });
		const int reverse = reverse_wall(wall);
		if (valid_wall(snapshot_, reverse) &&
		    step.opened_links.size() < LEVEL_METADATA_MAX_ROUTE_LINKS) {
			const auto &reverse_topology = snapshot_.topology.walls[reverse];
			step.opened_links.push_back(
			    { reverse_topology.segment, reverse_topology.side, reverse });
		}
		return append_step(std::move(step));
	}

	bool first_blastable_wall(
	    const route_path_result &path,
	    int &segment,
	    int &side,
	    int &wall) const
	{
		for (std::size_t index = 0;
		     index < path.sides.size() && index < path.segments.size(); ++index) {
			const auto edge = evaluate_route_edge(
			    snapshot_, query_, state_.progress, path.segments[index],
			    path.sides[index]);
			if (edge.action != route_required_action::destroy_blastable_wall)
				continue;
			segment = path.segments[index];
			side = path.sides[index];
			wall = edge.wall;
			return true;
		}
		return false;
	}

	bool side_is_route_exit(int segment, int side) const
	{
		if (!valid_segment(snapshot_, segment) || side < 0 ||
		    side >= LEVEL_METADATA_MAX_SIDES)
			return false;
		const auto &topology_side =
		    snapshot_.topology.segments[segment].sides[side];
		if (topology_side.child == -2)
			return true;
		if (!snapshot_.state.segments[segment].sides[side].exit_trigger)
			return false;
		const int wall = topology_side.wall;
		if (!valid_wall(snapshot_, wall))
			return true;
		const int trigger = snapshot_.state.walls[wall].trigger;
		return !valid_trigger(snapshot_, trigger) ||
		       snapshot_.topology.triggers[trigger].kind !=
		           route_trigger_kind::secret_exit;
	}

	bool append_target_step(
	    route_semantic_step_kind kind,
	    const route_target &target,
	    const char *label)
	{
		route_semantic_step step;
		step.kind = kind;
		step.segment = target.segment;
		step.label = label;
		step.aim_position = target.position;
		step.label_position = target.position;
		if (kind == route_semantic_step_kind::reactor)
			step.activation = route_activation_kind::destroy_reactor;
		else if (kind == route_semantic_step_kind::boss)
			step.activation = route_activation_kind::destroy_boss;
		else if (kind == route_semantic_step_kind::exit) {
			step.activation = route_activation_kind::enter_exit;
			step.side = side_is_route_exit(target.segment, target.side)
			                ? target.side
			                : -1;
			for (int side = 0;
			     step.side < 0 && side < LEVEL_METADATA_MAX_SIDES;
			     ++side)
				if (side_is_route_exit(target.segment, side))
					step.side = side;
			if (step.side >= 0)
				step.wall = snapshot_.topology.segments[target.segment]
				                .sides[step.side]
				                .wall;
			if (valid_wall(snapshot_, step.wall)) {
				step.trigger = snapshot_.state.walls[step.wall].trigger;
				if (valid_trigger(snapshot_, step.trigger)) {
					const auto &trigger =
					    snapshot_.topology.triggers[step.trigger];
					step.trigger_raw_type = trigger.raw_type;
					step.trigger_type_name =
					    dependency_trigger_type_name(trigger.kind);
				}
			}
		}
		return append_step(std::move(step));
	}

	void append_unresolved_obstruction()
	{
		if (!state_.unresolved_obstruction_valid)
			return;
		if (state_.unresolved_obstruction.blocker ==
		    route_edge_blocker::trigger) {
			const auto sources = discover_trigger_sources_internal(
			    snapshot_, state_.progress,
			    state_.unresolved_obstruction_segment,
			    state_.unresolved_obstruction_side, true);
			if (!sources.empty())
				append_trigger_step(sources.front());
		} else if (state_.unresolved_obstruction.blocker ==
		           route_edge_blocker::hidden_door)
			append_hidden_door_step(
			    state_.unresolved_obstruction_segment,
			    state_.unresolved_obstruction_side,
			    state_.unresolved_obstruction.wall);
	}

	bool open_hidden_door(int segment, int side, int wall, int depth)
	{
		if (!valid_wall(snapshot_, wall)) {
			set_problem("unknown hidden door route dependency");
			return false;
		}
		if (state_flag(state_.progress.opened_hidden_walls, wall))
			return true;
		if (state_flag(state_.hidden_door_in_progress, wall) ||
		    state_flag(state_.hidden_door_in_progress, reverse_wall(wall))) {
			set_problem("hidden door route dependency loop");
			return false;
		}
		route_position position;
		if (valid_segment(snapshot_, segment))
			position = snapshot_.topology.segments[segment].center;
		if (!position.valid && valid_segment(snapshot_, segment) && side >= 0 &&
		    side < LEVEL_METADATA_MAX_SIDES)
			position = snapshot_.topology.segments[segment].sides[side].center;
		if (!position.valid) {
			set_problem("hidden door source missing");
			return false;
		}
		set_wall_pair(state_.hidden_door_in_progress, wall, true);
		if (!move_to_target(segment, position, depth + 1)) {
			set_wall_pair(state_.hidden_door_in_progress, wall, false);
			return false;
		}
		state_.progress.current_segment = segment;
		state_.progress.current_position = position;
		if (!append_hidden_door_step(segment, side, wall)) {
			set_wall_pair(state_.hidden_door_in_progress, wall, false);
			return false;
		}
		route_progress_open_hidden_wall(snapshot_, state_.progress, wall);
		set_wall_pair(state_.hidden_door_in_progress, wall, false);
		return true;
	}

	bool append_key_step(const route_target &target, int key)
	{
		route_semantic_step step;
		const bool robot_carrier =
		    target.contained && target.object >= 0 &&
		    static_cast<std::size_t>(target.object) < snapshot_.state.objects.size() &&
		    snapshot_.state.objects[target.object].kind == route_object_kind::robot;
		step.kind = route_semantic_step_kind::key;
		step.segment = target.segment;
		step.key = target.key;
		if (robot_carrier) {
			step.key_carrier_object = target.object;
			step.activation = route_activation_kind::destroy_key_carrier;
			step.label =
			    std::string("Destroy robot carrying ") +
			    dependency_key_name(key) + " key";
		} else {
			step.activation = route_activation_kind::pickup_key;
			step.label = std::string(dependency_key_name(key)) + " key";
		}
		step.aim_position = target.position;
		step.label_position = target.position;
		return append_step(std::move(step));
	}

	bool acquire_key(int key, int depth)
	{
		if (key < 0 || key >= 3) {
			set_problem("unknown key route dependency");
			return false;
		}
		const int bit = 1 << key;
		if ((state_.progress.key_mask & bit) != 0)
			return true;
		if ((state_.progress.key_in_progress & bit) != 0) {
			set_problem(
			    std::string(dependency_key_name(key)) +
			    " key route dependency loop");
			state_.failed_key = key;
			return false;
		}
		if (targets_.keys[key].empty()) {
			set_problem(std::string(dependency_key_name(key)) + " key missing");
			return false;
		}
		const route_key_requirement keys[3] = {
			route_key_requirement::blue,
			route_key_requirement::red,
			route_key_requirement::gold,
		};
		const auto selected = select_key_target(
		    snapshot_, query_, state_.progress, keys[key], targets_.keys[key]);
		if (!selected.found) {
			set_problem(
			    std::string(dependency_key_name(key)) + " key unreachable");
			state_.failed_key = key;
			return false;
		}
		state_.progress.key_in_progress |= bit;
		const auto &target = targets_.keys[key][selected.selected_index];
		if (!move_to_target(target.segment, target.position, depth + 1)) {
			state_.progress.key_in_progress &= ~bit;
			return false;
		}
		append_key_step(target, key);
		state_.progress.key_mask |= bit;
		state_.progress.key_in_progress &= ~bit;
		return true;
	}

	bool acquire_recovery_key(int depth)
	{
		static constexpr int recovery_order[] = { 0, 2, 1 };
		std::string last_problem;
		for (const int key : recovery_order) {
			const int bit = 1 << key;
			if (((state_.progress.key_mask | state_.progress.key_in_progress |
			      state_.progress.avoided_key_mask) &
			     bit) != 0 ||
			    targets_.keys[key].empty())
				continue;
			const auto saved = state_;
			state_.problem.clear();
			if (acquire_key(key, depth + 1))
				return true;
			if (last_problem.empty() && !state_.problem.empty())
				last_problem = state_.problem;
			state_ = saved;
		}
		if (!last_problem.empty())
			state_.problem = last_problem;
		return false;
	}

	bool move_to_target_or_visible(const route_target &target, int depth)
	{
		if (move_to_target(target.segment, target.position, depth))
			return true;
		const std::string saved_problem = state_.problem;
		state_.problem.clear();
		const auto visible = visible_path_to_target(target);
		if (!visible.reached) {
			set_problem(
			    !saved_problem.empty() ? saved_problem : "route target unreachable");
			return false;
		}
		accumulate_path(visible);
		state_.progress.current_segment = visible.terminal_segment;
		state_.progress.current_position = visible.terminal_position;
		return true;
	}

	bool move_primary_with_key_recovery(const route_target &target)
	{
		const auto initial = state_;
		std::string last_problem;
		if (move_to_target_or_visible(target, 0))
			return true;
		if (!state_.problem.empty())
			last_problem = state_.problem;
		state_ = initial;
		for (int attempt = 0; attempt < 3; ++attempt) {
			state_.problem.clear();
			if (!acquire_recovery_key(0))
				break;
			const auto key_state = state_;
			state_.problem.clear();
			if (move_to_target_or_visible(target, 0))
				return true;
			if (!state_.problem.empty())
				last_problem = state_.problem;
			state_ = key_state;
		}
		if (state_.problem.empty() && !last_problem.empty())
			state_.problem = last_problem;
		return false;
	}

	bool progress_primary(bool &progressed)
	{
		progressed = false;
		if (state_.progress.control_center_destroyed)
			return true;
		if (targets_.boss_found) {
			if (!move_primary_with_key_recovery(targets_.boss) ||
			    !append_target_step(
			        route_semantic_step_kind::boss, targets_.boss,
			        "Boss robot"))
				return false;
			state_.progress.control_center_destroyed = true;
			progressed = true;
		} else if (targets_.reactor_found) {
			if (!move_primary_with_key_recovery(targets_.reactor) ||
			    !append_target_step(
			        route_semantic_step_kind::reactor, targets_.reactor,
			        "Reactor"))
				return false;
			state_.progress.control_center_destroyed = true;
			progressed = true;
		}
		return true;
	}

	bool move_to_endpoint(const route_target &target)
	{
		const auto initial = state_;
		if (move_to_target(target.segment, target.position, 0))
			return true;
		const std::string direct_problem = state_.problem;
		const int direct_frontier = state_.partial_frontier_segment;
		state_ = initial;
		state_.problem.clear();
		bool progressed = false;
		if (progress_primary(progressed) && progressed &&
		    move_to_target(target.segment, target.position, 0))
			return true;
		if (!progressed) {
			state_ = initial;
			state_.partial_frontier_segment = direct_frontier;
			if (!direct_problem.empty())
				state_.problem = direct_problem;
		}
		return false;
	}

	bool append_trigger_step(
	    route_trigger_source source,
	    bool use_current_endpoint = false,
	    bool unresolved = false)
	{
		if (!valid_trigger(snapshot_, source.trigger))
			return false;
		route_semantic_step step;
		step.kind = route_semantic_step_kind::trigger;
		step.segment = unresolved && valid_segment(
		                                 snapshot_, state_.partial_frontier_segment)
		                   ? state_.partial_frontier_segment
		               : unresolved
		                   ? state_.progress.current_segment
		               : use_current_endpoint
		                   ? state_.progress.current_segment
		                   : source.source_segment;
		step.side = (unresolved || use_current_endpoint) &&
		                    step.segment != source.source_segment
		                ? -1
		                : source.source_side;
		step.wall = source.source_wall;
		step.trigger = source.trigger;
		const auto &trigger = snapshot_.topology.triggers[source.trigger];
		step.trigger_raw_type = trigger.raw_type;
		step.trigger_type_name = dependency_trigger_type_name(trigger.kind);
		if (unresolved)
			step.activation = route_activation_kind::unresolved_trigger;
		else if (valid_wall(snapshot_, source.source_wall) &&
		         snapshot_.topology.walls[source.source_wall].shootable_trigger)
			step.activation = route_activation_kind::shoot_switch;
		else if (valid_wall(snapshot_, source.source_wall) &&
		         snapshot_.state.walls[source.source_wall].kind ==
		             route_wall_kind::open)
			step.activation = route_activation_kind::fly_through_trigger;
		else
			step.activation = route_activation_kind::pass_through_trigger;
		const char *action =
		    step.activation == route_activation_kind::unresolved_trigger
		        ? "Locate and activate switch"
		    : step.activation == route_activation_kind::shoot_switch
		        ? "Shoot switch"
		    : step.activation == route_activation_kind::fly_through_trigger
		        ? "Fly-through"
		        : "Pass through";
		step.label = std::string(action) + " trigger " +
		             std::to_string(source.trigger);
		if (step.activation == route_activation_kind::fly_through_trigger &&
		    valid_segment(snapshot_, source.source_segment))
			step.aim_position = crossing_aim_position(
			    snapshot_, source, state_.progress.current_position);
		if (valid_wall(snapshot_, source.source_wall)) {
			if (!step.aim_position.valid)
				step.aim_position =
				    snapshot_.topology.walls[source.source_wall].target;
			step.label_position = step.aim_position;
		}
		for (const auto &link : trigger.links) {
			if (step.opened_links.size() >= LEVEL_METADATA_MAX_ROUTE_LINKS)
				break;
			int wall = -1;
			if (valid_segment(snapshot_, link.segment) && link.side >= 0 &&
			    link.side < LEVEL_METADATA_MAX_SIDES)
				wall = snapshot_.topology.segments[link.segment].sides[link.side].wall;
			step.opened_links.push_back({ link.segment, link.side, wall });
		}
		return append_step(std::move(step));
	}

	bool find_unresolved_trigger_source(
	    int segment,
	    int side,
	    int trigger,
	    route_trigger_source &source)
	{
		const auto sources = discover_trigger_sources_internal(
		    snapshot_, state_.progress, segment, side, true);
		for (const auto &candidate : sources) {
			if (candidate.trigger != trigger ||
			    !valid_wall(snapshot_, candidate.source_wall) ||
			    !snapshot_.topology.walls[candidate.source_wall]
			         .shootable_trigger)
				continue;
			source = candidate;
			return true;
		}
		return false;
	}

	bool append_unresolved_trigger(
	    int segment,
	    int side,
	    int trigger,
	    const std::string &problem)
	{
		route_trigger_source source;
		if (!find_unresolved_trigger_source(segment, side, trigger, source))
			return false;
		if (!append_trigger_step(source, false, true) ||
		    !route_progress_fire_trigger(state_.progress, trigger))
			return false;
		state_.unresolved_action = true;
		if (state_.unresolved_action_problem.empty())
			state_.unresolved_action_problem =
			    problem.empty() ? "switch activation route unresolved" : problem;
		if (state_.problem.empty())
			state_.problem = state_.unresolved_action_problem;
		return true;
	}

	bool fire_trigger(int segment, int side, int depth)
	{
		auto raw_sources = discover_trigger_sources_internal(
		    snapshot_, state_.progress, segment, side, true);
		raw_sources.erase(
		    std::remove_if(
		        raw_sources.begin(), raw_sources.end(), [&](const auto &source) {
			        return !trigger_source_fits_navigator(snapshot_, query_, source);
		        }),
		    raw_sources.end());
		if (raw_sources.empty()) {
			set_problem("trigger source missing");
			return false;
		}
		auto source = raw_sources.front();
		auto firing_sources = discover_trigger_sources_internal(
		    snapshot_, state_.progress, segment, side, false);
		firing_sources.erase(
		    std::remove_if(
		        firing_sources.begin(), firing_sources.end(), [&](const auto &candidate) {
			        return !trigger_source_fits_navigator(
			            snapshot_, query_, candidate);
		        }),
		    firing_sources.end());
		const auto firing = select_trigger_firing_path(
		    snapshot_, query_, state_.progress, firing_sources, visibility_);
		if (firing.found)
			source = firing.source;
		const bool shootable = valid_wall(snapshot_, source.source_wall) &&
		                       snapshot_.topology.walls[source.source_wall]
		                           .shootable_trigger;
		if (state_flag(state_.progress.fired_triggers, source.trigger))
			return true;
		if (state_flag(state_.progress.trigger_in_progress, source.trigger)) {
			set_problem(
			    "trigger route dependency loop: trigger " +
			    std::to_string(source.trigger) + " source " +
			    std::to_string(source.source_segment) + ":" +
			    std::to_string(source.source_side) + " wall " +
			    std::to_string(source.source_wall) + " target " +
			    std::to_string(source.target_segment) + ":" +
			    std::to_string(source.target_side));
			state_.failed_trigger = source.trigger;
			return false;
		}
		if (!source.source_position.valid ||
		    !valid_segment(snapshot_, source.source_segment)) {
			set_problem("trigger source missing");
			state_.failed_trigger = source.trigger;
			return false;
		}
		state_.progress.trigger_in_progress[source.trigger] = 1;
		const int selected_source_segment = source.source_segment;
		if (firing.found) {
			accumulate_path(firing.path);
			state_.progress.current_segment = firing.terminal_segment;
			state_.progress.current_position = firing.terminal_position;
			if (shootable) {
				source.source_segment = firing.terminal_segment;
				source.source_position = firing.terminal_position;
				if (source.source_segment != selected_source_segment)
					source.source_side = -1;
			} else if (
			    firing.terminal_segment == source.source_segment &&
			    valid_wall(snapshot_, source.source_wall) &&
			    snapshot_.state.walls[source.source_wall].kind ==
			        route_wall_kind::open &&
			    snapshot_.topology.segments[source.source_segment].center.valid)
				state_.progress.current_position =
				    snapshot_.topology.segments[source.source_segment].center;
		} else if (!move_to_target(
		               source.source_segment, source.source_position, depth + 1)) {
			state_.progress.trigger_in_progress[source.trigger] = 0;
			return false;
		} else {
			state_.progress.current_segment = source.source_segment;
			state_.progress.current_position = source.source_position;
		}
		if (!append_trigger_step(source, firing.found && !shootable)) {
			state_.progress.trigger_in_progress[source.trigger] = 0;
			return false;
		}
		if (firing.found && firing.uses_transparent_surface)
			state_.steps.back().uses_transparent_surface = true;
		state_.progress.fired_triggers[source.trigger] = 1;
		state_.progress.trigger_in_progress[source.trigger] = 0;
		return true;
	}

	bool move_to_target(
	    int goal_segment,
	    const route_position &goal_position,
	    int depth)
	{
		if (depth > LEVEL_METADATA_MAX_ROUTE_STEPS) {
			set_problem("route dependency depth limit");
			return false;
		}
		const int saved_avoided_keys = state_.progress.avoided_key_mask;
		const auto saved_avoided_triggers = state_.progress.avoided_triggers;
		std::string last_dependency_problem;
		std::string unresolved_trigger_problem;
		int unresolved_trigger_segment = -1;
		int unresolved_trigger_side = -1;
		int unresolved_trigger = -1;
		for (int guard = 0; guard < LEVEL_METADATA_MAX_ROUTE_STEPS; ++guard) {
			const auto direct = path_to_position(
			    goal_segment, goal_position, false);
			if (direct.reached) {
				int blast_segment = -1;
				int blast_side = -1;
				int blast_wall = -1;
				if (first_blastable_wall(
				        direct, blast_segment, blast_side, blast_wall)) {
					if (!move_to_target(
					        blast_segment,
					        snapshot_.topology.segments[blast_segment].center,
					        depth + 1) ||
					    !append_blastable_wall_step(
					        blast_segment, blast_side, blast_wall) ||
					    !route_progress_destroy_blastable_wall(
					        snapshot_, state_.progress, blast_wall))
						return false;
					continue;
				}
				accumulate_path(direct);
				state_.progress.current_segment = goal_segment;
				state_.progress.current_position = goal_position;
				state_.progress.avoided_key_mask = saved_avoided_keys;
				state_.progress.avoided_triggers = saved_avoided_triggers;
				state_.failed_key = -1;
				state_.failed_trigger = -1;
				return true;
			}
			const auto optimistic = path_to_position(
			    goal_segment, goal_position, true);
			if (!optimistic.reached || !optimistic.has_obstruction) {
				state_.problem.clear();
				if (acquire_recovery_key(depth + 1)) {
					state_.progress.avoided_key_mask = saved_avoided_keys;
					state_.progress.avoided_triggers = saved_avoided_triggers;
					state_.failed_key = -1;
					state_.failed_trigger = -1;
					continue;
				}
				if (unresolved_trigger >= 0 &&
				    append_unresolved_trigger(
				        unresolved_trigger_segment, unresolved_trigger_side,
				        unresolved_trigger, unresolved_trigger_problem)) {
					if (unresolved_trigger < static_cast<int>(
					                             state_.progress.avoided_triggers.size()))
						state_.progress.avoided_triggers[unresolved_trigger] = 0;
					unresolved_trigger = -1;
					continue;
				}
				set_problem(
				    !last_dependency_problem.empty()
				        ? last_dependency_problem
				        : "route target unreachable");
				state_.progress.avoided_key_mask = saved_avoided_keys;
				state_.progress.avoided_triggers = saved_avoided_triggers;
				return false;
			}
			const auto &block = optimistic.first_obstruction;
			if (valid_segment(snapshot_, optimistic.first_obstruction_segment))
				state_.partial_frontier_segment =
				    optimistic.first_obstruction_segment;
			if (block.blocker == route_edge_blocker::missing_key) {
				const auto saved = state_;
				state_.failed_key = -1;
				const int key = key_index(block.key);
				if (!acquire_key(key, depth + 1)) {
					const std::string key_problem = state_.problem;
					const int failed = state_.failed_key >= 0
					                       ? state_.failed_key
					                       : key;
					state_ = saved;
					state_.failed_key = failed;
					last_dependency_problem = key_problem;
					state_.problem.clear();
					if (failed >= 0 && failed < 3)
						state_.progress.avoided_key_mask |= 1 << failed;
				}
				continue;
			}
			if (block.blocker == route_edge_blocker::trigger) {
				const auto saved = state_;
				state_.failed_trigger = -1;
				if (!fire_trigger(
				        optimistic.first_obstruction_segment,
				        optimistic.first_obstruction_side, depth + 1)) {
					const bool avoidable =
					    state_.problem == "trigger source missing" ||
					    state_.problem.rfind(
					        "trigger route dependency loop", 0) == 0;
					if (avoidable && valid_trigger(snapshot_, block.trigger)) {
						const std::string trigger_problem = state_.problem;
						const int failed = state_.failed_trigger >= 0
						                       ? state_.failed_trigger
						                       : block.trigger;
						state_ = saved;
						state_.failed_trigger = failed;
						last_dependency_problem = trigger_problem;
						if (allow_unresolved_triggers_ && !state_flag(
						                                      saved.progress.trigger_in_progress, failed)) {
							route_trigger_source unresolved_source;
							if (find_unresolved_trigger_source(
							        optimistic.first_obstruction_segment,
							        optimistic.first_obstruction_side, failed,
							        unresolved_source)) {
								unresolved_trigger_segment =
								    optimistic.first_obstruction_segment;
								unresolved_trigger_side =
								    optimistic.first_obstruction_side;
								unresolved_trigger = failed;
								unresolved_trigger_problem = trigger_problem;
							}
						}
						if (failed >= 0 &&
						    failed < static_cast<int>(
						                 state_.progress.avoided_triggers.size()))
							state_.progress.avoided_triggers[failed] = 1;
						continue;
					}
					note_unresolved_obstruction(
					    optimistic.first_obstruction_segment,
					    optimistic.first_obstruction_side, block);
					state_.progress.avoided_key_mask = saved_avoided_keys;
					state_.progress.avoided_triggers = saved_avoided_triggers;
					return false;
				}
				continue;
			}
			if (block.blocker == route_edge_blocker::hidden_door) {
				if (!open_hidden_door(
				        optimistic.first_obstruction_segment,
				        optimistic.first_obstruction_side, block.wall,
				        depth + 1)) {
					note_unresolved_obstruction(
					    optimistic.first_obstruction_segment,
					    optimistic.first_obstruction_side, block);
					state_.progress.avoided_key_mask = saved_avoided_keys;
					state_.progress.avoided_triggers = saved_avoided_triggers;
					return false;
				}
				continue;
			}
			set_problem("unsupported route dependency");
			state_.progress.avoided_key_mask = saved_avoided_keys;
			state_.progress.avoided_triggers = saved_avoided_triggers;
			return false;
		}
		set_problem(
		    !last_dependency_problem.empty()
		        ? last_dependency_problem
		        : "route dependency iteration limit");
		state_.progress.avoided_key_mask = saved_avoided_keys;
		state_.progress.avoided_triggers = saved_avoided_triggers;
		return false;
	}

	const route_snapshot &snapshot_;
	const route_query &query_;
	const route_visibility_query &visibility_;
	route_target_inventory targets_;
	bool allow_unresolved_triggers_;
	dependency_state state_;
};

} // namespace

route_dependency_result resolve_trigger_dependency(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    int segment,
    int side,
    const route_visibility_query &visibility)
{
	dependency_planner planner(snapshot, query, progress, visibility);
	return planner.resolve_trigger(segment, side);
}

route_plan_result plan_route(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_visibility_query &visibility)
{
	auto plan_once = [&](const route_query &attempt, bool allow_unresolved) {
		dependency_planner planner(
		    snapshot, attempt,
		    initial_route_progress_state(snapshot, attempt), visibility,
		    allow_unresolved);
		if (attempt.endpoint == route_endpoint_kind::end_of_level)
			return planner.plan_end_level();
		if (attempt.endpoint == route_endpoint_kind::unexplored)
			return planner.plan_unexplored();
		return planner.plan_segment(attempt.target_segment);
	};
	auto result = plan_once(query, false);
	if (result.status != route_plan_status::ok && query.navigator.radius > 0) {
		route_query relaxed = query;
		relaxed.navigator.radius = 0;
		result = plan_once(relaxed, false);
	}
	if (result.status == route_plan_status::ok ||
	    query.endpoint != route_endpoint_kind::end_of_level)
		return result;
	auto continued = plan_once(query, true);
	return continued.steps.size() > result.steps.size() ? continued : result;
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

int semantic_key_index(dxx_route::route_key_requirement key)
{
	switch (key) {
		case dxx_route::route_key_requirement::blue: return 0;
		case dxx_route::route_key_requirement::red: return 1;
		case dxx_route::route_key_requirement::gold: return 2;
		default: return -1;
	}
}

const char *semantic_key_name(int key)
{
	switch (key) {
		case 0: return "blue";
		case 1: return "red";
		case 2: return "gold";
		default: return "unknown";
	}
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

int view_wall_shootable(
    void *user,
    int segment,
    const dxx_route::route_position &from,
    int wall)
{
	const auto *context = static_cast<view_visibility_context *>(user);
	return context->view->wall_shootable_from_position(
	           context->view->user, segment, from.value.data(), wall) != 0;
}

int view_wall_shootable_without_transparency(
    void *user,
    int segment,
    const dxx_route::route_position &from,
    int wall)
{
	const auto *context = static_cast<view_visibility_context *>(user);
	return context->view->wall_shootable_without_transparency_from_position(
	           context->view->user, segment, from.value.data(), wall) != 0;
}

int plan_key_mask(const dxx_route::route_plan_result &plan)
{
	int mask = 0;
	for (const auto &step : plan.steps) {
		if (step.kind != dxx_route::route_semantic_step_kind::key)
			continue;
		const int key = semantic_key_index(step.key);
		if (key >= 0)
			mask |= 1 << key;
	}
	return mask;
}

int snapshot_present_key_mask(const dxx_route::route_snapshot &snapshot)
{
	int mask = 0;
	for (const auto &object : snapshot.state.objects) {
		if (object.should_be_dead)
			continue;
		const int direct_key = semantic_key_index(object.key);
		if (direct_key >= 0)
			mask |= 1 << direct_key;
		const int contained_key = semantic_key_index(object.contains_key);
		if (contained_key >= 0 && object.contains_count > 0)
			mask |= 1 << contained_key;
	}
	return mask;
}

int keys_before_transparent_shot(
    const dxx_route::route_plan_result &plan)
{
	int mask = 0;
	for (const auto &step : plan.steps) {
		if (step.uses_transparent_surface)
			return mask;
		if (step.kind != dxx_route::route_semantic_step_kind::key)
			continue;
		const int key = semantic_key_index(step.key);
		if (key >= 0)
			mask |= 1 << key;
	}
	return 0;
}

bool plan_uses_transparent_shot(const dxx_route::route_plan_result &plan)
{
	return std::any_of(
	    plan.steps.begin(), plan.steps.end(), [](const auto &step) {
		    return step.uses_transparent_surface;
	    });
}

int plan_unresolved_trigger_count(const dxx_route::route_plan_result &plan)
{
	return static_cast<int>(std::count_if(
	    plan.steps.begin(), plan.steps.end(), [](const auto &step) {
		    return step.activation ==
		           dxx_route::route_activation_kind::unresolved_trigger;
	    }));
}

bool plans_have_same_objective_sequence(
    const dxx_route::route_plan_result &left,
    const dxx_route::route_plan_result &right)
{
	return left.steps.size() == right.steps.size() &&
	       std::equal(
	           left.steps.begin(), left.steps.end(), right.steps.begin(),
	           [](const auto &left_step, const auto &right_step) {
		           return left_step.kind == right_step.kind &&
		                  left_step.trigger == right_step.trigger &&
		                  left_step.key == right_step.key;
	           });
}

void annotate_bypassable_keys(
    dxx_route::route_plan_result &plan,
    int key_mask)
{
	std::string keys;
	for (auto &step : plan.steps) {
		if (step.kind != dxx_route::route_semantic_step_kind::key)
			continue;
		const int key = semantic_key_index(step.key);
		if (key < 0 || (key_mask & (1 << key)) == 0)
			continue;
		step.can_be_bypassed = true;
		if (!keys.empty())
			keys += " and ";
		keys += semantic_key_name(key);
	}
	if (keys.empty())
		return;
	const std::string note =
	    keys + (key_mask & (key_mask - 1) ? " keys can be skipped" : " key can be skipped") +
	    " by shooting through a transparent wall";
	if (plan.note.empty())
		plan.note = note;
	else
		plan.note += "; " + note;
}

void project_position(
    const dxx_route::route_position &source,
    int &valid,
    int destination[3])
{
	valid = source.valid ? 1 : 0;
	for (int coordinate = 0; coordinate < 3; ++coordinate)
		destination[coordinate] = source.valid ? source.value[coordinate] : 0;
}

bool project_step(
    const dxx_route::route_semantic_step &source,
    level_metadata_route_step &destination)
{
	if (source.opened_links.size() > LEVEL_METADATA_MAX_ROUTE_LINKS)
		return false;
	std::memset(&destination, 0, sizeof(destination));
	destination.kind = static_cast<int>(source.kind);
	destination.seg = source.segment;
	destination.side = source.side;
	destination.wall_num = source.wall;
	destination.trigger_num = source.trigger;
	destination.trigger_type = source.trigger_raw_type;
	destination.key_index = semantic_key_index(source.key);
	destination.key_carrier_objnum = source.key_carrier_object;
	destination.can_be_bypassed = source.can_be_bypassed ? 1 : 0;
	destination.activation_kind = static_cast<int>(source.activation);
	project_position(
	    source.activation_position, destination.activation_pos_valid,
	    destination.activation_pos);
	project_position(
	    source.aim_position, destination.aim_pos_valid, destination.aim_pos);
	project_position(
	    source.label_position, destination.label_pos_valid,
	    destination.label_pos);
	destination.distance_from_previous = source.distance_from_previous;
	std::snprintf(
	    destination.label, sizeof(destination.label), "%s",
	    source.label.c_str());
	std::snprintf(
	    destination.trigger_type_name,
	    sizeof(destination.trigger_type_name), "%s",
	    source.trigger_type_name.c_str());
	destination.opened_link_count =
	    static_cast<int>(source.opened_links.size());
	for (int link = 0; link < destination.opened_link_count; ++link) {
		destination.opened_link_seg[link] = source.opened_links[link].segment;
		destination.opened_link_side[link] = source.opened_links[link].side;
		destination.opened_link_wall[link] = source.opened_links[link].wall;
	}
	return true;
}

bool project_plan(
    const dxx_route::route_plan_result &source,
    int endpoint_kind,
    level_metadata_state &destination,
    level_metadata_unexplored_route *unexplored,
    route_planner_plan_summary &summary,
    std::string &problem)
{
	if (source.steps.size() > LEVEL_METADATA_MAX_ROUTE_STEPS) {
		problem = "shared route exceeds route step capacity";
		return false;
	}
	level_metadata_state_clear(&destination);
	destination.route_status = static_cast<int>(source.status);
	std::snprintf(
	    destination.route_problem, sizeof(destination.route_problem), "%s",
	    source.problem.c_str());
	std::snprintf(
	    destination.route_note, sizeof(destination.route_note), "%s",
	    source.note.c_str());
	destination.route_step_count = static_cast<int>(source.steps.size());
	destination.travel_distance = source.travel_distance;
	destination.travel_time_seconds = static_cast<int>(std::floor(
	    source.travel_distance / LEVEL_METADATA_SHIP_SPEED_UNITS_PER_SECOND +
	    0.5));
	for (int step = 0; step < destination.route_step_count; ++step) {
		if (project_step(source.steps[step], destination.route_steps[step]))
			continue;
		problem = "shared route step exceeds opened-link capacity";
		return false;
	}

	std::memset(&summary, 0, sizeof(summary));
	summary.endpoint_kind = endpoint_kind;
	summary.route_step_count = destination.route_step_count;
	summary.first_pending_step = -1;
	summary.first_pending_path_terminal_segment = -1;
	summary.partial_frontier_segment = source.partial_frontier_segment;
	for (int step = 0; step < destination.route_step_count; ++step) {
		if (source.steps[step].kind ==
		    dxx_route::route_semantic_step_kind::start)
			continue;
		summary.first_pending_step = step;
		summary.first_pending_path_segment_count =
		    static_cast<int>(source.steps[step].path.segments.size());
		summary.first_pending_path_terminal_segment =
		    source.steps[step].path.terminal_segment;
		break;
	}
	if (unexplored) {
		unexplored->component_size = source.unexplored_component_size;
		unexplored->target_seg = source.unexplored_target_segment;
		unexplored->waypoint_seg = source.unexplored_waypoint_segment;
		unexplored->direct_reachable =
		    source.unexplored_direct_reachable ? 1 : 0;
	}
	return true;
}

} // namespace

extern "C" int route_planner_plan_view(
    const level_metadata_scan_view *view,
    int endpoint_kind,
    int target_segment,
    level_metadata_state *state,
    level_metadata_unexplored_route *unexplored,
    route_planner_plan_summary *summary,
    char *problem,
    int problem_capacity)
{
	copy_problem(problem, problem_capacity, "");
	if (!view || !state || !summary) {
		copy_problem(
		    problem, problem_capacity,
		    "shared route planning requires input and output");
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
		query.navigator.radius = view->navigator_radius;
		switch (endpoint_kind) {
			case ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL:
				query.endpoint =
				    dxx_route::route_endpoint_kind::end_of_level;
				break;
			case ROUTE_PLANNER_ENDPOINT_UNEXPLORED:
				query.endpoint = dxx_route::route_endpoint_kind::unexplored;
				break;
			case ROUTE_PLANNER_ENDPOINT_SEGMENT:
				query.endpoint = dxx_route::route_endpoint_kind::segment;
				query.target_segment = target_segment;
				break;
			default:
				copy_problem(
				    problem, problem_capacity,
				    "invalid shared route endpoint kind");
				return 0;
		}
		view_visibility_context visibility_context = { view };
		dxx_route::visibility_sample_cache sample_cache;
		dxx_route::route_visibility_query visibility;
		visibility.user = &visibility_context;
		if (view->target_visible_from_segment)
			visibility.target_visible = view_target_visible;
		if (view->wall_shootable_from_position)
			visibility.wall_shootable = view_wall_shootable;
		if (view->wall_shootable_without_transparency_from_position)
			visibility.wall_shootable_without_transparency =
			    view_wall_shootable_without_transparency;
		visibility.progress_user = view->progress_user;
		visibility.progress = view->progress;
		visibility.sample_cache = &sample_cache;
		auto result = dxx_route::plan_route(snapshot, query, visibility);
		if (endpoint_kind == ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL &&
		    result.status == dxx_route::route_plan_status::ok &&
		    plan_uses_transparent_shot(result) &&
		    visibility.wall_shootable_without_transparency) {
			const int preceding_keys = keys_before_transparent_shot(result);
			if (preceding_keys) {
				auto strict_visibility = visibility;
				strict_visibility.sample_cache_namespace++;
				strict_visibility.wall_shootable =
				    visibility.wall_shootable_without_transparency;
				auto strict = dxx_route::plan_route(
				    snapshot, query, strict_visibility);
				const int bypassable_keys =
				    preceding_keys & plan_key_mask(strict);
				if (strict.status == dxx_route::route_plan_status::ok &&
				    !plan_uses_transparent_shot(strict) && bypassable_keys &&
				    !plans_have_same_objective_sequence(strict, result) &&
				    plan_unresolved_trigger_count(strict) <=
				        plan_unresolved_trigger_count(result)) {
					annotate_bypassable_keys(strict, bypassable_keys);
					result = strict;
				}
			}
		}
		if (!project_plan(
		        result, endpoint_kind, *state, unexplored, *summary,
		        detail)) {
			copy_problem(problem, problem_capacity, detail.c_str());
			return 0;
		}
		if (endpoint_kind == ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL &&
		    result.status == dxx_route::route_plan_status::ok)
			state->unnecessary_key_mask =
			    snapshot_present_key_mask(snapshot) & ~plan_key_mask(result);
		return 1;
	} catch (const std::exception &error) {
		copy_problem(problem, problem_capacity, error.what());
	} catch (...) {
		copy_problem(
		    problem, problem_capacity, "shared route planning failed");
	}
	return 0;
}

extern "C" int route_planner_segment_reachable_view(
    const level_metadata_scan_view *view,
    int target_segment)
{
	if (!view)
		return 0;
	try {
		dxx_route::route_snapshot snapshot;
		std::string detail;
		if (!dxx_route::build_route_snapshot(*view, snapshot, &detail))
			return 0;
		dxx_route::route_query query;
		query.start = snapshot.state.start_position;
		query.progression.key_mask = snapshot.state.key_mask;
		query.navigator.radius = view->navigator_radius;
		query.endpoint = dxx_route::route_endpoint_kind::segment;
		query.target_segment = target_segment;
		view_visibility_context visibility_context = { view };
		dxx_route::visibility_sample_cache sample_cache;
		dxx_route::route_visibility_query visibility;
		visibility.user = &visibility_context;
		if (view->target_visible_from_segment)
			visibility.target_visible = view_target_visible;
		if (view->wall_shootable_from_position)
			visibility.wall_shootable = view_wall_shootable;
		if (view->wall_shootable_without_transparency_from_position)
			visibility.wall_shootable_without_transparency =
			    view_wall_shootable_without_transparency;
		visibility.progress_user = view->progress_user;
		visibility.progress = view->progress;
		visibility.sample_cache = &sample_cache;
		return dxx_route::plan_route(snapshot, query, visibility).status ==
		       dxx_route::route_plan_status::ok;
	} catch (...) {
		return 0;
	}
}
