#ifndef XL_SLOW_MOTION_RUNTIME_H
#define XL_SLOW_MOTION_RUNTIME_H
#include <stdint.h>
struct object;
struct vms_vector;
struct vms_angvec;
struct ai_local;
#ifdef __cplusplus
extern "C" {
#endif
void xl_slow_motion_reset(void);
const char *xl_omega_input_demo_error(void);
void xl_slow_motion_render_advance(void);
uint32_t xl_slow_motion_render_clock(void);
int xl_slow_motion_omega_blocked(void);
int xl_slow_motion_omega_fired(void);
int xl_slow_motion_primary_blocked(int64_t last, int64_t next, int64_t now);
int xl_slow_motion_flare_blocked(int64_t next, int64_t now);
int32_t xl_slow_motion_flare_delay(int enough_energy);
int xl_slow_motion_missile_blocked(int64_t next, int64_t now);
void xl_slow_motion_missile_fired(void);
int32_t xl_slow_motion_world_time(int32_t time);
int xl_slow_motion_ai_timers(struct ai_local *local, int secondary, int32_t time);
void xl_slow_motion_turn_delta(const struct object *actor, int32_t *pitch, int32_t *heading);
void xl_slow_motion_rotation(const struct object *actor, struct vms_angvec *angles);
void xl_slow_motion_offset(const struct object *actor, struct vms_vector *offset);
int32_t xl_slow_motion_lifetime(const struct object *actor, int32_t time);
#ifdef __cplusplus
}
#include "../slow_motion.h"
namespace xl_runtime
{
// Integration seam only; no gameplay input or pickup enables it yet
xl_compat::SlowMotionOutput slow_motion_step(xl_compat::SlowMotionInput input, const xl_compat::SlowMotionSink &sink = {});
const xl_compat::SlowMotionState &slow_motion_state();
} // namespace xl_runtime
#endif
#endif
