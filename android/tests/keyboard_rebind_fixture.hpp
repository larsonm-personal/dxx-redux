// Exercise the real native binding window handler without drawing its artwork.
namespace keyboard_rebind_fixture {
struct key_event {
	event_type type;
	int keycode;
};
static void send(window *wind, int key)
{
	key_event event = { EVENT_KEY_COMMAND, key };
	window_send_event(wind, reinterpret_cast<d_event *>(&event));
}
static void change(int key, bool cancel)
{
	char title[] = "Keys";
	kconfig(0, title);
	window *wind = window_get_front();
	require(wind != nullptr, "native keyboard menu opens");
	send(wind, KEY_ENTER);
	send(wind, cancel ? KEY_ESC : key);
	require(window_exists(wind), "binding capture or cancellation retains menu");
	send(wind, KEY_ESC);
	require(!window_exists(wind), "native keyboard menu closes and publishes bindings");
}
static void check_input(int key, bool active)
{
	Controls = {};
	FrameTime = F1_0 / 60;
	key_event event = { EVENT_KEY_COMMAND, key };
	kconfig_read_controls(reinterpret_cast<d_event *>(&event), 0);
	require((Controls.pitch_time != 0) == active, "mapped key produces native pitch control and unmapped key does not");
	event.type = EVENT_KEY_RELEASE;
	kconfig_read_controls(reinterpret_cast<d_event *>(&event), 0);
	require(Controls.pitch_time == 0 && Controls.key_pitch_forward_state == 0, "native key release clears pitch control");
}
static void run(const char *phase)
{
	const bool prepare = !std::strcmp(phase, "prepare");
	const bool restore = !std::strcmp(phase, "restore");
	const bool restored = !std::strcmp(phase, "read-restored");
	require(prepare || restore || restored || !std::strcmp(phase, "read-rebound"), "known binding phase");
	static char driver[] = "SDL_VIDEODRIVER=dummy";
	require(SDL_putenv(driver) == 0 && SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0, "initialize native keyboard event services");
	key_init();
	Player_num = 0;
	std::strcpy(Players[0].callsign, "keytest");
	GameArg.SysUsePlayersDir = 0;
	Game_mode = 0;
	GameArg.SndNoSound = GameArg.SndNoMusic = 1;
	// Localized labels are not rendered by this asset-free event test.
	for (unsigned i = 0; i < N_TEXT_STRINGS; ++i) Text_string[i] = const_cast<char *>("fixture");
	new_player_config();
	// Canvas/font geometry is required by window creation; no fake handler,
	// key table or player-file reader replaces production behavior.
	grs_screen screen = {};
	grs_font font = {};
	font.ft_w = 7;
	font.ft_h = 5;
	Gamefonts[GFONT_SMALL] = &font;
	FNTScaleX = FNTScaleY = 1;
	bytes pixels(640 * 480);
	gr_init_canvas(&screen.sc_canvas, pixels.data(), BM_LINEAR, 640, 480);
	screen.sc_w = 640;
	screen.sc_h = 480;
	screen.sc_mode = Game_screen_mode = SM(640, 480);
	grd_curscreen = &screen;
	gr_set_current_canvas(nullptr);
	const auto baseline = PlayerCfg;
	require(baseline.KeySettings[0][0] == KEY_UP, "known native pitch binding slot");
	for (const auto value : baseline.KeySettings[0]) require(value != KEY_U, "replacement key is unused in the original keyboard map");
	if (!prepare) {
		PlayerCfg.KeySettings[0][0] = KEY_Z;
		require(read_player_file() == 0, "read actual persisted player file in fresh process");
		kc_set_controls();
	}
	auto assert_map = [&](bool rebound) {
		for (size_t group = 0; group < 3; ++group)
			for (size_t i = 0; i < sizeof(PlayerCfg.KeySettings[group]); ++i)
				require(PlayerCfg.KeySettings[group][i] == (rebound && group == 0 && i == 0 ? KEY_U : baseline.KeySettings[group][i]), "whole persisted control map matches expected binding state");
	};
	assert_map(!prepare && !restored);
	check_input(KEY_UP, prepare || restored);
	check_input(KEY_U, !prepare && !restored);
	if (prepare || restore) {
		const auto before_cancel = PlayerCfg;
		change(0, true);
		require(!std::memcmp(PlayerCfg.KeySettings, before_cancel.KeySettings, sizeof(PlayerCfg.KeySettings)), "native Escape cancels without changing any binding");
		change(prepare ? KEY_U : KEY_UP, false);
		assert_map(prepare);
		check_input(KEY_UP, !prepare);
		check_input(KEY_U, prepare);
		require(write_player_file() == 0, "native player-file write persists menu bindings");
	}
	Gamefonts[GFONT_SMALL] = nullptr;
	grd_curscreen = nullptr;
	grd_curcanv = nullptr;
	SDL_Quit();
	std::printf("PASS: native keyboard %s\n", phase);
}
}
