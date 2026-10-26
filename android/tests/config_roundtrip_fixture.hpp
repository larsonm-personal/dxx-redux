// Selective recovery of the old host campaign's persistence oracle, using the
// existing actual-engine fixture rather than a second menu observation bridge.
static void test_config_roundtrip(const char *operation, const char *profile)
{
	const bool a = !std::strcmp(profile, "A");
	require(a || !std::strcmp(profile, "B"), "known config roundtrip profile");
	const bool write = !std::strcmp(operation, "write");
	require(write || !std::strcmp(operation, "read"), "known config roundtrip operation");
	Player_num = 0;
	std::strcpy(Players[0].callsign, "round");
	if (write) {
		// Native defaults on the first pass, existing settings on subsequent passes.
		const bool exists = PHYSFS_exists("descent.cfg");
		require(ReadConfigFile() == (exists ? 0 : 1), "load config before native write");
		GameCfg.DigiVolume = a ? 1 : 7;
		GameCfg.MusicVolume = a ? 3 : 6;
		GameCfg.ReverseStereo = a ? 1 : 0;
		GameCfg.MusicType = a ? MUSIC_TYPE_NONE : MUSIC_TYPE_BUILTIN;
		GameCfg.TexFilt = a ? 0 : 2;
		GameCfg.MenuTexFilt = a ? 2 : 0;
		GameCfg.HudTexFilt = a ? 1 : 2;
		GameCfg.VSync = a ? 0 : 1;
		GameCfg.AspectX = a ? 3 : 9;
		GameCfg.AspectY = a ? 4 : 16;
		Game_screen_mode = a ? SM(640, 480) : SM(1280, 720);
		require(WriteConfigFile() == 0, "native settings write succeeds");
	} else {
		// A fresh process must reconstruct settings from disk, not retained globals.
		GameCfg = {};
		require(ReadConfigFile() == 0, "persisted config must exist");
		require(GameCfg.DigiVolume == (a ? 1 : 7) && GameCfg.MusicVolume == (a ? 3 : 6) &&
		            GameCfg.ReverseStereo == (a ? 1 : 0) && GameCfg.MusicType == (a ? MUSIC_TYPE_NONE : MUSIC_TYPE_BUILTIN),
		        "audio settings survive native process restart");
		require(GameCfg.TexFilt == (a ? 0 : 2) && GameCfg.MenuTexFilt == (a ? 2 : 0) &&
		            GameCfg.HudTexFilt == (a ? 1 : 2) && GameCfg.VSync == (a ? 0 : 1),
		        "graphics settings survive native process restart");
		require(GameCfg.ResolutionX == (a ? 640 : 1280) && GameCfg.ResolutionY == (a ? 480 : 720) &&
		            GameCfg.AspectX == (a ? 3 : 9) && GameCfg.AspectY == (a ? 4 : 16),
		        "display settings survive native process restart");
	}
	std::printf("PASS: native config %s %s\n", operation, profile);
}
