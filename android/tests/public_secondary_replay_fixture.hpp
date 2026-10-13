// SDL batches differ only in the order of mouse motion and secondary release
// Keep live control dispatch authoritative; never emulate replay post-frame checks
static void capture_public_secondary_frames(const char *game_name, const char *mission, bool mouse_first)
{
	using nlohmann::json;
	constexpr int frame_count = 25;
	char error[256] = "";
	kc_set_controls();
	require(PlayerCfg.KeySettings[0][26] == KEY_SPACEBAR, "default secondary keyboard binding");
	require(Players[0].secondary_weapon == 0 && Players[0].secondary_ammo[0] == 5, "ordinary starting concussion inventory");
	require(Weapon_info[Secondary_weapon_to_weapon_info[0]].fire_wait == F1_0 / 2, "loaded concussion cooldown");
	require(input_demo_state_trace_start("capture-observation.jsonl", "record", game_name, mission, 1, Difficulty_level, "new_level", frame_count, error, sizeof(error)), error);
	const bool screenshots = std::getenv("DXX_PUBLIC_CAPTURE_SCREENSHOTS") != nullptr;
	const auto push_mouse = []() {
		SDL_Event event = {};
		event.type = SDL_MOUSEMOTION;
		require(SDL_PushEvent(&event) == 0, "queue ordinary mouse event");
	};
	event_flush();
	json observations = json::array();
	json screenshot_frames = json::array();
	for (int frame = 0; frame < frame_count; ++frame) {
		FrameTime = F1_0 / 20;
		const bool press = frame == 2 || frame == 12;
		const bool release = frame == 3 || frame == 13;
		const auto before_time = GameTime64;
		const auto before_cooldown = Next_missile_fire_time;
		const int before_ammo = Players[0].secondary_ammo[0];
		if (mouse_first && frame == 13) push_mouse();
		if (press || release) {
			SDL_Event key = {};
			key.type = press ? SDL_KEYDOWN : SDL_KEYUP;
			key.key.state = press ? SDL_PRESSED : SDL_RELEASED;
			key.key.keysym.sym = SDLK_SPACE;
			require(SDL_PushEvent(&key) == 0, "queue ordinary secondary key event");
		}
		if (!mouse_first && frame == 13) push_mouse();
		event_poll();
		observations.push_back({ { "frame", frame }, { "game_time", before_time }, { "cooldown_before", before_cooldown }, { "cooldown_after", Next_missile_fire_time }, { "ammo_before", before_ammo }, { "ammo_after", int(Players[0].secondary_ammo[0]) }, { "held", Controls.fire_secondary_state }, { "queued", Global_missile_firing_count } });
		if (frame == 12) require(before_time == before_cooldown - 8 && Players[0].secondary_ammo[0] == 4, "second press is eight ticks before the native cooldown");
		if (frame == 13) require(Players[0].secondary_ammo[0] == (mouse_first ? 3 : 4) && !Controls.fire_secondary_state, "ordinary SDL event order determines the second shot");
		calc_game_time();
		GameProcessFrame();
		require(render_update_main_view_objects(), "capture gameplay view state");
		if (screenshots && (frame == 1 || frame == 2 || frame == 13)) {
			GameArg.SysInputDemoNoRender = 0;
			game_render_frame_mono(1);
			screenshot_frames.push_back(frame);
			save_screen_shot(1);
			GameArg.SysInputDemoNoRender = 1;
		}
	}
	input_demo_result result;
	input_demo_capture_current_result(&result);
	require(input_demo_record_finish_input_tail(error, sizeof(error)), "finalize ordered input tail");
	require(input_demo_recorder_flush_with_result("capture.dximdemo", &result, error, sizeof(error)), error);
	input_demo_state_trace_stop();
	newdemo_write_end();
	require(PHYSFS_close(outfile), "close classic callback sink");
	outfile = nullptr;
	Newdemo_state = ND_STATE_NORMAL;
	const json report = { { "game", game_name }, { "scenario", mouse_first ? "secondary-mouse-first" : "secondary-release-first" }, { "frames", frame_count }, { "initial_ammo", 5 }, { "terminal_ammo", int(Players[0].secondary_ammo[0]) }, { "observations", observations }, { "screenshot_frames", screenshot_frames } };
	const auto text = report.dump(2) + "\n";
	write_fixture("capture.json", bytes(text.begin(), text.end()));
	std::puts("PASS: ordinary SDL secondary-fire capture");
}

// A short ordinary-start probe for native input phases beyond the two-shot case
static void capture_public_dispatch_frames(const char *game_name, const char *mission)
{
	using nlohmann::json;
	char error[256] = "";
	constexpr int frames = 8;
	kc_set_controls();
	event_flush();
	const auto key = [](SDLKey symbol, bool down) {
		SDL_Event event = {};
		event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
		event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
		event.key.keysym.sym = symbol;
		require(SDL_PushEvent(&event) == 0, "queue native dispatch probe key");
	};
	FrameTime = 1092;
#ifdef DXX_BUILD_DESCENT_II
	input_demo_replay_direct_command_event invalid_command = {};
	invalid_command.kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_GUIDEBOT_GOAL;
	invalid_command.value0 = INT_MAX;
	for (int validate_only : { 1, 0 })
		require(!input_demo_apply_ordered_direct_command(&invalid_command, validate_only, error, sizeof(error)),
		        "native Guide-Bot command rejects invalid keys before mutation");
#endif
	const unsigned int initial_rng_count = d_rand_get_call_count();
	for (int frame = 0; frame < frames; ++frame) {
		if (frame == 0) {
			key(SDLK_f, true);
			key(SDLK_f, false);
		}
		if (frame == 1) {
			key(SDLK_LCTRL, true);
			key(SDLK_LCTRL, false);
			key(SDLK_LCTRL, true);
		}
		if (frame == 2) {
			game_flush_inputs();
			game_flush_inputs(); // An empty native flush remains an explicit operation
			require(!Controls.fire_primary_state, "native clear releases held input");
		}
		if (frame == 3) {
			require(difficulty_change_to(1, DIFFICULTY_CHANGE_RECORD_DEMO), "native difficulty command before input");
			key(SDLK_f, true);
			key(SDLK_f, false);
		}
		if (frame == 4) key(SDLK_PRINT, true);
		if (frame == 5) {
			key(SDLK_PRINT, false);
			key(SDLK_b, true);
			key(SDLK_b, false);
			key(SDLK_PERIOD, true);
			key(SDLK_PERIOD, false);
			key(SDLK_SPACE, true);
			key(SDLK_SPACE, false);
		}
		if (frame == 6) input_demo_run_game_process_frame([]() {
			require(!input_demo_finish_replay_from_mine_exit(), "live mine-exit probe is not a replay finish");
			game_flush_inputs();
			input_demo_run_game_process_frame([]() { game_flush_inputs(); });
			game_flush_inputs();
		});
		// Frame 6 deliberately has no dispatch; empty is distinct from an idle event
		if (frame != 2 && frame != 6) event_poll();
		if (frame == 3) require(difficulty_change_to(2, DIFFICULTY_CHANGE_RECORD_DEMO), "native difficulty command after input");
		if (frame == 0) require(d_rand_get_call_count() > initial_rng_count, "first native input draws from SIM RNG");
		// Production calculates this after event polling, so samples use the previous value
		FrameTime = frame % 2 ? 2184 : 3276;
		calc_game_time();
		GameProcessFrame();
		require(render_update_main_view_objects(), "dispatch capture gameplay view state");
	}
	// Pending input after the last simulation is executable terminal evidence
	FrameTime = 1638;
	key(SDLK_f, true);
	key(SDLK_f, false);
	event_poll();
	input_demo_result result;
	input_demo_capture_current_result(&result);
	// A stop reached inside simulation must finalize existing input without
	// treating the snapshot itself as a forbidden new input dispatch
	input_demo_run_game_process_frame([]() {
		char tail_error[256] = "";
		require(input_demo_record_finish_input_tail(tail_error, sizeof(tail_error)), "finalize dispatch tail inside the simulation guard");
	});
	require(input_demo_recorder_flush_with_result("capture.dximdemo", &result, error, sizeof(error)), error);
	newdemo_write_end();
	require(PHYSFS_close(outfile), "close dispatch probe classic sink");
	outfile = nullptr;
	Newdemo_state = ND_STATE_NORMAL;
	const json report = { { "game", game_name }, { "mission", mission }, { "frames", frames }, { "scenario", "ordered-dispatch" } };
	const auto text = report.dump(2) + "\n";
	write_fixture("capture.json", bytes(text.begin(), text.end()));
	require(input_demo_load_replay_from_path("capture.dximdemo", error, sizeof(error)), error);
	require(input_demo_start_loaded_replay() == 0, "start dispatch probe replay at normal level entry");
	const control_info paused_controls = Controls;
	const fix paused_energy = Players[0].energy;
	const unsigned int paused_rng = d_rand_get_call_count();
	const fix64 paused_time = GameTime64;
	stop_time();
	for (int attempt = 0; attempt < 2; ++attempt)
		require(!input_demo_step_replay_frame(), "paused draw does not apply pending input");
	key(SDLK_f, true);
	key(SDLK_f, false);
	key(SDLK_LCTRL, true);
	key(SDLK_LCTRL, false);
	SDL_Event motion = {};
	motion.type = SDL_MOUSEMOTION;
	require(SDL_PushEvent(&motion) == 0, "queue live mouse during replay");
	event_poll();
	require(input_demo_replay_next_frame_index() == 0 && GameTime64 == paused_time &&
	            Players[0].energy == paused_energy && d_rand_get_call_count() == paused_rng &&
	            std::memcmp(&Controls, &paused_controls, sizeof(Controls)) == 0,
	        "paused draws and live SDL input cannot mutate recorded playback");
	start_time();
	// Exercise the ordinary user-close handler and its nonlocal exit, not only
	// the codec unload API; cancellation must never manufacture a final result
	if (!setjmp(LeaveEvents)) window_close(Game_wind);
	require(!Game_wind && !input_demo_replay_is_loaded(), "native window close cancels unfinished playback");
	require(!input_demo_replay_actual_result_written(), "cancelled playback has no successful terminal-write verdict");
	std::puts("PASS: native ordered dispatch capture, paused draw and live-input isolation");
}
