#ifndef XL_SLOW_MOTION_RUNTIME_H
#define XL_SLOW_MOTION_RUNTIME_H
#include <stdint.h>
struct object;
struct vms_vector;
#ifdef __cplusplus
extern "C" {
#endif
void xl_slow_motion_reset(void);
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
