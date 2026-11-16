static int original_afterburner_strip_test(const std::filesystem::path &root)
{
	char error[256]{};
	require(load_base_mission("wshock20", error, sizeof(error)), error);
	auto *target = mission_prepare_by_name("wshock20");
	require(target, "Cannot retain original afterburner mission descriptor");
	struct Release {
		mission_preparation **target;
		~Release()
		{
			mission_discard_prepared(target);
		}
	} release{ &target };
	auto expected = XlPreparedResources::prepare_level_inputs(target, 6);
	require(expected->scoped_textures() && expected->scoped_texture_playback(), "Afterburner strip requires original retained textures");
	const auto &clips = expected->scoped_texture_playback()->clips;
	const auto animation = std::find_if(clips.begin(), clips.end(), [](const auto &clip) { return clip.clip == VCLIP_AFTERBURNER_BLOB; });
	require(animation != clips.end(), "Original mission has no afterburner replacement strip");
	const auto &image = expected->scoped_textures()->images[animation->image];
	const auto &pixels = image.bitmap;
	new_player_config();
	input_demo_set_skip_level_intro(1);
	Difficulty_level = 2;
	StartNewGame(6);
	require(xl_level_extensions_active() && !Player_is_dead && !Game_suspended && !cheats.enabled,
	        "Afterburner strip requires original Warp Shock startup");
	require(xl_pog_vclip_frames(VCLIP_AFTERBURNER_BLOB) == int(pixels.frames), "Original afterburner strip was not installed");
	object emitter = *ConsoleObject;
	emitter.size = F1_0 / 16;
	compute_segment_center(&emitter.pos, &Segments[emitter.segnum]);
	emitter.orient = vmd_identity_matrix;
	std::vector<std::pair<int, int>> owners;
	for (int lifetime : { -1, 32769, 98307 }) {
		std::set<int> signatures;
		for (int slot = 0; slot <= Highest_object_index; ++slot)
			if (Objects[slot].type != OBJ_NONE) signatures.insert(Objects[slot].signature);
		drop_afterburner_blobs(&emitter, 2, F1_0 / 16, lifetime);
		unsigned created = 0;
		for (int slot = 0; slot <= Highest_object_index; ++slot) {
			const auto &object = Objects[slot];
			if (object.type == OBJ_NONE || signatures.count(object.signature)) continue;
			require(object.type == OBJ_FIREBALL && object.id == VCLIP_AFTERBURNER_BLOB, "Original strip control created an unexpected object");
			owners.emplace_back(slot, object.signature);
			++created;
		}
		require(created == 2, "Original strip control could not create both blobs");
	}
	unsigned comparisons = 0, retired = 0;
	std::set<int> selected_frames;
	const auto frame_bytes = pixels.pixels.size() / pixels.frames;
	for (int tick = 0; tick <= 120; ++tick) {
		retired = 0;
		for (const auto &owner : owners) {
			const auto &object = Objects[owner.first];
			if (object.type != OBJ_FIREBALL || object.signature != owner.second) {
				++retired;
				continue;
			}
			int frame = -999;
			require(xl_afterburner_frame(owner.first, object.id, object.lifeleft, int(pixels.frames), &frame), "Live original blob lost its animation clock");
			const int bitmap = xl_pog_vclip_bitmap(object.id, frame);
			if (frame < 0) {
				require(bitmap == -1, "Negative afterburner frame became visible");
				continue;
			}
			require(bitmap == animation->bitmaps.front(), "Afterburner frame lost its retained bitmap binding");
			const auto &native = GameBitmaps[bitmap];
			require(native.bm_type == BM_RGBA && native.bm_h == int(pixels.frame_height()) &&
			            !std::memcmp(native.bm_data, pixels.pixels.data() + frame_bytes * frame, frame_bytes),
			        "Live afterburner pixels differ from original independently retained strip");
			require(unsigned(native.bm_flags) == dxx_xl_texture_frame_flags(pixels.transparent_frames.data(), pixels.super_frames.data(), unsigned(frame)),
			        "Live afterburner transparency differs from original strip");
			selected_frames.insert(frame);
			++comparisons;
		}
		if (tick == 120) break;
		FrameTime = F1_0 / 60;
		Controls = {};
		calc_game_time();
		GameProcessFrame();
	}
	require(comparisons && selected_frames.size() == pixels.frames && retired == owners.size(),
	        "Afterburner strip control did not consume every authored frame and retire every blob");
	native_continuation_receipt(root / "texture-dependency-audit.json",
	                            { { "mission", "wshock20" }, { "level", 6 }, { "image", image.path }, { "image_sha256", image.hash }, { "blobs", owners.size() }, { "pixel_comparisons", comparisons }, { "selected_frames", selected_frames }, { "retired_blobs", retired }, { "ticks", 120 }, { "limitation", "Native gameplay clocks and retained pixel selection; no raster-output qualification" } });
	std::cout << "Original afterburner strip: " << comparisons << " pixel selections, all frames and blob retirement passed\n";
	return 0;
}
