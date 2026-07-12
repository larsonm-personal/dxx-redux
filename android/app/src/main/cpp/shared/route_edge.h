#ifndef DXX_ROUTE_EDGE_H
#define DXX_ROUTE_EDGE_H

#include "route_snapshot.h"

namespace dxx_route
{

enum class route_edge_blocker {
	none,
	invalid_topology,
	exit,
	hidden_door,
	hard_blocked,
	locked_door,
	missing_key,
	trigger,
	closed_wall
};

enum class route_required_action {
	none,
	open_hidden_door,
	activate_trigger,
	acquire_key,
	destroy_blastable_wall,
	wait_for_player
};

struct route_edge_decision {
	int legacy_cost = LEVEL_METADATA_ROUTE_EDGE_BLOCKED;
	route_edge_blocker blocker = route_edge_blocker::invalid_topology;
	route_required_action action = route_required_action::none;
	int wall = -1;
	int trigger = -1;
	route_key_requirement key = route_key_requirement::none;
};

struct route_progress_state {
	int current_segment = -1;
	route_position current_position;
	int key_mask = 0;
	int key_in_progress = 0;
	int avoided_key_mask = 0;
	bool control_center_destroyed = false;
	std::vector<unsigned char> fired_triggers;
	std::vector<unsigned char> avoided_triggers;
	std::vector<unsigned char> opened_hidden_walls;
};

route_edge_decision evaluate_route_edge(
    const route_snapshot &snapshot,
    const route_query &query,
    int segment,
    int side);

route_edge_decision evaluate_route_edge(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    int segment,
    int side);

route_edge_decision evaluate_route_edge(
    const route_snapshot &snapshot,
    const route_query &query,
    const route_progress_state &progress,
    route_key_requirement forbidden_missing_key,
    int segment,
    int side);

} // namespace dxx_route

#endif
