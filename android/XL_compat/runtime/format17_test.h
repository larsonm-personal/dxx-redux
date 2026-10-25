#include "no_damage_test.h"
// Native layout control only: the derived fixture intentionally removes no-damage
static void format17_layout_test(const xl_compat::Bytes &original, const char *directory)
{
	const auto source = xl_compat::inspect_level(original);
	require(source.level_version == 17 && source.game_version == 36, "layout control is not original 17/36");
	bool rejected = false;
	try {
		xl_compat::prepare_serialization_probe_17_36(original);
	} catch (const std::runtime_error &) {
		rejected = true;
	}
	require(rejected, "layout entry point bypassed original no-damage blocker");
	auto neutral = original;
	const auto color_bytes = (source.vertices.size() + source.segments.size() * 6 + 910) * 13;
	require(source.game_offset > color_bytes + source.segments.size() * 8, "invalid segment metadata boundary");
	const auto functions = source.game_offset - color_bytes - source.segments.size() * 8;
	int removed = 0;
	for (std::size_t i = 0; i < source.segments.size(); ++i) {
		require(neutral.at(functions + i * 8) == source.segments[i].function, "layout control metadata offsets differ");
		if (source.segments[i].function == 13) {
			neutral.at(functions + i * 8) = 0;
			++removed;
		}
	}
	require(removed == 26, "layout control did not identify all original no-damage segments");
	const auto probe = xl_compat::prepare_serialization_probe_17_36(neutral);
	const auto path = (std::filesystem::path(directory) / "format17-stock.rl2").string();
	write_bytes(path.c_str(), probe.stock_level);
	require(load_level("format17-stock.rl2") == 0, "native loader rejected 17/36 layout control");
	require(!xl_preview_pilot_active() && Num_segments == source.segments.size() && Num_vertices == source.vertices.size() &&
	            Num_walls == source.walls.size() && Num_triggers == source.triggers.size(),
	        "layout control changed native geometry or claimed XL activation");
	require(source.level_name == Current_level_name, "extra section header displaced native level name");
	for (std::size_t i = 0; i < source.vertices.size(); ++i)
		require(Vertices[i].x == source.vertices[i][0] && Vertices[i].y == source.vertices[i][1] && Vertices[i].z == source.vertices[i][2],
		        "native layout control changed vertex coordinates");
	for (std::size_t i = 0; i < source.segments.size(); ++i)
		require(Segment2s[i].special == probe.source.segments[i].function &&
		            (Segment2s[i].s2_flags & ~3) == (source.segments[i].flags & ~3),
		        "native layout control lost segment metadata");
	int native = 0;
	for (const auto &index : source.light_indices) {
		int remaining = index[2], target = index[3];
		do {
			const int chunk = (std::min) (remaining, 127);
			require(native < Num_static_lights && Dl_indices[native].segnum == index[0] && Dl_indices[native].sidenum == index[1] &&
			            Dl_indices[native].count == chunk && Dl_indices[native].index == target,
			        "native 17/36 light index differs");
			++native;
			remaining -= chunk;
			target += chunk;
		} while (remaining);
	}
	require(native == Num_static_lights, "native 17/36 light table count differs");
	for (std::size_t i = 0; i < source.delta_lights.size(); ++i) {
		const auto &expected = source.delta_lights[i];
		const auto &actual = Delta_lights[i];
		require(actual.segnum == expected[0] && actual.sidenum == expected[1], "native 17/36 delta target differs");
		for (int corner = 0; corner < 4; ++corner)
			require(actual.vert_light[corner] == expected[corner + 3], "native 17/36 delta contribution differs");
	}
	no_damage_collision_test(source);
	require(load_level("d2leva-1.rl2") == 0, "stock recovery after layout control failed");
	for (int i = 0; i < Num_segments; ++i)
		require(!xl_preview_no_damage_segment(i), "no-damage test binding survived stock recovery");
	std::cout << "17/36 native layout control: name, geometry, reserved bits and packed light indices verified; 26 no-damage functions removed only in derived fixture; original remains rejected\n";
}
