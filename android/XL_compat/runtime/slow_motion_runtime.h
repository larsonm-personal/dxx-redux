#ifndef XL_SLOW_MOTION_RUNTIME_H
#define XL_SLOW_MOTION_RUNTIME_H
#include <stdint.h>
#include "input_demo_dispatch.h"
struct object;
struct vms_vector;
struct vms_angvec;
struct ai_local;
// Keep these equipment bits synchronized with XL_compat/slow_motion.h
#define XL_PLAYER_FLAGS_SLOW_MOTION 32768u
#define XL_PLAYER_FLAGS_BULLET_TIME 65536u
#ifdef __cplusplus
extern "C" {
#endif
void xl_slow_motion_reset(void);
int xl_slow_motion_request(int bullet_time);
void xl_slow_motion_frame(void);
void xl_slow_motion_off(void);
int xl_time_equipment_initialize(struct object *actor, int spawned);
int xl_time_equipment_animate(struct object *actor);
int xl_time_equipment_draw(struct object *actor);
uint32_t xl_time_equipment_restore_hint(uint32_t equipment);
const char *xl_omega_input_demo_error(void);
void xl_input_demo_observe(const char *consumer, int subject, input_demo_environment_clock *clock);
int xl_input_demo_saved_environment(input_demo_environment_clock *clock);
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
namespace xl_compat
{
struct Inventory;
}
namespace xl_runtime
{
bool time_equipment_resources_required(const xl_compat::Inventory &level);
xl_compat::SlowMotionOutput slow_motion_step(xl_compat::SlowMotionInput input, const xl_compat::SlowMotionSink &sink = {});
const xl_compat::SlowMotionState &slow_motion_state();
} // namespace xl_runtime
#endif
#endif
