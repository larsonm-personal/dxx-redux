// Original package admission controls before headed fire qualification
// Interactive admission must preserve the production visibility gate
static void particle_original_native_case(const char *package, const char *image, const std::filesystem::path &installation)
{
	using namespace xl_compat;
	char error[256] = {};
	require(package && *package && image && *image, "Original particle qualification requires the package and original atlas");
	xl_preview_clear_mission();
	require(PHYSFS_addToSearchPath(package, 1), "Cannot mount read-only original Pumo package");
	std::ifstream input(image, std::ios::binary | std::ios::ate);
	require(bool(input) && input.tellg() >= 18 && input.tellg() <= 64 * 1024 * 1024, "Original fire atlas is missing or oversized");
	Bytes pixels(static_cast<std::size_t>(input.tellg()));
	input.seekg(0);
	input.read(reinterpret_cast<char *>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
	require(bool(input), "Original fire atlas read failed");
	const auto atlas = installation / "textures/d2x-xl/smokingfire.tga";
	std::filesystem::create_directories(atlas.parent_path());
	write_bytes(atlas.string().c_str(), pixels);
	require(load_base_mission("pmines", error, sizeof(error)), error);
	struct RestorePresentation {
		int no_render = GameArg.SysInputDemoNoRender;
		~RestorePresentation()
		{
			GameArg.SysInputDemoNoRender = no_render;
		}
	} restore_presentation;
	GameArg.SysInputDemoNoRender = 0;
	// This fire owner deliberately installs only smokingfire.tga
	require(!xl_level_supported_at("pm-hub.rl2", 0, 1), "Interactive Pumo hub bypassed renderer qualification");
	require(std::strstr(xl_level_rejection(), "XL skybox requires an active OpenGL renderer"), "Original Pumo hub lost its interactive visibility rejection");
	std::cout << "Original Pumo hub reaches its interactive skybox visibility rejection with smoke absent\n";
	require(!xl_level_supported_at("mvlev1.rl2", 0, 2), "Interactive Pumo fire scene unexpectedly bypassed renderer qualification");
	require(std::strstr(xl_level_rejection(), "XL fog requires an active OpenGL renderer"), "Original Pumo fire scene lost its interactive visibility rejection");
	GameArg.SysInputDemoNoRender = 1;
	require(xl_level_supported_at("pm-hub.rl2", 0, 1), xl_level_rejection());
	require(xl_level_supported_at("mvlev1.rl2", 0, 2), xl_level_rejection());
	xl_preview_clear_mission();
	std::filesystem::remove(atlas);
	require(PHYSFS_removeFromSearchPath(package), "Cannot unmount original Pumo package");
	std::cout << "Original Pumo native admission: interactive visibility rejection preserved; no-render skybox/fog preflight passed\n";
}
