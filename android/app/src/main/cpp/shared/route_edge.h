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

route_edge_decision evaluate_route_edge(
    const route_snapshot &snapshot,
    const route_query &query,
    int segment,
    int side);

} // namespace dxx_route

#endif
