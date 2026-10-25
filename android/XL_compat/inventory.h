#ifndef DXX_XL_COMPAT_INVENTORY_H
#define DXX_XL_COMPAT_INVENTORY_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif

namespace xl_compat
{

using Bytes = std::vector<std::uint8_t>;

struct Issue {
	std::string feature;
	int record;
	std::string detail;
};

struct Side {
	int wall = -1;
	int texture = -1;
	int overlay = -1;
	std::array<int, 12> uvl{};
};

struct Segment {
	int owner, group;
	std::array<int, 6> children;
	std::array<int, 8> vertices;
	std::array<Side, 6> sides;
	int function, producer, value, flags, light;
};

struct Color {
	int index;
	std::array<double, 3> rgb;
};

struct Trigger {
	int type, flags, value, time;
	std::vector<std::array<int, 2>> targets;
};

struct Wall {
	int segment, side, hitpoints, linked_wall;
	int type, flags, state, trigger, clip, keys, controlling_trigger, cloak;
};

struct Section {
	std::string name;
	int offset, count, declared_size;
};

struct Object {
	int type, id, control, movement, render, flags, segment;
	std::array<int, 3> position, last_position;
	std::array<int, 9> orientation;
	int size, shields, contains_type, contains_id, contains_count;
	std::vector<int> movement_data, control_data, render_data;
};

struct Inventory {
	int level_version = 0, game_version = 0;
	int mine_offset = 0, game_offset = 0;
	int object_triggers = 0;
	std::vector<int> object_trigger_heads;
	std::string palette, level_name;
	std::vector<std::string> model_names;
	int countdown = 0, reactor_strength = 0, secret_return_segment = 0;
	std::array<int, 9> secret_return_orientation{};
	std::vector<std::array<int, 5>> flicker_lights;
	std::vector<std::array<int, 2>> reactor_targets;
	std::vector<std::array<int, 6>> matcens;
	std::vector<std::array<int, 4>> light_indices;
	std::vector<std::array<int, 7>> delta_lights;
	std::vector<std::array<int, 3>> vertices;
	std::vector<Segment> segments;
	std::vector<Color> vertex_colors, side_colors, texture_colors;
	std::vector<Wall> walls;
	std::vector<Object> objects;
	std::vector<Trigger> triggers;
	std::vector<Section> sections;
	std::vector<Issue> unsupported, unknown;
	// Audited preserved data which is neither an unknown feature nor an admission grant
	std::vector<Issue> observations;
};

// Inspection is not admission: even a decoded file is not approved for gameplay
// Unsupported version combinations and malformed data throw std::runtime_error
Inventory inspect_level(const Bytes &bytes);
std::string inventory_json(const Inventory &inventory);
std::string json_string(const std::string &value);

struct Member {
	std::string name;
	Bytes bytes;
};
std::vector<Member> read_hog(const Bytes &bytes);

} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
