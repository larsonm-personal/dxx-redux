#include "slow_motion_runtime.h"
#include "preview.h"
extern "C" {
#include "inferno.h"
#include "object.h"
#include "game.h"
#include "vecmat.h"
}
namespace
{
xl_compat::SlowMotionState state;
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
