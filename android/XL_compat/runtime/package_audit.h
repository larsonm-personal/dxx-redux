// Native startup inventory only; does not certify progression or rendered resources
#include "companion_resources.h"
#include "geometry.h"
#include "light_sources.h"
#include "visibility.h"
extern "C" {
#include "collide.h"
}
static int package_audit(int argc, char **argv)
{
	try {
		auto audit_optics = xl_compat::classic_default_lighting_camera();
		const auto canvas_width = find_arg_value(argc, argv, "-package-audit-canvas-width");
		const auto canvas_height = find_arg_value(argc, argv, "-package-audit-canvas-height");
		if (canvas_width || canvas_height) {
			require(canvas_width && canvas_height, "Audit canvas requires both width and height");
			require(!find_arg_value(argc, argv, "-package-audit-check-ownership"), "Custom audit canvas cannot certify default runtime ownership");
			const auto dimension = [](const char *text) {
				std::size_t consumed = 0;
				const int value = std::stoi(text, &consumed);
				require(text[consumed] == '\0' && value >= 32 && value <= 16384, "Invalid audit canvas dimension");
				return value;
			};
			audit_optics.screen_width = dimension(canvas_width);
			audit_optics.screen_height = dimension(canvas_height);
			audit_optics.half_width = audit_optics.screen_width * 0.5f;
			audit_optics.half_height = audit_optics.screen_height * 0.5f;
		}
		require(xl_compat::geometry_fixed_normal({ 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }) == xl_compat::FixedLightPoint{ 0, 0, 65536 }, "XL tiny-edge normal scaling failed");
		require(xl_compat::geometry_fixed_normal({ 0, 0, 0 }, { 1, 1, 0 }, { 1, 0, 0 }) == xl_compat::FixedLightPoint{ 0, 0, -65536 }, "XL reversed tiny-edge normal failed");
		require(xl_compat::geometry_fixed_normal({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 0 }) == xl_compat::FixedLightPoint{}, "XL degenerate fixed normal failed");
		{
			xl_compat::ClassicDistanceRow row;
			row.scale = 2;
			row.entries = { { 10, 0, 1 }, { 10, 0, -1 }, { 0, -1, -1 }, { 65535, -1, -1 } };
			const std::vector<xl_compat::FixedLightPoint> centers{ { 65536, 0, 0 }, { 0, 131072, 0 } };
			require(xl_compat::classic_segment_distance(row, centers, 0, {}, {}) == 196628, "two-ended distance reconstruction differs");
			require(xl_compat::classic_segment_distance(row, centers, 1, {}, {}) == 131072, "single-ended distance reconstruction differs");
			require(xl_compat::classic_segment_distance(row, centers, 2, {}, { 196608, 262144, 0 }) == 327680, "direct distance reconstruction differs");
			require(xl_compat::classic_segment_distance(row, centers, 3, {}, {}) == -1 && xl_compat::classic_segment_distance(row, centers, 4, {}, {}) == -1, "unreachable distance query admitted");
		}
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission(find_arg_value(argc, argv, "-mission"), error, sizeof(error)), error);
		new_player_config();
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		nlohmann::json report;
		report["mission"] = Current_mission_filename;
		report["level"] = Gamesave_current_filename;
		report["experimental_audit_canvas"] = { { "width", audit_optics.screen_width }, { "height", audit_optics.screen_height }, { "half_width", audit_optics.half_width }, { "half_height", audit_optics.half_height }, { "qualified", bool(false) } };
		const auto companions = xl_compat::load_mission_lighting_companions(Gamesave_current_filename);
		const auto route_graph = xl_compat::classic_light_route_graph();
		const auto vertex_normals = xl_compat::compile_classic_vertex_normals();
		auto &normal_report = report["experimental_vertex_normals"];
		normal_report = { { "qualified", false }, { "vertices", nlohmann::json::array() } };
		std::size_t normal_contributions = 0;
		for (const auto &entry : vertex_normals) {
			for (const auto value : entry.normal) require(std::isfinite(value), "nonfinite compiled vertex normal");
			normal_contributions += entry.contributions;
			normal_report["vertices"].push_back({ { "normal", entry.normal }, { "contributions", entry.contributions } });
		}
		require(normal_contributions == (static_cast<std::size_t>(Highest_segment_index) + 1) * 24, "compiled vertex normals lost side corners");
		std::vector<std::array<xl_compat::GeometryRouteEdge, 6>> weighted_graph(route_graph.edges.size());
		for (std::size_t segment = 0; segment < route_graph.edges.size(); ++segment)
			for (unsigned side = 0; side < 6; ++side) {
				const auto &edge = route_graph.edges[segment][side];
				if (edge.passable) weighted_graph[segment][side] = { edge.child, edge.cost };
			}
		report["experimental_route_graph"] = { { "scale", route_graph.scale }, { "edges", nlohmann::json::array() }, { "qualified", false } };
		for (std::size_t segment = 0; segment < route_graph.edges.size(); ++segment)
			for (unsigned side = 0; side < 6; ++side) {
				const auto &edge = route_graph.edges[segment][side];
				if (edge.child < 0) continue;
				require(edge.distance > 0 && edge.cost > 0, "route edge has no positive cost");
				report["experimental_route_graph"]["edges"].push_back({ { "segment", segment }, { "side", side }, { "child", edge.child }, { "distance", edge.distance }, { "cost", edge.cost }, { "passable", edge.passable } });
				auto &entry = report["experimental_route_graph"]["edges"].back();
				const auto &surface = Segments[segment].sides[side];
				const int overlay = surface.tmap_num2 & 0x3fff;
				entry["base_texture"] = surface.tmap_num;
				entry["overlay_texture"] = overlay;
				entry["base_bitmap_flags"] = piggy_bitmap_get_flags(&GameBitmaps[Textures[surface.tmap_num].index]);
				entry["overlay_bitmap_flags"] = overlay ? piggy_bitmap_get_flags(&GameBitmaps[Textures[overlay].index]) : 0;
				entry["wall_index"] = surface.wall_num;
				if (surface.wall_num >= 0 && surface.wall_num < Num_walls) {
					const auto &wall = Walls[surface.wall_num];
					entry["wall"] = { { "type", wall.type }, { "flags", wall.flags }, { "state", wall.state }, { "cloak", wall.cloak_value } };
				}
			}
		report["lighting_companions"] = {
			{ "archive", companions.archive },
			{ "brightness_present", bool(companions.brightness) },
			{ "colors_present", bool(companions.colors) },
			{ "rendering_qualified", bool(false) }
		};
		if (companions.brightness)
			report["lighting_companions"]["nonzero_brightness"] = std::count_if(companions.brightness->begin(), companions.brightness->end(), [](auto value) { return value != 0; });
		if (companions.colors)
			report["lighting_companions"]["chromatic_colors"] = std::count_if(companions.colors->begin(), companions.colors->end(), [](const auto &value) { return value.channels[0] != value.channels[1] || value.channels[1] != value.channels[2]; });
		if (companions.brightness && companions.colors) {
			const auto optics = audit_optics;
			const auto segment_visibility = xl_compat::classic_segment_visibility(optics);
			std::size_t visible_pairs = 0;
			std::vector<std::size_t> visible_row_counts;
			for (std::size_t i = 0; i < segment_visibility.size(); ++i) {
				require(segment_visibility[i][i], "segment visibility lost diagonal");
				visible_row_counts.push_back(std::count(segment_visibility[i].begin(), segment_visibility[i].end(), bool(true)));
				for (std::size_t j = 0; j < segment_visibility.size(); ++j) {
					require(segment_visibility[i][j] == segment_visibility[j][i], "segment visibility is not symmetric");
					if (j >= i && segment_visibility[i][j]) ++visible_pairs;
				}
			}
			report["experimental_segment_visibility"] = { { "pairs_including_diagonal", visible_pairs }, { "row_counts", visible_row_counts }, { "qualified", false } };
			const auto sources = xl_compat::compile_classic_geometry_lights(*companions.brightness, *companions.colors);
			std::vector<std::vector<int>> mesh_vertices;
			std::vector<std::int32_t> segment_radii;
			for (int segment = 0; segment <= Highest_segment_index; ++segment)
				segment_radii.push_back(xl_compat::classic_segment_average_radius(segment));
			report["experimental_segment_average_radii"] = { { "qualified", false }, { "radii", segment_radii } };
			std::vector<std::vector<bool>> vertex_visibility(route_graph.centers.size(), std::vector<bool>(static_cast<std::size_t>(Highest_vertex_index) + 1, false));
			for (int segment = 0; segment <= Highest_segment_index; ++segment)
				mesh_vertices.push_back(xl_compat::classic_lighting_face_vertices(segment));
			report["geometry_light_sources"] = nlohmann::json::array();
			for (const auto &source : sources) {
				report["geometry_light_sources"].push_back({ { "segment", source.segment }, { "side", source.side }, { "created_texture", source.material.created_texture }, { "material_texture", source.material.material_texture }, { "brightness", source.material.brightness }, { "rgb", source.material.rgb }, { "ambient", source.material.ambient }, { "center_fixed", source.geometry.center }, { "emission_direction", source.emission_direction } });
				report["geometry_light_sources"].back()["variability"] = { { "variable", source.variable() }, { "destructible", source.destructible }, { "flickering", source.flickering }, { "triggered", source.triggered }, { "volatile_wall", source.volatile_wall } };
				report["geometry_light_sources"].back()["radius"] = source.radius;
				const auto views = xl_compat::classic_light_views(source.segment, source.side);
				require(bool(views), "original light source has degenerate view normal");
				auto &view_report = report["geometry_light_sources"].back()["visibility_views"];
				view_report = nlohmann::json::array();
				for (const auto &view : *views) {
					require(view.forward != xl_compat::FixedLightPoint{}, "original light view has zero direction");
					view_report.push_back({ { "position_fixed", view.position }, { "forward_fixed", view.forward } });
				}
				const auto candidates = xl_compat::classic_light_view_candidates(source.segment, source.side, optics);
				const auto reachable = xl_compat::classic_light_route_reachable(source.segment);
				const auto routes = xl_compat::geometry_weighted_routes(weighted_graph, source.segment);
				const auto distance_row = xl_compat::classic_distance_row(route_graph, segment_visibility, source.segment);
				std::int32_t maximum_reconstructed = 0;
				std::int32_t maximum_cost = 0;
				std::int32_t maximum_interior_distance = 0;
				std::size_t maximum_nodes = 0;
				for (std::size_t segment = 0; segment < routes.size(); ++segment) {
					require((routes[segment].cost >= 0) == std::binary_search(reachable.begin(), reachable.end(), static_cast<int>(segment)), "weighted route reachability differs from topology");
					maximum_cost = (std::max) (maximum_cost, routes[segment].cost);
					const auto reconstructed = xl_compat::classic_segment_distance(distance_row, route_graph.centers, static_cast<int>(segment), {}, {});
					require((reconstructed >= 0) == (routes[segment].cost >= 0), "reconstructed distance sign differs from weighted reachability");
					maximum_reconstructed = (std::max) (maximum_reconstructed, reconstructed);
					const auto path = xl_compat::geometry_route_path(routes, static_cast<int>(segment));
					if (!path.empty()) require(path.front() == source.segment && path.back() == static_cast<int>(segment), "reconstructed mission route endpoints differ");
					maximum_nodes = (std::max) (maximum_nodes, path.size());
					const auto interior = xl_compat::geometry_shorten_route(path, [&segment_visibility](int a, int b) { return bool(segment_visibility[a][b]); }, [&route_graph](int a, int b) { return xl_compat::geometry_fixed_distance(route_graph.centers[a], route_graph.centers[b]); });
					require((interior.distance >= 0) == !path.empty(), "route shortening changed reachability");
					maximum_interior_distance = (std::max) (maximum_interior_distance, interior.distance);
				}
				report["geometry_light_sources"].back()["experimental_weighted_routes"] = { { "maximum_cost", maximum_cost }, { "maximum_nodes", maximum_nodes }, { "reachable_count", reachable.size() }, { "qualified", false } };
				report["geometry_light_sources"].back()["experimental_weighted_routes"]["maximum_interior_distance_fixed"] = maximum_interior_distance;
				report["geometry_light_sources"].back()["experimental_distance_row"] = { { "scale", distance_row.scale }, { "maximum_zero_query_distance", maximum_reconstructed }, { "qualified", false } };
				std::vector<int> route_candidates;
				for (const auto candidate : candidates.segments)
					if (xl_compat::classic_segment_distance(distance_row, route_graph.centers, candidate, {}, {}) >= 0) route_candidates.push_back(candidate);
				for (const auto destination : route_candidates)
					for (const auto vertex : mesh_vertices[destination]) vertex_visibility[source.segment][vertex] = true;
				require(std::binary_search(candidates.segments.begin(), candidates.segments.end(), source.segment), "source candidate union lost its own segment");
				std::array<std::size_t, 5> counts;
				for (unsigned i = 0; i < counts.size(); ++i) counts[i] = candidates.views[i].size();
				report["geometry_light_sources"].back()["experimental_view_candidates"] = { { "per_view_counts", counts }, { "segments", candidates.segments }, { "qualified", false } };
				report["geometry_light_sources"].back()["experimental_route_reachability"] = { { "reachable_count", reachable.size() }, { "view_candidates", route_candidates }, { "segdist_qualified", false } };
			}
			std::vector<std::size_t> vertex_row_counts;
			for (const auto &row : vertex_visibility)
				vertex_row_counts.push_back(std::count(row.begin(), row.end(), bool(true)));
			report["experimental_vertex_visibility"] = { { "source_segment_row_counts", vertex_row_counts }, { "qualified", false }, { "profile", "classic D1 method-0 mesh faces, accumulated before range/ray updates" } };
			auto &nearest_report = report["experimental_nearest_vertex_lights"];
			nearest_report = { { "qualified", false }, { "vertices", nlohmann::json::array() } };
			auto &active_report = report["experimental_ambient_active_lights"];
			active_report = { { "qualified", false }, { "profile", "method-0 fresh sources assumed enabled with zero stored render distances; lifecycle not implemented" }, { "vertices", nlohmann::json::array() } };
			auto &ambient_report = report["experimental_ambient_colors"];
			ambient_report = { { "qualified", false }, { "profile", "fresh method-0, no material specular, range 0.5, ambient 0.3, diffuse 0.7, brightness 1, renormalized world normals" }, { "vertices", nlohmann::json::array() } };
			for (int vertex = 0; vertex <= Highest_vertex_index; ++vertex) {
				const auto nearest = xl_compat::classic_nearest_vertex_lights(vertex, sources, vertex_visibility);
				require(nearest.size() <= 32, "nearest vertex table exceeds reference cap");
				nlohmann::json entries = nlohmann::json::array();
				for (std::size_t i = 0; i < nearest.size(); ++i) {
					require(nearest[i].index >= 0 && static_cast<std::size_t>(nearest[i].index) < sources.size(), "nearest vertex source index invalid");
					if (i) require(nearest[i - 1].distance <= nearest[i].distance, "nearest vertex distances unsorted");
					entries.push_back({ { "source", nearest[i].index }, { "distance", nearest[i].distance } });
				}
				nearest_report["vertices"].push_back(entries);
				std::vector<xl_compat::GeometryLightDistance> ambient_candidates;
				const auto &v = Vertices[vertex];
				for (const auto &entry : nearest) {
					const auto &source = sources[entry.index];
					const auto distance = xl_compat::geometry_ambient_distance(xl_compat::geometry_fixed_distance({ v.x, v.y, v.z }, source.geometry.center), source.radius, segment_radii[source.segment]);
					// Destination -1 returns before Contribute writes render.xDistance
					if (distance) ambient_candidates.push_back({ entry.index, 0 });
				}
				nlohmann::json active_entries = nlohmann::json::array();
				xl_compat::GeometryColorSum color_sum;
				for (const auto &entry : xl_compat::geometry_active_lights(ambient_candidates)) {
					active_entries.push_back({ { "source", entry.index }, { "distance", entry.distance } });
					const auto &source = sources[entry.index];
					if (source.material.rgb == xl_compat::Rgb{}) continue;
					bool own = false;
					for (int corner = 0; corner < 4; ++corner)
						if (Segments[source.segment].verts[Side_to_verts_int[source.side][corner]] == vertex) own = true;
					if (source.material.ambient && !own) continue;
					xl_compat::GeometryColorInput input;
					input.face = source.geometry.face;
					for (unsigned axis = 0; axis < 3; ++axis) input.center[axis] = source.geometry.center[axis] / 65536.0f;
					input.emission_direction = source.emission_direction;
					input.receiver = { v.x / 65536.0f, v.y / 65536.0f, v.z / 65536.0f };
					input.normal = vertex_normals[vertex].normal;
					float length = 0;
					for (const auto axis : input.normal) length += axis * axis;
					length = std::sqrt(length);
					if (length)
						for (auto &axis : input.normal) axis /= length;
					input.light_color = source.material.rgb;
					input.material_ambient = { 0.3f, 0.3f, 0.3f };
					input.material_diffuse = { 0.7f, 0.7f, 0.7f };
					input.attenuation = { 0, 0, 0, 0.5f, 0.05f, 0.005f, float(222 * 65536), source.material.brightness };
					input.self = source.material.ambient;
					color_sum.add(xl_compat::geometry_source_color(input), input.self);
				}
				const auto color = color_sum.finish();
				for (const auto channel : color) require(std::isfinite(channel) && channel >= 0 && channel <= 1, "invalid ambient color");
				ambient_report["vertices"].push_back(color);
				active_report["vertices"].push_back(active_entries);
			}
			const auto runtime_visibility = xl_compat::compile_classic_vertex_visibility(sources, optics);
			require(runtime_visibility == vertex_visibility, "runtime source visibility differs from assembled audit");
			xl_compat::ClassicAmbientLighting ambient(sources, optics);
			const xl_compat::ClassicAmbientProfile profile;
			const auto baseline = ambient.gather(profile);
			if (find_arg_value(argc, argv, "-package-audit-check-ownership"))
				for (std::size_t vertex = 0; vertex < baseline.size(); ++vertex) {
					xl_compat::Rgb color;
					require(xl_classic_lighting_color(static_cast<int>(vertex), color.data()), "level load did not initialize classic ambient state");
					require(color == baseline[vertex], "level-owned ambient color differs from audit");
					std::int32_t r = -1, g = -1, b = -1;
					xl_preview_vertex(static_cast<int>(vertex), 65536, &r, &g, &b);
					require(r == static_cast<std::int32_t>(color[0] * 65536) && g == static_cast<std::int32_t>(color[1] * 65536) && b == static_cast<std::int32_t>(color[2] * 65536), "classic renderer hook did not consume ambient colors");
				}
			require(nlohmann::json(baseline) == ambient_report["vertices"], "owned ambient state differs from assembled audit");
			require(ambient.gather(profile) == baseline, "ambient gather mutates stored state");
			for (std::size_t i = 0; i < sources.size(); ++i)
				if (sources[i].variable()) ambient.set_source_state(i, false, 0);
			const auto without_variable = ambient.gather(profile);
			std::size_t changed_vertices = 0;
			for (std::size_t vertex = 0; vertex < baseline.size(); ++vertex)
				if (baseline[vertex] != without_variable[vertex]) ++changed_vertices;
			require(changed_vertices > 0, "disabling original variable lights has no ambient effect");
			for (std::size_t i = 0; i < sources.size(); ++i) ambient.set_source_state(i, false, 0);
			for (const auto &color : ambient.gather(profile)) require(color == xl_compat::Rgb{}, "disabled source leaked ambient color");
			for (std::size_t i = 0; i < sources.size(); ++i) ambient.set_source_state(i, true, 81920000);
			for (const auto &color : ambient.gather(profile)) require(color == xl_compat::Rgb{}, "ambient gather overwrote stored bucket distance");
			for (std::size_t i = 0; i < sources.size(); ++i) ambient.set_source_state(i, true, 0);
			require(ambient.gather(profile) == baseline, "restoring source state changed ambient colors");
			ambient.clear();
			require(ambient.gather(profile).empty(), "cleared ambient state retained a level");
			report["experimental_ambient_state"] = { { "qualified", false }, { "variable_toggle_changed_vertices", changed_vertices }, { "restored_exactly", bool(true) }, { "release_empty", bool(true) } };
			// Own-face admission bypasses visibility even for variable sources
			const std::vector<std::vector<bool>> hidden(vertex_visibility.size(), std::vector<bool>(static_cast<std::size_t>(Highest_vertex_index) + 1, false));
			for (const auto &source : sources) {
				const int vertex = Segments[source.segment].verts[Side_to_verts_int[source.side][0]];
				const auto nearest = xl_compat::classic_nearest_vertex_lights(vertex, { source }, hidden);
				require(nearest.size() == 1 && nearest[0].index == 0 && nearest[0].distance == 0, "own-face light lost with hidden visibility");
			}
			report["variable_geometry_light_count"] = std::count_if(sources.begin(), sources.end(), [](const auto &source) { return source.variable(); });
			for (std::size_t i = 1; i < sources.size(); ++i) {
				const auto &previous = sources[i - 1], &current = sources[i];
				require(!previous.variable() || current.variable(), "static light follows variable light");
				if (previous.variable() == current.variable())
					require(previous.segment < current.segment || (previous.segment == current.segment && previous.side < current.side), "geometry light identity order differs");
			}
		}
		char display_name[256];
		level_metadata_choose_level_display_name(Gamesave_current_filename, Current_level_name, display_name, sizeof(display_name));
		report["display_level_name"] = display_name;
		report["raw_level_title_bytes"] = nlohmann::json::array();
		for (const unsigned char *p = reinterpret_cast<const unsigned char *>(Current_level_name); *p; ++p)
			report["raw_level_title_bytes"].push_back(*p);
		report["native_d1_assets"] = bool(d1_in_d2_has_native_assets());
		report["boss_special_samples"] = { { "see_loop", digi_xlat_sound(SOUND_BOSS_SHARE_SEE) }, { "death", digi_xlat_sound(SOUND_BOSS_SHARE_DIE) } };
		for (const char *name : { "boss01", "boss02", "robot01", "robot02" }) {
			char lookup[16];
			std::strcpy(lookup, name);
			report["sound_registry"][name] = piggy_find_sound(lookup);
		}
		report["evidence"] = "native startup inventory; no normal-input completion or presentation claim";
		{
			// Synthetic screen coordinates test loaded geometry wiring only
			const auto forward_basis = xl_compat::geometry_camera_basis({ 0, 0, 65536 });
			require(forward_basis == std::array<xl_compat::FixedLightPoint, 3>{ xl_compat::FixedLightPoint{ 65536, 0, 0 }, { 0, 65536, 0 }, { 0, 0, 65536 } }, "forward camera basis differs");
			for (const int vertical : { -65536, 65536 }) {
				const auto basis = xl_compat::geometry_camera_basis({ 0, vertical, 0 });
				require(basis[0] == xl_compat::FixedLightPoint{ 65536, 0, 0 } && basis[1] == xl_compat::FixedLightPoint{ 0, 0, -65536 } && basis[2] == xl_compat::FixedLightPoint{ 0, vertical, 0 }, "vertical camera basis differs from reference special case");
			}
			std::vector<xl_compat::GeometryScreenPoint> projected(static_cast<std::size_t>(Highest_vertex_index) + 1, { 512, 512, true });
			require(xl_compat::classic_visible_segments(0, projected, 1024, 1024, 10).empty(), "fully behind mine admitted by visibility adapter");
			for (auto &point : projected) point.behind = false;
			const auto visible = xl_compat::classic_visible_segments(0, projected, 1024, 1024, 10);
			require(!visible.empty() && visible.front() == 0, "front-facing start omitted by visibility adapter");
			report["synthetic_visibility_adapter"] = { { "all_behind_rejected", true }, { "all_front_candidate_count", visible.size() }, { "camera_qualified", false } };
			projected.pop_back();
			bool rejected = false;
			try {
				(void) xl_compat::classic_visible_segments(0, projected, 1024, 1024, 10);
			} catch (const std::runtime_error &) {
				rejected = true;
			}
			require(rejected, "incomplete projected vertex table admitted");
			auto camera = xl_compat::classic_default_lighting_camera();
			camera.position = xl_compat::compile_classic_lighting_side(0, 0).segment_center;
			camera.forward = { 65536, 131072, 196608 };
			const auto camera_visible = xl_compat::classic_camera_visible_segments(0, camera);
			require(!camera_visible.empty() && camera_visible.front() == 0, "controlled camera lost its containing segment");
			report["controlled_visibility_camera"] = { { "start_segment", 0 }, { "forward_fixed", camera.forward }, { "position_fixed", camera.position }, { "projection_xy", camera.projection_xy }, { "visible_segments", camera_visible }, { "reference_qualified", false } };
			// Actual emitter at segment 0 side 0; same explicit experimental optics
			const auto light_views = xl_compat::classic_light_views(0, 0);
			require(bool(light_views), "controlled light source has no views");
			auto &light_report = report["controlled_light_visibility_views"];
			light_report = nlohmann::json::array();
			for (const auto &view : *light_views) {
				camera.position = view.position;
				camera.forward = view.forward;
				const auto segments = xl_compat::classic_camera_visible_segments(0, camera);
				require(!segments.empty() && segments.front() == 0, "source view lost its containing segment");
				light_report.push_back({ { "visible_segments", segments }, { "reference_qualified", false } });
			}
		}
		report["segments"] = nlohmann::json::array();
		std::array<unsigned, 3> compiled_side_counts{};
		std::array<unsigned, 2> lighting_split_counts{};
		unsigned changed_existing_splits = 0;
		float largest_raw_normal_delta = 0;
		unsigned raw_normal_count = 0;
		unsigned changed_fixed_normals = 0;
		std::array<unsigned, 2> xl_compiled_splits{};
		unsigned changed_compiled_splits = 0;
		unsigned light_portals = 0;
		for (int i = 0; i <= Highest_segment_index; ++i) {
			vms_vector center;
			compute_segment_center(&center, &Segments[i]);
			const auto point = [](const vms_vector &v) -> xl_compat::LightPoint { return { v.x / 65536.0f, v.y / 65536.0f, v.z / 65536.0f }; };
			nlohmann::json sides = nlohmann::json::array();
			for (int j = 0; j < 6; ++j) {
				const auto &side = Segments[i].sides[j];
				require(side.type >= SIDE_IS_QUAD && side.type <= SIDE_IS_TRI_13, "invalid compiled side type");
				++compiled_side_counts[side.type - SIDE_IS_QUAD];
				// XL's active CSide::Setup always triangulates four-corner sides
				// Probe the inherited split rule on a copy; keep collision geometry intact
				segment split_segment = Segments[i];
				add_side_as_2_triangles(&split_segment, j);
				const auto split_type = split_segment.sides[j].type;
				require(split_type == SIDE_IS_TRI_02 || split_type == SIDE_IS_TRI_13, "invalid lighting split type");
				++lighting_split_counts[split_type - SIDE_IS_TRI_02];
				if (side.type != SIDE_IS_QUAD && side.type != split_type) ++changed_existing_splits;
				xl_compat::GeometryLightFace face;
				for (int k = 0; k < 4; ++k) face.corners[k] = point(Vertices[Segments[i].verts[Side_to_verts_int[j][k]]]);
				int count, indices[6] = {};
				create_all_vertnum_lists(&count, indices, i, j);
				face.triangle_count = count;
				// Match XL SetupVertexList/DistToFace: a quad tests only corners 0,1,2
				for (int k = 0; k < count; ++k) {
					for (int c = 0; c < 3; ++c) face.triangles[k][c] = indices[k * 3 + c];
#ifdef COMPACT_SEGS
					vms_vector normal;
					get_side_normal(&Segments[i], j, k, &normal);
					face.normals[k] = point(normal);
#else
					face.normals[k] = point(side.normals[k]);
#endif
				}
				const auto hit = xl_compat::geometry_light_point(face, point(center));
				for (float coordinate : hit) require(std::isfinite(coordinate), "nonfinite compiled face light point");
				sides.push_back({ { "child", Segments[i].children[j] }, { "wall", side.wall_num }, { "texture", side.tmap_num }, { "overlay", side.tmap_num2 }, { "compiled_type", side.type }, { "corners", face.corners }, { "triangles", face.triangles }, { "triangle_count", face.triangle_count }, { "normals", face.normals }, { "segment_center_hit", hit } });
				sides.back()["lighting_split_candidate"] = split_type;
				const bool light_portal = xl_compat::classic_light_portal_passable(i, j);
				if (light_portal) ++light_portals;
				sides.back()["light_portal_eligible"] = light_portal;
				const auto compiled = xl_compat::compile_classic_lighting_side(i, j);
				++xl_compiled_splits[compiled.split_13 ? 1 : 0];
				if ((split_type == SIDE_IS_TRI_13) != compiled.split_13) ++changed_compiled_splits;
				const xl_compat::LightPoint compiled_receiver{ compiled.segment_center[0] / 65536.0f, compiled.segment_center[1] / 65536.0f, compiled.segment_center[2] / 65536.0f };
				const auto compiled_hit = xl_compat::geometry_light_point(compiled.face, compiled_receiver);
				for (auto coordinate : compiled_hit) require(std::isfinite(coordinate), "nonfinite XL compiled side hit point");
				sides.back()["xl_lighting_geometry"] = { { "triangles", compiled.face.triangles }, { "normals", compiled.face.normals }, { "average_normal", compiled.average_normal }, { "fixed_normals", compiled.fixed_normals }, { "center_fixed", compiled.center }, { "segment_center_fixed", compiled.segment_center }, { "segment_center_hit", compiled_hit } };
				std::array<xl_compat::LightPoint, 2> raw_normals;
				std::array<xl_compat::LightPoint, 2> ordered_normals;
				std::array<xl_compat::FixedLightPoint, 2> xl_fixed_normals;
				const std::array<std::array<unsigned, 3>, 2> normal_corners = split_type == SIDE_IS_TRI_02
				                                                                  ? std::array<std::array<unsigned, 3>, 2>{ { { 0, 1, 2 }, { 0, 2, 3 } } }
				                                                                  : std::array<std::array<unsigned, 3>, 2>{ { { 0, 1, 3 }, { 1, 2, 3 } } };
				for (unsigned k = 0; k < 2; ++k) {
					const auto &c = normal_corners[k];
					std::array<unsigned, 3> normal_order{ c[0], c[1], c[2] };
					const auto vertex_id = [&](unsigned corner) { return Segments[i].verts[Side_to_verts_int[j][corner]]; };
					std::array<unsigned, 3> winding{ 0, 1, 2 };
					if (IS_CHILD(Segments[i].children[j]))
						for (unsigned a = 1; a < 3; ++a)
							for (unsigned b = 0; b < a; ++b)
								if (vertex_id(normal_order[b]) > vertex_id(normal_order[a])) {
									std::swap(normal_order[a], normal_order[b]);
									std::swap(winding[a], winding[b]);
								}
					std::array<xl_compat::FixedLightPoint, 3> fixed_corners;
					for (unsigned a = 0; a < 3; ++a) {
						const auto &v = Vertices[vertex_id(normal_order[a])];
						fixed_corners[a] = { v.x, v.y, v.z };
					}
					xl_fixed_normals[k] = xl_compat::geometry_fixed_normal(fixed_corners[0], fixed_corners[1], fixed_corners[2]);
					if ((winding[0] + 3) % 4 == winding[1] || (winding[1] + 3) % 4 == winding[2])
						for (auto &value : xl_fixed_normals[k]) value = -value;
					raw_normals[k] = xl_compat::geometry_triangle_normal(face.corners[c[0]], face.corners[c[1]], face.corners[c[2]]);
					ordered_normals[k] = raw_normals[k];
					if (IS_CHILD(Segments[i].children[j])) {
						std::array<std::uint16_t, 3> vertex_ids;
						for (unsigned v = 0; v < 3; ++v) vertex_ids[v] = static_cast<std::uint16_t>(Segments[i].verts[Side_to_verts_int[j][c[v]]]);
						ordered_normals[k] = xl_compat::geometry_portal_normal({ face.corners[c[0]], face.corners[c[1]], face.corners[c[2]] }, vertex_ids);
					}
					for (float coordinate : raw_normals[k]) require(std::isfinite(coordinate), "nonfinite float triangle normal");
					++raw_normal_count;
#ifndef COMPACT_SEGS
					const auto fixed = point(split_segment.sides[j].normals[k]);
					const auto &engine_normal = split_segment.sides[j].normals[k];
					if (xl_fixed_normals[k] != xl_compat::FixedLightPoint{ engine_normal.x, engine_normal.y, engine_normal.z }) ++changed_fixed_normals;
					for (unsigned axis = 0; axis < 3; ++axis)
						largest_raw_normal_delta = (std::max) (largest_raw_normal_delta, std::abs(raw_normals[k][axis] - fixed[axis]));
#endif
				}
				// Winding-order diagnostic, before XL portal sorting and FixNormals
				sides.back()["raw_float_triangle_normals"] = raw_normals;
				sides.back()["xl_fixed_triangle_normals_candidate"] = xl_fixed_normals;
				sides.back()["ordered_float_triangle_normals_before_fix"] = ordered_normals;
#ifndef COMPACT_SEGS
				std::array<xl_compat::FixedLightPoint, 2> fixed_normals;
				for (unsigned k = 0; k < 2; ++k) {
					const auto &n = split_segment.sides[j].normals[k];
					fixed_normals[k] = { n.x, n.y, n.z };
				}
				const auto &plane_vertex = Vertices[split_segment.verts[Side_to_verts_int[j][0]]];
				const auto lighting_center = xl_compat::geometry_side_center(face.corners, fixed_normals, { plane_vertex.x, plane_vertex.y, plane_vertex.z });
				sides.back()["lighting_center_candidate_fixed"] = lighting_center;
				vms_vector toward_center{ center.x - lighting_center[0], center.y - lighting_center[1], center.z - lighting_center[2] };
				auto float_direction = point(toward_center);
				float squared = 0;
				for (auto value : float_direction) squared += value * value;
				if (squared)
					for (auto &value : float_direction) value /= std::sqrt(squared);
				vm_vec_normalize(&toward_center);
				const auto oriented = xl_compat::geometry_orient_normals(ordered_normals, fixed_normals, float_direction,
				                                                         { toward_center.x, toward_center.y, toward_center.z });
				sides.back()["oriented_float_normals_candidate"] = oriented;
#endif
			}
			report["segments"].push_back({ { "index", i }, { "sides", sides } });
		}
		report["compiled_side_counts_quad_tri02_tri13"] = compiled_side_counts;
		report["lighting_split_candidate_counts_tri02_tri13"] = lighting_split_counts;
		report["lighting_changed_existing_splits"] = changed_existing_splits;
		report["xl_compiled_splits_tri02_tri13"] = xl_compiled_splits;
		report["xl_compiled_splits_differing_from_engine"] = changed_compiled_splits;
		report["eligible_directed_light_portals"] = light_portals;
		report["raw_float_triangle_normal_count"] = raw_normal_count;
		report["xl_fixed_normals_differing_from_engine"] = changed_fixed_normals;
#ifndef COMPACT_SEGS
		report["raw_float_vs_fixed_normal_max_component_delta"] = largest_raw_normal_delta;
#endif
		report["objects"] = nlohmann::json::array();
		for (int i = 0; i <= Highest_object_index; ++i) {
			const auto &o = Objects[i];
			if (o.type == OBJ_NONE) continue;
			nlohmann::json entry = { { "index", i }, { "type", o.type }, { "id", o.id }, { "segment", o.segnum }, { "contains_type", o.contains_type }, { "contains_id", o.contains_id }, { "contains_count", o.contains_count } };
			if (o.type == OBJ_ROBOT) {
				const auto &robot = Robot_info[o.id];
				entry["boss"] = robot.boss_flag;
				entry["see_sound"] = robot.see_sound;
				entry["attack_sound"] = robot.attack_sound;
				entry["see_sample"] = digi_xlat_sound(robot.see_sound);
				entry["attack_sample"] = digi_xlat_sound(robot.attack_sound);
				const char *see_name = piggy_sound_duration_name(digi_xlat_sound(robot.see_sound));
				const char *attack_name = piggy_sound_duration_name(digi_xlat_sound(robot.attack_sound));
				entry["duration_catalog_see_name"] = see_name ? see_name : "";
				entry["duration_catalog_attack_name"] = attack_name ? attack_name : "";
			}
			report["objects"].push_back(entry);
		}
		report["walls"] = nlohmann::json::array();
		for (int i = 0; i < Num_walls; ++i) {
			const auto &w = Walls[i];
			report["walls"].push_back({ { "index", i }, { "segment", w.segnum }, { "side", w.sidenum }, { "type", w.type }, { "keys", w.keys }, { "flags", w.flags }, { "trigger", w.trigger } });
		}
		report["triggers"] = nlohmann::json::array();
		for (int i = 0; i < Num_triggers; ++i) {
			const auto &t = Triggers[i];
			nlohmann::json entry = { { "index", i }, { "type", t.type }, { "flags", t.flags }, { "links", nlohmann::json::array() } };
			for (int j = 0; j < t.num_links; ++j) entry["links"].push_back({ { "segment", t.seg[j] }, { "side", t.side[j] } });
			report["triggers"].push_back(entry);
		}
		const char *output = find_arg_value(argc, argv, "-package-audit");
		if (find_arg_value(argc, argv, "-package-audit-check-ownership")) {
			require(companions.brightness && companions.colors, "ownership probe requires both original companions");
			const auto original_segments = std::vector<segment>(Segments, Segments + Highest_segment_index + 1);
			std::vector<xl_compat::Rgb> original_colors(static_cast<std::size_t>(Highest_vertex_index) + 1);
			for (std::size_t vertex = 0; vertex < original_colors.size(); ++vertex)
				require(xl_classic_lighting_color(static_cast<int>(vertex), original_colors[vertex].data()), "missing ambient state before destruction probe");
			auto &destruction = report["experimental_light_destruction"];
			destruction = { { "qualified", bool(false) }, { "method", "real check_effect_blowup, forced player-owned impact; not normal-input shooting" }, { "sources", nlohmann::json::array() } };
			object impact{};
			impact.type = OBJ_WEAPON;
			impact.ctype.laser_info.parent_type = OBJ_PLAYER;
			impact.ctype.laser_info.parent_num = Players[Player_num].objnum;
			impact.ctype.laser_info.parent_signature = ConsoleObject->signature;
			for (const auto &light : xl_compat::compile_classic_geometry_lights(*companions.brightness, *companions.colors)) {
				if (!light.destructible) continue;
				vms_vector point;
				compute_center_point_on_side(&point, &Segments[light.segment], light.side);
				const int before = Segments[light.segment].sides[light.side].tmap_num2;
				require(check_effect_blowup(&Segments[light.segment], light.side, &point, &impact, 1), "original destructible source rejected player-owned impact");
				require(Light_subtracted[light.segment] & (1 << light.side), "destruction failed to mark subtracted light");
				destruction["sources"].push_back({ { "segment", light.segment }, { "side", light.side }, { "overlay_before", before }, { "overlay_after", Segments[light.segment].sides[light.side].tmap_num2 } });
			}
			std::size_t uvl_changes = 0, segment_light_changes = 0, ambient_changes = 0;
			for (int segment = 0; segment <= Highest_segment_index; ++segment) {
				if (Segments[segment].static_light != original_segments[segment].static_light) ++segment_light_changes;
				for (int side = 0; side < 6; ++side)
					for (int corner = 0; corner < 4; ++corner)
						if (Segments[segment].sides[side].uvls[corner].l != original_segments[segment].sides[side].uvls[corner].l) ++uvl_changes;
			}
			for (std::size_t vertex = 0; vertex < original_colors.size(); ++vertex) {
				xl_compat::Rgb color;
				require(xl_classic_lighting_color(static_cast<int>(vertex), color.data()), "destruction discarded ambient state");
				if (color != original_colors[vertex]) ++ambient_changes;
			}
			destruction["uvl_changes"] = uvl_changes;
			destruction["segment_light_changes"] = segment_light_changes;
			destruction["ambient_changes"] = ambient_changes;
			const auto destroyed_segments = std::vector<segment>(Segments, Segments + Highest_segment_index + 1);
			const auto destroyed_flags = std::vector<ubyte>(Light_subtracted, Light_subtracted + Highest_segment_index + 1);
			const auto saved_energy = Players[Player_num].energy;
			char destruction_save[] = "xl-classic-destruction.sav", destruction_description[] = "Classic lighting destruction";
			stop_time();
			require(state_save_all_sub(destruction_save, destruction_description), "classic destruction save failed");
			StartNewGame(1);
			for (std::size_t vertex = 0; vertex < original_colors.size(); ++vertex) {
				xl_compat::Rgb color;
				require(xl_classic_lighting_color(static_cast<int>(vertex), color.data()) && color == original_colors[vertex], "level restart did not restore initial ambient colors");
			}
			for (int segment = 0; segment <= Highest_segment_index; ++segment)
				require(!Light_subtracted[segment], "level restart retained destroyed light flags");
			destruction["restart_restored"] = bool(true);
			Players[Player_num].energy = 1;
			require(state_restore_all_sub(destruction_save, 0), "classic destruction restore failed");
			require(Players[Player_num].energy == saved_energy, "classic destruction restore lost player energy");
			require(Current_level_num == 1 && !std::strcmp(Current_mission_filename, "drgpit"), "classic destruction restore lost mission identity");
			require(static_cast<std::size_t>(Highest_segment_index) + 1 == destroyed_segments.size(), "classic restore changed segment count");
			for (int segment = 0; segment <= Highest_segment_index; ++segment) {
				require(Light_subtracted[segment] == destroyed_flags[segment], "classic restore lost light subtraction flags");
				require(Segments[segment].static_light == destroyed_segments[segment].static_light, "classic restore changed segment light");
				for (int side = 0; side < 6; ++side) {
					const auto &expected = destroyed_segments[segment].sides[side], &actual = Segments[segment].sides[side];
					require(actual.tmap_num == expected.tmap_num && actual.tmap_num2 == expected.tmap_num2, "classic restore lost destroyed texture state");
					for (int corner = 0; corner < 4; ++corner)
						require(actual.uvls[corner].l == expected.uvls[corner].l, "classic restore changed baked face light");
				}
			}
			for (std::size_t vertex = 0; vertex < original_colors.size(); ++vertex) {
				xl_compat::Rgb color;
				require(xl_classic_lighting_color(static_cast<int>(vertex), color.data()) && color == original_colors[vertex], "save restore lost method-0 setup ambient colors");
			}
			destruction["save_restore_verified"] = bool(true);
			// Subsequent ownership probes use the original undamaged level
			StartNewGame(1);
			require(xl_preview_load_level("__missing_classic_lighting_test__.rdl") != 0, "missing level unexpectedly loaded");
			require(!xl_classic_lighting_vertex_count(), "failed level replacement retained classic lighting");
			require(xl_classic_lighting_prepare(Gamesave_current_filename), "cannot restore lighting after failed precommit load");
			std::string upper = Gamesave_current_filename;
			for (auto &c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
			const auto folded = xl_compat::load_mission_lighting_companions(upper);
			require(folded.brightness && *folded.brightness == *companions.brightness && folded.colors, "case-correct companion resolution differs");
			const auto shadow = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "companion-shadow";
			std::filesystem::create_directories(shadow);
			const auto name = std::filesystem::path(Gamesave_current_filename).replace_extension(".lgt").filename();
			std::ofstream(shadow / name, std::ios::binary).write("bad", 3);
			require(PHYSFS_addToSearchPath(shadow.string().c_str(), 0), "cannot mount companion shadow fixture");
			require(!xl_classic_lighting_prepare(Gamesave_current_filename), "classic lighting accepted foreign companion shadow");
			float failed_color[3];
			require(!xl_classic_lighting_vertex_count() && !xl_classic_lighting_color(0, failed_color), "failed preparation retained previous classic lighting");
			bool rejected = false;
			try {
				xl_compat::load_mission_lighting_companions(Gamesave_current_filename);
			} catch (const std::exception &error) {
				rejected = std::string(error.what()).find("shadowed outside") != std::string::npos;
			}
			require(PHYSFS_removeFromSearchPath(shadow.string().c_str()), "cannot unmount companion shadow fixture");
			require(rejected, "foreign lighting shadow was not rejected with ownership diagnostic");
			const auto restored = xl_compat::load_mission_lighting_companions(Gamesave_current_filename);
			require(restored.brightness && *restored.brightness == *companions.brightness, "owned lighting did not recover after shadow removal");
			require(xl_classic_lighting_prepare(Gamesave_current_filename) && xl_classic_lighting_color(0, failed_color), "classic lighting did not recover after shadow removal");
			free_mission();
			float released_color[3];
			require(!xl_classic_lighting_vertex_count() && !xl_classic_lighting_color(0, released_color), "mission release retained classic ambient state");
			const auto cleared = xl_compat::load_mission_lighting_companions(upper);
			require(cleared.archive.empty() && !cleared.brightness && !cleared.colors, "lighting resolver retained a released mission");
			report["lighting_companions"]["ownership_probe"] = "case lookup, foreign-shadow rejection, recovery and mission release passed";
		}
		require(output && *output, "package audit requires output path");
		std::ofstream stream(output);
		stream << report.dump(2) << '\n';
		require(bool(stream), "cannot write package audit");
		return 0;
	} catch (const std::exception &failure) {
		std::cerr << failure.what() << '\n';
		return 1;
	}
}
