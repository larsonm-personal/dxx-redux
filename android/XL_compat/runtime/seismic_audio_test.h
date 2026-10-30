#ifndef DXX_XL_SEISMIC_AUDIO_TEST_H
#define DXX_XL_SEISMIC_AUDIO_TEST_H
#include "named_audio_state.h"
#include "named_audio.h"
#include "shared_audio.h"
#include "save_epoch_format.h"
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "game.h"
#include "gamemine.h"
#include "weapon.h"
#include "rng_shared.h"
extern int Seismic_sound_playing, Seismic_tremor_volume;
extern fix64 Next_seismic_sound_time;
void do_seismic_stuff(void);
}
namespace xl_seismic_audio_control
{
inline void check(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
template <class Output>
void run(Output output)
{
	using namespace xl_compat;
	const auto initial = named_audio_snapshot();
	{
		struct Guard {
			seismic_runtime_state physics{};
			fix64 clock = GameTime64, next = Next_seismic_sound_time;
			fix frequency = Level_shake_frequency, duration = Level_shake_duration;
			int playing = Seismic_sound_playing, volume = Seismic_tremor_volume, tick = d_tick_step;
			unsigned sim = 0, fx = 0, sim_calls = d_rand_get_stream_call_count(D_RNG_SIM), fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
			Guard()
			{
				weapon_get_seismic_runtime_state(&physics);
				check(d_rand_get_stream_state(D_RNG_SIM, &sim) && d_rand_get_stream_state(D_RNG_FX, &fx), "Seismic controls require stateful RNG streams");
			}
			~Guard()
			{
				weapon_set_seismic_runtime_state(&physics);
				GameTime64 = clock;
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
		GameTime64 += 10 * F1_0;
		Level_shake_duration = Level_shake_frequency = 0;
		d_tick_step = Seismic_sound_playing = Seismic_tremor_volume = 0;
		smega_rock_stuff();
		do_seismic_stuff();
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
		do_seismic_stuff();
		check(named_audio_snapshot() == earned, "Restored Earthshaker event restarted its earned loop");
		output();
		GameTime64 += F1_0 / 2;
		do_seismic_stuff();
		const auto changed = named_audio_snapshot();
		bool gained = false;
		for (const auto &voice : changed.at("voices"))
			if (voice[0] == earned.at("loop")[0]) gained = voice[6] == 12288;
		check(gained && changed.at("loop") == earned.at("loop") && Next_seismic_sound_time >= GameTime64 + 8192 && Next_seismic_sound_time <= GameTime64 + 16383, "Earthshaker continuation lost its source gain or cadence");
		output();
		xl_named_audio_stop();
		Seismic_sound_playing = 1;
		do_seismic_stuff();
		check(named_audio_snapshot().at("loop").size() == 2 && named_audio_snapshot().at("loop") != earned.at("loop"), "Active Earthshaker event trusted a stale playing flag");
		output();
		GameTime64 += 3 * F1_0;
		do_seismic_stuff();
		check(named_audio_snapshot().at("loop").empty() && !Seismic_sound_playing, "Expired Earthshaker event retained its loop");
		seismic_runtime_state disturbance{};
		disturbance.disturbance_start_time = GameTime64 - F1_0;
		disturbance.disturbance_end_time = GameTime64 + F1_0;
		weapon_set_seismic_runtime_state(&disturbance);
		Level_shake_frequency = F1_0;
		Seismic_sound_playing = 1;
		Seismic_tremor_volume = 0;
		do_seismic_stuff();
		check(named_audio_snapshot().at("loop").size() == 2, "Restored ongoing level disturbance did not recover its loop");
		output();
		const auto old_loop = named_audio_snapshot().at("loop");
		xl_audio_stop_channel(old_loop[0].template get<int>());
		const int unrelated = xl_audio_start_sample(1, 2, F1_0, 32767, 1, -1, -1);
		check(unrelated >= 0, "Cannot prepare unrelated seismic replacement control");
		do_seismic_stuff();
		check(named_audio_snapshot().at("loop").size() == 2 && named_audio_snapshot().at("loop") != old_loop && xl_audio_channel_playing(unrelated), "Ongoing level disturbance failed to recover after channel retirement");
		output();
		GameTime64 += 3 * F1_0;
		do_seismic_stuff();
		check(named_audio_snapshot().at("loop").empty() && xl_audio_channel_playing(unrelated), "Expired level disturbance stopped an unrelated voice");
		check(d_rand_get_stream_call_count(D_RNG_SIM) == guard.sim_calls, "Controlled sound recovery changed simulation RNG");
	}
	check(restore_named_audio(initial, true), "Cannot restore state after seismic event controls");
}
} // namespace xl_seismic_audio_control
#endif
