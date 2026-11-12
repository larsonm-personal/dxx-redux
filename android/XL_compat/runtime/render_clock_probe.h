#ifndef XL_RENDER_CLOCK_PROBE_H
#define XL_RENDER_CLOCK_PROBE_H
#include "named_audio_state.h"
#include "resource_validation.h"
#include "camera_save.h"
extern "C" {
#include "newmenu.h"
}
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
		const auto paused_time = GameTime64;
		const auto paused_sim = d_rand_get_stream_call_count(D_RNG_SIM);
		newmenu_item close{};
		close.type = NM_TYPE_MENU;
		char close_text[] = "Close";
		close.text = close_text;
		auto *menu = newmenu_do3(nullptr, nullptr, 1, &close, nullptr, nullptr, 0, nullptr);
		require(menu && window_get_front() != Game_wind && game_is_time_paused(), "Cannot open paused menu control");
		auto *menu_window = newmenu_get_window(menu);
		FrameTime = F1_0 / 60;
		for (int i = 0; i < 3; ++i) event_process();
		proof["paused_frame_time"] = FrameTime;
		proof["paused_game_time_unchanged"] = bool(GameTime64 == paused_time);
		proof["paused_sim_rng_unchanged"] = bool(d_rand_get_stream_call_count(D_RNG_SIM) == paused_sim);
		window_close(menu_window);
		require(write(), "Cannot write paused menu evidence");
		require((!xl || FrameTime == 0) && GameTime64 == paused_time && d_rand_get_stream_call_count(D_RNG_SIM) == paused_sim,
		        "Paused menu changed simulation time, frame duration or SIM RNG");
		sample("paused_menu", before, 3);
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
	ordered_json proof = { { "schema", "xl-time-equipment-gameplay-v1" }, { "assisted", bool{ true } }, { "complete", bool{ false } }, { "cold_resume", resume }, { "position_assists", 0 }, { "inventory_assists", 0 }, { "assists_during_recording", 0 }, { "level_load_assists", 0 }, { "carried_level", carried_level }, { "events", ordered_json::array() } };
	struct frame_timing {
		uint64_t count{}, delay{}, simulation{}, render{}, flip{};
	} timings[2];
	proof["timing_ms"]["probe_entered"] = SDL_GetTicks();
	const auto write = [&]() {
		for (int i = 0; i < 2; ++i) {
			const auto &t = timings[i];
			proof["frame_timing_ms"][i ? "recording" : "ordinary"] = {
				{ "frames", t.count }, { "delay", t.delay }, { "simulation", t.simulation }, { "render", t.render }, { "flip", t.flip }
			};
		}
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
		std::vector<int> original_robots(Highest_object_index + 1, -1);
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_ROBOT) original_robots[i] = Objects[i].signature;
		int combat_target = -1, combat_signature = -1;
		Controls = {};
		const auto pose = [&](const object &actor) {
			require(!input_demo_recorder_is_active(), "Time-equipment probe cannot relocate during recording");
			ConsoleObject->pos = ConsoleObject->last_pos = actor.pos;
			ConsoleObject->orient = actor.orient;
			ConsoleObject->mtype.phys_info.velocity = {};
			ConsoleObject->mtype.phys_info.rotvel = {};
			obj_relink(int(ConsoleObject - Objects), actor.segnum);
			proof["position_assists"] = proof["position_assists"].get<int>() + 1;
		};
		const auto frames = [&](int count, bool thrust = false, bool firing = false, bool aiming = false) {
			for (int i = 0; i < count; ++i) {
				const bool original_target = combat_target >= 0 && Objects[combat_target].type == OBJ_ROBOT && Objects[combat_target].signature == combat_signature;
				const fix target_shields = original_target ? Objects[combat_target].shields : 0;
				auto &timing = timings[input_demo_recorder_is_active() ? 1 : 0];
				// Observe wall time without refreshing the simulation's cached clock
				const auto before_delay = SDL_GetTicks();
				SDL_Delay(26);
				const auto before_simulation = SDL_GetTicks();
				timer_update();
				FrameTime = F1_0 / 60;
				Controls = {};
				Controls.forward_thrust_time = thrust ? FrameTime : 0;
				Controls.fire_primary_state = firing;
				if (aiming && original_target) {
					vms_vector aim;
					vm_vec_sub(&aim, &Objects[combat_target].pos, &ConsoleObject->pos);
					vm_vec_normalize(&aim);
					const auto &orientation = ConsoleObject->orient;
					const double x = double(vm_vec_dot(&aim, &orientation.rvec)) / F1_0;
					const double y = double(vm_vec_dot(&aim, &orientation.uvec)) / F1_0;
					const double z = double(vm_vec_dot(&aim, &orientation.fvec)) / F1_0;
					const auto input = [&](double value) { return fix(std::clamp(value, -1.0, 1.0) * FrameTime); };
					Controls.heading_time = input(std::atan2(x, z) * 8 - double(ConsoleObject->mtype.phys_info.rotvel.y) / F1_0 * 1.5);
					Controls.pitch_time = input(-std::atan2(y, std::hypot(x, z)) * 8 - double(ConsoleObject->mtype.phys_info.rotvel.x) / F1_0 * 1.5);
				}
				do_weapon_n_item_stuff();
				calc_game_time();
				GameProcessFrame();
				if (firing && original_target && input_demo_recorder_is_active() &&
				    !proof["combat"].contains("first_damage_frame") &&
				    (Objects[combat_target].signature != combat_signature || Objects[combat_target].shields < target_shields)) {
					const auto &clocks = xl_runtime::slow_motion_state().clocks;
					proof["combat"]["first_damage_frame"] = input_demo_recorder_frame_count() - 1;
					proof["combat"]["world_speed_at_damage"] = clocks[0].speed;
					proof["combat"]["player_speed_at_damage"] = clocks[1].speed;
				}
				const auto before_render = SDL_GetTicks();
				game_render_frame();
				const auto before_flip = SDL_GetTicks();
				gr_flip();
				const auto after_flip = SDL_GetTicks();
				++timing.count;
				timing.delay += Uint32(before_simulation - before_delay);
				timing.simulation += Uint32(before_render - before_simulation);
				timing.render += Uint32(before_flip - before_render);
				timing.flip += Uint32(after_flip - before_flip);
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
			// Expire source robot spawn protection through ordinary simulation
			frames(66);
			proof["warmup_frames"] = 66;
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
				proof["approaches"] = ordered_json::array();
				std::vector<vms_vector> directions{ inward, actor.orient.fvec, actor.orient.rvec, actor.orient.uvec,
					                                vms_vector{ -actor.orient.fvec.x, -actor.orient.fvec.y, -actor.orient.fvec.z },
					                                vms_vector{ -actor.orient.rvec.x, -actor.orient.rvec.y, -actor.orient.rvec.z },
					                                vms_vector{ -actor.orient.uvec.x, -actor.orient.uvec.y, -actor.orient.uvec.z } };
				// Search nearby clear approaches without changing the authored objects
				for (unsigned axis = 1; axis < 7; ++axis)
					for (unsigned lateral = 1; lateral < 7; ++lateral) {
						if (std::abs(vm_vec_dot(&directions[axis], &directions[lateral])) > F1_0 / 2) continue;
						vms_vector direction;
						vm_vec_scale_add(&direction, &directions[axis], &directions[lateral], F1_0 / 2);
						vm_vec_normalize(&direction);
						directions.push_back(direction);
					}
				for (unsigned candidate = 0; candidate < directions.size() * 3; ++candidate) {
					auto direction = directions[candidate % directions.size()];
					if (!vm_vec_mag_quick(&direction)) continue;
					vm_vec_scale_add(&approach.pos, &actor.pos, &direction, distance + (candidate / directions.size()) * distance / 2);
					proof["approaches"].push_back({ { "pickup", pickup }, { "position", { int(approach.pos.x), int(approach.pos.y), int(approach.pos.z) } } });
					auto &attempt = proof["approaches"].back();
					auto origin = actor.pos;
					fvi_query query{};
					query.p0 = &origin;
					query.p1 = &approach.pos;
					query.startseg = actor.segnum;
					query.rad = ConsoleObject->size;
					query.thisobjnum = short(ConsoleObject - Objects);
					fvi_info hit{};
					const int geometry = find_vector_intersection(&query, &hit);
					attempt["geometry_hit"] = geometry;
					attempt["segment"] = int(hit.hit_seg);
					if (geometry != HIT_NONE || hit.hit_seg < 0) continue;
					approach.segnum = short(hit.hit_seg);
					bool occupied = false;
					for (int robot = 0; robot <= Highest_object_index; ++robot)
						if (Objects[robot].type == OBJ_ROBOT && vm_vec_dist(&approach.pos, &Objects[robot].pos) < ConsoleObject->size + Objects[robot].size + F1_0) {
							occupied = true;
							attempt["overlapping_robots"].push_back(robot);
						}
					if (occupied) continue;
					if (id == 48) {
						vms_vector toward_pickup;
						vm_vec_sub(&toward_pickup, &actor.pos, &approach.pos);
						vm_vec_normalize_quick(&toward_pickup);
						fix nearest = 120 * F1_0;
						combat_target = -1;
						for (unsigned robot = 0; robot < original_robots.size(); ++robot) {
							const auto &target = Objects[robot];
							if (target.type == OBJ_ROBOT && vm_vec_dist(&approach.pos, &target.pos) < 120 * F1_0)
								attempt["nearby_robots"].push_back({ { "object", robot }, { "id", int(target.id) }, { "cloaked", int(target.ctype.ai_info.CLOAKED) }, { "cloak_type", int(Robot_info[target.id].cloak_type) }, { "boss", int(Robot_info[target.id].boss_flag) }, { "companion", int(Robot_info[target.id].companion) }, { "thief", int(Robot_info[target.id].thief) } });
							if (target.type != OBJ_ROBOT || target.signature != original_robots[robot] || target.shields <= 0 ||
							    target.ctype.ai_info.CLOAKED || Robot_info[target.id].cloak_type || Robot_info[target.id].boss_flag ||
							    Robot_info[target.id].companion || Robot_info[target.id].thief) continue;
							vms_vector toward_target;
							vm_vec_sub(&toward_target, &target.pos, &approach.pos);
							const fix distance_to_target = vm_vec_normalize_quick(&toward_target);
							const fix dot = vm_vec_dot(&toward_pickup, &toward_target);
							if (distance_to_target <= 120 * F1_0)
								attempt["target_lines"].push_back({ { "object", robot }, { "distance", distance_to_target }, { "alignment", dot } });
							if (distance_to_target >= nearest) continue;
							auto target_position = target.pos;
							query.p0 = &approach.pos;
							query.p1 = &target_position;
							query.startseg = approach.segnum;
							query.rad = 0;
							const int target_hit = find_vector_intersection(&query, &hit);
							attempt["target_lines"].back()["geometry_hit"] = target_hit;
							if (target_hit != HIT_NONE) continue;
							combat_target = int(robot);
							nearest = distance_to_target;
						}
						if (combat_target < 0) continue;
					}
					inward = direction;
					found = true;
					break;
				}
				require(found, "Original time-equipment approach has no clear pickup/combat line");
				if (id == 48) {
					combat_signature = Objects[combat_target].signature;
					proof["combat"] = { { "object", combat_target }, { "signature", combat_signature }, { "robot_id", int(Objects[combat_target].id) }, { "player", int(ConsoleObject - Objects) }, { "player_signature", int(ConsoleObject->signature) }, { "primary_weapon", int(Players[Player_num].primary_weapon) } };
				}
				vm_vec_negate(&inward);
				vm_vector_2_matrix(&approach.orient, &inward, nullptr, nullptr);
				pose(approach);
				if (id == 48) {
					proof["timing_ms"]["recording_start_entered"] = SDL_GetTicks();
					require(input_demo_start_quick_recording_common() && input_demo_recorder_uses_environment(), "Cannot start ordinary XL pickup recording");
					proof["timing_ms"]["recording_started"] = SDL_GetTicks();
					input_demo_environment_clock saved_before{}, saved_after{}, consumed_before{}, consumed_after{};
					require(xl_input_demo_saved_environment(&saved_before) && input_demo_recorder_peek_environment(&consumed_before), "Missing captured XL clock origin");
					const auto observed_time = xl_runtime::slow_motion_trace_snapshot();
					const auto observed_cameras = xl_camera_trace_snapshot();
					require(observed_time.at("bound").get<bool>() && observed_time == xl_runtime::slow_motion_trace_snapshot() &&
					            observed_cameras == xl_camera_trace_snapshot() && xl_input_demo_saved_environment(&saved_after) &&
					            input_demo_recorder_peek_environment(&consumed_after) &&
					            !std::memcmp(&saved_before, &saved_after, sizeof(saved_before)) &&
					            !std::memcmp(&consumed_before, &consumed_after, sizeof(consumed_before)),
					        "Detailed XL observation changed its state or clock basis");
					char error[256]{};
					const std::string observation = std::string(path) + ".capture.jsonl.gz";
					// The physical pickup determines the horizon; zero means not known yet
					require(input_demo_state_trace_start(observation.c_str(), "record", "d2", Current_mission_filename,
					                                     Current_level_num, Difficulty_level, "save_checkpoint", 0, error, sizeof(error)),
					        error);
					input_demo_trace_boundary("restored");
					proof["timing_ms"]["initial_boundary_written"] = SDL_GetTicks();
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
					require(Objects[combat_target].type == OBJ_ROBOT && Objects[combat_target].signature == combat_signature && Objects[combat_target].shields > 0 &&
					            Players[Player_num].primary_weapon == LASER_INDEX,
					        "Original time-equipment combat target or starting laser changed");
					unsigned aiming_frames = 0;
					for (; aiming_frames < 90; ++aiming_frames) {
						vms_vector aim;
						vm_vec_sub(&aim, &Objects[combat_target].pos, &ConsoleObject->pos);
						vm_vec_normalize(&aim);
						if (vm_vec_dot(&aim, &ConsoleObject->orient.fvec) > 63 * F1_0 / 64) break;
						frames(1, false, false, true);
					}
					proof["combat"]["aiming_frames"] = aiming_frames;
					require(aiming_frames < 90, "Ordinary controls did not aim at the original robot");
					proof["combat"]["shields_before"] = int(Objects[combat_target].shields);
					require(HandleGameKey(KEY_ALTED | KEY_S), "Earned slow-motion control was not consumed");
					frames(84, false, true, true);
					require(proof["combat"].contains("first_damage_frame") && proof["combat"]["world_speed_at_damage"].get<float>() > 1 &&
					            proof["combat"]["player_speed_at_damage"].get<float>() > 1,
					        "Ordinary laser did not damage the original robot during slow motion");
					proof["combat"]["shields_after"] = Objects[combat_target].signature == combat_signature ? int(Objects[combat_target].shields) : 0;
					proof["combat"]["firing_frames"] = 84;
					require(HandleGameKey(KEY_ALTED | KEY_S), "Earned slow-motion reversal was not consumed");
					frames(84);
					require(input_demo_state_trace_is_active(), "Live XL observation stopped early");
					proof["timing_ms"]["recording_frames_finished"] = SDL_GetTicks();
					proof["recorded_frames"] = input_demo_recorder_frame_count();
					require(input_demo_recorder_is_active() && input_demo_stop_quick_recording_common(), "Ordinary XL pickup recording stopped early");
					proof["timing_ms"]["recording_finalized"] = SDL_GetTicks();
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
		proof["timing_ms"]["save_entered"] = SDL_GetTicks();
		require(state_save_all_sub(file, description), "Cannot save active original time equipment");
		proof["timing_ms"]["save_finished"] = SDL_GetTicks();
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
