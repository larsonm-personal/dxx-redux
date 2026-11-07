#ifndef XL_OMEGA_DAMAGE_PROBE_H
#define XL_OMEGA_DAMAGE_PROBE_H
#ifndef DXX_XL_DEVELOPER_PROBE
#error Omega damage controls require a developer build
#endif
extern "C" {
int allowed_to_fire_laser(void);
extern fix64 Last_omega_fire_time;
void collide_robot_and_weapon(object *, object *, vms_vector *);
}
static int omega_completion_writes = 0;
static void omega_completion_write()
{
	++omega_completion_writes;
}
static void omega_completion_stop(int write)
{
	omega_completion_writes += write;
	input_demo_replay_unload();
}
static nlohmann::ordered_json omega_replay_policy_controls(const char *path)
{
	const auto require = [](bool ok, const char *message) { if (!ok) throw std::runtime_error(message); };
	const auto flags = Players[Player_num].primary_weapon_flags;
	const bool xl = xl_level_extensions_active();
	Players[Player_num].primary_weapon_flags &= ~(1 << OMEGA_INDEX);
	require(!xl_omega_input_demo_error(), "Omega-free input was rejected");
	require(maybe_start_input_demo_recording(0), "Actual Omega-free recorder did not start");
	input_demo_control_state controls{};
	input_demo_control_pulse pulse{};
	input_demo_result result{};
	input_demo_capture_current_result(&result);
	result.frame_count = 1;
	unsigned seed = 0;
	d_rand_get_stream_state(D_RNG_SIM, &seed);
	char error[256]{};
	require(input_demo_recorder_capture_frame(F1_0 / 60, &controls, &pulse, seed, 1, d_rand_get_call_count(), nullptr, nullptr, error, sizeof(error)), error);
	require(input_demo_record_finish_input_tail(error, sizeof(error)), error);
	const std::string replay_path = std::string(path) + ".dximdemo";
	require(input_demo_recorder_flush_with_result(replay_path.c_str(), &result, error, sizeof(error)), error);
	input_demo_recorder_cancel();
	Players[Player_num].primary_weapon_flags |= 1 << OMEGA_INDEX;
	require(bool(xl_omega_input_demo_error()) == xl, "Omega input admission crossed the classic boundary");
	nlohmann::ordered_json results = { { "omega_free_recording", true }, { "xl", xl }, { "rejected_paths", nlohmann::ordered_json::array() } };
	if (xl) {
		require(!maybe_start_input_demo_recording(0) && !input_demo_recorder_is_active(), "Owned Omega recording was accepted");
		for (int route = 0; route < 3; ++route) {
			require(input_demo_replay_load(replay_path.c_str(), error, sizeof(error)), error);
			const auto before = xl_runtime::weapon_clock_snapshot();
			const auto time = GameTime64;
			// Keep the fixture window alive while exercising the real abort callback.
			window *fixture_window = Game_wind;
			Game_wind = nullptr;
			if (route == 0) require(!input_demo_step_replay_frame(), "Headless Omega replay advanced");
			else {
				if (route == 2) {
					struct {
						event_type type;
						int keycode;
					} pause{ EVENT_KEY_COMMAND, KEY_SPACEBAR };
					window_send_event(fixture_window, reinterpret_cast<d_event *>(&pause));
				}
				d_event draw{};
				draw.type = EVENT_WINDOW_DRAW;
				window_send_event(fixture_window, &draw);
			}
			Game_wind = fixture_window;
			require(!input_demo_replay_is_loaded() && !input_demo_replay_actual_result_written() && GameTime64 == time && xl_runtime::weapon_clock_snapshot() == before, "Unsupported replay mutated clocks or accepted a result");
			results["rejected_paths"].push_back(route == 0 ? "headless" : route == 1 ? "rendered"
			                                                                         : "paused");
		}
		Players[Player_num].primary_weapon_flags &= ~(1 << OMEGA_INDEX);
		require(maybe_start_input_demo_recording(0), "Acquisition recording did not start");
		Players[Player_num].primary_weapon_flags |= 1 << OMEGA_INDEX;
		require(!input_demo_record_finish_input_tail(error, sizeof(error)) && !input_demo_recorder_is_active(), "Acquired Omega recording was accepted");
		results["acquired_recording_rejected"] = true;
		// Exercise the final-step exit entry before the outer post-step gate runs.
		require(input_demo_replay_load(replay_path.c_str(), error, sizeof(error)), error);
		omega_completion_writes = 0;
		fix64 timer = 1;
		require(input_demo_finish_replay_shared(0, &timer, nullptr, omega_completion_write), "Final-step exit was not handled");
		require(!omega_completion_writes && !input_demo_replay_is_loaded() && timer == 0, "Final-step exit published unsupported Omega success");
		results["simulation_exit_rejected"] = true;

		// Build a legacy terminal tail containing a real authored all-weapons command.
		// Low-level recorder APIs intentionally bypass the new live-capture admission.
		Players[Player_num].primary_weapon_flags &= ~(1 << OMEGA_INDEX);
		require(maybe_start_input_demo_recording(0), "Terminal-tail fixture recorder did not start");
		d_rand_get_stream_state(D_RNG_SIM, &seed);
		require(input_demo_recorder_capture_frame(F1_0 / 60, &controls, &pulse, seed, 1, d_rand_get_call_count(), nullptr, nullptr, error, sizeof(error)), error);
		require(input_demo_recorder_begin_input_batch(seed, d_rand_get_call_count(), FrameTime, error, sizeof(error)), error);
		require(input_demo_recorder_stage_direct_command_grant_all_weapons(INPUT_DEMO_GRANT_ALL_WEAPONS_D2_HONESTBOB, error, sizeof(error)), error);
		require(input_demo_recorder_finish_input_tail(&controls, &pulse, seed, d_rand_get_call_count(), error, sizeof(error)), error);
		const std::string tail_path = std::string(path) + ".tail.dximdemo";
		require(input_demo_recorder_flush_with_result(tail_path.c_str(), &result, error, sizeof(error)), error);
		input_demo_recorder_cancel();
		for (bool early_exit : { false, true }) {
			Players[Player_num].primary_weapon_flags &= ~(1 << OMEGA_INDEX);
			require(input_demo_replay_load(tail_path.c_str(), error, sizeof(error)), error);
			omega_completion_writes = 0;
			if (early_exit) require(input_demo_finish_replay_shared(0, &timer, nullptr, omega_completion_write), "Tail exit not handled");
			else input_demo_advance_replay_frame_shared(nullptr, omega_completion_stop);
			require(Players[Player_num].primary_weapon_flags & (1 << OMEGA_INDEX), "Authored terminal tail did not acquire Omega");
			require(!omega_completion_writes && !input_demo_replay_is_loaded(), "Terminal-tail Omega published success");
		}
		results["terminal_tail_rejected"] = true;

	} else {
		require(maybe_start_input_demo_recording(0), "Classic Omega recording was rejected");
		input_demo_recorder_cancel();
		results["classic_recording_allowed"] = true;
		for (bool early_exit : { false, true }) {
			require(input_demo_replay_load(replay_path.c_str(), error, sizeof(error)), error);
			omega_completion_writes = 0;
			fix64 timer = 1;
			if (early_exit) require(input_demo_finish_replay_shared(0, &timer, nullptr, omega_completion_write), "Classic exit not handled");
			else input_demo_advance_replay_frame_shared(nullptr, omega_completion_stop);
			require(omega_completion_writes == 1 && !input_demo_replay_is_loaded(), "Classic Omega completion was rejected");
		}
		results["classic_completion_allowed"] = true;
	}
	Players[Player_num].primary_weapon_flags = flags;
	return results;
}
// Assisted component controls: actual firing, blob construction and robot collision
static int xl_omega_damage_probe(const char *path)
{
	using nlohmann::ordered_json;
	ordered_json proof = { { "schema", "xl-omega-damage-v1" }, { "assisted", true }, { "complete", false }, { "cases", ordered_json::array() } };
	try {
		const auto require = [](bool ok, const char *message) { if (!ok) throw std::runtime_error(message); };
		const auto *renderer = glGetString(GL_RENDERER);
		require(renderer && Game_wind && !game_is_time_paused(), "Damage controls need a rendered game");
		proof["renderer"] = reinterpret_cast<const char *>(renderer);
		proof["mission"] = Current_mission_filename;
		proof["input_policy"] = omega_replay_policy_controls(path);
		const int original_mode = Game_mode;
		int robot_id = -1;
		for (int i = 0; i < N_robot_types; ++i)
			if (!Robot_info[i].boss_flag && !Robot_info[i].companion && !Robot_info[i].thief) {
				robot_id = i;
				break;
			}
		require(robot_id >= 0, "No ordinary damage target");
		PlayerCfg.Cockpit3DView[0] = PlayerCfg.Cockpit3DView[1] = 0;
		for (int multiplayer : { 0, 1 })
			for (int hz : { 20, 30, 60, 120 }) {
				Game_mode = multiplayer ? GM_MULTI : original_mode;
				xl_slow_motion_reset();
				const bool xl = xl_level_extensions_active();
				Player_is_dead = 0;
				Difficulty_level = 4;
				Players[Player_num].primary_weapon_flags |= 1 << OMEGA_INDEX;
				Players[Player_num].primary_weapon = OMEGA_INDEX;
				Players[Player_num].energy = 0;
				Omega_charge = MAX_OMEGA_CHARGE;
				GameTime64 = 10 * F1_0;
				Next_laser_fire_time = GameTime64;
				Last_laser_fired_time = Last_omega_fire_time = 0;
				d_srand(0x12345678);
				const int target = obj_create(OBJ_ROBOT, robot_id, ConsoleObject->segnum, &ConsoleObject->pos, &ConsoleObject->orient, F1_0, CT_NONE, MT_NONE, RT_NONE);
				require(target >= 0, "Cannot allocate damage target");
				Objects[target].shields = 10000 * F1_0;
				std::vector<int> existing;
				for (int i = 0; i <= Highest_object_index; ++i) existing.push_back(Objects[i].type == OBJ_NONE ? -1 : Objects[i].signature);
				int shots = 0, remainder = 0;
				int64_t damage = 0, charge_used = 0;
				for (int frame = 0; frame < hz; ++frame) {
					remainder += F1_0;
					FrameTime = remainder / hz;
					remainder %= hz;
					GameTime64 += FrameTime;
					if (allowed_to_fire_laser()) {
						const auto before_charge = Omega_charge;
						const int shot = obj_create(OBJ_WEAPON, OMEGA_ID, ConsoleObject->segnum, &ConsoleObject->pos, &ConsoleObject->orient, F1_0 / 8, CT_WEAPON, MT_NONE, RT_NONE);
						require(shot >= 0, "Cannot allocate Omega control");
						auto position = ConsoleObject->pos;
						do_omega_stuff(ConsoleObject, &position, &Objects[shot]);
						int blob = -1;
						for (int i = 0; i <= Highest_object_index; ++i)
							if (Objects[i].type == OBJ_WEAPON && Objects[i].id == OMEGA_ID && Objects[i].movement_type == MT_PHYSICS && (i >= int(existing.size()) || existing[i] != Objects[i].signature)) blob = i;
						require(blob >= 0 && Omega_charge < before_charge, "Accepted shot produced no real blob");
						const auto expected = fixmul(32 * (xl ? FrameTime : F1_0 / 20), Weapon_info[OMEGA_ID].strength[Difficulty_level]);
						require(Objects[blob].shields == expected, "Omega blob damage does not match its charge clock");
						const auto before_shields = Objects[target].shields;
						collide_robot_and_weapon(&Objects[target], &Objects[blob], &position);
						require(before_shields - Objects[target].shields == expected, "Actual robot damage differs from the blob");
						damage += before_shields - Objects[target].shields;
						charge_used += before_charge - Omega_charge;
						++shots;
						for (int i = Highest_object_index; i >= 0; --i)
							if (Objects[i].type != OBJ_NONE && (i >= int(existing.size()) || existing[i] != Objects[i].signature)) obj_delete(i);
					}
					game_render_frame();
					require(glGetError() == GL_NO_ERROR, "Damage control render failed");
				}
				proof["cases"].push_back({ { "hz", hz }, { "multiplayer", bool(multiplayer) }, { "xl", xl }, { "shots", shots }, { "charge_used", charge_used }, { "target_damage", damage }, { "charge_remaining", Omega_charge }, { "target_remaining", int(Objects[target].shields) }, { "strength", int(Weapon_info[OMEGA_ID].strength[Difficulty_level]) } });
				obj_delete(target);
			}
		Game_mode = original_mode;
		proof["complete"] = true;
	} catch (const std::exception &e) {
		proof["error"] = e.what();
	}
	std::ofstream output(path);
	output << proof.dump(2) << '\n';
	output.flush();
	return output.good() && proof["complete"] == true ? 0 : 1;
}
#endif
