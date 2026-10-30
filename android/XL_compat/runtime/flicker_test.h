#include "../tests/flicker_fixture.h"

static std::vector<xl_flicker_state> flicker_snapshot()
{
	std::vector<xl_flicker_state> result(xl_flicker_count());
	for (std::size_t i = 0; i < result.size(); ++i) require(xl_flicker_get(static_cast<int>(i), &result[i]), "Cannot capture flicker phase");
	return result;
}
static bool same_flickers(const std::vector<xl_flicker_state> &a, const std::vector<xl_flicker_state> &b)
{
	return a.size() == b.size() && (a.empty() || !std::memcmp(a.data(), b.data(), a.size() * sizeof(a[0])));
}
static void flicker_frame(fix elapsed)
{
	const auto old = FrameTime;
	FrameTime = elapsed;
	flicker_lights();
	FrameTime = old;
}

static void flicker_stock_rotation()
{
	const std::vector<flickering_light> saved(Flickering_lights, Flickering_lights + Num_flickering_lights);
	int target_segment = -1, target_side = -1;
	for (int s = 0; s < Num_segments && target_segment < 0; ++s)
		for (int side = 0; side < 6; ++side)
			if (Segments[s].children[side] == -1 && Segments[s].sides[side].wall_num < 0) {
				target_segment = s;
				target_side = side;
				break;
			}
	require(target_segment >= 0, "Redux flicker control lacks an outer face");
	auto &face = Segments[target_segment].sides[target_side];
	const auto original_face = face;
	const auto original_brightness = TmapInfo[275].lighting;
	face.tmap_num = 275;
	face.tmap_num2 = 0;
	TmapInfo[275].lighting = F1_0;
	Num_flickering_lights = 1;
	Flickering_lights[0] = { static_cast<short>(target_segment), static_cast<short>(target_side), 2, 0, F1_0 / 4 };
	flicker_frame(1);
	require(Flickering_lights[0].mask == 4 && Flickering_lights[0].timer == F1_0 / 4 - 1, "Redux flicker rotation changed");
	face = original_face;
	TmapInfo[275].lighting = original_brightness;
	Num_flickering_lights = static_cast<int>(saved.size());
	std::copy(saved.begin(), saved.end(), Flickering_lights);
}

static void flicker_rejection_and_visibility(const std::filesystem::path &root)
{
	std::ifstream input(root / "missions" / "flicker.hog", std::ios::binary);
	const xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	for (const auto &member : xl_compat::read_hog(bytes))
		if (member.name == "neptunxl.rl2") {
			auto rows = xl_compat::inspect_level(member.bytes).flicker_lights;
			rows.back()[4] = -1;
			write_bytes((root / "preview-user" / "negative.rl2").string().c_str(), xl_test::flicker_table(member.bytes, rows));
		}
	const auto phase = flicker_snapshot();
	const auto light = light_snapshot();
	require(!xl_level_supported("negative.rl2", 0) && std::strstr(xl_level_rejection(), "negative delay") &&
	            xl_preview_load_level("negative.rl2") != 0 && same_flickers(phase, flicker_snapshot()) && same_light_snapshot(light, light_snapshot()),
	        "Negative flicker delay changed live state or lacked rejection");
	require(PHYSFS_delete("negative.rl2") && xl_level_supported("neptunxl.rl2", 0), "Valid flicker source did not recover after rejection");
	auto &face = Segments[phase[128].segment].sides[phase[128].side];
	const auto original_face = face;
	const auto original_wall = Walls[0];
	const auto original_child = Segments[phase[128].segment].children[phase[128].side];
	Segments[phase[128].segment].children[phase[128].side] = 0;
	face.wall_num = 0;
	Walls[0].type = WALL_OPEN;
	require(!(WALL_IS_DOORWAY(&Segments[phase[128].segment], phase[128].side) & WID_RENDER_FLAG), "Flicker invisibility control is not an open connection");
	flicker_frame(1);
	xl_flicker_state hidden{};
	require(xl_flicker_get(128, &hidden) && hidden.timer == INT32_MIN, "Invisible XL surface did not disable flickering");
	face = original_face;
	Walls[0] = original_wall;
	Segments[phase[128].segment].children[phase[128].side] = original_child;
	enable_flicker(hidden.segment, hidden.side);
	flicker_frame(1);
	xl_flicker_state visible{};
	require(xl_flicker_get(128, &visible) && visible.timer >= 0 && visible.mask == ((hidden.mask >> 1) | (hidden.mask << 31)), "Visible surface did not resume its earned pattern");
	std::cout << "Negative-delay admission/recovery and actual visibility disable/restart passed\n";
}
static void flicker_samples(const std::filesystem::path &root)
{
	std::ofstream out(root / "flicker-samples.txt");
	int defined = 0, corrected = 0;
	for (uint32_t mask : { 0u, 1u, 2u, 0x80000000u, 0x80000001u, 0x5a5a5a5au, UINT32_MAX })
		for (int timer : { INT32_MIN, -1000, -1, 0, 1, 1000, 32768, INT32_MAX })
			for (int delay : { 0, 1, 123, 32768, INT32_MAX })
				for (int elapsed : { 0, 1, 655, 32768, INT32_MAX })
					for (int visible : { 0, 1 })
						for (int emitting : { 0, 1 }) {
							xl_flicker_state state{ 0, 0, delay, timer, mask };
							const int action = xl_flicker_step(&state, elapsed, visible, emitting);
							const int64_t delta = int64_t(timer) - elapsed;
							const bool active = timer != INT32_MIN && visible && emitting && delay > 0;
							const bool safe = !active || (delta >= INT32_MIN && (delta >= 0 || (-delta + delay - 1) / delay <= 10000));
							if (safe) {
								out << timer << ' ' << mask << ' ' << delay << ' ' << elapsed << ' ' << visible << ' ' << emitting << ' ' << state.timer << ' ' << state.mask << ' ' << action << '\n';
								++defined;
							} else {
								require(state.timer >= 0 && state.timer < delay && state.mask == ((mask >> 1) | (mask << 31)), "Large flicker elapsed lost single rotation or bounded phase");
								++corrected;
							}
						}
	out.close();
	require(bool(out), "Cannot finish flicker samples");
	std::cout << defined << " defined flicker samples and " << corrected << " overflow/bounded-catch-up controls passed\n";
}
static xl_compat::Inventory write_flicker_fixture(const std::filesystem::path &root)
{
	std::ifstream input(root / "neptunxl.hog", std::ios::binary);
	const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	xl_compat::Bytes output{ 'D', 'H', 'F' };
	xl_compat::Inventory result;
	for (auto member : xl_compat::read_hog(original)) {
		if (member.name == "neptunxl.rl2") {
			const auto source = xl_compat::inspect_level(member.bytes);
			member.bytes = xl_test::changing_light_tables(member.bytes, source.light_indices, source.delta_lights, true);
			std::vector<std::array<int, 5>> rows(128, { source.light_indices[0][0], source.light_indices[0][1], 2, F1_0 / 8, F1_0 / 4 });
			rows.push_back({ source.light_indices[1][0], source.light_indices[1][1], 2, F1_0 / 8, F1_0 / 4 });
			rows.push_back({ source.light_indices[0][0], source.light_indices[0][1], 2, 0, 0 });
			member.bytes = xl_test::flicker_table(member.bytes, rows);
			result = xl_compat::inspect_level(member.bytes);
			require(xl_compat::runtime_blockers(result).empty(), "Full flicker fixture not admitted");
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "Flicker fixture name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<uint8_t>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "flicker.hog").string().c_str(), output);
	std::ofstream descriptor(root / "flicker.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived XL flickers\nnum_levels = 1\nneptunxl.rl2\n";
	xl_compat::Bytes zero{ 'D', 'H', 'F' };
	bool replaced = false;
	for (auto member : xl_compat::read_hog(output)) {
		if (member.name == "neptunxl.lgt") {
			std::fill(member.bytes.begin(), member.bytes.end(), 0);
			replaced = true;
		}
		char name[13]{};
		std::strcpy(name, member.name.c_str());
		zero.insert(zero.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) zero.push_back(static_cast<uint8_t>(member.bytes.size() >> shift));
		zero.insert(zero.end(), member.bytes.begin(), member.bytes.end());
	}
	require(replaced, "Flicker fixture lacks the owned brightness table");
	write_bytes((root / "fzero.hog").string().c_str(), zero);
	std::ofstream zero_descriptor(root / "fzero.mn2", std::ios::binary);
	zero_descriptor << "d2x-name = Derived XL zero flicker\nnum_levels = 1\nneptunxl.rl2\n";
	return result;
}
static void flicker_bad_state(const std::filesystem::path &root)
{
	auto *file = PHYSFS_openWrite("flicker-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot write flicker trailer");
	const auto user = root / "preview-user";
	std::ifstream input(user / "flicker-state.bin", std::ios::binary);
	const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	const auto phase = flicker_snapshot();
	const auto lights = light_snapshot();
	for (int variant = 0; variant < 12; ++variant) {
		auto block = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
		auto &rows = block["flickers"];
		if (variant == 0) rows = nlohmann::json::array();
		if (variant == 1) rows.push_back(rows[0]);
		if (variant == 2) rows[0][0] = 0;
		if (variant == 3) rows[0][2] = 0;
		if (variant == 4) rows[0][4] = 3;
		if (variant == 5) rows[0][4] = UINT64_MAX;
		if (variant == 6) rows[0][3] = nlohmann::json::boolean_t(1);
		if (variant == 7) rows[0][3] = 0.5;
		if (variant == 8) rows[0][3] = INT64_MIN;
		if (variant == 9) rows[0][3] = -123;
		if (variant == 10) block.erase("flickers");
		if (variant == 11) rows[0][3] = INT32_MAX;
		const auto text = block.dump();
		xl_compat::Bytes bad(encoded.begin(), encoded.begin() + 16);
		bad.insert(bad.end(), text.begin(), text.end());
		uint32_t sum = 2166136261u;
		for (unsigned char byte : text) sum = (sum ^ byte) * 16777619u;
		for (unsigned b = 0; b < 4; ++b) {
			bad[8 + b] = uint8_t(text.size() >> (8 * b));
			bad[12 + b] = uint8_t(sum >> (8 * b));
		}
		write_bytes((user / "flicker-bad.bin").string().c_str(), bad);
		file = PHYSFS_openRead("flicker-bad.bin");
		require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file) && same_flickers(phase, flicker_snapshot()) && same_light_snapshot(lights, light_snapshot()), "Bad flicker state accepted or partially applied");
	}
	file = PHYSFS_openRead("flicker-state.bin");
	require(file && xl_wall_trigger_read(file) && PHYSFS_close(file) && same_flickers(phase, flicker_snapshot()), "Valid flicker state did not recover");
	std::cout << "Twelve malformed flicker payloads rejected without changing clocks or light consumers\n";
}
static int flicker_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		const auto source = write_flicker_fixture(root / "missions");
		require(source.flicker_lights.size() == 130, "Flicker fixture lost source rows");
		flicker_samples(root);
		require(load_base_mission("flicker", error, sizeof(error)), error);
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		Game_suspended = SUSP_ROBOTS;
		require(xl_flicker_count() == 130 && Num_flickering_lights == 0, "Flicker table narrowed or stock shadow retained");
		const auto initial = light_snapshot();
		flicker_frame(F1_0 / 8);
		auto phase = flicker_snapshot();
		require(phase[0].timer == 0 && phase[128].timer == 0 && phase[0].mask == 2 && phase[129].timer == INT32_MIN, "Exact-boundary or zero-delay flicker differs");
		flicker_frame(1);
		phase = flicker_snapshot();
		require(phase[0].mask == 1 && phase[128].mask == 1 && phase[0].timer == F1_0 / 4 - 1, "XL pattern did not rotate right");
		flicker_frame(F1_0 / 4);
		const auto dark = light_snapshot();
		require(!same_light_snapshot(initial, dark), "Flicker phase did not reach actual light consumers");
		phase = flicker_snapshot();
		require(phase[0].mask == 0x80000000u && phase[128].mask == 0x80000000u, "High-row flicker phase lost");
		disable_flicker(phase[0].segment, phase[0].side);
		const auto earned = flicker_snapshot();
		require(earned[0].timer == INT32_MIN && earned[1].timer == phase[1].timer, "Flicker disable did not select first duplicate only");
		char save[] = "flicker-earned.sav", description[] = "XL flicker phase";
		stop_time();
		require(state_save_all_sub(save, description), "Cannot save earned flicker phase");
		flicker_frame(F1_0);
		const auto advanced = flicker_snapshot();
		const auto advanced_lights = light_snapshot();
		require(advanced[128].mask == 0x40000000u, "Long frame advanced pattern more than once");
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Flicker stock transition failed");
		require(!xl_flicker_count() && !xl_flicker_frame() && !xl_flicker_enable(0, 0, 1), "Owned flicker state leaked into stock");
		flicker_stock_rotation();
		require(state_restore_all_sub(save, 0) && same_flickers(earned, flicker_snapshot()) && same_light_snapshot(dark, light_snapshot()), "Disk through stock lost earned flicker phase or brightness");
		flicker_frame(F1_0);
		require(same_flickers(advanced, flicker_snapshot()) && same_light_snapshot(advanced_lights, light_snapshot()), "Restored flicker continuation diverged");
		require(state_restore_all_sub(save, 0), "Cannot reset authored flicker controls");
		require(!game_is_time_paused(), "Native save/restore left the simulation paused");
		require(check_trigger_sub(Num_triggers - 2, Player_num, 1) == 0, "Authored flicker off failed");
		phase = flicker_snapshot();
		require(phase[0].timer == INT32_MIN && phase[128].timer == INT32_MIN, "Authored off did not disable flicker owners");
		require(check_trigger_sub(Num_triggers - 1, Player_num, 1) == 0, "Authored flicker on failed");
		phase = flicker_snapshot();
		require(phase[0].timer == 0 && phase[128].timer == 0, "Authored on did not restart source clocks");
		flicker_frame(1);
		phase = flicker_snapshot();
		require(phase[0].mask == 0x40000000u && phase[128].mask == 0x40000000u, "Restart incorrectly reset earned pattern");
		flicker_bad_state(root);
		flicker_rejection_and_visibility(root);
		require(load_base_mission("fzero", error, sizeof(error)) && !load_level("neptunxl.rl2"), "Zero-brightness flicker source failed to load");
		int brightness = -1;
		require(xl_prepared_texture_brightness(278, &brightness) && !brightness && xl_flicker_count() == 130, "Owned zero LGT was replaced by stock brightness");
		flicker_frame(1);
		for (const auto &entry : flicker_snapshot()) require(entry.timer == INT32_MIN, "Zero authored emission did not disable flickering");
		std::cout << "Owned all-zero LGT disables every flicker without falling back to HAM brightness\n";
		std::cout << "XL flicker controls passed: full owned table, right rotation, authored controls, exact continuation and stock isolation\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL flicker failed: " << error.what() << '\n';
		return 1;
	}
}
