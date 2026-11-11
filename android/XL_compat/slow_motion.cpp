#include "slow_motion.h"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace xl_compat
{
namespace
{
class Frame
{
	SlowMotionState &s;
	const SlowMotionInput &in;
	const SlowMotionSink &sink;

  public:
	SlowMotionOutput out;
	Frame(SlowMotionState &state, const SlowMotionInput &input, const SlowMotionSink &effects) : s(state), in(input), sink(effects) {}
	bool emit(SlowMotionEvent event)
	{
		out.events.push_back(event);
		return sink ? sink(event) : false;
	}
	bool slow()
	{
		auto &clock = s.clocks[0];
		return clock.active = clock.direction > 0 || clock.speed > 1;
	}
	bool bullet()
	{
		return s.clocks[1].active = slow() && (s.clocks[1].direction < 0 || s.clocks[1].speed == 1);
	}
	void set(int index, int direction)
	{
		auto &clock = s.clocks[index];
		clock.direction = direction;
		if (clock.direction) {
			clock.direction = -clock.direction;
			if (s.transition_sound) {
				emit(SlowMotionEvent::stop_transition_sound);
				s.transition_sound = false;
			}
		} else clock.direction = clock.speed > 1 ? -1 : 1;
		clock.updated_ms = in.now_ms;
	}
	void message()
	{
		const auto &world = s.clocks[0];
		const auto &player = s.clocks[1];
		const bool speeding = world.direction <= 0 &&
		                      (world.direction < 0 || (world.direction == 0 && world.speed == 1) ||
		                       player.direction < 0 || (player.direction == 0 && player.speed == 1));
		if (in.mixer) {
			s.transition_sound = emit(speeding ? SlowMotionEvent::play_speedup : SlowMotionEvent::play_slowdown);
		}
		emit(speeding ? SlowMotionEvent::speeding_message : SlowMotionEvent::slowing_message);
	}
	bool toggle()
	{
		if (in.reactor_destroyed) return false;
		const bool allowed = in.cheat || (in.energy > 10 * 65536 && (in.equipment & slow_motion_equipment));
		// Upstream's OR mask also accepts slow-motion equipment alone for bullet time
		const bool bullet_allowed = allowed && (in.cheat || (in.equipment & (slow_motion_equipment | bullet_time_equipment)));
		const bool slow_request = allowed && in.slow_pressed;
		bool bullet_request = bullet_allowed && in.bullet_pressed;
		out.consumed_controls = true;
		if (slow()) {
			if (!in.cheat) out.energy_delta = -((4 + in.difficulty) * in.frame * (1 + bullet())) / 6;
			if (!allowed) {
				if (s.clocks[0].direction != -1) {
					set(0, 1);
					if (!bullet() && s.clocks[1].direction != -1) set(1, 1);
					message();
				}
				return false;
			}
			if (!bullet_allowed) {
				if (s.clocks[1].direction != -1) set(1, -1);
				bullet_request = false;
			}
		}
		if (!(slow_request || bullet_request)) return false;
		if (bullet_request) {
			if (slow()) {
				if (bullet()) set(0, 1);
				set(1, 1);
			} else {
				set(0, -1);
				if (!bullet()) set(1, 1);
			}
		} else if (slow()) {
			if (bullet()) set(1, -1);
			else {
				set(0, 1);
				if (!bullet()) set(1, 1);
			}
		} else {
			set(0, -1);
			if (bullet()) set(1, -1);
		}
		return true;
	}
	void advance()
	{
		if (in.force_off) {
			// InitCountdown calls SlowMotionOff separately from the normal frame
			if (slow() && s.clocks[0].direction != -1) {
				set(0, 1);
				if (!bullet()) set(1, 1);
				message();
			}
			return;
		}
		const bool notify = toggle();
		for (std::size_t i = 0; i < s.clocks.size(); ++i) {
			auto &clock = s.clocks[i];
			// One step only, strictly after 25 ms; do not catch up missed frames
			if (!clock.direction || in.now_ms - clock.updated_ms <= 25) continue;
			clock.speed += clock.direction * (2.0f / 80);
			if (clock.speed >= 3) {
				clock.speed = 3;
				clock.direction = 0;
				if (!i) emit(SlowMotionEvent::slow_audio);
			} else if (clock.speed <= 1) {
				clock.speed = 1;
				clock.direction = 0;
				if (!i) emit(SlowMotionEvent::normal_audio);
			}
			clock.updated_ms = in.now_ms;
		}
		if (notify) message();
	}
};
} // namespace
SlowMotionOutput slow_motion_frame(SlowMotionState &state, const SlowMotionInput &input, const SlowMotionSink &sink)
{
	if (!input.xl || input.multiplayer || input.nostalgia) return {};
	// Native frames are uncapped, including stalls after loading or first rendering
	// Bound the source's largest (difficulty + 4) * frame * 2 energy expression
	if (input.now_ms < 0 || input.frame < 0 || input.frame > (std::numeric_limits<std::int32_t>::max)() / 16 || input.difficulty < 0 || input.difficulty > 4 ||
	    input.energy < -1 || input.energy > 200 * 65536)
		throw std::invalid_argument("Unqualified slow-motion input");
	for (const auto &clock : state.clocks)
		if (!std::isfinite(clock.speed) || clock.speed < 1 || clock.speed > 3 || clock.direction < -1 || clock.direction > 1 ||
		    clock.updated_ms < 0 || clock.updated_ms > input.now_ms)
			throw std::invalid_argument("Unqualified slow-motion clock");
	Frame frame(state, input, sink);
	frame.advance();
	return frame.out;
}
} // namespace xl_compat
