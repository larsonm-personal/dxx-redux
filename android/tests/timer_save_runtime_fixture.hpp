// Ordinary-save boundaries on public levels, with synthetic timing and seismic events
// Pickup uses the real drop action; contact is represented by calling do_powerup
extern "C" {
int allowed_to_fire_laser(void);
int allowed_to_fire_missile(void);
int allowed_to_fire_flare(void);
void do_cloak_stuff(void);
void do_invulnerable_stuff(void);
#ifdef DXX_BUILD_DESCENT_II
void DropSecondaryWeapon(void);
void do_seismic_stuff(void);
int start_seismic_disturbance(void);
void StartNewLevelSecret(int level_num, int page_in_textures);
extern int First_secret_visit;
extern int Level_shake_frequency, Level_shake_duration;
#endif
}
#ifdef DXX_BUILD_DESCENT_II
#include "seismic_runtime.h"

static nlohmann::json timer_seismic_state()
{
	seismic_runtime_state state;
	weapon_get_seismic_runtime_state(&state);
	return { { "detonations", { state.detonation_times[0], state.detonation_times[1], state.detonation_times[2], state.detonation_times[3] } },
	         { "start", state.disturbance_start_time }, { "end", state.disturbance_end_time }, { "magnitude", state.tremor_magnitude } };
}

static void test_seismic_runtime_record()
{
	const fix64 previous_epoch = GameTime64;
	const fix64 record_epoch = ((fix64)1 << 40) + 1000;
	GameTime64 = record_epoch;
	const seismic_runtime_state expected = { { 0, record_epoch, record_epoch - 1, record_epoch - INT32_MAX }, record_epoch - 123, record_epoch + 456, -789 };
	const seismic_runtime_state clear = {};
	weapon_set_seismic_runtime_state(&expected);
	const auto expected_json = timer_seismic_state();
	PHYSFS_file *fp = PHYSFS_openWrite("seismic-record.bin");
	require(fp && seismic_runtime_write(fp) && PHYSFS_close(fp), "write full-width seismic record");
	PHYSFS_uint32 words[SEISMIC_RUNTIME_DISK_BYTES / 4];
	fp = PHYSFS_openRead("seismic-record.bin");
	require(fp && PHYSFS_read(fp, words, sizeof(words), 1) == 1 && PHYSFS_close(fp), "read seismic fixture words");
	for (int swap : { 0, 1 }) {
		PHYSFS_uint32 encoded[SEISMIC_RUNTIME_DISK_BYTES / 4];
		for (unsigned i = 0; i < sizeof(words) / sizeof(words[0]); ++i)
			encoded[i] = swap ? SWAPINT(words[i]) : words[i];
		for (int length = 0; length <= SEISMIC_RUNTIME_DISK_BYTES; ++length) {
			fp = PHYSFS_openWrite("seismic-record-check.bin");
			require(fp && (length == 0 || PHYSFS_write(fp, encoded, length, 1) == 1) && PHYSFS_close(fp), "write seismic truncation control");
			weapon_set_seismic_runtime_state(&clear);
			const auto before = timer_seismic_state();
			fp = PHYSFS_openRead("seismic-record-check.bin");
			require(fp != nullptr, "open seismic record control");
			const int accepted = seismic_runtime_read(fp, swap, 1, record_epoch, record_epoch);
			PHYSFS_close(fp);
			require(accepted == (length == SEISMIC_RUNTIME_DISK_BYTES), "seismic record requires all bytes in both byte orders");
			require(timer_seismic_state() == (accepted ? expected_json : before), "seismic record is exact or fails atomically");
		}
		weapon_set_seismic_runtime_state(&clear);
		const auto before = timer_seismic_state();
		fp = PHYSFS_openRead("seismic-record-check.bin");
		require(fp && seismic_runtime_read(fp, swap, 0, record_epoch, record_epoch) && PHYSFS_close(fp), "preflight complete seismic record");
		require(timer_seismic_state() == before, "seismic preflight does not mutate live state");
	}
	// Explicit override controls preserve active sentinels and ambient zero starts
	GameTime64 = 100;
	const seismic_runtime_state override_source = { { 100, 99, 0, 1 }, 0, 300, 123 };
	weapon_set_seismic_runtime_state(&override_source);
	fp = PHYSFS_openWrite("seismic-override.bin");
	require(fp && seismic_runtime_write(fp) && PHYSFS_close(fp), "write seismic override fixture");
	fp = PHYSFS_openRead("seismic-override.bin");
	require(fp && seismic_runtime_read(fp, 0, 1, 100, 0) && PHYSFS_close(fp), "rebase seismic override fixture");
	seismic_runtime_state rebased;
	weapon_get_seismic_runtime_state(&rebased);
	require(rebased.detonation_times[0] == -1 && rebased.detonation_times[1] == -1 && rebased.detonation_times[2] == 0 && rebased.detonation_times[3] == -99 &&
	            rebased.disturbance_start_time == -100 && rebased.disturbance_end_time == 200 && rebased.tremor_magnitude == 123,
	        "override retains inactive slots, active zero within one tick, and ambient endpoints as a pair");
	GameTime64 = record_epoch;
	for (int invalid = 0; invalid < 7; ++invalid) {
		seismic_runtime_state corrupt = expected;
		if (invalid == 0) corrupt.detonation_times[0] = SAVE_EPOCH_MAX + 1;
		if (invalid == 1) corrupt.disturbance_end_time = -SAVE_EPOCH_MAX - 1;
		if (invalid == 2) corrupt.tremor_magnitude = F1_0 + 1;
		if (invalid == 3) corrupt.detonation_times[0] = record_epoch - ((fix64)INT32_MAX + 1);
		if (invalid == 4) corrupt.disturbance_end_time = corrupt.disturbance_start_time + (fix64)INT32_MAX + 1;
		if (invalid == 5) corrupt.detonation_times[0] = record_epoch + 1;
		if (invalid == 6) corrupt.disturbance_end_time = corrupt.disturbance_start_time - 1;
		weapon_set_seismic_runtime_state(&corrupt);
		fp = PHYSFS_openWrite("seismic-invalid-write.bin");
		require(fp && !seismic_runtime_write(fp) && PHYSFS_close(fp), "writer rejects unsafe seismic arithmetic before emitting a record");
		PHYSFS_uint32 malformed[SEISMIC_RUNTIME_DISK_BYTES / 4];
		for (int i = 0; i < SEISMIC_RUNTIME_DETONATION_SLOTS + 2; ++i) {
			const fix64 value = i < SEISMIC_RUNTIME_DETONATION_SLOTS ? corrupt.detonation_times[i] :
				i == SEISMIC_RUNTIME_DETONATION_SLOTS ? corrupt.disturbance_start_time : corrupt.disturbance_end_time;
			const PHYSFS_uint64 bits = (PHYSFS_uint64)value;
			malformed[i * 2] = (PHYSFS_uint32)bits;
			malformed[i * 2 + 1] = (PHYSFS_uint32)(bits >> 32);
		}
		malformed[SEISMIC_RUNTIME_DISK_BYTES / 4 - 1] = (PHYSFS_uint32)corrupt.tremor_magnitude;
		for (int swap : { 0, 1 }) {
			PHYSFS_uint32 encoded[SEISMIC_RUNTIME_DISK_BYTES / 4];
			for (unsigned i = 0; i < sizeof(malformed) / sizeof(malformed[0]); ++i)
				encoded[i] = swap ? SWAPINT(malformed[i]) : malformed[i];
			fp = PHYSFS_openWrite("seismic-invalid.bin");
			require(fp && PHYSFS_write(fp, encoded, sizeof(encoded), 1) == 1 && PHYSFS_close(fp), "write corrupt seismic record fixture");
			weapon_set_seismic_runtime_state(&clear);
			const auto before = timer_seismic_state();
			fp = PHYSFS_openRead("seismic-invalid.bin");
			require(fp && !seismic_runtime_read(fp, swap, 1, record_epoch, record_epoch) && PHYSFS_close(fp), "reject invalid seismic ranges and fixed-point overflow witnesses");
			require(timer_seismic_state() == before, "invalid seismic range leaves runtime untouched");
		}
	}
	GameTime64 = previous_epoch;
	weapon_set_seismic_runtime_state(&clear);
}
#endif

static void exercise_timer_save_runtime(const char *scenario)
{
	char filename[] = "timer-runtime.sav", description[21] = "Timer runtime";
	const auto save = [&]() {
		stop_time();
		require(state_save_all_sub(filename, description), "save ordinary timer checkpoint");
	};
	const auto restore = [&]() {
#ifdef DXX_BUILD_DESCENT_II
		require(state_restore_all_sub(filename, 0), "restore ordinary timer checkpoint");
#else
		require(state_restore_all_sub(filename), "restore ordinary timer checkpoint");
#endif
	};
	const fix64 epochs[] = { 0, 10 * F1_0, ((fix64)1 << 40) + 17 };
	const fix ages[] = { 0, 1, F1_0, 2 * F1_0 - 1, 2 * F1_0, 2 * F1_0 + 1 };
#ifdef DXX_BUILD_DESCENT_II
	if (std::strcmp(scenario, "pickup") == 0) {
		for (const fix64 epoch : epochs) for (const fix age : ages) {
			GameTime64 = epoch;
			Players[Player_num].secondary_weapon = CONCUSSION_INDEX;
			Players[Player_num].secondary_ammo[CONCUSSION_INDEX] = 2;
			Players[Player_num].secondary_weapon_flags |= 1 << CONCUSSION_INDEX;
			DropSecondaryWeapon();
			int slot = -1;
			for (int i = 0; i <= Highest_object_index; ++i)
				if (Objects[i].type == OBJ_POWERUP && Objects[i].ctype.powerup_info.creation_time == epoch &&
				    (Objects[i].ctype.powerup_info.flags & PF_SPAT_BY_PLAYER)) slot = i;
			require(slot >= 0 && Players[Player_num].secondary_ammo[CONCUSSION_INDEX] == 1, "real player drop creates a concussion pickup and consumes one missile");
			GameTime64 += age;
			save();
			const int expected_used = do_powerup(&Objects[slot]);
			const int expected_ammo = Players[Player_num].secondary_ammo[CONCUSSION_INDEX];
			require(expected_used == (epoch == 0 || age >= 2 * F1_0), "retain original zero-epoch and two-second pickup semantics");
			restore();
			require(GameTime64 == epoch + age && Objects[slot].ctype.powerup_info.creation_time == epoch, "pickup save retains exact native clock and creation timestamp");
			require(do_powerup(&Objects[slot]) == expected_used && Players[Player_num].secondary_ammo[CONCUSSION_INDEX] == expected_ammo,
			        "restored pickup grace and collected ammo match uninterrupted contact");
			obj_delete(slot);
		}
		std::puts("PASS: ordinary pickup grace at zero, positive and 64-bit epochs");
		return;
	}
	if (std::strcmp(scenario, "seismic-level") == 0) {
		GameTime64 = 11 * F1_0;
		const seismic_runtime_state old_mine = { { 10 * F1_0, 0, 0, 0 }, 10 * F1_0, 12 * F1_0, 1234 };
		weapon_set_seismic_runtime_state(&old_mine);
		input_demo_set_skip_level_intro(1);
		StartNewLevel(Current_level_num);
		const seismic_runtime_state clear = {};
		seismic_runtime_state actual;
		weapon_get_seismic_runtime_state(&actual);
		if (actual.disturbance_start_time || actual.disturbance_end_time || actual.tremor_magnitude)
			std::fprintf(stderr, "Fresh-level seismic state: %s\n", timer_seismic_state().dump().c_str());
		require(GameTime64 == 0 && actual.disturbance_start_time == 0 && actual.disturbance_end_time == 0 && actual.tremor_magnitude == 0,
		        "new level resets old ambient endpoints and previous-frame tremor magnitude");
		for (int i = 0; i < SEISMIC_RUNTIME_DETONATION_SLOTS; ++i)
			require(actual.detonation_times[i] == clear.detonation_times[i], "new level resets every Earthshaker slot");
		save();
		restore();
		require(GameTime64 == 0 && timer_seismic_state()["magnitude"] == 0, "immediate fresh-level save remains valid after an old mine's seismic event");
		// The public demo has no secret map; route its unchanged first map through the real fresh-secret hook
		if (!Secret_level_names) Secret_level_names = (d_fname *)d_malloc(sizeof(d_fname));
		std::strcpy(Secret_level_names[0], Level_names[0]);
		Last_secret_level = -1;
		First_secret_visit = 1;
		GameTime64 = 11 * F1_0;
		weapon_set_seismic_runtime_state(&old_mine);
		input_demo_set_skip_level_intro(1);
		StartNewLevelSecret(-1, 0);
		weapon_get_seismic_runtime_state(&actual);
		require(actual.disturbance_start_time == 0 && actual.disturbance_end_time == 0 && actual.tremor_magnitude == 0,
		        "fresh synthetic secret entry clears old ambient endpoints and gameplay magnitude");
		for (int i = 0; i < SEISMIC_RUNTIME_DETONATION_SLOTS; ++i)
			require(actual.detonation_times[i] == 0, "fresh secret entry clears every Earthshaker slot");
		std::puts("PASS: real normal and synthetic fresh-secret seismic resets and immediate ordinary save");
		return;
	}
	if (std::strcmp(scenario, "seismic") == 0 || std::strcmp(scenario, "ambient") == 0) {
		test_seismic_runtime_record();
		const bool ambient = std::strcmp(scenario, "ambient") == 0;
		const int original_frequency = Level_shake_frequency, original_duration = Level_shake_duration;
		for (const int events : { 1, 4 }) for (const fix64 epoch : epochs) for (const fix age : ages) {
			if (ambient && events != 1) continue;
			GameTime64 = epoch;
			vm_vec_zero(&ConsoleObject->mtype.phys_info.rotvel);
			seismic_runtime_state clear = {};
			weapon_set_seismic_runtime_state(&clear);
			Level_shake_frequency = ambient ? 1 : 0;
			Level_shake_duration = ambient ? 2 * F1_0 : 0;
			FrameTime = F1_0 / 30;
			if (ambient)
				require(start_seismic_disturbance(), "synthetic public-level parameters start a real ambient event");
			else
				for (int event = 0; event < events; ++event) smega_rock_stuff();
			GameTime64 += age;
			// This previous-frame value affects guided controls and AI before the next seismic update
			seismic_runtime_state seeded;
			weapon_get_seismic_runtime_state(&seeded);
			seeded.tremor_magnitude = 1234;
			weapon_set_seismic_runtime_state(&seeded);
			const auto before = timer_seismic_state();
			save();
			const auto observe = [&]() {
				d_tick_step = 1;
				do_seismic_stuff();
				unsigned sim_state = 0;
				require(d_rand_get_state(&sim_state), "observe seismic SIM state");
				return nlohmann::json{ { "rotation", { +ConsoleObject->mtype.phys_info.rotvel.x, +ConsoleObject->mtype.phys_info.rotvel.y, +ConsoleObject->mtype.phys_info.rotvel.z } },
				                       { "runtime", timer_seismic_state() }, { "sim", sim_state } };
			};
			require(d_rand_set_stream_state(D_RNG_FX, 0x13579bdu), "seed uninterrupted cosmetic stream independently");
			const auto expected = observe();
			restore();
			if (timer_seismic_state() != before)
				std::fprintf(stderr, "Seismic checkpoint mismatch: %s\n", nlohmann::json{ { "expected", before }, { "actual", timer_seismic_state() } }.dump().c_str());
			require(GameTime64 == epoch + age && timer_seismic_state() == before, "ordinary save restores full seismic simulation state before continuation");
			// Public D2 has no configured ambient event; use identical synthetic immutable level parameters on both paths
			Level_shake_frequency = ambient ? 1 : 0;
			Level_shake_duration = ambient ? 2 * F1_0 : 0;
			FrameTime = F1_0 / 30;
			require(d_rand_set_stream_state(D_RNG_FX, 0x2468aceu), "perturb restored cosmetic stream independently");
			require(observe() == expected, "different FX seeds leave restored seismic rotation, active slots, magnitude and SIM state identical");
		}
		Level_shake_frequency = original_frequency;
		Level_shake_duration = original_duration;
		std::puts("PASS: synthetic public-level seismic continuation and atomic record controls");
		return;
	}
#endif
	require(std::strcmp(scenario, "cooldowns") == 0, "known timer runtime scenario");
	for (const fix64 epoch : epochs) for (const fix delta : { -1, 0, 1 }) {
		GameTime64 = epoch;
		Next_laser_fire_time = Next_missile_fire_time = Next_flare_fire_time = epoch + delta;
		Players[Player_num].flags |= PLAYER_FLAGS_CLOAKED | PLAYER_FLAGS_INVULNERABLE;
		Players[Player_num].cloak_time = epoch - CLOAK_TIME_MAX + delta;
		Players[Player_num].invulnerable_time = epoch - INVULNERABLE_TIME_MAX + delta;
		save();
		const auto observe = [&]() {
			const int laser = allowed_to_fire_laser(), missile = allowed_to_fire_missile(), flare = allowed_to_fire_flare();
			do_cloak_stuff();
			do_invulnerable_stuff();
			return nlohmann::json{ laser, missile, flare, !!(Players[Player_num].flags & PLAYER_FLAGS_CLOAKED), !!(Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE) };
		};
		const auto expected = observe();
		restore();
		require(GameTime64 == epoch && observe() == expected, "ordinary cooldown admission and temporary-effect expiry match at exact native epoch");
	}
	std::puts("PASS: ordinary cooldown and temporary-effect boundaries at zero, positive and 64-bit epochs");
}
