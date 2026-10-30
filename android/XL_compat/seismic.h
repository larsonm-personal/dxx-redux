#ifndef DXX_XL_SEISMIC_H
#define DXX_XL_SEISMIC_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace xl_compat
{
constexpr int seismic_unit = 65536, seismic_lifetime = 2 * seismic_unit;
struct SeismicState {
	std::array<std::int64_t, 4> detonations{};
	std::array<float, 4> scales{};
	std::int64_t start = 0, end = 0, next_sound = 0;
	int strength = 0, pending = 0, magnitude = 0, volume = 0;
	bool sound = false;
};
inline bool seismic_action(SeismicState &state, int value, int seconds)
{
	if (seconds > 32767) return false;
	state.strength = (std::max) (value, 0);
	state.pending = (seconds < 0 ? 10 : seconds) * seismic_unit;
	return true;
}
inline void seismic_detonate(SeismicState &state, std::int64_t now, float distance, bool scaled = true)
{
	for (auto &time : state.detonations)
		if (time + seismic_lifetime < now) time = 0;
	const float scale = !scaled || distance <= 200.0f ? 1.0f : 200.0f / distance;
	for (unsigned i = 0; i < state.detonations.size(); ++i)
		if (!state.detonations[i]) {
			state.detonations[i] = now;
			state.scales[i] = scale;
			break;
		}
}
struct SeismicOutput {
	struct Sound {
		int operation = 0, volume = 0;
	};
	std::array<Sound, 3> sounds{};
	unsigned sound_count = 0;
	int rotation_x = 0, rotation_z = 0;
	bool updated = false;
};
template <class SimulationRandom, class AudioRandom>
SeismicOutput advance_seismic(SeismicState &state, std::int64_t now, bool tick, SimulationRandom simulation_random, AudioRandom audio_random)
{
	SeismicOutput output;
	if (!tick) return output;
	output.updated = true;
	const int previous_volume = state.volume;
	state.magnitude = state.volume = 0;
	const auto start_sound = [&] {
		if (state.sound) return;
		output.sounds[output.sound_count++] = { 1, seismic_unit };
		state.sound = true;
		state.next_sound = now + audio_random() / 2;
	};
	const auto rock = [&](std::int64_t authored, float scale) {
		const int strength = int((std::clamp) (authored, std::int64_t(1), std::int64_t(16)));
		state.volume += strength;
		float factor = float(3 * seismic_unit / 16 + (16 - strength) * seismic_unit / 32) / float(seismic_unit);
		if (scale > 0.0f) factor *= scale;
		const auto rotation = [&] {
			const float value = float(16383 - simulation_random()) * factor;
			return int(value < 0.0f ? value - 0.5f : value + 0.5f);
		};
		const int x = rotation(), z = rotation();
		output.rotation_x += x;
		output.rotation_z += z;
		state.magnitude += x;
	};
	for (unsigned i = 0; i < state.detonations.size(); ++i) {
		if (!state.detonations[i]) continue;
		const auto elapsed = now - state.detonations[i];
		start_sound();
		if (elapsed >= seismic_lifetime) {
			state.detonations[i] = 0;
			state.scales[i] = 0.0f;
		} else rock((seismic_lifetime - elapsed) / (seismic_lifetime / 16), state.scales[i]);
	}
	if (state.strength) {
		bool active = state.start < now && state.end > now;
		if (!active && state.pending > 0) {
			state.start = now;
			state.end = now + state.pending;
			state.pending = 0;
			start_sound();
			active = true;
		}
		if (active) rock(std::abs(now - state.start - (state.end - state.start) / 2), float(state.strength) / float(seismic_unit));
	}
	if (previous_volume) {
		if (!state.volume) {
			output.sounds[output.sound_count++] = { 2, 0 };
			state.sound = false;
		}
		if (now > state.next_sound && state.volume) {
			output.sounds[output.sound_count++] = { 3, (std::min) (seismic_unit, state.volume * 2048) };
			state.next_sound = now + audio_random() / 4 + 8192;
		}
	}
	return output;
}
} // namespace xl_compat
#endif
