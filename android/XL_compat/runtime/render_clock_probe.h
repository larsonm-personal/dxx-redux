#ifndef XL_RENDER_CLOCK_PROBE_H
#define XL_RENDER_CLOCK_PROBE_H
#include "named_audio_state.h"
#include "resource_validation.h"
// Developer qualification only; this deliberately controls camera and end-level state
static int xl_render_clock_probe(const char *path)
{
	using nlohmann::ordered_json;
	ordered_json proof = { { "schema", "xl-render-clock-v1" }, { "assisted", bool{ true } }, { "complete", bool{ false } }, { "events", ordered_json::array() } };
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
			require(write(), "Cannot write render-clock progress");
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
		if (xl) {
			require(HandleGameKey(KEY_ALTED | KEY_S) && HandleGameKey(KEY_ALTED | KEY_B), "Cannot queue ordinary time controls before save");
			const auto pending = xl_runtime::slow_motion_snapshot();
			require(pending.at("slow_requested").get<bool>() && pending.at("bullet_requested").get<bool>(), "Time requests were consumed before simulation");
			auto invalid = pending;
			invalid["slow_requested"] = 1;
			require(!xl_runtime::restore_slow_motion(invalid, true) && xl_runtime::slow_motion_snapshot() == pending,
			        "Malformed pending time controls partially changed runtime state");
		}
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
		if (xl) {
			const auto pending = xl_runtime::slow_motion_snapshot();
			require(pending.at("slow_requested").get<bool>() && pending.at("bullet_requested").get<bool>(), "Ordinary save lost pending time controls");
		}
		require(xl_slow_motion_render_clock() == (xl ? 1u : 0u), "Saved render counter identity changed");
		before = xl_slow_motion_render_clock();
		sample("restored", before, 0);
		before = xl_slow_motion_render_clock();
		game_render_frame();
		sample("main_after_restore", before, 1);
		xl_slow_motion_reset();
		require(xl_slow_motion_render_clock() == 0, "Level-clear render counter did not reset");
		proof["reset_count"] = xl_slow_motion_render_clock();
		proof["complete"] = bool{ true };
		return write() ? 0 : 1;
	} catch (const std::exception &error) {
		proof["error"] = error.what();
		write();
		std::fprintf(stderr, "XL render-clock probe failed: %s\n", error.what());
		return 1;
	}
}
// Reuse the rendered probe's ordinary startup, renderer and private save directory
static int xl_time_equipment_probe(const char *path, bool resume, int carried_level)
{
	using nlohmann::ordered_json;
	ordered_json proof = { { "schema", "xl-time-equipment-gameplay-v1" }, { "assisted", bool{ true } }, { "complete", bool{ false } }, { "cold_resume", resume }, { "position_assists", 0 }, { "level_load_assists", 0 }, { "carried_level", carried_level }, { "events", ordered_json::array() } };
	const auto write = [&]() {
		std::ofstream out(path);
		out << proof.dump(2) << '\n';
		return out.good();
	};
	try {
		const auto require = [](bool ok, const char *message) {
			if (!ok) throw std::runtime_error(message);
		};
		const auto *renderer = glGetString(GL_RENDERER);
		require(renderer && Game_wind && xl_level_extensions_active() && !game_is_time_paused() &&
		            route_confirmation_get_summary()->status == ROUTE_CONFIRMATION_IDLE,
		        "Time equipment requires an initialized idle XL game");
		proof["renderer"] = reinterpret_cast<const char *>(renderer);
		proof["mission"] = Current_mission_filename;
		proof["level"] = Current_level_num;
		char driver[128]{};
		proof["audio_driver"] = SDL_AudioDriverName(driver, sizeof(driver)) ? driver : "";
		proof["audio_backend_ready"] = bool(xl_named_audio_backend_ready());
		proof["sound_disabled"] = bool(GameArg.SndNoSound);
		const int level = Current_level_num;
		int running_level = level;
		const auto initial = *ConsoleObject;
		Controls = {};
		const auto pose = [&](const object &actor) {
			ConsoleObject->pos = ConsoleObject->last_pos = actor.pos;
			ConsoleObject->orient = actor.orient;
			ConsoleObject->mtype.phys_info.velocity = {};
			obj_relink(int(ConsoleObject - Objects), actor.segnum);
			proof["position_assists"] = proof["position_assists"].get<int>() + 1;
		};
		const auto frames = [&](int count, bool thrust = false) {
			for (int i = 0; i < count; ++i) {
				SDL_Delay(26);
				timer_update();
				FrameTime = F1_0 / 60;
				Controls = {};
				Controls.forward_thrust_time = thrust ? FrameTime : 0;
				do_weapon_n_item_stuff();
				calc_game_time();
				GameProcessFrame();
				game_render_frame();
				gr_flip();
				require(glGetError() == GL_NO_ERROR && Current_level_num == running_level && !Player_is_dead,
				        "Time equipment gameplay failed or changed level");
			}
		};
		const auto sample = [&](const char *event) {
			const auto audio = xl_compat::named_audio_diagnostics();
			proof["events"].push_back({ { "event", event }, { "level", Current_level_num }, { "clocks", xl_runtime::slow_motion_diagnostics() }, { "energy", int(Players[Player_num].energy) }, { "shields", int(Players[Player_num].shields) }, { "elapsed_ms", SDL_GetTicks() }, { "audio", { { "slowdown", audio.at("slowdown") }, { "output_callbacks", audio.at("output_callbacks") }, { "output_frames", audio.at("output_frames") }, { "paused", audio.at("paused") }, { "resume_pending", audio.at("resume_pending") }, { "output_volume", audio.at("output_volume") }, { "effects_volume", audio.at("effects_volume") }, { "starts", audio.at("starts") }, { "completions", audio.at("completions") }, { "external_resources", audio.at("external_resources") } } }, { "position", { int(ConsoleObject->pos.x), int(ConsoleObject->pos.y), int(ConsoleObject->pos.z) } } });
			require(write(), "Cannot write time-equipment progress");
			// Developer screenshots read the back buffer before it is swapped/cleared
			game_render_frame();
			save_screen_shot(0);
			gr_flip();
		};
		char file[] = "time-equipment.sav", description[] = "Original time equipment";
		if (resume) {
			require(state_restore_all_sub(file, 0), "Cannot cold-restore earned time equipment");
			if (game_is_time_paused()) start_time();
			require((Players[Player_num].flags & (XL_PLAYER_FLAGS_SLOW_MOTION | XL_PLAYER_FLAGS_BULLET_TIME)) ==
			                (XL_PLAYER_FLAGS_SLOW_MOTION | XL_PLAYER_FLAGS_BULLET_TIME) &&
			            xl_runtime::slow_motion_state().clocks[0].speed == 3 && xl_runtime::slow_motion_state().clocks[1].speed == 1,
			        "Cold restore lost equipment or active bullet-time clocks");
			sample("cold_restored");
			frames(12);
		} else {
			for (const int id : { 48, 49 }) {
				int pickup = -1;
				for (int i = 0; i <= Highest_object_index; ++i)
					if (Objects[i].type == OBJ_POWERUP && Objects[i].id == id) {
						pickup = i;
						break;
					}
				require(pickup >= 0, "Original mission has no time-equipment pickup");
				const auto actor = Objects[pickup];
				auto approach = actor;
				// Native pickup assistance reaches twice the combined collision radii
				const fix distance = 2 * (actor.size + ConsoleObject->size) + F1_0;
				vms_vector center, inward;
				compute_segment_center(&center, &Segments[actor.segnum]);
				vm_vec_sub(&inward, &center, &actor.pos);
				vm_vec_normalize_quick(&inward);
				bool found = false;
				for (auto direction : { inward, actor.orient.fvec, actor.orient.rvec, actor.orient.uvec,
				                        vms_vector{ -actor.orient.fvec.x, -actor.orient.fvec.y, -actor.orient.fvec.z },
				                        vms_vector{ -actor.orient.rvec.x, -actor.orient.rvec.y, -actor.orient.rvec.z },
				                        vms_vector{ -actor.orient.uvec.x, -actor.orient.uvec.y, -actor.orient.uvec.z } }) {
					if (!vm_vec_mag_quick(&direction)) continue;
					vm_vec_scale_add(&approach.pos, &actor.pos, &direction, distance);
					auto origin = actor.pos;
					fvi_query query{};
					query.p0 = &origin;
					query.p1 = &approach.pos;
					query.startseg = actor.segnum;
					query.rad = ConsoleObject->size;
					query.thisobjnum = short(ConsoleObject - Objects);
					fvi_info hit{};
					if (find_vector_intersection(&query, &hit) != HIT_NONE || hit.hit_seg < 0) continue;
					approach.segnum = short(hit.hit_seg);
					inward = direction;
					found = true;
					break;
				}
				require(found, "Original time-equipment approach is obstructed");
				vm_vec_negate(&inward);
				vm_vector_2_matrix(&approach.orient, &inward, nullptr, nullptr);
				pose(approach);
				if (id == 48) {
					require(input_demo_start_quick_recording_common() && input_demo_recorder_uses_environment(), "Cannot start ordinary XL pickup recording");
					char error[256]{};
					const std::string observation = std::string(path) + ".capture.jsonl.gz";
					// The physical pickup determines the horizon; zero means not known yet
					require(input_demo_state_trace_start(observation.c_str(), "record", "d2", Current_mission_filename,
					                                     Current_level_num, Difficulty_level, "save_checkpoint", 0, error, sizeof(error)),
					        error);
					input_demo_trace_boundary("restored");
				}
				frames(2);
				require(!(Players[Player_num].flags & (id == 48 ? XL_PLAYER_FLAGS_SLOW_MOTION : XL_PLAYER_FLAGS_BULLET_TIME)),
				        "Time-equipment approach collected before its visible observation");
				sample(id == 48 ? "visible_slow_motion_pickup" : "visible_bullet_time_pickup");
				proof["events"].back()["pickup_id"] = id;
				proof["events"].back()["pickup_frame"] = Objects[pickup].rtype.vclip_info.framenum;
				for (unsigned frame = 0; frame < 90 && !(Players[Player_num].flags & (id == 48 ? XL_PLAYER_FLAGS_SLOW_MOTION : XL_PLAYER_FLAGS_BULLET_TIME)); ++frame)
					frames(1, true);
				require(Players[Player_num].flags & (id == 48 ? XL_PLAYER_FLAGS_SLOW_MOTION : XL_PLAYER_FLAGS_BULLET_TIME),
				        "Normal collision did not collect the original time equipment");
				if (id == 48) {
					require(HandleGameKey(KEY_ALTED | KEY_S), "Earned slow-motion control was not consumed");
					frames(84);
					require(HandleGameKey(KEY_ALTED | KEY_S), "Earned slow-motion reversal was not consumed");
					frames(84);
					require(input_demo_state_trace_is_active(), "Live XL observation stopped early");
					input_demo_trace_boundary("terminal");
					input_demo_state_trace_stop();
					require(input_demo_recorder_is_active() && input_demo_stop_quick_recording_common(), "Ordinary XL pickup recording stopped early");
				}
			}
			// Original equipment rooms can replenish the energy being measured
			pose(initial);
			frames(2);
			sample("earned_original_pickups");
		}
		const fix energy = Players[Player_num].energy;
		require(HandleGameKey(KEY_ALTED | KEY_S), "Normal slow-motion key was not consumed");
		frames(40);
		const auto paused_clocks = xl_runtime::slow_motion_diagnostics();
		const auto paused_energy = Players[Player_num].energy;
		d_event deactivated{ EVENT_WINDOW_DEACTIVATED }, activated{ EVENT_WINDOW_ACTIVATED };
		window_send_event(Game_wind, &deactivated);
		require(game_is_time_paused(), "Normal window deactivation did not pause time equipment");
		SDL_Delay(120);
		require(xl_runtime::slow_motion_diagnostics() == paused_clocks && Players[Player_num].energy == paused_energy,
		        "Paused time equipment changed clocks or energy");
		window_send_event(Game_wind, &activated);
		require(!game_is_time_paused(), "Normal window activation did not resume time equipment");
		proof["pause_resume_preserved"] = bool{ true };
		frames(44);
		sample("ordinary_slow_motion");
		require(xl_runtime::slow_motion_state().clocks[0].speed == 3 && xl_runtime::slow_motion_state().clocks[1].speed == 3 &&
		            Players[Player_num].energy < energy,
		        "Ordinary slow motion did not slow both clocks and spend energy");
		// Same ordinary thrust interval from the same original pose, with live AI
		const auto movement = [&](const char *event) {
			pose(initial);
			frames(24, true);
			const auto distance = vm_vec_dist(&initial.pos, &ConsoleObject->pos);
			sample(event);
			require(distance > 0, "Ordinary player thrust did not move the ship");
			return distance;
		};
		const auto slow_distance = movement("slow_motion_thrust");
		require(HandleGameKey(KEY_ALTED | KEY_B), "Normal bullet-time key was not consumed");
		frames(84);
		sample("bullet_time");
		require(xl_runtime::slow_motion_state().clocks[0].speed == 3 && xl_runtime::slow_motion_state().clocks[1].speed == 1,
		        "Bullet time did not separate the player and world clocks");
		const auto bullet_distance = movement("bullet_time_thrust");
		require(bullet_distance > slow_distance, "Bullet time did not preserve faster ordinary player movement");
		proof["same_input_movement"] = { { "frames", 24 }, { "slow_distance_raw", slow_distance }, { "bullet_distance_raw", bullet_distance } };
		if (resume) {
			require(HandleGameKey(KEY_ALTED | KEY_B), "Normal time-off key was not consumed");
			frames(84);
			sample("normal_time_after_resume");
			require(xl_runtime::slow_motion_state().clocks[0].speed == 1 && xl_runtime::slow_motion_state().clocks[1].speed == 1,
			        "Cold continuation did not return to normal time");
		}
		stop_time();
		require(state_save_all_sub(file, description), "Cannot save active original time equipment");
		if (resume && carried_level) {
			// Explicit level-load assists qualify carried resources, not campaign travel
			running_level = carried_level;
			proof["phase"] = "carried_level_load";
			require(write(), "Cannot write carried-level progress");
			input_demo_set_skip_level_intro(1);
			StartNewLevel(running_level);
			const auto equipment = XL_PLAYER_FLAGS_SLOW_MOTION | XL_PLAYER_FLAGS_BULLET_TIME;
			require(xl_level_extensions_active() && (Players[Player_num].flags & equipment) == equipment &&
			            xl_runtime::slow_motion_state().clocks[0].speed == 1 && xl_runtime::slow_motion_state().clocks[1].speed == 1 &&
			            xl_prepared_time_equipment_image(0) && xl_prepared_time_equipment_image(1),
			        "Level transition lost carried devices/resources or retained active clocks");
			if (game_is_time_paused()) start_time();
			require(HandleGameKey(KEY_ALTED | KEY_B), "Carried bullet-time key was not consumed");
			frames(84);
			sample("carried_to_selected_level");
			char carried[] = "time-equipment-carried.sav";
			stop_time();
			require(state_save_all_sub(carried, description), "Cannot save carried time equipment");
			proof["phase"] = "carried_new_game";
			require(write(), "Cannot write carried new-game progress");
			input_demo_set_skip_level_intro(1);
			// Same fresh negative-level entry as the existing rendered readiness probe
			if (running_level < 0) Newdemo_state = ND_STATE_PLAYBACK;
			StartNewGame(running_level);
			if (running_level < 0) Newdemo_state = ND_STATE_NORMAL;
			require(!(Players[Player_num].flags & equipment) && !xl_prepared_time_equipment_image(0) && !xl_prepared_time_equipment_image(1),
			        "New game did not clear devices or carried-only resources");
			sample("new_game_cleared_equipment");
			char unequipped[] = "time-equipment-new-game.sav";
			stop_time();
			require(state_save_all_sub(unequipped, description), "Cannot save the unequipped new game");
			proof["phase"] = "carried_restore";
			require(write(), "Cannot write carried restore progress");
			require(state_restore_all_sub(carried, 0) && (Players[Player_num].flags & equipment) == equipment &&
			            xl_prepared_time_equipment_image(0) && xl_prepared_time_equipment_image(1) &&
			            xl_runtime::slow_motion_state().clocks[0].speed == 3 && xl_runtime::slow_motion_state().clocks[1].speed == 1,
			        "Carried save did not prepare resources before restoring devices and clocks");
			if (game_is_time_paused()) start_time();
			frames(12);
			sample("carried_save_restored_after_new_game");
			proof["phase"] = "unequipped_restore";
			require(write(), "Cannot write unequipped restore progress");
			require(state_restore_all_sub(unequipped, 0) && !(Players[Player_num].flags & equipment) &&
			            !xl_prepared_time_equipment_image(0) && !xl_prepared_time_equipment_image(1),
			        "Unequipped save retained the running game's carried resources");
			sample("unequipped_save_restored_after_carried");
			proof["level_load_assists"] = 4;
		}
		proof["phase"] = "complete";
		proof["complete"] = bool{ true };
		return write() ? 0 : 1;
	} catch (const std::exception &error) {
		proof["error"] = error.what();
		write();
		std::fprintf(stderr, "XL time-equipment gameplay failed: %s\n", error.what());
		return 1;
	}
}
#endif
