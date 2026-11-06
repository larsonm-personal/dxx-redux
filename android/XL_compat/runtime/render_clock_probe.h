#ifndef XL_RENDER_CLOCK_PROBE_H
#define XL_RENDER_CLOCK_PROBE_H
// Developer qualification only; this deliberately controls camera and end-level state
static int xl_render_clock_probe(const char *path)
{
	using nlohmann::ordered_json;
	ordered_json proof = { { "schema", "xl-render-clock-v1" }, { "assisted", true }, { "complete", false }, { "events", ordered_json::array() } };
	const auto write = [&]() {
		std::ofstream out(path);
		out << proof.dump(2) << '\n';
		out.flush();
		return out.good();
	};
	try {
		const auto require = [](bool ok, const char *message) {
			if (!ok) throw std::runtime_error(message);
		};
		const auto *renderer = glGetString(GL_RENDERER);
		require(renderer && Game_wind && !game_is_time_paused() && route_confirmation_get_summary()->status == ROUTE_CONFIRMATION_IDLE, "Render probe requires an initialized idle game");
		proof["renderer"] = reinterpret_cast<const char *>(renderer);
		proof["xl_active"] = bool(xl_level_extensions_active());
		proof["mission"] = Current_mission_filename;
		proof["level"] = Current_level_num;
		const bool xl = xl_level_extensions_active();
		const auto level = Current_level_num;
		PlayerCfg.Cockpit3DView[0] = PlayerCfg.Cockpit3DView[1] = 0;
		Controls = {};
		FrameTime = F1_0 / 60;
		xl_slow_motion_reset();
		GameTime64 = 10 * F1_0;
		Player_is_dead = 0;
		Difficulty_level = 4;
		Players[Player_num].primary_weapon_flags |= 1 << OMEGA_INDEX;
		Players[Player_num].energy = 100 * F1_0;
		Omega_charge = F1_0 / 2;
		const int shot = obj_create(OBJ_WEAPON, OMEGA_ID, ConsoleObject->segnum, &ConsoleObject->pos, &ConsoleObject->orient, F1_0 / 8, CT_WEAPON, MT_NONE, RT_NONE);
		require(shot >= 0, "Cannot allocate actual Omega shot");
		auto firing_position = ConsoleObject->pos;
		do_omega_stuff(ConsoleObject, &firing_position, &Objects[shot]);
		proof["shot_charge"] = Omega_charge;
		proof["shot_delay"] = Next_laser_fire_time - GameTime64;
		require(Omega_charge == F1_0 / 2 - (xl ? FrameTime : F1_0 / 20) && Next_laser_fire_time - GameTime64 == (xl ? 1 : F1_0 / 20), "Omega shot consumption/deadline differs");
		require(glGetError() == GL_NO_ERROR, "Initial OpenGL error");
		const auto sample = [&](const char *event, unsigned before, unsigned expected) {
			glFinish();
			require(glGetError() == GL_NO_ERROR, "OpenGL failed during render-clock control");
			const auto after = xl_slow_motion_render_clock();
			FrameTime = F1_0 / 60;
			Omega_charge = F1_0 / 2;
			Players[Player_num].energy = 100 * F1_0;
			omega_charge_frame();
			proof["events"].push_back({ { "event", event }, { "before", before }, { "after", after }, { "charge", Omega_charge } });
			const bool charge_allowed = xl && after != 0 && after != 1;
			require(Omega_charge == F1_0 / 2 + (charge_allowed ? FrameTime / 4 : 0), "Rendered Omega recharge gate differs");
			require(after - before == (xl ? expected : 0), "Native render-clock contribution differs");
			require(Current_level_num == level && !Player_is_dead, "Render fixture died or changed levels");
		};
		auto before = xl_slow_motion_render_clock();
		for (int i = 0; i < 3; ++i) GameProcessFrame();
		sample("simulation_only", before, 0);
		before = xl_slow_motion_render_clock();
		game_render_frame();
		sample("main", before, 1);
		const auto before_rejection = xl_runtime::weapon_clock_snapshot();
		const auto next_shot = Next_laser_fire_time;
		Omega_charge = 0;
		const int rejected = obj_create(OBJ_WEAPON, OMEGA_ID, ConsoleObject->segnum, &ConsoleObject->pos, &ConsoleObject->orient, F1_0 / 8, CT_WEAPON, MT_NONE, RT_NONE);
		require(rejected >= 0, "Cannot allocate rejected Omega control");
		do_omega_stuff(ConsoleObject, &firing_position, &Objects[rejected]);
		require(Omega_charge == 0 && Next_laser_fire_time == next_shot && xl_runtime::weapon_clock_snapshot() == before_rejection, "Rejected Omega shot changed firing state");
		Omega_charge = F1_0 / 2;
		char save_name[] = "omega-render.sav", description[] = "Omega render clock";
		stop_time();
		require(state_save_all_sub(save_name, description), "Cannot save rendered Omega phase");
		before = xl_slow_motion_render_clock();
		game_render_frame_mono(0);
		sample("auxiliary_mono", before, 0);
		before = xl_slow_motion_render_clock();
		do_automap();
		auto *map = window_get_front();
		require(map && map != Game_wind && Automap_active, "Native automap did not open");
		d_event draw{};
		draw.type = EVENT_WINDOW_DRAW;
		window_send_event(map, &draw);
		sample("automap", before, 1);
		window_close(map);
		require(!Automap_active, "Native automap did not close");
		before = xl_slow_motion_render_clock();
		Endlevel_sequence = 1;
		game_render_frame();
		sample("endlevel_main", before, 2);
		before = xl_slow_motion_render_clock();
		game_render_frame_mono(0);
		sample("endlevel_auxiliary", before, 1);
		Endlevel_sequence = 0;
		before = xl_slow_motion_render_clock();
		for (int i = 0; i < 3; ++i) GameProcessFrame();
		sample("simulation_after_render", before, 0);
		before = xl_slow_motion_render_clock();
		game_render_frame();
		sample("main_again", before, 1);
		require(state_restore_all_sub(save_name, 0), "Cannot restore rendered Omega phase");
		require(xl_slow_motion_render_clock() == (xl ? 1u : 0u), "Saved render counter identity changed");
		before = xl_slow_motion_render_clock();
		sample("restored", before, 0);
		before = xl_slow_motion_render_clock();
		game_render_frame();
		sample("main_after_restore", before, 1);
		xl_slow_motion_reset();
		require(xl_slow_motion_render_clock() == 0, "Level-clear render counter did not reset");
		proof["reset_count"] = xl_slow_motion_render_clock();
		proof["complete"] = true;
		return write() ? 0 : 1;
	} catch (const std::exception &error) {
		proof["error"] = error.what();
		write();
		std::fprintf(stderr, "XL render-clock probe failed: %s\n", error.what());
		return 1;
	}
}
#endif
