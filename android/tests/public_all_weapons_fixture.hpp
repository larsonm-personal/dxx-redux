#include "weapon_cheat_shared.h"

// Fresh-level capture driven by native SDL text and ordinary weapon controls
static void capture_public_all_weapons(const char *game_name, const char *mission, bool bigred)
{
	using nlohmann::json;
	constexpr int frames = 40;
	char error[256] = "";
#ifdef DXX_BUILD_DESCENT_II
	const bool d1_profile = d1_in_d2_use_d1_gameplay();
	const char *code = "honestbob";
	const int grant = weapon_cheat_default_grant();
	require(!bigred, "bigred is a native D1 input spelling");
#else
	const bool d1_profile = true;
	const char *code = bigred ? "bigred" : "scourge";
	const int grant = bigred ? INPUT_DEMO_GRANT_ALL_WEAPONS_D1_BIGRED : INPUT_DEMO_GRANT_ALL_WEAPONS_D1_SCOURGE;
#endif
	const int vulcan_grant_max = d1_profile ? 784 : Primary_ammo_max[VULCAN_INDEX];
	const auto key = [](SDLKey symbol, bool down, bool text = false) {
		SDL_Event event = {};
		event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
		event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
		event.key.keysym.sym = symbol;
		if (text && down) event.key.keysym.unicode = static_cast<Uint16>(symbol);
		require(SDL_PushEvent(&event) == 0, "queue native all-weapons key");
	};
	const auto tap = [&](SDLKey symbol, bool text = false) {
		key(symbol, true, text);
		key(symbol, false);
		event_poll();
	};
	const auto type = [&](const char *text) {
		for (; *text; ++text) tap(static_cast<SDLKey>(*text), true);
	};
	const auto inventory = [&]() {
		const auto &p = Players[0];
		json primary = json::array(), secondary = json::array();
		for (int i = 0; i < MAX_PRIMARY_WEAPONS; ++i) primary.push_back(p.primary_ammo[i]);
		for (int i = 0; i < MAX_SECONDARY_WEAPONS; ++i) secondary.push_back(p.secondary_ammo[i]);
		return json{ { "primary_flags", p.primary_weapon_flags }, { "secondary_flags", p.secondary_weapon_flags }, { "primary_ammo", primary }, { "secondary_ammo", secondary }, { "energy", p.energy }, { "laser_level", p.laser_level }, { "flags", p.flags }, { "score", p.score }, { "cheats_enabled", int(cheats.enabled) }, { "primary_selected", p.primary_weapon }, { "secondary_selected", p.secondary_weapon } };
	};
	kc_set_controls();
	event_flush();
	require(!cheats.enabled && Players[0].primary_weapon_flags == 1 && Players[0].secondary_weapon_flags == 1 &&
	            Players[0].laser_level == 0 && Players[0].secondary_ammo[0] == 5 && Players[0].energy == INITIAL_ENERGY,
	        "ordinary new-level inventory before recorded grant");
	const json initial = inventory();
	require(input_demo_state_trace_start("capture-observation.jsonl.gz", "record", game_name, mission, 1,
	                                     Difficulty_level, "new_level", frames, error, sizeof(error)),
	        error);
	json observations = json::array(), shots = json::array();
	std::vector<int> seen_signatures;
	bool spreadfire_seen = false, gauss_seen = false;
	for (int frame = 0; frame < frames; ++frame) {
		FrameTime = F1_0 / 8;
		if (frame == 1) {
			tap(SDLK_3);
			require(Players[0].primary_weapon == LASER_INDEX, "selection before grant cannot obtain Spreadfire");
		}
		if (frame == 2) {
#ifndef DXX_BUILD_DESCENT_II
			type("gabbagabbahey");
			require(cheats.enabled && Players[0].primary_weapon_flags == 1, "D1 enable is recorded without granting inventory");
#endif
			type(code);
			require(input_demo_recorder_is_active() && Players[0].secondary_ammo[0] == 20 &&
			            Players[0].energy == MAX_ENERGY && cheats.enabled && Players[0].score == 0 &&
			            (Players[0].flags & PLAYER_FLAGS_QUAD_LASERS),
			        "native grant updates inventory and ordinary cheat effects during recording");
			require(Players[0].laser_level == (d1_profile ? MAX_LASER_LEVEL : 5), "grant respects the active laser profile");
			observations.push_back({ { "frame", frame }, { "phase", "first_grant" }, { "inventory", inventory() } });
			tap(SDLK_SPACE);
			require(Players[0].secondary_ammo[0] == 19, "ordinary missile fire spends the first grant in the same input batch");
			observations.push_back({ { "frame", frame }, { "phase", "between_grants" }, { "inventory", inventory() } });
			type(code);
			require(Players[0].secondary_ammo[0] == 20, "second same-batch grant replenishes after fire in native order");
			observations.push_back({ { "frame", frame }, { "phase", "repeated_grant" }, { "inventory", inventory() } });
		}
		if (frame == 3) {
			tap(SDLK_3);
			require(Players[0].primary_weapon == SPREADFIRE_INDEX, "ordinary native selection obtains granted Spreadfire");
		}
		if (frame == 4) key(SDLK_LCTRL, true);
		if (frame == 18) key(SDLK_LCTRL, false);
#ifdef DXX_BUILD_DESCENT_II
		if (frame == 20 && !d1_profile) {
			tap(SDLK_2);
			tap(SDLK_2);
			require(Players[0].primary_weapon == GAUSS_INDEX, "ordinary native toggle obtains granted Gauss");
		}
#endif
		if (frame == 21) key(SDLK_LCTRL, true);
		if (frame == 33) key(SDLK_LCTRL, false);
		if (frame == 34) {
			event_poll();
			observations.push_back({ { "frame", frame }, { "phase", "spent_inventory" }, { "inventory", inventory() } });
			type(code);
			require(Players[0].energy == MAX_ENERGY && Players[0].primary_ammo[VULCAN_INDEX] == vulcan_grant_max,
			        "later grant replenishes spent energy and ammunition");
			observations.push_back({ { "frame", frame }, { "phase", "later_grant" }, { "inventory", inventory() } });
		}
		if (frame == 35) key(SDLK_LCTRL, true);
		if (frame == 37) key(SDLK_LCTRL, false);
		event_poll();
		calc_game_time();
		GameProcessFrame();
		require(render_update_game_view_automap() && render_update_main_view_objects(), "capture native gameplay view");
		for (int slot = 0; slot <= Highest_object_index; ++slot) {
			const object &obj = Objects[slot];
			if (obj.type != OBJ_WEAPON || obj.ctype.laser_info.parent_type != OBJ_PLAYER ||
			    obj.ctype.laser_info.parent_num != Players[0].objnum ||
			    std::find(seen_signatures.begin(), seen_signatures.end(), obj.signature) != seen_signatures.end()) continue;
			seen_signatures.push_back(obj.signature);
			shots.push_back({ { "frame", frame }, { "object", slot }, { "signature", obj.signature }, { "weapon_id", obj.id }, { "selected", int(Players[0].primary_weapon) } });
			if (obj.id == (d1_profile ? 20 : SPREADFIRE_ID)) spreadfire_seen = true;
#ifdef DXX_BUILD_DESCENT_II
			if (!d1_profile && obj.id == GAUSS_ID) gauss_seen = true;
#endif
		}
		if (std::getenv("DXX_PUBLIC_CAPTURE_SCREENSHOTS") && (frame == 2 || frame == 13 || frame == 30)) {
			GameArg.SysInputDemoNoRender = 0;
			game_render_frame_mono(1);
			save_screen_shot(1);
			GameArg.SysInputDemoNoRender = 1;
		}
		require(input_demo_recorder_is_active(), "all native grant/control paths remain recordable");
	}
	require(spreadfire_seen && (d1_profile || gauss_seen), "actual granted Spreadfire and native D2 Gauss projectiles were created");
	FrameTime = F1_0 / 16;
	type(code);
	require(input_demo_recorder_is_active() && Players[0].energy == MAX_ENERGY &&
	            Players[0].primary_ammo[VULCAN_INDEX] == vulcan_grant_max,
	        "terminal input-tail grant applies without another simulation frame");
	observations.push_back({ { "frame", frames }, { "phase", "tail_grant" }, { "inventory", inventory() } });
	input_demo_result result;
	input_demo_capture_current_result(&result);
	require(input_demo_record_finish_input_tail(error, sizeof(error)), error);
	require(input_demo_recorder_flush_with_result("capture.dximdemo", &result, error, sizeof(error)), error);
	input_demo_state_trace_stop();
	newdemo_write_end();
	require(PHYSFS_close(outfile), "close all-weapons classic callback sink");
	outfile = nullptr;
	Newdemo_state = ND_STATE_NORMAL;
	const json terminal = inventory();

	// Isolated post-capture policy tests never alter the immutable live recording
	input_demo_replay_direct_command_event event = {};
	event.kind = INPUT_DEMO_REPLAY_DIRECT_COMMAND_GRANT_ALL_WEAPONS;
	event.value0 = grant;
	const player saved_player = Players[0];
	const game_cheats saved_cheats = cheats;
	const int selected_before = Players[0].primary_weapon;
	do_weapon_select(d1_profile ? PLASMA_INDEX : FUSION_INDEX, 0);
	require(Players[0].primary_weapon == selected_before, "excluded shareware direct selection remains unavailable");
	for (int validate_only : { 1, 0 }) {
		event.value0 = d1_profile ? INPUT_DEMO_GRANT_ALL_WEAPONS_D2_HONESTBOB : INPUT_DEMO_GRANT_ALL_WEAPONS_D1_SCOURGE;
		require(!input_demo_apply_ordered_direct_command(&event, validate_only, error, sizeof(error)), "wrong gameplay-profile grant fails before mutation");
		event.value0 = grant;
		event.value1 = 1;
		require(!input_demo_apply_ordered_direct_command(&event, validate_only, error, sizeof(error)), "unused grant argument fails before mutation");
		event.value1 = 0;
	}
	const int saved_endlevel = Endlevel_sequence;
	Endlevel_sequence = 1;
	for (int validate_only : { 1, 0 })
		require(!input_demo_apply_ordered_direct_command(&event, validate_only, error, sizeof(error)), "endlevel grants reject a phase bypassed by native cheat input");
	Endlevel_sequence = saved_endlevel;
	const int saved_dead = Player_is_dead;
	Player_is_dead = 1;
	for (int validate_only : { 1, 0 })
		require(!input_demo_apply_ordered_direct_command(&event, validate_only, error, sizeof(error)), "dead-phase grants fail before mutation");
	Player_is_dead = saved_dead;
	require(input_demo_apply_ordered_direct_command(&event, 1, error, sizeof(error)), "valid grant preflight");
	require(!std::memcmp(&saved_player, &Players[0], sizeof(player)) && !std::memcmp(&saved_cheats, &cheats, sizeof(cheats)), "preflight and invalid grants are mutation-free");
	const std::vector<int> variants = d1_profile ? std::vector<int>{ INPUT_DEMO_GRANT_ALL_WEAPONS_D1_SCOURGE, INPUT_DEMO_GRANT_ALL_WEAPONS_D1_BIGRED } : std::vector<int>{ INPUT_DEMO_GRANT_ALL_WEAPONS_D2_HONESTBOB };
#ifdef DXX_BUILD_DESCENT_II
	const int saved_ham_version = Piggy_hamfile_version;
#endif
	for (const int variant : variants) {
		for (const int edition : { 2, 3 }) {
			Players[0] = saved_player;
			cheats = saved_cheats;
			Players[0].energy = 2;
			Players[0].shields = 3;
			Players[0].score = 1337;
			Players[0].primary_weapon_flags = HAS_PLASMA_FLAG;
			Players[0].secondary_weapon_flags = HAS_MEGA_FLAG;
			Players[0].flags |= PLAYER_FLAGS_CLOAKED | PLAYER_FLAGS_BLUE_KEY;
			for (int i = 0; i < MAX_PRIMARY_WEAPONS; ++i) Players[0].primary_ammo[i] = i + 3;
			for (int i = 0; i < MAX_SECONDARY_WEAPONS; ++i) Players[0].secondary_ammo[i] = i + 2;
			player expected = Players[0];
			expected.energy = MAX_ENERGY;
			expected.score = 0;
			expected.flags |= PLAYER_FLAGS_QUAD_LASERS;
			expected.laser_level = d1_profile ? MAX_LASER_LEVEL : 5;
			if (d1_profile) {
				const int count = variant == INPUT_DEMO_GRANT_ALL_WEAPONS_D1_SCOURGE ? 3 : 5;
				if (count == 3) {
					expected.primary_weapon_flags |= 0xe7;
					expected.secondary_weapon_flags |= 0xe7;
				} else {
					expected.primary_weapon_flags = 0xff;
					expected.secondary_weapon_flags = 0xff;
				}
				for (int i = 0; i < count; ++i) {
					expected.primary_ammo[i] = i == VULCAN_INDEX ? 784 : Primary_ammo_max[i];
					expected.secondary_ammo[i] = Secondary_ammo_max[i];
				}
			} else {
#ifdef DXX_BUILD_DESCENT_II
				Piggy_hamfile_version = edition;
				expected.primary_weapon_flags = edition == 2 ? 0xfccf : 0xffdf;
				expected.secondary_weapon_flags = edition == 2 ? 0xfcef : 0xffff;
				for (int i = 0; i < MAX_PRIMARY_WEAPONS; ++i) expected.primary_ammo[i] = Primary_ammo_max[i];
				for (int i = 0; i < MAX_SECONDARY_WEAPONS; ++i) expected.secondary_ammo[i] = Secondary_ammo_max[i];
				if (edition == 2) expected.secondary_ammo[MEGA_INDEX] = expected.secondary_ammo[SMISSILE4_INDEX] = expected.secondary_ammo[SMISSILE5_INDEX] = 0;
#endif
			}
#ifndef DXX_BUILD_DESCENT_II
			const int toggle_before = variant == INPUT_DEMO_GRANT_ALL_WEAPONS_D1_BIGRED ? cheats.wowie2 : cheats.wowie;
#else
			const int toggle_before = cheats.wowie;
#endif
			weapon_cheat_execute_live(variant);
#ifndef DXX_BUILD_DESCENT_II
			require((variant == INPUT_DEMO_GRANT_ALL_WEAPONS_D1_BIGRED ? cheats.wowie2 : cheats.wowie) == !toggle_before, "ordinary D1 grant toggles its matching cheat field");
#else
			require(cheats.wowie == !toggle_before, "ordinary D2 grant toggles its cheat field");
#endif
			require(!std::memcmp(&expected, &Players[0], sizeof(player)) && cheats.enabled && !input_demo_recorder_is_active() && input_demo_recorder_pending_input_operation_count() == 0,
			        "recording-off shareware/retail policy preserves exact grant effects and all unrelated player fields");
		}
	}
#ifdef DXX_BUILD_DESCENT_II
	Piggy_hamfile_version = saved_ham_version;
#endif
	if (d1_profile) {
		Players[0] = saved_player;
		Players[0].score = 42;
		player expected = Players[0];
		expected.score = 0;
		for (int repeat = 0; repeat < 2; ++repeat) {
			weapon_cheat_execute_live(0);
			require(cheats.enabled && !std::memcmp(&expected, &Players[0], sizeof(player)), "repeated D1 enablement changes only score and enablement");
		}
	}
	Players[0] = saved_player;
	cheats = saved_cheats;
	input_demo_recorder_settings settings;
	input_demo_recorder_settings_clear(&settings);
	settings.game = std::strcmp(game_name, "d1") == 0 ? INPUT_DEMO_GAME_D1 : INPUT_DEMO_GAME_D2;
	settings.mission = mission;
	settings.level = 1;
	settings.difficulty = Difficulty_level;
	settings.rng_mode = "lcg_state";
	for (int ordered : { 1, 0 }) {
		settings.ordered_input = ordered;
		settings.has_view_projection = ordered;
		input_demo_capture_view_projection(&settings.view_projection);
		settings.has_rear_view_baseline = ordered;
		input_demo_get_rear_view_state(&settings.rear_view_baseline.active, &settings.rear_view_baseline.leave_mode,
		                               &settings.rear_view_baseline.current_cockpit, &settings.rear_view_baseline.preferred_cockpit);
		require(input_demo_recorder_start(&settings, error, sizeof(error)), error);
		const int flags = Players[0].flags;
#ifdef DXX_BUILD_DESCENT_II
		const char *unsupported = "almighty";
#else
		const char *unsupported = "racerx";
#endif
		for (; *unsupported; ++unsupported) {
			tap(static_cast<SDLKey>(*unsupported), true);
			if (unsupported[1]) require(input_demo_recorder_is_active(), "unsupported cheat capture survives until exact native recognition");
		}
		require(!input_demo_recorder_is_active() && ((flags ^ Players[0].flags) & PLAYER_FLAGS_INVULNERABLE),
		        "unsupported native cheat cancels capture but retains its ordinary gameplay effect");
	}
	Players[0] = saved_player;
	cheats = saved_cheats;
	const json report = { { "game", game_name }, { "mission", mission }, { "scenario", bigred ? "all-weapons-bigred" : "all-weapons" }, { "profile", d1_profile ? "d1" : "d2" }, { "grant", grant }, { "native_code", code }, { "frames", frames }, { "initial", initial }, { "observations", observations }, { "shots", shots }, { "terminal", terminal }, { "post_capture_policy_checks", "pass" }, { "retail_assets_playback", false }, { "android_device_run", false } };
	const auto text = report.dump(2) + "\n";
	write_fixture("capture.json", bytes(text.begin(), text.end()));
	std::puts("PASS: native all-weapons input, repeated grants, profile guards and real firing");
}
