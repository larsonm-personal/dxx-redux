#include "route_planner.h"
#include "route_planner_c.h"
#include "guidebot_route_decision.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <queue>
#include <unordered_map>

namespace dxx_route
{
namespace
{

route_planner_work_summary Work_summary;
bool Work_tracking_enabled = false;
constexpr double ROUTE_SWITCH_SHOT_DISTANCE_WEIGHT = 4.0;
constexpr double ROUTE_SWITCH_STEEP_COSINE = 0.25881904510252074;
constexpr double ROUTE_SWITCH_MIN_COSINE = 0.01;
constexpr int ROUTE_ENTRY_STATE_COUNT = LEVEL_METADATA_MAX_SIDES + 1;
constexpr int ROUTE_CANONICAL_ENTRY_STATE = LEVEL_METADATA_MAX_SIDES;

bool valid_segment(const route_snapshot &snapshot, int segment);
bool valid_wall(const route_snapshot &snapshot, int wall);

guidebot_route_objective_identity target_identity(const route_target &target)
{
	guidebot_route_objective_identity identity = {};

	switch (target.kind) {
		case route_target_kind::key:
			identity.kind = LEVEL_METADATA_ROUTE_KEY;
			break;
		case route_target_kind::reactor:
			identity.kind = LEVEL_METADATA_ROUTE_REACTOR;
			break;
		case route_target_kind::boss:
			identity.kind = LEVEL_METADATA_ROUTE_BOSS;
			break;
		case route_target_kind::exit:
			identity.kind = LEVEL_METADATA_ROUTE_EXIT;
			break;
	}
	identity.trigger = -1;
	identity.wall = -1;
	identity.object = target.object;
	identity.segment = target.segment;
	identity.side = target.side;
	return identity;
}

guidebot_route_objective_identity trigger_identity(
    const route_trigger_source &source)
{
	guidebot_route_objective_identity identity = {};

	identity.kind = LEVEL_METADATA_ROUTE_TRIGGER;
	identity.trigger = source.trigger;
	identity.wall = source.source_wall;
	identity.object = -1;
	identity.segment = source.source_segment;
	identity.side = source.source_side;
	return identity;
}

bool stable_identity_less(
    const guidebot_route_objective_identity &left,
    const guidebot_route_objective_identity &right)
{
	return guidebot_route_objective_identity_compare(&left, &right) < 0;
}

bool position_identity_less(
    int left_segment,
    const route_position &left,
    int right_segment,
    const route_position &right)
{
	if (left_segment != right_segment)
		return left_segment < right_segment;
	return left.value < right.value;
}

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
		if (nodes_[left].distance != nodes_[right].distance)
			return nodes_[left].distance < nodes_[right].distance;
		return left < right;
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

bool consume_analysis_work(const route_visibility_query &visibility)
{
	auto *budget = visibility.analysis_budget;
	if (!budget)
		return true;
	if (budget->cancelled && budget->cancelled(budget->cancel_user)) {
		budget->was_cancelled = true;
		return false;
	}
	if (budget->work_limit && budget->work_used >= budget->work_limit) {
		budget->exhausted = true;
		return false;
	}
	budget->work_used++;
	return true;
}

bool state_flag(const std::vector<unsigned char> &values, int index)
{
	return index >= 0 && index < static_cast<int>(values.size()) &&
	       values[index] != 0;
}

bool trigger_effect_needed(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int trigger)
{
	if (!valid_trigger(snapshot, trigger))
		return false;
	const auto kind = snapshot.topology.triggers[trigger].kind;
	auto wall_needs_effect = [&](int wall) {
		if (!valid_wall(snapshot, wall))
			return false;
		switch (kind) {
			case route_trigger_kind::open_door:
			case route_trigger_kind::toggle_door:
				return !route_progress_wall_opened(snapshot, progress, wall);
			case route_trigger_kind::close_door:
				return route_progress_wall_opened(snapshot, progress, wall);
			case route_trigger_kind::illusion_off:
			case route_trigger_kind::open_wall:
				return route_progress_wall_kind(snapshot, progress, wall) !=
				       route_wall_kind::open;
			case route_trigger_kind::illusion_on:
			case route_trigger_kind::illusory_wall:
				return route_progress_wall_kind(snapshot, progress, wall) !=
				       route_wall_kind::illusion;
			case route_trigger_kind::unlock_door:
				return route_progress_wall_locked(snapshot, progress, wall);
			case route_trigger_kind::lock_door:
				return !route_progress_wall_locked(snapshot, progress, wall);
			case route_trigger_kind::close_wall:
				return route_progress_wall_kind(snapshot, progress, wall) !=
				       route_wall_kind::closed;
			default: return false;
		}
	};
	for (const auto &link : snapshot.topology.triggers[trigger].links) {
		if (!valid_segment(snapshot, link.segment) || link.side < 0 ||
		    link.side >= LEVEL_METADATA_MAX_SIDES)
			continue;
		const auto &side = snapshot.topology.segments[link.segment].sides[link.side];
		if (wall_needs_effect(side.wall))
			return true;
		if (valid_segment(snapshot, side.child) && side.reverse_side >= 0 &&
		    side.reverse_side < LEVEL_METADATA_MAX_SIDES &&
		    wall_needs_effect(snapshot.topology.segments[side.child]
		                          .sides[side.reverse_side]
		                          .wall))
			return true;
	}
	return false;
}

bool trigger_source_wall_valid(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int wall,
    bool require_path_opener = true,
    bool require_exposed = true)
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
	       (!require_path_opener || route_trigger_changes_navigation(
	                                    snapshot.topology.triggers[trigger].kind)) &&
	       (!snapshot.topology.has_switch_reveal_pattern ||
	        trigger_effect_needed(snapshot, progress, trigger)) &&
	       !state_flag(progress.consumed_one_shot_triggers, trigger) &&
	       !state_flag(progress.fired_triggers, trigger) &&
	       (!require_exposed || !topology.shootable_trigger ||
	        route_progress_wall_kind(snapshot, progress, wall) !=
	            route_wall_kind::open);
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
	if (valid_wall(snapshot, source.source_wall) && visibility.wall_shootable) {
		if (!consume_analysis_work(visibility))
			return false;
		return visibility.wall_shootable(
		    visibility.user, segment, position, source.source_wall);
	}
	if (!visibility.target_visible || !consume_analysis_work(visibility))
		return false;
	return visibility.target_visible(
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
	if (visibility.analysis_budget &&
	    visibility.analysis_budget->cache_entry_limit &&
	    cache.size() >= visibility.analysis_budget->cache_entry_limit)
		return;
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
    bool include_in_progress,
    bool include_unexposed = false)
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
	auto side_has_candidate = [&](int candidate_segment, int candidate_side) {
		if (!valid_segment(snapshot, candidate_segment) || candidate_side < 0 ||
		    candidate_side >= LEVEL_METADATA_MAX_SIDES)
			return false;
		for (const int wall : snapshot.topology.segments[candidate_segment]
		                          .sides[candidate_side]
		                          .opener_walls)
			if (trigger_source_wall_valid(
			        snapshot, progress, wall, true, !include_unexposed))
				return true;
		return false;
	};
	if (!side_has_candidate(target_segment, target_side)) {
		target_segment = child;
		target_side = requested_side.reverse_side;
		if (!side_has_candidate(target_segment, target_side))
			return result;
	}
	const auto &target =
	    snapshot.topology.segments[target_segment].sides[target_side];
	for (const int source_wall : target.opener_walls) {
		if (!trigger_source_wall_valid(
		        snapshot, progress, source_wall, true, !include_unexposed))
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

std::vector<route_trigger_source> discover_sources_for_trigger(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int trigger,
    bool include_unexposed)
{
	std::vector<route_trigger_source> result;
	if (!valid_trigger(snapshot, trigger))
		return result;
	for (int wall = 0; wall < static_cast<int>(snapshot.topology.walls.size());
	     ++wall) {
		if (snapshot.state.walls[wall].trigger != trigger ||
		    !trigger_source_wall_valid(
		        snapshot, progress, wall, false, !include_unexposed))
			continue;
		const auto &source_topology = snapshot.topology.walls[wall];
		route_position source_position = source_topology.target;
		if (!source_position.valid && valid_segment(snapshot, source_topology.segment))
			source_position = snapshot.topology.segments[source_topology.segment].center;
		if (!source_position.valid)
			continue;
		route_trigger_source source;
		source.source_wall = wall;
		source.source_segment = source_topology.segment;
		source.source_side = source_topology.side;
		source.trigger = trigger;
		source.trigger_kind = snapshot.topology.triggers[trigger].kind;
		source.source_position = source_position;
		if (!snapshot.topology.triggers[trigger].links.empty()) {
			const auto &link = snapshot.topology.triggers[trigger].links.front();
			source.target_segment = link.segment;
			source.target_side = link.side;
			if (valid_segment(snapshot, link.segment) && link.side >= 0 &&
			    link.side < LEVEL_METADATA_MAX_SIDES)
				source.target_wall = snapshot.topology.segments[link.segment]
				                         .sides[link.side]
				                         .wall;
		}
		result.push_back(source);
	}
	return result;
}

bool trigger_targets_wall(
    const route_snapshot &snapshot,
    int trigger,
    int target_wall)
{
	if (!valid_trigger(snapshot, trigger) || !valid_wall(snapshot, target_wall))
		return false;
	for (const auto &link : snapshot.topology.triggers[trigger].links) {
		if (!valid_segment(snapshot, link.segment) || link.side < 0 ||
		    link.side >= LEVEL_METADATA_MAX_SIDES)
			continue;
		const auto &side = snapshot.topology.segments[link.segment].sides[link.side];
		if (side.wall == target_wall)
			return true;
		if (valid_segment(snapshot, side.child) && side.reverse_side >= 0 &&
		    side.reverse_side < LEVEL_METADATA_MAX_SIDES &&
		    snapshot.topology.segments[side.child]
		            .sides[side.reverse_side]
		            .wall == target_wall)
			return true;
	}
	return false;
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

void reset_route_planner_work_summary()
{
	Work_summary = route_planner_work_summary{};
}

route_planner_work_summary get_route_planner_work_summary()
{
	return Work_summary;
}

void set_route_planner_work_tracking(bool enabled)
{
	Work_tracking_enabled = enabled;
}

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
	result.consumed_one_shot_triggers.resize(snapshot.state.triggers.size());
	result.trigger_in_progress.resize(snapshot.state.triggers.size());
	result.avoided_triggers.resize(snapshot.state.triggers.size());
	for (const auto &wall : snapshot.state.walls) {
		result.wall_kinds.push_back(wall.kind);
		result.wall_locked.push_back(wall.locked ? 1 : 0);
		result.wall_opened.push_back(wall.opened ? 1 : 0);
	}
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

bool route_progress_apply_trigger(
    const route_snapshot &snapshot,
    route_progress_state &progress,
    int trigger)
{
	if (progress.fired_triggers.size() != snapshot.state.triggers.size())
		progress.fired_triggers.resize(snapshot.state.triggers.size());
	if (progress.consumed_one_shot_triggers.size() !=
	    snapshot.state.triggers.size())
		progress.consumed_one_shot_triggers.resize(
		    snapshot.state.triggers.size());
	if (progress.wall_kinds.size() != snapshot.state.walls.size() ||
	    progress.wall_locked.size() != snapshot.state.walls.size() ||
	    progress.wall_opened.size() != snapshot.state.walls.size()) {
		progress.wall_kinds.clear();
		progress.wall_locked.clear();
		progress.wall_opened.clear();
		for (const auto &wall : snapshot.state.walls) {
			progress.wall_kinds.push_back(wall.kind);
			progress.wall_locked.push_back(wall.locked ? 1 : 0);
			progress.wall_opened.push_back(wall.opened ? 1 : 0);
		}
	}
	if (!valid_trigger(snapshot, trigger) ||
	    snapshot.state.triggers[trigger].disabled ||
	    state_flag(progress.consumed_one_shot_triggers, trigger))
		return false;
	const auto kind = snapshot.topology.triggers[trigger].kind;
	if (!route_trigger_changes_navigation(kind))
		return route_progress_fire_trigger(progress, trigger);
	std::vector<int> changed_walls;
	auto apply_wall = [&](int wall) {
		if (!valid_wall(snapshot, wall))
			return;
		const auto previous_kind = progress.wall_kinds[wall];
		const auto previous_locked = progress.wall_locked[wall];
		const auto previous_opened = progress.wall_opened[wall];
		switch (kind) {
			case route_trigger_kind::open_door:
				progress.wall_opened[wall] = 1;
				break;
			case route_trigger_kind::close_door:
				progress.wall_opened[wall] = 0;
				break;
			case route_trigger_kind::toggle_door:
				progress.wall_opened[wall] =
				    progress.wall_opened[wall] ? 0 : 1;
				break;
			case route_trigger_kind::illusion_off:
			case route_trigger_kind::open_wall:
				progress.wall_kinds[wall] = route_wall_kind::open;
				break;
			case route_trigger_kind::illusion_on:
			case route_trigger_kind::illusory_wall:
				progress.wall_kinds[wall] = route_wall_kind::illusion;
				break;
			case route_trigger_kind::unlock_door:
				progress.wall_locked[wall] = 0;
				break;
			case route_trigger_kind::lock_door:
				progress.wall_locked[wall] = 1;
				break;
			case route_trigger_kind::close_wall:
				progress.wall_kinds[wall] = route_wall_kind::closed;
				break;
			default: break;
		}
		if ((previous_kind != progress.wall_kinds[wall] ||
		     previous_locked != progress.wall_locked[wall] ||
		     previous_opened != progress.wall_opened[wall]) &&
		    std::find(changed_walls.begin(), changed_walls.end(), wall) ==
		        changed_walls.end())
			changed_walls.push_back(wall);
	};
	for (const auto &link : snapshot.topology.triggers[trigger].links) {
		if (!valid_segment(snapshot, link.segment) || link.side < 0 ||
		    link.side >= LEVEL_METADATA_MAX_SIDES)
			continue;
		const auto &side = snapshot.topology.segments[link.segment].sides[link.side];
		apply_wall(side.wall);
		if (!valid_segment(snapshot, side.child) || side.reverse_side < 0 ||
		    side.reverse_side >= LEVEL_METADATA_MAX_SIDES)
			continue;
		apply_wall(snapshot.topology.segments[side.child]
		               .sides[side.reverse_side]
		               .wall);
	}
	/* A contrary transition rearms only the previously fired actions whose
	 * linked effect is needed again. This permits useful open/close cycles
	 * without making a trigger recursively available while routing to it. */
	for (int candidate = 0;
	     candidate < static_cast<int>(snapshot.topology.triggers.size());
	     ++candidate) {
		if (candidate == trigger ||
		    !state_flag(progress.fired_triggers, candidate) ||
		    !trigger_effect_needed(snapshot, progress, candidate))
			continue;
		if (std::any_of(
		        changed_walls.begin(), changed_walls.end(), [&](int wall) {
			        return snapshot.topology.walls[wall].shootable_trigger &&
			               trigger_targets_wall(snapshot, candidate, wall);
		        }))
			progress.fired_triggers[candidate] = 0;
	}
	progress.fired_triggers[trigger] = 1;
	if (snapshot.topology.triggers[trigger].one_shot)
		progress.consumed_one_shot_triggers[trigger] = 1;
	return true;
}

void route_progress_traverse_path(
    const route_snapshot &snapshot,
    route_progress_state &progress,
    const route_path_result &path,
    bool complete_trigger_effects)
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
		    snapshot.state.triggers[trigger].disabled)
			continue;
		if (complete_trigger_effects)
			route_progress_apply_trigger(snapshot, progress, trigger);
		else if (route_trigger_opens_path(
		             snapshot.topology.triggers[trigger].kind))
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
	if (Work_tracking_enabled)
		++Work_summary.search_calls;
	route_search_result result;
	result.start_segment = progress.current_segment;
	const int count = static_cast<int>(snapshot.topology.segments.size());
	result.nodes.resize(count);
	result.state_nodes.resize(count * ROUTE_ENTRY_STATE_COUNT);
	result.best_state_by_segment.resize(count, -1);
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
	for (auto &node : result.state_nodes) {
		node.distance = infinity;
		node.progress_weight = max_progress;
	}
	std::vector<unsigned char> closed(result.state_nodes.size(), 0);
	const int start_state =
	    result.start_segment * ROUTE_ENTRY_STATE_COUNT +
	    ROUTE_CANONICAL_ENTRY_STATE;
	auto &start_node = result.nodes[result.start_segment];
	start_node.reachable = true;
	start_node.distance = point_distance(start, start_center);
	start_node.progress_weight = 0;
	result.state_nodes[start_state] = start_node;
	result.best_state_by_segment[result.start_segment] = start_state;
	route_heap heap(result.state_nodes, options.prioritize_progress);
	heap.push(start_state);
	while (!heap.empty()) {
		const int current_state = heap.pop();
		const int current = current_state / ROUTE_ENTRY_STATE_COUNT;
		const int entry_side = current_state % ROUTE_ENTRY_STATE_COUNT;
		if (Work_tracking_enabled)
			++Work_summary.visited_segments;
		result.visit_order.push_back(current);
		closed[current_state] = 1;
		for (int side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			if (Work_tracking_enabled)
				++Work_summary.considered_edges;
			const auto &current_topology =
			    snapshot.topology.segments[current];
			if (entry_side < LEVEL_METADATA_MAX_SIDES &&
			    !(current_topology.transit_exit_masks[entry_side] &
			      (1u << side)))
				continue;
			const auto &topology_side = current_topology.sides[side];
			const int child = topology_side.child;
			if (!valid_segment(snapshot, child))
				continue;
			const int clearance = topology_side.clearance_radius;
			int child_entry = ROUTE_CANONICAL_ENTRY_STATE;
			if (query.navigator.radius > 0 && clearance > 0 &&
			    clearance < query.navigator.radius) {
				child_entry = topology_side.reverse_side;
				if (child_entry < 0 ||
				    child_entry >= LEVEL_METADATA_MAX_SIDES ||
				    snapshot.topology.segments[child]
				            .transit_exit_masks[child_entry] == 0)
					continue;
			}
			const int child_state =
			    child * ROUTE_ENTRY_STATE_COUNT + child_entry;
			if (closed[child_state])
				continue;
			const auto edge = evaluate_route_edge(
			    snapshot, query, progress, options.forbidden_missing_key,
			    current, side);
			if (Work_tracking_enabled)
				++Work_summary.evaluated_edges;
			if (edge.progress_cost == LEVEL_METADATA_ROUTE_EDGE_BLOCKED ||
			    (!options.optimistic &&
			     edge.progress_cost == LEVEL_METADATA_ROUTE_EDGE_PROGRESS))
				continue;
			const auto &current_center = current_topology.center;
			const auto &child_center = snapshot.topology.segments[child].center;
			if (!current_center.valid || !child_center.valid)
				continue;
			const double distance = result.state_nodes[current_state].distance +
			                        point_distance(current_center, child_center);
			const int progress =
			    result.state_nodes[current_state].progress_weight +
			    (edge.progress_cost == LEVEL_METADATA_ROUTE_EDGE_PROGRESS ? 1 : 0);
			auto &node = result.state_nodes[child_state];
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
			node.parent_segment = current_state;
			node.parent_side = side;
			node.incoming_edge = edge;
			auto &best = result.nodes[child];
			if (!best.reachable ||
			    (options.prioritize_progress &&
			     progress < best.progress_weight) ||
			    ((!options.prioritize_progress ||
			      progress == best.progress_weight) &&
			     distance < best.distance)) {
				best = node;
				best.parent_segment = current;
				result.best_state_by_segment[child] = child_state;
			}
			if (heap.contains(child_state))
				heap.decrease(child_state);
			else
				heap.push(child_state);
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
	std::vector<route_edge_decision> reverse_edges;
	int current_state =
	    target_segment < static_cast<int>(search.best_state_by_segment.size())
	        ? search.best_state_by_segment[target_segment]
	        : -1;
	for (; current_state >= 0 &&
	       current_state < static_cast<int>(search.state_nodes.size());
	     current_state = search.state_nodes[current_state].parent_segment) {
		const int current = current_state / ROUTE_ENTRY_STATE_COUNT;
		reverse_segments.push_back(current);
		if (current == search.start_segment)
			break;
		reverse_sides.push_back(search.state_nodes[current_state].parent_side);
		reverse_edges.push_back(
		    search.state_nodes[current_state].incoming_edge);
	}
	if (reverse_segments.empty() || reverse_segments.back() != search.start_segment) {
		result = route_path_result{};
		return result;
	}
	result.segments.assign(reverse_segments.rbegin(), reverse_segments.rend());
	result.sides.assign(reverse_sides.rbegin(), reverse_sides.rend());
	std::reverse(reverse_edges.begin(), reverse_edges.end());
	for (std::size_t index = 1; index < result.segments.size(); ++index) {
		const auto &edge = reverse_edges[index - 1];
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
		const bool equal_cost =
		    node.progress_weight == best_progress && distance == best_distance;
		if (node.progress_weight > best_progress ||
		    (node.progress_weight == best_progress && distance > best_distance) ||
		    (equal_cost && result.selected_index >= 0 &&
		     !stable_identity_less(
		         target_identity(target),
		         target_identity(targets[result.selected_index]))))
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
		if (distance > best_distance ||
		    (distance == best_distance && result.selected_index >= 0 &&
		     !stable_identity_less(
		         target_identity(target),
		         target_identity(targets[result.selected_index]))))
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

int path_traversed_key_mask(
    const route_snapshot &snapshot,
    const route_path_result &path)
{
	int mask = 0;
	for (std::size_t index = 0;
	     index < path.sides.size() && index < path.segments.size(); ++index) {
		const int segment = path.segments[index];
		const int side = path.sides[index];
		if (!valid_segment(snapshot, segment) || side < 0 ||
		    side >= LEVEL_METADATA_MAX_SIDES)
			continue;
		const int wall = snapshot.topology.segments[segment].sides[side].wall;
		if (!valid_wall(snapshot, wall))
			continue;
		const int key = key_index(snapshot.state.walls[wall].key);
		if (key >= 0)
			mask |= 1 << key;
	}
	return mask;
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

struct switch_guidance_graph {
	std::vector<int> component_ids;
	int component_count = 0;
	std::vector<std::vector<int>> optimistic_edges;
};

switch_guidance_graph build_switch_guidance_graph(
    const route_snapshot &snapshot,
    const route_query &query)
{
	switch_guidance_graph result;
	const int segment_count =
	    static_cast<int>(snapshot.topology.segments.size());
	result.component_ids.assign(segment_count, -1);
	result.optimistic_edges.resize(segment_count);
	std::vector<std::vector<int>> passable_edges(segment_count);
	std::vector<std::vector<int>> reverse_edges(segment_count);
	const auto initial_progress = initial_route_progress_state(snapshot, query);
	for (int segment = 0; segment < segment_count; ++segment) {
		if (!snapshot.topology.segments[segment].center.valid)
			continue;
		for (int side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
			const auto &topology_side =
			    snapshot.topology.segments[segment].sides[side];
			const int child = topology_side.child;
			if (!valid_segment(snapshot, child) ||
			    !snapshot.topology.segments[child].center.valid)
				continue;
			if (query.navigator.radius > 0 &&
			    topology_side.clearance_radius > 0 &&
			    topology_side.clearance_radius < query.navigator.radius)
				continue;
			const auto edge = evaluate_route_edge(
			    snapshot, query, initial_progress, segment, side);
			if (edge.progress_cost == LEVEL_METADATA_ROUTE_EDGE_BLOCKED)
				continue;
			result.optimistic_edges[segment].push_back(child);
			if (edge.progress_cost != LEVEL_METADATA_ROUTE_EDGE_PASSABLE)
				continue;
			passable_edges[segment].push_back(child);
			reverse_edges[child].push_back(segment);
		}
	}
	std::vector<unsigned char> visited(segment_count, 0);
	std::vector<int> finish_order;
	finish_order.reserve(segment_count);
	std::vector<std::pair<int, std::size_t>> depth_first;
	for (int segment = 0; segment < segment_count; ++segment) {
		if (!visited[segment] &&
		    snapshot.topology.segments[segment].center.valid) {
			visited[segment] = 1;
			depth_first.push_back({ segment, 0 });
			while (!depth_first.empty()) {
				auto &entry = depth_first.back();
				if (entry.second < passable_edges[entry.first].size()) {
					const int child =
					    passable_edges[entry.first][entry.second++];
					if (!visited[child]) {
						visited[child] = 1;
						depth_first.push_back({ child, 0 });
					}
					continue;
				}
				finish_order.push_back(entry.first);
				depth_first.pop_back();
			}
		}
	}
	std::vector<int> component_stack;
	for (auto current = finish_order.rbegin(); current != finish_order.rend();
	     ++current) {
		if (result.component_ids[*current] >= 0)
			continue;
		result.component_ids[*current] = result.component_count;
		component_stack.push_back(*current);
		while (!component_stack.empty()) {
			const int segment = component_stack.back();
			component_stack.pop_back();
			for (const int child : reverse_edges[segment]) {
				if (result.component_ids[child] >= 0)
					continue;
				result.component_ids[child] = result.component_count;
				component_stack.push_back(child);
			}
		}
		++result.component_count;
	}
	return result;
}

static route_trigger_path_selection select_trigger_firing_path_internal(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    const std::vector<route_trigger_source> &sources,
    const route_visibility_query &visibility,
    const switch_guidance_graph *guidance_graph)
{
	route_trigger_path_selection result;
	route_trigger_path_selection keyed_result;
	double result_score = std::numeric_limits<double>::infinity();
	double result_shot_distance = std::numeric_limits<double>::infinity();
	double keyed_shot_distance = std::numeric_limits<double>::infinity();
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
		double candidate_score = std::numeric_limits<double>::infinity();
		candidate.source = source;
		const bool shootable = valid_wall(snapshot, source.source_wall) &&
		                       snapshot.topology.walls[source.source_wall]
		                           .shootable_trigger;
		if (shootable) {
			auto record_guidance_candidate = [&](
			                                     int segment,
			                                     const route_position &terminal,
			                                     int quality, int incidence,
			                                     double utility,
			                                     double initial_start_utility) {
				for (auto &existing : candidate.guidance_candidates) {
					if (existing.segment != segment)
						continue;
					if (utility < existing.utility) {
						existing.position = terminal;
						existing.quality = quality;
						existing.incidence_cosine = incidence;
						existing.utility = utility;
					}
					if (initial_start_utility >= 0.0 &&
					    (existing.initial_start_utility < 0.0 ||
					     initial_start_utility <
					         existing.initial_start_utility))
						existing.initial_start_utility =
						    initial_start_utility;
					return;
				}
				route_trigger_path_selection::guidance_candidate guidance;
				guidance.segment = segment;
				guidance.position = terminal;
				guidance.quality = quality;
				guidance.incidence_cosine = incidence;
				guidance.utility = utility;
				guidance.initial_start_utility = initial_start_utility;
				candidate.guidance_candidates.push_back(guidance);
			};
			auto accept_position = [&](int segment, const route_position &terminal,
			                           double extra_distance,
			                           double initial_start_utility = -1.0) {
				auto path = build_route_path(search, segment);
				path.distance += extra_distance;
				int incidence = LEVEL_METADATA_SHOT_COSINE_ONE;
				if (visibility.wall_shot_incidence_cosine)
					incidence = std::max(
					    0,
					    std::min(
					        LEVEL_METADATA_SHOT_COSINE_ONE,
					        visibility.wall_shot_incidence_cosine(
					            visibility.user, terminal,
					            source.source_wall)));
				const double incidence_ratio =
				    static_cast<double>(incidence) /
				    LEVEL_METADATA_SHOT_COSINE_ONE;
				const double angle_multiplier =
				    incidence_ratio < ROUTE_SWITCH_STEEP_COSINE
				        ? ROUTE_SWITCH_STEEP_COSINE /
				              std::max(incidence_ratio, ROUTE_SWITCH_MIN_COSINE)
				        : 1.0;
				/* A remote line of fire may be technically valid while providing no
				 * useful switch guidance.  Weight shot length strongly enough that
				 * Guide-Bot leads the player near the switch.  Steep confirmed shots
				 * remain valid but lose to similarly useful square-on positions. */
				const double score =
				    path.distance +
				    point_distance(terminal, source.source_position) *
				        ROUTE_SWITCH_SHOT_DISTANCE_WEIGHT * angle_multiplier;
				const int quality = visibility.approximate_shots
				                        ? LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE
				                    : incidence_ratio < ROUTE_SWITCH_STEEP_COSINE
				                        ? LEVEL_METADATA_SWITCH_SHOT_CONFIRMED_STEEP
				                        : LEVEL_METADATA_SWITCH_SHOT_CONFIRMED;
				record_guidance_candidate(
				    segment, terminal, quality, incidence,
				    point_distance(terminal, source.source_position) *
				        angle_multiplier,
				    initial_start_utility);
				if (candidate.found &&
				    (score > candidate_score ||
				     (score == candidate_score &&
				      !position_identity_less(
				          segment, terminal, candidate.terminal_segment,
				          candidate.terminal_position))))
					return;
				path.progress_weight = 0;
				path.terminal_segment = segment;
				path.terminal_position = terminal;
				candidate.path = std::move(path);
				candidate.terminal_segment = segment;
				candidate.terminal_position = terminal;
				candidate.switch_shot_incidence_cosine = incidence;
				candidate.switch_shot_quality = quality;
				candidate.found = true;
				candidate_score = score;
			};
			/* Retain one conservative approach point per initially disconnected
			 * region.  The topology partition is compiled once for the whole plan;
			 * each switch only needs a graph traversal to rank its regions. */
			if (guidance_graph && valid_segment(snapshot, source.source_segment)) {
				const int segment_count = static_cast<int>(
				    guidance_graph->component_ids.size());
				const double infinity =
				    std::numeric_limits<double>::infinity();
				std::vector<double> strategic_distance(
				    segment_count, infinity);
				using distance_entry = std::pair<double, int>;
				std::priority_queue<
				    distance_entry, std::vector<distance_entry>,
				    std::greater<distance_entry>>
				    queue;
				strategic_distance[source.source_segment] = point_distance(
				    source.source_position,
				    snapshot.topology.segments[source.source_segment].center);
				queue.push({
				    strategic_distance[source.source_segment],
				    source.source_segment,
				});
				while (!queue.empty()) {
					const auto entry = queue.top();
					queue.pop();
					const int current = entry.second;
					if (entry.first != strategic_distance[current])
						continue;
					for (const int next :
					     guidance_graph->optimistic_edges[current]) {
						const double distance = entry.first + point_distance(
						                                          snapshot.topology.segments[current].center,
						                                          snapshot.topology.segments[next].center);
						if (distance >= strategic_distance[next])
							continue;
						strategic_distance[next] = distance;
						queue.push({ distance, next });
					}
				}
				for (int component = 0;
				     component < guidance_graph->component_count; ++component) {
					int best_segment = -1;
					int best_incidence = LEVEL_METADATA_SHOT_COSINE_ONE;
					double best_utility =
					    std::numeric_limits<double>::infinity();
					for (int segment = 0; segment < segment_count; ++segment) {
						if (guidance_graph->component_ids[segment] != component ||
						    strategic_distance[segment] == infinity)
							continue;
						const auto &center =
						    snapshot.topology.segments[segment].center;
						if (!center.valid)
							continue;
						int incidence = LEVEL_METADATA_SHOT_COSINE_ONE;
						if (visibility.wall_shot_incidence_cosine)
							incidence = std::max(
							    0,
							    std::min(
							        LEVEL_METADATA_SHOT_COSINE_ONE,
							        visibility.wall_shot_incidence_cosine(
							            visibility.user, center,
							            source.source_wall)));
						const double incidence_ratio =
						    static_cast<double>(incidence) /
						    LEVEL_METADATA_SHOT_COSINE_ONE;
						const double angle_multiplier =
						    incidence_ratio >= ROUTE_SWITCH_STEEP_COSINE
						        ? 1.0
						        : ROUTE_SWITCH_STEEP_COSINE /
						              std::max(incidence_ratio, 0.01);
						const double utility =
						    strategic_distance[segment] +
						    point_distance(center, source.source_position) *
						        angle_multiplier * 0.001;
						if (utility < best_utility ||
						    (utility == best_utility && segment < best_segment)) {
							best_segment = segment;
							best_incidence = incidence;
							best_utility = utility;
						}
					}
					if (best_segment >= 0)
						record_guidance_candidate(
						    best_segment,
						    snapshot.topology.segments[best_segment].center,
						    LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE,
						    best_incidence, best_utility, best_utility);
				}
			}
			/* A nearer route can still be a needlessly difficult long shot.  When
			 * the player already owns a key, separately test the switch's own
			 * segment if reaching it uses that keyed door. */
			if (progress.key_mask && valid_segment(snapshot, source.source_segment) &&
			    search.nodes[source.source_segment].reachable) {
				auto path = build_route_path(search, source.source_segment);
				if (path_traversed_key_mask(snapshot, path) & progress.key_mask) {
					double extra_distance = 0.0;
					route_position terminal;
					if (visible_source_position(
					        snapshot, progress, source, visibility,
					        source.source_segment, terminal, extra_distance)) {
						path.distance += extra_distance;
						const double shot_distance = point_distance(
						    terminal, source.source_position);
						if (!keyed_result.found ||
						    shot_distance < keyed_shot_distance ||
						    (shot_distance == keyed_shot_distance &&
						     (path.distance < keyed_result.path.distance ||
						      (path.distance == keyed_result.path.distance &&
						       stable_identity_less(
						           trigger_identity(source),
						           trigger_identity(keyed_result.source)))))) {
							path.progress_weight = 0;
							path.terminal_segment = source.source_segment;
							path.terminal_position = terminal;
							keyed_result.source = source;
							keyed_result.path = std::move(path);
							keyed_result.terminal_segment = source.source_segment;
							keyed_result.terminal_position = terminal;
							keyed_result.switch_shot_quality =
							    LEVEL_METADATA_SWITCH_SHOT_CONFIRMED;
							keyed_result.switch_shot_incidence_cosine =
							    visibility.wall_shot_incidence_cosine
							        ? std::max(
							              0,
							              std::min(
							                  LEVEL_METADATA_SHOT_COSINE_ONE,
							                  visibility.wall_shot_incidence_cosine(
							                      visibility.user, terminal,
							                      source.source_wall)))
							        : LEVEL_METADATA_SHOT_COSINE_ONE;
							if (keyed_result.switch_shot_incidence_cosine <
							    LEVEL_METADATA_SHOT_COSINE_ONE *
							        ROUTE_SWITCH_STEEP_COSINE)
								keyed_result.switch_shot_quality =
								    LEVEL_METADATA_SWITCH_SHOT_CONFIRMED_STEEP;
							route_trigger_path_selection::guidance_candidate guidance;
							guidance.segment = source.source_segment;
							guidance.position = terminal;
							guidance.quality = keyed_result.switch_shot_quality;
							guidance.incidence_cosine =
							    keyed_result.switch_shot_incidence_cosine;
							guidance.utility = shot_distance;
							keyed_result.guidance_candidates = { guidance };
							keyed_result.found = true;
							keyed_shot_distance = shot_distance;
						}
					}
				}
			}
			/* Find the route-optimal center-line candidate first.  Once the path
			 * lower bound cannot improve the route, exact center visibility farther
			 * away is unnecessary. */
			for (int index = 0; index < segments; ++index) {
				const int segment = search.visit_order[index];
				double extra_distance = 0.0;
				route_position terminal;
				if (candidate.found &&
				    search.nodes[segment].distance > candidate_score)
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

			/* Switch guidance is a different question from finding the shortest
			 * mission route.  Preselect the geometrically best cells across the
			 * whole reachable mine, then do the expensive exact face/edge sampling
			 * for only that bounded set.  This compiles useful alternate firing
			 * poses ahead of time even when their route distance cannot beat the
			 * canonical mission step. */
			struct detailed_guidance_candidate {
				int segment;
				double utility;
			};
			std::vector<detailed_guidance_candidate> detailed_guidance;
			detailed_guidance.reserve(segments);
			for (int index = 0; index < segments; ++index) {
				const int segment = search.visit_order[index];
				const auto &center = snapshot.topology.segments[segment].center;
				if (!center.valid)
					continue;
				int incidence = LEVEL_METADATA_SHOT_COSINE_ONE;
				if (visibility.wall_shot_incidence_cosine)
					incidence = std::max(
					    0,
					    std::min(
					        LEVEL_METADATA_SHOT_COSINE_ONE,
					        visibility.wall_shot_incidence_cosine(
					            visibility.user, center, source.source_wall)));
				const double incidence_ratio =
				    static_cast<double>(incidence) /
				    LEVEL_METADATA_SHOT_COSINE_ONE;
				const double angle_multiplier =
				    incidence_ratio >= ROUTE_SWITCH_STEEP_COSINE
				        ? 1.0
				        : ROUTE_SWITCH_STEEP_COSINE /
				              std::max(incidence_ratio, 0.01);
				detailed_guidance.push_back({
				    segment,
				    point_distance(center, source.source_position) *
				        angle_multiplier,
				});
			}
			std::sort(
			    detailed_guidance.begin(), detailed_guidance.end(),
			    [](const auto &left, const auto &right) {
				    if (left.utility != right.utility)
					    return left.utility < right.utility;
				    return left.segment < right.segment;
			    });
			if (detailed_guidance.size() > 8)
				detailed_guidance.resize(8);
			for (const auto &guidance : detailed_guidance) {
				double extra_distance = 0.0;
				route_position terminal;
				if (visible_source_detailed_position(
				        snapshot, source, visibility, guidance.segment,
				        terminal, extra_distance))
					accept_position(
					    guidance.segment, terminal, extra_distance);
			}
			for (int index = 0; index < segments; ++index) {
				const int segment = search.visit_order[index];
				double extra_distance = 0.0;
				route_position terminal;
				if (candidate.found &&
				    search.nodes[segment].distance > candidate_score)
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
			candidate_score = candidate.path.distance;
		}
		if (!candidate.found ||
		    (result.found &&
		     (candidate_score > result_score ||
		      (candidate_score == result_score &&
		       !stable_identity_less(
		           trigger_identity(candidate.source),
		           trigger_identity(result.source))))))
			continue;
		result = std::move(candidate);
		result_score = candidate_score;
		result_shot_distance = point_distance(
		    result.terminal_position, result.source.source_position);
	}
	report_progress(visibility, "route_visibility", total, total);
	if (keyed_result.found &&
	    (keyed_shot_distance < result_shot_distance ||
	     (keyed_shot_distance == result_shot_distance &&
	      (!result.found || stable_identity_less(
	                            trigger_identity(keyed_result.source),
	                            trigger_identity(result.source))))))
		result = std::move(keyed_result);
	if (result.found && visibility.wall_shootable_without_transparency &&
	    valid_wall(snapshot, result.source.source_wall) &&
	    consume_analysis_work(visibility))
		result.uses_transparent_surface =
		    !visibility.wall_shootable_without_transparency(
		        visibility.user, result.terminal_segment,
		        result.terminal_position, result.source.source_wall);
	return result;
}

route_trigger_path_selection select_trigger_firing_path(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    const std::vector<route_trigger_source> &sources,
    const route_visibility_query &visibility)
{
	return select_trigger_firing_path_internal(
	    snapshot, query, progress, sources, visibility, nullptr);
}

route_target_inventory discover_route_targets(const route_snapshot &snapshot)
{
	route_target_inventory result;
	for (const auto &wall : snapshot.state.walls) {
		const int key = key_index(wall.key);
		if (key >= 0)
			result.required_key_mask |= 1 << key;
	}
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
		case route_trigger_kind::close_door: return "close_door";
		case route_trigger_kind::toggle_door: return "toggle_door";
		case route_trigger_kind::exit: return "exit";
		case route_trigger_kind::secret_exit: return "secret_exit";
		case route_trigger_kind::illusion_off: return "illusion_off";
		case route_trigger_kind::illusion_on: return "illusion_on";
		case route_trigger_kind::unlock_door: return "unlock_door";
		case route_trigger_kind::lock_door: return "lock_door";
		case route_trigger_kind::open_wall: return "open_wall";
		case route_trigger_kind::close_wall: return "close_wall";
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
	    bool allow_unresolved_triggers = false,
	    int semantic_key_mask = -1,
	    std::array<int, 3> semantic_key_order = { { 0, 2, 1 } },
	    bool transition_aware_paths = true)
	    : snapshot_(snapshot), query_(query), visibility_(visibility),
	      targets_(discover_route_targets(snapshot)),
	      switch_guidance_graph_(build_switch_guidance_graph(snapshot, query)),
	      allow_unresolved_triggers_(allow_unresolved_triggers),
	      semantic_key_mask_(semantic_key_mask),
	      semantic_key_order_(semantic_key_order),
	      transition_aware_paths_(transition_aware_paths)
	{
		state_.progress = progress;
		if (semantic_key_mask_ >= 0)
			state_.progress.wall_state_authoritative = true;
		state_.hidden_door_in_progress.resize(snapshot.state.walls.size());
	}

	route_dependency_result resolve_trigger(int segment, int side)
	{
		route_dependency_result result;
		result.progress = state_.progress;
		const auto sources = discover_trigger_sources_internal(
		    snapshot_, state_.progress, segment, side, true, true);
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
		for (const int key : semantic_key_order_) {
			const int bit = 1 << key;
			if (semantic_key_mask_ < 0 ||
			    !(semantic_key_mask_ & bit) ||
			    targets_.keys[key].empty() ||
			    (state_.progress.key_mask & bit) != 0)
				continue;
			const auto before_key = state_;
			const bool acquired = acquire_key(key, 0);
			if (!acquired) {
				state_ = before_key;
			}
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
		const auto exit_prefix = state_;
		auto post_primary_key_steps = [&](const dependency_state &candidate) {
			return static_cast<int>(std::count_if(
			    candidate.steps.begin() + exit_prefix.steps.size(),
			    candidate.steps.end(), [](const route_semantic_step &step) {
				    return step.kind == route_semantic_step_kind::key;
			    }));
		};

		const bool conventional_ok =
		    acquire_exit_key(target) &&
		    move_to_target(target.segment, activation, 0) &&
		    append_target_step(
		        route_semantic_step_kind::exit, target, "Exit");
		const auto conventional = state_;
		bool uncollected_key_exists = false;
		for (int key = 0; key < 3; ++key)
			if (!targets_.keys[key].empty() &&
			    !(conventional.progress.key_mask & (1 << key))) {
				uncollected_key_exists = true;
				break;
			}
		if (conventional_ok && !uncollected_key_exists)
			return make_plan_result(route_plan_status::ok);

		/* Some exits hide their key dependency behind post-reactor doors, so
		 * the exit wall alone cannot identify the required key. Probe the exit
		 * from the post-reactor state to discover those dependencies. Keep the
		 * conventional route unless the probe finds additional required keys,
		 * which prevents ordinary routes from accumulating speculative travel. */
		state_ = exit_prefix;
		state_.problem.clear();
		const bool discovered_ok =
		    move_to_target(target.segment, activation, 0) &&
		    acquire_exit_key(target) &&
		    move_to_target(target.segment, activation, 0) &&
		    append_target_step(
		        route_semantic_step_kind::exit, target, "Exit");
		const auto discovered = state_;
		if (conventional_ok &&
		    (!discovered_ok ||
		     post_primary_key_steps(conventional) >=
		         post_primary_key_steps(discovered)))
			state_ = conventional;
		else if (discovered_ok)
			state_ = discovered;
		else
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
	    bool prefer_keyed_progress,
	    route_key_requirement forbidden_key = route_key_requirement::none)
	{
		route_search_options options;
		options.optimistic = optimistic;
		options.prioritize_progress =
		    forbidden_key == route_key_requirement::none;
		options.forbidden_missing_key = forbidden_key;
		auto find_path = [&](const route_progress_state &progress,
		                     const route_search_options &search_options) {
			const auto search = search_routes(
			    snapshot_, query_, progress, search_options);
			if (!search.problem.empty())
				return route_path_result{};
			auto result = build_route_path(search, segment);
			if (!result.reached || !position.valid ||
			    !valid_segment(snapshot_, segment) ||
			    !snapshot_.topology.segments[segment].center.valid)
				return route_path_result{};
			result.distance += point_distance(
			    snapshot_.topology.segments[segment].center, position);
			result.terminal_segment = segment;
			result.terminal_position = position;
			return result;
		};
		auto path = find_path(state_.progress, options);
		/* Prefer conventional keyed progression when excluding the selected
		 * trigger exposes an equally direct keyed-door dependency. Keep very
		 * large custom levels on the single-pass route to bound analysis cost */
		static constexpr std::size_t keyed_preference_segment_limit = 2048;
		if (snapshot_.topology.segments.size() <=
		        keyed_preference_segment_limit &&
		    prefer_keyed_progress && optimistic && options.prioritize_progress &&
		    path.reached &&
		    path.has_obstruction &&
		    path.first_obstruction.blocker == route_edge_blocker::trigger &&
		    valid_trigger(snapshot_, path.first_obstruction.trigger)) {
			auto preferred_progress = state_.progress;
			preferred_progress.avoided_triggers[path.first_obstruction.trigger] = 1;
			auto preferred = find_path(preferred_progress, options);
			if (preferred.reached &&
			    preferred.progress_weight <= path.progress_weight &&
			    preferred.has_obstruction &&
			    preferred.first_obstruction.blocker ==
			        route_edge_blocker::missing_key)
				path = std::move(preferred);
		}
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
		/* D2 bosses can engage or teleport from a room sealed by a
		 * buddy-proof wall.  Route the Guide-Bot to the player handoff point
		 * instead of treating the boss, and therefore the exit, as unreachable. */
		if (target.kind == route_target_kind::boss) {
			route_path_result best;
			for (const int segment : search.visit_order) {
				const auto &topology_segment =
				    snapshot_.topology.segments[segment];
				for (int side = 0; side < LEVEL_METADATA_MAX_SIDES; ++side) {
					const auto &topology_side = topology_segment.sides[side];
					if (topology_side.child != target.segment)
						continue;
					const int reverse_side = topology_side.reverse_side;
					const int reverse_wall =
					    reverse_side >= 0 &&
					            reverse_side < LEVEL_METADATA_MAX_SIDES
					        ? snapshot_.topology.segments[target.segment]
					              .sides[reverse_side]
					              .wall
					        : -1;
					const bool buddy_proof =
					    (valid_wall(snapshot_, topology_side.wall) &&
					     snapshot_.state.walls[topology_side.wall].buddy_proof) ||
					    (valid_wall(snapshot_, reverse_wall) &&
					     snapshot_.state.walls[reverse_wall].buddy_proof);
					if (!buddy_proof || !topology_segment.center.valid)
						continue;
					auto candidate = build_route_path(search, segment);
					if (!candidate.reached ||
					    (best.reached && candidate.distance >= best.distance))
						continue;
					candidate.terminal_segment = segment;
					candidate.terminal_position = topology_segment.center;
					candidate.waits_for_player = true;
					best = std::move(candidate);
				}
			}
			if (best.reached) {
				report_progress(
				    visibility_, "route_target_visibility", total, total);
				return best;
			}
		}
		report_progress(
		    visibility_, "route_target_visibility", total, total);
		return {};
	}

	void append_pending_path(const route_path_result &path)
	{
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

	route_path_result path_prefix_through_edge(
	    const route_path_result &path,
	    std::size_t edge) const
	{
		route_path_result prefix;
		if (edge >= path.sides.size() || edge + 1 >= path.segments.size())
			return prefix;
		prefix.reached = true;
		prefix.segments.assign(
		    path.segments.begin(), path.segments.begin() + edge + 2);
		prefix.sides.assign(
		    path.sides.begin(), path.sides.begin() + edge + 1);
		prefix.terminal_segment = prefix.segments.back();
		prefix.terminal_position =
		    snapshot_.topology.segments[prefix.terminal_segment].center;
		prefix.distance = point_distance(
		    state_.progress.current_position,
		    snapshot_.topology.segments[prefix.segments.front()].center);
		for (std::size_t index = 0; index <= edge; ++index)
			prefix.distance += point_distance(
			    snapshot_.topology.segments[prefix.segments[index]].center,
			    snapshot_.topology.segments[prefix.segments[index + 1]].center);
		return prefix;
	}

	bool accumulate_path(const route_path_result &path)
	{
		if (!state_.progress.wall_state_authoritative || !transition_aware_paths_) {
			route_progress_traverse_path(snapshot_, state_.progress, path, false);
			append_pending_path(path);
			return true;
		}
		for (std::size_t index = 0;
		     index < path.sides.size() && index < path.segments.size(); ++index) {
			const int segment = path.segments[index];
			const int side = path.sides[index];
			if (!valid_segment(snapshot_, segment) || side < 0 ||
			    side >= LEVEL_METADATA_MAX_SIDES)
				continue;
			const int wall = snapshot_.topology.segments[segment].sides[side].wall;
			if (!valid_wall(snapshot_, wall) ||
			    snapshot_.topology.walls[wall].shootable_trigger)
				continue;
			const int trigger = snapshot_.state.walls[wall].trigger;
			if (!valid_trigger(snapshot_, trigger) ||
			    snapshot_.state.triggers[trigger].disabled)
				continue;
			const bool navigation_transition = route_trigger_changes_navigation(
			    snapshot_.topology.triggers[trigger].kind);
			if (!route_progress_apply_trigger(
			        snapshot_, state_.progress, trigger) ||
			    !navigation_transition || index + 1 == path.sides.size())
				continue;
			const auto prefix = path_prefix_through_edge(path, index);
			if (!prefix.reached || !prefix.terminal_position.valid)
				return false;
			append_pending_path(prefix);
			state_.progress.current_segment = prefix.terminal_segment;
			state_.progress.current_position = prefix.terminal_position;
			return false;
		}
		append_pending_path(path);
		return true;
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

	bool append_hidden_door_step(
	    int segment, int side, int wall, int action_segment = -1)
	{
		route_semantic_step step;
		step.kind = route_semantic_step_kind::hidden_door;
		step.segment = valid_segment(snapshot_, action_segment)
		                   ? action_segment
		                   : segment;
		step.side = step.segment == segment ? side : -1;
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

	bool append_blastable_wall_step(
	    int segment, int side, int wall, int action_segment = -1)
	{
		route_semantic_step step;
		step.kind = route_semantic_step_kind::blastable_wall;
		step.segment = valid_segment(snapshot_, action_segment)
		                   ? action_segment
		                   : segment;
		step.side = step.segment == segment ? side : -1;
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
				append_trigger_step(sources.front(), false, true);
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

	bool resolve_shot_blocker(
	    int wall,
	    int firing_segment,
	    const route_position &firing_position,
	    int depth)
	{
		if (!valid_wall(snapshot_, wall) ||
		    !valid_segment(snapshot_, firing_segment) ||
		    !firing_position.valid) {
			set_problem("conditional shot blocker missing");
			return false;
		}
		const auto &topology = snapshot_.topology.walls[wall];
		if (!valid_segment(snapshot_, topology.segment) || topology.side < 0 ||
		    topology.side >= LEVEL_METADATA_MAX_SIDES) {
			set_problem("conditional shot blocker topology invalid");
			return false;
		}
		const auto &wall_state = snapshot_.state.walls[wall];
		if (wall_state.kind == route_wall_kind::blastable) {
			if (!move_to_target(
			        firing_segment, firing_position, depth + 1) ||
			    !append_blastable_wall_step(
			        topology.segment, topology.side, wall, firing_segment) ||
			    !route_progress_destroy_blastable_wall(
			        snapshot_, state_.progress, wall))
				return false;
			return true;
		}
		if (wall_state.kind != route_wall_kind::door || !wall_state.hidden ||
		    wall_state.key != route_key_requirement::none) {
			set_problem("conditional shot blocker is not shoot-open");
			return false;
		}
		auto edge = evaluate_route_edge(
		    snapshot_, query_, state_.progress, topology.segment,
		    topology.side);
		if (edge.blocker == route_edge_blocker::trigger) {
			if (!fire_trigger(
			        topology.segment, topology.side, depth + 1))
				return false;
			edge = evaluate_route_edge(
			    snapshot_, query_, state_.progress, topology.segment,
			    topology.side);
		}
		if (edge.progress_cost == LEVEL_METADATA_ROUTE_EDGE_BLOCKED) {
			set_problem("conditional hidden door cannot be unlocked");
			return false;
		}
		if (!move_to_target(
		        firing_segment, firing_position, depth + 1) ||
		    !append_hidden_door_step(
		        topology.segment, topology.side, wall, firing_segment) ||
		    !route_progress_open_hidden_wall(
		        snapshot_, state_.progress, wall))
			return false;
		return true;
	}

	bool append_key_step(const route_target &target, int key)
	{
		route_semantic_step step;
		const bool robot_carrier = is_robot_carrier(target);
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

	bool is_robot_carrier(const route_target &target) const
	{
		return target.contained && target.object >= 0 &&
		       static_cast<std::size_t>(target.object) <
		           snapshot_.state.objects.size() &&
		       snapshot_.state.objects[target.object].kind ==
		           route_object_kind::robot;
	}

	void preserve_carrier_continuation_anchor(const route_target &target)
	{
		/* A fleeing carrier can cross an opened asymmetric door toward the
		 * player, so continuation stays on the last safely reversible side */
		if (!is_robot_carrier(target) ||
		    !snapshot_.state.objects[target.object].fleeing ||
		    state_.steps.empty())
			return;
		const auto &path = state_.steps.back().path;
		for (std::size_t index = 0;
		     index < path.sides.size() && index + 1 < path.segments.size();
		     ++index) {
			const int anchor_segment = path.segments[index];
			const int carrier_side_segment = path.segments[index + 1];
			if (!valid_segment(snapshot_, anchor_segment) ||
			    !valid_segment(snapshot_, carrier_side_segment))
				continue;
			const int forward_side = path.sides[index];
			if (forward_side < 0 || forward_side >= LEVEL_METADATA_MAX_SIDES)
				continue;
			const int reverse_side = snapshot_.topology
			                             .segments[anchor_segment]
			                             .sides[forward_side]
			                             .reverse_side;
			if (reverse_side < 0 || reverse_side >= LEVEL_METADATA_MAX_SIDES)
				continue;
			const auto reverse = evaluate_route_edge(
			    snapshot_, query_, state_.progress, carrier_side_segment,
			    reverse_side);
			if (reverse.progress_cost != LEVEL_METADATA_ROUTE_EDGE_BLOCKED)
				continue;
			const auto &anchor =
			    snapshot_.topology.segments[anchor_segment].center;
			if (!anchor.valid)
				continue;
			state_.progress.current_segment = anchor_segment;
			state_.progress.current_position = anchor;
			return;
		}
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
		if (semantic_key_mask_ >= 0 && !(semantic_key_mask_ & bit)) {
			set_problem(
			    std::string(dependency_key_name(key)) +
			    " key excluded from prerequisite trial");
			state_.failed_key = key;
			return false;
		}
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
		if (!append_key_step(target, key)) {
			state_.progress.key_in_progress &= ~bit;
			return false;
		}
		state_.progress.key_mask |= bit;
		state_.progress.key_in_progress &= ~bit;
		preserve_carrier_continuation_anchor(target);
		return true;
	}

	bool acquire_exit_key(const route_target &target)
	{
		if (!valid_segment(snapshot_, target.segment) || target.side < 0 ||
		    target.side >= LEVEL_METADATA_MAX_SIDES)
			return true;
		const int wall = snapshot_.topology.segments[target.segment]
		                     .sides[target.side]
		                     .wall;
		if (!valid_wall(snapshot_, wall))
			return true;
		const auto &exit_wall = snapshot_.state.walls[wall];
		if (exit_wall.opened || exit_wall.kind != route_wall_kind::door)
			return true;
		const auto &exit_side =
		    snapshot_.topology.segments[target.segment].sides[target.side];
		for (const int source_wall : exit_side.opener_walls) {
			if (!valid_wall(snapshot_, source_wall))
				continue;
			const int trigger = snapshot_.state.walls[source_wall].trigger;
			if (valid_trigger(snapshot_, trigger) &&
			    state_flag(state_.progress.fired_triggers, trigger))
				return true;
		}
		const int required_key = key_index(exit_wall.key);
		return required_key < 0 || acquire_key(required_key, 0);
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
		if (!accumulate_path(visible))
			return move_to_target_or_visible(target, depth + 1);
		if (visible.waits_for_player)
			state_.note =
			    "Guide-Bot waits at the buddy-proof wall outside the boss";
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
		         route_progress_wall_kind(
		             snapshot_, state_.progress, source.source_wall) ==
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
		    !route_progress_apply_trigger(snapshot_, state_.progress, trigger))
			return false;
		state_.unresolved_action = true;
		if (state_.unresolved_action_problem.empty())
			state_.unresolved_action_problem =
			    problem.empty() ? "switch activation route unresolved" : problem;
		if (state_.problem.empty())
			state_.problem = state_.unresolved_action_problem;
		return true;
	}

	bool expose_trigger_source(
	    const route_trigger_source &hidden_source,
	    int depth)
	{
		if (!valid_wall(snapshot_, hidden_source.source_wall))
			return false;
		const auto exposure_start = state_;
		std::string last_problem;
		for (int trigger = 0;
		     trigger < static_cast<int>(snapshot_.topology.triggers.size());
		     ++trigger) {
			const auto kind = snapshot_.topology.triggers[trigger].kind;
			if (kind != route_trigger_kind::close_wall &&
			    kind != route_trigger_kind::illusion_on &&
			    kind != route_trigger_kind::close_door)
				continue;
			if (!trigger_targets_wall(
			        snapshot_, trigger, hidden_source.source_wall))
				continue;
			auto sources = discover_sources_for_trigger(
			    snapshot_, state_.progress, trigger, false);
			sources.erase(
			    std::remove_if(
			        sources.begin(), sources.end(), [&](const auto &source) {
				        return !trigger_source_fits_navigator(
				            snapshot_, query_, source);
			        }),
			    sources.end());
			if (sources.empty())
				continue;
			state_ = exposure_start;
			const auto &link = snapshot_.topology.triggers[trigger].links.front();
			if (!fire_trigger(
			        link.segment, link.side, depth + 1, &sources)) {
				if (!state_.problem.empty())
					last_problem = state_.problem;
				continue;
			}
			if (route_progress_wall_kind(
			        snapshot_, state_.progress, hidden_source.source_wall) !=
			    route_wall_kind::open)
				return true;
		}
		state_ = exposure_start;
		if (!last_problem.empty())
			state_.problem = last_problem;
		return false;
	}

	bool prepare_unreachable_trigger_source(
	    const route_trigger_source &source,
	    int depth)
	{
		if (!valid_wall(snapshot_, source.source_wall))
			return false;
		bool has_restorer = false;
		for (int trigger = 0;
		     trigger < static_cast<int>(snapshot_.topology.triggers.size());
		     ++trigger) {
			const auto kind = snapshot_.topology.triggers[trigger].kind;
			if (kind != route_trigger_kind::close_wall &&
			    kind != route_trigger_kind::illusion_on &&
			    kind != route_trigger_kind::close_door)
				continue;
			if (!trigger_targets_wall(snapshot_, trigger, source.source_wall))
				continue;
			has_restorer = true;
			break;
		}
		if (!has_restorer)
			return false;
		const auto preparation_start = state_;
		const auto initial_kind = route_progress_wall_kind(
		    snapshot_, state_.progress, source.source_wall);
		std::string last_problem;
		for (int trigger = 0;
		     trigger < static_cast<int>(snapshot_.topology.triggers.size());
		     ++trigger) {
			if (trigger == source.trigger ||
			    !route_trigger_opens_path(
			        snapshot_.topology.triggers[trigger].kind) ||
			    !trigger_targets_wall(snapshot_, trigger, source.source_wall) ||
			    !trigger_effect_needed(snapshot_, state_.progress, trigger))
				continue;
			auto sources = discover_sources_for_trigger(
			    snapshot_, state_.progress, trigger, false);
			sources.erase(
			    std::remove_if(
			        sources.begin(), sources.end(), [&](const auto &candidate) {
				        return !trigger_source_fits_navigator(
				            snapshot_, query_, candidate);
			        }),
			    sources.end());
			if (sources.empty() ||
			    snapshot_.topology.triggers[trigger].links.empty())
				continue;
			state_ = preparation_start;
			state_.problem.clear();
			const auto &link = snapshot_.topology.triggers[trigger].links.front();
			if (!fire_trigger(
			        link.segment, link.side, depth + 1, &sources)) {
				if (!state_.problem.empty())
					last_problem = state_.problem;
				continue;
			}
			if (route_progress_wall_kind(
			        snapshot_, state_.progress, source.source_wall) != initial_kind)
				return true;
		}
		state_ = preparation_start;
		if (!last_problem.empty())
			state_.problem = last_problem;
		return false;
	}

	bool fire_trigger(
	    int segment,
	    int side,
	    int depth,
	    const std::vector<route_trigger_source> *forced_sources = nullptr)
	{
		auto raw_sources = forced_sources
		                       ? *forced_sources
		                       : discover_trigger_sources_internal(
		                             snapshot_, state_.progress, segment, side,
		                             true, true);
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
		auto firing_sources = raw_sources;
		firing_sources.erase(
		    std::remove_if(
		        firing_sources.begin(), firing_sources.end(), [&](const auto &candidate) {
			        return !trigger_source_fits_navigator(
			                   snapshot_, query_, candidate) ||
			               !trigger_source_wall_valid(
			                   snapshot_, state_.progress,
			                   candidate.source_wall, false, true);
		        }),
		    firing_sources.end());
		if (firing_sources.empty() &&
		    valid_wall(snapshot_, source.source_wall) &&
		    snapshot_.topology.walls[source.source_wall].shootable_trigger &&
		    route_progress_wall_kind(
		        snapshot_, state_.progress, source.source_wall) ==
		        route_wall_kind::open) {
			if (state_flag(
			        state_.progress.trigger_in_progress, source.trigger)) {
				set_problem(
				    "trigger exposure dependency loop: trigger " +
				    std::to_string(source.trigger));
				state_.failed_trigger = source.trigger;
				return false;
			}
			state_.progress.trigger_in_progress[source.trigger] = 1;
			const bool exposed = expose_trigger_source(source, depth + 1);
			state_.progress.trigger_in_progress[source.trigger] = 0;
			if (!exposed) {
				set_problem(
				    "switch surface restoration unresolved: trigger " +
				    std::to_string(source.trigger) + " wall " +
				    std::to_string(source.source_wall));
				state_.failed_trigger = source.trigger;
				return false;
			}
			if (forced_sources) {
				auto exposed_sources = discover_sources_for_trigger(
				    snapshot_, state_.progress, source.trigger, false);
				return fire_trigger(
				    segment, side, depth + 1, &exposed_sources);
			}
			return fire_trigger(segment, side, depth + 1);
		}
		const auto firing = select_trigger_firing_path_internal(
		    snapshot_, query_, state_.progress, firing_sources, visibility_,
		    &switch_guidance_graph_);
		auto selected_firing = firing;
		bool conditional_firing = false;
		if (!selected_firing.found &&
		    visibility_.wall_potentially_shootable) {
			auto approximate_visibility = visibility_;
			approximate_visibility.wall_shootable =
			    visibility_.wall_potentially_shootable;
			approximate_visibility.approximate_shots = true;
			approximate_visibility.sample_cache_namespace ^= 0x80000000u;
			selected_firing = select_trigger_firing_path_internal(
			    snapshot_, query_, state_.progress, firing_sources,
			    approximate_visibility, &switch_guidance_graph_);
		}
		if (!selected_firing.found &&
		    visibility_.wall_conditionally_shootable &&
		    visibility_.wall_first_shot_blocker) {
			auto conditional_visibility = visibility_;
			conditional_visibility.wall_shootable =
			    visibility_.wall_conditionally_shootable;
			conditional_visibility.sample_cache_namespace ^= 0x40000000u;
			selected_firing = select_trigger_firing_path_internal(
			    snapshot_, query_, state_.progress, firing_sources,
			    conditional_visibility, &switch_guidance_graph_);
			conditional_firing = selected_firing.found;
		}
		if (selected_firing.found)
			source = selected_firing.source;
		const bool shootable = valid_wall(snapshot_, source.source_wall) &&
		                       snapshot_.topology.walls[source.source_wall]
		                           .shootable_trigger;
		/* A fired bit denotes an effect that is still active. Contrary wall
		 * transitions clear that bit when they restore a shootable surface, so
		 * only a genuinely rearmed trigger reaches the firing path again. */
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
		const auto activation_start = state_;
		state_.progress.trigger_in_progress[source.trigger] = 1;
		const int selected_source_segment = source.source_segment;
		if (conditional_firing) {
			if (!consume_analysis_work(visibility_)) {
				state_.progress.trigger_in_progress[source.trigger] = 0;
				set_problem("conditional shot blocker analysis incomplete");
				return false;
			}
			const int blocker = visibility_.wall_first_shot_blocker(
			    visibility_.user, selected_firing.terminal_segment,
			    selected_firing.terminal_position, source.source_wall);
			if (blocker < 0 ||
			    !resolve_shot_blocker(
			        blocker, selected_firing.terminal_segment,
			        selected_firing.terminal_position, depth + 1)) {
				state_.progress.trigger_in_progress[source.trigger] = 0;
				if (state_.problem.empty())
					set_problem("conditional shot blocker unresolved");
				return false;
			}
		}
		if (selected_firing.found) {
			if (!conditional_firing &&
			    !accumulate_path(selected_firing.path)) {
				state_.progress.trigger_in_progress[source.trigger] = 0;
				return fire_trigger(
				    segment, side, depth + 1, forced_sources);
			}
			state_.progress.current_segment = selected_firing.terminal_segment;
			state_.progress.current_position = selected_firing.terminal_position;
			if (shootable) {
				source.source_segment = selected_firing.terminal_segment;
				source.source_position = selected_firing.terminal_position;
				if (source.source_segment != selected_source_segment)
					source.source_side = -1;
			} else if (
			    selected_firing.terminal_segment == source.source_segment &&
			    valid_wall(snapshot_, source.source_wall) &&
			    route_progress_wall_kind(
			        snapshot_, state_.progress, source.source_wall) ==
			        route_wall_kind::open &&
			    snapshot_.topology.segments[source.source_segment].center.valid)
				state_.progress.current_position =
				    snapshot_.topology.segments[source.source_segment].center;
		} else if (!move_to_target(
		               source.source_segment, source.source_position, depth + 1)) {
			const std::string activation_problem = state_.problem;
			state_ = activation_start;
			state_.problem.clear();
			if (prepare_unreachable_trigger_source(source, depth + 1))
				return fire_trigger(segment, side, depth + 1, forced_sources);
			state_ = activation_start;
			if (!activation_problem.empty())
				state_.problem = activation_problem;
			return false;
		} else {
			state_.progress.current_segment = source.source_segment;
			state_.progress.current_position = source.source_position;
		}
		if (!append_trigger_step(
		        source, selected_firing.found && !shootable)) {
			state_.progress.trigger_in_progress[source.trigger] = 0;
			return false;
		}
		if (selected_firing.found && selected_firing.uses_transparent_surface)
			state_.steps.back().uses_transparent_surface = true;
		if (selected_firing.found && shootable) {
			state_.steps.back().switch_shot_quality =
			    selected_firing.switch_shot_quality;
			state_.steps.back().switch_shot_incidence_cosine =
			    selected_firing.switch_shot_incidence_cosine;
			auto guidance = selected_firing.guidance_candidates;
			std::sort(
			    guidance.begin(), guidance.end(), [](const auto &left, const auto &right) {
				    const bool left_approximate =
				        left.quality == LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE;
				    const bool right_approximate =
				        right.quality == LEVEL_METADATA_SWITCH_SHOT_APPROXIMATE;
				    if (left_approximate != right_approximate)
					    return !left_approximate;
				    if (left.utility != right.utility)
					    return left.utility < right.utility;
				    return left.segment < right.segment;
			    });
			auto &compiled = state_.steps.back().switch_guidance_candidates;
			route_trigger_path_selection::guidance_candidate canonical;
			canonical.segment = selected_firing.terminal_segment;
			canonical.position = selected_firing.terminal_position;
			canonical.quality = selected_firing.switch_shot_quality;
			canonical.incidence_cosine =
			    selected_firing.switch_shot_incidence_cosine;
			compiled.push_back(canonical);
			auto append_unique = [&](const auto &candidate) {
				if (std::any_of(
				        compiled.begin(), compiled.end(), [&](const auto &existing) {
					        return existing.segment == candidate.segment;
				        }))
					return;
				compiled.push_back(candidate);
			};
			auto initial_guidance = guidance;
			initial_guidance.erase(
			    std::remove_if(
			        initial_guidance.begin(), initial_guidance.end(),
			        [](const auto &candidate) {
				        return candidate.initial_start_utility < 0.0;
			        }),
			    initial_guidance.end());
			std::sort(
			    initial_guidance.begin(), initial_guidance.end(),
			    [](const auto &left, const auto &right) {
				    if (left.initial_start_utility !=
				        right.initial_start_utility)
					    return left.initial_start_utility <
					           right.initial_start_utility;
				    return left.segment < right.segment;
			    });
			for (const auto &candidate : initial_guidance) {
				if (compiled.size() >=
				    LEVEL_METADATA_MAX_SWITCH_GUIDANCE_CANDIDATES - 3)
					break;
				append_unique(candidate);
			}
			for (const auto &candidate : guidance) {
				if (compiled.size() >=
				    LEVEL_METADATA_MAX_SWITCH_GUIDANCE_CANDIDATES)
					break;
				append_unique(candidate);
			}
		}
		if (!route_progress_apply_trigger(
		        snapshot_, state_.progress, source.trigger)) {
			state_.progress.trigger_in_progress[source.trigger] = 0;
			set_problem("trigger state transition failed");
			return false;
		}
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
			    goal_segment, goal_position, false, false);
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
				if (!accumulate_path(direct))
					continue;
				state_.progress.current_segment = goal_segment;
				state_.progress.current_position = goal_position;
				state_.progress.avoided_key_mask = saved_avoided_keys;
				state_.progress.avoided_triggers = saved_avoided_triggers;
				state_.failed_key = -1;
				state_.failed_trigger = -1;
				return true;
			}
			const auto optimistic = path_to_position(
			    goal_segment, goal_position, true, depth == 0);
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
	switch_guidance_graph switch_guidance_graph_;
	bool allow_unresolved_triggers_;
	int semantic_key_mask_;
	std::array<int, 3> semantic_key_order_;
	bool transition_aware_paths_;
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
    const route_visibility_query &visibility,
    bool allow_transition_compatibility)
{
	auto plan_once = [&](const route_query &attempt, bool allow_unresolved,
	                     int semantic_key_mask,
	                     const std::array<int, 3> &semantic_key_order,
	                     bool transition_aware_paths) {
		dependency_planner planner(
		    snapshot, attempt,
		    initial_route_progress_state(snapshot, attempt), visibility,
		    allow_unresolved, semantic_key_mask, semantic_key_order,
		    transition_aware_paths);
		if (attempt.endpoint == route_endpoint_kind::end_of_level)
			return planner.plan_end_level();
		if (attempt.endpoint == route_endpoint_kind::unexplored)
			return planner.plan_unexplored();
		return planner.plan_segment(attempt.target_segment);
	};
	auto plan_mode = [&](int semantic_key_mask,
	                     const std::array<int, 3> &semantic_key_order,
	                     bool allow_relaxed, bool allow_unresolved,
	                     bool transition_aware_paths = true) {
		auto result = plan_once(
		    query, false, semantic_key_mask, semantic_key_order,
		    transition_aware_paths);
		if (allow_relaxed && result.status != route_plan_status::ok &&
		    query.navigator.radius > 0) {
			route_query relaxed = query;
			relaxed.navigator.radius = 0;
			result = plan_once(
			    relaxed, false, semantic_key_mask, semantic_key_order,
			    transition_aware_paths);
		}
		if (result.status == route_plan_status::ok ||
		    query.endpoint != route_endpoint_kind::end_of_level)
			return result;
		if (!allow_unresolved)
			return result;
		auto continued = plan_once(
		    query, true, semantic_key_mask, semantic_key_order,
		    transition_aware_paths);
		return continued.steps.size() > result.steps.size() ? continued : result;
	};
	const std::array<int, 3> default_key_order = { { 0, 2, 1 } };
	if (query.endpoint != route_endpoint_kind::end_of_level)
		return plan_mode(-1, default_key_order, true, true);
	const int relevant_key_mask =
	    discover_route_targets(snapshot).required_key_mask;
	std::vector<route_plan_result> completing_plans;
	route_plan_result best_partial;
	bool have_partial = false;
	auto key_step_mask = [](const route_plan_result &plan) {
		int mask = 0;
		for (const auto &step : plan.steps) {
			if (step.kind != route_semantic_step_kind::key)
				continue;
			const int key = key_index(step.key);
			if (key >= 0)
				mask |= 1 << key;
		}
		return mask;
	};
	auto has_unresolved_trigger = [](const route_plan_result &plan) {
		return std::any_of(
		    plan.steps.begin(), plan.steps.end(), [](const auto &step) {
			    return step.activation ==
			           route_activation_kind::unresolved_trigger;
		    });
	};
	auto collect_completing_plans = [&](bool transition_aware_paths) {
		for (int allowed_key_mask = 0; allowed_key_mask < 8;
		     ++allowed_key_mask) {
			if ((allowed_key_mask & ~relevant_key_mask) != 0)
				continue;
			std::vector<int> selected_keys;
			for (int key = 0; key < 3; ++key)
				if (allowed_key_mask & (1 << key))
					selected_keys.push_back(key);
			do {
				std::array<int, 3> key_order = default_key_order;
				int order_index = 0;
				for (const int key : selected_keys)
					key_order[order_index++] = key;
				for (const int key : default_key_order)
					if (!(allowed_key_mask & (1 << key)))
						key_order[order_index++] = key;
				auto candidate = plan_mode(
				    allowed_key_mask, key_order, false, false,
				    transition_aware_paths);
				if (candidate.status == route_plan_status::ok)
					completing_plans.push_back(std::move(candidate));
				else if (transition_aware_paths &&
				         (!have_partial ||
				          candidate.steps.size() > best_partial.steps.size())) {
					best_partial = std::move(candidate);
					have_partial = true;
				}
			} while (std::next_permutation(
			    selected_keys.begin(), selected_keys.end()));
		}
	};
	collect_completing_plans(true);
	if (allow_transition_compatibility && completing_plans.empty())
		collect_completing_plans(false);
	if (completing_plans.empty()) {
		auto diagnostic = plan_mode(
		    relevant_key_mask, default_key_order, false, true);
		if (has_unresolved_trigger(diagnostic)) {
			diagnostic.status = route_plan_status::partial;
			if (diagnostic.problem.empty())
				diagnostic.problem = "switch activation route unresolved";
		}
		if (!have_partial ||
		    diagnostic.steps.size() > best_partial.steps.size())
			return diagnostic;
		return best_partial;
	}
	auto bit_count = [](int mask) {
		int count = 0;
		for (; mask; mask >>= 1)
			count += mask & 1;
		return count;
	};
	auto best = std::min_element(
	    completing_plans.begin(), completing_plans.end(),
	    [&](const route_plan_result &left, const route_plan_result &right) {
		    const int left_keys = bit_count(key_step_mask(left));
		    const int right_keys = bit_count(key_step_mask(right));
		    if (left_keys != right_keys)
			    return left_keys < right_keys;
		    if (left.travel_distance != right.travel_distance)
			    return left.travel_distance < right.travel_distance;
		    return left.steps.size() < right.steps.size();
	    });
	int required_key_mask = relevant_key_mask;
	int completing_key_mask_set = 0;
	for (const auto &candidate : completing_plans) {
		const int acquired_key_mask = key_step_mask(candidate);
		required_key_mask &= acquired_key_mask;
		completing_key_mask_set |= 1 << acquired_key_mask;
	}
	best->required_key_mask = required_key_mask;
	best->completing_key_mask_set = completing_key_mask_set;
	return std::move(*best);
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

int view_wall_potentially_shootable(
    void *user,
    int segment,
    const dxx_route::route_position &from,
    int wall)
{
	const auto *context = static_cast<view_visibility_context *>(user);
	return context->view->wall_potentially_shootable_from_position(
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

int view_wall_conditionally_shootable(
    void *user,
    int segment,
    const dxx_route::route_position &from,
    int wall)
{
	const auto *context = static_cast<view_visibility_context *>(user);
	return context->view->wall_conditionally_shootable_from_position(
	           context->view->user, segment, from.value.data(), wall) != 0;
}

int view_wall_first_shot_blocker(
    void *user,
    int segment,
    const dxx_route::route_position &from,
    int wall)
{
	const auto *context = static_cast<view_visibility_context *>(user);
	return context->view->wall_first_shot_blocker_from_position(
	    context->view->user, segment, from.value.data(), wall);
}

int view_wall_shot_incidence_cosine(
    void *user,
    const dxx_route::route_position &from,
    int wall)
{
	const auto *context = static_cast<view_visibility_context *>(user);
	return context->view->wall_shot_incidence_cosine(
	    context->view->user, from.value.data(), wall);
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
	destination.switch_shot_quality = source.switch_shot_quality;
	destination.switch_shot_incidence_cosine =
	    source.switch_shot_incidence_cosine;
	destination.switch_guidance_candidate_count = static_cast<int>(
	    std::min(
	        source.switch_guidance_candidates.size(),
	        static_cast<std::size_t>(
	            LEVEL_METADATA_MAX_SWITCH_GUIDANCE_CANDIDATES)));
	for (int index = 0; index < destination.switch_guidance_candidate_count;
	     ++index) {
		const auto &candidate = source.switch_guidance_candidates[index];

		destination.switch_guidance_candidate_seg[index] = candidate.segment;
		for (int coordinate = 0; coordinate < 3; ++coordinate)
			destination.switch_guidance_candidate_pos[index][coordinate] =
			    candidate.position.valid ? candidate.position.value[coordinate] : 0;
		destination.switch_guidance_candidate_quality[index] = candidate.quality;
		destination.switch_guidance_candidate_incidence[index] =
		    candidate.incidence_cosine;
	}
	destination.path_segment_count =
	    static_cast<int>(source.path.segments.size());
	destination.path_terminal_segment = source.path.terminal_segment;
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
	destination.route_required_key_mask = source.required_key_mask;
	destination.route_completing_key_mask_set =
	    source.completing_key_mask_set;
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
		dxx_route::route_analysis_budget analysis_budget;
		analysis_budget.work_limit = 2000000;
		analysis_budget.cache_entry_limit = 65536;
		analysis_budget.cancel_user = view->cancel_user;
		analysis_budget.cancelled = view->cancelled;
		dxx_route::route_visibility_query visibility;
		visibility.user = &visibility_context;
		if (view->target_visible_from_segment)
			visibility.target_visible = view_target_visible;
		if (view->wall_shootable_from_position)
			visibility.wall_shootable = view_wall_shootable;
		if (view->wall_potentially_shootable_from_position)
			visibility.wall_potentially_shootable =
			    view_wall_potentially_shootable;
		if (view->wall_shootable_without_transparency_from_position)
			visibility.wall_shootable_without_transparency =
			    view_wall_shootable_without_transparency;
		if (view->wall_conditionally_shootable_from_position)
			visibility.wall_conditionally_shootable =
			    view_wall_conditionally_shootable;
		if (view->wall_first_shot_blocker_from_position)
			visibility.wall_first_shot_blocker =
			    view_wall_first_shot_blocker;
		if (view->wall_shot_incidence_cosine)
			visibility.wall_shot_incidence_cosine =
			    view_wall_shot_incidence_cosine;
		visibility.progress_user = view->progress_user;
		visibility.progress = view->progress;
		visibility.sample_cache = &sample_cache;
		visibility.analysis_budget = &analysis_budget;
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
				strict_visibility.wall_potentially_shootable = nullptr;
				auto strict = dxx_route::plan_route(
				    snapshot, query, strict_visibility, false);
				const int result_keys = plan_key_mask(result);
				const int strict_keys = plan_key_mask(strict);
				const int bypassable_keys =
				    preceding_keys & strict_keys;
				if (strict.status == dxx_route::route_plan_status::ok &&
				    !plan_uses_transparent_shot(strict) && bypassable_keys &&
				    (strict_keys & result_keys) == result_keys &&
				    !plans_have_same_objective_sequence(strict, result) &&
				    plan_unresolved_trigger_count(strict) <=
				        plan_unresolved_trigger_count(result)) {
					annotate_bypassable_keys(strict, bypassable_keys);
					result = strict;
				}
			}
		}
		if (analysis_budget.was_cancelled || analysis_budget.exhausted) {
			if (analysis_budget.exhausted && result.steps.size() > 1 &&
			    project_plan(
			        result, endpoint_kind, *state, unexplored, *summary,
			        detail)) {
				state->route_status = LEVEL_METADATA_ROUTE_PARTIAL;
				copy_problem(
				    state->route_problem,
				    static_cast<int>(sizeof(state->route_problem)),
				    "shared route planning reached its work budget");
				return 1;
			}
			copy_problem(
			    problem, problem_capacity,
			    analysis_budget.was_cancelled
			        ? "shared route planning cancelled"
			        : "shared route planning exceeded its work budget");
			return 0;
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
		dxx_route::route_analysis_budget analysis_budget;
		analysis_budget.work_limit = 2000000;
		analysis_budget.cache_entry_limit = 65536;
		analysis_budget.cancel_user = view->cancel_user;
		analysis_budget.cancelled = view->cancelled;
		dxx_route::route_visibility_query visibility;
		visibility.user = &visibility_context;
		if (view->target_visible_from_segment)
			visibility.target_visible = view_target_visible;
		if (view->wall_shootable_from_position)
			visibility.wall_shootable = view_wall_shootable;
		if (view->wall_potentially_shootable_from_position)
			visibility.wall_potentially_shootable =
			    view_wall_potentially_shootable;
		if (view->wall_shootable_without_transparency_from_position)
			visibility.wall_shootable_without_transparency =
			    view_wall_shootable_without_transparency;
		if (view->wall_conditionally_shootable_from_position)
			visibility.wall_conditionally_shootable =
			    view_wall_conditionally_shootable;
		if (view->wall_first_shot_blocker_from_position)
			visibility.wall_first_shot_blocker =
			    view_wall_first_shot_blocker;
		if (view->wall_shot_incidence_cosine)
			visibility.wall_shot_incidence_cosine =
			    view_wall_shot_incidence_cosine;
		visibility.progress_user = view->progress_user;
		visibility.progress = view->progress;
		visibility.sample_cache = &sample_cache;
		visibility.analysis_budget = &analysis_budget;
		const auto result = dxx_route::plan_route(snapshot, query, visibility);
		return !analysis_budget.exhausted && !analysis_budget.was_cancelled &&
		       result.status == dxx_route::route_plan_status::ok;
	} catch (...) {
		return 0;
	}
}
