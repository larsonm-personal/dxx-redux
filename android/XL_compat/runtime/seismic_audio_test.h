#ifndef DXX_XL_SEISMIC_AUDIO_TEST_H
#define DXX_XL_SEISMIC_AUDIO_TEST_H
#include "named_audio_state.h"
#include "named_audio.h"
#include "shared_audio.h"
#include "save_epoch_format.h"
#include "seismic_gameplay.h"
#include "../seismic.h"
#include "../countdown.h"
#include "geometry.h"
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "game.h"
#include "gamemine.h"
#include "weapon.h"
#include "ai.h"
#include "switch.h"
#include "rng_shared.h"
extern int Seismic_sound_playing, Seismic_tremor_volume, Seismic_tremor_magnitude;
extern fix64 Next_seismic_sound_time;
void do_seismic_stuff(void);
}
namespace xl_seismic_audio_control
{
inline void check(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
// The source-verified model is the oracle; execute the real engine frame with
// the same SIM draws, then compare applied rotations, state and cadence.
inline void checked_frame()
{
	using namespace xl_compat;
	const auto before = seismic_gameplay_snapshot();
	SeismicState expected;
	expected.strength = before[0].get<int>();
	expected.pending = before[1].get<int>();
	if (!before[2].is_null()) {
		expected.start = GameTime64 + before[2][0].get<std::int64_t>();
		expected.end = GameTime64 + before[2][1].get<std::int64_t>();
	}
	for (unsigned i = 0; i < 4; ++i) {
		if (!before[3][i][0].is_null()) expected.detonations[i] = GameTime64 + before[3][i][0].get<std::int64_t>();
		expected.scales[i] = before[3][i][1].get<float>();
	}
	expected.magnitude = before[4].get<int>();
	expected.volume = Seismic_tremor_volume;
	expected.next_sound = Next_seismic_sound_time;
	expected.sound = !named_audio_snapshot().at("loop").empty();
	int phase = before[5].get<int>();
	const auto elapsed = std::int64_t(before[6].get<int>()) + std::int64_t(FrameTime) * 1000;
	const int fraction = int(elapsed % F1_0);
	const bool tick = countdown_tick(phase, elapsed / F1_0);
	unsigned sim = 0, fx = 0;
	check(d_rand_get_stream_state(D_RNG_SIM, &sim) && d_rand_get_stream_state(D_RNG_FX, &fx), "Cannot read seismic oracle RNG");
	const unsigned calls = d_rand_get_stream_call_count(D_RNG_SIM), effects = d_rand_get_stream_call_count(D_RNG_FX);
	const auto result = advance_seismic(expected, GameTime64, tick, [] { return d_rand(); }, [] { return d_rand_fx(); });
	const unsigned expected_calls = d_rand_get_stream_call_count(D_RNG_SIM);
	d_rand_set_stream_state(D_RNG_SIM, sim);
	d_rand_set_stream_call_count(D_RNG_SIM, calls);
	d_rand_set_stream_state(D_RNG_FX, fx);
	d_rand_set_stream_call_count(D_RNG_FX, effects);
	const auto player = ConsoleObject->mtype.phys_info.rotvel;
	const bool has_buddy = Buddy_objnum >= 0 && Buddy_objnum <= Highest_object_index && Objects[Buddy_objnum].type == OBJ_ROBOT;
	const auto buddy = has_buddy ? Objects[Buddy_objnum].mtype.phys_info.rotvel : vms_vector{};
	do_seismic_stuff();
	const auto actual = seismic_gameplay_snapshot();
	check(d_rand_get_stream_call_count(D_RNG_SIM) == expected_calls, "XL seismic frame consumed incorrect SIM draws");
	const auto rotation_matches = [&](const vms_vector &a, const vms_vector &b, unsigned scale) {
		return std::uint32_t(a.x) == std::uint32_t(b.x) + std::uint32_t(result.rotation_x) * scale && a.y == b.y && std::uint32_t(a.z) == std::uint32_t(b.z) + std::uint32_t(result.rotation_z) * scale;
	};
	check(rotation_matches(ConsoleObject->mtype.phys_info.rotvel, player, 1), "Actual XL player rocking differs from source-verified model");
	check(!has_buddy || rotation_matches(Objects[Buddy_objnum].mtype.phys_info.rotvel, buddy, 4), "Actual XL companion rocking differs from source-verified model");
	check(actual[0] == expected.strength && actual[1] == expected.pending && actual[4] == expected.magnitude && actual[5] == phase && actual[6] == fraction, "Actual XL seismic state/cadence differs from source-verified model");
	for (unsigned i = 0; i < 4; ++i) {
		check(actual[3][i][0] == (expected.detonations[i] ? nlohmann::json(expected.detonations[i] - GameTime64) : nlohmann::json(nullptr)) && actual[3][i][1] == expected.scales[i], "Actual Earthshaker slot or distance scale differs");
	}
}
template <class Output>
void run(Output output)
{
	using namespace xl_compat;
	const auto initial = named_audio_snapshot();
	{
		struct Guard {
			seismic_runtime_state physics{};
			nlohmann::json seismic = seismic_gameplay_snapshot();
			fix frame = FrameTime;
			vms_vector player_rotation = ConsoleObject->mtype.phys_info.rotvel;
			int original_buddy = Buddy_objnum, buddy = Buddy_objnum;
			bool created_buddy = false;
			vms_vector buddy_rotation = buddy >= 0 && buddy <= Highest_object_index ? Objects[buddy].mtype.phys_info.rotvel : vms_vector{};
			fix64 clock = GameTime64, next = Next_seismic_sound_time;
			fix frequency = Level_shake_frequency, duration = Level_shake_duration;
			int playing = Seismic_sound_playing, volume = Seismic_tremor_volume, tick = d_tick_step;
			unsigned sim = 0, fx = 0, sim_calls = d_rand_get_stream_call_count(D_RNG_SIM), fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
			Guard()
			{
				weapon_get_seismic_runtime_state(&physics);
				if (buddy < 0 || buddy > Highest_object_index || Objects[buddy].type != OBJ_ROBOT) {
					for (int i = 0; i <= Highest_object_index; ++i)
						if (Objects[i].type == OBJ_ROBOT) {
							buddy = i;
							break;
						}
				}
				if (buddy < 0 || buddy > Highest_object_index || Objects[buddy].type != OBJ_ROBOT) {
					buddy = obj_create(OBJ_ROBOT, 0, ConsoleObject->segnum, &ConsoleObject->pos, &ConsoleObject->orient, F1_0, CT_NONE, MT_PHYSICS, RT_NONE);
					created_buddy = buddy >= 0;
				}
				check(buddy >= 0 && buddy <= Highest_object_index && Objects[buddy].type == OBJ_ROBOT, "Seismic companion control requires a real robot object");
				buddy_rotation = Objects[buddy].mtype.phys_info.rotvel;
				Buddy_objnum = buddy;
				check(!game_is_time_paused(), "Seismic physics controls require running simulation");
				check(d_rand_get_stream_state(D_RNG_SIM, &sim) && d_rand_get_stream_state(D_RNG_FX, &fx), "Seismic controls require stateful RNG streams");
			}
			~Guard()
			{
				weapon_set_seismic_runtime_state(&physics);
				GameTime64 = clock;
				restore_seismic_gameplay(seismic, true);
				FrameTime = frame;
				Buddy_objnum = original_buddy;
				ConsoleObject->mtype.phys_info.rotvel = player_rotation;
				if (buddy >= 0 && buddy <= Highest_object_index) Objects[buddy].mtype.phys_info.rotvel = buddy_rotation;
				if (created_buddy) obj_delete(buddy);
				Next_seismic_sound_time = next;
				Level_shake_frequency = frequency;
				Level_shake_duration = duration;
				Seismic_sound_playing = playing;
				Seismic_tremor_volume = volume;
				d_tick_step = tick;
				d_rand_set_stream_state(D_RNG_SIM, sim);
				d_rand_set_stream_state(D_RNG_FX, fx);
				d_rand_set_stream_call_count(D_RNG_SIM, sim_calls);
				d_rand_set_stream_call_count(D_RNG_FX, fx_calls);
			}
		} guard;
		xl_named_audio_stop();
		weapon_reset_seismic_runtime_state();
		xl_seismic_clear();
		FrameTime = F1_0 / 32;
		GameTime64 += 10 * F1_0;
		Level_shake_duration = Level_shake_frequency = 0;
		d_tick_step = Seismic_sound_playing = Seismic_tremor_volume = 0;
		smega_rock_stuff();
		checked_frame();
		check(named_audio_snapshot().at("loop").size() == 2 && Seismic_tremor_volume > 0, "Actual Earthshaker event did not start an XL loop");
		output();
		const auto earned = named_audio_snapshot();
		const auto earned_deadline = Next_seismic_sound_time;
		const auto earned_volume = Seismic_tremor_volume;
		for (unsigned variant = 0; variant < 8; ++variant) {
			auto bad = earned;
			if (variant == 0) bad.erase("seismic");
			if (variant == 1) bad["seismic"] = nullptr;
			if (variant == 2) bad["seismic"] = { 0 };
			if (variant == 3) bad["seismic"][0] = 0.5;
			if (variant == 4) bad["seismic"][0] = UINT64_MAX;
			if (variant == 5) bad["seismic"][1] = -1;
			if (variant == 6) bad["seismic"][1] = 81;
			if (variant == 7) bad["seismic"][0] = SAVE_EPOCH_MAX;
			check(!restore_named_audio(bad, false) && !restore_named_audio(bad, true) && named_audio_snapshot() == earned, "Malformed seismic timing partially changed playback");
		}
		// Audio restoration cannot trust the unrelated legacy playback flag
		Seismic_sound_playing = 0;
		Next_seismic_sound_time += 10 * F1_0;
		Seismic_tremor_volume = 0;
		check(restore_named_audio(earned, true), "Cannot restore earned seismic output");
		check(Next_seismic_sound_time == earned_deadline && Seismic_tremor_volume == earned_volume, "Seismic restore lost its volume cadence");
		GameTime64 += F1_0;
		check(restore_named_audio(earned, true) && Next_seismic_sound_time == earned_deadline + F1_0 && named_audio_snapshot() == earned, "Seismic deadline did not rebase to the restored epoch");
		GameTime64 -= F1_0;
		check(restore_named_audio(earned, true), "Cannot recover seismic epoch control");
		checked_frame();
		check(named_audio_snapshot() == earned, "Restored Earthshaker event restarted its earned loop");
		output();
		GameTime64 += F1_0 / 2;
		checked_frame();
		const auto changed = named_audio_snapshot();
		bool gained = false;
		for (const auto &voice : changed.at("voices"))
			if (voice[0] == earned.at("loop")[0]) gained = voice[6] == 12288;
		check(gained && changed.at("loop") == earned.at("loop") && Next_seismic_sound_time >= GameTime64 + 8192 && Next_seismic_sound_time <= GameTime64 + 16383, "Earthshaker continuation lost its source gain or cadence");
		output();
		xl_named_audio_stop();
		Seismic_sound_playing = 1;
		checked_frame();
		check(named_audio_snapshot().at("loop").size() == 2 && named_audio_snapshot().at("loop") != earned.at("loop"), "Active Earthshaker event trusted a stale playing flag");
		output();
		GameTime64 += 3 * F1_0;
		checked_frame();
		check(named_audio_snapshot().at("loop").empty() && !Seismic_sound_playing, "Expired Earthshaker event retained its loop");
		const nlohmann::json disturbance = { F1_0, 0, { -F1_0, F1_0 }, { { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 }, { nullptr, 0.0 } }, 0, 0, 0 };
		check(restore_seismic_gameplay(disturbance, true), "Cannot prepare active XL disturbance");
		Level_shake_frequency = F1_0;
		Seismic_sound_playing = 1;
		Seismic_tremor_volume = 0;
		checked_frame();
		check(named_audio_snapshot().at("loop").size() == 2, "Restored ongoing level disturbance did not recover its loop");
		output();
		const auto old_loop = named_audio_snapshot().at("loop");
		xl_audio_stop_channel(old_loop[0].template get<int>());
		const int unrelated = xl_audio_start_sample(1, 2, F1_0, 32767, 1, -1, -1);
		check(unrelated >= 0, "Cannot prepare unrelated seismic replacement control");
		checked_frame();
		check(named_audio_snapshot().at("loop").size() == 2 && named_audio_snapshot().at("loop") != old_loop && xl_audio_channel_playing(unrelated), "Ongoing level disturbance failed to recover after channel retirement");
		output();
		GameTime64 += 3 * F1_0;
		checked_frame();
		check(named_audio_snapshot().at("loop").empty() && xl_audio_channel_playing(unrelated), "Expired level disturbance stopped an unrelated voice");
		// Exercise real spatial entry points, slot pressure and a sub-tick phase.
		xl_named_audio_stop();
		xl_seismic_clear();
		for (int distance : { 0, 200, 400, 2000, 1000 }) {
			auto position = ConsoleObject->pos;
			position.x += distance * F1_0;
			smega_rock_stuff_at(&position);
		}
		const auto slots = seismic_gameplay_snapshot();
		check(slots[3][0][1] == 1.0f && slots[3][1][1] == 1.0f && slots[3][2][1] == 0.5f && slots[3][3][1] == 0.1f, "Earthshaker position or full-slot drop differs");
		xl_seismic_clear();
		auto diagonal = ConsoleObject->pos;
		diagonal.x += 201 * F1_0 + 17;
		diagonal.y += 101 * F1_0 + 23;
		diagonal.z += 37 * F1_0 + 31;
		smega_rock_stuff_at(&diagonal);
		const float diagonal_distance = float(geometry_fixed_distance({ diagonal.x, diagonal.y, diagonal.z }, { ConsoleObject->pos.x, ConsoleObject->pos.y, ConsoleObject->pos.z })) / F1_0;
		check(seismic_gameplay_snapshot()[3][0][1] == 200.0f / diagonal_distance, "Oblique Earthshaker distance used Redux fixed math instead of source default math");
		FrameTime = F1_0 / 100;
		checked_frame();
		check(seismic_gameplay_snapshot()[5] == 9, "Seismic sub-tick did not retain its phase");
		FrameTime = F1_0 / 50;
		checked_frame();
		check(seismic_gameplay_snapshot()[5] == 0, "Seismic tick did not discard phase overshoot");
		FrameTime = F1_0 / 32;
		trigger action{};
		action.type = 36;
		for (int strength : { -F1_0, 0, F1_0, 3 * F1_0, INT32_MAX }) {
			xl_seismic_clear();
			action.value = strength;
			action.time = -1;
			check(xl_seismic_action(&action) && seismic_gameplay_snapshot()[1] == 10 * F1_0, "Negative mine-shake duration lost its ten-second default");
			checked_frame();
			if (strength > 0) check(seismic_gameplay_snapshot()[1] == 0, "Mine disturbance did not consume its pending duration once");
			GameTime64 += F1_0 / 32;
			checked_frame();
		}
		const auto high = seismic_gameplay_snapshot();
		check(std::abs(std::int64_t(Seismic_tremor_magnitude)) > F1_0, "High-strength save control did not exceed the legacy magnitude range");
		seismic_runtime_state legacy{};
		weapon_get_seismic_runtime_state(&legacy);
		check(legacy.tremor_magnitude == 0 && restore_seismic_gameplay(high, false), "XL wide seismic state leaked into the bounded Redux save record");
		action.time = 32768;
		check(!xl_seismic_action(&action) && seismic_gameplay_snapshot() == high, "Overflow seismic action changed state");
		for (unsigned variant = 0; variant < 14; ++variant) {
			auto bad = high;
			if (variant == 0) bad = nullptr;
			if (variant == 1) bad.erase(bad.begin());
			if (variant == 2) bad[0] = UINT64_MAX;
			if (variant == 3) bad[1] = UINT64_MAX;
			if (variant == 4) bad[2] = { 1, 2 };
			if (variant == 5) bad[2] = { 0, 0 };
			if (variant == 6) bad[3][0][0] = 1;
			if (variant == 7) bad[3][0][1] = -0.5;
			if (variant == 8) bad[3][0][1] = 1.5;
			if (variant == 9) bad[4] = UINT64_MAX;
			if (variant == 10) bad[5] = 25;
			if (variant == 11) bad[6] = F1_0;
			if (variant == 12) bad[1] = 0.5;
			if (variant == 13) bad[3][0] = { nullptr };
			check(!restore_seismic_gameplay(bad, false) && !restore_seismic_gameplay(bad, true) && seismic_gameplay_snapshot() == high, "Malformed seismic physics partially changed live state");
		}
		GameTime64 += 20000000000LL;
		check(restore_seismic_gameplay(high, true) && seismic_gameplay_snapshot() == high, "Seismic restore failed beyond the 32-bit epoch");
		checked_frame();
		check(d_rand_get_stream_call_count(D_RNG_SIM) > guard.sim_calls, "Actual XL rocking consumed no simulation draws");
	}
	check(restore_named_audio(initial, true), "Cannot restore state after seismic event controls");
}
} // namespace xl_seismic_audio_control
#endif
