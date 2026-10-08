// Real native config I/O, shared by desktop and Android library integration
extern "C" {
#include "coop_save.h"
#ifdef ANDROID
#include "android_render_fov.h"
extern int ogl_aniso_level, ogl_msaa_samples, g_texfilt_level;
#endif
#ifdef __ANDROID__
#include "auto_net.h"
#endif
}

static void test_config_policy(const char *output)
{
	using nlohmann::json;
	json trace = json::array();
	Player_num = 0;
#ifdef __ANDROID__
	std::strcpy(auto_net_callsign, "guest");
#endif
	auto write = [](const char *name, const std::string &text) {
		PHYSFS_file *file = PHYSFS_openWrite(name);
		require(file && PHYSFS_writeBytes(file, text.data(), text.size()) == static_cast<PHYSFS_sint64>(text.size()) && PHYSFS_close(file), "write config fixture");
	};
	auto read = []() {
		PHYSFS_file *file = PHYSFS_openRead("descent.cfg");
		require(file != nullptr, "open serialized config");
		std::string text(static_cast<size_t>(PHYSFS_fileLength(file)), '\0');
		require(PHYSFS_readBytes(file, &text[0], text.size()) == static_cast<PHYSFS_sint64>(text.size()) && PHYSFS_close(file), "read serialized config");
		return text;
	};
	for (int discs = 0; discs < 4; ++discs) {
		for (const char *name : { "descent.cfg", "descent_ii.gog", "descent_ii.inst" })
			if (PHYSFS_exists(name)) require(PHYSFS_delete(name), "clear owned config fixture");
		// First-run policy checks only presence; these markers are never decoded
		if (discs & 1) write("descent_ii.gog", "presence marker\n");
		if (discs & 2) write("descent_ii.inst", "presence marker\n");
		PlayerCfg = {};
		Game_screen_mode = SM(800, 600);
#ifdef ANDROID
		ogl_aniso_level = 11;
		ogl_msaa_samples = 22;
		g_texfilt_level = 33;
		android_render_set_main_view_fov(110);
#endif
		require(ReadConfigFile() == 1, "missing config retains native startup return");
#ifdef ANDROID
		require(PlayerCfg.ControlType == CONTROL_USING_JOYSTICK && PlayerCfg.AutomapFreeFlight == 1, "Android first-run pilot defaults retained");
		require(ogl_aniso_level == 11 && ogl_msaa_samples == 22 && g_texfilt_level == 33 && android_render_get_main_view_fov() == 110, "missing config does not apply loaded graphics");
#else
		require(PlayerCfg.ControlType == 0 && PlayerCfg.AutomapFreeFlight == 0, "desktop startup leaves pilot controls unchanged");
#endif
#if defined(ANDROID) && defined(DXX_BUILD_DESCENT_II)
		require(GameCfg.MusicType == (discs == 3 ? MUSIC_TYPE_REDBOOK : MUSIC_TYPE_BUILTIN) && GameCfg.OrigTrackOrder == (discs == 3), "D2 first-run disc/music defaults retained");
#endif
		trace.push_back({ { "no_file", discs }, { "control", PlayerCfg.ControlType }, { "automap", PlayerCfg.AutomapFreeFlight }, { "music", GameCfg.MusicType }, { "track_order", GameCfg.OrigTrackOrder }, { "screen", Game_screen_mode } });
	}
	for (const char *name : { "descent_ii.gog", "descent_ii.inst" })
		if (PHYSFS_exists(name)) require(PHYSFS_delete(name), "remove first-run presence marker");
	for (const char *current : { "", COOP_AUTOSAVE_CALLSIGN, "guest", "GUEST", "pilot", "CoOp" })
		for (const char *saved : { "", COOP_AUTOSAVE_CALLSIGN, "saved", "guest" }) {
			GameCfg = {};
			Game_screen_mode = SM(800, 600);
			std::strcpy(Players[0].callsign, current);
			std::strcpy(GameCfg.LastPlayer, saved);
			const char *expected = current;
#ifdef ANDROID
			bool retain = !*current || !std::strcmp(current, COOP_AUTOSAVE_CALLSIGN);
#ifdef __ANDROID__
			retain = retain || !std::strcmp(current, "guest") || !std::strcmp(current, "GUEST");
#endif
			if (retain) expected = *saved && std::strcmp(saved, COOP_AUTOSAVE_CALLSIGN) ? saved : "";
#endif
			require(WriteConfigFile() == 0, "write actual pilot retention config");
			const std::string serialized = read();
			require(ReadConfigFile() == 0 && !std::strcmp(GameCfg.LastPlayer, expected), "last-player round trip preserves native selection");
			require(!std::strcmp(Players[0].callsign, current), "config write leaves current pilot unchanged");
			trace.push_back({ { "current", current }, { "saved", saved }, { "last_player", GameCfg.LastPlayer }, { "config", serialized } });
		}
	for (int filter : { -1, 0, 1, 2, 3 })
		for (int fov : { 0, 90, 100, 110, 120, 130 }) {
			const std::string text = "TexFilt=" + std::to_string(filter) + "\nMainViewFov=" + std::to_string(fov) + "\nAnisoLevel=4\nMsaaLevel=8\nMenuTexFilt=2\nHudTexFilt=0\nMusicType=2\nOrigTrackOrder=1\nMovieTexFilt=1\n";
			write("descent.cfg", text);
			require(ReadConfigFile() == 0, "load actual graphics config");
#ifdef ANDROID
			const int expected_filter = (std::max) (0, (std::min) (2, filter));
			const int expected_fov = fov == 100 || fov == 110 || fov == 120 ? fov : 0;
			require(GameCfg.TexFilt == expected_filter && g_texfilt_level == expected_filter && ogl_aniso_level == 4 && ogl_msaa_samples == 8, "loaded graphics synchronize native runtime globals");
			require(GameCfg.MainViewFov == expected_fov && android_render_get_main_view_fov() == expected_fov, "loaded FOV retains native clamp policy");
#else
			require(GameCfg.TexFilt == filter && GameCfg.MainViewFov == fov, "desktop config parsing remains unchanged");
#endif
			require(GameCfg.MenuTexFilt == 2 && GameCfg.HudTexFilt == 0 && GameCfg.MusicType == 2 && GameCfg.OrigTrackOrder == 1, "adjacent graphics and music fields retained");
#ifdef DXX_BUILD_DESCENT_II
			require(GameCfg.MovieTexFilt == 1, "D2 movie policy retained");
#endif
			require(WriteConfigFile() == 0, "round trip loaded graphics config");
			trace.push_back({ { "filter_input", filter }, { "fov_input", fov }, { "filter", GameCfg.TexFilt }, { "fov", GameCfg.MainViewFov }, { "config", read() } });
		}
	require(PHYSFS_delete("descent.cfg"), "remove owned config fixture");
	std::FILE *file = std::fopen(output, "wb");
	const std::string text = trace.dump(2) + "\n";
	require(file && std::fwrite(text.data(), 1, text.size(), file) == text.size() && std::fclose(file) == 0, "write stable config policy trace");
	std::printf("PASS: %zu actual config policy cases\n", trace.size());
}
