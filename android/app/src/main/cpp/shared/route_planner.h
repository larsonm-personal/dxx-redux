#ifndef DXX_ROUTE_PLANNER_H
#define DXX_ROUTE_PLANNER_H

#include "route_edge.h"

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
	std::string problem;
};

struct route_path_result {
	bool reached = false;
	double distance = 0.0;
	int progress_weight = 0;
	std::vector<int> segments;
	std::vector<int> sides;
	bool has_obstruction = false;
	route_edge_decision first_obstruction;
};

route_search_result search_routes(
    const route_snapshot &snapshot,
    const route_query &query,
    bool optimistic);

route_path_result build_route_path(
    const route_search_result &search,
    int target_segment);

} // namespace dxx_route

#endif
