#ifndef DXX_ROUTE_SNAPSHOT_H
#define DXX_ROUTE_SNAPSHOT_H

#include "level_metadata_scan.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dxx_route
{

enum class route_trigger_kind {
	other,
	open_door,
	exit,
	secret_exit,
	illusion_off,
	unlock_door,
	open_wall,
	illusory_wall
};

enum class route_wall_kind {
	none,
	blastable,
	door,
	illusion,
	open,
	other
};

enum class route_key_requirement {
	none,
	blue,
	red,
	gold,
	unknown
};

enum class route_endpoint_kind {
	end_of_level,
	unexplored,
	segment
};

enum class route_object_kind {
	other,
	robot,
	powerup,
	control_center
};

struct route_position {
	std::array<int, 3> value = {};
	bool valid = false;
};

struct route_progression_profile {
	int key_mask = 0;
	bool can_open_ordinary_doors = true;
	bool can_activate_triggers = true;
	bool can_destroy_blastable_walls = true;
	bool can_destroy_reactor = true;
	bool can_destroy_boss = true;
};

struct route_navigator_profile {
	int radius = 0;
	bool companion = false;
	bool respects_buddy_proof_walls = false;
	bool can_fire_flare = false;
	bool can_cross_triggers = true;
};

struct route_query {
	route_endpoint_kind endpoint = route_endpoint_kind::end_of_level;
	int target_segment = -1;
	route_position start;
	route_progression_profile progression;
	route_navigator_profile navigator;
	bool allow_partial = true;
};

struct route_topology_side {
	int child = -1;
	int reverse_side = -1;
	int wall = -1;
	int clearance_radius = 0;
	route_position center;
	std::vector<int> opener_walls;
};

struct route_topology_segment {
	route_position center;
	bool control_center = false;
	std::array<route_position, 8> vertices;
	std::array<route_topology_side, LEVEL_METADATA_MAX_SIDES> sides;
};

struct route_topology_wall {
	int segment = -1;
	int side = -1;
	route_position target;
	bool shootable_trigger = false;
};

struct route_topology_trigger_link {
	int segment = -1;
	int side = -1;
};

struct route_topology_trigger {
	int raw_type = -1;
	route_trigger_kind kind = route_trigger_kind::other;
	std::vector<route_topology_trigger_link> links;
};

struct route_topology {
	std::vector<route_topology_segment> segments;
	std::vector<route_topology_wall> walls;
	std::vector<route_topology_trigger> triggers;
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
	route_wall_kind kind = route_wall_kind::none;
	int flags = 0;
	int keys = -1;
	route_key_requirement key = route_key_requirement::none;
	int clip_flags = 0;
	int trigger = -1;
	bool locked = false;
	bool opened = false;
	bool hidden = false;
	bool buddy_proof = false;
};

struct route_state_trigger {
	int flags = 0;
	bool disabled = false;
};

struct route_state_object {
	int segment = -1;
	int type = -1;
	int id = -1;
	int flags = 0;
	int contains_type = -1;
	int contains_id = -1;
	int contains_count = 0;
	route_object_kind kind = route_object_kind::other;
	route_key_requirement key = route_key_requirement::none;
	route_key_requirement contains_key = route_key_requirement::none;
	route_position position;
	bool should_be_dead = false;
	bool boss = false;
	bool companion = false;
	bool fleeing = false;
};

struct route_state_fingerprints {
	std::uint64_t start = 0;
	std::uint64_t progression = 0;
	std::uint64_t navigation = 0;
	std::uint64_t triggers = 0;
	std::uint64_t objects = 0;
	std::uint64_t automap = 0;
};

struct route_state {
	int start_segment = -1;
	route_position start_position;
	int key_mask = 0;
	bool control_center_destroyed = false;
	std::vector<route_state_segment> segments;
	std::vector<route_state_wall> walls;
	std::vector<route_state_trigger> triggers;
	std::vector<route_state_object> objects;
	route_state_fingerprints fingerprints;
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
