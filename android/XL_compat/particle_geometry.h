#ifndef DXX_XL_PARTICLE_GEOMETRY_H
#define DXX_XL_PARTICLE_GEOMETRY_H
#include "inventory.h"
#include <cmath>
#include <stdexcept>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
using ParticlePoint = std::array<int, 3>;
struct ParticleFace {
	std::array<ParticlePoint, 4> corners{};
	unsigned count = 0;
};

inline ParticleFace particle_face(const Inventory &level, int segment, int side, bool density_geometry)
{
	if (segment < 0 || std::size_t(segment) >= level.segments.size() || side < 0 || side >= 6)
		throw std::runtime_error("XL particle emitter references an invalid segment or side");
	const auto &s = level.segments[segment];
	// FaceSize uses classic slots even when the emission side has explicit corners
	const auto &slots = density_geometry ? classic_side_corners[side] : s.sides[side].corners;
	ParticleFace result;
	for (const int slot : slots) {
		if (slot == 255) continue;
		if (slot < 0 || slot >= 8) throw std::runtime_error("XL particle side has an invalid corner");
		const int vertex = s.vertices[slot];
		if (vertex == 65535) continue;
		if (vertex < 0 || std::size_t(vertex) >= level.vertices.size())
			throw std::runtime_error("XL particle side references an invalid vertex");
		result.corners[result.count++] = level.vertices[vertex];
	}
	if (result.count < 3) throw std::runtime_error("XL particle emitter needs at least three face corners");
	return result;
}

inline float particle_triangle_size(const ParticlePoint &a, const ParticlePoint &b, const ParticlePoint &c)
{
	std::array<float, 3> u{}, v{};
	for (unsigned axis = 0; axis < 3; ++axis) {
		const auto ab = std::int64_t(b[axis]) - a[axis], ac = std::int64_t(c[axis]) - a[axis];
		if (ab < INT32_MIN || ab > INT32_MAX || ac < INT32_MIN || ac > INT32_MAX)
			throw std::runtime_error("XL particle face exceeds the source fixed-point range");
		u[axis] = float(ab) / 65536.0f;
		v[axis] = float(ac) / 65536.0f;
	}
	const float x = u[1] * v[2] - u[2] * v[1];
	const float y = u[2] * v[0] - u[0] * v[2];
	const float z = u[0] * v[1] - u[1] * v[0];
	// XL normalizes area to a 20 by 20 world-unit quad
	return float(std::sqrt(double(x * x + y * y + z * z))) / 800.0f;
}

inline float particle_face_size(const ParticleFace &face)
{
	if (face.count < 3 || face.count > 4) throw std::runtime_error("XL particle face has an invalid corner count");
	float size = particle_triangle_size(face.corners[0], face.corners[1], face.corners[2]);
	if (face.count == 4) size += particle_triangle_size(face.corners[0], face.corners[2], face.corners[3]);
	return size;
}

// Supply the isolated effects RNG; this helper never calls the simulation RNG
template <typename Random>
ParticlePoint particle_face_point(const ParticleFace &face, Random &&random)
{
	if (face.count < 3 || face.count > 4) throw std::runtime_error("XL particle face has an invalid corner count");
	const bool first = face.count == 3 || random(2) != 0;
	const auto &a = face.corners[0];
	const auto &b = face.corners[first ? 1 : 2];
	const auto &c = face.corners[first ? 2 : 3];
	int wa = 2 * random(32768), wb = 2 * random(32768);
	if (wa + wb > 65536) {
		wa = 65536 - wa;
		wb = 65536 - wb;
	}
	const int wc = 65536 - wa - wb;
	ParticlePoint result{};
	for (unsigned axis = 0; axis < 3; ++axis) {
		// Each source vector multiply truncates separately, before addition
		result[axis] = int(double(wa) / 65536.0 * a[axis]) + int(double(wb) / 65536.0 * b[axis]) + int(double(wc) / 65536.0 * c[axis]);
	}
	return result;
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
