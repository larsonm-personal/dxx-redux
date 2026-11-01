#ifndef DXX_XL_CAMERA_CONTACT_H
#define DXX_XL_CAMERA_CONTACT_H
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace xl_compat
{
template <typename RandomShort>
inline bool camera_gauss_spin(int &skip, std::int32_t frame, std::array<std::int32_t, 3> &thrust, RandomShort random_short)
{
	if (frame < 0 || skip < 0 || skip > 127) throw std::runtime_error("Camera Gauss state exceeds qualified bounds");
	if (std::int64_t(skip) * frame >= 65536) return false;
	if (skip == 127) throw std::runtime_error("Camera Gauss skip count exceeds native signed storage");
	++skip;
	for (auto &axis : thrust) axis = std::int32_t(double(std::int64_t(frame) * skip) / 65536.0 * double(16383 - random_short()));
	return true;
}

// Pinned nonstatic/nonphysics BumpTwoObjects followed by release ApplyForce
inline std::array<std::int32_t, 3> camera_force_velocity(std::array<std::int32_t, 3> velocity, const std::array<std::int32_t, 3> &force, std::int32_t mass, bool blocked)
{
	if (!mass || blocked) return velocity;
	if (mass < 0 || mass > INT32_MAX / 2) throw std::runtime_error("Camera contact mass exceeds qualified bounds");
	const float inverse_mass = 1.0f / (float(mass) / 65536.0f);
	auto result = velocity;
	for (std::size_t axis = 0; axis < result.size(); ++axis) {
		const float acceleration = float(force[axis]) * inverse_mass;
		if (double(acceleration) < INT32_MIN || double(acceleration) > INT32_MAX)
			throw std::runtime_error("Camera contact acceleration exceeds fixed-point bounds");
		const auto next = std::int64_t(velocity[axis]) + std::int32_t(acceleration);
		if (next < INT32_MIN || next > INT32_MAX) throw std::runtime_error("Camera contact velocity exceeds fixed-point bounds");
		result[axis] = std::int32_t(next);
	}
	return result;
}
inline std::array<std::int32_t, 3> camera_reflect_velocity(std::array<std::int32_t, 3> velocity, std::int32_t mass, bool blocked)
{
	if (!mass || blocked) return velocity;
	if (mass < 0 || mass > INT32_MAX / 2) throw std::runtime_error("Camera contact mass exceeds qualified bounds");
	std::array<std::int32_t, 3> force;
	for (unsigned axis = 0; axis < 3; ++axis) {
		const double value = double(-2 * mass) / 65536.0 * double(velocity[axis]);
		if (value < INT32_MIN || value > INT32_MAX) throw std::runtime_error("Camera contact force exceeds fixed-point bounds");
		force[axis] = std::int32_t(value);
	}
	return camera_force_velocity(velocity, force, mass, false);
}
} // namespace xl_compat
#endif
