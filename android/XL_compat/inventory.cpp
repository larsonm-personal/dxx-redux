#include "inventory.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <set>
#include <stdexcept>

namespace xl_compat
{
namespace
{

// Layout reference: D2X-XL 11038eb53dd86f3bcdc027dddd3f93c60f02536e
// gameio/loadgeometry.cpp, gameio/loadobjects.cpp, main/segment.cpp, main/side.cpp
// 17/36 checked against arbruijn/d2x-xl@2576a98b5de310e3ac7c9777df2677d1627b8bae
// and the hash-pinned original Observatory archive
// Bounded inspection pairs, not arbitrary XL or gameplay admission
class Reader
{
	const Bytes &data;
	std::size_t cursor, end;

  public:
	Reader(const Bytes &bytes, std::size_t start, std::size_t limit) : data(bytes), cursor(start), end(limit)
	{
		if (start > limit || limit > bytes.size()) fail("invalid section bounds");
	}
	[[noreturn]] void fail(const std::string &why) const
	{
		throw std::runtime_error("offset " + std::to_string(cursor) + ": " + why);
	}
	std::size_t position() const
	{
		return cursor;
	}
	void skip(std::size_t count)
	{
		if (count > end - cursor) fail("truncated section");
		cursor += count;
	}
	std::uint64_t unsigned_value(unsigned count)
	{
		const auto start = cursor;
		skip(count);
		std::uint64_t value = 0;
		for (unsigned i = 0; i < count; ++i) value |= std::uint64_t(data[start + i]) << (8 * i);
		return value;
	}
	int u8()
	{
		return static_cast<int>(unsigned_value(1));
	}
	int s8()
	{
		const int value = u8();
		return value < 128 ? value : value - 256;
	}
	int u16()
	{
		return static_cast<int>(unsigned_value(2));
	}
	int s16()
	{
		const int value = u16();
		return value < 32768 ? value : value - 65536;
	}
	int s32()
	{
		const auto value = unsigned_value(4);
		return static_cast<int>(value <= 0x7fffffff ? static_cast<std::int64_t>(value) : static_cast<std::int64_t>(value) - 0x100000000LL);
	}
	double f64()
	{
		const auto bits = unsigned_value(8);
		double value;
		static_assert(sizeof(value) == sizeof(bits), "64-bit double required");
		std::memcpy(&value, &bits, sizeof(value));
		if (!std::isfinite(value)) fail("non-finite color");
		return value;
	}
	std::string line(std::size_t maximum)
	{
		std::string value;
		for (std::size_t i = 0; i < maximum; ++i) {
			const auto c = u8();
			if (c == 0 || c == '\n') return value;
			value += static_cast<char>(c);
		}
		fail("unterminated string");
	}
	std::string fixed_string(std::size_t size)
	{
		std::string value;
		bool ended = false;
		for (std::size_t i = 0; i < size; ++i) {
			const auto c = u8();
			if (!c) ended = true;
			if (!ended) value += static_cast<char>(c);
		}
		return value;
	}
};

void require(bool condition, const std::string &message)
{
	if (!condition) throw std::runtime_error(message);
}

void issue(std::vector<Issue> &list, const std::string &feature, int record, const std::string &detail)
{
	list.push_back({ feature, record, detail });
}

void read_colors(Reader &r, std::vector<Color> &colors, std::size_t count, bool floating)
{
	colors.reserve(count);
	for (std::size_t i = 0; i < count; ++i) {
		Color c;
		c.index = r.u8();
		for (auto &component : c.rgb) component = floating ? r.f64() : double(r.s32()) / 2147483647.0;
		colors.push_back(c);
	}
}

const Section &section(const Inventory &out, const char *name)
{
	for (const auto &s : out.sections)
		if (s.name == name) return s;
	throw std::runtime_error(std::string("missing section ") + name);
}

std::size_t section_end(const Inventory &out, const Section &s, std::size_t file_size)
{
	auto end = file_size;
	for (const auto &next : out.sections)
		if (next.offset > s.offset) end = std::min(end, static_cast<std::size_t>(next.offset));
	return end;
}

Reader section_reader(const Bytes &bytes, const Inventory &out, const Section &s, std::size_t record_size)
{
	require(s.offset >= out.game_offset, s.name + ": missing or invalid offset");
	const auto end = section_end(out, s, bytes.size());
	require(static_cast<std::size_t>(s.offset) <= end, s.name + ": invalid section extent");
	require(static_cast<std::size_t>(s.count) <= (end - s.offset) / record_size, s.name + ": count exceeds section extent");
	return Reader(bytes, s.offset, end);
}

template <class T, std::size_t N>
void array_json(std::ostream &s, const std::array<T, N> &values)
{
	s << '[';
	for (std::size_t i = 0; i < N; ++i) {
		if (i) s << ", ";
		s << values[i];
	}
	s << ']';
}

template <std::size_t N>
void rows_json(std::ostream &s, const char *name, const std::vector<std::array<int, N>> &rows)
{
	s << ",\n  " << json_string(name) << ": [";
	for (std::size_t i = 0; i < rows.size(); ++i) {
		if (i) s << ',';
		s << "\n    ";
		array_json(s, rows[i]);
	}
	s << (rows.empty() ? "]" : "\n  ]");
}

void issues_json(std::ostream &s, const std::vector<Issue> &issues)
{
	s << '[';
	for (std::size_t i = 0; i < issues.size(); ++i) {
		if (i) s << ',';
		const auto &v = issues[i];
		s << "\n    {\"feature\": " << json_string(v.feature) << ", \"record\": " << v.record
		  << ", \"detail\": " << json_string(v.detail) << '}';
	}
	s << (issues.empty() ? "]" : "\n  ]");
}

Trigger read_trigger(Reader &r, Inventory &out, int index, int segment_count, bool attached)
{
	Trigger t{};
	t.type = r.u8();
	t.flags = attached ? r.u16() : r.u8();
	const int count = r.u8();
	r.skip(1);
	require(count <= 10, "invalid trigger target count " + std::to_string(index));
	t.value = r.s32();
	t.time = r.s32();
	std::array<int, 10> segments{}, sides{};
	for (auto &v : segments) v = r.s16();
	for (auto &v : sides) v = r.s16();
	const std::string prefix = attached ? "object_" : "";
	for (int j = 0; j < count; ++j) {
		// Object teleports use only the segment; their serialized side is inert
		if (attached && t.type == 14) require(segments[j] >= 0 && segments[j] < segment_count, "invalid object action destination " + std::to_string(index));
		else if (attached && t.type == 26 && j == 0) require(segments[j] < segment_count, "invalid spawn-bot selection destination " + std::to_string(index));
		else if (t.type <= 14 || t.type == 31) require(segments[j] >= 0 && segments[j] < segment_count && sides[j] >= 0 && sides[j] < 6, "invalid trigger target " + std::to_string(index));
		if (t.type == 32 || t.type == 33) {
			if (sides[j] < 0) issue(out.unsupported, prefix + "effect_trigger_target", index, "enable/disable effect-object targets require effect runtime support");
			else require(segments[j] >= 0 && segments[j] < segment_count && sides[j] < 6, "invalid trigger target " + std::to_string(index));
		}
		t.targets.push_back({ segments[j], sides[j] });
	}
	if (t.type > 13) issue(out.unsupported, prefix + "trigger_type", index, std::to_string(t.type));
	if (t.flags & ~159) issue(out.unsupported, prefix + "trigger_flags", index, std::to_string(t.flags));
	return t;
}
} // namespace

Inventory inspect_level(const Bytes &bytes)
{
	Inventory out;
	Reader header(bytes, 0, bytes.size());
	require(header.s32() == 0x504c564c, "not a PLVL level");
	out.level_version = header.s32();
	require(out.level_version == 15 || out.level_version == 17 || out.level_version == 23, "unsupported level version " + std::to_string(out.level_version));
	out.mine_offset = header.s32();
	out.game_offset = header.s32();
	require(out.mine_offset >= 16 && out.game_offset > out.mine_offset && static_cast<std::size_t>(out.game_offset) < bytes.size(), "invalid mine/game offsets");

	Reader game(bytes, out.game_offset, bytes.size());
	require(game.u16() == 0x6705, "invalid game data signature");
	out.game_version = game.u16();
	require((out.level_version == 15 && out.game_version == 33) || (out.level_version == 17 && out.game_version == 36) || (out.level_version == 23 && out.game_version == 40),
	        "unsupported level/game version pair " + std::to_string(out.level_version) + "/" + std::to_string(out.game_version));
	game.skip(31);
	const char *section_names[] = { "objects", "walls", "doors", "triggers", "links", "reactor", "matcens", "light_indices", "delta_lights", "equipment" };
	const unsigned section_count = out.level_version >= 17 ? 10 : 9;
	for (unsigned i = 0; i < section_count; ++i) {
		const auto *name = section_names[i];
		Section s{ name, game.s32(), game.s32(), game.s32() };
		require(s.count >= 0 && s.count <= 1000000, s.name + ": invalid count");
		require(s.offset >= -1 && (s.offset == -1 || static_cast<std::size_t>(s.offset) <= bytes.size()), s.name + ": invalid offset");
		require(!s.count || (s.offset >= out.game_offset && static_cast<std::size_t>(s.offset) < bytes.size()), s.name + ": missing data");
		out.sections.push_back(s);
	}
	out.level_name = game.line(256);
	const int model_count = game.u16();
	if (model_count == 0x614d || model_count == 0x5547 || model_count == 0x5053) {
		issue(out.unknown, "model_name_marker", -1, std::to_string(model_count));
	} else {
		require(model_count <= 1000, "model name count exceeds inspection limit");
		for (int i = 0; i < model_count; ++i) out.model_names.push_back(game.fixed_string(13));
	}
	for (const auto &s : out.sections) {
		if (s.count) require(static_cast<std::size_t>(s.offset) >= game.position(), s.name + ": overlaps game header");
	}

	Reader extra(bytes, 16, out.mine_offset);
	extra.skip(7);
	out.palette = extra.line(256);
	out.countdown = extra.s32();
	out.reactor_strength = extra.s32();
	const int flickers = extra.s32();
	require(flickers >= 0 && flickers <= 100000, "invalid flicker count");
	for (int i = 0; i < flickers; ++i) out.flicker_lights.push_back({ extra.s16(), extra.s16(), extra.s32(), extra.s32(), extra.s32() });
	out.secret_return_segment = extra.s32();
	for (auto &v : out.secret_return_orientation) v = extra.s32();
	if (extra.position() != static_cast<std::size_t>(out.mine_offset)) issue(out.unknown, "header_trailing_bytes", -1, std::to_string(out.mine_offset - extra.position()));

	Reader mine(bytes, out.mine_offset, out.game_offset);
	const int compiled_version = mine.u8();
	if (compiled_version != 0) issue(out.unknown, "compiled_mine_version", -1, std::to_string(compiled_version));
	const int vertex_count = mine.s16(), segment_count = mine.s16();
	require(vertex_count > 0 && segment_count > 0, "invalid geometry counts");
	require(out.secret_return_segment >= 0 && out.secret_return_segment < segment_count, "invalid secret return segment");
	for (const auto &f : out.flicker_lights) require(f[0] >= 0 && f[0] < segment_count && f[1] >= 0 && f[1] < 6, "invalid flicker target");
	for (int i = 0; i < vertex_count; ++i) out.vertices.push_back({ mine.s32(), mine.s32(), mine.s32() });
	for (int i = 0; i < segment_count; ++i) {
		Segment seg{};
		seg.owner = mine.s8();
		seg.group = mine.s8();
		const int flags = mine.u8();
		// Bit 6 is the legacy special-data marker, ignored for level versions > 5
		require((flags & 0x80) == 0, "unknown segment child flags at " + std::to_string(i));
		for (int side = 0; side < 6; ++side) {
			seg.children[side] = flags & (1 << side) ? mine.s16() : -1;
			require(seg.children[side] >= -2 && seg.children[side] < segment_count, "invalid child at segment " + std::to_string(i));
		}
		for (auto &v : seg.vertices) {
			v = mine.u16();
			require(v < vertex_count, "invalid vertex at segment " + std::to_string(i));
		}
		const int wall_flags = mine.u8();
		require((wall_flags & 0xc0) == 0, "unknown wall mask at segment " + std::to_string(i));
		for (int side = 0; side < 6; ++side) {
			auto &s = seg.sides[side];
			if (wall_flags & (1 << side)) {
				s.wall = mine.u16();
				if (s.wall == 65535) s.wall = -1;
				require(s.wall < section(out, "walls").count, "invalid wall reference at segment " + std::to_string(i));
				if (s.wall >= 255) issue(out.unsupported, "wall_reference", i, std::to_string(s.wall));
			}
		}
		for (int side = 0; side < 6; ++side) {
			auto &s = seg.sides[side];
			if (seg.children[side] != -1 && s.wall == -1) continue;
			const int texture = mine.u16();
			s.texture = texture & 0x7fff;
			s.overlay = texture & 0x8000 ? mine.u16() : 0;
			if (s.texture >= 910 || (s.overlay & 0x3fff) >= 910) issue(out.unknown, "texture_id", i, std::to_string(s.texture) + "/" + std::to_string(s.overlay));
			for (int v = 0; v < 4; ++v) {
				s.uvl[v * 3] = mine.s16();
				s.uvl[v * 3 + 1] = mine.s16();
				s.uvl[v * 3 + 2] = mine.u16();
			}
		}
		if (seg.owner != -1 || seg.group != -1) issue(out.unknown, "segment_owner_group", i, std::to_string(seg.owner) + "/" + std::to_string(seg.group));
		out.segments.push_back(seg);
	}
	for (int i = 0; i < segment_count; ++i) {
		auto &seg = out.segments[i];
		seg.function = mine.u8();
		seg.producer = mine.s8();
		seg.value = mine.s8();
		seg.flags = mine.u8();
		if (out.level_version == 23) {
			seg.properties = mine.u8();
			seg.damage = { mine.s16(), mine.s16() };
			if (seg.properties & ~(segment_water | segment_no_damage)) issue(out.unsupported, "segment_properties", i, std::to_string(seg.properties));
			if (seg.damage[0] || seg.damage[1]) issue(out.unsupported, "segment_damage", i, std::to_string(seg.damage[0]) + "/" + std::to_string(seg.damage[1]));
		}
		seg.light = mine.s32();
		if (seg.function == 2 || seg.function > 6) issue(out.unsupported, out.level_version == 23 ? "segment_function_v21" : "segment_function", i, std::to_string(seg.function));
		if (seg.flags & ~3) {
			// Pinned 17/36 reference consumes only ambient bits 0/1; retain all raw bits
			// See evidence/segment-flags-17-36.md; other profiles stay unaudited
			if (out.level_version == 17 && out.game_version == 36)
				issue(out.observations, "reserved_segment_flags", i, std::to_string(seg.flags));
			else issue(out.unknown, "segment_flags", i, std::to_string(seg.flags));
		}
	}
	read_colors(mine, out.vertex_colors, vertex_count, false);
	read_colors(mine, out.side_colors, static_cast<std::size_t>(segment_count) * 6, false);
	read_colors(mine, out.texture_colors, 910, out.level_version == 15);
	if (mine.position() != static_cast<std::size_t>(out.game_offset)) issue(out.unknown, "mine_trailing_bytes", -1, std::to_string(out.game_offset - mine.position()));

	const auto &walls = section(out, "walls");
	if (walls.count) {
		auto r = section_reader(bytes, out, walls, out.game_version == 40 ? 25 : 24);
		for (int i = 0; i < walls.count; ++i) {
			Wall w{ r.s32(), r.s32(), r.s32(), r.s32(), r.u8(), out.game_version == 40 ? r.u16() : r.u8(), r.u8(), r.u8(), r.s8(), r.u8(), r.s8(), r.u8() };
			require(w.segment >= 0 && w.segment < segment_count && w.side >= 0 && w.side < 6, "invalid wall location " + std::to_string(i));
			require(w.linked_wall >= -1 && w.linked_wall < walls.count, "invalid linked wall " + std::to_string(i));
			require(w.trigger == 255 || w.trigger < section(out, "triggers").count, "invalid wall trigger " + std::to_string(i));
			if (w.flags > 255) issue(out.unsupported, "wall_flags", i, std::to_string(w.flags));
			if (w.type > 7) issue(out.unsupported, "wall_type", i, std::to_string(w.type));
			if (w.flags & 4) issue(out.unsupported, "additive_wall_rendering", i, std::to_string(w.flags));
			if (w.state > 6) issue(out.unsupported, "wall_state", i, std::to_string(w.state));
			if (w.keys & ~15) issue(out.unsupported, "wall_keys", i, std::to_string(w.keys));
			out.walls.push_back(w);
		}
	}
	const auto &triggers = section(out, "triggers");
	if (triggers.offset >= 0) {
		auto r = section_reader(bytes, out, triggers, 52);
		for (int i = 0; i < triggers.count; ++i) out.triggers.push_back(read_trigger(r, out, i, segment_count, false));
		out.object_triggers = r.s32();
		require(out.object_triggers >= 0, "negative object trigger count");
		const auto end = section_end(out, triggers, bytes.size());
		const auto remaining = end - r.position();
		if (out.object_triggers) {
			const auto width = out.game_version >= 40 ? 55u : 59u;
			require(static_cast<std::size_t>(out.object_triggers) <= remaining / width, "object trigger count exceeds section extent");
			issue(out.unsupported, "object_triggers", -1, std::to_string(out.object_triggers) + " decoded records; runtime admission checks action and owner requirements");
			for (int i = 0; i < out.object_triggers; ++i) out.object_trigger_records.push_back({ read_trigger(r, out, i, segment_count, true) });
			for (int i = 0; i < out.object_triggers; ++i) {
				auto &record = out.object_trigger_records[i];
				if (out.game_version < 40) {
					record.previous = r.s16();
					record.next = r.s16();
				}
				record.object = r.s16();
				require(record.object >= -1 && record.object < section(out, "objects").count, "invalid object trigger owner " + std::to_string(i));
			}
			if (out.game_version < 36) {
				for (int i = 0; i < 700; ++i) out.object_trigger_heads.push_back(r.s16());
			} else if (out.game_version < 40) {
				const int count = r.s16();
				require(count >= 0 && static_cast<std::size_t>(count) <= (end - r.position()) / 4, "invalid object trigger sparse reference count");
				for (int i = 0; i < count; ++i) out.object_trigger_sparse_heads.push_back({ r.s16(), r.s16() });
			}
			require(r.position() == end, "unexpected object trigger section tail");
		} else if (out.game_version == 36) {
			// Observed writers retain an empty sparse count; the reference permits omission
			if (remaining) require(r.u16() == 0, "nonempty game-36 object reference table without triggers");
			require(r.position() == end, "unexpected game-36 object reference tail");
		} else if (out.game_version == 40) {
			require(remaining == 0, "unexpected game-40 object reference tail");
		} else if (remaining == 1400) {
			// Neutral legacy padding is accepted independently of the reference read path
			for (int i = 0; i < 700; ++i) {
				const auto head = r.s16();
				out.object_trigger_heads.push_back(head);
				if (head != -1) issue(out.unknown, "object_trigger_head", i, "nonempty head without object triggers");
			}
		} else if (remaining) issue(out.unknown, "object_trigger_tail", -1, std::to_string(remaining) + " bytes require reference interpretation");
	} else {
		issue(out.unknown, "object_triggers", -1, "no trigger section to inspect");
	}
	const auto &objects = section(out, "objects");
	if (objects.count) {
		auto r = section_reader(bytes, out, objects, out.game_version == 40 ? 80 : 79);
		std::array<int, 21> effect_types{};
		std::array<int, 14> effect_renders{};
		int static_objects = 0, waypoints = 0;
		for (int i = 0; i < objects.count; ++i) {
			Object o{};
			o.source_offset = r.position();
			o.type = r.u8();
			o.id = r.u8();
			o.control = r.u8();
			o.movement = r.u8();
			o.render = r.u8();
			o.reader_id = o.id;
			o.reader_render = o.render;
			o.flags = r.u8();
			if (out.game_version == 40) {
				o.multiplayer = r.u8();
				if (o.multiplayer) issue(out.unsupported, "multiplayer_only_object", i, std::to_string(o.multiplayer));
			}
			o.segment = r.s16();
			require(o.segment >= 0 && o.segment < segment_count, "invalid object segment " + std::to_string(i));
			for (auto &v : o.position) v = r.s32();
			for (auto &v : o.orientation) v = r.s32();
			o.size = r.s32();
			o.shields = r.s32();
			for (auto &v : o.last_position) v = r.s32();
			o.contains_type = r.s8();
			o.contains_id = r.s8();
			o.contains_count = r.s8();
			if (o.type == 18 || o.type == 19 || o.type == 20) ++effect_types[o.type];
			else if (o.type > 15 && o.type != 255) issue(out.unsupported, "object_type", i, std::to_string(o.type));
			if (o.movement == 1) {
				for (int j = 0; j < 15; ++j) o.movement_data.push_back(r.s32());
				o.movement_data.push_back(r.s16());
				o.movement_data.push_back(r.u16());
			} else if (o.movement == 3) {
				for (int j = 0; j < 3; ++j) o.movement_data.push_back(r.s32());
			} else if (o.movement == 2) {
				// MT_STATIC is defined, but CObject::Read warns and consumes no payload
				++static_objects;
			} else if (o.movement != 0) r.fail("unsupported movement layout " + std::to_string(o.movement) + " at object " + std::to_string(i));
			switch (o.control) {
				case 1:
					for (int j = 0; j < 12; ++j) o.control_data.push_back(r.u8());
					for (int j = 0; j < 4; ++j) o.control_data.push_back(r.s16());
					break;
				case 2:
					o.control_data.push_back(r.s32());
					o.control_data.push_back(r.s32());
					o.control_data.push_back(r.s16());
					break;
				case 9:
					o.control_data.push_back(r.s16());
					o.control_data.push_back(r.s16());
					o.control_data.push_back(r.s32());
					break;
				case 13:
				case 14:
					o.control_data.push_back(r.s32());
					break;
				case 17:
					for (int j = 0; j < 3; ++j) o.control_data.push_back(r.s32());
					o.reader_id = 3; // CT_WAYPOINT normalizes identity before rendering data
					++waypoints;
					break;
				case 0:
				case 4:
				case 5:
				case 12:
				case 16: break;
				default: r.fail("unsupported control layout " + std::to_string(o.control) + " at object " + std::to_string(i));
			}
			switch (o.render) {
				case 1:
				case 6:
					o.render_data.push_back(r.s32());
					for (int j = 0; j < 30; ++j) o.render_data.push_back(r.s16());
					o.render_data.push_back(r.s32());
					o.render_data.push_back(r.s32());
					break;
				case 2:
				case 4:
				case 5:
				case 7:
					o.render_data.push_back(r.s32());
					o.render_data.push_back(r.s32());
					o.render_data.push_back(r.u8());
					break;
				case 0:
				case 3: break;
				case 8:
					++effect_renders[o.render];
					break;
				case 11:
					for (int j = 0; j < 6; ++j) o.render_data.push_back(r.s32());
					for (int j = 0; j < 5; ++j) o.render_data.push_back(r.u8());
					o.render_data.push_back(out.level_version < 18 ? 0 : r.u8());
					o.render_data.push_back(out.level_version < 19 ? 1 : r.u8());
					o.reader_id = 0;
					++effect_renders[o.render];
					break;
				case 12:
					for (int j = 0; j < 5; ++j) o.render_data.push_back(r.s32());
					o.render_data.push_back(out.level_version <= 22 ? -1 : r.s32());
					for (int j = 0; j < 6; ++j) o.render_data.push_back(r.s16());
					o.render_data.push_back(out.level_version <= 21 ? 3 : r.u8());
					for (int j = 0; j < 12; ++j) o.render_data.push_back(r.u8());
					o.render_data.push_back(out.level_version < 19 ? 1 : r.u8());
					if (o.reader_id != 1) o.reader_render = o.reader_id == 2 ? 13 : 11;
					++effect_renders[o.render];
					break;
				case 13:
					for (int j = 0; j < 40; ++j) o.render_data.push_back(r.u8());
					o.render_data.push_back(r.s32()); // Serialized volume, before float scaling
					o.render_data.push_back(out.level_version < 19 ? 1 : r.u8());
					o.reader_id = 2;
					++effect_renders[o.render];
					break;
				default: r.fail("unsupported render layout " + std::to_string(o.render) + " at object " + std::to_string(i));
			}
			o.source_end = r.position();
			out.objects.push_back(o);
		}
		if (r.position() != section_end(out, objects, bytes.size())) issue(out.unknown, "object_trailing_bytes", -1, std::to_string(section_end(out, objects, bytes.size()) - r.position()));
		for (int type : { 18, 19, 20 })
			if (effect_types[type]) issue(out.unsupported, "effect_object_types", -1, std::to_string(effect_types[type]) + " objects of type " + std::to_string(type) + "; runtime behavior unimplemented");
		for (const auto &entry : std::array<std::pair<int, const char *>, 4>{ { { 8, "effect_thrusters" }, { 11, "effect_particles" }, { 12, "effect_lightning" }, { 13, "effect_sounds" } } })
			if (effect_renders[entry.first]) issue(out.unsupported, entry.second, -1, std::to_string(effect_renders[entry.first]) + " decoded payloads; runtime behavior unimplemented");
		if (static_objects) issue(out.unsupported, "static_movement", -1, std::to_string(static_objects) + " static objects; reference reader warns, runtime behavior unimplemented");
		if (waypoints) issue(out.unsupported, "waypoint_controllers", -1, std::to_string(waypoints) + " decoded controllers; runtime behavior unimplemented");
	}
	const auto &reactor = section(out, "reactor");
	require(reactor.count <= 1, "multiple reactor trigger records");
	if (reactor.count) {
		auto r = section_reader(bytes, out, reactor, 42);
		const auto count = r.s16();
		require(count >= 0 && count <= 10, "invalid reactor trigger count");
		std::array<int, 10> segments{}, sides{};
		for (auto &v : segments) v = r.s16();
		for (auto &v : sides) v = r.s16();
		for (int i = 0; i < count; ++i) {
			require(segments[i] >= 0 && segments[i] < segment_count && sides[i] >= 0 && sides[i] < 6, "invalid reactor target");
			out.reactor_targets.push_back({ segments[i], sides[i] });
		}
	}
	const auto &matcens = section(out, "matcens");
	if (matcens.count) {
		auto r = section_reader(bytes, out, matcens, 20);
		for (int i = 0; i < matcens.count; ++i) out.matcens.push_back({ r.s32(), r.s32(), r.s32(), r.s32(), r.s16(), r.s16() });
	}
	const auto &indices = section(out, "light_indices"), &deltas = section(out, "delta_lights");
	if (out.level_version >= 17) {
		const auto &equipment = section(out, "equipment");
		if (equipment.count) {
			auto r = section_reader(bytes, out, equipment, 20);
			for (int i = 0; i < equipment.count; ++i) out.equipment.push_back({ r.s32(), r.s32(), r.s32(), r.s32(), r.s16(), r.s16() });
		}
	}
	if (indices.count) {
		auto r = section_reader(bytes, out, indices, 6);
		for (int i = 0; i < indices.count; ++i) {
			std::array<int, 4> values{ r.s16(), r.u8(), r.u8(), r.u16() };
			if (out.game_version >= 36) {
				// XL packs side into 3 bits and count into the remaining 13 bits
				values[2] = (values[2] << 5) | (values[1] >> 3);
				values[1] &= 7;
			}
			require(values[0] >= 0 && values[0] < segment_count && values[1] < 6 && values[3] <= deltas.count && values[2] <= deltas.count - values[3], "invalid light index " + std::to_string(i));
			out.light_indices.push_back(values);
		}
	}
	if (deltas.count) {
		auto r = section_reader(bytes, out, deltas, 8);
		for (int i = 0; i < deltas.count; ++i) {
			std::array<int, 7> values{ r.s16(), r.u8(), r.u8(), r.u8(), r.u8(), r.u8(), r.u8() };
			require(values[0] >= 0 && values[0] < segment_count && values[1] < 6, "invalid delta light " + std::to_string(i));
			out.delta_lights.push_back(values);
		}
	}
	for (const auto *name : { "doors", "links" })
		if (section(out, name).count) issue(out.unknown, name, -1, "nonempty legacy section");
	// Do not let successful structural decoding masquerade as a complete audit
	issue(out.unknown, "object_dependencies", -1, "robot, weapon, powerup and model IDs require asset-aware validation and behavior comparison");
	issue(out.unknown, "runtime_semantics", -1, "decoded records require reference comparison and runtime validation");
	issue(out.unknown, "assets", -1, "mission asset resolution and reference runtime not verified");
	issue(out.unsupported, "embedded_lighting", -1, "XL color tables decoded but no faithful rendering integration exists");
	return out;
}

std::string json_string(const std::string &value)
{
	std::ostringstream s;
	s << '"';
	for (const unsigned char c : value) {
		if (c == '"' || c == '\\') s << '\\' << c;
		else if (c < 32 || c >= 127) s << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
		else s << c;
	}
	s << '"';
	return s.str();
}

bool has_registered_colors(const Inventory &inventory)
{
	for (const auto *table : { &inventory.vertex_colors, &inventory.side_colors })
		for (const auto &color : *table)
			for (const auto channel : color.rgb) {
				const float loaded = static_cast<float>(channel);
				if (loaded > 0 && loaded < 1) return true;
			}
	return false;
}

std::string inventory_json(const Inventory &v)
{
	std::ostringstream s;
	s.imbue(std::locale::classic());
	s << std::setprecision(std::numeric_limits<double>::max_digits10);
	s << "{\n  \"schema\": 1,\n  \"status\": \"inspection_only\",\n  \"playable\": false,\n  \"level_version\": " << v.level_version
	  << ",\n  \"game_version\": " << v.game_version << ",\n  \"mine_offset\": " << v.mine_offset << ",\n  \"game_offset\": " << v.game_offset;
	s << ",\n  \"unsupported\": ";
	issues_json(s, v.unsupported);
	s << ",\n  \"unknown\": ";
	issues_json(s, v.unknown);
	if (!v.observations.empty()) {
		s << ",\n  \"observations\": ";
		issues_json(s, v.observations);
	}
	s << ",\n  \"sections\": [";
	for (std::size_t i = 0; i < v.sections.size(); ++i) {
		if (i) s << ',';
		const auto &x = v.sections[i];
		s << "\n    {\"name\": " << json_string(x.name) << ", \"offset\": " << x.offset << ", \"count\": " << x.count << ", \"declared_size\": " << x.declared_size << '}';
	}
	s << "\n  ],\n  \"vertices\": [";
	for (std::size_t i = 0; i < v.vertices.size(); ++i) {
		if (i) s << ',';
		s << "\n    ";
		array_json(s, v.vertices[i]);
	}
	s << "\n  ],\n  \"segments\": [";
	for (std::size_t i = 0; i < v.segments.size(); ++i) {
		if (i) s << ',';
		const auto &x = v.segments[i];
		s << "\n    {\"owner\": " << x.owner << ", \"group\": " << x.group << ", \"function\": " << x.function << ", \"producer\": " << x.producer
		  << ", \"value\": " << x.value << ", \"flags\": " << x.flags << ", \"light\": " << x.light;
		if (v.level_version == 23) {
			s << ", \"properties\": " << x.properties << ", \"damage\": ";
			array_json(s, x.damage);
		}
		s << ", \"children\": ";
		array_json(s, x.children);
		s << ", \"vertices\": ";
		array_json(s, x.vertices);
		s << ", \"sides\": [";
		for (std::size_t j = 0; j < x.sides.size(); ++j) {
			if (j) s << ", ";
			const auto &side = x.sides[j];
			s << "{\"wall\": " << side.wall << ", \"texture\": " << side.texture << ", \"overlay\": " << side.overlay << ", \"uvl\": ";
			array_json(s, side.uvl);
			s << '}';
		}
		s << "]}";
	}
	s << "\n  ],\n  \"walls\": [";
	for (std::size_t i = 0; i < v.walls.size(); ++i) {
		if (i) s << ',';
		const auto &w = v.walls[i];
		s << "\n    {\"segment\": " << w.segment << ", \"side\": " << w.side << ", \"hitpoints\": " << w.hitpoints << ", \"linked_wall\": " << w.linked_wall
		  << ", \"type\": " << w.type << ", \"flags\": " << w.flags << ", \"state\": " << w.state << ", \"trigger\": " << w.trigger << ", \"clip\": " << w.clip
		  << ", \"keys\": " << w.keys << ", \"controlling_trigger\": " << w.controlling_trigger << ", \"cloak\": " << w.cloak << '}';
	}
	s << "\n  ],\n  \"triggers\": [";
	for (std::size_t i = 0; i < v.triggers.size(); ++i) {
		if (i) s << ',';
		const auto &t = v.triggers[i];
		s << "\n    {\"type\": " << t.type << ", \"flags\": " << t.flags << ", \"value\": " << t.value << ", \"time\": " << t.time << ", \"targets\": [";
		for (std::size_t j = 0; j < t.targets.size(); ++j) {
			if (j) s << ", ";
			array_json(s, t.targets[j]);
		}
		s << "]}";
	}
	s << "\n  ],\n  \"object_triggers\": " << v.object_triggers;
	s << ",\n  \"object_trigger_heads\": [";
	for (std::size_t i = 0; i < v.object_trigger_heads.size(); ++i) {
		if (i) s << ", ";
		s << v.object_trigger_heads[i];
	}
	s << ']';
	if (!v.object_trigger_records.empty()) {
		s << ",\n  \"object_trigger_records\": [";
		for (std::size_t i = 0; i < v.object_trigger_records.size(); ++i) {
			if (i) s << ',';
			const auto &record = v.object_trigger_records[i];
			const auto &t = record.action;
			s << "\n    {\"object\": " << record.object << ", \"previous\": " << record.previous << ", \"next\": " << record.next
			  << ", \"type\": " << t.type << ", \"flags\": " << t.flags << ", \"value\": " << t.value << ", \"time\": " << t.time << ", \"targets\": [";
			for (std::size_t j = 0; j < t.targets.size(); ++j) {
				if (j) s << ", ";
				array_json(s, t.targets[j]);
			}
			s << "]}";
		}
		s << "\n  ]";
	}
	if (!v.object_trigger_sparse_heads.empty()) rows_json(s, "object_trigger_sparse_heads", v.object_trigger_sparse_heads);
	s << ",\n  \"objects\": [";
	for (std::size_t i = 0; i < v.objects.size(); ++i) {
		if (i) s << ',';
		const auto &o = v.objects[i];
		s << "\n    {\"type\": " << o.type << ", \"id\": " << o.id << ", \"control\": " << o.control << ", \"movement\": " << o.movement
		  << ", \"render\": " << o.render << ", \"flags\": " << o.flags << ", \"segment\": " << o.segment << ", \"position\": ";
		array_json(s, o.position);
		s << ", \"orientation\": ";
		array_json(s, o.orientation);
		s << ", \"last_position\": ";
		array_json(s, o.last_position);
		s << ", \"size\": " << o.size << ", \"shields\": " << o.shields << ", \"contains_type\": " << o.contains_type << ", \"contains_id\": " << o.contains_id
		  << ", \"contains_count\": " << o.contains_count;
		if (v.game_version == 40) s << ", \"multiplayer\": " << o.multiplayer;
		if (o.render >= 8 || o.control == 17 || o.movement == 2 || (o.type >= 18 && o.type <= 20)) {
			s << ", \"source_offset\": " << o.source_offset << ", \"source_end\": " << o.source_end
			  << ", \"reference_read_id\": " << o.reader_id << ", \"reference_read_render\": " << o.reader_render;
			if (o.render == 11) s << ", \"particle_type\": " << o.render_data[11] << ", \"effect_enabled\": " << o.render_data[12];
			if (o.render == 12) s << ", \"lightning_waypoint\": " << o.render_data[5] << ", \"lightning_width\": " << o.render_data[12]
				                  << ", \"lightning_sound\": " << o.render_data[18] << ", \"effect_enabled\": " << o.render_data[25];
			if (o.render == 13) {
				std::string filename;
				bool ascii = true;
				for (int j = 0; j < 39 && o.render_data[j]; ++j) {
					const int byte = o.render_data[j];
					if (byte < 32 || byte > 126) {
						ascii = false;
						break;
					}
					filename += static_cast<char>(byte >= 'A' && byte <= 'Z' ? byte + ('a' - 'A') : byte);
				}
				if (ascii) s << ", \"sound_filename_ascii\": " << json_string(filename);
				s << ", \"sound_volume_raw\": " << o.render_data[40] << ", \"effect_enabled\": " << o.render_data[41];
			}
		}
		const std::pair<const char *, const std::vector<int> *> payloads[] = { { "movement_data", &o.movement_data }, { "control_data", &o.control_data }, { "render_data", &o.render_data } };
		for (const auto &entry : payloads) {
			s << ", " << json_string(entry.first) << ": [";
			for (std::size_t j = 0; j < entry.second->size(); ++j) {
				if (j) s << ", ";
				s << (*entry.second)[j];
			}
			s << ']';
		}
		s << '}';
	}
	s << "\n  ]";
	s << ",\n  \"palette\": " << json_string(v.palette) << ",\n  \"level_name\": " << json_string(v.level_name)
	  << ",\n  \"countdown\": " << v.countdown << ",\n  \"reactor_strength\": " << v.reactor_strength << ",\n  \"secret_return_segment\": " << v.secret_return_segment
	  << ",\n  \"secret_return_orientation\": ";
	array_json(s, v.secret_return_orientation);
	s << ",\n  \"model_names\": [";
	for (std::size_t i = 0; i < v.model_names.size(); ++i) {
		if (i) s << ", ";
		s << json_string(v.model_names[i]);
	}
	s << ']';
	rows_json(s, "flicker_lights", v.flicker_lights);
	rows_json(s, "reactor_targets", v.reactor_targets);
	rows_json(s, "matcens", v.matcens);
	if (!v.equipment.empty()) rows_json(s, "equipment", v.equipment);
	rows_json(s, "light_indices", v.light_indices);
	rows_json(s, "delta_lights", v.delta_lights);
	const std::pair<const char *, const std::vector<Color> *> colors[] = { { "vertex_colors", &v.vertex_colors }, { "side_colors", &v.side_colors }, { "texture_colors", &v.texture_colors } };
	for (const auto &entry : colors) {
		s << ",\n  " << json_string(entry.first) << ": [";
		for (std::size_t i = 0; i < entry.second->size(); ++i) {
			if (i) s << ',';
			const auto &c = (*entry.second)[i];
			s << "\n    {\"index\": " << c.index << ", \"rgb\": ";
			array_json(s, c.rgb);
			s << '}';
		}
		s << "\n  ]";
	}
	s << "\n}\n";
	return s.str();
}

std::vector<Member> read_hog(const Bytes &bytes)
{
	Reader r(bytes, 0, bytes.size());
	require(r.u8() == 'D' && r.u8() == 'H' && r.u8() == 'F', "only standard DHF HOG inspection is implemented");
	std::vector<Member> members;
	std::set<std::string> names;
	while (r.position() < bytes.size()) {
		std::string name;
		bool terminated = false;
		for (int i = 0; i < 13; ++i) {
			const int c = r.u8();
			if (!c) terminated = true;
			if (!terminated) name += static_cast<char>(c);
		}
		require(!name.empty() && name.find_first_of("/\\:") == std::string::npos, "invalid HOG member name");
		auto key = name;
		for (auto &c : key)
			if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
		require(names.insert(key).second, "ambiguous duplicate HOG member " + name);
		const int size = r.s32();
		require(size >= 0, "negative HOG member size");
		const auto start = r.position();
		r.skip(static_cast<std::size_t>(size));
		members.push_back({ name, Bytes(bytes.begin() + start, bytes.begin() + r.position()) });
	}
	return members;
}
} // namespace xl_compat
