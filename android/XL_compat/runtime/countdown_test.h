#include "countdown_controls.h"
#include "oof_resource.h"
#include "../tests/oof_metadata_controls.h"
#include "../tests/countdown_fixture.h"
static void countdown_corruption(const std::filesystem::path &user)
{
	using xl_countdown_controls::require;
	// Keep live voices in the atomicity assertion without callback advancement
	const auto audio_status = SDL_GetAudioStatus();
	SDL_PauseAudio(1);
	auto *file = PHYSFS_openWrite("countdown-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot capture countdown trailer");
	std::ifstream input(user / "countdown-state.bin", std::ios::binary);
	const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	const auto original = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
	for (int variant = 0; variant < 15; ++variant) {
		auto changed = original;
		auto &row = changed["countdown"];
		if (variant < 7) row[variant] = variant == 0 ? 2 : variant == 1 ? -129 * F1_0
			                                           : variant == 2   ? -129
			                                           : variant == 3   ? 32768
			                                           : variant == 4   ? -1
			                                           : variant == 5   ? 25
			                                                            : F1_0;
		if (variant == 7) row[1] = row[1].get<int>() - 1;
		if (variant == 8) row[0] = 0;
		if (variant == 9) row[3] = UINT64_MAX;
		if (variant == 10) row[4] = 0.5;
		if (variant == 11) row.push_back(0);
		if (variant == 12) changed.erase("countdown");
		if (variant == 13) row[3] = -1;
		if (variant == 14) row[4] = F1_0 * 25 / 1000 + 1;
		const auto payload = changed.dump();
		xl_compat::Bytes bytes(encoded.begin(), encoded.begin() + 16);
		bytes.insert(bytes.end(), payload.begin(), payload.end());
		std::uint32_t checksum = 2166136261u;
		for (unsigned char byte : payload) checksum = (checksum ^ byte) * 16777619u;
		xl_test::layout_patch(bytes, 8, unsigned(payload.size()));
		xl_test::layout_patch(bytes, 12, checksum);
		write_bytes((user / "countdown-bad.bin").string().c_str(), bytes);
		const auto before = xl_countdown_controls::snapshot();
		file = PHYSFS_openRead("countdown-bad.bin");
		require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file) && xl_countdown_controls::equal(before, xl_countdown_controls::snapshot()), "Malformed countdown state admitted or partially published");
		file = PHYSFS_openWrite("countdown-after.bin");
		require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot recapture countdown trailer");
		std::ifstream after(user / "countdown-after.bin", std::ios::binary);
		require(xl_compat::Bytes(std::istreambuf_iterator<char>(after), std::istreambuf_iterator<char>()) == encoded, "Rejected countdown changed other owned state");
	}
	if (audio_status == SDL_AUDIO_PLAYING) SDL_PauseAudio(0);
}
static int countdown_test(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		using namespace xl_countdown_controls;
		using xl_countdown_controls::require;
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		xl_test::write_countdown_fixture(root / "missions/neptunxl.hog", root / "missions");
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		for (int ordinal : { 1, 2 }) {
			require(load_base_mission("xcount", error, sizeof(error)), error);
			StartNewGame(ordinal);
			const auto reactor_slots = ControlCenterTriggers;
			if (ordinal == 2) {
				require(reactor_slots.num_links == 10, "Reference-capped reactor count not loaded");
				for (int i = 0; i < 10; ++i) require(reactor_slots.seg[i] == reactor_slots.seg[0] && reactor_slots.side[i] == reactor_slots.side[0], "Duplicate reactor slots changed during loading");
			}
			Game_suspended = SUSP_ROBOTS;
			const auto owned_model = xl_owned_oof_model(132);
			require(owned_model && owned_model->model.has_special_points && owned_model->model.has_rotation_tracks && owned_model->skins.size() == 1, "Typed OOF metadata or owned skin missing after native load");
			const auto expected_mesh = xl_compat::compile_oof_mesh(xl_compat::read_oof(xl_oof_test::fixture(-1, 2, 0, "", 1, 1)), false);
			require(xl_oof_test::same_mesh(owned_model->mesh, expected_mesh), "Metadata changed native static render output");
			// Existing native model slots retain stock collision data while OOF owns rendering
			require(Polygon_models[132].model_data && Polygon_models[132].model_data_size > 0, "Native stock collision model missing");
			const xl_compat::Bytes native_geometry(Polygon_models[132].model_data, Polygon_models[132].model_data + Polygon_models[132].model_data_size);
			const auto model_sources = xl_resource_mod_sources();
			const auto model_path = root / "mods/xcount/models/model132.oof";
			const auto original_model = xl_oof_test::metadata_fixture();
			write_bytes(model_path.string().c_str(), xl_oof_test::metadata_fixture(1));
			xl_compat::OwnedModelSet changed;
			try {
				changed = xl_prepare_owned_models(ordinal);
			} catch (...) {
				write_bytes(model_path.string().c_str(), original_model);
				throw;
			}
			write_bytes(model_path.string().c_str(), original_model);
			require(changed.sources.at("mods/xcount/models/model132.oof") != model_sources.at("mods/xcount/models/model132.oof") && xl_oof_test::same_mesh(changed.models.at(132)->mesh, expected_mesh), "Authored RANI changes lost source identity or changed static output");
			require(xl_owned_oof_model(132) == owned_model && xl_resource_mod_sources() == model_sources, "Preparing metadata changes mutated installed ownership");
			auto malformed_model = original_model;
			malformed_model.pop_back();
			write_bytes(model_path.string().c_str(), malformed_model);
			bool metadata_rejected = false;
			try {
				(void) xl_prepare_owned_models(ordinal);
			} catch (const std::exception &) {
				metadata_rejected = true;
			}
			write_bytes(model_path.string().c_str(), original_model);
			require(metadata_rejected && xl_owned_oof_model(132) == owned_model && xl_resource_mod_sources() == model_sources, "Malformed metadata changed installed resources");
			events();
			frames();
			for (const char *bad : { "badcount.rl2", "badobject.rl2", "badbase.rl2" }) {
				const auto before = snapshot();
				require(!xl_level_supported(bad, 0) && std::strstr(xl_level_rejection(), "countdown") && load_level(bad) != 0 && equal(before, snapshot()), "Invalid countdown admission lacks reason or changed active state");
			}
			const auto earned = prepare();
			require(native_environment_observation().at("countdown") == nlohmann::json::array({ earned.pending, earned.phase, earned.fraction }),
			        "Environment observation lost pending or fractional countdown cadence");
			stop_time();
			countdown_corruption(root / "preview-user");
			char save[] = "xl-countdown.sav", description[] = "XL countdown continuation";
			require(state_save_all_sub(save, description), "Cannot save countdown through normal disk path");
			continuation(earned);
			require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Countdown stock transition failed");
			require(!xl_owned_oof_model(132) && xl_resource_mod_sources().empty(), "OOF metadata ownership leaked into classic mission");
			require(Polygon_models[132].model_data_size == int(native_geometry.size()) && std::equal(native_geometry.begin(), native_geometry.end(), Polygon_models[132].model_data), "Metadata changed stock collision data");
			const auto stock = snapshot();
			trigger action{};
			action.type = 25;
			action.time = 99;
			require(!xl_countdown_action(&action), "XL countdown action ran in Redux");
			xl_countdown_frame();
			require(equal(snapshot(), stock), "XL countdown consumer changed Redux state");
			require(state_restore_all_sub(save, 0) && xl_source_level_version() == (ordinal == 1 ? 23 : 27) && equal(snapshot(), earned), "Disk through stock changed exact countdown state");
			require(ControlCenterTriggers.num_links == reactor_slots.num_links, "Reactor count changed across disk-through-stock restore");
			for (int i = 0; i < reactor_slots.num_links; ++i) require(ControlCenterTriggers.seg[i] == reactor_slots.seg[i] && ControlCenterTriggers.side[i] == reactor_slots.side[i], "Reactor order changed across disk-through-stock restore");
			continuation(earned);
			require(!game_is_time_paused(), "Native save owner left an unbalanced pause");
			require(Polygon_models[132].model_data_size == int(native_geometry.size()) && std::equal(native_geometry.begin(), native_geometry.end(), Polygon_models[132].model_data), "Restored metadata changed stock collision data");
			const auto restored_model = xl_owned_oof_model(132);
			require(restored_model && restored_model->model.has_rotation_tracks && restored_model->model.has_special_points && xl_resource_mod_sources() == model_sources && xl_oof_test::same_mesh(restored_model->mesh, expected_mesh), "OOF metadata identity/output changed across disk-through-stock restore");
			std::cout << (ordinal == 1 ? 23 : 27) << "/40 countdown: wall/master/object/autoplay, warnings, rocking, 15 malformed trailers and disk-through-stock passed\n";
		}
		std::cout << "XL countdown runtime controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
