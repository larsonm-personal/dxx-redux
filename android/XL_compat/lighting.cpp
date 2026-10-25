#include "lighting.h"
#include "inventory.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace xl_compat
{
namespace
{
LightPoint difference(const LightPoint &a, const LightPoint &b)
{
	return { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
}
float dot(const LightPoint &a, const LightPoint &b)
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
LightPoint along(const LightPoint &origin, const LightPoint &direction, float distance)
{
	return { origin[0] + direction[0] * distance, origin[1] + direction[1] * distance, origin[2] + direction[2] * distance };
}
void sort_geometry_distances(std::vector<GeometryLightDistance> &distances, int left, int right)
{
	int l = left, r = right;
	const auto pivot = distances[(l + r) / 2].distance;
	do {
		while (distances[l].distance < pivot) ++l;
		while (distances[r].distance > pivot) --r;
		if (l <= r) {
			if (l < r) std::swap(distances[l], distances[r]);
			++l;
			--r;
		}
	} while (l <= r);
	if (l < right) sort_geometry_distances(distances, l, right);
	if (left < r) sort_geometry_distances(distances, left, r);
}
} // namespace

LightPoint geometry_emission_direction(const std::array<FixedLightPoint, 2> &normals)
{
	LightPoint result;
	for (unsigned axis = 0; axis < 3; ++axis) {
		const auto average = (std::int64_t(normals[0][axis]) + normals[1][axis]) / 2;
		result[axis] = static_cast<float>(average) / 65536.0f;
	}
	const float magnitude = std::sqrt(dot(result, result));
	if (magnitude)
		for (auto &value : result) value /= magnitude;
	return result;
}

FixedLightPoint geometry_fixed_cross(const FixedLightPoint &a, const FixedLightPoint &b)
{
	for (unsigned axis = 0; axis < 3; ++axis)
		if (a[axis] < -262144 || a[axis] > 262143 || b[axis] < -262144 || b[axis] > 262143)
			throw std::runtime_error("XL fixed cross requires scaled edges");
	FixedLightPoint result;
	for (unsigned axis = 0; axis < 3; ++axis) {
		const auto j = (axis + 1) % 3, k = (axis + 2) % 3;
		const auto product = std::int64_t(a[j]) * b[k] - std::int64_t(a[k]) * b[j];
		result[axis] = static_cast<std::int32_t>(product / 65536 - (product < 0 && product % 65536 ? 1 : 0));
	}
	return result;
}

FixedLightPoint geometry_side_center(const std::array<LightPoint, 4> &corners, const std::array<FixedLightPoint, 2> &normals)
{
	const auto checked = [](double value) -> std::int32_t {
		if (!std::isfinite(value) || value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
			throw std::runtime_error("Geometry side center exceeds fixed-point range");
		return static_cast<std::int32_t>(value);
	};
	FixedLightPoint center{}, start{}, direction{}, offset{};
	for (unsigned axis = 0; axis < 3; ++axis) {
		float sum = 0;
		for (const auto &corner : corners) sum += corner[axis];
		center[axis] = checked((sum / 4.0f) * 65536.0f);
		const auto average = (std::int64_t(normals[0][axis]) + normals[1][axis]) / 2;
		start[axis] = checked(double(center[axis]) + average);
		direction[axis] = checked(-2.0 * average);
		offset[axis] = checked(double(start[axis]) - checked(corners[0][axis] * 65536.0f));
	}
	const auto fixed_dot = [&](const FixedLightPoint &a, const FixedLightPoint &b) {
		return checked((double(a[0]) * b[0] + double(a[1]) * b[1] + double(a[2]) * b[2]) / 65536.0);
	};
	const auto denominator = checked(-double(fixed_dot(normals[0], direction)));
	if (!denominator) return center;
	const auto numerator = fixed_dot(normals[0], offset);
	const auto fraction = checked(double(numerator) / denominator * 65536.0);
	if (fraction > 65536) return center;
	for (unsigned axis = 0; axis < 3; ++axis) {
		// XL's active FixMul macro truncates the double product toward zero
		const auto scaled = checked(double(fraction) / 65536.0 * direction[axis]);
		center[axis] = checked(double(start[axis]) + scaled);
	}
	return center;
}

std::array<LightPoint, 3> geometry_orient_normals(const std::array<LightPoint, 2> &normals,
                                                  const std::array<FixedLightPoint, 2> &fixed_normals, const LightPoint &float_direction, const FixedLightPoint &fixed_direction)
{
	std::array<LightPoint, 3> result{ normals[0], normals[1], {} };
	for (unsigned i = 0; i < 2; ++i)
		if (dot(result[i], float_direction) < 0)
			for (auto &value : result[i]) value = -value;
	for (unsigned axis = 0; axis < 3; ++axis) result[2][axis] = (result[0][axis] + result[1][axis]) / 2;
	for (unsigned i = 0; i < 2; ++i) {
		const auto &n = fixed_normals[i];
		const double projection = (double(n[0]) * fixed_direction[0] + double(n[1]) * fixed_direction[1] + double(n[2]) * fixed_direction[2]) / 65536.0;
		// Fixed dot truncates toward zero: a negative fraction does not trigger a flip
		if (std::trunc(projection) < 0)
			for (auto &value : result[i]) value = -value;
	}
	// Pinned XL flips float normals again in its fixed pass and retains the earlier average
	return result;
}

LightPoint geometry_triangle_normal(const LightPoint &a, const LightPoint &b, const LightPoint &c)
{
	const auto u = difference(b, a), v = difference(c, b);
	LightPoint normal{ u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0] };
	const float magnitude = std::sqrt(dot(normal, normal));
	if (magnitude)
		for (auto &coordinate : normal) coordinate /= magnitude;
	return normal;
}

LightPoint geometry_portal_normal(const std::array<LightPoint, 3> &points, const std::array<std::uint16_t, 3> &vertices)
{
	for (const auto vertex : vertices)
		if (vertex == 0xffff) throw std::runtime_error("Invalid portal normal vertex");
	std::array<unsigned, 3> order{ 0, 1, 2 };
	for (unsigned i = 1; i < 3; ++i)
		for (unsigned j = 0; j < i; ++j)
			if (vertices[order[j]] > vertices[order[i]]) std::swap(order[i], order[j]);
	auto normal = geometry_triangle_normal(points[order[0]], points[order[1]], points[order[2]]);
	// Fourth reference vertex is the 0xffff sentinel, which sorts last
	if ((order[0] + 3) % 4 == order[1] || (order[1] + 3) % 4 == order[2])
		for (auto &coordinate : normal) coordinate = -coordinate;
	return normal;
}

LightPoint geometry_light_point(const GeometryLightFace &face, const LightPoint &receiver)
{
	if (face.corner_count < 3 || face.corner_count > 4 || face.triangle_count < 1 || face.triangle_count > 2)
		throw std::runtime_error("Invalid geometry light face dimensions");
	for (unsigned i = 0; i < face.triangle_count; ++i) {
		const auto &indices = face.triangles[i];
		for (const auto index : indices)
			if (index >= face.corner_count) throw std::runtime_error("Invalid geometry light triangle corner");
	}
	for (unsigned i = 0; i < face.triangle_count; ++i) {
		const auto &normal = face.normals[i];
		if (std::sqrt(dot(normal, normal)) < 1e-6f) continue;
		const auto &indices = face.triangles[i];
		const auto &a = face.corners[indices[0]];
		const float distance = dot(difference(receiver, a), normal);
		const auto projected = along(receiver, normal, -distance);
		const auto v0 = difference(face.corners[indices[2]], a);
		const auto v1 = difference(face.corners[indices[1]], a);
		const auto v2 = difference(projected, a);
		const float dot00 = dot(v0, v0), dot01 = dot(v0, v1), dot02 = dot(v0, v2), dot11 = dot(v1, v1), dot12 = dot(v1, v2);
		const float inverse = 1.0f / (dot00 * dot11 - dot01 * dot01);
		const float u = (dot11 * dot02 - dot01 * dot12) * inverse;
		const float v = (dot00 * dot12 - dot01 * dot02) * inverse;
		if (!(u < -0.00001f || v < -0.00001f || u + v > 1.00001f))
			return std::abs(distance) < 0.001f ? receiver : projected;
	}
	LightPoint result{};
	float nearest = 1e10f;
	for (unsigned i = 0; i < face.corner_count; ++i) {
		const auto &a = face.corners[i];
		const auto &b = face.corners[(i + 1) % face.corner_count];
		const auto edge = difference(b, a);
		const float squared = std::abs(dot(edge, edge));
		const float amount = squared == 0 ? 0 : dot(difference(receiver, a), edge) / squared;
		const auto point = amount < 0 ? a : amount > 1 ? b
		                                               : along(a, edge, amount);
		const auto delta = difference(point, receiver);
		const float distance = std::sqrt(dot(delta, delta));
		if (nearest > distance) {
			nearest = distance;
			result = point;
		}
	}
	return result;
}

std::vector<GeometryLightDistance> nearest_geometry_lights(std::vector<GeometryLightDistance> candidates)
{
	// Reference light indices are signed 16-bit; never narrow an unbounded vector
	if (candidates.size() > 32768) throw std::runtime_error("Geometry light candidates exceed reference index capacity");
	for (const auto &candidate : candidates)
		if (candidate.index < 0 || candidate.index > 32767) throw std::runtime_error("Invalid geometry light candidate index");
	if (!candidates.empty()) sort_geometry_distances(candidates, 0, static_cast<int>(candidates.size()) - 1);
	if (candidates.size() > 32) candidates.resize(32);
	return candidates;
}

std::optional<GeometryAttenuation> geometry_attenuation(const GeometryLightSample &input)
{
	if (input.brightness <= 0) return std::nullopt;
	float distance = input.distance * input.range_scale;
	float normal_dot = input.normal_dot;
	float attenuation = 1;
	if (distance <= 0) {
		distance = 0;
		normal_dot = 1;
	} else {
		float decay = std::min(input.emission_angle, normal_dot);
		if (decay < 0) {
			decay += 1.0000001f;
			distance /= decay * decay;
		}
		if (distance > input.cutoff) return std::nullopt;
		attenuation = 1.0f + input.linear_attenuation * distance + input.quadratic_attenuation * distance * distance;
	}
	attenuation /= input.brightness;
	return GeometryAttenuation{ distance, normal_dot, attenuation };
}

namespace
{
void scale(Rgb &color, float factor)
{
	for (auto &channel : color) channel *= factor;
}
void cap_to_light(Rgb &color, float light)
{
	const auto maximum = *std::max_element(color.begin(), color.end());
	if (maximum > light) scale(color, light / maximum);
}
std::int32_t fixed(std::int64_t value)
{
	if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
		throw std::runtime_error("XL vertex light overflow");
	return static_cast<std::int32_t>(value);
}
} // namespace

std::int32_t flare_intensity(std::int32_t light, std::int32_t life, unsigned sample)
{
	if (sample > 32767) throw std::runtime_error("Invalid XL flare random sample");
	const float intensity = (1.75f + float(sample) / (32767.0f * 4.0f)) * std::min(light, life);
	if (!std::isfinite(intensity) || double(intensity) < std::numeric_limits<std::int32_t>::min() || double(intensity) > std::numeric_limits<std::int32_t>::max())
		throw std::runtime_error("XL flare intensity overflow");
	return static_cast<std::int32_t>(intensity);
}

void DynamicVertexLight::add(std::int32_t contribution, const Rgb *source_color, bool force_color)
{
	const auto next_intensity = fixed(std::int64_t(intensity) + contribution);
	if (source_color) {
		for (const float channel : *source_color)
			if (!std::isfinite(channel)) throw std::runtime_error("Invalid XL dynamic source color");
		const bool white = (*source_color)[0] == 1 && (*source_color)[1] == 1 && (*source_color)[2] == 1;
		if (force_color || !white) {
			if (has_color) {
				for (unsigned i = 0; i < 3; ++i) color[i] = (color[i] + (*source_color)[i]) / 2;
			} else color = *source_color;
			has_color = true;
		}
	}
	intensity = next_intensity;
}

std::vector<float> vertex_brightness(const Inventory &level)
{
	if (level.level_version != 15 || level.game_version != 33)
		throw std::runtime_error("XL brightness requires audited level 15/game 33 geometry");
	// Pinned XL segment.cpp sideVertIndex; only the audited four-corner layout
	constexpr unsigned corners[6][4] = {
		{ 7, 6, 2, 3 }, { 0, 4, 7, 3 }, { 0, 1, 5, 4 }, { 2, 6, 5, 1 }, { 4, 5, 6, 7 }, { 3, 2, 1, 0 }
	};
	std::vector<float> brightness(level.vertices.size(), 0);
	for (const auto &segment : level.segments) {
		for (unsigned side = 0; side < 6; ++side) {
			// Open portals have no serialized UVLs and do not contribute
			if (segment.children[side] != -1 && segment.sides[side].wall == -1) continue;
			for (unsigned corner = 0; corner < 4; ++corner) {
				const int vertex = segment.vertices[corners[side][corner]];
				const int light = segment.sides[side].uvl[corner * 3 + 2];
				if (vertex < 0 || std::size_t(vertex) >= brightness.size() || light < 0 || light > 65535)
					throw std::runtime_error("Invalid XL brightness geometry");
				// The disk light is an unsigned 16-bit half-fixed value
				brightness[vertex] = std::max(brightness[vertex], float(light * 2) / 65536);
			}
		}
	}
	return brightness;
}

ShadedVertex shade_vertex(const VertexLighting &in)
{
	if (in.blend < 0 || in.blend > 2) throw std::runtime_error("Unknown XL face blend");
	auto color = in.face;
	if (in.ambient_enabled) {
		for (unsigned i = 0; i < 3; ++i) {
			if (in.blend == 1) color[i] *= in.ambient[i];
			else if (in.blend == 2) color[i] = color[i] * in.face_alpha + in.ambient[i] * (1 - in.face_alpha);
			else color[i] += in.ambient[i];
		}
	}
	auto light = in.darkness ? 0 : in.static_light;
	if (!in.darkness && in.flashing) {
		const auto product = std::int64_t(light) * in.flash_scale;
		light = fixed(product >= 0 ? product / 65536 : -((-product + 65535) / 65536));
	}
	const float fl = float(light) / 65536, dl = float(in.dynamic_light) / 65536;
	light = fixed(std::int64_t(light) + in.dynamic_light);
	if (in.extra_game_info && in.has_dynamic_color) {
		if (in.mix_colors) {
			if (in.colored) {
				if (in.ambient_enabled && fl != 0 && in.vertex_brightness != 0) {
					scale(color, fl / in.vertex_brightness);
					for (unsigned i = 0; i < 3; ++i) color[i] += in.dynamic_color[i] * dl;
					cap_to_light(color, fl + dl);
				} else if (in.ambient_enabled) {
					color = in.dynamic_color;
					scale(color, dl);
					cap_to_light(color, dl);
				} else {
					for (unsigned i = 0; i < 3; ++i) color[i] = fl + in.dynamic_color[i] * dl;
					cap_to_light(color, fl + dl);
				}
			} else color.fill(fl + dl);
			if (in.cap_colors)
				for (auto &channel : color) channel = std::min(channel, 1.0f);
		} else {
			color = in.dynamic_color;
			scale(color, float(std::pow(float(light) / 65536, 1.0f / 3.0f)));
		}
	} else cap_to_light(color, fl + dl);
	scale(color, in.brightness);
	const float bright_light = float(light) * in.brightness;
	if (!std::isfinite(bright_light) || double(bright_light) < std::numeric_limits<std::int32_t>::min() || double(bright_light) > std::numeric_limits<std::int32_t>::max())
		throw std::runtime_error("Invalid XL vertex brightness");
	light = std::min(static_cast<std::int32_t>(bright_light), std::int32_t(65536));
	const float factor = in.scale_light ? (in.headlight ? 0.4f : 0.3f) : 1.0f;
	if (!in.face_index || !in.ambient_enabled || in.outside_endlevel)
		color.fill((in.no_bitmap_lighting ? 1.0f : float(light) / 65536) * factor);
	else if (factor != 1) scale(color, factor);
	for (const auto channel : color)
		if (!std::isfinite(channel)) throw std::runtime_error("Invalid XL vertex color");
	return { color, light };
}
} // namespace xl_compat
