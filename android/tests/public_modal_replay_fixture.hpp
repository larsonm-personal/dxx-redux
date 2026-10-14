extern "C" {
void toggle_cockpit_reverse(void);
}

#ifdef DXX_BUILD_DESCENT_II
extern "C" {
extern int HighlightMarker;
void DropMarker(int player_marker_num);
}
#endif

// Isolated synthetic marker regression; never admitted as a public replay capture
static void test_native_modal_recording_off(const char *game_name)
{
	using nlohmann::json;
	input_demo_recorder_cancel();
	newdemo_write_end();
	require(PHYSFS_close(outfile), "close isolated native regression classic sink");
	outfile = nullptr;
	Newdemo_state = ND_STATE_NORMAL;
	game_flush_inputs();
	kc_set_controls();
	FrameTime = 1092;
	const fix64 before_time = GameTime64;
	const int saved_sticky = PlayerCfg.StickyRearview;
	const auto key = [](SDLKey symbol, bool down) {
		SDL_Event event = {};
		event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
		event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
		event.key.keysym.sym = symbol;
		require(SDL_PushEvent(&event) == 0, "queue recording-off native key");
	};
	const auto tap = [&](SDLKey symbol) {
		key(symbol, true);
		key(symbol, false);
		event_poll();
	};
	json rear_cases = json::array(), marker_cases = json::array();
	const auto check_rear = [&](int sticky, const char *phase, int active, int armed) {
		int actual_active, actual_armed, cockpit, preferred;
		input_demo_get_rear_view_state(&actual_active, &actual_armed, &cockpit, &preferred);
		require(actual_active == active && actual_armed == armed &&
		            cockpit == (active && preferred == CM_FULL_COCKPIT ? CM_REAR_VIEW : preferred),
		        "recording-off rear state, hold decision and native cockpit effects");
		require(!input_demo_recorder_is_active() && input_demo_recorder_pending_input_operation_count() == 0,
		        "recording-off rear input creates no recording state");
		rear_cases.push_back({ { "sticky", sticky }, { "phase", phase }, { "rear", actual_active }, { "leave", actual_armed }, { "cockpit", cockpit }, { "preferred", preferred } });
	};
	for (int sticky : { 0, 1 }) {
		PlayerCfg.StickyRearview = sticky;
		tap(SDLK_r);
		check_rear(sticky, "same_batch_tap", 1, 0);
		tap(SDLK_r);
		check_rear(sticky, "toggle_off", 0, 0);
		key(SDLK_r, true);
		event_poll();
		check_rear(sticky, "held_enter", 1, 0);
		if (sticky) {
			SDL_Delay(300);
			timer_update();
		}
		SDL_Event motion = {};
		motion.type = SDL_MOUSEMOTION;
		require(SDL_PushEvent(&motion) == 0, "queue recording-off held callback");
		event_poll();
		check_rear(sticky, "held_armed", 1, 1);
		key(SDLK_r, false);
		event_poll();
		check_rear(sticky, "held_release", 0, 1);
		reset_rear_view();
		check_rear(sticky, "reset_preserves_hold_decision", 0, 1);
	}
	for (int armed : { 0, 1 }) {
		require(input_demo_restore_rear_view_state(1, armed, CM_REAR_VIEW, CM_FULL_COCKPIT, 0),
		        "restore an initially active native rear-view baseline");
		check_rear(saved_sticky, "active_baseline_restore", 1, armed);
		require(input_demo_apply_rear_view_action(armed ? INPUT_DEMO_REAR_VIEW_RELEASE_OFF : INPUT_DEMO_REAR_VIEW_TOGGLE_OFF, 0),
		        "first recorded release or toggle applies after an active baseline restore");
		check_rear(saved_sticky, armed ? "active_baseline_release" : "active_baseline_toggle", 0, armed);
	}
	PlayerCfg.StickyRearview = saved_sticky;
	json cockpit_reverse_cases = json::array();
	for (int expected : { CM_FULL_SCREEN, CM_STATUS_BAR, CM_FULL_COCKPIT }) {
		toggle_cockpit_reverse();
		require(PlayerCfg.CurrentCockpitMode == expected && PlayerCfg.PreferredCockpitMode == expected &&
		            !input_demo_recorder_is_active() && input_demo_recorder_pending_input_operation_count() == 0,
		        "recording-off reverse cockpit helper preserves native mode cycle without an invented key binding");
		cockpit_reverse_cases.push_back({ { "cockpit", expected }, { "preferred", expected } });
	}
#ifdef DXX_BUILD_DESCENT_II
	// Seed marker-only synthetic cases through the native marker creator
	for (int slot = 0; slot < NUM_MARKERS; ++slot) {
		require(MarkerObject[slot] == -1, "isolated marker slots start empty");
		std::snprintf(MarkerMessage[slot], MARKER_MESSAGE_LEN, "absolute slot %d", slot);
		DropMarker(slot);
		require(MarkerObject[slot] >= 0, "create synthetic marker with native allocator");
	}
	key(SDLK_TAB, true);
	event_poll();
	require(Automap_active && window_get_front() != Game_wind, "native map owns isolated marker confirmation");
	key(SDLK_TAB, false);
	event_poll();
	for (int player : { 0, 1 }) {
		Player_num = player;
		// Native digit selection exposes absolute slots zero through nine
		for (int slot = 0; slot < 10; ++slot) {
			for (int accepted : { 0, 1 }) {
				tap(slot == 9 ? SDLK_0 : static_cast<SDLKey>(SDLK_1 + slot));
				require(HighlightMarker == slot, "native digit selects absolute marker slot");
				int before_objects[NUM_MARKERS], before_signatures[NUM_MARKERS];
				char before_messages[NUM_MARKERS][MARKER_MESSAGE_LEN];
				std::memcpy(before_objects, MarkerObject, sizeof(before_objects));
				std::memcpy(before_messages, MarkerMessage, sizeof(before_messages));
				for (int other = 0; other < NUM_MARKERS; ++other)
					before_signatures[other] = Objects[MarkerObject[other]].signature;
				const SDL_TimerID confirmation = SDL_AddTimer(50, [](Uint32 interval, void *choice) -> Uint32 {
					const bool yes = *static_cast<const int *>(choice) != 0;
					SDL_Event event = {};
					event.type = SDL_KEYDOWN;
					event.key.state = SDL_PRESSED;
					event.key.keysym.sym = yes ? SDLK_RETURN : SDLK_ESCAPE;
					SDL_PushEvent(&event);
					event.type = SDL_KEYUP;
					event.key.state = SDL_RELEASED;
					SDL_PushEvent(&event);
					return interval; }, &accepted);
				require(confirmation != 0, "drive native marker confirmation or cancellation");
				key(SDLK_LCTRL, true);
				key(SDLK_d, true);
				event_poll();
				SDL_RemoveTimer(confirmation);
				key(SDLK_d, false);
				key(SDLK_LCTRL, false);
				event_poll();
				require(Automap_active, "native marker prompt returns to the same map");
				for (int other = 0; other < NUM_MARKERS; ++other) {
					if (accepted && other == slot) {
						require(MarkerObject[other] == -1 && MarkerMessage[other][0] == 0 &&
						            std::memcmp(MarkerMessage[other] + 1, before_messages[other] + 1, MARKER_MESSAGE_LEN - 1) == 0 &&
						            Objects[before_objects[other]].type == OBJ_NONE && Objects[before_objects[other]].signature == -1,
						        "recording-off Ctrl+D preserves exact native absolute-slot deletion effects");
					} else {
						require(MarkerObject[other] == before_objects[other] &&
						            std::memcmp(MarkerMessage[other], before_messages[other], MARKER_MESSAGE_LEN) == 0 &&
						            Objects[before_objects[other]].type == OBJ_MARKER &&
						            Objects[before_objects[other]].signature == before_signatures[other],
						        "recording-off marker deletion or cancellation preserves every other slot");
					}
				}
				require(HighlightMarker == (accepted ? -1 : slot), "native marker highlight reset or cancellation");
				require(!input_demo_recorder_is_active() && input_demo_recorder_pending_input_operation_count() == 0,
				        "recording-off remote-owned marker deletion creates no recording state");
				marker_cases.push_back({ { "player", player }, { "absolute_slot", slot }, { "accepted", accepted }, { "object", before_objects[slot] }, { "signature", before_signatures[slot] }, { "other_slots_unchanged", true } });
				if (accepted) {
					Player_num = 0;
					std::memcpy(MarkerMessage[slot], before_messages[slot], MARKER_MESSAGE_LEN);
					DropMarker(slot);
					Player_num = player;
				}
			}
		}
	}
	Player_num = 0;
	tap(SDLK_ESCAPE);
	require(!Automap_active, "isolated marker regression closes its map");
#endif
	require(GameTime64 == before_time, "isolated input regression never advances the simulation");
	input_demo_recorder_settings projection_settings;
	input_demo_recorder_settings_clear(&projection_settings);
#ifdef DXX_BUILD_DESCENT_II
	projection_settings.game = INPUT_DEMO_GAME_D2;
	projection_settings.mission = "d2demo";
#else
	projection_settings.game = INPUT_DEMO_GAME_D1;
	projection_settings.mission = "d1";
#endif
	projection_settings.level = Current_level_num;
	projection_settings.difficulty = Difficulty_level;
	projection_settings.rng_mode = "lcg_state";
	projection_settings.ordered_input = 1;
	projection_settings.has_view_projection = 1;
	input_demo_capture_view_projection(&projection_settings.view_projection);
	const fix original_zoom = Render_zoom;
	const std::vector<object> projection_objects(Objects, Objects + Highest_object_index + 1);
	const std::vector<ai_local> projection_ai(Ai_local_info, Ai_local_info + Highest_object_index + 1);
	const control_info projection_controls = Controls;
	const unsigned projection_rng_calls = d_rand_get_call_count();
	char projection_error[256] = "";
	require(input_demo_recorder_start(&projection_settings, projection_error, sizeof(projection_error)), projection_error);
	input_demo_record_input_context();
	require(input_demo_recorder_is_active(), "unchanged initialized projection permits recording");
	Render_zoom = original_zoom + 1;
	input_demo_record_input_context();
	Render_zoom = original_zoom;
	require(!input_demo_recorder_is_active() && input_demo_recorder_pending_input_operation_count() == 0,
	        "native input hook cancels recording when initialized projection changes");
	input_demo_view_projection restored_projection;
	input_demo_capture_view_projection(&restored_projection);
	require(!std::memcmp(&restored_projection, &projection_settings.view_projection, sizeof(restored_projection)) &&
	            !std::memcmp(projection_objects.data(), Objects, projection_objects.size() * sizeof(object)) &&
	            !std::memcmp(projection_ai.data(), Ai_local_info, projection_ai.size() * sizeof(ai_local)) &&
	            !std::memcmp(&projection_controls, &Controls, sizeof(Controls)) &&
	            d_rand_get_call_count() == projection_rng_calls && GameTime64 == before_time,
	        "projection cancellation guard restores projection and leaves simulation unchanged");
	const json projection_guard = { { "field", "Render_zoom" }, { "baseline", original_zoom }, { "changed", original_zoom + 1 }, { "cancelled", true }, { "restored", true } };
	const json missile_helpers = test_native_missile_view_helpers_recording_off();
	const json missile_map = test_native_missile_view_map_recording_off();
	const json report = { { "game", game_name }, { "scenario", "marker-delete-native" }, { "recording", false }, { "public_replay_admission", false }, { "rear_cases", rear_cases }, { "marker_cases", marker_cases }, { "cockpit_reverse_cases", cockpit_reverse_cases }, { "projection_guard", projection_guard }, { "missile_helpers", missile_helpers }, { "missile_map", missile_map } };
	const auto text = report.dump(2) + "\n";
	write_fixture("capture.json", bytes(text.begin(), text.end()));
	std::puts("PASS: isolated recording-off rear-view and absolute-slot marker semantics");
}

// Native SDL modal input coverage from an ordinary, unmodified level start
static void capture_public_modal_frames(const char *game_name, const char *mission, bool active_tail)
{
	using nlohmann::json;
	constexpr int frame_count = 16;
	constexpr int redraw_count = 1100;
	char error[256] = "";
	kc_set_controls();
	require(PlayerCfg.KeySettings[0][36] == KEY_R && PlayerCfg.KeySettings[0][44] == KEY_TAB,
	        "default native rear-view and automap bindings");
	require(PlayerCfg.KeySettings[0][30] == KEY_A && PlayerCfg.KeySettings[0][26] == KEY_SPACEBAR,
	        "default native movement and secondary bindings");
	require(input_demo_state_trace_start("capture-observation.jsonl", "record", game_name, mission, 1,
	                                     Difficulty_level, "new_level", frame_count, error, sizeof(error)),
	        error);
	event_flush();
	const int saved_sticky = PlayerCfg.StickyRearview;
	const auto key = [](SDLKey symbol, bool down) {
		SDL_Event event = {};
		event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
		event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
		event.key.keysym.sym = symbol;
		if (symbol >= SDLK_SPACE && symbol <= SDLK_z) event.key.keysym.unicode = static_cast<Uint16>(symbol);
		require(SDL_PushEvent(&event) == 0, "queue native modal key");
	};
	const auto tap = [&](SDLKey symbol) {
		key(symbol, true);
		key(symbol, false);
		event_poll();
	};
	const auto mouse = []() {
		SDL_Event event = {};
		event.type = SDL_MOUSEMOTION;
		require(SDL_PushEvent(&event) == 0, "queue native modal mouse callback");
		event_poll();
	};
	const auto hold_delay = []() {
		SDL_Delay(300);
		timer_update();
	};
	const auto rear = [](int active, int armed) {
		int actual_active, actual_armed, cockpit, preferred;
		input_demo_get_rear_view_state(&actual_active, &actual_armed, &cockpit, &preferred);
		require(actual_active == active && actual_armed == armed, "native rear-view state and hold decision");
		require(cockpit == (active && preferred == CM_FULL_COCKPIT ? CM_REAR_VIEW : preferred),
		        "native rear-view cockpit transition and preferred-mode restore");
	};
	const auto open_map = [&]() {
		require(!Automap_active && window_get_front() == Game_wind, "ordinary game window before map entry");
		key(SDLK_TAB, true);
		event_poll();
		require(Automap_active && window_get_front() != Game_wind, "SDL Tab opens native automap");
	};
	const auto draw_map = [](int count) {
		require(Automap_active && window_get_front() != Game_wind, "draw actual native automap window");
		d_event event = { EVENT_WINDOW_DRAW };
		for (int draw = 0; draw < count; ++draw)
			window_send_event(window_get_front(), &event);
	};
	const auto escape_map = [&]() {
		key(SDLK_ESCAPE, true);
		event_poll();
		require(!Automap_active && window_get_front() == Game_wind, "SDL Escape closes native automap");
		key(SDLK_ESCAPE, false);
		event_poll();
	};
	json observations = json::array();
	json checks = json::array();
	const auto observe = [&](int frame, const char *phase) {
		int active, armed, cockpit, preferred;
		input_demo_get_rear_view_state(&active, &armed, &cockpit, &preferred);
		unsigned int visited_hash = 2166136261u;
		for (int segment = 0; segment <= Highest_segment_index; ++segment)
			visited_hash = (visited_hash ^ Automap_visited[segment]) * 16777619u;
		observations.push_back({ { "frame", frame }, { "phase", phase }, { "game_time", GameTime64 }, { "frame_time", FrameTime }, { "rear", active }, { "leave", armed }, { "cockpit", cockpit }, { "preferred", preferred }, { "automap", Automap_active }, { "visited_hash", visited_hash }, { "ammo", int(Players[0].secondary_ammo[0]) }, { "energy", int(Players[0].energy) }, { "pending_operations", input_demo_recorder_pending_input_operation_count() } });
	};
	const vms_vector starting_position = ConsoleObject->pos;
	int marker_object = -1, marker_signature = -1;
	const char *marker_message = "modal capture";
	json marker = { { "supported", false } };
	for (int frame = 0; frame < frame_count; ++frame) {
		FrameTime = frame % 2 ? 1638 : 1092;
		if (frame == 0) {
			tap(SDLK_F3);
			require(PlayerCfg.CurrentCockpitMode == CM_STATUS_BAR && PlayerCfg.PreferredCockpitMode == CM_STATUS_BAR,
			        "native F3 selects status-bar cockpit across a simulation boundary");
			checks.push_back("native_f3_status_bar");
		}
		if (frame == 1) {
			PlayerCfg.StickyRearview = 1;
			tap(SDLK_r);
			rear(1, 0);
			checks.push_back("sticky_same_batch_tap");
		}
		if (frame == 2) {
			tap(SDLK_r);
			rear(0, 0);
			checks.push_back("sticky_toggle_off");
			const int cockpit = PlayerCfg.CurrentCockpitMode, preferred = PlayerCfg.PreferredCockpitMode;
			key(SDLK_LSHIFT, true);
			tap(SDLK_F3);
			key(SDLK_LSHIFT, false);
			event_poll();
			require(PlayerCfg.CurrentCockpitMode == cockpit && PlayerCfg.PreferredCockpitMode == preferred,
			        "shifted F3 remains an unbound native no-op");
			checks.push_back("native_shift_f3_noop");
		}
		if (frame == 3) {
			key(SDLK_r, true);
			event_poll();
			rear(1, 0);
			hold_delay();
			mouse();
			rear(1, 1);
			key(SDLK_r, false);
			event_poll();
			rear(0, 1);
			checks.push_back("sticky_native_clock_hold_release");
			tap(SDLK_F3);
			require(PlayerCfg.CurrentCockpitMode == CM_FULL_SCREEN && PlayerCfg.PreferredCockpitMode == CM_FULL_SCREEN,
			        "native F3 selects fullscreen before subsequent rear-view input");
			checks.push_back("native_f3_fullscreen");
		}
		if (frame == 4) {
			PlayerCfg.StickyRearview = 0;
			tap(SDLK_r);
			rear(1, 0);
			checks.push_back("nonsticky_same_batch_tap");
		}
		if (frame == 5) {
			tap(SDLK_r);
			rear(0, 0);
			checks.push_back("nonsticky_toggle_off");
		}
		if (frame == 6) {
			key(SDLK_r, true);
			event_poll();
			mouse();
			rear(1, 1);
			key(SDLK_r, false);
			event_poll();
			rear(0, 1);
			checks.push_back("nonsticky_held_callback_release");
			tap(SDLK_F3);
			require(PlayerCfg.CurrentCockpitMode == CM_FULL_COCKPIT && PlayerCfg.PreferredCockpitMode == CM_FULL_COCKPIT,
			        "native F3 restores full cockpit before later rear-view and map transitions");
			checks.push_back("native_f3_full_cockpit_restore");
		}
		if (frame == 7) {
			const int ammunition = Players[0].secondary_ammo[0];
			key(SDLK_a, true);
			key(SDLK_LCTRL, true);
			key(SDLK_SPACE, true);
			key(SDLK_SPACE, false);
			event_poll();
			require(Players[0].secondary_ammo[0] == ammunition - 1 && Controls.forward_thrust_time > 0,
			        "ordinary movement and secondary fire before map entry");
			checks.push_back("movement_and_fire_before_map");
		}
		if (frame == 8) {
			require(std::memcmp(&starting_position, &ConsoleObject->pos, sizeof(starting_position)) != 0,
			        "ordinary thrust moved the unmodified player");
			open_map();
			observe(frame, "map_open");
			key(SDLK_TAB, false);
			key(SDLK_a, false);
			key(SDLK_LCTRL, false);
			event_poll();
			require(Automap_active, "native map tap stays open");
			const auto before_time = GameTime64;
			const int ammunition = Players[0].secondary_ammo[0];
			const fix energy = Players[0].energy;
			const vms_vector position = ConsoleObject->pos;
			tap(SDLK_SPACE);
			tap(SDLK_LCTRL);
			tap(SDLK_f);
			const auto operations = input_demo_recorder_pending_input_operation_count();
			draw_map(redraw_count);
			require(input_demo_recorder_is_active() && input_demo_recorder_pending_input_operation_count() == operations,
			        "1100 real map redraws do not grow ordered input");
			require(GameTime64 == before_time && Players[0].secondary_ammo[0] == ammunition &&
			            Players[0].energy == energy && std::memcmp(&position, &ConsoleObject->pos, sizeof(position)) == 0,
			        "native map inputs and redraws consume no ammunition, energy, movement or simulation time");
			observe(frame, "after_redraws");
			key(SDLK_TAB, true);
			event_poll();
			require(!Automap_active, "second native Tab toggles map closed");
			key(SDLK_TAB, false);
			event_poll();
			checks.push_back("map_tap_toggle");
			checks.push_back("map_fire_is_not_gameplay_fire");
			checks.push_back("1100_redraws_without_operation_growth");
		}
		if (frame == 9) {
			open_map();
			key(SDLK_TAB, false);
			event_poll();
			draw_map(1);
			escape_map();
			checks.push_back("map_escape");
		}
		if (frame == 10) {
			open_map();
			hold_delay();
			draw_map(1);
			key(SDLK_TAB, false);
			event_poll();
			// Native entry flushes controls and does not seed am->controls from the opening key
			require(Automap_active, "native long map hold retains its original toggle behavior");
			observe(frame, "long_map_hold_release");
			escape_map();
			checks.push_back("map_long_hold_preserves_native_toggle");
		}
		if (frame == 11 || frame == 12) {
			PlayerCfg.StickyRearview = frame == 11;
			key(SDLK_r, true);
			event_poll();
			open_map();
			rear(1, frame == 12);
			hold_delay();
			draw_map(2);
			key(SDLK_r, false);
			key(SDLK_TAB, false);
			event_poll();
			rear(1, frame == 12);
			observe(frame, "rear_held_through_map");
			escape_map();
			mouse();
			if (frame == 11) {
				rear(1, 0);
				tap(SDLK_r);
				rear(0, 0);
			} else rear(0, 1);
			checks.push_back(frame == 11 ? "unarmed_rear_hold_through_map" : "armed_rear_hold_through_map");
		}
#ifdef DXX_BUILD_DESCENT_II
		if (frame == 13) {
			require(MarkerObject[0] == -1, "ordinary level has free marker slot zero");
			tap(SDLK_F4);
			for (const char *letter = marker_message; *letter; ++letter) tap(static_cast<SDLKey>(*letter));
			tap(SDLK_RETURN);
			marker_object = MarkerObject[0];
			require(marker_object >= 0 && Objects[marker_object].type == OBJ_MARKER &&
			            Objects[marker_object].id == 0 && std::strcmp(MarkerMessage[0], marker_message) == 0,
			        "ordinary F4 marker entry creates exact message and object");
			marker_signature = Objects[marker_object].signature;
			marker = { { "supported", true }, { "slot", 0 }, { "object", marker_object }, { "signature", marker_signature }, { "message", marker_message }, { "deleted", false } };
			checks.push_back("native_marker_drop");
		}
		if (frame == 14) {
			open_map();
			key(SDLK_TAB, false);
			event_poll();
			tap(SDLK_1);
			require(MarkerObject[0] == marker_object && Objects[marker_object].signature == marker_signature &&
			            std::strcmp(MarkerMessage[0], marker_message) == 0,
			        "selected native marker is the original object and message");
			// The native confirmation owns a nested event loop and flushes queued input on activation
			const SDL_TimerID confirmation = SDL_AddTimer(100, [](Uint32 interval, void *) -> Uint32 {
				SDL_Event event = {};
				event.type = SDL_KEYDOWN;
				event.key.state = SDL_PRESSED;
				event.key.keysym.sym = SDLK_RETURN;
				SDL_PushEvent(&event);
				event.type = SDL_KEYUP;
				event.key.state = SDL_RELEASED;
				SDL_PushEvent(&event);
				return interval; }, nullptr);
			require(confirmation != 0, "queue native marker-deletion confirmation");
			key(SDLK_LCTRL, true);
			key(SDLK_d, true);
			event_poll();
			SDL_RemoveTimer(confirmation);
			key(SDLK_d, false);
			key(SDLK_LCTRL, false);
			event_poll();
			require(MarkerObject[0] == -1 && MarkerMessage[0][0] == 0 &&
			            Objects[marker_object].type == OBJ_NONE && Objects[marker_object].signature == -1,
			        "confirmed native Ctrl+D deletes the exact marker object, signature and message");
			marker["deleted"] = true;
			escape_map();
			checks.push_back("native_confirmed_marker_delete");
		}
#else
		(void) marker_object;
		(void) marker_signature;
		(void) marker_message;
		if (frame == 13 || frame == 14) event_poll();
#endif
		if (frame == 15) event_poll();
		require(!Automap_active, "every simulation boundary has a closed native map");
		observe(frame, "before_simulation");
		// Native input uses the previous interval, while this simulation gets its next fixed step
		FrameTime = frame % 2 ? 3276 : 2184;
		calc_game_time();
		GameProcessFrame();
		require(render_update_game_view_automap(), "modal capture native camera map discovery");
		require(render_update_main_view_objects(), "modal capture gameplay view state");
		observe(frame, "after_simulation");
	}
	PlayerCfg.StickyRearview = saved_sticky;
	if (active_tail) {
		FrameTime = 1365;
		open_map();
		key(SDLK_TAB, false);
		event_poll();
		draw_map(3);
		require(Automap_active, "terminal input tail retains an active native map");
		checks.push_back("active_map_terminal_tail");
		observe(frame_count, "active_tail");
	}
	input_demo_result result;
	input_demo_capture_current_result(&result);
	require(input_demo_record_finish_input_tail(error, sizeof(error)), error);
	require(input_demo_recorder_flush_with_result("capture.dximdemo", &result, error, sizeof(error)), error);
	input_demo_state_trace_stop();
	newdemo_write_end();
	require(PHYSFS_close(outfile), "close modal capture classic sink");
	outfile = nullptr;
	Newdemo_state = ND_STATE_NORMAL;
	const json report = { { "game", game_name }, { "mission", mission }, { "scenario", active_tail ? "ordered-modal-tail" : "ordered-modal" }, { "frames", frame_count }, { "redraw_count", redraw_count }, { "checks", checks }, { "marker", marker }, { "observations", observations }, { "terminal_automap", Automap_active } };
	const auto text = report.dump(2) + "\n";
	write_fixture("capture.json", bytes(text.begin(), text.end()));
	std::puts("PASS: ordinary SDL rear-view, automap and marker capture");
}
