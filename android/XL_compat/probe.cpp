#include "probe.h"
#include "admission.h"
#include "capacity.h"
#include "producer_layout.h"
#include "ambient_sound.h"
#include "static_explosion.h"
#include "effect_targets.h"

#include <limits>
#include <algorithm>
#include <stdexcept>

namespace xl_compat
{
namespace
{

void require(bool value, const std::string &why)
{
	if (!value) throw std::runtime_error("gameplay probe: " + why);
}

void put(Bytes &bytes, int value, unsigned width)
{
	const auto bits = static_cast<std::uint32_t>(value);
	for (unsigned i = 0; i < width; ++i) bytes.push_back(static_cast<std::uint8_t>(bits >> (i * 8)));
}

void patch(Bytes &bytes, std::size_t offset, int value, unsigned width = 4)
{
	require(offset <= bytes.size() && width <= bytes.size() - offset, "patch outside file");
	const auto bits = static_cast<std::uint32_t>(value);
	for (unsigned i = 0; i < width; ++i) bytes[offset + i] = static_cast<std::uint8_t>(bits >> (i * 8));
}

int read_offset(const Bytes &bytes, std::size_t offset)
{
	require(offset <= bytes.size() && 4 <= bytes.size() - offset, "offset outside file");
	std::uint32_t value = 0;
	for (unsigned i = 0; i < 4; ++i) value |= std::uint32_t(bytes[offset + i]) << (i * 8);
	if (value == 0xffffffff) return -1;
	require(value <= static_cast<std::uint32_t>(std::numeric_limits<int>::max()), "invalid absolute offset");
	return static_cast<int>(value);
}
} // namespace

static GameplayProbe prepare_probe(const Bytes &source, bool water_segments, int qualification_version, bool source_runtime = false)
{
	GameplayProbe result;
	result.source = inspect_level(source);
	const auto &v = result.source;
	auto blockers = runtime_blockers(v, water_segments);
	const bool serialization_17_36 = v.level_version == 17 && v.game_version == 36;
	const bool serialization_23_40 = v.level_version == 23 && v.game_version == 40;
	const bool modern_layout = modern_runtime_layout(v);
	if (qualification_version == 17) require(serialization_17_36, "layout qualification requires 17/36");
	if (qualification_version == 23) {
		require(serialization_23_40, "layout qualification requires 23/40");
	}
	require(blockers.empty(), blocker_message(blockers));
	const auto producers = producer_layout(v);
	if (!source_runtime) {
		require(std::none_of(v.objects.begin(), v.objects.end(), ambient_sound_shape), "ambient sound objects require the source-owned XL runtime");
		require(std::none_of(v.objects.begin(), v.objects.end(), static_explosion_shape), "static explosion objects require the source-owned XL runtime");
		require(v.flicker_lights.size() <= 100, "classic probe flicker capacity requires the source-owned XL runtime");
		std::size_t translated_indices = 0;
		for (const auto &index : v.light_indices) {
			translated_indices += index[2] ? (index[2] + 126) / 127 : 1;
			require(index[3] <= 32767 && index[2] <= 32768 - index[3], "classic probe light indices require the source-owned XL runtime");
		}
		require(translated_indices <= 500 && v.delta_lights.size() <= 32000,
		        "classic probe light capacities require the source-owned XL runtime");
	}
	// Object actions remain in the owned source inventory, outside stock trigger storage
	// Wide walls use an owned private encoding; other records retain stock widths
	require(v.walls.size() <= DXX_XL_MAX_WALLS && xl_object_capacity_for_count(v.objects.size()) <= DXX_XL_MAX_OBJECTS && v.segments.size() <= 9000,
	        "stock representation capacity exceeded");
	const bool wide_walls = v.walls.size() > DXX_CLASSIC_MAX_WALLS;

	auto &out = result.stock_level;
	out.assign(source.begin(), source.begin() + v.mine_offset);
	if (source_runtime) {
		const auto begin = static_cast<std::size_t>(v.flicker_offset) + 4;
		const auto end = begin + v.flicker_lights.size() * 16;
		require(begin <= end && end <= out.size(), "invalid owned flicker header span");
		out.erase(out.begin() + begin, out.begin() + end);
		patch(out, v.flicker_offset, 0);
		patch(out, 8, static_cast<int>(out.size()));
	}
	patch(out, 4, 8);
	put(out, wide_walls ? DXX_XL_WIDE_WALL_MINE : 0, 1);
	put(out, static_cast<int>(v.vertices.size()), 2);
	put(out, static_cast<int>(v.segments.size()), 2);
	for (const auto &vertex : v.vertices)
		for (const auto component : vertex) put(out, component, 4);
	for (const auto &seg : v.segments) {
		require(seg.owner == -1, "nonneutral segment owner");
		int children = 0, walls = 0;
		for (int i = 0; i < 6; ++i) {
			if (seg.children[i] != -1) children |= 1 << i;
			if (seg.sides[i].wall != -1) walls |= 1 << i;
		}
		put(out, children, 1);
		for (int i = 0; i < 6; ++i)
			if (children & (1 << i)) put(out, seg.children[i], 2);
		for (const auto vertex : seg.vertices) put(out, vertex, 2);
		put(out, walls, 1);
		for (int i = 0; i < 6; ++i) {
			if (walls & (1 << i)) {
				require(seg.sides[i].wall >= 0 && std::size_t(seg.sides[i].wall) < v.walls.size(), "unrepresentable wall ID");
				put(out, seg.sides[i].wall, wide_walls ? 2 : 1);
			}
		}
		for (int i = 0; i < 6; ++i) {
			const auto &side = seg.sides[i];
			if (seg.children[i] != -1 && side.wall == -1) continue;
			require(side.texture >= 0 && side.texture < 910, "unrepresentable texture");
			put(out, side.texture | (side.overlay ? 0x8000 : 0), 2);
			if (side.overlay) put(out, side.overlay, 2);
			for (const auto component : side.uvl) put(out, component, 2);
		}
	}
	for (std::size_t segment = 0; segment < v.segments.size(); ++segment) {
		const auto &seg = v.segments[segment];
		const int function = producer_function(seg);
		// The runtime retains water in the source inventory, never in a stock enum
		put(out, (v.level_version < 21 && ((water_segments && function == 7) || function == 13 || function == 11)) || (modern_layout && (function == 11 || function == 9)) ? 0 : function, 1);
		// Wide authored fields stay in the owned source. Only robot assignment enters
		// the native shadow, after geometry-order initialization and capacity checks
		// Both loaders rebuild station values; other functions never consume them
		put(out, function == 4 ? producers.robot_index[segment] : v.level_version >= 25 ? -1
		                                                                                : seg.producer,
		    1);
		put(out, v.level_version >= 25 ? 0 : seg.value, 1);
		put(out, seg.flags, 1);
		put(out, seg.light, 4);
	}
	require(out.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()), "level size overflow");
	const int new_game_offset = static_cast<int>(out.size());
	const bool equipment_header = serialization_17_36 || modern_layout;
	const int removed_header = (equipment_header ? 12 : 0) + (v.level_version >= 27 ? 16 : 0);
	if (equipment_header) {
		require(v.sections.size() == 10 && v.sections[9].name == "equipment" && v.sections[9].count == static_cast<int>(v.equipment.size()), "equipment section does not match decoded records");
		out.insert(out.end(), source.begin() + v.game_offset, source.begin() + v.game_offset + 143);
		out.insert(out.end(), source.begin() + v.game_offset + 143 + removed_header, source.end());
		patch(out, new_game_offset + 4, 143);
	} else out.insert(out.end(), source.begin() + v.game_offset, source.end());
	patch(out, 12, new_game_offset);
	patch(out, new_game_offset + 2, 32, 2);
	// Preserve the game records byte for byte, including the neutral legacy object-head
	// padding. Stock game 32 reads explicit section offsets and never consumes that tail
	const auto rebase = [&](std::size_t field) {
		const int old = read_offset(source, v.game_offset + field);
		if (old <= 0) return;
		require(old >= v.game_offset + (equipment_header ? 143 + removed_header : 0) && static_cast<std::size_t>(old) <= source.size(), "invalid game section offset");
		patch(out, new_game_offset + field, old - v.game_offset + new_game_offset - removed_header);
	};
	rebase(27); // Legacy unused player block
	for (std::size_t i = 0; i < 9; ++i) rebase(35 + i * 12);
	if (!v.matcens.empty()) {
		// XL initializes producers in geometry order, then reads row i for maker i
		// Redux's native station list also includes reactors, requiring fresh links
		patch(out, new_game_offset + 35 + 6 * 12, static_cast<int>(out.size()));
		patch(out, new_game_offset + 43 + 6 * 12, 20);
		int station = 0;
		for (std::size_t segment = 0; segment < v.segments.size(); ++segment) {
			const auto &source_segment = v.segments[segment];
			const int function = producer_function(source_segment);
			if (function < 1 || function > 4) continue;
			if (function == 4) {
				const auto &row = v.matcens.at(producers.robot_index[segment]);
				for (int field = 0; field < 4; ++field) put(out, row[field], 4);
				put(out, static_cast<int>(segment), 2);
				put(out, station, 2);
			}
			++station;
		}
	}
	// XL Read normalizes authored master semaphores, but save restoration retains
	// the live raw count. Apply this only to the source-level translation
	const auto trigger_offset = read_offset(out, new_game_offset + 71);
	for (std::size_t i = 0; i < v.triggers.size(); ++i) {
		const auto &trigger = v.triggers[i];
		const auto offset = static_cast<std::size_t>(trigger_offset) + i * 52;
		if (trigger.type == 32 || trigger.type == 33) {
			const auto targets = effect_targets(trigger, v.segments.size(), v.objects.size(), false);
			patch(out, offset + 2, int(targets.size()), 1);
			for (std::size_t link = 0; link < targets.size(); ++link) {
				patch(out, offset + 12 + link * 2, targets[link][0], 2);
				patch(out, offset + 32 + link * 2, targets[link][1], 2);
			}
		}
		if (trigger.type != 31) continue;
		if (v.game_version < 39) patch(out, offset + 4, 0);
		else if (trigger.value > 0) {
			patch(out, offset + 4, trigger.value / 65536);
			patch(out, offset + 1, (trigger.flags | 4) & (trigger.flags & 128 ? ~8 : ~0), 1);
		}
	}
	const bool ambient_objects = std::any_of(v.objects.begin(), v.objects.end(), ambient_sound_shape);
	if (modern_layout || ambient_objects) {
		// Stock uses explicit section offsets. Retain the original game payload as
		// inert evidence, then point at records with the audited stock widths
		require(out.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()), "object table offset overflow");
		patch(out, new_game_offset + 35, static_cast<int>(out.size()));
		for (const auto &object : v.objects) {
			const unsigned header = modern_layout ? 80 : 79;
			require(object.multiplayer == 0 && object.source_end >= object.source_offset + header && object.source_end <= source.size(), "invalid object translation span");
			const auto start = out.size();
			out.insert(out.end(), source.begin() + object.source_offset, source.begin() + object.source_offset + 6);
			const auto end = ambient_sound_shape(object) ? object.source_offset + header : object.source_end;
			out.insert(out.end(), source.begin() + object.source_offset + (modern_layout ? 7 : 6), source.begin() + end);
			if (ambient_sound_shape(object)) {
				patch(out, start + 1, object.reader_id, 1);
				patch(out, start + 4, 0, 1); // Audio payload and enable state remain source-owned
			}
		}
	}
	if (modern_layout) {
		require(out.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()), "wall table offset overflow");
		patch(out, new_game_offset + 47, static_cast<int>(out.size()));
		patch(out, new_game_offset + 55, 24);
		for (std::size_t i = 0; i < v.walls.size(); ++i) {
			const auto start = static_cast<std::size_t>(v.sections.at(1).offset) + i * 25;
			require(v.walls[i].flags <= 255 && start + 25 <= source.size(), "invalid wall translation span");
			out.insert(out.end(), source.begin() + start, source.begin() + start + 18);
			out.insert(out.end(), source.begin() + start + 19, source.begin() + start + 25);
		}
	}
	if (source_runtime) {
		// Never apply a narrowed stock shadow as well as the owned XL table
		for (int section : { 7, 8 }) {
			patch(out, new_game_offset + 35 + section * 12, -1);
			patch(out, new_game_offset + 39 + section * 12, 0);
		}
	} else if (equipment_header || std::any_of(v.light_indices.begin(), v.light_indices.end(), [](const auto &index) { return index[2] > 127; })) {
		// Stock traverses every matching source-face entry in order. Split unsigned XL
		// counts without changing delta order or per-target clamping; retain raw source data
		require(out.size() <= static_cast<std::size_t>(std::numeric_limits<int>::max()) - 500 * 6, "light table offset overflow");
		const int offset = static_cast<int>(out.size());
		int count = 0;
		for (const auto &index : v.light_indices) {
			int remaining = index[2], target = index[3];
			do {
				const int chunk = (std::min) (remaining, 127);
				put(out, index[0], 2);
				put(out, index[1], 1);
				put(out, chunk, 1);
				put(out, target, 2);
				++count;
				target += chunk;
				remaining -= chunk;
			} while (remaining);
		}
		patch(out, new_game_offset + 35 + 7 * 12, offset);
		patch(out, new_game_offset + 39 + 7 * 12, count);
		patch(out, new_game_offset + 43 + 7 * 12, 6);
	}
	return result;
}

GameplayProbe prepare_gameplay_probe(const Bytes &source, bool water_segments)
{
	return prepare_probe(source, water_segments, 0);
}

GameplayProbe prepare_source_runtime_probe(const Bytes &source, bool water_segments)
{
	return prepare_probe(source, water_segments, 0, true);
}

GameplayProbe prepare_serialization_probe_17_36(const Bytes &source)
{
	return prepare_probe(source, false, 17);
}

GameplayProbe prepare_serialization_probe_23_40(const Bytes &source)
{
	return prepare_probe(source, false, 23);
}
} // namespace xl_compat
