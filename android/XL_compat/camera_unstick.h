#ifndef DXX_XL_CAMERA_UNSTICK_H
#define DXX_XL_CAMERA_UNSTICK_H
#include "model_hitbox_query.h"
namespace xl_compat
{
struct CameraUnstickState {
	FixedLightPoint old_position{}, new_position{}, offset{};
};
// The query observes the actor position after the source's per-iteration update
template <typename Query>
void camera_unstick(CameraUnstickState &state, FixedLightPoint &position, const FixedLightPoint &camera_position,
                    const FixedLightPoint &old_velocity, int32_t size, Query query)
{
	if (old_velocity == FixedLightPoint{}) {
		state.offset = geometry_default_direction(hitbox_difference(camera_position, position));
		for (auto &component : state.offset) {
			const auto product = int64_t(component) * size;
			component = hitbox_fixed(double(product) / 65536.0);
		}
	}
	position = state.old_position;
	const bool moved = position != state.new_position;
	auto candidate = state.new_position, offset = state.offset;
	for (int iteration = 0, direction = -1; iteration < 8 || direction < 0; ++iteration) {
		for (auto &component : offset) component /= 2;
		if (offset == FixedLightPoint{}) break;
		for (unsigned axis = 0; axis < 3; ++axis) candidate[axis] = hitbox_fixed(double(candidate[axis]) + int64_t(offset[axis]) * direction);
		if (!moved) position = candidate;
		direction = query(position, candidate) ? -1 : 1;
		if (direction > 0) state.new_position = candidate;
	}
	position = state.new_position;
}
} // namespace xl_compat
#endif
