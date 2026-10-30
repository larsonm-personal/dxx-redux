#ifndef DXX_XL_COUNTDOWN_H
#define DXX_XL_COUNTDOWN_H
#include <algorithm>
#include <array>
#include <cstdint>

namespace xl_compat
{
constexpr int countdown_unit = 65536;
struct Countdown {
	int destroyed = 0, timer = 0, seconds = 0, total = 0, pending = 0;
};
inline int countdown_total(int authored, int base, int difficulty, bool d1 = false)
{
	if (difficulty < 0 || difficulty >= 5) return -1;
	static constexpr int defaults[2][5] = { { 90, 60, 45, 35, 30 }, { 50, 45, 40, 35, 30 } };
	const std::int64_t total = authored > 0 ? authored : base != 30 ? std::int64_t(base) + std::int64_t(base) * (4 - difficulty) / 2
	                                                                : defaults[d1][difficulty];
	return total >= 0 && total <= 32767 ? int(total) : -1;
}
inline bool initialize_countdown(Countdown &state, int authored, int base, int difficulty, int timer = -1, bool destroyed = true, bool d1 = false)
{
	const int total = countdown_total(authored, base, difficulty, d1);
	if (total < 0) return false;
	state.total = total;
	state.timer = timer < 0 ? total * countdown_unit : timer ? timer
	                                                         : countdown_unit;
	if (destroyed) state.destroyed = 1;
	// InitCountdown retains displayed seconds and the pending frame accumulator
	return true;
}
struct CountdownInput {
	int frame = 0, difficulty = 0;
	bool tick = true, limited = true, paused = false, dead = false, builtin_last = false;
};
inline bool countdown_tick(int &phase, std::int64_t elapsed_ms)
{
	phase = int((std::min) (std::int64_t(25), std::int64_t(phase) + (std::max) (std::int64_t(0), elapsed_ms)));
	if (phase < 25) return false;
	phase = 0;
	return true;
}
struct CountdownOutput {
	struct Sound {
		int id, volume;
	};
	std::array<Sound, 4> sounds{};
	unsigned sound_count = 0;
	int rotation_x = 0, rotation_z = 0, flash = -1;
	bool death = false;
	void sound(int id, int volume = 3 * countdown_unit)
	{
		sounds[sound_count++] = { id, volume };
	}
};
template <typename Random>
CountdownOutput advance_countdown(Countdown &state, const CountdownInput &input, Random random)
{
	CountdownOutput output;
	if (!state.destroyed || input.dead) {
		state.pending = 0;
		return output;
	}
	state.pending = int((std::min) (std::int64_t(INT32_MAX), std::int64_t(state.pending) + (std::max) (input.frame, 0)));
	if ((input.limited && !input.tick) || input.builtin_last) return output;
	const int seconds = (std::min) (state.seconds, 16);
	const int scale = input.difficulty ? 1 : 4;
	const int magnitude = 3 * countdown_unit / 16 + (16 - seconds) * countdown_unit / 32;
	output.rotation_x = int((std::int64_t(random()) * magnitude) >> 16) / scale;
	output.rotation_z = int((std::int64_t(random()) * magnitude) >> 16) / scale;
	const int previous = state.timer;
	if (!input.paused) state.timer = int((std::max) (std::int64_t(INT32_MIN), std::int64_t(state.timer) - state.pending));
	state.pending = 0;
	const auto rounded_seconds = [](int time) { return int((std::int64_t(time) + 7 * countdown_unit / 8) >> 16); };
	state.seconds = rounded_seconds(state.timer);
	// These IDs are checked against the pinned source sound constants
	if (previous > 51 * countdown_unit / 4 && state.timer <= 51 * countdown_unit / 4) output.sound(113);
	if (rounded_seconds(previous) != state.seconds) {
		if (state.seconds >= 0 && state.seconds < 10) output.sound(100 + state.seconds);
		if (state.seconds == state.total - 1) output.sound(114);
	}
	if (state.timer > 0) {
		const auto elapsed = [&](int time) { return (std::int64_t(state.total) * countdown_unit - time) / 42598; };
		if (elapsed(state.timer) != elapsed(previous) && state.seconds < state.total - 5) output.sound(32);
	} else {
		if (previous > 0) output.sound(33, countdown_unit);
		output.flash = int((std::min) (std::int64_t(64), (-std::int64_t(state.timer) * 16) >> 16));
		output.death = output.flash >= 64;
	}
	return output;
}
} // namespace xl_compat
#endif
