#include "../tests/changing_light_fixture.h"

static std::vector<xl_light_segment_state> light_snapshot()
{
	std::vector<xl_light_segment_state> result(xl_light_segment_count());
	for (std::size_t i = 0; i < result.size(); ++i)
		require(xl_light_segment_get(static_cast<int>(i), &result[i]), "Cannot capture actual light consumers");
	return result;
}
static bool same_light_snapshot(const std::vector<xl_light_segment_state> &a, const std::vector<xl_light_segment_state> &b)
{
	return a.size() == b.size() && (a.empty() || !std::memcmp(a.data(), b.data(), a.size() * sizeof(a[0])));
}

static void light_segment_graph(const xl_compat::Inventory &source)
{
	require(Num_segments > 37, "Light graph needs enough original segments");
	auto sample = source;
	sample.light_indices.clear();
	sample.delta_lights.clear();
	xl_changing_light_bind(&sample);
	const std::vector<segment> original(Segments, Segments + Num_segments);
	const auto texture0 = TmapInfo[0].lighting, texture1 = TmapInfo[1].lighting;
	TmapInfo[0].lighting = 0;
	TmapInfo[1].lighting = F1_0 / 8;
	for (int branches : { 2, 5 })
		for (int direction : { -1, 1 }) {
			for (int s = 0; s < Num_segments; ++s) {
				for (int v = 0; v < 8; ++v) Segments[s].verts[v] = original[0].verts[v];
				for (int side = 0; side < 6; ++side) {
					Segments[s].children[side] = -1;
					Segments[s].sides[side].wall_num = -1;
					Segments[s].sides[side].tmap_num = 1;
					Segments[s].sides[side].tmap_num2 = 0;
				}
				Segment2s[s].static_light = F1_0 / 2;
			}
			for (int b = 0; b < branches; ++b) {
				Segments[0].children[b] = static_cast<short>(b + 1);
				for (int side = 0; side < 6; ++side)
					Segments[b + 1].children[side] = static_cast<short>(6 + b * 6 + side);
			}
			// A cycle and a repeated child must not apply light twice
			Segments[1].children[0] = 0;
			Segments[2].children[0] = 1;
			// A third-depth destination must remain unchanged
			Segments[7].children[0] = 37;
			require(xl_changing_light(0, 5, direction), "Segment graph selected stock lighting");
			int changed = 0;
			for (int s = 0; s < Num_segments; ++s) {
				const bool visited = s <= branches || (s >= 6 && s < 6 + branches * 6 && s != 6 && s != 12);
				const auto expected = visited ? (direction < 0 ? 0 : F1_0 / 2 + F1_0 - 1) : F1_0 / 2;
				require(Segment2s[s].static_light == expected, "Segment light lost depth, deduplication, clamp or large visited set");
				changed += visited;
			}
			std::cout << "Segment-light graph: " << changed << " unique segments, direction " << direction << ", depth and cycle controls passed\n";
		}
	std::copy(original.begin(), original.end(), Segments);
	TmapInfo[0].lighting = texture0;
	TmapInfo[1].lighting = texture1;
	xl_changing_light_clear();
}

static void light_reference_samples(const xl_compat::Inventory &source, const std::filesystem::path &root)
{
	auto sample = source;
	sample.light_indices = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };
	std::ofstream out(root / "light-samples.txt");
	int matched = 0, corrected = 0;
	for (int count : { 0, 1, 2, 127, 128, 255, 8191 })
		for (int base : { 0, 1, 32768, F1_0, INT32_MAX - 100000 })
			for (int value : { 0, 1, 31, 255 })
				for (int direction : { -1, 0, 1, 17 }) {
					sample.light_indices[0][2] = count;
					sample.delta_lights.assign(count, { 0, 0, 0, value, value / 2, 0, value % 3 });
					xl_changing_light_bind(&sample);
					for (auto &uvl : Segments[0].sides[0].uvls) uvl.l = base;
					require(xl_changing_light(0, 0, direction), "Light sample selected Redux");
					bool defined = true;
					int64_t accumulated = 0;
					const int values[] = { value, value / 2, 0, value % 3 };
					for (int c = 0; c < 4; ++c) {
						const int64_t total = int64_t(direction) * values[c] * DL_SCALE * count;
						accumulated += total;
						const auto expected = std::max(INT64_C(0), std::min(int64_t(base) + total, int64_t(INT32_MAX)));
						require(Segments[0].sides[0].uvls[c].l == expected, "Light arithmetic lost source clamping or defined saturation");
						defined &= int64_t(base) + total <= INT32_MAX;
					}
					defined &= accumulated >= INT32_MIN && accumulated <= INT32_MAX;
					if (defined) {
						out << "D " << base << ' ' << value << ' ' << direction << ' ' << count;
						for (const auto &uvl : Segments[0].sides[0].uvls) out << ' ' << uvl.l;
						out << '\n';
						++matched;
					} else ++corrected;
				}
	for (int distance : { 0, 1, F1_0, F1_0 + 1, 2 * F1_0, 13 * F1_0, 80 * F1_0, 80 * F1_0 + 1 })
		for (int brightness : { -3 * F1_0, -F1_0, -F1_0 / 8, 0, F1_0 / 8, F1_0, 3 * F1_0 })
			out << "S " << distance << ' ' << brightness << ' ' << xl_light_at_distance(distance, brightness) << '\n';
	out.close();
	require(bool(out), "Cannot finish light reference samples");
	xl_changing_light_clear();
	std::cout << matched << " defined light samples, " << corrected << " overflow controls and 56 source segment-light samples written\n";
}

static xl_compat::Inventory write_changing_light_fixture(const std::filesystem::path &root)
{
	std::ifstream input(root / "neptunxl.hog", std::ios::binary);
	const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	xl_compat::Bytes output{ 'D', 'H', 'F' };
	xl_compat::Inventory result;
	for (auto member : xl_compat::read_hog(original)) {
		if (member.name == "neptunxl.rl2") {
			const auto source = xl_compat::inspect_level(member.bytes);
			require(source.light_indices.size() >= 2, "Light fixture lacks original emitters");
			std::vector<std::array<int, 4>> indices;
			for (int i = 0; i < 500; ++i) indices.push_back({ source.light_indices[0][0], source.light_indices[0][1], 1, i });
			indices.push_back({ source.light_indices[1][0], source.light_indices[1][1], 8191, 65535 });
			std::vector<std::array<int, 7>> deltas;
			for (int i = 0; i < 65535 + 8191; ++i)
				deltas.push_back({ i % static_cast<int>(source.segments.size()), (i / static_cast<int>(source.segments.size())) % 6, 0, 1, 2, 3, 4 });
			member.bytes = xl_test::changing_light_tables(member.bytes, indices, deltas, true);
			result = xl_compat::inspect_level(member.bytes);
			require(xl_compat::runtime_blockers(result).empty(), "Expanded light fixture was not admitted");
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "Light fixture member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<uint8_t>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	require(result.light_indices.size() == 501 && result.delta_lights.size() == 73726, "Expanded light fixture lost records");
	write_bytes((root / "xlights.hog").string().c_str(), output);
	std::ofstream descriptor(root / "xlights.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived XL light tables\nnum_levels = 1\nneptunxl.rl2\n";
	return result;
}

static void light_bad_state(const std::filesystem::path &root)
{
	auto *file = PHYSFS_openWrite("light-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot write lighting trailer");
	const auto user = root / "preview-user";
	std::ifstream input(user / "light-state.bin", std::ios::binary);
	const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	const auto before = light_snapshot();
	for (int variant = 0; variant < 9; ++variant) {
		auto block = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
		auto &rows = block["lighting"];
		if (variant == 0) rows = nlohmann::json::array();
		if (variant == 1) rows.push_back(rows[0]);
		if (variant == 2) rows[0].push_back(0);
		if (variant == 3) rows[0][0] = UINT64_MAX;
		if (variant == 4) rows[0][1] = INT64_MIN;
		if (variant == 5) rows[0][24] = nlohmann::json::boolean_t(1);
		if (variant == 6) rows[0][2] = 0.5;
		if (variant == 7) block.erase("lighting");
		if (variant == 8) block["source"] = xl_level_trigger_identity() ^ UINT64_C(1);
		const auto text = block.dump();
		xl_compat::Bytes bad(encoded.begin(), encoded.begin() + 16);
		bad.insert(bad.end(), text.begin(), text.end());
		std::uint32_t sum = 2166136261u;
		for (unsigned char byte : text) sum = (sum ^ byte) * 16777619u;
		for (unsigned byte = 0; byte < 4; ++byte) {
			bad[8 + byte] = uint8_t(text.size() >> (8 * byte));
			bad[12 + byte] = uint8_t(sum >> (8 * byte));
		}
		write_bytes((user / "light-bad.bin").string().c_str(), bad);
		file = PHYSFS_openRead("light-bad.bin");
		require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file) && same_light_snapshot(before, light_snapshot()), "Malformed light state was accepted or changed live brightness");
	}
	file = PHYSFS_openRead("light-state.bin");
	require(file && xl_wall_trigger_read(file) && PHYSFS_close(file) && same_light_snapshot(before, light_snapshot()), "Valid light state did not recover");
	std::cout << "Nine malformed light payloads rejected without changing live brightness; valid recovery passed\n";
}

static int changing_light_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		const auto source = write_changing_light_fixture(root / "missions");
		require(load_base_mission("xlights", error, sizeof(error)), error);
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		Game_suspended = SUSP_ROBOTS;
		light_reference_samples(source, root);
		light_segment_graph(source);
		StartNewGame(1);
		Game_suspended = SUSP_ROBOTS;
		require_owned_light_records(source);
		const auto before = light_snapshot();
		std::vector<std::array<int64_t, 24>> contribution(Num_segments);
		for (const auto &index : source.light_indices)
			for (int i = 0; i < index[2]; ++i) {
				const auto &delta = source.delta_lights[index[3] + i];
				for (int c = 0; c < 4; ++c) contribution[delta[0]][delta[1] * 4 + c] += int64_t(delta[c + 3]) * DL_SCALE;
			}
		require(check_trigger_sub(Num_triggers - 2, Player_num, 1) == 0, "Authored expanded light-off action failed");
		const auto dark = light_snapshot();
		for (int s = 0; s < Num_segments; ++s)
			for (int c = 0; c < 24; ++c)
				require(dark[s].light[c] == std::max(INT64_C(0), int64_t(before[s].light[c]) - contribution[s][c]), "Expanded light-off lost a contribution or clamping");
		check_trigger_sub(Num_triggers - 2, Player_num, 1);
		for (int i = 0; i < 2; ++i) require(subtract_light(source.light_indices[i][0], source.light_indices[i][1]) == 0, "Repeated light subtraction was not idempotent");
		require(same_light_snapshot(dark, light_snapshot()), "Repeated light-off changed earned state");
		require(check_trigger_sub(Num_triggers - 1, Player_num, 1) == 0, "Authored expanded light-on action failed");
		const auto earned = light_snapshot();
		for (int s = 0; s < Num_segments; ++s)
			for (int c = 0; c < 24; ++c)
				require(earned[s].light[c] == dark[s].light[c] + contribution[s][c], "Expanded light-on lost its high-index range");
		require(!same_light_snapshot(before, earned), "Fixture did not expose non-invertible lower clamping");
		char save[] = "lights-earned.sav", description[] = "XL earned light state";
		stop_time();
		require(state_save_all_sub(save, description), "Cannot save earned changing lights");
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Light stock transition failed");
		require(!xl_light_index_count() && !xl_light_segment_count() && !xl_changing_light(0, 0, -1), "Owned XL light state leaked into stock");
		require(state_restore_all_sub(save, 0) && same_light_snapshot(earned, light_snapshot()), "Source restore reconstructed masks but lost earned clamped brightness");
		require(!game_is_time_paused(), "Native save/restore left the simulation paused");
		require_owned_light_records(source);
		light_bad_state(root);
		std::cout << "XL changing-light controls passed: 501 sources, 73726 deltas, full first/count widths, authored toggles and exact disk through stock\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL changing-light failed: " << error.what() << '\n';
		return 1;
	}
}
