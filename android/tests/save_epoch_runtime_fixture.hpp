// Native checkpoint epochs, malformed headers and real continuation on public assets
#include "save_epoch_runtime.h"
#include "input_demo_codec.h"
#ifdef DXX_BUILD_DESCENT_II
extern "C" {
extern fix64 Time_flash_last_played;
extern fix Flash_effect;
void diminish_palette_towards_normal(void);
}
#endif

static int save_epoch_fixture_restore(char *filename)
{
#ifdef DXX_BUILD_DESCENT_II
	return state_restore_all_sub(filename, 0);
#else
	return state_restore_all_sub(filename);
#endif
}

static void test_save_epoch_record()
{
	const fix64 live_epoch = GameTime64;
	const fix64 epochs[] = { 0, 1, ((fix64)1 << 32) - 1, ((fix64)1 << 40) + 12345, SAVE_EPOCH_MAX };
	for (const fix64 epoch : epochs) {
		PHYSFS_file *fp = PHYSFS_openWrite("save-epoch-record.bin");
		require(fp && save_epoch_write(fp, epoch) && PHYSFS_close(fp), "write valid full-width epoch");
		const bytes native = read_save_fixture("save-epoch-record.bin");
		require(native.size() == SAVE_EPOCH_DISK_BYTES, "authoritative epoch occupies exactly eight bytes");
		PHYSFS_uint32 words[2];
		std::memcpy(words, native.data(), sizeof(words));
		require(words[0] == (PHYSFS_uint32)epoch && words[1] == (PHYSFS_uint32)((PHYSFS_uint64)epoch >> 32),
		        "epoch wire order is low word then high word");
		for (int swap : { 0, 1 }) {
			bytes encoded = native;
			if (swap) for (size_t offset : { size_t(0), size_t(4) })
				std::reverse(encoded.begin() + offset, encoded.begin() + offset + 4);
			for (size_t length = 0; length <= encoded.size(); ++length) {
				write_fixture("save-epoch-check.bin", bytes(encoded.begin(), encoded.begin() + length));
				fix64 actual = -17;
				fp = PHYSFS_openRead("save-epoch-check.bin");
				require(fp != nullptr, "open epoch record boundary control");
				const int accepted = save_epoch_read(fp, swap, &actual);
				PHYSFS_close(fp);
				require(accepted == (length == SAVE_EPOCH_DISK_BYTES), "epoch reader rejects every truncated record in either byte order");
				require(actual == (accepted ? epoch : -17) && GameTime64 == live_epoch, "epoch decode is exact and never changes live game time");
			}
		}
	}
	const uint64_t invalid[] = { uint64_t(SAVE_EPOCH_MAX) + 1, uint64_t(INT64_MAX), uint64_t(1) << 63, UINT64_MAX };
	for (const uint64_t value : invalid) for (int swap : { 0, 1 }) {
		PHYSFS_uint32 words[] = { PHYSFS_uint32(value), PHYSFS_uint32(value >> 32) };
		if (swap) for (auto &word : words) word = SWAPINT(word);
		bytes encoded(sizeof(words));
		std::memcpy(encoded.data(), words, sizeof(words));
		write_fixture("save-epoch-check.bin", encoded);
		fix64 actual = -17;
		PHYSFS_file *fp = PHYSFS_openRead("save-epoch-check.bin");
		require(fp && !save_epoch_read(fp, swap, &actual) && PHYSFS_close(fp), "reject out-of-range or negative epoch bit patterns");
		require(actual == -17 && GameTime64 == live_epoch, "invalid epoch fails without changing output or live clock");
	}
	for (const fix64 epoch : { fix64(-1), fix64(SAVE_EPOCH_MAX) + 1 }) {
		PHYSFS_file *fp = PHYSFS_openWrite("save-epoch-check.bin");
		require(fp && !save_epoch_write(fp, epoch) && PHYSFS_fileLength(fp) == 0 && PHYSFS_close(fp), "invalid epoch writer emits no partial record");
	}
}

static nlohmann::json save_epoch_fixture_snapshot()
{
	const object &o = *ConsoleObject;
	unsigned int sim = 0, fx = 0;
	require(d_rand_get_state(&sim) && d_rand_get_stream_state(D_RNG_FX, &fx), "observe checkpoint RNG streams");
	return { { "epoch", GameTime64 }, { "phase", controls_get_wiggle_phase() },
	         { "position", { o.pos.x, o.pos.y, o.pos.z } },
	         { "orientation", { o.orient.rvec.x, o.orient.rvec.y, o.orient.rvec.z,
	                            o.orient.uvec.x, o.orient.uvec.y, o.orient.uvec.z,
	                            o.orient.fvec.x, o.orient.fvec.y, o.orient.fvec.z } },
	         { "velocity", { o.mtype.phys_info.velocity.x, o.mtype.phys_info.velocity.y, o.mtype.phys_info.velocity.z } },
	         { "rotation", { o.mtype.phys_info.rotvel.x, o.mtype.phys_info.rotvel.y, o.mtype.phys_info.rotvel.z } },
	         { "shields", int(Players[Player_num].shields) }, { "sim", sim }, { "fx", fx } };
}

static void save_epoch_fixture_step()
{
	FrameTime = F1_0 / 30;
	Controls = {};
	do_weapon_n_item_stuff();
	calc_game_time();
	GameProcessFrame();
	require(render_update_main_view_objects(), "update epoch continuation gameplay view");
}

static void exercise_save_epoch_runtime(const char *scenario)
{
	using nlohmann::json;
	char filename[] = "save-epoch-runtime.sav", description[21] = "Save epoch runtime";
	const auto save = [&]() {
		stop_time();
		require(state_save_all_sub(filename, description), "write ordinary native-epoch checkpoint");
	};
	const auto restore = [&]() { require(save_epoch_fixture_restore(filename), "restore ordinary native-epoch checkpoint"); };
	test_save_epoch_record();
	if (std::strcmp(scenario, "record") == 0) {
		std::puts("PASS: epoch record ranges, all byte truncations, atomic decode and both byte orders");
		return;
	}
	if (std::strcmp(scenario, "load") == 0) {
		const bytes data = read_save_fixture("save-epoch-reference.json");
		const json reference = json::parse(data.begin(), data.end());
		GameTime64 = 0;
		restore();
		require(save_epoch_fixture_snapshot() == reference["initial"], "cold ordinary restore retains exact saved epoch and player state");
		for (const auto &expected : reference["frames"]) {
			save_epoch_fixture_step();
			require(save_epoch_fixture_snapshot() == expected, "cold-load continuation matches the previous process at its native epoch");
		}
		std::puts("PASS: separate-process epoch restore and 30 real continuation frames");
		return;
	}
	if (std::strcmp(scenario, "replay") == 0) {
		const fix64 saved_epoch = ((fix64)1 << 40) + 12345;
		const fix64 replay_epoch = ((fix64)1 << 35) + 45678;
		GameTime64 = saved_epoch;
		Next_laser_fire_time = saved_epoch + 17;
		save();
		const bytes checkpoint = read_save_fixture(filename);
		input_demo_file demo;
		std::string message;
		require(input_demo_file_read("capture.dximdemo", &demo, &message), message.c_str());
		demo.metadata.start_mode = "save_checkpoint";
		demo.metadata.start_save = filename;
		demo.metadata.has_level_start = false;
		demo.has_checkpoint = true;
		demo.checkpoint.format = "dgss";
		demo.checkpoint.encoding = "base64";
		demo.checkpoint.compression = "none";
		demo.checkpoint.size = static_cast<uint32_t>(checkpoint.size());
		demo.checkpoint.save_name = filename;
		demo.checkpoint.has_start_gt = 1;
		demo.checkpoint.start_gt = replay_epoch;
		require(input_demo_sha256_hex(checkpoint.data(), checkpoint.size(), &demo.checkpoint.sha256, &message), message.c_str());
		require(input_demo_base64_encode(checkpoint.data(), checkpoint.size(), &demo.checkpoint.data, &message), message.c_str());
		require(input_demo_file_write("save-epoch-replay.dximdemo", demo, &message), message.c_str());
		char error[512] = {};
		require(input_demo_replay_load("save-epoch-replay.dximdemo", error, sizeof(error)), error);
		require(input_demo_replay_has_checkpoint() && input_demo_replay_checkpoint_start_gt() == replay_epoch,
		        "real replay loader activates a checkpoint with an independent absolute epoch");
		GameTime64 = 99;
		restore();
		require(GameTime64 == replay_epoch && Next_laser_fire_time == replay_epoch + 17,
		        "loaded replay checkpoint epoch takes precedence without adding the saved absolute epoch");
		input_demo_replay_unload();
		restore();
		require(GameTime64 == saved_epoch && Next_laser_fire_time == saved_epoch + 17,
		        "the same checkpoint returns to ordinary saved-epoch behavior after replay unload");
		std::puts("PASS: actual replay-loader checkpoint override and ordinary-load precedence control");
		return;
	}
#ifdef DXX_BUILD_DESCENT_II
	if (std::strcmp(scenario, "secret") == 0) {
		require(Current_level_num > 0 && Secret_return_segment >= 0 && Secret_return_segment <= Highest_segment_index,
		        "public base map provides a safe authored secret-return segment");
		const int expected_return_segment = Secret_return_segment;
		const vms_matrix expected_return_orientation = Secret_return_orient;
		GameTime64 = ((fix64)1 << 40) + 12345;
		const fix64 saved_epoch = GameTime64;
		const int saved_objects = Highest_object_index;
		save();
		// Invoke the actual secret-return save reader on public base-level data;
		// this is not an authored secret-level travel or exit-trigger test
		GameTime64 += 10 * F1_0;
		Players[Player_num].flags &= ~(PLAYER_FLAGS_CLOAKED | PLAYER_FLAGS_INVULNERABLE);
		Players[Player_num].energy = 73 * F1_0;
		Players[Player_num].shields = 81 * F1_0;
		require(state_restore_all_sub(filename, 1), "restore new-format checkpoint through actual native secret-return path");
		require(GameTime64 == 0 && Highest_object_index == saved_objects &&
		            Players[Player_num].energy == 73 * F1_0 && Players[Player_num].shields == 81 * F1_0,
		        "secret-return consumes the saved epoch while retaining zero clock and carried inventory policy");
		vms_vector expected_position;
		compute_segment_center(&expected_position, &Segments[expected_return_segment]);
		require(ConsoleObject->segnum == expected_return_segment &&
		            !std::memcmp(&ConsoleObject->pos, &expected_position, sizeof(expected_position)) &&
		            !std::memcmp(&ConsoleObject->orient, &expected_return_orientation, sizeof(expected_return_orientation)) &&
		            controls_get_wiggle_phase() == 0,
		        "secret-return retains authored return pose and legacy phase reset");
		restore();
		require(GameTime64 == saved_epoch, "same save still selects its native epoch through ordinary restore");
		std::puts("PASS: actual native D2 secret-return reader, carried inventory and zero-clock policy");
		return;
	}
	if (std::strcmp(scenario, "flash") == 0) {
		for (const fix64 epoch : { fix64(10) * F1_0, ((fix64)1 << 40) + 12345 }) {
			const fix64 times[] = { 0, epoch, epoch - 1, epoch + 1, epoch - F1_0 / 8, epoch - F1_0 / 8 - 1 };
			for (const fix64 timestamp : times) {
				GameTime64 = epoch;
				Time_flash_last_played = timestamp;
				Flash_effect = F1_0;
				PaletteRedAdd = 2;
				PaletteGreenAdd = 3;
				PaletteBlueAdd = 4;
				save();
				const auto step = []() {
					FrameTime = F1_0 / 30;
					diminish_palette_towards_normal();
					unsigned fx = 0;
					require(d_rand_get_stream_state(D_RNG_FX, &fx), "observe flash continuation effects RNG");
					return json { { "last_played", Time_flash_last_played }, { "flash", Flash_effect },
					              { "palette", { PaletteRedAdd, PaletteGreenAdd, PaletteBlueAdd } }, { "fx", fx } };
				};
				const auto expected = step();
				restore();
				require(GameTime64 == epoch && Time_flash_last_played == timestamp,
				        "flash ordinary restore preserves inactive zero, exact-now and nearby absolute timestamps");
				const auto actual = step();
				if (actual != expected)
					std::fprintf(stderr, "flash epoch=%lld timestamp=%lld differences=%s\n", (long long)epoch, (long long)timestamp,
					             json::diff(expected, actual).dump().c_str());
				require(actual == expected, "real flash cadence, palette and effects RNG match uninterrupted continuation");
			}
		}
		std::puts("PASS: D2 flash active sentinel and real cadence at normal and high epochs");
		return;
	}
	if (std::strcmp(scenario, "ai-phase") == 0) {
		int slot = -1;
		for (int i = 0; i <= Highest_object_index; ++i) {
			const object &o = Objects[i];
			if (o.type == OBJ_ROBOT && !Robot_info[o.id].attack_type && !Robot_info[o.id].thief && !Robot_info[o.id].kamikaze && !Robot_info[o.id].companion) {
				slot = i;
				break;
			}
		}
		require(slot >= 0, "public level provides an ordinary native D2 AI actor");
		const int objval = (slot & 15) ^ 10;
		const int phase = objval > 4 ? objval : objval ^ 15;
		GameTime64 = ((fix64)1 << 40) + ((fix64)phase << 18) + 12345;
		const fix64 expected_epoch = GameTime64;
		Objects[slot].ctype.ai_info.danger_laser_num = -1;
		vm_vec_zero(&Objects[slot].mtype.phys_info.velocity);
		Ai_local_info[slot].next_fire = -8 * F1_0;
		FrameTime = F1_0 / 30;
		save();
		const auto decision = [&]() {
			vms_vector direction = { 0, 0, F1_0 };
			ai_move_relative_to_player(&Objects[slot], &Ai_local_info[slot], 1000 * F1_0, &direction, 10 * F1_0, 0, 1);
			const auto &v = Objects[slot].mtype.phys_info.velocity;
			unsigned sim = 0;
			require(d_rand_get_state(&sim), "observe native AI decision SIM state");
			return json { { "velocity", { v.x, v.y, v.z } }, { "sim", sim } };
		};
		const json expected = decision();
		restore();
		FrameTime = F1_0 / 30;
		require(GameTime64 == expected_epoch && decision() == expected, "native AI movement branch retains the saved absolute clock phase");
		restore();
		FrameTime = F1_0 / 30;
		GameTime64 = 0;
		require(decision() != expected, "zero-epoch negative control changes the real native AI movement decision");
		std::puts("PASS: real native D2 AI epoch-dependent movement with a zero-clock negative control");
		return;
	}
#endif
	if (std::strcmp(scenario, "high") == 0 || std::strcmp(scenario, "endian") == 0) {
		GameTime64 = ((fix64)1 << 40) + 12345;
		Next_laser_fire_time = GameTime64 + 17;
		Next_missile_fire_time = GameTime64 + 31;
		Last_laser_fired_time = GameTime64 - 19;
		Next_flare_fire_time = GameTime64 + 127;
		const fix64 expected_epoch = GameTime64;
		const auto expected = save_epoch_fixture_snapshot();
		save();
		GameTime64 = 0;
		restore();
		require(GameTime64 == expected_epoch && save_epoch_fixture_snapshot() == expected, "ordinary restore retains a clock beyond 32 bits");
		require(Next_laser_fire_time == expected_epoch + 17 && Next_missile_fire_time == expected_epoch + 31 &&
		            Last_laser_fired_time == expected_epoch - 19 && Next_flare_fire_time == expected_epoch + 127,
		        "authoritative epoch is installed before relative firing timestamps are decoded");
		if (std::strcmp(scenario, "endian") == 0) {
			test_complete_save_endian(filename, "save-epoch-opposite");
			require(GameTime64 == expected_epoch, "complete opposite-endian restore retains all epoch bits");
		}
		std::printf("PASS: %s ordinary epoch and relative timestamp decoding\n", scenario);
		return;
	}
	if (std::strcmp(scenario, "malformed") == 0) {
		save();
		const bytes valid = read_save_fixture(filename);
		const fix64 valid_epoch = GameTime64;
		for (const fix64 invalid_epoch : { fix64(-1), fix64(SAVE_EPOCH_MAX) + 1 }) {
			GameTime64 = invalid_epoch;
			stop_time();
			require(!state_save_all_sub(filename, description), "ordinary writer rejects invalid native epoch");
			require(read_save_fixture(filename) == valid, "invalid native epoch cannot truncate an existing checkpoint");
		}
		GameTime64 = valid_epoch;
		// Native single-player framing: id/version, description, RGB thumbnail,
		// between-level marker, mission name, current/next levels and legacy zero clock
		const size_t legacy_offset = 8 + 20 + 100 * 50 * 3 + 4 + 9 + 8;
		const size_t epoch_offset = legacy_offset + 4;
		require(valid.size() > epoch_offset + SAVE_EPOCH_DISK_BYTES, "ordinary save has a complete new-format header");
		PHYSFS_sint32 legacy = -1;
		std::memcpy(&legacy, valid.data() + legacy_offset, sizeof(legacy));
		require(legacy == 0, "legacy header clock remains zero");
		const auto before_player = save_epoch_fixture_snapshot();
		const std::vector<object> before_objects(Objects, Objects + MAX_OBJECTS);
		const auto before_world = saved_world_snapshot("save-epoch-before-reject");
		char damaged_name[] = "save-epoch-damaged.sav";
		const auto reject = [&](const bytes &damaged) {
			write_fixture(damaged_name, damaged);
			require(!save_epoch_fixture_restore(damaged_name), "reject malformed epoch header through ordinary restore");
			require(save_epoch_fixture_snapshot() == before_player && saved_world_snapshot("save-epoch-after-reject") == before_world &&
			            std::memcmp(Objects, before_objects.data(), before_objects.size() * sizeof(object)) == 0,
			        "malformed header leaves player, objects, observed world, epoch and both RNG streams unchanged");
		};
		for (size_t length = 0; length < SAVE_EPOCH_DISK_BYTES; ++length)
			reject(bytes(valid.begin(), valid.begin() + epoch_offset + length));
		for (const uint64_t value : { uint64_t(SAVE_EPOCH_MAX) + 1, uint64_t(INT64_MAX), uint64_t(1) << 63, UINT64_MAX }) {
			bytes damaged = valid;
			const PHYSFS_uint32 words[] = { PHYSFS_uint32(value), PHYSFS_uint32(value >> 32) };
			std::memcpy(damaged.data() + epoch_offset, words, sizeof(words));
			reject(damaged);
		}
		restore();
		require(save_epoch_fixture_snapshot() == before_player, "valid checkpoint still loads after malformed header controls");
		std::puts("PASS: truncated and out-of-range epoch headers reject before live-world replacement");
		return;
	}
	const bool repeated = std::strcmp(scenario, "repeat") == 0;
	require(repeated || std::strcmp(scenario, "ordinary") == 0 || std::strcmp(scenario, "save") == 0,
	        "known save-epoch runtime scenario");
	for (int generation = 0; generation < (repeated ? 3 : 1); ++generation) {
		if (generation) for (int frame = 0; frame < 7; ++frame) save_epoch_fixture_step();
		const auto initial = save_epoch_fixture_snapshot();
		save();
		std::vector<json> expected;
		for (int frame = 0; frame < 30; ++frame) {
			save_epoch_fixture_step();
			expected.push_back(save_epoch_fixture_snapshot());
		}
		const std::string reference = json { { "initial", initial }, { "frames", expected } }.dump();
		write_fixture("save-epoch-reference.json", bytes(reference.begin(), reference.end()));
		restore();
		require(save_epoch_fixture_snapshot() == initial, "ordinary restore retains original epoch, player state and RNG");
		for (size_t frame = 0; frame < expected.size(); ++frame) {
			save_epoch_fixture_step();
			const auto actual = save_epoch_fixture_snapshot();
			if (actual != expected[frame])
				std::fprintf(stderr, "epoch generation=%d frame=%zu differences=%s\n", generation, frame, json::diff(expected[frame], actual).dump().c_str());
			require(actual == expected[frame], "ordinary native-epoch continuation matches uninterrupted real gameplay");
		}
		restore();
	}
	if (std::strcmp(scenario, "ordinary") == 0) {
		const bytes baseline_save = read_save_fixture(filename);
		const auto baseline_player = save_epoch_fixture_snapshot();
		// Synthetic global boss timing on public data: no boss encounter is claimed
		GameTime64 = 1;
		Boss_dying = 1;
		Boss_dying_start_time = 1;
		Boss_dying_sound_playing = 0;
		save();
		restore();
		require(GameTime64 == 1 && Boss_dying == 1 && Boss_dying_start_time == -1,
		        "tiny-epoch active boss delta cannot decode into the inactive zero sentinel");
		Boss_dying = 0;
		Boss_dying_start_time = 0;
		save();
		restore();
		require(GameTime64 == 1 && Boss_dying == 0 && Boss_dying_start_time == 0,
		        "tiny-epoch inactive boss retains the zero sentinel");
		write_fixture(filename, baseline_save);
		restore();
		require(save_epoch_fixture_snapshot() == baseline_player, "boss sentinel control leaves the ordinary continuation baseline intact");
	}
	std::printf("PASS: %s saved native epoch and 30 real continuation frames per generation\n", scenario);
}
