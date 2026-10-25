// Published original-asset context, without admitting or executing its mission.
static int static_texture_catalog_test(int argc, char **argv)
{
	try {
		char error[256] = {}, descriptor[PATH_MAX], texture[256];
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		const char *alias = find_arg_value(argc, argv, "-asset-alias");
		const char *texture_name = find_arg_value(argc, argv, "-texture-name");
		if (!texture_name) texture_name = "ceil002";
		require(!strcmp(texture_name, "ceil002") || !strcmp(texture_name, "fan03") || !strcmp(texture_name, "misc075"), "unknown original texture witness");
		const char *expected_path = find_arg_value(argc, argv, "-expected-texture");
		require(alias && expected_path, "missing original texture fixture arguments");
		android_mission_assets_init();
		require(android_mission_assets_prepare(alias, descriptor, sizeof(descriptor)) && android_mission_assets_activate(), "published original mission context rejected");
		require(android_mission_assets_find_static_texture("missions/single/boilpnt.hog", texture_name, texture, sizeof(texture)), "original static texture did not resolve");
		std::ifstream input(expected_path, std::ios::binary | std::ios::ate);
		const auto size = input.tellg();
		require(input && size == (!strcmp(texture_name, "ceil002") ? 17445 : 263205), "original texture fixture length changed");
		input.seekg(0);
		std::vector<unsigned char> expected(static_cast<size_t>(size)), actual(expected.size());
		require(bool(input.read(reinterpret_cast<char *>(expected.data()), size)), "cannot read original texture fixture");
		PHYSFS_file *file = PHYSFS_openRead(texture);
		require(file && PHYSFS_fileLength(file) == size && PHYSFS_readBytes(file, actual.data(), actual.size()) == size, "published texture payload changed");
		PHYSFS_close(file);
		require(actual == expected, "native resolver selected different original texture bytes");
		android_mission_assets_free_begin();
		android_mission_assets_free_end();
		require(!android_mission_assets_find_static_texture("boilpnt.hog", texture_name, texture, sizeof(texture)), "original texture survived context teardown");
		android_mission_assets_shutdown();
		std::cout << "Original BoilingPoint static texture: published context, exact bytes and stock teardown passed; gameplay not executed\n";
		return 0;
	} catch (const std::exception &e) {
		std::fprintf(stderr, "Original static texture FAIL: %s\n", e.what());
		return 1;
	}
}
