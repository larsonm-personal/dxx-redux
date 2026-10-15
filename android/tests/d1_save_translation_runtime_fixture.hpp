// Actual native D1 save -> imported D1 gameplay, including the early version 23 epoch
#include "controls.h"
#include "save_epoch_format.h"

static void exercise_d1_save_translation_runtime(const char *directory, const char *checkpoints)
{
	using nlohmann::json;
	const std::string hog = std::string(directory) + "/descent.hog";
	require(PHYSFS_mount(directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "mount matching public D1 resources");
	char error[256] = {};
	require(input_demo_configure_startup_fp_environment(error, sizeof(error)), error);
	Game_mode = 0;
	GameArg.SndNoSound = GameArg.SndNoMusic = 1;
	GameArg.SysInputDemoNoRender = 1;
	GameArg.SysWindow = GameCfg.WindowMode = 1;
	GameCfg.AspectX = 4;
	GameCfg.AspectY = 3;
	GameCfg.TexFilt = 0;
	Game_screen_mode = SM(640, 480);
	digi_select_system(SDLAUDIO_SYSTEM);
#ifdef DXX_BUILD_DESCENT_II
	GameArg.SndDigiSampleRate = SAMPLE_RATE_11K;
	require(PHYSFS_mount(checkpoints, "checkpoints", 1), "mount native phase/Fusion checkpoints");
	require(d1_in_d2_init_base_resources(1), "select original D1 resources for checkpoint translation");
#else
	(void)checkpoints;
#endif
	load_text();
	require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0 && gr_init(Game_screen_mode) == 0, "initialize native save thumbnail renderer");
	key_init();
	mouse_init();
	char palette[] = "palette.256";
	gr_use_palette_table(palette);
	gamefont_init();
	gamedata_init();
	texmerge_init(10);
#ifdef DXX_BUILD_DESCENT_II
	d1_in_d2_init_startup_bitmaps();
	char mission[] = "descent";
#else
	char mission[] = "";
#endif
	init_game();
	new_player_config();
	require(load_mission_by_name(mission), "select native D1 mission for phase/Fusion checkpoints");
	Player_num = 0;
	N_players = 1;
	std::strcpy(Players[0].callsign, "phase");
	Difficulty_level = 2;
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	if (!Game_wind) ::game();
	const auto step = []() {
		FrameTime = F1_0 / 30;
		Controls = {};
		do_weapon_n_item_stuff();
		calc_game_time();
		GameProcessFrame();
		require(render_update_main_view_objects(), "update imported continuation gameplay view");
	};
	const auto snapshot = []() {
		const object &o = *ConsoleObject;
		unsigned int sim = 0, fx = 0;
		d_rand_get_state(&sim);
		d_rand_get_stream_state(D_RNG_FX, &fx);
		int fusion_shots = 0;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_WEAPON && Objects[i].id == FUSION_ID && Objects[i].ctype.laser_info.parent_type == OBJ_PLAYER)
				++fusion_shots;
		return json { { "phase", controls_get_wiggle_phase() },
			{ "pos", { o.pos.x, o.pos.y, o.pos.z } },
			{ "orient", { o.orient.rvec.x, o.orient.rvec.y, o.orient.rvec.z,
			                o.orient.uvec.x, o.orient.uvec.y, o.orient.uvec.z,
			                o.orient.fvec.x, o.orient.fvec.y, o.orient.fvec.z } },
			{ "velocity", { o.mtype.phys_info.velocity.x, o.mtype.phys_info.velocity.y, o.mtype.phys_info.velocity.z } },
			{ "shields", +Players[Player_num].shields }, { "energy", +Players[Player_num].energy },
			{ "fusion_charge", Fusion_charge }, { "fusion_active", Auto_fire_fusion_cannon_time != 0 },
			{ "fusion_shots", fusion_shots }, { "sim", sim }, { "fx", fx } };
	};
	json report = json::array();
	int scenario = 0;
	for (const char *name : { "idle", "due", "past", "soon", "future", "inactive-charged" }) {
		const std::string stem = std::string("phase-fusion-") + name;
#ifndef DXX_BUILD_DESCENT_II
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		for (int frame = 0; frame < 17; ++frame) step();
		// Public shareware excludes Fusion geometry; make the explicit synthetic
		// weapon fixture use its valid laser geometry identically in both engines
		Weapon_info[FUSION_ID] = Weapon_info[LASER_ID_L1];
		Players[Player_num].primary_weapon_flags |= 1 << FUSION_INDEX;
		Players[Player_num].primary_weapon = FUSION_INDEX;
		Players[Player_num].energy = 100 * F1_0;
		Fusion_charge = scenario ? F1_0 / 2 : 0;
		Auto_fire_fusion_cannon_time = scenario == 1 ? GameTime64 : scenario == 2 ? GameTime64 - 1 :
		                               scenario == 3 ? GameTime64 + 1 : scenario == 4 ? GameTime64 + 10 * (F1_0 / 30) : 0;
		Next_laser_fire_time = GameTime64;
		// Include wrap and lower-byte carry, without altering the ordinary save clock
		controls_restore_wiggle_phase(scenario & 1 ? 65535 : 255);
		char description[21] = "Phase Fusion import";
		std::string filename = stem + ".sav";
		stop_time();
		require(state_save_all_sub(&filename[0], description), "write actual native version23 epoch/phase/Fusion checkpoint");
		const auto saved = read_save_fixture(filename);
		require(saved[4] == SAVE_EPOCH_D1_VERSION, "native producer writes version23");
		write_fixture((stem + "-opposite.sav").c_str(), save_endian_fixture(saved).convert());
		json expected = { { "saved_epoch", GameTime64 }, { "initial", snapshot() }, { "frames", json::array() } };
		for (int frame = 0; frame < 16; ++frame) {
			step();
			expected["frames"].push_back(snapshot());
		}
		require(expected["frames"][0]["fusion_shots"] == (scenario >= 1 && scenario <= 3 ? 2 : 0),
		        "native next-frame Fusion observation is nonvacuous");
		require(expected["frames"][10]["fusion_shots"] == (scenario >= 1 && scenario <= 4 ? 2 : 0),
		        "native continuation observes scheduled Fusion firing");
		const std::string text = expected.dump(2) + "\n";
		write_fixture((stem + ".json").c_str(), bytes(text.begin(), text.end()));
		report.push_back({ { "scenario", name }, { "phase", expected["initial"]["phase"] }, { "frames", 16 } });
#else
		const bytes expected_bytes = read_save_fixture("checkpoints/" + stem + ".json");
		const json expected = json::parse(expected_bytes.begin(), expected_bytes.end());
		for (const char *suffix : { ".sav", "-opposite.sav" }) {
			const bytes saved = read_save_fixture("checkpoints/" + stem + suffix);
			d1_save_translate_checkpoint_start start = {};
			require(d1_save_translate_read_checkpoint_start(saved.data(), saved.size(), &start) && start.version == SAVE_EPOCH_D1_VERSION,
			        "decode actual native version23 checkpoint");
			require(start.game_time == 0 && start.has_saved_epoch && start.saved_epoch > 0 &&
			            start.saved_epoch == expected["saved_epoch"].get<fix64>(),
			        "keep the legacy replay clock separate from validated epoch metadata");
			const size_t epoch_offset = 8 + 20 + 100 * 50 * 3 + 4 + 9 + 3 * 4;
			input_demo_set_skip_level_intro(1);
			StartNewGame(start.current_level);
			GameTime64 = start.saved_epoch;
			Difficulty_level = start.difficulty;
			Weapon_info[FUSION_ID] = Weapon_info[LASER_ID_L1];
			if (!scenario) {
				for (int version : { 14, 20, 22, 24, 25 }) {
					bytes damaged = saved;
					set_int(damaged, 4, start.checkpoint_swap ? SWAPINT(version) : version);
					d1_save_translate_checkpoint_start rejected = {};
					require(!d1_save_translate_read_checkpoint_start(damaged.data(), damaged.size(), &rejected),
					        "reject Android and unknown checkpoint layouts");
				}
				for (size_t length = 0; length < SAVE_EPOCH_DISK_BYTES; ++length) {
					d1_save_translate_checkpoint_start rejected = {};
					require(!d1_save_translate_read_checkpoint_start(saved.data(), epoch_offset + length, &rejected),
					        "reject every incomplete early epoch before player decoding");
				}
				for (uint64_t epoch : { uint64_t(0), uint64_t(SAVE_EPOCH_MAX), uint64_t(SAVE_EPOCH_MAX) + 1, UINT64_MAX }) {
					bytes changed = saved;
					uint32_t words[2] = { uint32_t(epoch), uint32_t(epoch >> 32) };
					if (start.checkpoint_swap) {
						words[0] = SWAPINT(words[0]);
						words[1] = SWAPINT(words[1]);
					}
					std::memcpy(changed.data() + epoch_offset, words, sizeof(words));
					d1_save_translate_checkpoint_start decoded = {};
					const bool valid = epoch <= uint64_t(SAVE_EPOCH_MAX);
					require(bool(d1_save_translate_read_checkpoint_start(changed.data(), changed.size(), &decoded)) == valid,
					        "validate the complete epoch range in both byte orders");
					if (valid)
						require(decoded.has_saved_epoch && decoded.saved_epoch == fix64(epoch) &&
						            decoded.game_time == 0 && decoded.object_stream_offset == start.object_stream_offset &&
						            decoded.energy == start.energy && GameTime64 == start.saved_epoch,
						        "epoch metadata neither shifts the player/object stream nor publishes a replay clock");
				}
			}
			controls_restore_wiggle_phase(12345);
			Auto_fire_fusion_cannon_time = 98765;
			const json before = snapshot();
			const std::vector<object> previous_objects(Objects, Objects + Highest_object_index + 1);
			for (int invalid = 0; invalid < 12; ++invalid) {
				bytes damaged = saved;
				if (invalid < 8) damaged.resize(damaged.size() - invalid - 1);
				else {
					const int value = invalid == 8 ? -1 : invalid == 9 ? 65536 : invalid == 10 ? -1 : 2;
					set_int(damaged, damaged.size() - (invalid < 10 ? 8 : 4), start.checkpoint_swap ? SWAPINT(value) : value);
				}
				require(!d1_save_translate_apply_checkpoint_objects(damaged.data(), damaged.size(), &start),
				        "reject malformed phase/Fusion tail in either byte order");
				require(snapshot() == before && std::memcmp(Objects, previous_objects.data(), previous_objects.size() * sizeof(object)) == 0,
				        "failed translation leaves objects, phase, Fusion and RNG unchanged");
			}
			require(d1_save_translate_apply_checkpoint_objects(saved.data(), saved.size(), &start), "apply actual native checkpoint through translator");
			d1_save_translate_apply_checkpoint_player(&start, "phase");
			ConsoleObject = Viewer = &Objects[Players[Player_num].objnum];
			require(GameTime64 == start.saved_epoch && snapshot() == expected["initial"], "translation preserves the caller-selected epoch and initial phase/Fusion state");
			if (scenario == 1) require(Auto_fire_fusion_cannon_time == GameTime64, "translated active due-now deadline follows the caller clock");
			for (int frame = 0; frame < 16; ++frame) {
				step();
				if (snapshot() != expected["frames"][frame])
					std::fprintf(stderr, "D1 translation %s%s frame=%d expected=%s actual=%s\n", name, suffix, frame,
					             expected["frames"][frame].dump().c_str(), snapshot().dump().c_str());
				require(snapshot() == expected["frames"][frame], "translated gameplay matches native phase/Fusion continuation");
			}
			// An external replay may deliberately select a different clock
			GameTime64 = 0;
			require(d1_save_translate_apply_checkpoint_objects(saved.data(), saved.size(), &start), "translate version23 at an external zero epoch");
			d1_save_translate_apply_checkpoint_player(&start, "phase");
			require(GameTime64 == 0 && snapshot() == expected["initial"], "saved epoch metadata never overrides the external replay clock");
			if (scenario == 1) require(Auto_fire_fusion_cannon_time == -1, "translated active due-now deadline uses nonzero sentinel at zero epoch");
			// Native21 has the phase/active tail, but no early epoch field
			bytes legacy = saved;
			legacy.erase(legacy.begin() + epoch_offset, legacy.begin() + epoch_offset + SAVE_EPOCH_DISK_BYTES);
			d1_save_translate_checkpoint_start legacy_start = {};
			for (int version : { 15, 16, 17, 18, 19, 21 }) {
				set_int(legacy, 4, start.checkpoint_swap ? SWAPINT(version) : version);
				require(d1_save_translate_read_checkpoint_start(legacy.data(), legacy.size(), &legacy_start) &&
				            !legacy_start.has_saved_epoch && legacy_start.game_time == 0 &&
				            legacy_start.object_stream_offset + SAVE_EPOCH_DISK_BYTES == start.object_stream_offset,
				        "decode every supported native legacy header without an epoch field");
			}
			require(d1_save_translate_apply_checkpoint_objects(legacy.data(), legacy.size(), &legacy_start), "preserve native21 translation with its original header and tail");
			require(GameTime64 == 0 && snapshot() == expected["initial"], "native21 retains phase/Fusion state at the external epoch");
			// Native19 also lacks phase/active words; preserve its historical delta
			// interpretation while clearing phase left by a newer checkpoint
			legacy.resize(legacy.size() - 8);
			set_int(legacy, 4, start.checkpoint_swap ? SWAPINT(19) : 19);
			require(d1_save_translate_read_checkpoint_start(legacy.data(), legacy.size(), &legacy_start), "decode genuine native19 prefix");
			GameTime64 = 1234;
			controls_restore_wiggle_phase(54321);
			require(d1_save_translate_apply_checkpoint_objects(legacy.data(), legacy.size(), &legacy_start), "preserve native19 translation");
			require(GameTime64 == 1234 && controls_get_wiggle_phase() == 1234, "legacy translation preserves the caller clock and clears previously restored phase offset");
			if (scenario == 1) require(Auto_fire_fusion_cannon_time == GameTime64, "legacy translation retains unflagged deadline behavior");
			report.push_back({ { "scenario", name }, { "endian", start.checkpoint_swap ? "opposite" : "native" }, { "frames", 16 } });
		}
#endif
		++scenario;
	}
	// The native writer reserves a zero boss-death delta by emitting -1
	// At epoch 1 that active delta must not decode back into the inactive zero
	for (int active : { 0, 1 }) {
		const std::string stem = active ? "boss-tiny-epoch-active" : "boss-tiny-epoch-inactive";
#ifndef DXX_BUILD_DESCENT_II
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		GameTime64 = 1;
		Boss_dying = active;
		Boss_dying_start_time = active;
		Boss_dying_sound_playing = 0;
		std::string filename = stem + ".sav";
		char description[21] = "Tiny boss epoch";
		stop_time();
		require(state_save_all_sub(&filename[0], description), "write actual native tiny-epoch boss checkpoint");
		const auto saved = read_save_fixture(filename);
		write_fixture((stem + "-opposite.sav").c_str(), save_endian_fixture(saved).convert());
		report.push_back({ { "scenario", stem }, { "saved_epoch", GameTime64 }, { "boss_start", Boss_dying_start_time } });
#else
		for (const char *suffix : { ".sav", "-opposite.sav" }) {
			const bytes saved = read_save_fixture("checkpoints/" + stem + suffix);
			d1_save_translate_checkpoint_start start = {};
			require(d1_save_translate_read_checkpoint_start(saved.data(), saved.size(), &start) &&
			            start.has_saved_epoch && start.saved_epoch == 1 && start.game_time == 0,
			        "decode native tiny-epoch boss checkpoint in both byte orders");
			input_demo_set_skip_level_intro(1);
			StartNewGame(start.current_level);
			GameTime64 = start.saved_epoch;
			Boss_dying = !active;
			Boss_dying_start_time = 12345;
			require(d1_save_translate_apply_checkpoint_objects(saved.data(), saved.size(), &start), "translate native tiny-epoch boss checkpoint");
			require(GameTime64 == 1 && Boss_dying == active && Boss_dying_start_time == (active ? -1 : 0),
			        "tiny-epoch translation preserves active boss death and the inactive-zero sentinel");
			report.push_back({ { "scenario", stem }, { "endian", start.checkpoint_swap ? "opposite" : "native" },
			                   { "restored_epoch", GameTime64 }, { "boss_start", Boss_dying_start_time } });
		}
#endif
	}
	const std::string result = report.dump(2) + "\n";
	write_fixture("d1-phase-fusion-translation.json", bytes(result.begin(), result.end()));
	std::puts("PASS: actual native D1 phase/Fusion saves and imported continuation");
}
