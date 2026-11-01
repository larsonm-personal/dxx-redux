#ifndef DXX_XL_CAMERA_SPHERE_UNSTICK_H
#define DXX_XL_CAMERA_SPHERE_UNSTICK_H
#include "camera_unstick.h"
#include "camera_hit_state.h"
#include "model_sphere_query.h"
namespace xl_compat
{
// Default nHitboxes=2 / nDamageModel=0 composition of the reverse object query
inline unsigned camera_sphere_unstick(CameraUnstickState &state, CameraHitState &hits,
                                      FixedLightPoint &position, const FixedLightPoint &camera_position,
                                      const FixedLightPoint &old_velocity, int32_t size, int type,
                                      int32_t ticks, int shield_duration)
{
	unsigned queries = 0;
	camera_unstick(state, position, camera_position, old_velocity, size,
	               [&](const FixedLightPoint &start, const FixedLightPoint &end) -> bool {
		               ++queries;
		               FixedLightPoint hit{};
		               // Source passes the physical actor as stationary AND supplies its radius
		               const int result = model_sphere_query(hit, start, end, position, hitbox_fixed(double(size) * 2));
		               if (result < 0) return false;
		               hits.register_hit(hit, position, size, type, ticks, shield_duration);
		               return result != 0;
	               });
	return queries;
}
} // namespace xl_compat
#endif
