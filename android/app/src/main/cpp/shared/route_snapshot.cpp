#include "route_snapshot.h"
#include "route_snapshot_c.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <utility>

namespace dxx_route
{
namespace
{

class stable_hasher
{
  public:
	void add_bool(bool value)
	{
		add_byte(value ? 1u : 0u);
	}

	void add_int(int value)
	{
		const std::uint32_t bits = static_cast<std::uint32_t>(value);
		for (unsigned shift = 0; shift < 32; shift += 8)
			add_byte(static_cast<unsigned char>(bits >> shift));
	}

	std::uint64_t value() const
	{
		return m_value;
	}

  private:
	void add_byte(unsigned char value)
	{
		m_value ^= value;
		m_value *= 1099511628211ull;
	}

	std::uint64_t m_value = 14695981039346656037ull;
};

void hash_position(stable_hasher &hasher, const route_position &position)
{
	hasher.add_bool(position.valid);
	for (const int coordinate : position.value)
		hasher.add_int(coordinate);
}

route_position read_position(int (*callback)(void *, int, int[3]),
                             void *user, int index)
{
	route_position result;
	int value[3] = {};
	if (callback && callback(user, index, value)) {
		result.valid = true;
		result.value = { { value[0], value[1], value[2] } };
	}
	return result;
}

route_position read_segment_vertex(const level_metadata_scan_view &view,
                                   int segment, int vertex)
{
	route_position result;
	int value[3] = {};
	if (view.segment_vertex &&
	    view.segment_vertex(view.user, segment, vertex, value)) {
		result.valid = true;
		result.value = { { value[0], value[1], value[2] } };
	}
	return result;
}

route_position read_start_position(const level_metadata_scan_view &view)
{
	route_position result;
	int value[3] = {};
	if (view.start_position && view.start_position(view.user, value)) {
		result.valid = true;
		result.value = { { value[0], value[1], value[2] } };
	}
	return result;
}

std::uint64_t hash_topology(const route_topology &topology)
{
	stable_hasher hasher;
	hasher.add_int(1);
	hasher.add_int(static_cast<int>(topology.segments.size()));
	hasher.add_int(static_cast<int>(topology.walls.size()));
	for (const auto &segment : topology.segments) {
		hash_position(hasher, segment.center);
		for (const auto &vertex : segment.vertices)
			hash_position(hasher, vertex);
		for (const auto &side : segment.sides) {
			hasher.add_int(side.child);
			hasher.add_int(side.reverse_side);
			hasher.add_int(side.wall);
		}
	}
	for (const auto &wall : topology.walls) {
		hasher.add_int(wall.segment);
		hasher.add_int(wall.side);
	}
	return hasher.value();
}

std::uint64_t hash_state(const route_state &state)
{
	stable_hasher hasher;
	hasher.add_int(1);
	hasher.add_int(state.start_segment);
	hash_position(hasher, state.start_position);
	hasher.add_int(state.key_mask);
	hasher.add_bool(state.control_center_destroyed);
	hasher.add_int(static_cast<int>(state.segments.size()));
	hasher.add_int(static_cast<int>(state.walls.size()));
	for (const auto &segment : state.segments) {
		hasher.add_bool(segment.explored);
		for (const auto &side : segment.sides) {
			hasher.add_bool(side.flyable);
			hasher.add_bool(side.hard_blocked);
			hasher.add_bool(side.control_center_link);
			hasher.add_bool(side.exit_trigger);
		}
	}
	for (const auto &wall : state.walls) {
		hasher.add_int(wall.type);
		hasher.add_int(wall.flags);
		hasher.add_int(wall.keys);
		hasher.add_int(wall.clip_flags);
		hasher.add_int(wall.trigger);
	}
	return hasher.value();
}

bool fail(std::string *problem, const char *message)
{
	if (problem)
		*problem = message;
	return false;
}

} // namespace

bool build_route_snapshot(const level_metadata_scan_view &view,
                          route_snapshot &snapshot,
                          std::string *problem)
{
	if (problem)
		problem->clear();
	if (view.num_segments <= 0 ||
	    view.num_segments > LEVEL_METADATA_MAX_SEGMENTS)
		return fail(problem, "invalid route snapshot segment count");
	if (view.num_walls < 0 || view.num_walls > LEVEL_METADATA_MAX_WALLS)
		return fail(problem, "invalid route snapshot wall count");
	if (!view.segment_child)
		return fail(problem, "route snapshot requires segment adjacency");

	route_snapshot next;
	next.topology.segments.resize(static_cast<std::size_t>(view.num_segments));
	next.topology.walls.resize(static_cast<std::size_t>(view.num_walls));
	next.state.start_segment = view.start_segment;
	next.state.start_position = read_start_position(view);
	next.state.key_mask = view.initial_key_mask;
	next.state.control_center_destroyed =
	    view.initial_control_center_destroyed != 0;
	next.state.segments.resize(static_cast<std::size_t>(view.num_segments));
	next.state.walls.resize(static_cast<std::size_t>(view.num_walls));

	for (int segment_index = 0; segment_index < view.num_segments;
	     ++segment_index) {
		auto &topology_segment = next.topology.segments[segment_index];
		auto &state_segment = next.state.segments[segment_index];
		topology_segment.center =
		    read_position(view.segment_center, view.user, segment_index);
		for (int vertex = 0; vertex < 8; ++vertex)
			topology_segment.vertices[vertex] =
			    read_segment_vertex(view, segment_index, vertex);
		state_segment.explored = view.segment_is_explored &&
		                         view.segment_is_explored(
		                             view.user, segment_index) != 0;
		for (int side_index = 0; side_index < LEVEL_METADATA_MAX_SIDES;
		     ++side_index) {
			auto &topology_side = topology_segment.sides[side_index];
			auto &state_side = state_segment.sides[side_index];
			topology_side.child = view.segment_child(
			    view.user, segment_index, side_index);
			if (topology_side.child >= 0 &&
			    topology_side.child < view.num_segments && view.reverse_side)
				topology_side.reverse_side = view.reverse_side(
				    view.user, segment_index, topology_side.child);
			if (view.wall_num)
				topology_side.wall = view.wall_num(
				    view.user, segment_index, side_index);
			state_side.flyable = view.side_is_flyable &&
			                     view.side_is_flyable(
			                         view.user, segment_index, side_index) != 0;
			state_side.hard_blocked = view.side_is_hard_blocked &&
			                          view.side_is_hard_blocked(
			                              view.user, segment_index, side_index) != 0;
			state_side.control_center_link =
			    view.side_is_control_center_link &&
			    view.side_is_control_center_link(
			        view.user, segment_index, side_index) != 0;
			state_side.exit_trigger = view.side_has_exit_trigger &&
			                          view.side_has_exit_trigger(
			                              view.user, segment_index, side_index) != 0;
		}
	}

	for (int wall_index = 0; wall_index < view.num_walls; ++wall_index) {
		auto &topology_wall = next.topology.walls[wall_index];
		auto &state_wall = next.state.walls[wall_index];
		if (view.wall_segment)
			topology_wall.segment = view.wall_segment(view.user, wall_index);
		if (view.wall_side)
			topology_wall.side = view.wall_side(view.user, wall_index);
		if (view.wall_type)
			state_wall.type = view.wall_type(view.user, wall_index);
		if (view.wall_flags)
			state_wall.flags = view.wall_flags(view.user, wall_index);
		if (view.wall_keys)
			state_wall.keys = view.wall_keys(view.user, wall_index);
		if (view.wall_clip_flags)
			state_wall.clip_flags = view.wall_clip_flags(view.user, wall_index);
		if (view.wall_trigger)
			state_wall.trigger = view.wall_trigger(view.user, wall_index);
	}

	next.topology.hash = hash_topology(next.topology);
	next.state.hash = hash_state(next.state);
	snapshot = std::move(next);
	return true;
}

} // namespace dxx_route

namespace
{

void copy_problem(char *out, int capacity, const char *problem)
{
	if (!out || capacity <= 0)
		return;
	std::snprintf(out, static_cast<std::size_t>(capacity), "%s",
	              problem ? problem : "");
}

} // namespace

extern "C" int route_snapshot_build_summary(
    const level_metadata_scan_view *view,
    route_snapshot_summary *summary,
    char *problem,
    int problem_capacity)
{
	if (summary)
		std::memset(summary, 0, sizeof(*summary));
	copy_problem(problem, problem_capacity, "");
	if (!view || !summary) {
		copy_problem(problem, problem_capacity,
		             "route snapshot summary requires input and output");
		return 0;
	}
	try {
		dxx_route::route_snapshot snapshot;
		std::string detail;
		if (!dxx_route::build_route_snapshot(*view, snapshot, &detail)) {
			copy_problem(problem, problem_capacity, detail.c_str());
			return 0;
		}
		summary->topology_hash = snapshot.topology.hash;
		summary->state_hash = snapshot.state.hash;
		summary->segment_count =
		    static_cast<int>(snapshot.topology.segments.size());
		summary->wall_count =
		    static_cast<int>(snapshot.topology.walls.size());
		summary->start_segment = snapshot.state.start_segment;
		summary->key_mask = snapshot.state.key_mask;
		summary->control_center_destroyed =
		    snapshot.state.control_center_destroyed ? 1 : 0;
		return 1;
	} catch (const std::exception &error) {
		copy_problem(problem, problem_capacity, error.what());
	} catch (...) {
		copy_problem(problem, problem_capacity,
		             "unknown route snapshot failure");
	}
	return 0;
}
