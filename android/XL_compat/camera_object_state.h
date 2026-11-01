#ifndef DXX_XL_CAMERA_OBJECT_STATE_H
#define DXX_XL_CAMERA_OBJECT_STATE_H
#include <cstdint>
#include <stdexcept>

namespace xl_compat
{
struct CameraObjectState {
	int id = 0, control = 0, movement = 0, model = 0, ai_mode = 0;
	std::int32_t size = 0, life = 0;
};
// Pinned InitCamBots calls AdjustSize before assigning the addon model
// A zero old-model radius leaves the serialized size untouched
inline void initialize_camera_object(CameraObjectState &object, int robot, int model, std::int32_t old_model_radius)
{
	object.id = robot;
	if (old_model_radius) object.size = old_model_radius;
	object.life = 0x3fffffff;
	object.control = 18;
	object.movement = 0;
	object.model = model;
	object.ai_mode = 0;
}

// Pinned CCamera::Rotate retains t0, even after a successful rotation
// The caller applies the heading to the original orientation and repairs the matrix
struct CameraRotation {
	std::int16_t angle = 0, delta = 0;
	std::int64_t t0 = 0;
	bool advance(bool attached, int behavior, std::int64_t now, int speed, std::int32_t &brakes)
	{
		if (!attached || (t0 >= 0 && now - t0 < 1000 / 90) || behavior != 129) return false;
		if (speed < 1000 || now < 0 || now > INT32_MAX)
			throw std::runtime_error("Camera rotation exceeds qualified speed/clock bounds");
		constexpr int limit = 65536 / 8, degree = 65536 / 360;
		const int magnitude = delta < 0 ? -int(delta) : int(delta);
		const int step = degree / (speed / 1000);
		const int sign = delta < 0 ? -1 : 1;
		if (magnitude != step) delta = std::int16_t(sign * step);
		brakes = std::int32_t(now);
		if (angle >= limit || angle <= -limit) {
			if (angle * sign - limit >= delta * sign)
				angle = std::int16_t(sign * limit + delta - sign);
			delta = std::int16_t(-delta);
		}
		angle = std::int16_t(angle + delta);
		return true;
	}
};
} // namespace xl_compat
#endif
