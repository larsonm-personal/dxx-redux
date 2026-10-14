// Fixed SIM and inputs must survive unrelated effects seeds and counters
static nlohmann::json fx_isolation_gameplay_snapshot()
{
	using nlohmann::json;
	object_runtime_state allocator = {};
	object_get_runtime_state(&allocator);
	unsigned sim_state = 0;
	require(d_rand_get_state(&sim_state), "observe independent SIM state");
	json result = { { "sim", { sim_state, d_rand_get_call_count() } },
		            { "allocator", { allocator.num_objects, allocator.highest_object_index, allocator.signature_seed } },
		            { "free", json::array() },
		            { "objects", json::object() } };
	for (int i = allocator.num_objects; i < MAX_OBJECTS; ++i) result["free"].push_back(allocator.free_obj_list[i]);
	for (int slot = 0; slot <= Highest_object_index; ++slot) {
		const auto &object = Objects[slot];
		if (object.type == OBJ_NONE) continue;
		auto state = input_demo_object_trace_snapshot(object, slot);
		// Skip only the specifically identified visual death camera
		for (const auto *key : { "next", "prev" }) {
			int neighbor = state[key];
			while (neighbor >= 0 && (Player_is_dead && &Objects[neighbor] == Dead_player_camera))
				neighbor = std::strcmp(key, "next") == 0 ? Objects[neighbor].next : Objects[neighbor].prev;
			state[key] = neighbor;
		}
		if (Player_is_dead && &object == Dead_player_camera) {
			// Death-camera geometry may differ; fireball geometry and admission must match
			for (const auto *key : { "pos", "last_pos", "orient", "size", "segnum", "next", "prev" }) state.erase(key);
		}
		if (state.contains("ai_local")) state["ai_local"].erase("next_misc_sound_time"); // Audio-only deadline
		result["objects"][std::to_string(slot)] = state;
	}
	return result;
}

// Original helper order and failure paths: D1 MAIN/OBJECT.C 765-795 at f6fb1c30a06725b403dfa7db4b251ecea1c3efc1
// D2 SOURCE/MAIN/OBJECT.C 615-645 at 75ba13c44259f1e40c0df70961c78abba898eafe (videogamepreservation source imports)
static void test_fireball_original_geometry(bool check_rng = true)
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
#endif
	GameArg.SndNoSound = 1;
	for (const int profile : { 1, 2 }) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
		const bool native = profile == 1;
#else
		(void) profile;
		const bool native = true;
#endif
		for (const int condition : { 0, 1, 2 })
			for (const unsigned fx_seed : { 1u, 3896797992u }) {
				init_test_corridor();
				ConsoleObject->size = condition == 1 ? 512 * F1_0 : F1_0;
				if (condition == 2)
					for (int slot = 1; slot < MAX_OBJECTS; ++slot)
						require(obj_create(OBJ_LIGHT, 0, 0, &ConsoleObject->pos, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE) >= 0, "fill actual object pool");
				const fix scale = 3 * F1_0;
				d_srand(1234);
				vms_vector offset;
				offset.x = (d_rand() - 16384) | 1;
				offset.y = d_rand() - 16384;
				offset.z = d_rand() - 16384;
				vm_vec_normalize_quick(&offset);
				vm_vec_scale(&offset, ConsoleObject->size / 2);
				vms_vector point = ConsoleObject->pos;
				vm_vec_add2(&point, &offset);
				const int random_size = d_rand();
				const fix size = fixmul(scale, native ? F1_0 + random_size * 4 : F1_0 / 2 + random_size * 4 / 2);
				const int segment = find_point_seg(&point, ConsoleObject->segnum);
				unsigned expected_sim = 0, actual_sim = 0;
				require(d_rand_get_state(&expected_sim), "observe original four-draw reference state");
				require(condition == 1 ? segment == -1 : segment >= 0, "reference includes valid and invalid random offsets");
				d_srand(1234);
				d_rand_reset_call_count();
				d_srand_fx(check_rng ? fx_seed : 1234);
				d_rand_set_stream_call_count(D_RNG_FX, 3000);
				object_runtime_state before = {}, after = {};
				object_get_runtime_state(&before);
				create_small_fireball_on_object(ConsoleObject, scale, 0);
				object_get_runtime_state(&after);
				if (check_rng) {
					require(d_rand_get_state(&actual_sim) && actual_sim == expected_sim && d_rand_get_call_count() == 4, "original helper geometry consumes x, y, z, size in order");
					require(d_rand_get_stream_call_count(D_RNG_FX) == (condition == 0 ? 3001u : 3000u), "crackle FX draw occurs only after successful allocation");
				}
				if (condition == 0) {
					const int slot = ConsoleObject->attached_obj;
					require(slot > 0 && after.num_objects == before.num_objects + 1, "valid offset creates one attached fireball");
					const auto &fireball = Objects[slot];
					require(fireball.segnum == segment && fireball.size == size && !std::memcmp(&fireball.pos, &point, sizeof(point)) && !std::memcmp(&fireball.last_pos, &point, sizeof(point)), "original position, size and segment match the reference");
					require(fireball.lifeleft == Vclip[VCLIP_SMALL_EXPLOSION].play_time, "original fixed vclip lifetime remains intact");
				} else
					require(ConsoleObject->attached_obj == -1 && before.num_objects == after.num_objects && before.signature_seed == after.signature_seed, "invalid offset and full pool retain original zero-allocation behavior");
			}
	}
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_fx_gameplay_isolation(int only_scenario = -1)
{
	using nlohmann::json;
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
#endif
	const auto original_clip = Vclip[VCLIP_SMALL_EXPLOSION];
	const auto original_weapon = Weapon_info[0];
	GameArg.SndNoSound = 1;
#ifdef DXX_BUILD_DESCENT_II
	const int sample_rate = GameArg.SndDigiSampleRate;
	GameArg.SndDigiSampleRate = 0;
#endif
	for (const int profile : { 1, 2 }) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#else
		(void) profile;
#endif
		for (int scenario = 0; scenario < 6; ++scenario) {
			if (only_scenario >= 0 && scenario != only_scenario) continue;
			json expected = json::array();
			unsigned reference_fx_state = 0;
			for (const unsigned fx_seed : { 1u, 5678u, 3896797992u, 4262131764u }) {
				Game_mode = 0;
				Player_num = 0;
				Player_is_dead = 0;
				Dead_player_camera = nullptr;
				Player_exploded = Player_eggs_dropped = Death_sequence_aborted = 0;
				Endlevel_sequence = 0;
				Newdemo_state = ND_STATE_NORMAL;
				init_test_corridor();
				GameTime64 = 10 * F1_0;
				Control_center_destroyed = 0;
				Countdown_seconds_left = 10;
				int parent = 0;
				if (scenario == 2) {
					vms_vector point = {};
					parent = obj_create(OBJ_CNTRLCEN, 0, 0, &point, &vmd_identity_matrix, 8 * F1_0, CT_NONE, MT_NONE, RT_NONE);
					Dead_controlcen_object_num = parent;
				} else if (scenario == 3 || scenario == 4) {
					auto &boss = create_boss_frame_scene();
					parent = &boss - Objects;
#ifdef DXX_BUILD_DESCENT_II
					if (profile == 2) Robot_info[boss.id].boss_flag = BOSS_D2;
#endif
					Boss_dying = 1;
					Boss_dying_sound_playing = 1;
					Boss_dying_start_time = GameTime64 - (scenario == 3 ? F1_0 / 4 : 21 * F1_0 / 4);
				} else if (scenario == 5) {
					Players[0].objnum = 0;
					start_player_death_sequence(ConsoleObject);
				} else if (scenario == 1)
					ConsoleObject->size = 512 * F1_0; // Original algorithm rejects outside-mine offsets
				Vclip[VCLIP_SMALL_EXPLOSION].play_time = F1_0 / 8;
				Weapon_info[0] = {};
				Weapon_info[0].speed[Difficulty_level] = 40 * F1_0;
				d_srand(1234);
				d_rand_reset_call_count();
				d_srand_fx(fx_seed);
				d_rand_set_stream_call_count(D_RNG_FX, fx_seed == 1 ? 0 : 3000);
				json observed = json::array();
				int fireballs_seen = 0;
				for (int frame = 0; frame < 32; ++frame) {
					const fix times[] = { F1_0 / 120, 1114, F1_0 / 90, 327 };
					FrameTime = times[frame % 4];
					GameTime64 += FrameTime;
					d_tick_count = frame / 2;
					d_tick_step = frame % 2;
					if (scenario <= 1) {
						const int old_attached = Objects[parent].attached_obj;
						create_small_fireball_on_object(&Objects[parent], F1_0, 0);
						require(scenario == 1 ? Objects[parent].attached_obj == old_attached : Objects[parent].attached_obj != old_attached,
						        "original offset validation admits valid points and rejects outside-mine points");
					} else if (scenario == 2) do_controlcen_dead_frame();
					else if (scenario <= 4) {
#ifdef DXX_BUILD_DESCENT_II
						if (profile == 2) do_boss_dying_frame(&Objects[parent]);
						else
#endif
							do_ai_frame(&Objects[parent]);
					} else dead_player_frame();
					if (frame % 3 == 0) {
						vms_vector point = {};
						const int slot = obj_create(OBJ_WEAPON, 0, 0, &point, &vmd_identity_matrix, F1_0 / 8, CT_WEAPON, MT_NONE, RT_NONE);
						require(slot > 0, "allocate following gameplay weapon");
						Objects[slot].lifeleft = F1_0;
					}
					for (int slot = 1; slot <= Highest_object_index; ++slot) {
						auto &object = Objects[slot];
						if (object.type == OBJ_FIREBALL) {
							++fireballs_seen;
							object.lifeleft -= FrameTime;
							do_explosion_sequence(&object);
						} else if (object.type == OBJ_WEAPON)
							Laser_do_weapon_sequence(&object, 0, F1_0 / 60, frame, 1);
					}
					obj_delete_all_that_should_be_dead();
					observed.push_back(fx_isolation_gameplay_snapshot());
				}
				if (scenario != 1 && !fireballs_seen) std::fprintf(stderr, "Empty FX route profile=%d scenario=%d seed=%u\n", profile, scenario, fx_seed);
				require(scenario == 1 ? fireballs_seen == 0 : fireballs_seen > 0, "differential route preserves original admission behavior");
				unsigned fx_state = 0;
				require(d_rand_get_stream_state(D_RNG_FX, &fx_state), "observe effects variation");
				if (fx_seed == 1) {
					expected = observed;
					reference_fx_state = fx_state;
				} else if (expected != observed) {
					for (size_t frame = 0; frame < expected.size(); ++frame)
						if (expected[frame] != observed[frame]) {
							const auto differences = json::diff(expected[frame], observed[frame]);
							std::fprintf(stderr, "FX isolation profile=%d scenario=%d seed=%u first_frame=%zu differences=%zu first=%s\n", profile, scenario, fx_seed, frame, differences.size(), differences.front().dump().c_str());
							break;
						}
					require(false, "FX variation changed SIM, gameplay object state, allocation or weapon scheduling");
				}
				require(fx_seed == 1 || fx_state != reference_fx_state, "effects stream genuinely varies independently");
				if (scenario == 5) dead_player_end();
				Dead_controlcen_object_num = -1;
				Boss_dying = 0;
			}
		}
	}
	Vclip[VCLIP_SMALL_EXPLOSION] = original_clip;
	Weapon_info[0] = original_weapon;
#ifdef DXX_BUILD_DESCENT_II
	GameArg.SndDigiSampleRate = sample_rate;
	Current_mission = nullptr;
#endif
}
