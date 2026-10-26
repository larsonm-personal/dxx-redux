// Private native layout qualification; no gameplay or resource admission grant
static int format23_layout_test(int argc, char **argv)
{
	try {
		const auto path = find_arg_value(argc, argv, "-format23-test");
		const auto directory = find_arg_value(argc, argv, "-extra-dir");
		require(path && directory, "23/40 test requires original HOG and private directory");
		std::ifstream stream(path, std::ios::binary);
		require(bool(stream), "cannot open original 23/40 HOG");
		const xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
		const auto members = xl_compat::read_hog(bytes);
		const auto member = std::find_if(members.begin(), members.end(), [](const auto &entry) { return entry.name == "neptunxl.rl2"; });
		require(member != members.end(), "original Neptune level missing");
		const auto probe = xl_compat::prepare_serialization_probe_23_40(member->bytes);
		const auto &source = probe.source;
		write_bytes((std::filesystem::path(directory) / "format23.rl2").string().c_str(), probe.stock_level);
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(directory, error, sizeof(error)), error);
		require(load_base_mission(nullptr, error, sizeof(error)), error);
		require(load_level("format23.rl2") == 0, "stock loader rejected translated 23/40");
		require(!xl_preview_pilot_active(), "layout fixture incorrectly activated XL");
		require(Num_segments == source.segments.size() && Num_vertices == source.vertices.size() && Num_walls == source.walls.size() && Num_triggers == source.triggers.size(), "native 23/40 counts differ");
		require(source.level_name == Current_level_name, "native level name displaced");
		for (std::size_t i = 0; i < source.vertices.size(); ++i)
			require(Vertices[i].x == source.vertices[i][0] && Vertices[i].y == source.vertices[i][1] && Vertices[i].z == source.vertices[i][2], "native vertex differs");
		int station = 0;
		for (std::size_t i = 0; i < source.segments.size(); ++i) {
			const auto &expected = source.segments[i];
			// The native loader rebuilds station indices in segment order
			const int value = expected.function >= 1 && expected.function <= 4 ? station++ : expected.value;
			require(Segment2s[i].special == expected.function && Segment2s[i].matcen_num == expected.producer && Segment2s[i].value == value && Segment2s[i].static_light == expected.light, "native segment metadata or station mapping differs");
			for (int corner = 0; corner < 8; ++corner) require(Segments[i].verts[corner] == expected.vertices[corner], "native vertex reference differs");
			for (int side = 0; side < 6; ++side) {
				const auto &actual = Segments[i].sides[side];
				const auto &wanted = expected.sides[side];
				require(Segments[i].children[side] == expected.children[side] && actual.wall_num == wanted.wall, "native topology differs");
				if (wanted.texture < 0) continue;
				require(actual.tmap_num == wanted.texture && actual.tmap_num2 == wanted.overlay, "native texture reference differs");
				for (int corner = 0; corner < 4; ++corner)
					require(actual.uvls[corner].u == wanted.uvl[corner * 3] * 32 && actual.uvls[corner].v == wanted.uvl[corner * 3 + 1] * 32 && actual.uvls[corner].l == wanted.uvl[corner * 3 + 2] * 2, "native UVL differs");
			}
		}
		for (std::size_t i = 0; i < source.walls.size(); ++i) {
			const auto &expected = source.walls[i];
			const auto &actual = Walls[i];
			// Stock reconstructs reverse trigger links; serialized values may be stale
			int controller = -1;
			for (std::size_t t = 0; t < source.triggers.size(); ++t) {
				const auto &trigger = source.triggers[t];
				if (trigger.type == TT_MATCEN || trigger.type == TT_LIGHT_OFF || trigger.type == TT_LIGHT_ON) continue;
				for (const auto &target : trigger.targets)
					if (source.segments[target[0]].sides[target[1]].wall == int(i)) controller = int(t);
			}
			require(actual.segnum == expected.segment && actual.sidenum == expected.side && actual.hps == expected.hitpoints && actual.linked_wall == expected.linked_wall && actual.type == expected.type && actual.flags == expected.flags && actual.state == expected.state && (actual.trigger & 255) == expected.trigger && actual.clip_num == expected.clip && actual.keys == expected.keys && actual.controlling_trigger == controller && actual.cloak_value == expected.cloak, "native wall differs");
		}
		require(Highest_object_index + 1 == source.objects.size(), "native object count differs");
		for (std::size_t i = 0; i < source.objects.size(); ++i) {
			const auto &expected = source.objects[i];
			const auto &actual = Objects[i];
			require(actual.type == expected.type && actual.id == expected.id && actual.movement_type == expected.movement && actual.render_type == expected.render, "native object identity differs");
			require(actual.pos.x == expected.position[0] && actual.pos.y == expected.position[1] && actual.pos.z == expected.position[2], "native object position differs");
			// The loader repairs stale segment membership from the retained position
			require(actual.segnum == find_point_seg(&actual.pos, expected.segment), "native object membership differs from position");
			if (actual.segnum != expected.segment) std::cout << "23/40 object " << i << " membership repaired " << expected.segment << " -> " << actual.segnum << '\n';
		}
		for (std::size_t i = 0; i < source.triggers.size(); ++i) {
			const auto &expected = source.triggers[i];
			const auto &actual = Triggers[i];
			require(actual.type == expected.type && actual.flags == expected.flags && actual.num_links == expected.targets.size() && actual.value == expected.value && actual.time == expected.time, "native trigger differs");
			for (std::size_t target = 0; target < expected.targets.size(); ++target)
				require(actual.seg[target] == expected.targets[target][0] && actual.side[target] == expected.targets[target][1], "native trigger target differs");
		}
		require(load_level("d2leva-1.rl2") == 0 && !xl_preview_pilot_active(), "stock recovery after 23/40 translation failed");
		std::cout << "23/40 native layout: geometry, UVLs, walls, object identities/positions, triggers and stock recovery passed; resources and gameplay unqualified\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
