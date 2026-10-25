#include "geometry.h"
#include <limits>
#include <stdexcept>
#include <algorithm>
#include <cmath>
extern "C" {
#include "vecmat.h"
#include "segment.h"
#include "gameseg.h"
}

namespace xl_compat
{
namespace
{
FixedLightPoint edge(const FixedLightPoint &a, const FixedLightPoint &b)
{
	FixedLightPoint result;
	std::uint32_t check = 0;
	for (unsigned axis = 0; axis < 3; ++axis) {
		const auto value = std::int64_t(a[axis]) - b[axis];
		if (value <= (std::numeric_limits<std::int32_t>::min)() || value > (std::numeric_limits<std::int32_t>::max)())
			throw std::runtime_error("XL normal edge exceeds fixed-point range");
		result[axis] = static_cast<std::int32_t>(value);
		check |= static_cast<std::uint32_t>(value < 0 ? -value : value);
	}
	if (!check) return result;
	unsigned shift = 0;
	if (check & 0xfffc0000) {
		while (check & 0xfff00000) {
			shift += 4;
			check >>= 4;
		}
		while (check & 0xfffc0000) {
			shift += 2;
			check >>= 2;
		}
		const auto divisor = std::int64_t(1) << shift;
		for (auto &value : result) value = static_cast<std::int32_t>(value / divisor - (value < 0 && value % divisor ? 1 : 0));
	} else if (!(check & 0xffff8000)) {
		while (!(check & 0xfffff000)) {
			shift += 4;
			check <<= 4;
		}
		while (!(check & 0xffff8000)) {
			shift += 2;
			check <<= 2;
		}
		for (auto &value : result) value = static_cast<std::int32_t>(std::int64_t(value) * (std::int64_t(1) << shift));
	}
	return result;
}
} // namespace

FixedLightPoint geometry_fixed_direction(const FixedLightPoint &direction)
{
	vms_vector vector{ direction[0], direction[1], direction[2] };
	const auto magnitude = vm_vec_mag(&vector);
	FixedLightPoint result{};
	if (magnitude > 0)
		for (unsigned axis = 0; axis < 3; ++axis) result[axis] = static_cast<std::int32_t>(double(direction[axis]) / magnitude * 65536.0);
	return result;
}

FixedLightPoint geometry_fixed_normal(const FixedLightPoint &a, const FixedLightPoint &b, const FixedLightPoint &c)
{
	const auto u = edge(b, a), v = edge(c, b);
	return geometry_fixed_direction(geometry_fixed_cross(u, v));
}

ClassicLightingSide compile_classic_lighting_side(int segment_index, int side_index)
{
	if (segment_index < 0 || segment_index > Highest_segment_index || side_index < 0 || side_index >= 6)
		throw std::runtime_error("Invalid classic lighting side");
	const auto &segment = Segments[segment_index];
	ClassicLightingSide result{};
	std::array<FixedLightPoint, 4> fixed_corners;
	std::array<std::uint16_t, 4> ids;
	for (unsigned i = 0; i < 4; ++i) {
		const auto id = segment.verts[Side_to_verts_int[side_index][i]];
		if (id < 0 || id >= 0xffff || id > Highest_vertex_index) throw std::runtime_error("Invalid classic lighting vertex");
		ids[i] = static_cast<std::uint16_t>(id);
		const auto &v = Vertices[id];
		fixed_corners[i] = { v.x, v.y, v.z };
		result.face.corners[i] = { v.x / 65536.0f, v.y / 65536.0f, v.z / 65536.0f };
	}
	const bool solid = segment.children[side_index] < 0;
	if (solid) {
		const auto normal = geometry_fixed_normal(fixed_corners[0], fixed_corners[1], fixed_corners[2]);
		double projection = 0;
		for (unsigned axis = 0; axis < 3; ++axis) projection += double(normal[axis]) * (std::int64_t(fixed_corners[3][axis]) - fixed_corners[1][axis]);
		result.split_13 = std::trunc(projection / 65536.0) < 0;
	} else {
		const auto least = std::min_element(ids.begin(), ids.end()) - ids.begin();
		result.split_13 = least == 1 || least == 3;
	}
	const std::array<std::array<unsigned, 3>, 2> normals_order = result.split_13
	                                                                 ? std::array<std::array<unsigned, 3>, 2>{ { { 0, 1, 3 }, { 1, 2, 3 } } }
	                                                                 : std::array<std::array<unsigned, 3>, 2>{ { { 0, 1, 2 }, { 0, 2, 3 } } };
	result.face.triangles = result.split_13
	                            ? std::array<std::array<unsigned, 3>, 2>{ { { 3, 0, 1 }, { 1, 2, 3 } } }
	                            : std::array<std::array<unsigned, 3>, 2>{ { { 0, 1, 2 }, { 2, 3, 0 } } };
	for (unsigned triangle = 0; triangle < 2; ++triangle) {
		auto order = normals_order[triangle];
		std::array<unsigned, 3> winding{ 0, 1, 2 };
		if (!solid)
			for (unsigned i = 1; i < 3; ++i)
				for (unsigned j = 0; j < i; ++j)
					if (ids[order[j]] > ids[order[i]]) {
						std::swap(order[i], order[j]);
						std::swap(winding[i], winding[j]);
					}
		result.fixed_normals[triangle] = geometry_fixed_normal(fixed_corners[order[0]], fixed_corners[order[1]], fixed_corners[order[2]]);
		result.face.normals[triangle] = geometry_triangle_normal(result.face.corners[order[0]], result.face.corners[order[1]], result.face.corners[order[2]]);
		if ((winding[0] + 3) % 4 == winding[1] || (winding[1] + 3) % 4 == winding[2]) {
			for (auto &value : result.fixed_normals[triangle]) value = -value;
			for (auto &value : result.face.normals[triangle]) value = -value;
		}
	}
	result.center = geometry_side_center(result.face.corners, result.fixed_normals);
	LightPoint sum{};
	for (const auto id : segment.verts) {
		if (id < 0 || id > Highest_vertex_index) throw std::runtime_error("Invalid classic segment vertex");
		const auto &v = Vertices[id];
		sum[0] += v.x / 65536.0f;
		sum[1] += v.y / 65536.0f;
		sum[2] += v.z / 65536.0f;
	}
	FixedLightPoint direction;
	LightPoint float_direction;
	float squared = 0;
	for (unsigned axis = 0; axis < 3; ++axis) {
		result.segment_center[axis] = static_cast<std::int32_t>(sum[axis] / 8.0f * 65536.0f);
		const auto delta = std::int64_t(result.segment_center[axis]) - result.center[axis];
		if (delta < (std::numeric_limits<std::int32_t>::min)() || delta > (std::numeric_limits<std::int32_t>::max)()) throw std::runtime_error("XL center direction exceeds fixed-point range");
		direction[axis] = static_cast<std::int32_t>(delta);
		float_direction[axis] = direction[axis] / 65536.0f;
		squared += float_direction[axis] * float_direction[axis];
	}
	if (squared)
		for (auto &value : float_direction) value /= std::sqrt(squared);
	const auto oriented = geometry_orient_normals(result.face.normals, result.fixed_normals, float_direction, geometry_fixed_direction(direction));
	result.face.normals = { oriented[0], oriented[1] };
	result.average_normal = oriented[2];
	return result;
}
} // namespace xl_compat
