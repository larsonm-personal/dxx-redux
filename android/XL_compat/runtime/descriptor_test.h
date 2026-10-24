// Native descriptor discovery without admitting unaudited XL gameplay
static int descriptor_test(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission("d2", error, sizeof(error)), error);
		require(load_level("d2leva-1.rl2") == 0, "stock baseline level failed");
		const auto stock_definitions = gameplay_definitions();
		const auto stock_model = arena_model();
		const int stock_segments = Num_segments;
		for (int pass = 0; pass < 2; ++pass) {
			require(load_base_mission("obsrv-xl", error, sizeof(error)), error);
			require(!std::strcmp(Current_mission_longname, "Observatory XL"), "original XL title was not discovered");
			require(Current_mission->enhanced == MISSION_ENHANCED_XL, "XL descriptor identity lost");
			require(Last_level == 1 && Last_secret_level == 0 && !std::strcmp(Level_names[0], "obsrv-xl.rl2"), "original level list differs");
			require(!Current_mission->alternate_ham_file && bm_compatibility_warning_count() == 0, "XL descriptor was treated as stock extra-HAM");
			require(gameplay_definitions() == stock_definitions && arena_model() == stock_model, "descriptor changed gameplay definitions or model payload");
			for (const auto &entry : { std::pair<const char *, int>{ "obsrv-xl.rl2", 52176 }, { "obsrv-xl.pog", 26144 } }) {
				const char *resolved = PHYSFS_getRealDir(entry.first);
				require(resolved && std::filesystem::path(resolved).filename() == "obsrv-xl.hog", "original companion HOG not selected");
				PHYSFS_file *file = PHYSFS_openRead(entry.first);
				require(file != nullptr, "original archive member missing");
				const auto size = PHYSFS_fileLength(file);
				PHYSFS_close(file);
				require(size == entry.second, "original archive member size differs");
			}
			const auto rng_calls = d_rand_get_stream_call_count(D_RNG_SIM);
			unsigned rng_before = 0, rng_after = 0;
			require(d_rand_get_stream_state(D_RNG_SIM, &rng_before), "SIM state unavailable");
			auto geometry = [] {
				return std::string(reinterpret_cast<const char *>(Segments), sizeof(*Segments) * Num_segments) +
				       std::string(reinterpret_cast<const char *>(Vertices), sizeof(*Vertices) * Num_vertices);
			};
			const auto before_geometry = geometry();
			const auto before_time = GameTime64;
			require(load_level("obsrv-xl.rl2") != 0, "unaudited Observatory gameplay admitted");
			require(load_level("d2leva-1.rl2") != 0, "XL descriptor admitted even a stock-format level");
			require(d_rand_get_stream_state(D_RNG_SIM, &rng_after) && rng_after == rng_before &&
			            d_rand_get_stream_call_count(D_RNG_SIM) == rng_calls && GameTime64 == before_time,
			        "rejection changed SIM state, draw count or game time");
			require(Num_segments == stock_segments && geometry() == before_geometry, "rejection changed loaded geometry");
			require(!xl_preview_pilot_active() && !xl_preview_water_segment(187), "rejection activated an XL runtime profile");
			require(load_base_mission("d2", error, sizeof(error)), error);
			require(Current_mission->enhanced == 0 && load_level("d2leva-1.rl2") == 0, "stock recovery failed");
			require(!PHYSFS_getRealDir("obsrv-xl.rl2"), "XL companion archive leaked into stock mission");
			require(gameplay_definitions() == stock_definitions, "stock definitions changed after XL descriptor");
		}
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		auto fixture = [&](const char *name, const char *body, bool accepted) {
			std::ofstream out(root / (std::string(name) + ".mn2"));
			out << body;
			out.close();
			require(bool(load_base_mission(name, error, sizeof(error))) == accepted, "descriptor fixture acceptance differs");
			if (accepted) require(Current_mission->enhanced == MISSION_ENHANCED_XL && load_level("d2leva-1.rl2") != 0, "fixture lost XL runtime guard");
		};
		// A stock enhanced-mission fallback must not try to decode this synthetic HAM
		{
			std::ofstream poison(root.parent_path() / "xlcase.ham", std::ios::binary);
			poison << "invalid HAM sentinel";
		}
		require(PHYSFS_getRealDir("xlcase.ham") != nullptr, "HAM negative control is not mounted");
		// The companion HOG must be inspectable without decoding even stock-named assets
		xl_compat::Bytes shadow{ 'D', 'H', 'F' };
		const char member[13] = "descent2.ham";
		shadow.insert(shadow.end(), member, member + sizeof(member));
		shadow.insert(shadow.end(), { 4, 0, 0, 0, 'B', 'A', 'D', '!' });
		write_bytes((root / "xlshadow.hog").string().c_str(), shadow);
		fixture("xlshadow", "d2x-name = Shadow assets\nnum_levels = 1\nd2leva-1.rl2\n", true);
		const char *shadow_dir = PHYSFS_getRealDir("descent2.ham");
		require(shadow_dir && std::filesystem::path(shadow_dir).filename() == "xlshadow.hog", "stock-name HAM negative control is not active");
		require(load_mission_ham() == 0 && gameplay_definitions() == stock_definitions && arena_model() == stock_model,
		        "descriptor selection decoded or freed shadowed gameplay resources");
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "shadow HAM recovery failed");
		fixture("xlcase", "  D2X-NaMe = Case test\ntype = normal\nnum_levels = 1\nd2leva-1.rl2\n", true);
		fixture("xlmix", "name = Legacy title\nnum_levels = 1\nd2leva-1.rl2\nd2x-name = XL identity\n", false);
		fixture("xlback", "d2x-name = XL title\nnum_levels = 1\nd2leva-1.rl2\n  name = Legacy identity\n", false);
		fixture("xlff", "d2x-name = XL title\nnum_levels = 1\nd2leva-1.rl2\n\fname = Legacy identity\n", false);
		fixture("xlvt", "d2x-name = XL title\nnum_levels = 1\nd2leva-1.rl2\n\vzname = Legacy identity\n", false);
		fixture("xlvtcrit", "d2x-name = XL title\nnum_levels = 1\nd2leva-1.rl2\n\v!ham = other.ham\n", false);
		fixture("xldup", "d2x-name = First\n  d2x-name = Second\nnum_levels = 1\nd2leva-1.rl2\n", false);
		const auto hidden_title = std::string("d2x-name = A") + std::string(80, ' ') + "B\nnum_levels = 1\nd2leva-1.rl2\n";
		fixture("xlhidden", hidden_title.c_str(), false);
		const auto truncated_title = std::string("d2x-name = A;") + std::string(PATH_MAX, 'x') + "\nnum_levels = 1\nd2leva-1.rl2\n";
		fixture("xltrunc", truncated_title.c_str(), false);
		fixture("xllong", "d2x-name = Title exceeding the twenty five character native limit\nnum_levels = 1\nd2leva-1.rl2\n", false);
		fixture("xlfile", "d2x-name = Long filename\nnum_levels = 1\ntoolongfilename.rl2\n", false);
		fixture("xlsecrt", "d2x-name = Secret mapping\nnum_levels = 2\nd2leva-1.rl2\nd2leva-2.rl2\nnum_secrets = 1\nd2leva-s.rl2,2\n", true);
		require(Last_level == 2 && Last_secret_level == -1 && Secret_level_table[0] == 2 &&
		            !std::strcmp(Secret_level_names[0], "d2leva-s.rl2"),
		        "XL secret mapping differs");
		fixture("xlbadsec", "d2x-name = Bad secret\nnum_levels = 1\nd2leva-1.rl2\nnum_secrets = 1\nd2leva-s.rl2,2\n", false);
		fixture("xlsuffix", "d2x-name-extra = Invalid\nnum_levels = 1\nd2leva-1.rl2\n", false);
		fixture("xlempty", "d2x-name = ; comment only\nnum_levels = 1\nd2leva-1.rl2\n", false);
		fixture("xlcount", "d2x-name = Invalid count\nnum_levels = 99999999999999\n", false);
		fixture("xlcrit", "d2x-name = Critical\nnum_levels = 1\nd2leva-1.rl2\n  !ham = other.ham\n", false);
		fixture("xlhog", "d2x-name = Archive\nnum_levels = 1\nd2leva-1.rl2\nhog = other.hog\n", false);
		require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "malformed descriptor recovery failed");
		std::cout << "Original Observatory descriptor: title, level list and companion archive resolved; gameplay rejected; stock isolation and malformed descriptors passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::fprintf(stderr, "XL descriptor FAIL: %s\n", e.what());
		return 1;
	}
}
