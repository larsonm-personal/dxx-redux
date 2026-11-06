#ifndef XL_SLOW_MOTION_H
#define XL_SLOW_MOTION_H
#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace xl_compat
{
// Pinned default speedup=6 profile; no pickup or engine-clock admission
constexpr std::uint32_t slow_motion_equipment = 32768, bullet_time_equipment = 65536;
struct SlowMotionClock {
	float speed = 1;
	int direction = 0;
	std::int64_t updated_ms = 0;
	bool active = false;
};
struct SlowMotionState {
	std::array<SlowMotionClock, 2> clocks{}; // World, then console player
	bool transition_sound = false;
};
enum class SlowMotionEvent { stop_transition_sound,
	                         slowing_message,
	                         speeding_message,
	                         play_slowdown,
	                         play_speedup,
	                         slow_audio,
	                         normal_audio };
struct SlowMotionInput {
	bool xl = false, multiplayer = false, nostalgia = false, reactor_destroyed = false;
	bool cheat = false, mixer = false;
	std::uint32_t equipment = 0;
	std::int32_t energy = 0, frame = 0;
	int difficulty = 0;
	std::int64_t now_ms = 0;
	bool slow_pressed = false, bullet_pressed = false;
};
struct SlowMotionOutput {
	bool consumed_controls = false;
	std::int32_t energy_delta = 0;       // Apply through the qualified player resource updater
	std::vector<SlowMotionEvent> events; // Audit of delivered requests; do not dispatch again
};
// The sink runs in source order; only play events use its actual success result
// An absent sink means playback unavailable. Adapters must not throw or reenter
using SlowMotionSink = std::function<bool(SlowMotionEvent)>;
// Input/state are validated before mutation. Disabled classic/network paths are inert
SlowMotionOutput slow_motion_frame(SlowMotionState &state, const SlowMotionInput &input, const SlowMotionSink &sink = {});
} // namespace xl_compat
#endif
