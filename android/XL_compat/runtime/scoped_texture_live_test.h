static int scoped_texture_live_test(int argc, char **argv, const std::filesystem::path &root)
{
	char error[256]{};
	const char *incoming_text = find_arg_value(argc, argv, "-audit-level");
	require(incoming_text && (!std::strcmp(incoming_text, "1") || !std::strcmp(incoming_text, "7")), "Live texture control needs original Boiling Point level 1 or 7");
	const int incoming_level = !std::strcmp(incoming_text, "1") ? 1 : 7;
	const char *image_name = incoming_level == 1 ? "rock194.tga" : "metl060.tga";
	const char *level_directory = incoming_level == 1 ? "level01" : "level07";
	const std::string original_path = std::string("mods/boilpnt/textures/") + image_name;
	const std::string priority_path = std::string("mods/boilpnt/textures/") + level_directory + "/" + image_name;
	const char *outgoing = find_arg_value(argc, argv, "-audit-outgoing");
	if (!outgoing) outgoing = "d2";
	const int outgoing_level = !std::strcmp(outgoing, "wshock20") ? 6 : 1;
	require(!std::strcmp(outgoing, "d2") || !std::strcmp(outgoing, "wshock20"), "Live texture control needs stock or original Warp Shock");
	require(std::filesystem::weakly_canonical(root).parent_path() == std::filesystem::current_path(),
	        "Live texture mutations require an isolated extra directory under the run directory");
	const auto textures = root / "mods/boilpnt/textures";
	const auto held = root / "mods/boilpnt/textures-held-control";
	const auto override_directory = textures / level_directory;
	const auto override_file = override_directory / image_name;
	std::vector<unsigned char> malformed{ 'B', 'A', 'D' };
	const char *malformed_reason = "invalid TGA file size";
	if (incoming_level == 1) {
		malformed.assign(22, 0);
		malformed[2] = 10;
		malformed[12] = malformed[14] = 1;
		malformed[16] = 24;
		malformed[18] = 129; // Two pixels in a one-pixel image
		malformed_reason = "packet exceeds its pixel or input bounds";
	}
	require(std::filesystem::is_directory(textures) && !std::filesystem::is_symlink(textures) &&
	            !std::filesystem::exists(held) && !std::filesystem::exists(override_directory),
	        "Live texture control would overwrite fixture content");
	struct RestoreFixture {
		std::filesystem::path textures, held, directory, file;
		~RestoreFixture()
		{
			std::error_code ignored;
			if (std::filesystem::exists(held)) std::filesystem::rename(held, textures, ignored);
			std::filesystem::remove(file, ignored);
			std::filesystem::remove(directory, ignored);
		}
	} restore{ textures, held, override_directory, override_file };
	require(load_base_mission(outgoing, error, sizeof(error)), error);
	new_player_config();
	input_demo_set_skip_level_intro(1);
	Difficulty_level = 2;
	StartNewGame(outgoing_level);
	require(!Player_is_dead && !Game_suspended && !cheats.enabled, "Live texture control requires ordinary gameplay");
	int remainder = 0;
	const auto advance = [&] {
		for (int frame = 0; frame < 60; ++frame) {
			remainder += F1_0;
			FrameTime = remainder / 60;
			remainder %= 60;
			std::memset(&Controls, 0, sizeof(Controls));
			Controls.forward_thrust_time = FrameTime;
			Controls.fire_primary_state = 1;
			calc_game_time();
			GameProcessFrame();
			require(!Player_is_dead, "Live texture control lost its pilot");
		}
		std::memset(&Controls, 0, sizeof(Controls));
	};
	const auto snapshot = [] {
		nlohmann::json objects = nlohmann::json::object(), pixels = nlohmann::json::array();
		for (int slot = 0; slot < object_capacity(); ++slot)
			if (Objects[slot].type != OBJ_NONE) objects[std::to_string(slot)] = input_demo_object_trace_snapshot(Objects[slot], slot);
		for (int bitmap = 1; bitmap < Num_bitmap_files; ++bitmap) {
			xl_pog_pixel_view view{};
			if (!xl_pog_physics_view(bitmap, &view)) continue;
			const auto &native = GameBitmaps[bitmap];
			pixels.push_back({ { "bitmap", bitmap }, { "pixels", picosha2::hash256_hex_string(view.pixels, view.pixels + size_t(view.row_size) * view.height) }, { "display_address", reinterpret_cast<uintptr_t>(native.bm_data) }, { "flags", native.bm_flags }, { "width", native.bm_w }, { "height", native.bm_h } });
		}
		uint32_t sim = 0;
		require(d_rand_get_state(&sim), "Cannot observe live texture SIM state");
		return nlohmann::json{ { "world", input_demo_world_trace_snapshot() }, { "pool", input_demo_object_pool_trace_snapshot() }, { "objects", objects }, { "xl", input_demo_native_xl_trace_snapshot() }, { "pixels", pixels }, { "sim_state", sim }, { "sim_calls", d_rand_get_call_count() } };
	};
	advance();
	const auto before = snapshot();
	const auto definitions = gameplay_definitions();
	const auto live = Current_mission;
	auto *target = mission_prepare_by_name("boilpnt");
	require(target, "Cannot prepare original Boiling Point target");
	struct Release {
		mission_preparation **target;
		~Release()
		{
			mission_discard_prepared(target);
		}
	} release{ &target };
	const auto unchanged = [&] {
		require(Current_mission == live && gameplay_definitions() == definitions && snapshot() == before,
		        "Incoming texture operation changed outgoing world, objects, XL state, pixels or SIM RNG");
	};
	auto candidate = XlPreparedResources::prepare_level(target, incoming_level);
	require(candidate->has_scoped_textures(), "Original incoming level lacks qualified scoped textures");
	if (incoming_level == 1) {
		bool found = false;
		for (const auto &image : candidate->scoped_textures()->images)
			if (image.path == original_path) {
				require(image.hash == "ffbc219965339e0f079838b1919836adc2e1037ef67ab679d717d97adf01f064" &&
				            picosha2::hash256_hex_string(image.bitmap.pixels) == "370bf54a6cda458661dd8439c1ff749d38931b6dd005dcd0d69e6512b1a7b3e5",
				        "Original RLE candidate differs from independent decoded pixels");
				found = true;
			}
		require(found, "Original incoming RLE image was not selected");
	}
	XlPreparedResources moved(std::move(*candidate));
	candidate.reset();
	unchanged();
	nlohmann::json rejections = nlohmann::json::array();
	for (bool missing : { false, true }) {
		if (missing) std::filesystem::rename(textures, held);
		else {
			std::filesystem::create_directory(override_directory);
			write_bytes(override_file.string().c_str(), malformed);
		}
		std::string rejection;
		try {
			(void) XlPreparedResources::prepare_level(target, incoming_level);
		} catch (const std::runtime_error &failure) {
			rejection = failure.what();
		}
		require(rejection.find(missing ? "no level POG or qualified scoped TGA set" : malformed_reason) != std::string::npos,
		        "Invalid original texture fixture was accepted or rejected for an unrelated reason");
		unchanged();
		if (missing) std::filesystem::rename(held, textures);
		else {
			std::filesystem::remove(override_file);
			std::filesystem::remove(override_directory);
		}
		auto recovered = XlPreparedResources::prepare_level(target, incoming_level);
		require(recovered->has_scoped_textures() && recovered->scoped_textures()->images.size() == moved.scoped_textures()->images.size(),
		        "Valid incoming preparation did not recover after rejection");
		unchanged();
		rejections.push_back({ { "case", missing ? "missing_set" : "malformed_priority_override" }, { "error", rejection }, { "valid_recovery", true } });
	}
	// Authentic alternate pixels in the isolated candidate, with the same image count
	std::filesystem::create_directory(override_directory);
	std::filesystem::copy_file(textures / "ceil002.tga", override_file);
	{
		auto different = XlPreparedResources::prepare_level(target, incoming_level);
		const auto &original_images = moved.scoped_textures()->images;
		const auto &different_images = different->scoped_textures()->images;
		require(original_images.size() == different_images.size(), "Different texture candidate changed the image count");
		const auto find_image = [](const auto &images, const char *path) -> const xl_compat::ScopedTextureImage & {
			for (const auto &image : images)
				if (image.path == path) return image;
			throw std::runtime_error("Texture independence control lost its selected image");
		};
		const auto &original_image = find_image(original_images, original_path.c_str());
		const auto &different_image = find_image(different_images, priority_path.c_str());
		require(original_image.bitmap.pixels != different_image.bitmap.pixels && original_image.hash != different_image.hash,
		        "Equal-count texture candidates reused the same input or pixels");
		unchanged();
		std::filesystem::remove(override_file);
		std::filesystem::remove(override_directory);
		auto recovered = XlPreparedResources::prepare_level(target, incoming_level);
		const auto &recovered_image = find_image(recovered->scoped_textures()->images, original_path.c_str());
		require(recovered_image.bitmap.pixels == original_image.bitmap.pixels && recovered_image.hash == original_image.hash &&
		            different_image.bitmap.pixels != recovered_image.bitmap.pixels &&
		            recovered_image.bitmap.pixels.data() != original_image.bitmap.pixels.data(),
		        "Preparing original inputs changed independent retained texture candidates");
		unchanged();
	}
	candidate = std::make_unique<XlPreparedResources>(std::move(moved));
	candidate.reset();
	require(mission_discard_prepared(&target), "Discarded texture candidate retained descriptor leases");
	unchanged();
	const auto time = GameTime64;
	advance();
	require(GameTime64 > time, "Outgoing level did not continue after texture rejection");
	const auto after_continuation = snapshot();
	require(load_base_mission("boilpnt", error, sizeof(error)), error);
	input_demo_set_skip_level_intro(1);
	StartNewGame(incoming_level);
	const auto saved_sources = xl_resource_mod_sources();
	const auto saved_world = snapshot();
	const auto saved_definitions = gameplay_definitions();
	require(saved_sources.count(original_path) &&
	            xl_resource_sources_for_level(incoming_level) == saved_sources,
	        "Original save preflight omitted retained scoped texture identity");
	std::filesystem::create_directory(override_directory);
	std::filesystem::copy_file(textures / "ceil002.tga", override_file);
	const auto different_sources = xl_resource_sources_for_level(incoming_level);
	require(different_sources != saved_sources && different_sources.count(priority_path.c_str()) &&
	            !different_sources.count(original_path.c_str()),
	        "Original save preflight admitted changed priority texture identity");
	require(snapshot() == saved_world && gameplay_definitions() == saved_definitions && xl_resource_mod_sources() == saved_sources,
	        "Save source preflight changed the live world or installed resource owner");
	write_bytes(override_file.string().c_str(), malformed);
	bool malformed_rejected = false;
	try {
		(void) xl_resource_sources_for_level(incoming_level);
	} catch (const std::runtime_error &failure) {
		malformed_rejected = std::string(failure.what()).find(malformed_reason) != std::string::npos;
	}
	require(malformed_rejected && snapshot() == saved_world && gameplay_definitions() == saved_definitions && xl_resource_mod_sources() == saved_sources,
	        "Malformed save source preflight failed rejection or live-state isolation");
	std::filesystem::remove(override_file);
	std::filesystem::remove(override_directory);
	require(xl_resource_sources_for_level(incoming_level) == saved_sources, "Restored original texture did not recover save source identity");
	native_continuation_receipt(root / "texture-dependency-audit.json", { { "outgoing", outgoing }, { "outgoing_level", outgoing_level }, { "incoming", std::string("boilpnt:") + incoming_text }, { "rejections", rejections }, { "equal_count_independence", true }, { "before", before }, { "after_continuation", after_continuation }, { "ordinary_frames", 120 }, { "save_source_controls", { { "original", true }, { "changed_priority", true }, { "malformed_rejected", true }, { "live_state_unchanged", true }, { "recovered", true }, { "source_count", saved_sources.size() } } }, { "limitation", "Controlled incoming preparation and save source preflight; separate from sampled readiness or a full cold restore" } });
	return 0;
}
