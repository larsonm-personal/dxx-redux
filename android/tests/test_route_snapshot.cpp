#include "route_snapshot.h"
#include "route_snapshot_c.h"
#include "route_edge.h"
#include "route_planner.h"
#include "route_planner_c.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>

#ifdef NDEBUG
#undef assert
#define assert(condition)                                                        \
	do {                                                                         \
		if (!(condition)) {                                                       \
			fprintf(stderr, "assertion failed: %s (%s:%d)\n", #condition,       \
			        __FILE__, __LINE__);                                           \
			abort();                                                               \
		}                                                                        \
	} while (0)
#endif

namespace
{

struct test_level {
	int explored[2] = {};
	int flyable = 0;
	int wall_flags = 0;
	int wall_opening = 0;
	int key_mask = 0;
	int child = 1;
	int trigger_flags = 0;
	int object_segment = 1;
	int wall_key[2] = { 20, 21 };
	int side_clearance = 8 * 65536;
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

int segment_special(void *, int segment)
{
	return segment == 0 ? 7 : 0;
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

int side_clearance(void *user, int segment, int side)
{
	return side == 0 && (segment == 0 || segment == 1)
	           ? static_cast<test_level *>(user)->side_clearance
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

int wall_is_opening(void *user, int wall)
{
	return wall == 0 ? static_cast<test_level *>(user)->wall_opening : 0;
}

int wall_keys(void *user, int wall)
{
	return static_cast<test_level *>(user)->wall_key[wall];
}

int wall_clip_flags(void *, int wall)
{
	return 30 + wall;
}

int wall_trigger(void *, int wall)
{
	(void) wall;
	return 0;
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

int side_center(void *, int segment, int side, int xyz[3])
{
	xyz[0] = segment * 200 + side * 10;
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

struct test_visibility {
	int segment = -1;
	int wall = -1;
	dxx_route::route_position position;
};

bool wall_visible(
    void *user,
    int segment,
    const dxx_route::route_position &position,
    int wall)
{
	const auto &visible = *static_cast<test_visibility *>(user);
	return segment == visible.segment && wall == visible.wall &&
	       position.value == visible.position.value;
}

dxx_route::route_snapshot make_nested_trigger_snapshot()
{
	dxx_route::route_snapshot snapshot;
	snapshot.topology.segments.resize(4);
	snapshot.state.segments.resize(4);
	for (int segment = 0; segment < 4; ++segment) {
		auto &center = snapshot.topology.segments[segment].center;
		center.valid = true;
		center.value = { segment * 10 * 65536, 0, 0 };
	}
	auto connect = [&](int from, int side, int to, int reverse) {
		snapshot.topology.segments[from].sides[side].child = to;
		snapshot.topology.segments[from].sides[side].reverse_side = reverse;
		snapshot.topology.segments[from].sides[side].center =
		    snapshot.topology.segments[from].center;
	};
	connect(0, 0, 1, 0);
	connect(1, 0, 0, 0);
	connect(1, 1, 2, 0);
	connect(2, 0, 1, 1);
	connect(2, 1, 3, 0);
	connect(3, 0, 2, 1);
	snapshot.state.segments[0].sides[0].flyable = true;
	snapshot.state.segments[1].sides[0].flyable = true;
	snapshot.state.segments[1].sides[1].hard_blocked = true;
	snapshot.state.segments[2].sides[0].hard_blocked = true;
	snapshot.state.segments[2].sides[1].hard_blocked = true;
	snapshot.state.segments[3].sides[0].hard_blocked = true;

	snapshot.topology.walls.resize(4);
	snapshot.state.walls.resize(4);
	auto source_wall = [&](int wall, int segment, int side, int trigger) {
		auto &topology = snapshot.topology.walls[wall];
		topology.segment = segment;
		topology.side = side;
		topology.target = snapshot.topology.segments[segment].center;
		topology.shootable_trigger = true;
		snapshot.topology.segments[segment].sides[side].wall = wall;
		snapshot.state.walls[wall].kind = dxx_route::route_wall_kind::open;
		snapshot.state.walls[wall].trigger = trigger;
	};
	auto target_wall = [&](int wall, int segment, int side, int source) {
		auto &topology = snapshot.topology.walls[wall];
		topology.segment = segment;
		topology.side = side;
		topology.target = snapshot.topology.segments[segment].center;
		snapshot.topology.segments[segment].sides[side].wall = wall;
		snapshot.topology.segments[segment].sides[side].opener_walls = {
			source,
		};
		snapshot.state.walls[wall].kind = dxx_route::route_wall_kind::door;
	};
	source_wall(0, 0, 2, 1);
	target_wall(1, 1, 1, 0);
	source_wall(2, 2, 2, 0);
	target_wall(3, 2, 1, 2);

	snapshot.topology.triggers.resize(2);
	snapshot.state.triggers.resize(2);
	for (int trigger = 0; trigger < 2; ++trigger) {
		snapshot.topology.triggers[trigger].raw_type = 70;
		snapshot.topology.triggers[trigger].kind =
		    dxx_route::route_trigger_kind::open_door;
	}
	snapshot.topology.triggers[0].links.push_back({ 2, 1 });
	snapshot.topology.triggers[1].links.push_back({ 1, 1 });
	snapshot.state.start_segment = 0;
	snapshot.state.start_position = snapshot.topology.segments[0].center;
	return snapshot;
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
	view.wall_type_blastable = 10;
	view.wall_type_door = 11;
	view.wall_type_illusion = 12;
	view.wall_type_open = 13;
	view.wall_flag_door_locked = 1;
	view.wall_flag_door_opened = 4;
	view.wall_clip_hidden = 2;
	view.wall_key_none = 20;
	view.wall_key_blue = 21;
	view.wall_key_red = 22;
	view.wall_key_gold = 23;
	view.segment_special_control_center = 7;
	view.obj_type_robot = 80;
	view.obj_type_powerup = 90;
	view.obj_type_control_center = 91;
	view.obj_flag_should_be_dead = 1;
	view.powerup_key_blue = 100;
	view.powerup_key_red = 101;
	view.powerup_key_gold = 102;
	view.segment_child = segment_child;
	view.segment_is_explored = segment_explored;
	view.segment_special = segment_special;
	view.reverse_side = reverse_side;
	view.side_is_flyable = side_flyable;
	view.side_clearance_radius = side_clearance;
	view.side_is_hard_blocked = side_hard_blocked;
	view.side_is_control_center_link = side_control_center_link;
	view.side_has_exit_trigger = side_exit;
	view.wall_num = wall_num;
	view.wall_segment = wall_segment;
	view.wall_side = wall_side;
	view.wall_type = wall_type;
	view.wall_flags = wall_flags;
	view.wall_is_opening = wall_is_opening;
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
	view.side_center = side_center;
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

	if (!dxx_route::build_route_snapshot(view, first, &problem)) {
		fprintf(stderr, "initial route snapshot failed: %s\n", problem.c_str());
		return 1;
	}
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
	assert(first.topology.segments[0].sides[0].clearance_radius ==
	       level.side_clearance);
	assert(first.topology.segments[1].sides[0].center.value[2] == 202);
	assert(first.topology.walls[1].target.value[2] == 202);
	assert(first.topology.segments[1].center.value[0] == 100);
	assert(first.topology.segments[0].control_center);
	assert(first.topology.segments[1].vertices[7].value[2] == 1072);
	assert(first.state.start_position.value[2] == 9);
	assert(first.state.segments[0].sides[0].control_center_link);
	assert(first.state.segments[1].sides[0].hard_blocked);
	assert(first.state.segments[1].sides[0].exit_trigger);
	assert(first.state.walls[1].trigger == 0);
	assert(first.state.walls[0].kind ==
	       dxx_route::route_wall_kind::blastable);
	assert(first.state.walls[1].kind ==
	       dxx_route::route_wall_kind::door);
	assert(first.state.walls[1].key ==
	       dxx_route::route_key_requirement::blue);
	test_level zero_key_level = level;
	zero_key_level.wall_key[1] = 0;
	auto zero_key_view = make_view(zero_key_level);
	dxx_route::route_snapshot zero_key_snapshot;
	assert(dxx_route::build_route_snapshot(
	    zero_key_view, zero_key_snapshot, &problem));
	assert(zero_key_snapshot.state.walls[1].key ==
	       dxx_route::route_key_requirement::none);
	assert(first.state.walls[0].hidden);
	assert(!first.state.walls[0].locked);
	assert(first.topology.segments[1].sides[0].opener_walls[0] == 0);
	assert(first.topology.triggers[0].kind ==
	       dxx_route::route_trigger_kind::open_door);
	assert(first.topology.triggers[0].links[0].segment == 1);
	assert(first.state.objects[0].position.value[2] == 87);
	assert(first.state.objects[0].kind == dxx_route::route_object_kind::robot);
	assert(!first.state.objects[0].should_be_dead);
	assert(first.state.objects[0].boss);
	dxx_route::route_query edge_query;
	edge_query.progression.key_mask = first.state.key_mask;
	auto blastable_edge = dxx_route::evaluate_route_edge(
	    first, edge_query, 0, 0);
	assert(blastable_edge.progress_cost ==
	       LEVEL_METADATA_ROUTE_EDGE_PASSABLE);
	assert(blastable_edge.action ==
	       dxx_route::route_required_action::destroy_blastable_wall);
	edge_query.navigator.radius = level.side_clearance + 1;
	auto narrow_edge = dxx_route::evaluate_route_edge(
	    first, edge_query, 0, 0);
	assert(narrow_edge.progress_cost == LEVEL_METADATA_ROUTE_EDGE_PASSABLE);
	edge_query.navigator.radius = level.side_clearance;
	assert(dxx_route::evaluate_route_edge(first, edge_query, 0, 0)
	           .progress_cost == LEVEL_METADATA_ROUTE_EDGE_PASSABLE);
	edge_query.navigator.companion = true;
	auto triggered_snapshot = first;
	triggered_snapshot.state.segments[1].sides[0].exit_trigger = false;
	auto triggered_edge = dxx_route::evaluate_route_edge(
	    triggered_snapshot, edge_query, 1, 0);
	assert(triggered_edge.progress_cost ==
	       LEVEL_METADATA_ROUTE_EDGE_PROGRESS);
	assert(triggered_edge.blocker ==
	       dxx_route::route_edge_blocker::trigger);
	assert(triggered_edge.action ==
	       dxx_route::route_required_action::activate_trigger);
	auto edge_cases = first;
	edge_cases.state.segments[1].sides[0].exit_trigger = false;
	edge_cases.state.triggers[0].disabled = true;
	edge_cases.state.walls[1].hidden = true;
	edge_cases.state.walls[1].key = dxx_route::route_key_requirement::none;
	auto hidden_edge = dxx_route::evaluate_route_edge(
	    edge_cases, edge_query, 1, 0);
	assert(hidden_edge.progress_cost == LEVEL_METADATA_ROUTE_EDGE_PROGRESS);
	assert(hidden_edge.action ==
	       dxx_route::route_required_action::open_hidden_door);
	edge_cases.state.walls[1].hidden = false;
	edge_cases.state.segments[1].sides[0].hard_blocked = false;
	edge_cases.state.walls[1].key = dxx_route::route_key_requirement::blue;
	auto key_edge = dxx_route::evaluate_route_edge(
	    edge_cases, edge_query, 1, 0);
	assert(key_edge.progress_cost == LEVEL_METADATA_ROUTE_EDGE_PROGRESS);
	assert(key_edge.blocker == dxx_route::route_edge_blocker::missing_key);
	assert(key_edge.action == dxx_route::route_required_action::acquire_key);
	edge_query.progression.key_mask = LEVEL_METADATA_KEY_MASK_BLUE;
	assert(dxx_route::evaluate_route_edge(edge_cases, edge_query, 1, 0)
	           .progress_cost == LEVEL_METADATA_ROUTE_EDGE_PASSABLE);
	edge_cases.state.walls[1].locked = true;
	assert(dxx_route::evaluate_route_edge(edge_cases, edge_query, 1, 0)
	           .blocker == dxx_route::route_edge_blocker::locked_door);
	edge_cases.state.walls[1].locked = false;
	edge_cases.state.segments[1].sides[0].hard_blocked = true;
	auto buddy_edge = dxx_route::evaluate_route_edge(
	    edge_cases, edge_query, 1, 0);
	assert(buddy_edge.blocker == dxx_route::route_edge_blocker::hard_blocked);
	assert(buddy_edge.action ==
	       dxx_route::route_required_action::wait_for_player);
	dxx_route::route_query planner_query;
	planner_query.start = first.state.start_position;
	planner_query.progression.key_mask = first.state.key_mask;
	auto destroyed_snapshot = first;
	destroyed_snapshot.state.control_center_destroyed = true;
	auto destroyed_query = planner_query;
	destroyed_query.endpoint = dxx_route::route_endpoint_kind::end_of_level;
	const auto destroyed_plan = dxx_route::plan_route(
	    destroyed_snapshot, destroyed_query);
	assert(destroyed_plan.status == dxx_route::route_plan_status::ok);
	assert(destroyed_plan.steps.size() == 2);
	assert(destroyed_plan.steps[1].kind ==
	       dxx_route::route_semantic_step_kind::exit);
	assert(destroyed_plan.steps[1].side == 0);
	assert(destroyed_plan.steps[1].activation_position.valid);
	assert(destroyed_plan.steps[1].aim_position.value ==
	       destroyed_snapshot.topology.segments[1].sides[0].center.value);
	assert(destroyed_plan.steps[1].activation_position.value !=
	       destroyed_plan.steps[1].aim_position.value);
	auto blast_query = planner_query;
	blast_query.endpoint = dxx_route::route_endpoint_kind::segment;
	blast_query.target_segment = 1;
	const auto blast_plan = dxx_route::plan_route(first, blast_query);
	assert(blast_plan.status == dxx_route::route_plan_status::ok);
	assert(blast_plan.steps.size() == 3);
	assert(blast_plan.steps[1].kind ==
	       dxx_route::route_semantic_step_kind::blastable_wall);
	assert(blast_plan.steps[1].activation ==
	       dxx_route::route_activation_kind::destroy_blastable_wall);
	assert(blast_plan.steps[1].segment == 0);
	assert(blast_plan.steps[1].side == 0);
	assert(blast_plan.steps[1].wall == 0);
	assert(blast_plan.steps[1].activation_position.value ==
	       first.topology.segments[0].center.value);
	assert(blast_plan.steps[1].aim_position.value ==
	       first.topology.walls[0].target.value);
	assert(blast_plan.steps[2].kind ==
	       dxx_route::route_semantic_step_kind::unexplored);
	auto blasted_progress = dxx_route::initial_route_progress_state(
	    first, blast_query);
	assert(dxx_route::route_progress_destroy_blastable_wall(
	    first, blasted_progress, 0));
	assert(blasted_progress.destroyed_blastable_walls[0]);
	assert(blasted_progress.destroyed_blastable_walls[1]);
	assert(dxx_route::evaluate_route_edge(
	           first, blast_query, blasted_progress, 0, 0)
	           .action == dxx_route::route_required_action::none);
	const auto pessimistic_search = dxx_route::search_routes(
	    first, planner_query, false);
	assert(pessimistic_search.problem.empty());
	const auto direct_path = dxx_route::build_route_path(
	    pessimistic_search, 1);
	assert(direct_path.reached);
	assert(direct_path.progress_weight == 0);
	assert(direct_path.segments.size() == 2);
	assert(direct_path.segments[0] == 0);
	assert(direct_path.segments[1] == 1);
	assert(direct_path.sides.size() == 1);
	assert(direct_path.sides[0] == 0);
	assert(!direct_path.has_obstruction);
	auto clearance_query = planner_query;
	clearance_query.navigator.radius = level.side_clearance + 1;
	const auto clearance_search = dxx_route::search_routes(
	    first, clearance_query, false);
	assert(!clearance_search.nodes[1].reachable);
	clearance_query.endpoint = dxx_route::route_endpoint_kind::segment;
	clearance_query.target_segment = 1;
	const auto clearance_fallback = dxx_route::plan_route(
	    first, clearance_query);
	assert(clearance_fallback.status == dxx_route::route_plan_status::ok);
	assert(clearance_fallback.steps.size() == 3);
	assert(clearance_fallback.steps[1].kind ==
	       dxx_route::route_semantic_step_kind::blastable_wall);
	auto obstructed_snapshot = first;
	obstructed_snapshot.state.walls[0].kind = dxx_route::route_wall_kind::door;
	obstructed_snapshot.state.walls[0].hidden = true;
	obstructed_snapshot.state.walls[0].key = dxx_route::route_key_requirement::none;
	obstructed_snapshot.topology.segments[0].sides[0].center.value[0] = 50;
	obstructed_snapshot.topology.walls[0].target =
	    obstructed_snapshot.topology.segments[0].sides[0].center;
	const auto optimistic_search = dxx_route::search_routes(
	    obstructed_snapshot, planner_query, true);
	const auto obstructed_path = dxx_route::build_route_path(
	    optimistic_search, 1);
	assert(obstructed_path.reached);
	assert(obstructed_path.progress_weight == 1);
	assert(obstructed_path.has_obstruction);
	assert(obstructed_path.first_obstruction.action ==
	       dxx_route::route_required_action::open_hidden_door);
	auto hidden_query = planner_query;
	hidden_query.endpoint = dxx_route::route_endpoint_kind::segment;
	hidden_query.target_segment = 1;
	const auto hidden_plan = dxx_route::plan_route(
	    obstructed_snapshot, hidden_query);
	assert(hidden_plan.status == dxx_route::route_plan_status::ok);
	assert(hidden_plan.steps.size() >= 2);
	const auto &hidden_step = hidden_plan.steps[1];
	assert(hidden_step.kind ==
	       dxx_route::route_semantic_step_kind::hidden_door);
	assert(hidden_step.activation_position.value ==
	       obstructed_snapshot.topology.segments[0].center.value);
	assert(hidden_step.aim_position.value ==
	       obstructed_snapshot.topology.walls[0].target.value);
	assert(hidden_step.activation_position.value !=
	       hidden_step.aim_position.value);
	auto progress = dxx_route::initial_route_progress_state(
	    obstructed_snapshot, planner_query);
	assert(dxx_route::route_progress_open_hidden_wall(
	    obstructed_snapshot, progress, 0));
	assert(progress.opened_hidden_walls[0]);
	assert(progress.opened_hidden_walls[1]);
	const auto opened_search = dxx_route::search_routes(
	    obstructed_snapshot, planner_query, progress, false);
	assert(opened_search.nodes[1].reachable);
	assert(dxx_route::route_progress_acquire_key(
	    progress, dxx_route::route_key_requirement::blue));
	assert((progress.key_mask & LEVEL_METADATA_KEY_MASK_BLUE) != 0);
	auto trigger_progress = dxx_route::initial_route_progress_state(
	    triggered_snapshot, planner_query);
	assert(dxx_route::route_progress_fire_trigger(trigger_progress, 0));
	assert(dxx_route::evaluate_route_edge(
	           triggered_snapshot, planner_query, trigger_progress, 1, 0)
	           .progress_cost == LEVEL_METADATA_ROUTE_EDGE_PASSABLE);
	trigger_progress.fired_triggers[0] = 0;
	trigger_progress.avoided_triggers[0] = 1;
	assert(dxx_route::evaluate_route_edge(
	           triggered_snapshot, planner_query, trigger_progress, 1, 0)
	           .progress_cost == LEVEL_METADATA_ROUTE_EDGE_BLOCKED);
	auto source_progress = dxx_route::initial_route_progress_state(
	    triggered_snapshot, planner_query);
	const auto trigger_sources = dxx_route::discover_trigger_sources(
	    triggered_snapshot, source_progress, 1, 0);
	assert(trigger_sources.size() == 1);
	assert(trigger_sources[0].target_segment == 1);
	assert(trigger_sources[0].target_side == 0);
	assert(trigger_sources[0].target_wall == 1);
	assert(trigger_sources[0].source_wall == 0);
	assert(trigger_sources[0].source_segment == 0);
	assert(trigger_sources[0].source_side == 0);
	assert(trigger_sources[0].trigger == 0);
	assert(trigger_sources[0].trigger_kind ==
	       dxx_route::route_trigger_kind::open_door);
	assert(trigger_sources[0].source_position.valid);
	assert(trigger_sources[0].source_position.value[0] == 0);
	assert(trigger_sources[0].source_position.value[1] == 1);
	assert(trigger_sources[0].source_position.value[2] == 2);
	const auto direct_firing = dxx_route::select_trigger_firing_path(
	    triggered_snapshot, planner_query, source_progress, trigger_sources);
	assert(direct_firing.found);
	assert(direct_firing.source.source_wall == 0);
	assert(direct_firing.source.trigger == 0);
	assert(direct_firing.terminal_segment == 0);
	assert(direct_firing.terminal_position.value ==
	       trigger_sources[0].source_position.value);
	assert(direct_firing.path.reached);
	assert(direct_firing.path.segments.size() == 1);
	const auto direct_dependency = dxx_route::resolve_trigger_dependency(
	    triggered_snapshot, planner_query, source_progress, 1, 0);
	assert(direct_dependency.attempted);
	assert(direct_dependency.resolved);
	assert(direct_dependency.problem.empty());
	assert(direct_dependency.steps.size() == 1);
	assert(direct_dependency.progress.fired_triggers[0]);
	const auto &direct_trigger_step = direct_dependency.steps[0];
	assert(direct_trigger_step.kind ==
	       dxx_route::route_semantic_step_kind::trigger);
	assert(direct_trigger_step.trigger == 0);
	assert(direct_trigger_step.wall == 0);
	assert(direct_trigger_step.segment == 0);
	assert(direct_trigger_step.side == 0);
	assert(direct_trigger_step.activation ==
	       dxx_route::route_activation_kind::pass_through_trigger);
	assert(direct_trigger_step.activation_position.value ==
	       trigger_sources[0].source_position.value);
	assert(direct_trigger_step.aim_position.value ==
	       triggered_snapshot.topology.walls[0].target.value);
	assert(direct_trigger_step.opened_links.size() == 1);
	assert(direct_trigger_step.opened_links[0].segment == 1);
	assert(direct_trigger_step.opened_links[0].side == 0);
	assert(direct_trigger_step.opened_links[0].wall == 1);
	assert(direct_trigger_step.path.reached);
	assert(direct_trigger_step.path.segments.size() == 1);
	assert(direct_trigger_step.path.segments[0] == 0);
	auto fly_through_snapshot = triggered_snapshot;
	fly_through_snapshot.state.walls[0].kind =
	    dxx_route::route_wall_kind::open;
	fly_through_snapshot.topology.segments[1].center =
	    fly_through_snapshot.topology.segments[0].center;
	fly_through_snapshot.topology.walls[0].target =
	    fly_through_snapshot.topology.segments[0].center;
	fly_through_snapshot.topology.segments[0].vertices[7].value = { 0, 0, 0 };
	fly_through_snapshot.topology.segments[0].vertices[6].value = { 0, 100, 0 };
	fly_through_snapshot.topology.segments[0].vertices[2].value = { 0, 200, 0 };
	fly_through_snapshot.topology.segments[0].vertices[3].value = { 0, 0, 100 };
	const auto fly_through_sources = dxx_route::discover_trigger_sources(
	    fly_through_snapshot, source_progress, 1, 0);
	assert(fly_through_sources.size() == 1);
	assert(fly_through_sources[0].source_position.value ==
	       fly_through_snapshot.topology.segments[0].center.value);
	const auto fly_through_dependency = dxx_route::resolve_trigger_dependency(
	    fly_through_snapshot, planner_query, source_progress, 1, 0);
	assert(fly_through_dependency.resolved);
	assert(fly_through_dependency.steps.size() == 1);
	const auto &fly_through_step = fly_through_dependency.steps[0];
	assert(fly_through_step.activation ==
	       dxx_route::route_activation_kind::fly_through_trigger);
	assert(fly_through_step.activation_position.value ==
	       fly_through_snapshot.topology.segments[0].center.value);
	assert(fly_through_step.activation_position.value !=
	       fly_through_step.aim_position.value);
	assert(fly_through_step.aim_position.value[0] >
	       fly_through_step.activation_position.value[0]);
	const auto nested_snapshot = make_nested_trigger_snapshot();
	dxx_route::route_query nested_query;
	nested_query.start = nested_snapshot.state.start_position;
	const auto nested_progress = dxx_route::initial_route_progress_state(
	    nested_snapshot, nested_query);
	const auto nested_dependency = dxx_route::resolve_trigger_dependency(
	    nested_snapshot, nested_query, nested_progress, 2, 1);
	assert(nested_dependency.attempted);
	assert(nested_dependency.resolved);
	assert(nested_dependency.problem.empty());
	assert(nested_dependency.steps.size() == 2);
	assert(nested_dependency.steps[0].trigger == 1);
	assert(nested_dependency.steps[1].trigger == 0);
	assert(nested_dependency.progress.fired_triggers[0]);
	assert(nested_dependency.progress.fired_triggers[1]);
	assert(nested_dependency.steps[0].path.segments.size() == 1);
	assert(nested_dependency.steps[0].path.segments[0] == 0);
	assert(nested_dependency.steps[1].path.segments.size() == 3);
	assert(nested_dependency.steps[1].path.segments[0] == 0);
	assert(nested_dependency.steps[1].path.segments[1] == 1);
	assert(nested_dependency.steps[1].path.segments[2] == 2);
	auto loop_snapshot = nested_snapshot;
	loop_snapshot.topology.walls[0].segment = 2;
	loop_snapshot.topology.walls[0].side = 3;
	loop_snapshot.topology.walls[0].target =
	    loop_snapshot.topology.segments[2].center;
	loop_snapshot.topology.segments[0].sides[2].wall = -1;
	loop_snapshot.topology.segments[2].sides[3].wall = 0;
	const auto loop_progress = dxx_route::initial_route_progress_state(
	    loop_snapshot, nested_query);
	const auto loop_dependency = dxx_route::resolve_trigger_dependency(
	    loop_snapshot, nested_query, loop_progress, 2, 1);
	assert(loop_dependency.attempted);
	assert(!loop_dependency.resolved);
	assert(loop_dependency.problem.rfind(
	           "trigger route dependency loop", 0) == 0);
	assert(loop_dependency.steps.empty());
	auto frontier_query = nested_query;
	frontier_query.endpoint = dxx_route::route_endpoint_kind::segment;
	frontier_query.target_segment = 3;
	const auto frontier_plan = dxx_route::plan_route(
	    loop_snapshot, frontier_query);
	assert(frontier_plan.status != dxx_route::route_plan_status::ok);
	assert(frontier_plan.partial_frontier_segment >= 0);
	assert(frontier_plan.partial_frontier_segment != frontier_query.target_segment);
	auto visible_snapshot = triggered_snapshot;
	visible_snapshot.state.segments[0].sides[0].hard_blocked = true;
	visible_snapshot.topology.walls[0].segment = 1;
	visible_snapshot.topology.walls[0].side = 0;
	visible_snapshot.topology.walls[0].target =
	    visible_snapshot.topology.segments[1].center;
	const auto visible_sources = dxx_route::discover_trigger_sources(
	    visible_snapshot, source_progress, 1, 0);
	assert(visible_sources.size() == 1);
	test_visibility visible;
	visible.segment = 0;
	visible.wall = 0;
	visible.position = visible_snapshot.topology.segments[0].center;
	dxx_route::route_visibility_query visibility;
	visibility.user = &visible;
	visibility.wall_visible = wall_visible;
	const auto visible_firing = dxx_route::select_trigger_firing_path(
	    visible_snapshot, planner_query, source_progress, visible_sources,
	    visibility);
	assert(visible_firing.found);
	assert(visible_firing.terminal_segment == 0);
	assert(visible_firing.terminal_position.value == visible.position.value);
	assert(visible_firing.path.reached);
	assert(visible_firing.path.segments.size() == 1);
	source_progress.fired_triggers[0] = 1;
	assert(dxx_route::discover_trigger_sources(
	           triggered_snapshot, source_progress, 1, 0)
	           .empty());
	source_progress.fired_triggers[0] = 0;
	source_progress.trigger_in_progress[0] = 1;
	assert(dxx_route::discover_trigger_sources(
	           triggered_snapshot, source_progress, 1, 0)
	           .empty());
	auto disabled_trigger_snapshot = triggered_snapshot;
	disabled_trigger_snapshot.state.triggers[0].disabled = true;
	source_progress.trigger_in_progress[0] = 0;
	assert(dxx_route::discover_trigger_sources(
	           disabled_trigger_snapshot, source_progress, 1, 0)
	           .empty());
	level_metadata_state shared_plan = {};
	level_metadata_unexplored_route shared_unexplored = {};
	route_planner_plan_summary plan_summary = {};
	char plan_problem[96] = {};
	assert(route_planner_plan_view(
	    &view, ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL, -1, &shared_plan,
	    nullptr, &plan_summary, plan_problem, sizeof(plan_problem)));
	assert(plan_problem[0] == '\0');
	assert(plan_summary.endpoint_kind == ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL);
	assert(plan_summary.route_step_count == shared_plan.route_step_count);
	assert(shared_plan.travel_distance > 0.0);
	assert(plan_summary.first_pending_step == 1);
	assert(plan_summary.first_pending_path_segment_count > 0);
	assert(plan_summary.first_pending_path_terminal_segment >= 0);
	assert(route_planner_plan_view(
	    &view, ROUTE_PLANNER_ENDPOINT_SEGMENT, 1, &shared_plan, nullptr,
	    &plan_summary, plan_problem, sizeof(plan_problem)));
	assert(plan_summary.endpoint_kind == ROUTE_PLANNER_ENDPOINT_SEGMENT);
	assert(plan_summary.first_pending_step == 1);
	assert(shared_plan.route_steps[1].kind ==
	       LEVEL_METADATA_ROUTE_BLASTABLE_WALL);
	assert(plan_summary.first_pending_path_terminal_segment == 0);
	assert(route_planner_plan_view(
	    &view, ROUTE_PLANNER_ENDPOINT_UNEXPLORED, -1, &shared_plan,
	    &shared_unexplored, &plan_summary, plan_problem,
	    sizeof(plan_problem)));
	assert(shared_unexplored.component_size > 0);
	assert(shared_unexplored.target_seg >= 0);
	assert(shared_unexplored.waypoint_seg >= 0);
	assert(plan_summary.endpoint_kind == ROUTE_PLANNER_ENDPOINT_UNEXPLORED);
	assert(!route_planner_plan_view(
	    &view, -1, -1, &shared_plan, nullptr, &plan_summary, plan_problem,
	    sizeof(plan_problem)));
	assert(std::string(plan_problem) == "invalid shared route endpoint kind");
	const auto targets = dxx_route::discover_route_targets(first);
	assert(targets.reactor_found);
	assert(targets.reactor.segment == 0);
	assert(targets.boss_found);
	assert(targets.boss.segment == 1);
	assert(targets.exits.size() == 1);
	assert(targets.exits[0].segment == 1);
	assert(targets.exits[0].side == 0);
	auto selection_targets = targets.exits;
	auto nearer_exit = targets.exits[0];
	nearer_exit.position = first.topology.segments[1].center;
	selection_targets.push_back(nearer_exit);
	const auto selected_exit = dxx_route::select_route_target(
	    first, planner_query,
	    dxx_route::initial_route_progress_state(first, planner_query),
	    selection_targets);
	assert(selected_exit.found);
	assert(selected_exit.selected_index == 1);
	assert(selected_exit.path.reached);
	assert(selected_exit.path.segments.size() == 2);
	selection_targets[0] = nearer_exit;
	assert(dxx_route::select_route_target(
	           first, planner_query,
	           dxx_route::initial_route_progress_state(first, planner_query),
	           selection_targets)
	           .selected_index == 0);
	auto key_snapshot = first;
	key_snapshot.state.objects[0].kind = dxx_route::route_object_kind::powerup;
	key_snapshot.state.objects[0].key = dxx_route::route_key_requirement::blue;
	key_snapshot.state.objects[0].boss = false;
	key_snapshot.state.objects[0].contains_count = 1;
	key_snapshot.state.objects[0].contains_key = dxx_route::route_key_requirement::gold;
	const auto key_targets = dxx_route::discover_route_targets(key_snapshot);
	assert(key_targets.keys[0].size() == 1);
	assert(!key_targets.keys[0][0].contained);
	assert(key_targets.keys[2].size() == 1);
	assert(key_targets.keys[2][0].contained);
	auto carrier_snapshot = first;
	carrier_snapshot.state.objects[0].boss = false;
	carrier_snapshot.state.objects[0].kind =
	    dxx_route::route_object_kind::robot;
	carrier_snapshot.state.objects[0].key =
	    dxx_route::route_key_requirement::none;
	carrier_snapshot.state.objects[0].contains_count = 1;
	carrier_snapshot.state.objects[0].contains_key =
	    dxx_route::route_key_requirement::blue;
	carrier_snapshot.state.objects[0].segment = 0;
	carrier_snapshot.state.objects[0].position =
	    carrier_snapshot.topology.segments[0].center;
	carrier_snapshot.state.control_center_destroyed = true;
	carrier_snapshot.state.segments[0].sides[0].flyable = false;
	carrier_snapshot.state.segments[0].sides[0].control_center_link = false;
	carrier_snapshot.state.walls[0].kind =
	    dxx_route::route_wall_kind::door;
	carrier_snapshot.state.walls[0].hidden = false;
	carrier_snapshot.state.walls[0].key =
	    dxx_route::route_key_requirement::blue;
	carrier_snapshot.state.walls[0].trigger = -1;
	carrier_snapshot.topology.segments[0].sides[0].opener_walls.clear();
	carrier_snapshot.topology.walls[0].shootable_trigger = false;
	auto carrier_query = planner_query;
	carrier_query.endpoint = dxx_route::route_endpoint_kind::segment;
	carrier_query.target_segment = 1;
	const auto carrier_plan = dxx_route::plan_route(
	    carrier_snapshot, carrier_query);
	assert(carrier_plan.status == dxx_route::route_plan_status::ok);
	const auto carrier_step = std::find_if(
	    carrier_plan.steps.begin(), carrier_plan.steps.end(),
	    [](const dxx_route::route_semantic_step &step) {
		    return step.kind == dxx_route::route_semantic_step_kind::key;
	    });
	assert(carrier_step != carrier_plan.steps.end());
	assert(carrier_step->activation ==
	       dxx_route::route_activation_kind::destroy_key_carrier);
	assert(carrier_step->key_carrier_object == 0);
	assert(carrier_step->label == "Destroy robot carrying blue key");
	const auto selected_key = dxx_route::select_key_target(
	    key_snapshot, planner_query,
	    dxx_route::initial_route_progress_state(key_snapshot, planner_query),
	    dxx_route::route_key_requirement::blue, key_targets.keys[0]);
	assert(selected_key.found);
	assert(selected_key.selected_index == 0);
	auto forbidden_key_snapshot = key_snapshot;
	forbidden_key_snapshot.state.walls[0].kind =
	    dxx_route::route_wall_kind::door;
	forbidden_key_snapshot.state.walls[0].hidden = false;
	forbidden_key_snapshot.state.walls[0].key =
	    dxx_route::route_key_requirement::blue;
	forbidden_key_snapshot.state.triggers[0].disabled = true;
	assert(!dxx_route::select_key_target(
	            forbidden_key_snapshot, planner_query,
	            dxx_route::initial_route_progress_state(
	                forbidden_key_snapshot, planner_query),
	            dxx_route::route_key_requirement::blue,
	            key_targets.keys[0])
	            .found);
	assert(first.topology.hash != 0);
	assert(first.state.hash != 0);
	route_snapshot_summary summary = {};
	char summary_problem[96] = {};
	assert(route_snapshot_build_summary(
	    &view, &summary, summary_problem, sizeof(summary_problem)));
	assert(summary_problem[0] == '\0');
	assert(summary.topology_hash == first.topology.hash);
	assert(summary.state_hash == first.state.hash);
	assert(summary.start_hash == first.state.fingerprints.start);
	assert(summary.progression_hash == first.state.fingerprints.progression);
	assert(summary.navigation_hash == first.state.fingerprints.navigation);
	assert(summary.trigger_hash == first.state.fingerprints.triggers);
	assert(summary.object_hash == first.state.fingerprints.objects);
	assert(summary.automap_hash == first.state.fingerprints.automap);
	assert(summary.segment_count == 2);
	assert(summary.wall_count == 2);
	assert(summary.trigger_count == 1);
	assert(summary.object_count == 1);
	assert(summary.start_segment == 0);

	assert(dxx_route::build_route_snapshot(view, repeated, nullptr));
	assert(repeated.topology.hash == first.topology.hash);
	assert(repeated.state.hash == first.state.hash);
	assert(repeated.state.fingerprints.automap ==
	       first.state.fingerprints.automap);
	level.wall_opening = 1;
	view = make_view(level);
	dxx_route::route_snapshot opening_state;
	assert(dxx_route::build_route_snapshot(view, opening_state, nullptr));
	assert(opening_state.state.walls[0].opened);
	assert(opening_state.state.fingerprints.navigation !=
	       first.state.fingerprints.navigation);
	level.wall_opening = 0;

	level.explored[1] = 1;
	view = make_view(level);
	dxx_route::route_snapshot automap_changed;
	assert(dxx_route::build_route_snapshot(view, automap_changed, nullptr));
	assert(automap_changed.state.fingerprints.automap !=
	       first.state.fingerprints.automap);
	assert(automap_changed.state.fingerprints.navigation ==
	       first.state.fingerprints.navigation);
	assert(automap_changed.state.fingerprints.progression ==
	       first.state.fingerprints.progression);
	assert(automap_changed.state.fingerprints.triggers ==
	       first.state.fingerprints.triggers);
	assert(automap_changed.state.fingerprints.objects ==
	       first.state.fingerprints.objects);
	assert(automap_changed.state.fingerprints.start ==
	       first.state.fingerprints.start);
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
	assert(changed_state.state.walls[0].locked);
	assert(changed_state.state.walls[0].opened);

	dxx_route::route_query query;
	query.endpoint = dxx_route::route_endpoint_kind::unexplored;
	query.start = changed_state.state.start_position;
	query.progression.key_mask = changed_state.state.key_mask;
	query.navigator.companion = true;
	query.navigator.respects_buddy_proof_walls = true;
	assert(query.endpoint == dxx_route::route_endpoint_kind::unexplored);
	assert(query.progression.key_mask == LEVEL_METADATA_KEY_MASK_BLUE);
	assert(query.navigator.companion);

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
