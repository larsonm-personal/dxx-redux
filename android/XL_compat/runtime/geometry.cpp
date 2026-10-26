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

static FixedLightPoint setup_direction(const FixedLightPoint &direction)
{
	vms_vector vector{ direction[0], direction[1], direction[2] };
	const auto magnitude = vm_vec_mag(&vector);
	FixedLightPoint result{};
	if (magnitude > 0)
		for (unsigned axis = 0; axis < 3; ++axis) result[axis] = static_cast<std::int32_t>(double(direction[axis]) / magnitude * 65536.0);
	return result;
}

FixedLightPoint geometry_fixed_direction(const FixedLightPoint &direction)
{
	return geometry_default_direction(direction);
}

std::array<FixedLightPoint, 3> geometry_camera_basis(const FixedLightPoint &forward)
{
	std::array<FixedLightPoint, 3> basis{};
	auto &right = basis[0], &up = basis[1], &direction = basis[2];
	direction = geometry_fixed_direction(forward);
	if (direction == FixedLightPoint{}) throw std::runtime_error("Degenerate geometry camera direction");
	if (!direction[0] && !direction[2]) {
		right[0] = 65536;
		// Reference tests z here, not y, even though z is zero in this branch
		up[2] = direction[2] < 0 ? 65536 : -65536;
	} else {
		right = geometry_fixed_direction({ direction[2], 0, -direction[0] });
		up = geometry_fixed_cross(direction, right);
	}
	return basis;
}

FixedLightPoint geometry_fixed_normal(const FixedLightPoint &a, const FixedLightPoint &b, const FixedLightPoint &c)
{
	const auto u = edge(b, a), v = edge(c, b);
	return setup_direction(geometry_fixed_cross(u, v));
}

std::int32_t geometry_fixed_distance(const FixedLightPoint &a, const FixedLightPoint &b)
{
	FixedLightPoint delta;
	for (unsigned axis = 0; axis < 3; ++axis) {
		const auto value = std::int64_t(a[axis]) - b[axis];
		if (value < (std::numeric_limits<std::int32_t>::min)() || value > (std::numeric_limits<std::int32_t>::max)())
			throw std::runtime_error("XL geometry distance exceeds fixed-point range");
		delta[axis] = static_cast<std::int32_t>(value);
	}
	return geometry_default_magnitude(delta);
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
	result.center = geometry_side_center(result.face.corners, result.fixed_normals, fixed_corners[0]);
	for (const auto &corner : fixed_corners) result.outer_radius = (std::max) (result.outer_radius, geometry_fixed_distance(result.center, corner));
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
	const auto oriented = geometry_orient_normals(result.face.normals, result.fixed_normals, float_direction, setup_direction(direction));
	result.face.normals = { oriented[0], oriented[1] };
	result.average_normal = oriented[2];
	return result;
}

std::int32_t classic_segment_average_radius(int segment)
{
	if (segment < 0 || segment > Highest_segment_index) throw std::runtime_error("Invalid classic radius segment");
	const auto center = compile_classic_lighting_side(segment, 0).segment_center;
	std::int32_t inner = (std::numeric_limits<std::int32_t>::max)(), outer = 0;
	for (int side = 0; side < 6; ++side) {
		const auto geometry = compile_classic_lighting_side(segment, side);
		const int a = geometry.split_13 ? 3 : 0, b = geometry.split_13 ? 1 : 2;
		const int anchor = (std::min) (Segments[segment].verts[Side_to_verts_int[side][a]], Segments[segment].verts[Side_to_verts_int[side][b]]);
		if (anchor < 0 || anchor > Highest_vertex_index) throw std::runtime_error("Invalid classic radius anchor");
		const auto &v = Vertices[anchor];
		const FixedLightPoint point{ v.x, v.y, v.z };
		std::int64_t sum = 0;
		unsigned count = 0;
		for (const auto &normal : geometry.fixed_normals) {
			double projection = 0;
			for (unsigned axis = 0; axis < 3; ++axis) projection += double(std::int64_t(center[axis]) - point[axis]) * normal[axis];
			projection /= 65536.0;
			if (projection < (std::numeric_limits<std::int32_t>::min)() || projection > (std::numeric_limits<std::int32_t>::max)())
				throw std::runtime_error("Classic radius plane distance exceeds fixed-point range");
			const auto distance = static_cast<std::int32_t>(projection);
			if (distance >= -65) {
				sum += distance;
				++count;
			}
		}
		if (count == 2) sum /= 2;
		if (sum > (std::numeric_limits<std::int32_t>::max)()) throw std::runtime_error("Classic side distance exceeds fixed-point range");
		inner = (std::min) (inner, static_cast<std::int32_t>(sum));
	}
	for (const auto vertex : Segments[segment].verts) {
		if (vertex < 0 || vertex > Highest_vertex_index) throw std::runtime_error("Invalid classic radius vertex");
		const auto &v = Vertices[vertex];
		outer = (std::max) (outer, geometry_fixed_distance(center, { v.x, v.y, v.z }));
	}
	return static_cast<std::int32_t>((std::int64_t(inner) + outer) / 2);
}

std::vector<ClassicVertexNormal> compile_classic_vertex_normals()
{
	if (Highest_vertex_index < 0 || Highest_segment_index < 0)
		throw std::runtime_error("Classic vertex normals require loaded geometry");
	std::vector<ClassicVertexNormal> result(static_cast<std::size_t>(Highest_vertex_index) + 1);
	for (int segment = 0; segment <= Highest_segment_index; ++segment)
		for (int side = 0; side < 6; ++side) {
			const auto geometry = compile_classic_lighting_side(segment, side);
			LightPoint average;
			for (unsigned axis = 0; axis < 3; ++axis)
				average[axis] = static_cast<float>((std::int64_t(geometry.fixed_normals[0][axis]) + geometry.fixed_normals[1][axis]) / 2) / 65536.0f;
			for (int corner = 0; corner < 4; ++corner) {
				const int vertex = Segments[segment].verts[Side_to_verts_int[side][corner]];
				if (vertex < 0 || vertex > Highest_vertex_index)
					throw std::runtime_error("Invalid classic normal vertex");
				auto &entry = result[vertex];
				for (unsigned axis = 0; axis < 3; ++axis) entry.normal[axis] += average[axis];
				++entry.contributions;
			}
		}
	for (auto &entry : result) {
		// AddToVertexNormal calls both operator+= (which increments) and n++
		const auto faces = static_cast<std::uint8_t>(entry.contributions * 2);
		if (!faces) continue;
		if (faces > 1)
			for (auto &axis : entry.normal) axis /= static_cast<float>(faces);
		const auto &n = entry.normal;
		const float magnitude = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
		if (magnitude)
			for (auto &axis : entry.normal) axis /= magnitude;
	}
	return result;
}
} // namespace xl_compat
