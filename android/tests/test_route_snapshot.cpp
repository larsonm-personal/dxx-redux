#include "route_snapshot.h"
#include "route_snapshot_c.h"

#include <cassert>
#include <cstdio>
#include <string>

namespace
{

struct test_level {
	int explored[2] = {};
	int flyable = 0;
	int wall_flags = 0;
	int key_mask = 0;
	int child = 1;
	int trigger_flags = 0;
	int object_segment = 1;
};

int segment_child(void *user, int segment, int side)
{
	const auto &level = *static_cast<test_level *>(user);
	if (side != 0)
		return -1;
	if (segment == 0)
		return level.child;
	if (segment == 1)
		return 0;
	return -1;
}

int segment_explored(void *user, int segment)
{
	return static_cast<test_level *>(user)->explored[segment];
}

int reverse_side(void *, int, int)
{
	return 0;
}

int side_flyable(void *user, int segment, int side)
{
	return segment == 0 && side == 0
	           ? static_cast<test_level *>(user)->flyable
	           : 0;
}

int side_hard_blocked(void *, int segment, int side)
{
	return segment == 1 && side == 0;
}

int side_control_center_link(void *, int segment, int side)
{
	return segment == 0 && side == 0;
}

int side_exit(void *, int segment, int side)
{
	return segment == 1 && side == 0;
}

int wall_num(void *, int segment, int side)
{
	return side == 0 ? segment : -1;
}

int wall_segment(void *, int wall)
{
	return wall;
}

int wall_side(void *, int)
{
	return 0;
}

int wall_type(void *, int wall)
{
	return 10 + wall;
}

int wall_flags(void *user, int wall)
{
	return wall == 0 ? static_cast<test_level *>(user)->wall_flags : 0;
}

int wall_keys(void *, int wall)
{
	return 20 + wall;
}

int wall_clip_flags(void *, int wall)
{
	return 30 + wall;
}

int wall_trigger(void *, int wall)
{
	return 40 + wall;
}

int opener_count(void *, int segment, int side)
{
	return segment == 1 && side == 0 ? 1 : 0;
}

int opener_wall(void *, int, int, int)
{
	return 0;
}

int trigger_type(void *, int)
{
	return 70;
}

int trigger_flags(void *user, int)
{
	return static_cast<test_level *>(user)->trigger_flags;
}

int trigger_link_count(void *, int)
{
	return 1;
}

int trigger_link_segment(void *, int, int)
{
	return 1;
}

int trigger_link_side(void *, int, int)
{
	return 0;
}

int object_count(void *)
{
	return 1;
}

int object_segment(void *user, int)
{
	return static_cast<test_level *>(user)->object_segment;
}

int object_type(void *, int)
{
	return 80;
}

int object_id(void *, int)
{
	return 81;
}

int object_flags(void *, int)
{
	return 82;
}

int object_contains_type(void *, int)
{
	return 83;
}

int object_contains_id(void *, int)
{
	return 84;
}

int object_contains_count(void *, int)
{
	return 2;
}

int object_position(void *, int, int xyz[3])
{
	xyz[0] = 85;
	xyz[1] = 86;
	xyz[2] = 87;
	return 1;
}

int object_is_boss(void *, int)
{
	return 1;
}

int object_is_companion(void *, int)
{
	return 0;
}

int segment_center(void *, int segment, int xyz[3])
{
	xyz[0] = segment * 100;
	xyz[1] = segment * 100 + 1;
	xyz[2] = segment * 100 + 2;
	return 1;
}

int segment_vertex(void *, int segment, int vertex, int xyz[3])
{
	xyz[0] = segment * 1000 + vertex * 10;
	xyz[1] = xyz[0] + 1;
	xyz[2] = xyz[0] + 2;
	return 1;
}

int start_position(void *, int xyz[3])
{
	xyz[0] = 7;
	xyz[1] = 8;
	xyz[2] = 9;
	return 1;
}

level_metadata_scan_view make_view(test_level &level)
{
	level_metadata_scan_view view = {};
	view.user = &level;
	view.num_segments = 2;
	view.num_walls = 2;
	view.num_triggers = 1;
	view.start_segment = 0;
	view.initial_key_mask = level.key_mask;
	view.segment_child = segment_child;
	view.segment_is_explored = segment_explored;
	view.reverse_side = reverse_side;
	view.side_is_flyable = side_flyable;
	view.side_is_hard_blocked = side_hard_blocked;
	view.side_is_control_center_link = side_control_center_link;
	view.side_has_exit_trigger = side_exit;
	view.wall_num = wall_num;
	view.wall_segment = wall_segment;
	view.wall_side = wall_side;
	view.wall_type = wall_type;
	view.wall_flags = wall_flags;
	view.wall_keys = wall_keys;
	view.wall_clip_flags = wall_clip_flags;
	view.wall_trigger = wall_trigger;
	view.trigger_type_open_door = 70;
	view.trigger_flag_disabled = 4;
	view.triggered_side_opener_count = opener_count;
	view.triggered_side_opener_wall_num = opener_wall;
	view.trigger_type = trigger_type;
	view.trigger_flags = trigger_flags;
	view.trigger_link_count = trigger_link_count;
	view.trigger_link_segment = trigger_link_segment;
	view.trigger_link_side = trigger_link_side;
	view.object_count = object_count;
	view.object_segment = object_segment;
	view.object_type = object_type;
	view.object_id = object_id;
	view.object_flags = object_flags;
	view.object_contains_type = object_contains_type;
	view.object_contains_id = object_contains_id;
	view.object_contains_count = object_contains_count;
	view.object_position = object_position;
	view.object_is_boss = object_is_boss;
	view.object_is_companion = object_is_companion;
	view.segment_center = segment_center;
	view.segment_vertex = segment_vertex;
	view.start_position = start_position;
	return view;
}

} // namespace

int main()
{
	test_level level;
	auto view = make_view(level);
	dxx_route::route_snapshot first;
	dxx_route::route_snapshot repeated;
	std::string problem;

	assert(dxx_route::build_route_snapshot(view, first, &problem));
	assert(problem.empty());
	assert(first.topology.segments.size() == 2);
	assert(first.topology.walls.size() == 2);
	assert(first.state.segments.size() == 2);
	assert(first.state.walls.size() == 2);
	assert(first.topology.triggers.size() == 1);
	assert(first.state.triggers.size() == 1);
	assert(first.state.objects.size() == 1);
	assert(first.topology.segments[0].sides[0].child == 1);
	assert(first.topology.segments[0].sides[0].reverse_side == 0);
	assert(first.topology.segments[0].sides[0].wall == 0);
	assert(first.topology.segments[1].center.value[0] == 100);
	assert(first.topology.segments[1].vertices[7].value[2] == 1072);
	assert(first.state.start_position.value[2] == 9);
	assert(first.state.segments[0].sides[0].control_center_link);
	assert(first.state.segments[1].sides[0].hard_blocked);
	assert(first.state.segments[1].sides[0].exit_trigger);
	assert(first.state.walls[1].trigger == 41);
	assert(first.topology.segments[1].sides[0].opener_walls[0] == 0);
	assert(first.topology.triggers[0].kind ==
	       dxx_route::route_trigger_kind::open_door);
	assert(first.topology.triggers[0].links[0].segment == 1);
	assert(first.state.objects[0].position.value[2] == 87);
	assert(first.state.objects[0].boss);
	assert(first.topology.hash != 0);
	assert(first.state.hash != 0);
	route_snapshot_summary summary = {};
	char summary_problem[96] = {};
	assert(route_snapshot_build_summary(
	    &view, &summary, summary_problem, sizeof(summary_problem)));
	assert(summary_problem[0] == '\0');
	assert(summary.topology_hash == first.topology.hash);
	assert(summary.state_hash == first.state.hash);
	assert(summary.segment_count == 2);
	assert(summary.wall_count == 2);
	assert(summary.trigger_count == 1);
	assert(summary.object_count == 1);
	assert(summary.start_segment == 0);

	assert(dxx_route::build_route_snapshot(view, repeated, nullptr));
	assert(repeated.topology.hash == first.topology.hash);
	assert(repeated.state.hash == first.state.hash);

	level.explored[1] = 1;
	level.flyable = 1;
	level.wall_flags = 5;
	level.trigger_flags = 4;
	level.object_segment = 0;
	level.key_mask = LEVEL_METADATA_KEY_MASK_BLUE;
	view = make_view(level);
	dxx_route::route_snapshot changed_state;
	assert(dxx_route::build_route_snapshot(view, changed_state, nullptr));
	assert(changed_state.topology.hash == first.topology.hash);
	assert(changed_state.state.hash != first.state.hash);
	assert(changed_state.state.triggers[0].disabled);

	level.child = -1;
	view = make_view(level);
	dxx_route::route_snapshot changed_topology;
	assert(dxx_route::build_route_snapshot(view, changed_topology, nullptr));
	assert(changed_topology.topology.hash != first.topology.hash);

	view.num_segments = 0;
	assert(!dxx_route::build_route_snapshot(view, changed_topology, &problem));
	assert(problem == "invalid route snapshot segment count");

	view = make_view(level);
	view.segment_child = nullptr;
	assert(!dxx_route::build_route_snapshot(view, changed_topology, &problem));
	assert(problem == "route snapshot requires segment adjacency");
	assert(!route_snapshot_build_summary(
	    &view, &summary, summary_problem, sizeof(summary_problem)));
	assert(std::string(summary_problem) ==
	       "route snapshot requires segment adjacency");

	puts("route snapshot tests passed");
	return 0;
}
