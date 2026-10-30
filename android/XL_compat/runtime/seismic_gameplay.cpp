#include "seismic_gameplay.h"
#include "../seismic.h"
#include "../countdown.h"
#include "gameplay_audio.h"
#include "preview.h"
#include "save_epoch_format.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "game.h"
#include "gamemine.h"
#include "switch.h"
#include "digi.h"
#include "ai.h"
#include "rng_shared.h"
extern int Seismic_sound, Seismic_sound_playing, Seismic_tremor_volume, Seismic_tremor_magnitude;
extern fix64 Next_seismic_sound_time;
}
namespace
{
xl_compat::SeismicState current;
int phase = 0, fraction = 0;
void add_rotation(fix &target, int delta)
{
	const auto bits = std::uint32_t(target) + std::uint32_t(delta);
	std::memcpy(&target, &bits, sizeof(target));
}
} // namespace
extern "C" void xl_seismic_clear(void)
{
	current = {};
	phase = fraction = 0;
}
extern "C" void xl_seismic_bind(void)
{
	xl_seismic_clear();
	current.strength = Level_shake_frequency;
	current.pending = Level_shake_duration;
	Seismic_tremor_magnitude = 0;
}
extern "C" int xl_seismic_owned(void)
{
	return xl_level_extensions_active();
}
extern "C" int xl_seismic_action_valid(const trigger *action)
{
	return action && action->type == 36 && action->time <= 32767;
}
extern "C" int xl_seismic_action(const trigger *action)
{
	return xl_level_extensions_active() && xl_seismic_action_valid(action) && xl_compat::seismic_action(current, action->value, action->time);
}
extern "C" int xl_seismic_detonation(const vms_vector *position)
{
	if (!xl_level_extensions_active()) return 0;
	const float distance = position && ConsoleObject ? float(vm_vec_dist(position, &ConsoleObject->pos)) / F1_0 : 0.0f;
	xl_compat::seismic_detonate(current, GameTime64, distance, position != nullptr);
	return 1;
}
extern "C" int xl_seismic_frame(void)
{
	if (!xl_level_extensions_active()) return 0;
	if (game_is_time_paused()) return 1;
	// Use the existing saved simulation-clock policy, with source 25 ms cadence
	const auto elapsed = std::int64_t(fraction) + std::int64_t((std::max) (FrameTime, 0)) * 1000;
	fraction = int(elapsed % F1_0);
	if (!xl_compat::countdown_tick(phase, elapsed / F1_0)) return 1;
	xl_gameplay_seismic_begin();
	current.sound = Seismic_sound_playing != 0;
	current.volume = Seismic_tremor_volume;
	current.next_sound = Next_seismic_sound_time;
	const auto output = xl_compat::advance_seismic(current, GameTime64, true, [] { return d_rand(); }, [] { return d_rand_fx(); });
	Seismic_sound_playing = current.sound;
	Seismic_tremor_volume = current.volume;
	Seismic_tremor_magnitude = current.magnitude;
	Next_seismic_sound_time = current.next_sound;
	if (ConsoleObject) {
		add_rotation(ConsoleObject->mtype.phys_info.rotvel.x, output.rotation_x);
		add_rotation(ConsoleObject->mtype.phys_info.rotvel.z, output.rotation_z);
	}
	if (Buddy_objnum >= 0 && Buddy_objnum <= Highest_object_index && Objects[Buddy_objnum].type == OBJ_ROBOT) {
		add_rotation(Objects[Buddy_objnum].mtype.phys_info.rotvel.x, output.rotation_x * 4);
		add_rotation(Objects[Buddy_objnum].mtype.phys_info.rotvel.z, output.rotation_z * 4);
	}
	for (unsigned i = 0; i < output.sound_count; ++i) {
		const auto &sound = output.sounds[i];
		if (sound.operation == 1) digi_play_sample_looping(Seismic_sound, sound.volume, -1, -1);
		else if (sound.operation == 2) digi_stop_looping_sound();
		else digi_change_looping_volume(sound.volume);
	}
	xl_gameplay_seismic_end();
	return 1;
}
nlohmann::json xl_compat::seismic_gameplay_snapshot()
{
	using nlohmann::json;
	json detonations = json::array();
	for (unsigned i = 0; i < current.detonations.size(); ++i)
		detonations.push_back({ current.detonations[i] ? json(current.detonations[i] - GameTime64) : json(nullptr), current.scales[i] });
	const json interval = current.start || current.end ? json{ current.start - GameTime64, current.end - GameTime64 } : json(nullptr);
	return json{ current.strength, current.pending, interval, detonations, Seismic_tremor_magnitude, phase, fraction };
}
bool xl_compat::restore_seismic_gameplay(const nlohmann::json &state, bool apply)
{
	try {
		if (!save_epoch_valid(GameTime64) || !state.is_array() || state.size() != 7) return false;
		const auto integer = [](const nlohmann::json &value, std::int64_t low, std::int64_t high) {
			if (!value.is_number_integer() || (value.is_number_unsigned() && value.get<std::uint64_t>() > std::uint64_t(high))) throw std::runtime_error("Invalid seismic integer");
			const auto result = value.get<std::int64_t>();
			if (result < low || result > high) throw std::runtime_error("Seismic integer outside bounds");
			return result;
		};
		auto prepared = current;
		prepared.strength = int(integer(state[0], INT32_MIN, INT32_MAX));
		prepared.pending = int(integer(state[1], INT32_MIN, 32767 * F1_0));
		prepared.magnitude = int(integer(state[4], INT32_MIN, INT32_MAX));
		const int saved_phase = int(integer(state[5], 0, 24)), saved_fraction = int(integer(state[6], 0, F1_0 - 1));
		prepared.start = prepared.end = 0;
		if (!state[2].is_null()) {
			if (!state[2].is_array() || state[2].size() != 2) return false;
			prepared.start = GameTime64 + integer(state[2][0], -SAVE_EPOCH_MAX, 0);
			prepared.end = GameTime64 + integer(state[2][1], -SAVE_EPOCH_MAX, INT32_MAX);
			if (!save_timestamp_valid(prepared.start) || !save_timestamp_valid(prepared.end) || prepared.end <= prepared.start || prepared.end - prepared.start > INT32_MAX) return false;
		}
		if (!state[3].is_array() || state[3].size() != 4) return false;
		for (unsigned i = 0; i < 4; ++i) {
			const auto &row = state[3][i];
			if (!row.is_array() || row.size() != 2 || !row[1].is_number()) return false;
			prepared.detonations[i] = 0;
			prepared.scales[i] = row[1].get<float>();
			if (!std::isfinite(prepared.scales[i]) || prepared.scales[i] < 0.0f || prepared.scales[i] > 1.0f) return false;
			if (!row[0].is_null()) {
				prepared.detonations[i] = GameTime64 + integer(row[0], -INT32_MAX, 0);
				if (!save_timestamp_valid(prepared.detonations[i])) return false;
				if (!prepared.detonations[i]) prepared.detonations[i] = -1;
			}
		}
		if (apply) {
			current = prepared;
			phase = saved_phase;
			fraction = saved_fraction;
			Seismic_tremor_magnitude = current.magnitude;
		}
		return true;
	} catch (const std::exception &) {
		return false;
	}
}
