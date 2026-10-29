// Controlled runtime comparison of equivalent authored geometry; no navigation
#include "resource_validation.h"
#include "repair.h"

static xl_compat::Bytes runtime_layout_read(const char *name)
{
	std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFS_openRead(name), PHYSFS_close);
	require(file && PHYSFS_fileLength(file.get()) > 0 && PHYSFS_fileLength(file.get()) < 1024 * 1024, "Missing bounded runtime layout fixture");
	xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file.get())));
	require(PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) == static_cast<PHYSFS_sint64>(bytes.size()), "Incomplete runtime layout fixture");
	return bytes;
}

static std::vector<int> runtime_layout_geometry(const xl_compat::Inventory &source)
{
	require(xl_level_extensions_active() && xl_source_level_version() == source.level_version && xl_source_game_version() == 40, "Normal loader did not retain XL layout ownership");
	require(Num_segments == source.segments.size() && Num_vertices == source.vertices.size() && Num_walls == source.walls.size() && Num_triggers == source.triggers.size(), "Modern native counts differ");
	require(source.level_name == Current_level_name, "Modern game header displaced level name");
	std::vector<int> geometry;
	for (std::size_t i = 0; i < source.vertices.size(); ++i) {
		require(Vertices[i].x == source.vertices[i][0] && Vertices[i].y == source.vertices[i][1] && Vertices[i].z == source.vertices[i][2], "Modern native vertex changed");
		geometry.insert(geometry.end(), { Vertices[i].x, Vertices[i].y, Vertices[i].z });
	}
	int station = 0;
	for (std::size_t i = 0; i < source.segments.size(); ++i) {
		const auto &expected = source.segments[i];
		const bool special = expected.function >= 1 && expected.function <= 4;
		const int value = special ? station++ : source.level_version >= 25 ? 0
		                                                                   : expected.value;
		require(Segment2s[i].special == expected.function && Segment2s[i].value == value && Segment2s[i].static_light == expected.light, "Modern station normalization or metadata differs");
		require(Segment2s[i].matcen_num == (source.level_version >= 25 ? -1 : expected.producer), "Inert wide producer aliased native robot assignment");
		require((Segment2s[i].s2_flags & ~3) == (expected.flags & ~3), "Modern segment flags lost");
		require(bool(xl_preview_no_damage_segment(static_cast<int>(i))) == bool(expected.properties & xl_compat::segment_no_damage), "Owned modern properties were discarded");
		vms_vector center;
		compute_segment_center(&center, &Segments[i]);
		require(find_point_seg(&center, static_cast<int>(i)) == static_cast<int>(i), "Modern compiled collision planes lost segment center");
		for (int vertex = 0; vertex < 8; ++vertex) require(Segments[i].verts[vertex] == expected.vertices[vertex], "Modern vertex reference changed");
		for (int side = 0; side < 6; ++side) {
			const auto &actual = Segments[i].sides[side];
			const auto &wanted = expected.sides[side];
			require(Segments[i].children[side] == expected.children[side] && actual.wall_num == wanted.wall, "Modern portal topology changed");
			geometry.insert(geometry.end(), { actual.type, actual.wall_num, Segments[i].children[side] });
#ifndef COMPACT_SEGS
			for (const auto &normal : actual.normals) geometry.insert(geometry.end(), { normal.x, normal.y, normal.z });
#endif
			if (wanted.texture < 0) continue;
			require(actual.tmap_num == wanted.texture && actual.tmap_num2 == wanted.overlay, "Modern texture/orientation changed");
			geometry.insert(geometry.end(), { actual.tmap_num, actual.tmap_num2 });
			for (int corner = 0; corner < 4; ++corner) {
				const auto &uvl = actual.uvls[corner];
				require(uvl.u == wanted.uvl[corner * 3] * 32 && uvl.v == wanted.uvl[corner * 3 + 1] * 32 && uvl.l == wanted.uvl[corner * 3 + 2] * 2, "Modern UVL changed");
				geometry.insert(geometry.end(), { uvl.u, uvl.v, uvl.l });
			}
		}
	}
	for (std::size_t i = 0; i < source.walls.size(); ++i) {
		const auto &actual = Walls[i];
		const auto &expected = source.walls[i];
		require(actual.segnum == expected.segment && actual.sidenum == expected.side && actual.hps == expected.hitpoints && actual.linked_wall == expected.linked_wall && actual.type == expected.type && actual.flags == expected.flags && actual.state == expected.state && (actual.trigger & 255) == expected.trigger && actual.clip_num == expected.clip && actual.keys == expected.keys && actual.cloak_value == expected.cloak, "Modern wall record changed");
	}
	require(Highest_object_index + 1 == source.objects.size(), "Modern object record boundaries changed");
	for (std::size_t i = 0; i < source.objects.size(); ++i) {
		const auto &actual = Objects[i];
		const auto &expected = source.objects[i];
		require(actual.type == expected.type && actual.id == expected.id && actual.movement_type == expected.movement && actual.render_type == expected.render, "Modern object identity changed");
		require(actual.pos.x == expected.position[0] && actual.pos.y == expected.position[1] && actual.pos.z == expected.position[2] && actual.segnum == find_point_seg(&actual.pos, expected.segment), "Modern object position or membership changed");
	}
	for (std::size_t i = 0; i < source.triggers.size(); ++i) {
		const auto &actual = Triggers[i];
		const auto &expected = source.triggers[i];
		require(actual.type == expected.type && actual.flags == expected.flags && actual.value == expected.value && actual.time == expected.time && actual.num_links == expected.targets.size(), "Modern trigger record changed");
		for (std::size_t target = 0; target < expected.targets.size(); ++target)
			require(actual.seg[target] == expected.targets[target][0] && actual.side[target] == expected.targets[target][1], "Modern trigger target changed");
	}
	return geometry;
}

static int runtime_layout_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		std::vector<int> baseline;
		std::uint64_t previous_identity = 0;
		int ordinal = 0;
		for (int version : { 23, 25, 27 }) {
			++ordinal;
			require(load_base_mission("rtlayout", error, sizeof(error)), error);
			const auto filename = "rt" + std::to_string(version) + ".rl2";
			const auto source = xl_compat::inspect_level(runtime_layout_read(filename.c_str()));
			require(xl_level_supported(filename.c_str(), 0), xl_level_rejection());
			require(!xl_level_supported(filename.c_str(), 1), "Modern layout bypassed multiplayer admission");
			require(!load_level(filename.c_str()), "Modern normal load failed");
			const auto geometry = runtime_layout_geometry(source);
			if (version == 23) baseline = geometry;
			else require(geometry == baseline, "Equivalent layouts changed compiled normals, topology, textures or UVLs");
			for (int texture = 0; texture < 910; ++texture) {
				int brightness = -1;
				require(xl_prepared_texture_brightness(texture, &brightness) && brightness == (texture == 302 ? (version - 22) * 16384 : 0), "Modern LGT ownership/precedence differs");
			}
			for (std::size_t vertex = 0; vertex < source.vertices.size(); ++vertex) colored(source, static_cast<int>(vertex));
			require_owned_light_records(source);
			std::strcpy(Players[Player_num].callsign, "XLTest");
			input_demo_set_skip_level_intro(1);
			StartNewGame(ordinal);
			Game_suspended = SUSP_ROBOTS;
			require(xl_source_level_version() == version && !std::strcmp(Gamesave_current_filename, filename.c_str()), "Normal game startup lost modern source");
			int repair = -1, protected_room = -1;
			for (std::size_t i = 0; i < source.segments.size(); ++i) {
				if (source.segments[i].function == 2) repair = static_cast<int>(i);
				if (source.segments[i].properties & xl_compat::segment_no_damage) protected_room = static_cast<int>(i);
			}
			require(repair >= 0 && protected_room >= 0 && xl_repair_segment(repair), "Modern repair/property fixture not active");
			obj_relink(ConsoleObject - Objects, repair);
			compute_segment_center(&ConsoleObject->pos, &Segments[repair]);
			ConsoleObject->movement_type = MT_NONE;
			ConsoleObject->control_type = CT_NONE;
			FrameTime = F1_0 / 4;
			Players[Player_num].shields = i2f(50);
			object_move_one(ConsoleObject);
			require(Players[Player_num].shields == i2f(50) + 11 * F1_0 / 4, "Modern actual repair frame used the Redux rate");
			obj_relink(ConsoleObject - Objects, protected_room);
			compute_segment_center(&ConsoleObject->pos, &Segments[protected_room]);
			Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
			const auto shields = Players[Player_num].shields;
			apply_damage_to_player(ConsoleObject, nullptr, i2f(10), 0);
			require(Players[Player_num].shields == shields, "Modern no-damage property lost gameplay behavior");
			require(!source.light_indices.empty(), "Missing modern changing-light fixture");
			const auto light = source.light_indices.front();
			require(subtract_light(light[0], light[1]) == 1, "Modern source light did not change");
			const auto earned = light_snapshot();
			const auto identity = xl_level_trigger_identity();
			require(identity && identity != previous_identity, "Modern layout not bound to save identity");
			previous_identity = identity;
			for (const char *bad : { "badside.rl2", "badcorn.rl2", "badfog.rl2" }) {
				const auto position = ConsoleObject->pos;
				require(!xl_level_supported(bad, 0) && load_level(bad) != 0, "Malformed or unsupported modern geometry/fog admitted");
				require(xl_level_trigger_identity() == identity && same_light_snapshot(earned, light_snapshot()) && !std::memcmp(&position, &ConsoleObject->pos, sizeof(position)) && Players[Player_num].shields == shields, "Rejected modern load altered active earned state");
			}
			char save[] = "rtlayout.sav", description[] = "XL runtime layout";
			stop_time();
			require(state_save_all_sub(save, description), "Modern disk save failed");
			auto *trailer = PHYSFS_openWrite("rtlayout-state.bin");
			require(trailer && xl_wall_trigger_write(trailer) && PHYSFS_close(trailer), "Modern save trailer capture failed");
			if (version == 27) {
				require(!load_level("fg27.rl2") && xl_level_trigger_identity() != identity, "Changed authored fog did not change source identity");
				const auto fresh = light_snapshot();
				trailer = PHYSFS_openRead("rtlayout-state.bin");
				require(trailer && !xl_wall_trigger_read(trailer) && PHYSFS_close(trailer) && same_light_snapshot(fresh, light_snapshot()), "Foreign fog identity trailer changed live state");
			}
			require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Modern-to-stock transition leaked runtime");
			int brightness = 0;
			require(!xl_prepared_texture_brightness(302, &brightness) && !xl_repair_segment(repair) && !xl_preview_no_damage_segment(protected_room), "Modern resources or properties leaked into stock");
			require(state_restore_all_sub(save, 0) && xl_source_level_version() == version && xl_level_trigger_identity() == identity && same_light_snapshot(earned, light_snapshot()) && Players[Player_num].shields == shields, "Modern disk-through-stock restore lost source or earned state");
			require(add_light(light[0], light[1]) == 1, "Restored modern light cannot be enabled");
			start_time();
			std::cout << version << "/40 runtime layout: geometry/normals/UVLs, objects/walls/triggers, resources, repair/protection, rejection and disk-through-stock passed\n";
		}
		std::cout << "Canonical modern runtime layouts passed; original missions remain independently gated\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "Modern runtime layout failed: " << e.what() << '\n';
		return 1;
	}
}
