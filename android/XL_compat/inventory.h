#ifndef DXX_XL_COMPAT_INVENTORY_H
#define DXX_XL_COMPAT_INVENTORY_H

#include <array>
#include <cstddef>
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

// Pinned XL sideVertIndex, also the classic Redux side order
inline constexpr std::array<std::array<int, 4>, 6> classic_side_corners{ { { 7, 6, 2, 3 }, { 0, 4, 7, 3 }, { 0, 1, 5, 4 }, { 2, 6, 5, 1 }, { 4, 5, 6, 7 }, { 3, 2, 1, 0 } } };

struct Side {
	int wall = -1;
	int texture = -1;
	int overlay = -1;
	std::array<int, 12> uvl{};
	// Serialized local vertex slots for v25+, with trailing 255 for absent corners
	std::array<int, 4> corners{};
};

struct Segment {
	int owner, group;
	std::array<int, 6> children;
	std::array<int, 8> vertices;
	std::array<Side, 6> sides;
	int function, producer, value, flags, light;
	int properties = 0;
	std::array<int, 2> damage{};
};

enum SegmentProperty {
	segment_water = 1,
	segment_lava = 2,
	segment_blocked = 4,
	segment_no_damage = 8,
	segment_outdoors = 16
};

// XL v21 separates properties from function; legacy combined IDs must not leak
// into modern team functions. This decodes semantics, not runtime admission
inline int segment_property_bits(int level_version, const Segment &segment)
{
	if (level_version >= 21) return segment.properties;
	switch (segment.function) {
		case 7: return segment_water;
		case 8: return segment_lava;
		case 12:
		case 14: return segment_blocked;
		case 13: return segment_no_damage;
		case 16: return segment_outdoors;
		default: return 0;
	}
}

struct Color {
	int index;
	std::array<double, 3> rgb;
};

struct Trigger {
	int type, flags, value, time;
	std::vector<std::array<int, 2>> targets;
};

struct ObjectTrigger {
	Trigger action;
	int object = -1;
	// Retained legacy fields; XL rebuilds owner groups instead of following links
	int previous = -1, next = -1;
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
	int reader_id = -1, reader_render = -1;
	int multiplayer = 0;
	std::size_t source_offset = 0, source_end = 0;
	std::array<int, 3> position, last_position;
	std::array<int, 9> orientation;
	int size, shields, contains_type, contains_id, contains_count;
	std::vector<int> movement_data, control_data, render_data;
};

struct Inventory {
	int level_version = 0, game_version = 0;
	int mine_offset = 0, game_offset = 0;
	int flicker_offset = 0;
	int object_triggers = 0;
	std::vector<int> object_trigger_heads;
	std::vector<ObjectTrigger> object_trigger_records;
	std::vector<std::array<int, 2>> object_trigger_sparse_heads;
	std::string palette, level_name;
	std::vector<std::string> model_names;
	int countdown = 0, reactor_strength = 0, secret_return_segment = 0;
	std::array<int, 9> secret_return_orientation{};
	std::vector<std::array<int, 5>> flicker_lights;
	std::vector<std::array<int, 2>> reactor_targets;
	std::vector<std::array<int, 6>> matcens;
	std::vector<std::array<int, 6>> equipment;
	std::vector<std::array<int, 4>> light_indices;
	std::vector<std::array<int, 7>> delta_lights;
	// v27 stores four RGB byte triples and a distance byte in units of 20
	std::vector<std::array<int, 4>> fog_settings;
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

// Audited modern record layouts; individual geometry, feature and asset gates remain
inline bool modern_runtime_layout(const Inventory &level)
{
	return level.game_version == 40 && (level.level_version == 23 || level.level_version == 25 || level.level_version == 27);
}

// Inspection is not admission: even a decoded file is not approved for gameplay
// Unsupported version combinations and malformed data throw std::runtime_error
Inventory inspect_level(const Bytes &bytes);
// ReadColor registration predicate used by method-0 ambient reconstruction
bool has_registered_colors(const Inventory &inventory);
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
