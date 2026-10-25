// Native startup inventory only; does not certify progression or rendered resources
#include "companion_resources.h"
#include "geometry.h"
#include "light_sources.h"
static int package_audit(int argc, char **argv)
{
	try {
		require(xl_compat::geometry_fixed_normal({ 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }) == xl_compat::FixedLightPoint{ 0, 0, 65536 }, "XL tiny-edge normal scaling failed");
		require(xl_compat::geometry_fixed_normal({ 0, 0, 0 }, { 1, 1, 0 }, { 1, 0, 0 }) == xl_compat::FixedLightPoint{ 0, 0, -65536 }, "XL reversed tiny-edge normal failed");
		require(xl_compat::geometry_fixed_normal({ 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 0 }) == xl_compat::FixedLightPoint{}, "XL degenerate fixed normal failed");
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
		const auto companions = xl_compat::load_mission_lighting_companions(Gamesave_current_filename);
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
			const auto sources = xl_compat::compile_classic_geometry_lights(*companions.brightness, *companions.colors);
			report["geometry_light_sources"] = nlohmann::json::array();
			for (const auto &source : sources)
				report["geometry_light_sources"].push_back({ { "segment", source.segment }, { "side", source.side }, { "created_texture", source.material.created_texture }, { "material_texture", source.material.material_texture }, { "brightness", source.material.brightness }, { "rgb", source.material.rgb }, { "ambient", source.material.ambient }, { "center_fixed", source.geometry.center }, { "emission_direction", source.emission_direction } });
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
		report["segments"] = nlohmann::json::array();
		std::array<unsigned, 3> compiled_side_counts{};
		std::array<unsigned, 2> lighting_split_counts{};
		unsigned changed_existing_splits = 0;
		float largest_raw_normal_delta = 0;
		unsigned raw_normal_count = 0;
		unsigned changed_fixed_normals = 0;
		std::array<unsigned, 2> xl_compiled_splits{};
		unsigned changed_compiled_splits = 0;
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
				const auto lighting_center = xl_compat::geometry_side_center(face.corners, fixed_normals);
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
			std::string upper = Gamesave_current_filename;
			for (auto &c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
			const auto folded = xl_compat::load_mission_lighting_companions(upper);
			require(folded.brightness && *folded.brightness == *companions.brightness && folded.colors, "case-correct companion resolution differs");
			const auto shadow = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "companion-shadow";
			std::filesystem::create_directories(shadow);
			const auto name = std::filesystem::path(Gamesave_current_filename).replace_extension(".lgt").filename();
			std::ofstream(shadow / name, std::ios::binary).write("bad", 3);
			require(PHYSFS_addToSearchPath(shadow.string().c_str(), 0), "cannot mount companion shadow fixture");
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
			free_mission();
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
