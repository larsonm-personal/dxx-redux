#include "route_snapshot.h"
#include "route_snapshot_c.h"
#include "route_edge.h"
#include "route_edge_c.h"
#include "route_planner.h"
#include "route_planner_c.h"

#include <cassert>
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
	assert(blastable_edge.legacy_cost ==
	       LEVEL_METADATA_ROUTE_EDGE_PASSABLE);
	assert(blastable_edge.action ==
	       dxx_route::route_required_action::destroy_blastable_wall);
	edge_query.navigator.companion = true;
	auto triggered_snapshot = first;
	triggered_snapshot.state.segments[1].sides[0].exit_trigger = false;
	auto triggered_edge = dxx_route::evaluate_route_edge(
	    triggered_snapshot, edge_query, 1, 0);
	assert(triggered_edge.legacy_cost ==
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
	assert(hidden_edge.legacy_cost == LEVEL_METADATA_ROUTE_EDGE_PROGRESS);
	assert(hidden_edge.action ==
	       dxx_route::route_required_action::open_hidden_door);
	edge_cases.state.walls[1].hidden = false;
	edge_cases.state.segments[1].sides[0].hard_blocked = false;
	edge_cases.state.walls[1].key = dxx_route::route_key_requirement::blue;
	auto key_edge = dxx_route::evaluate_route_edge(
	    edge_cases, edge_query, 1, 0);
	assert(key_edge.legacy_cost == LEVEL_METADATA_ROUTE_EDGE_PROGRESS);
	assert(key_edge.blocker == dxx_route::route_edge_blocker::missing_key);
	assert(key_edge.action == dxx_route::route_required_action::acquire_key);
	edge_query.progression.key_mask = LEVEL_METADATA_KEY_MASK_BLUE;
	assert(dxx_route::evaluate_route_edge(edge_cases, edge_query, 1, 0)
	           .legacy_cost == LEVEL_METADATA_ROUTE_EDGE_PASSABLE);
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
	route_edge_shadow_summary edge_summary = {};
	char edge_problem[96] = {};
	if (!route_edge_compare_view(
	        &view, &edge_summary, edge_problem, sizeof(edge_problem))) {
		fprintf(stderr, "route edge comparison failed: %s\n", edge_problem);
		return 1;
	}
	assert(edge_problem[0] == '\0');
	assert(edge_summary.compared_edge_count ==
	       2 * LEVEL_METADATA_MAX_SIDES);
	assert(edge_summary.mismatch_count == 0);
	dxx_route::route_query planner_query;
	planner_query.start = first.state.start_position;
	planner_query.progression.key_mask = first.state.key_mask;
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
	auto obstructed_snapshot = first;
	obstructed_snapshot.state.walls[0].kind = dxx_route::route_wall_kind::door;
	obstructed_snapshot.state.walls[0].hidden = true;
	obstructed_snapshot.state.walls[0].key = dxx_route::route_key_requirement::none;
	const auto optimistic_search = dxx_route::search_routes(
	    obstructed_snapshot, planner_query, true);
	const auto obstructed_path = dxx_route::build_route_path(
	    optimistic_search, 1);
	assert(obstructed_path.reached);
	assert(obstructed_path.progress_weight == 1);
	assert(obstructed_path.has_obstruction);
	assert(obstructed_path.first_obstruction.action ==
	       dxx_route::route_required_action::open_hidden_door);
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
	           .legacy_cost == LEVEL_METADATA_ROUTE_EDGE_PASSABLE);
	trigger_progress.fired_triggers[0] = 0;
	trigger_progress.avoided_triggers[0] = 1;
	assert(dxx_route::evaluate_route_edge(
	           triggered_snapshot, planner_query, trigger_progress, 1, 0)
	           .legacy_cost == LEVEL_METADATA_ROUTE_EDGE_BLOCKED);
	route_planner_shadow_summary planner_summary = {};
	char planner_problem[96] = {};
	if (!route_planner_compare_view(
	        &view, &planner_summary, planner_problem, sizeof(planner_problem))) {
		fprintf(stderr, "route planner comparison failed: %s\n", planner_problem);
		return 1;
	}
	assert(planner_problem[0] == '\0');
	assert(planner_summary.compared_progress_state_count == 4);
	assert(planner_summary.compared_node_count == 16);
	assert(planner_summary.mismatch_count == 0);
	assert(planner_summary.compared_target_count == 9);
	assert(planner_summary.target_mismatch_count == 0);
	const auto targets = dxx_route::discover_route_targets(first);
	assert(targets.reactor_found);
	assert(targets.reactor.segment == 0);
	assert(targets.boss_found);
	assert(targets.boss.segment == 1);
	assert(targets.exits.size() == 1);
	assert(targets.exits[0].segment == 1);
	assert(targets.exits[0].side == 0);
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
