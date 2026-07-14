#ifndef DXX_ROUTE_PLANNER_H
#define DXX_ROUTE_PLANNER_H

#include "route_edge.h"

#include <array>
#include <string>
#include <vector>

namespace dxx_route
{

struct route_search_node {
	bool reachable = false;
	double distance = 0.0;
	int progress_weight = 0;
	int parent_segment = -1;
	int parent_side = -1;
	route_edge_decision incoming_edge;
};

struct route_search_result {
	int start_segment = -1;
	std::vector<route_search_node> nodes;
	std::vector<int> visit_order;
	std::string problem;
};

struct route_search_options {
	bool optimistic = false;
	bool prioritize_progress = true;
	route_key_requirement forbidden_missing_key =
	    route_key_requirement::none;
};

struct route_path_result {
	bool reached = false;
	double distance = 0.0;
	int progress_weight = 0;
	std::vector<int> segments;
	std::vector<int> sides;
	int terminal_segment = -1;
	route_position terminal_position;
	bool has_obstruction = false;
	int first_obstruction_segment = -1;
	int first_obstruction_side = -1;
	route_edge_decision first_obstruction;
};

enum class route_target_kind {
	key,
	reactor,
	boss,
	exit
};

struct route_target {
	route_target_kind kind = route_target_kind::key;
	route_key_requirement key = route_key_requirement::none;
	int segment = -1;
	int side = -1;
	int object = -1;
	bool contained = false;
	route_position position;
};

struct route_target_inventory {
	std::array<std::vector<route_target>, 3> keys;
	bool reactor_found = false;
	route_target reactor;
	bool boss_found = false;
	route_target boss;
	std::vector<route_target> exits;
};

struct route_target_selection {
	bool found = false;
	int selected_index = -1;
	double distance = 0.0;
	int progress_weight = 0;
	route_path_result path;
};

struct route_trigger_source {
	int target_segment = -1;
	int target_side = -1;
	int target_wall = -1;
	int source_wall = -1;
	int source_segment = -1;
	int source_side = -1;
	int trigger = -1;
	route_trigger_kind trigger_kind = route_trigger_kind::other;
	route_position source_position;
};

struct route_visibility_query {
	void *user = nullptr;
	bool (*target_visible)(
	    void *user,
	    int segment,
	    const route_position &from,
	    int target_segment,
	    const route_position &target) = nullptr;
	bool (*wall_visible)(
	    void *user,
	    int segment,
	    const route_position &from,
	    int wall) = nullptr;
};

struct route_trigger_path_selection {
	bool found = false;
	route_trigger_source source;
	route_path_result path;
	int terminal_segment = -1;
	route_position terminal_position;
};

enum class route_semantic_step_kind {
	start = LEVEL_METADATA_ROUTE_START,
	key = LEVEL_METADATA_ROUTE_KEY,
	trigger = LEVEL_METADATA_ROUTE_TRIGGER,
	reactor = LEVEL_METADATA_ROUTE_REACTOR,
	boss = LEVEL_METADATA_ROUTE_BOSS,
	exit = LEVEL_METADATA_ROUTE_EXIT,
	hidden_door = LEVEL_METADATA_ROUTE_HIDDEN_DOOR,
	unexplored = LEVEL_METADATA_ROUTE_UNEXPLORED,
	blastable_wall = LEVEL_METADATA_ROUTE_BLASTABLE_WALL
};

enum class route_activation_kind {
	none = LEVEL_METADATA_ROUTE_ACTIVATION_NONE,
	pickup_key = LEVEL_METADATA_ROUTE_ACTIVATION_PICKUP_KEY,
	shoot_switch = LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH,
	fly_through_trigger = LEVEL_METADATA_ROUTE_ACTIVATION_FLY_THROUGH_TRIGGER,
	pass_through_trigger = LEVEL_METADATA_ROUTE_ACTIVATION_PASS_THROUGH_TRIGGER,
	open_hidden_door = LEVEL_METADATA_ROUTE_ACTIVATION_OPEN_HIDDEN_DOOR,
	destroy_reactor = LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_REACTOR,
	destroy_boss = LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BOSS,
	enter_exit = LEVEL_METADATA_ROUTE_ACTIVATION_ENTER_EXIT,
	destroy_blastable_wall =
	    LEVEL_METADATA_ROUTE_ACTIVATION_DESTROY_BLASTABLE_WALL
};

struct route_effect_link {
	int segment = -1;
	int side = -1;
	int wall = -1;
};

struct route_semantic_step {
	route_semantic_step_kind kind = route_semantic_step_kind::start;
	int segment = -1;
	int side = -1;
	int wall = -1;
	int trigger = -1;
	int trigger_raw_type = -1;
	route_key_requirement key = route_key_requirement::none;
	route_activation_kind activation = route_activation_kind::none;
	route_position activation_position;
	route_position aim_position;
	route_position label_position;
	double distance_from_previous = 0.0;
	std::string label;
	std::string trigger_type_name;
	std::vector<route_effect_link> opened_links;
	route_path_result path;
};

struct route_dependency_result {
	bool attempted = false;
	bool resolved = false;
	route_progress_state progress;
	std::vector<route_semantic_step> steps;
	std::string problem;
	int failed_trigger = -1;
	int failed_key = -1;
	double pending_distance = 0.0;
	route_path_result pending_path;
};

enum class route_plan_status {
	ok = 0,
	partial = 1,
	failed = 2
};

struct route_plan_result {
	route_plan_status status = route_plan_status::failed;
	route_progress_state progress;
	std::vector<route_semantic_step> steps;
	std::string problem;
	std::string note;
	double travel_distance = 0.0;
	int partial_frontier_segment = -1;
	int unexplored_component_size = 0;
	int unexplored_target_segment = -1;
	int unexplored_waypoint_segment = -1;
	bool unexplored_direct_reachable = false;
};

route_search_result search_routes(
    const route_snapshot &snapshot,
    const route_query &query,
    bool optimistic);

route_search_result search_routes(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    bool optimistic);

route_search_result search_routes(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    const route_search_options &options);

route_progress_state initial_route_progress_state(
    const route_snapshot &snapshot,
    const route_query &query);

bool route_progress_acquire_key(
    route_progress_state &progress,
    route_key_requirement key);
bool route_progress_fire_trigger(route_progress_state &progress, int trigger);
bool route_progress_open_hidden_wall(
    const route_snapshot &snapshot,
    route_progress_state &progress,
    int wall);

bool route_progress_destroy_blastable_wall(
    const route_snapshot &snapshot,
    route_progress_state &progress,
    int wall);

route_path_result build_route_path(
    const route_search_result &search,
    int target_segment);

route_target_inventory discover_route_targets(const route_snapshot &snapshot);

std::vector<route_trigger_source> discover_trigger_sources(
    const route_snapshot &snapshot,
    const route_progress_state &progress,
    int segment,
    int side);

route_trigger_path_selection select_trigger_firing_path(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    const std::vector<route_trigger_source> &sources,
    const route_visibility_query &visibility = {});

route_dependency_result resolve_trigger_dependency(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    int segment,
    int side,
    const route_visibility_query &visibility = {});

route_plan_result plan_route(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_visibility_query &visibility = {});

route_target_selection select_route_target(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    const std::vector<route_target> &targets);

route_target_selection select_key_target(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    route_key_requirement key,
    const std::vector<route_target> &targets);

} // namespace dxx_route

#endif
