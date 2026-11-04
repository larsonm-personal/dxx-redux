// Isolated collision controls over caller-supplied derived geometry, not progression
extern "C" {
#include "fvi.h"
}
static void boost_crossing_test(const std::filesystem::path &geometry_path)
{
	std::ifstream input(geometry_path);
	nlohmann::json geometry;
	input >> geometry;
	const int base_segment = Num_segments, base_vertex = Num_vertices;
	const int old_highest = Highest_segment_index;
	const auto original = *ConsoleObject;
	const auto old_frame = FrameTime;
	const int count = static_cast<int>(geometry.at("segments").size());
	require(base_segment + count < MAX_SEGMENTS && base_vertex + geometry.at("vertices").size() < MAX_VERTICES, "crossing fixture exceeds geometry capacity");
	for (const auto &point : geometry.at("vertices"))
		Vertices[Num_vertices++] = { point[0].get<int>(), point[1].get<int>(), point[2].get<int>() };
	for (int index = 0; index < count; ++index) {
		auto &segment = Segments[base_segment + index];
		segment = {};
		segment.objects = -1;
		const auto &record = geometry.at("segments")[index];
		for (int vertex = 0; vertex < 8; ++vertex) segment.verts[vertex] = static_cast<short>(base_vertex + record.at("vertices")[vertex].get<int>());
		for (int side = 0; side < 6; ++side) {
			const int child = record.at("children")[side].get<int>();
			segment.children[side] = static_cast<short>(child < 0 ? -1 : base_segment + child);
			segment.sides[side].wall_num = -1;
		}
	}
	Num_segments += count;
	Highest_segment_index = Num_segments - 1;
	for (int index = 0; index < count; ++index) validate_segment(&Segments[base_segment + index]);
	const auto valid_path = [&](const int *path, int size, int from, int to) {
		if (size < 1 || path[0] != from || path[size - 1] != to) return false;
		for (int index = 1; index < size; ++index) {
			if (path[index] < 0 || path[index] > Highest_segment_index || path[index - 1] < 0 || path[index - 1] > Highest_segment_index) return false;
			if (path[index - 1] == path[index]) continue;
			if (find_connect_side(&Segments[path[index]], &Segments[path[index - 1]]) < 0) return false;
		}
		return true;
	};
	std::ofstream output(geometry_path.parent_path() / "crossing-probe.jsonl");
	std::ofstream collisions(geometry_path.parent_path() / "crossing-collisions.jsonl");
	int cases = 0, bad_fvi = 0, bad_physics = 0;
	for (int index = 0; index + 1 < count; ++index) {
		const int segment = base_segment + index;
		const int side = find_connect_side(&Segments[segment + 1], &Segments[segment]);
		require(side >= 0, "crossing fixture is not a connected path");
		vms_vector center, target, direction;
		compute_segment_center(&center, &Segments[segment]);
		compute_center_point_on_side(&target, &Segments[segment], side);
		vm_vec_sub(&direction, &target, &center);
		vm_vec_normalize(&direction);
		for (int dx : { -5, 0, 5 })
			for (int dy : { -5, 0, 5 })
				for (int dz : { -5, 0, 5 })
					for (int speed : { 100, 460 })
						for (fix frame : { F1_0 / 120, F1_0 / 60, F1_0 / 30, F1_0 / 20, F1_0 / 10, F1_0 / 5, F1_0 / 4, F1_0 / 2, F1_0 }) {
							vms_vector start = center, velocity, end;
							start.x += i2f(dx);
							start.y += i2f(dy);
							start.z += i2f(dz);
							const fix radius = 301466;
							const auto masks = get_seg_masks(&start, segment, radius, __FILE__, __LINE__);
							if (masks.centermask) continue;
							bool blocked = false;
							for (int face = 0; face < 6; ++face)
								if ((masks.sidemask & (1 << face)) && Segments[segment].children[face] < 0) blocked = true;
							if (blocked) continue;
							vm_vec_copy_scale(&velocity, &direction, i2f(speed));
							vm_vec_scale_add(&end, &start, &velocity, frame);
							fvi_query query{};
							query.p0 = &start;
							query.p1 = &end;
							query.startseg = segment;
							query.rad = radius;
							query.thisobjnum = ConsoleObject - Objects;
							query.flags = FQ_GET_SEGLIST;
							fvi_info hit{};
							const int fate = find_vector_intersection(&query, &hit);
							const bool fvi_valid = fate != HIT_BAD_P0 && hit.hit_seg >= 0 && valid_path(hit.seglist, hit.n_segs, segment, hit.hit_seg);
							obj_relink(ConsoleObject - Objects, segment);
							ConsoleObject->pos = ConsoleObject->last_pos = start;
							ConsoleObject->control_type = CT_NONE;
							ConsoleObject->movement_type = MT_PHYSICS;
							ConsoleObject->size = radius;
							ConsoleObject->mtype.phys_info = {};
							ConsoleObject->mtype.phys_info.mass = i2f(4);
							ConsoleObject->mtype.phys_info.velocity = velocity;
							FrameTime = frame;
							xl_speed_boost_clear();
							do_physics_sim(ConsoleObject);
							const bool physics_valid = valid_path(phys_seglist, n_phys_segs, segment, ConsoleObject->segnum);
							const auto point = [](const vms_vector &value) { return std::array<int, 3>{ value.x, value.y, value.z }; };
							collisions << nlohmann::json{ { "from", index }, { "frame", frame }, { "speed", speed }, { "offset", { dx, dy, dz } }, { "fate", fate }, { "hit", point(hit.hit_pnt) }, { "normal", point(hit.hit_wallnorm) }, { "segment", hit.hit_seg }, { "side", hit.hit_side }, { "side_segment", hit.hit_side_seg }, { "face", hit.hit_face }, { "object", hit.hit_object }, { "position", point(ConsoleObject->pos) }, { "velocity", point(ConsoleObject->mtype.phys_info.velocity) }, { "end", ConsoleObject->segnum } }.dump() << '\n';
							++cases;
							bad_fvi += !fvi_valid;
							bad_physics += !physics_valid;
							if (!fvi_valid || !physics_valid) {
								nlohmann::json row = { { "from", index }, { "frame", frame }, { "speed", speed }, { "offset", { dx, dy, dz } }, { "fate", fate }, { "fvi_end", hit.hit_seg - base_segment }, { "physics_end", ConsoleObject->segnum - base_segment }, { "fvi_valid", fvi_valid }, { "physics_valid", physics_valid } };
								row["fvi_path"] = std::vector<int>(hit.seglist, hit.seglist + hit.n_segs);
								row["physics_path"] = std::vector<int>(phys_seglist, phys_seglist + n_phys_segs);
								query.rad = 0;
								query.p1 = &hit.hit_pnt;
								fvi_info point_hit{};
								find_vector_intersection(&query, &point_hit);
								row["point_end"] = point_hit.hit_seg - base_segment;
								row["point_path"] = std::vector<int>(point_hit.seglist, point_hit.seglist + point_hit.n_segs);
								row["point_valid"] = point_hit.hit_seg == hit.hit_seg && valid_path(point_hit.seglist, point_hit.n_segs, segment, point_hit.hit_seg);
								output << row.dump() << '\n';
							}
						}
	}
	obj_relink(ConsoleObject - Objects, original.segnum);
	const int next = ConsoleObject->next, previous = ConsoleObject->prev;
	*ConsoleObject = original;
	ConsoleObject->next = next;
	ConsoleObject->prev = previous;
	Num_segments = base_segment;
	Num_vertices = base_vertex;
	Highest_segment_index = old_highest;
	FrameTime = old_frame;
	std::cout << "Crossing controls: " << cases << " cases, bad FVI " << bad_fvi << ", bad physics " << bad_physics << '\n';
	require(cases > 100 && !bad_fvi && !bad_physics, "movement crossing path omitted connected portals");
}

static void boost_crossing_controls(const std::filesystem::path &root)
{
	// Independently generated boxes, not copied mission geometry or trigger dispatch
	nlohmann::json geometry = { { "vertices", nlohmann::json::array() }, { "segments", nlohmann::json::array() } };
	for (int index = 0; index < 5; ++index) {
		const int base = index * 8, z = index * 40;
		for (const std::array<int, 3> vertex : { std::array<int, 3>{ 1010, -10, z + 40 }, { 1010, 10, z + 40 }, { 990, 10, z + 40 }, { 990, -10, z + 40 }, { 1010, -10, z }, { 1010, 10, z }, { 990, 10, z }, { 990, -10, z } })
			geometry["vertices"].push_back({ i2f(vertex[0]), i2f(vertex[1]), i2f(vertex[2]) });
		geometry["segments"].push_back({ { "vertices", { base, base + 1, base + 2, base + 3, base + 4, base + 5, base + 6, base + 7 } },
		                                 { "children", { -1, -1, -1, -1, index ? index - 1 : -1, index < 4 ? index + 1 : -1 } } });
	}
	const auto file = root / "crossing-geometry.json";
	std::ofstream(file) << geometry.dump(2);
	boost_crossing_test(file);
}
