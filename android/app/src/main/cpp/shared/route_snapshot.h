#ifndef DXX_ROUTE_SNAPSHOT_H
#define DXX_ROUTE_SNAPSHOT_H

#include "level_metadata_scan.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dxx_route
{

struct route_position {
	std::array<int, 3> value = {};
	bool valid = false;
};

struct route_topology_side {
	int child = -1;
	int reverse_side = -1;
	int wall = -1;
};

struct route_topology_segment {
	route_position center;
	std::array<route_position, 8> vertices;
	std::array<route_topology_side, LEVEL_METADATA_MAX_SIDES> sides;
};

struct route_topology_wall {
	int segment = -1;
	int side = -1;
};

struct route_topology {
	std::vector<route_topology_segment> segments;
	std::vector<route_topology_wall> walls;
	std::uint64_t hash = 0;
};

struct route_state_side {
	bool flyable = false;
	bool hard_blocked = false;
	bool control_center_link = false;
	bool exit_trigger = false;
};

struct route_state_segment {
	bool explored = false;
	std::array<route_state_side, LEVEL_METADATA_MAX_SIDES> sides;
};

struct route_state_wall {
	int type = -1;
	int flags = 0;
	int keys = -1;
	int clip_flags = 0;
	int trigger = -1;
};

struct route_state {
	int start_segment = -1;
	route_position start_position;
	int key_mask = 0;
	bool control_center_destroyed = false;
	std::vector<route_state_segment> segments;
	std::vector<route_state_wall> walls;
	std::uint64_t hash = 0;
};

struct route_snapshot {
	route_topology topology;
	route_state state;
};

bool build_route_snapshot(const level_metadata_scan_view &view,
                          route_snapshot &snapshot,
                          std::string *problem = nullptr);

} // namespace dxx_route

#endif
