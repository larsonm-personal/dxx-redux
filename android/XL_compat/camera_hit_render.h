#ifndef DXX_XL_CAMERA_HIT_RENDER_H
#define DXX_XL_CAMERA_HIT_RENDER_H
#include "camera_hit_state.h"
#include <cmath>
namespace xl_compat
{
struct CameraHitEnvelope {
	bool needed = false;
	unsigned active = 0;
	std::array<bool, 3> slots{};
	std::array<float, 3> inverse_radius{ 1e6f, 1e6f, 1e6f };
};
// Scalar NeedEffect/SetupHitEffectShader profile; transforms and drawing are separate
inline CameraHitEnvelope camera_hit_envelope(const CameraHitState &state, int32_t size, int32_t ticks, int duration, bool hardware_transform)
{
	if (size <= 0 || (duration != 500 && duration != 2000)) throw std::runtime_error("Camera hit envelope exceeds qualified profile");
	const float object_size = float(size) / 65536.0f;
	const float needed_size = 1.0f + 2.0f / object_size;
	float shader_size = needed_size;
	if (!hardware_transform) shader_size *= object_size;
	CameraHitEnvelope result;
	for (unsigned i = 0; i < state.times.size(); ++i) {
		const int64_t elapsed = int64_t(ticks) - state.times[i];
		if (elapsed < INT32_MIN || elapsed > INT32_MAX) throw std::runtime_error("Camera hit envelope clock exceeds qualified signed range");
		// Source sqrt of a future timestamp yields NaN, which fails its h > threshold test
		if (elapsed < 0 || elapsed >= duration) continue;
		const float fade = float(::cos(::sqrt(float(elapsed) / float(duration)) * 3.141592653589793240 / 2));
		result.needed = result.needed || needed_size * fade > 1.0f / 1e6f;
		const float radius = shader_size * fade;
		if (radius > 1.0f / 1e6f) {
			result.slots[i] = true;
			result.inverse_radius[i] = 1.0f / radius;
			++result.active;
		}
	}
	return result;
}
using HitRenderMatrix = std::array<FixedLightPoint, 3>;
inline HitRenderMatrix camera_hit_inverse_transpose(const HitRenderMatrix &orientation)
{
	HitRenderMatrix m{};
	for (unsigned row = 0; row < 3; ++row)
		for (unsigned column = 0; column < 3; ++column) m[row][column] = orientation[column][row];
	const auto multiply = [](int32_t a, int32_t b) { return hitbox_fixed(double(b) / 65536.0 * double(a)); };
	const auto minor = [&](int32_t a, int32_t b, int32_t c, int32_t d) { return hitbox_fixed(double(multiply(a, b)) - multiply(c, d)); };
	int32_t determinant = multiply(m[0][0], minor(m[1][1], m[2][2], m[1][2], m[2][1]));
	determinant = hitbox_fixed(double(determinant) + multiply(m[0][1], minor(m[1][2], m[2][0], m[1][0], m[2][2])));
	determinant = hitbox_fixed(double(determinant) + multiply(m[0][2], minor(m[1][0], m[2][1], m[1][1], m[2][0])));
	// Source FixDiv explicitly returns fixed integer 1 when the divisor is zero
	const auto divide = [&](int32_t value) { return hitbox_fixed(determinant ? double(value) / double(determinant) * 65536.0 : 1); };
	return { { { divide(minor(m[1][1], m[2][2], m[1][2], m[2][1])), divide(minor(m[0][2], m[2][1], m[0][1], m[2][2])), divide(minor(m[0][1], m[1][2], m[0][2], m[1][1])) },
		       { divide(minor(m[1][2], m[2][0], m[1][0], m[2][2])), divide(minor(m[0][0], m[2][2], m[0][2], m[2][0])), divide(minor(m[0][2], m[1][0], m[0][0], m[1][2])) },
		       { divide(minor(m[1][0], m[2][1], m[1][1], m[2][0])), divide(minor(m[0][1], m[2][0], m[0][0], m[2][1])), divide(minor(m[0][0], m[1][1], m[0][1], m[1][0])) } } };
}
struct CameraHitHardware {
	CameraHitEnvelope envelope;
	std::array<std::array<float, 4>, 3> vectors{};
};
inline CameraHitHardware camera_hit_hardware(const CameraHitState &state, int32_t size, int32_t ticks, int duration, const HitRenderMatrix &orientation)
{
	CameraHitHardware result;
	result.envelope = camera_hit_envelope(state, size, ticks, duration, true);
	const auto matrix = camera_hit_inverse_transpose(orientation);
	for (unsigned i = 0; i < 3; ++i) {
		if (!result.envelope.slots[i]) continue;
		auto &v = result.vectors[i];
		for (unsigned axis = 0; axis < 3; ++axis) v[axis] = float(hitbox_dot(state.points[i], matrix[axis])) / 65536.0f;
		const float magnitude = float(::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));
		if (magnitude)
			for (unsigned axis = 0; axis < 3; ++axis) v[axis] /= magnitude;
	}
	return result;
}
} // namespace xl_compat
#endif
