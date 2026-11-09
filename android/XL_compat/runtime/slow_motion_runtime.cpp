#include <nlohmann/json.hpp>
#include "slow_motion_save.h"
#include "save_epoch_format.h"
#include "slow_motion_runtime.h"
#include "preview.h"
extern "C" {
#include "inferno.h"
#include "object.h"
#include "game.h"
#include "vecmat.h"
#include "aistruct.h"
}
namespace
{
xl_compat::SlowMotionState state;
int64_t last_missile;
uint32_t render_clock, last_omega_frame;
bool enabled()
{
	return xl_level_extensions_active() && !(Game_mode & GM_MULTI);
}
} // namespace
namespace xl_runtime
{
xl_compat::SlowMotionOutput slow_motion_step(xl_compat::SlowMotionInput input, const xl_compat::SlowMotionSink &sink)
{
	input.xl = xl_level_extensions_active();
	input.multiplayer = (Game_mode & GM_MULTI) != 0;
	return xl_compat::slow_motion_frame(state, input, sink);
}
const xl_compat::SlowMotionState &slow_motion_state()
{
	return state;
}
} // namespace xl_runtime
extern "C" void xl_slow_motion_reset(void)
{
	state = {};
	last_missile = 0;
	// The reference loader resets the render counter, not the Omega marker
	render_clock = 0;
}
extern "C" void xl_slow_motion_offset(const object *actor, vms_vector *offset)
{
	if (!enabled()) return;
	const float speed = state.clocks[actor == ConsoleObject ? 1 : 0].speed;
	if (speed == 1) return;
	// Reference UpdateOffset scales the computed vector, after fluid displacement
	const auto scale = static_cast<fix>(65536 / speed);
	offset->x = static_cast<fix>(double(offset->x) * scale / 65536.0);
	offset->y = static_cast<fix>(double(offset->y) * scale / 65536.0);
	offset->z = static_cast<fix>(double(offset->z) * scale / 65536.0);
}
extern "C" int32_t xl_slow_motion_lifetime(const object *actor, int32_t time)
{
	// Redux markers use lifeleft as their flashing clock, unlike the XL renderer
	if (!enabled() || actor->type == OBJ_MARKER) return time;
	const auto remaining = actor->lifeleft;
	if (remaining == 0x3ffffffe || remaining == IMMORTAL_TIME || time <= 0) return 0;
	if (state.clocks[0].speed == 1) return time;
	return static_cast<int32_t>(time / state.clocks[0].speed);
}

extern "C" void xl_slow_motion_rotation(const object *actor, vms_angvec *angles)
{
	if (!enabled()) return;
	const float speed = state.clocks[actor == ConsoleObject ? 1 : 0].speed;
	if (speed == 1) return;
	// DoPhysicsSimRot divides already wrapped angles, not thrust or frame time
	angles->p = static_cast<fixang>(angles->p / speed);
	angles->h = static_cast<fixang>(angles->h / speed);
	angles->b = static_cast<fixang>(angles->b / speed);
}

extern "C" void xl_slow_motion_turn_delta(const object *actor, int32_t *pitch, int32_t *heading)
{
	if (!enabled()) return;
	const float speed = state.clocks[actor == ConsoleObject ? 1 : 0].speed;
	if (speed == 1) return;
	// Reference TurnTowardsVector scales before sign-change saturation
	*pitch = static_cast<fix>(*pitch / speed);
	*heading = static_cast<fix>(*heading / speed);
}

extern "C" int32_t xl_slow_motion_world_time(int32_t time)
{
	if (!enabled() || state.clocks[0].speed == 1) return time;
	return static_cast<int32_t>(time / state.clocks[0].speed);
}
extern "C" int xl_slow_motion_ai_timers(ai_local *local, int secondary, int32_t time)
{
	if (!enabled()) return 0;
	const float speed = state.clocks[0].speed;
	if (local->next_fire > -8 * F1_0 * speed) local->next_fire -= xl_slow_motion_world_time(time);
	if (secondary != -1) {
		if (local->next_fire2 > -8 * F1_0 * speed) local->next_fire2 -= xl_slow_motion_world_time(time);
	} else local->next_fire2 = 8 * F1_0;
	// Scheduling is intentionally not on the world clock
	if (local->time_since_processed < 256 * F1_0) local->time_since_processed += time;
	return 1;
}

static int firing_blocked(int64_t last, int64_t next, int64_t now, int seconds)
{
	if (!enabled()) return next > now;
	const float speed = state.clocks[0].speed;
	int64_t deadline = next;
	if (speed > 1) {
		// Relative fixed-point intervals retain precision on Redux's 64-bit clock
		const bool positive = next >= last;
		const uint64_t magnitude = positive ? uint64_t(next) - uint64_t(last) : uint64_t(last) - uint64_t(next);
		if (magnitude > uint64_t(INT32_MAX)) return 1;
		const int64_t interval = positive ? int64_t(magnitude) : -int64_t(magnitude);
		const float scaled = float(interval) * speed;
		// The reference casts to fix; reject values outside that defined domain
		if (double(scaled) > INT32_MAX || double(scaled) < INT32_MIN) return 1;
		const auto delay = static_cast<int32_t>(scaled);
		if ((delay > 0 && last > INT64_MAX - delay) || (delay < 0 && last < INT64_MIN - delay)) return 1;
		deadline = last + delay;
	}
	// Compare remaining duration, not float absolute timestamps
	return deadline > now && uint64_t(deadline) - uint64_t(now) < double(seconds * F1_0 * speed);
}
extern "C" int xl_slow_motion_flare_blocked(int64_t next, int64_t now)
{
	if (!enabled()) return next > now;
	return next > now && uint64_t(next) - uint64_t(now) < 2 * F1_0;
}
extern "C" int32_t xl_slow_motion_flare_delay(int enough_energy)
{
	const float speed = enabled() ? state.clocks[0].speed : 1;
	// F2X then integer division for sufficient energy; multiply for empty energy
	return enough_energy ? int32_t(speed * F1_0) / 4 : int32_t(speed * (2 * F1_0));
}

extern "C" int xl_slow_motion_primary_blocked(int64_t last, int64_t next, int64_t now)
{
	return firing_blocked(last, next, now, 2);
}
extern "C" int xl_slow_motion_missile_blocked(int64_t next, int64_t now)
{
	return firing_blocked(last_missile, next, now, 5);
}
extern "C" void xl_slow_motion_missile_fired(void)
{
	if (enabled()) last_missile = GameTime64;
}
nlohmann::json xl_runtime::weapon_clock_snapshot()
{
	return nlohmann::json{ last_missile - GameTime64, render_clock, last_omega_frame };
}
bool xl_runtime::restore_weapon_clock(const nlohmann::json &saved, bool apply)
{
	try {
		if (!save_epoch_valid(GameTime64) || !saved.is_array() || saved.size() != 3) return false;
		const auto integer = [](const nlohmann::json &value, int64_t low, int64_t high) {
			if (!value.is_number_integer() || (value.is_number_unsigned() && value.get<uint64_t>() > uint64_t(high))) throw std::runtime_error("Invalid weapon clock integer");
			const auto result = value.get<int64_t>();
			if (result < low || result > high) throw std::runtime_error("Weapon clock outside bounds");
			return result;
		};
		const auto missile = GameTime64 + integer(saved[0], -SAVE_EPOCH_MAX, 0);
		const auto rendered = uint32_t(integer(saved[1], 0, UINT32_MAX));
		const auto omega = uint32_t(integer(saved[2], 0, UINT32_MAX));
		if (!save_timestamp_valid(missile)) return false;
		if (apply) {
			last_missile = missile;
			render_clock = rendered;
			last_omega_frame = omega;
		}
		return true;
	} catch (const std::exception &) {
		return false;
	}
}

extern "C" void xl_slow_motion_render_advance(void)
{
	if (enabled()) ++render_clock;
}
extern "C" uint32_t xl_slow_motion_render_clock(void)
{
	return render_clock;
}

extern "C" int xl_slow_motion_omega_blocked(void)
{
	if (!enabled()) return -1;
	return last_omega_frame == render_clock || last_omega_frame == render_clock - 1;
}
extern "C" int xl_slow_motion_omega_fired(void)
{
	if (!enabled()) return 0;
	last_omega_frame = render_clock;
	return 1;
}
