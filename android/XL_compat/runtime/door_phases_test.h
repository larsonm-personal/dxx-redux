#include "doors.h"
#include "texture_pixels.h"

static void descriptor_door_phase_controls(const xl_assets::ObjectDependencies &graph, const xl_compat::ScopedTexturePlayback &plan)
{
	xl_compat::Inventory fixture;
	fixture.segments.resize(Num_segments);
	for (int face = 0; face < 2; ++face) {
		xl_compat::Wall wall{};
		wall.segment = 0;
		wall.side = face;
		wall.type = WALL_DOOR;
		wall.clip = 0;
		fixture.walls.push_back(wall);
	}
	xl_door_phases_bind(fixture, graph, &plan);
	const auto initial = xl_door_phases_snapshot();
	require(initial.is_array() && initial.size() == std::size_t(Num_segments), "Door side owner omitted geometry");
	require(xl_door_record_frame(0, 0, 0, 2) && xl_door_record_frame(0, 1, 0, 3), "Independent door side phase update failed");
	const auto saved = xl_door_phases_snapshot();
	require(saved[0][0] == -3 && saved[0][1] == -4, "Door side phase does not use source encoding");
	auto &front = Segments[0].sides[0];
	auto &back = Segments[0].sides[1];
	struct RestoreSides {
		side a, b;
		~RestoreSides()
		{
			Segments[0].sides[0] = a;
			Segments[0].sides[1] = b;
			xl_door_phases_clear();
		}
	} sides{ front, back };
	const auto &clip = graph.wall_clip_data[0];
	front.tmap_num = back.tmap_num = clip.frames[0];
	front.tmap_num2 = back.tmap_num2 = 0;
	auto *ordinary = &GameBitmaps[graph.texture_data[clip.frames[0]].index];
	const auto *a = xl_door_side_bitmap(0, 0, front.tmap_num, ordinary);
	const auto *b = xl_door_side_bitmap(0, 1, back.tmap_num, ordinary);
	require(a != ordinary && b != ordinary && a != b && a->bm_data[0] == 12 && b->bm_data[0] == 0 &&
	            !xl_face_pixel_transparency(0, 0, 0, 0) && xl_face_pixel_transparency(0, 1, 0, 0),
	        "Door drawing and pixel collision did not select independent side frames");
	front.tmap_num = clip.frames[1];
	require(xl_door_side_bitmap(0, 0, front.tmap_num, ordinary) == a && xl_door_phases_snapshot() == saved,
	        "Authored alias texture change reset the side phase");
	require(xl_door_record_frame(0, 0, 0, 15), "Door final phase update failed");
	const auto advanced = xl_door_phases_snapshot();
	require(xl_door_phases_restore(saved, false) && xl_door_phases_snapshot() == advanced &&
	            xl_door_phases_restore(saved, true) && xl_door_phases_snapshot() == saved,
	        "Door phase validation mutated state or restore lost independent phases");
	unsigned malformed = 0;
	for (int variant = 0; variant < 8; ++variant) {
		auto bad = saved;
		if (variant == 0) bad = nlohmann::json::object();
		if (variant == 1) bad.erase(bad.begin());
		if (variant == 2) bad[0].push_back(0);
		if (variant == 3) bad[0][0] = -17;
		if (variant == 4) bad[0][0] = 2;
		if (variant == 5) bad[0][0] = UINT64_MAX;
		// Native GL headers define false as integer zero
		if (variant == 6) bad[0][0] = bool(0);
		if (variant == 7) bad[0][2] = -1;
		if (xl_door_phases_restore(bad, true) || xl_door_phases_snapshot() != saved)
			throw std::runtime_error("Malformed side phase variant " + std::to_string(variant) + " was accepted or partially applied");
		++malformed;
	}
	require(malformed == 8, "Malformed side phase state was accepted or partially applied");
	xl_door_phases_clear();
	xl_door_phases_bind(fixture, graph, &plan);
	require(xl_door_phases_snapshot() == initial && xl_door_phases_restore(saved, true) &&
	            xl_door_side_bitmap(0, 0, front.tmap_num, ordinary) == a,
	        "Rebound door side owner failed to restore its retained phase");
	std::cout << "Derived door phases: independent drawing/pixel views, authored aliases, validation/apply, eight malformed states and owner rebind passed\n";
}
