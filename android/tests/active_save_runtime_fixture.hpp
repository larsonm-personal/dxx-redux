// Synthetic checkpoints on public D2 data, using ordinary runtime save readers and writers
#ifdef DXX_BUILD_DESCENT_II
extern "C" void do_countdown_frame(void);
#endif

static void exercise_active_save_runtime(const char *scenario)
{
#ifdef DXX_BUILD_DESCENT_II
	char filename[] = "active-runtime.sav", description[21] = "Active runtime";
	const auto save = [&]() {
		stop_time();
		require(state_save_all_sub(filename, description), "save ordinary active runtime checkpoint");
	};
	const auto restore = [&]() {
		require(state_restore_all_sub(filename, 0), "restore ordinary active runtime checkpoint");
	};
	if (std::strcmp(scenario, "countdown") == 0) {
		// Seed the checkpoint only; subsequent time and physics use the real countdown routine
		Control_center_destroyed = 1;
		Countdown_timer = 10 * F1_0;
		Countdown_seconds_left = 10;
		Total_countdown_time = 30;
		FrameTime = F1_0 / 30;
		d_tick_step = 1;
		save();
		do_countdown_frame();
		const auto expected = ConsoleObject->mtype.phys_info.rotvel;
		unsigned expected_rng = 0;
		require(d_rand_get_state(&expected_rng), "capture uninterrupted countdown SIM state");
		for (int i = 0; i < 32; ++i)
			do_countdown_frame();
		require(Countdown_seconds_left == 9, "advance past saved countdown second");
		restore();
		require(Countdown_timer == 10 * F1_0 && Countdown_seconds_left == 10, "restore countdown timer and derived seconds");
		d_tick_step = 1;
		do_countdown_frame();
		unsigned actual_rng = 0;
		require(d_rand_get_state(&actual_rng), "capture resumed countdown SIM state");
		require(!std::memcmp(&expected, &ConsoleObject->mtype.phys_info.rotvel, sizeof(expected)) && actual_rng == expected_rng,
		        "first resumed countdown physics and SIM state match uninterrupted step");
		// A fresh process starts with a zero cached second; exercise that independent stale value too
		stop_time();
		Countdown_seconds_left = 0;
		restore();
		require(Countdown_seconds_left == 10, "restore is independent of zero cached seconds");
		d_tick_step = 1;
		do_countdown_frame();
		require(!std::memcmp(&expected, &ConsoleObject->mtype.phys_info.rotvel, sizeof(expected)), "zero-cache resumed physics matches");
		std::puts("PASS: countdown warm and zero-cache ordinary-save continuation");
		return;
	}
	require(std::strcmp(scenario, "robot-death") == 0, "known active runtime scenario");
	const fix64 epochs[] = { GameTime64, ((fix64)1 << 40) + 12345 };
	for (const fix64 epoch : epochs) {
		GameTime64 = epoch;
		int slot = -1;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_ROBOT) {
				slot = i;
				break;
			}
		require(slot >= 0 && GameTime64 > 0, "public level has a robot and elapsed game time");
		const int signature = Objects[slot].signature;
		struct death_step {
			int active, exploding;
			vms_vector rotation;
			unsigned sim_state;
		};
		const auto continue_death = [&]() {
			std::vector<death_step> steps;
			for (int frame = 0; frame < 240; ++frame) {
				FrameTime = F1_0 / 30;
				GameTime64 += FrameTime;
				const int active = do_any_robot_dying_frame(&Objects[slot]);
				unsigned sim_state = 0;
				require(d_rand_get_state(&sim_state), "observe dying-frame SIM state");
				steps.push_back({active, (Objects[slot].flags & OF_EXPLODING) != 0, Objects[slot].mtype.phys_info.rotvel, sim_state});
				if (Objects[slot].flags & OF_EXPLODING)
					return steps;
			}
			require(false, "active death sequence reaches real explosion dispatch");
			return steps;
		};
		start_robot_death_sequence(&Objects[slot]);
		save();
		const auto expected_steps = continue_death();
		restore();
		require(Objects[slot].type == OBJ_ROBOT && Objects[slot].signature == signature &&
		            Objects[slot].ctype.ai_info.dying_start_time == GameTime64 - 1,
		        "same-frame death save retains active identity with the existing nonzero sentinel convention");
		const auto actual_steps = continue_death();
		require(actual_steps.size() == expected_steps.size(), "restored death reaches explosion on the same simulation step");
		for (size_t i = 0; i < expected_steps.size(); ++i) {
			const auto &expected = expected_steps[i], &actual = actual_steps[i];
			require(expected.active == 1 && actual.active == expected.active && actual.exploding == expected.exploding &&
			            actual.sim_state == expected.sim_state,
			        "restored death dispatch, explosion progression and SIM state match uninterrupted death");
			// The existing boss sentinel convention advances a same-frame death by one fixed-point time unit
			require(std::abs(actual.rotation.x - expected.rotation.x) <= 1 &&
			            std::abs(actual.rotation.y - expected.rotation.y) <= 1 &&
			            std::abs(actual.rotation.z - expected.rotation.z) <= 1,
			        "restored death rotation matches within one fixed-point unit");
		}
		stop_time();
		restore();
		// Nonzero elapsed deaths must keep their exact relative time
		GameTime64 += F1_0 / 30;
		start_robot_death_sequence(&Objects[slot]);
		GameTime64 += F1_0 / 30;
		save();
		restore();
		require(Objects[slot].ctype.ai_info.dying_start_time == GameTime64 - (F1_0 / 30), "elapsed death offset remains exact");
		require(do_any_robot_dying_frame(&Objects[slot]) == 1, "elapsed death resumes its active dispatch");
		require(Objects[slot].mtype.phys_info.rotvel.x == (F1_0 / 30) / 9 &&
		            Objects[slot].mtype.phys_info.rotvel.y == (F1_0 / 30) / 5 &&
		            Objects[slot].mtype.phys_info.rotvel.z == (F1_0 / 30) / 7,
		        "one-frame elapsed control performs the expected death rotation");
		Objects[slot].ctype.ai_info.dying_start_time = 0;
		save();
		restore();
		require(Objects[slot].ctype.ai_info.dying_start_time == 0, "living robot remains outside death sequence");
		const auto living_rotation = Objects[slot].mtype.phys_info.rotvel;
		require(do_any_robot_dying_frame(&Objects[slot]) == 0 &&
		            !std::memcmp(&living_rotation, &Objects[slot].mtype.phys_info.rotvel, sizeof(living_rotation)),
		        "inactive control does not run the death action");
		std::printf("Robot-death continuation passed at epoch=%lld\n", (long long)epoch);
	}
	std::puts("PASS: same-frame, elapsed and inactive robot-death ordinary saves at normal and 64-bit epochs");
#else
	(void) scenario;
	require(false, "active runtime fixture requires D2");
#endif
}
