#ifndef DXX_XL_LIGHTNING_RENDER_H
#define DXX_XL_LIGHTNING_RENDER_H
#include "lightning_path.h"
#include <algorithm>
#include <cmath>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
using LightningVertex = std::array<float, 3>;
struct LightningCore {
	std::vector<LightningVertex> vertices;
	float average_depth;
};
struct LightningClipVertex {
	std::array<float, 4> position;
	std::array<float, 2> uv;
};
inline LightningClipVertex lightning_clip_vertex(const LightningVertex &position, const std::array<float, 2> &uv,
                                                 const std::array<float, 16> &matrix)
{
	LightningClipVertex result{ {}, uv };
	for (unsigned row = 0; row < 4; ++row)
		result.position[row] = matrix[row] * position[0] + matrix[4 + row] * position[1] + matrix[8 + row] * position[2] + matrix[12 + row];
	return result;
}
inline bool lightning_outside_clip(const LightningClipVertex &vertex)
{
	const auto &p = vertex.position;
	return std::abs(p[0]) > p[3] || std::abs(p[1]) > p[3] || std::abs(p[2]) > p[3];
}
// Source viewport projection uses positive camera Z and only the projection 3x3
inline std::array<int, 2> lightning_screen_point(const LightningVertex &camera, const std::array<float, 16> &projection, int width, int height)
{
	if (width <= 0 || height <= 0) throw std::runtime_error("XL lightning canvas has invalid dimensions");
	std::array<int, 2> result;
	for (unsigned axis = 0; axis < 2; ++axis) {
		const float projected = projection[axis] * camera[0] + projection[4 + axis] * camera[1] + projection[8 + axis] * camera[2];
		const float coordinate = (float(axis ? height : width) / 2.f) * (1.f + projected / std::abs(camera[2]));
		if (!std::isfinite(coordinate) || double(coordinate) < INT32_MIN || double(coordinate) > INT32_MAX)
			throw std::runtime_error("XL lightning projected extent exceeds the source integer range");
		result[axis] = int(coordinate);
	}
	return result;
}
// Authored persistent bolts have negative life and do not use transient alpha fades
template <class Random>
std::array<float, 4> lightning_draw_color(std::array<float, 4> color, bool visible_blur, Random &random)
{
	for (unsigned channel = 0; channel < 3; ++channel) color[channel] *= float(1.0 - random.unit() / 5.0);
	// Source CFloatVector division scales xyz, leaving alpha unchanged
	if (visible_blur)
		for (unsigned channel = 0; channel < 3; ++channel) color[channel] /= 4.0f;
	return color;
}
struct LightningGlow {
	std::vector<LightningVertex> vertices;
	std::vector<std::array<float, 2>> uv;
	float distance_scale;
};
namespace lightning_render_detail
{
inline LightningVertex add(LightningVertex a, const LightningVertex &b)
{
	for (unsigned i = 0; i < 3; ++i) a[i] += b[i];
	return a;
}
inline LightningVertex subtract(LightningVertex a, const LightningVertex &b)
{
	for (unsigned i = 0; i < 3; ++i) a[i] -= b[i];
	return a;
}
inline LightningVertex multiply(LightningVertex a, float scale)
{
	for (auto &value : a) value *= scale;
	return a;
}
inline LightningVertex divide(LightningVertex a, float scale)
{
	for (auto &value : a) value /= scale;
	return a;
}
inline LightningVertex average(const LightningVertex &a, const LightningVertex &b)
{
	return divide(add(a, b), 2.0f);
}
inline LightningVertex normal(const LightningVertex &p0, const LightningVertex &p1, const LightningVertex &p2)
{
	const auto a = subtract(p1, p0), b = subtract(p2, p1);
	LightningVertex cross{ a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
	const float length = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
	return length ? divide(cross, length) : cross;
}
} // namespace lightning_render_detail

template <class ViewDepth>
LightningCore lightning_core_geometry(const std::vector<LightningPoint> &nodes, ViewDepth view_depth)
{
	using namespace lightning_render_detail;
	if (nodes.size() < 2) throw std::runtime_error("XL lightning core requires at least two nodes");
	LightningCore result;
	result.vertices.reserve(nodes.size() + 4);
	float nearest = 1e30f, farthest = -1e30f;
	for (const auto &node : nodes) {
		LightningVertex vertex;
		for (unsigned axis = 0; axis < 3; ++axis) vertex[axis] = float(node[axis]) / 65536.0f;
		result.vertices.push_back(vertex);
		const float depth = view_depth(vertex);
		if (nearest > depth) nearest = depth;
		if (farthest < depth) farthest = depth;
	}
	const auto cap = [](const LightningVertex &endpoint, const LightningVertex &adjacent) {
		const auto direction = subtract(endpoint, adjacent);
		const float length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] + direction[2] * direction[2]);
		return add(divide(direction, 100.0f * length), endpoint);
	};
	const auto first = result.vertices.front(), last = result.vertices.back();
	const auto first_cap = cap(first, result.vertices[1]);
	const auto last_cap = cap(last, result.vertices[nodes.size() - 2]);
	result.vertices.insert(result.vertices.end(), { first_cap, first, last, last_cap });
	result.average_depth = (nearest + farthest) / 2.0f;
	return result;
}

inline LightningGlow lightning_glow_geometry(const std::vector<LightningPoint> &nodes, int random_nodes, float width, bool outer_glow,
                                             const LightningVertex &eye, float average_depth, float depth_range)
{
	using namespace lightning_render_detail;
	if (random_nodes < 0 || std::size_t(random_nodes) + 2 > nodes.size() || !std::isfinite(width) || !std::isfinite(average_depth) || !std::isfinite(depth_range) || depth_range <= 0)
		throw std::runtime_error("XL lightning glow has invalid geometry inputs");
	const auto count = nodes.size() - 1 - random_nodes;
	LightningGlow result;
	result.distance_scale = std::pow(1.0f - average_depth / depth_range, 50.0f);
	const float radius = width * result.distance_scale * 0.25f;
	result.vertices.reserve(count * (outer_glow ? 8 : 4));
	result.uv.reserve(count * (outer_glow ? 8 : 4));
	LightningVertex previous{}, current{};
	constexpr std::array<std::array<std::array<float, 2>, 4>, 3> texture{ { { { { 0, .45f }, { 1, .45f }, { 1, .55f }, { 0, .55f } } },
		                                                                    { { { 0, .15f }, { 1, .15f }, { 1, .5f }, { 0, .5f } } },
		                                                                    { { { 0, .5f }, { 1, .5f }, { 1, .85f }, { 0, .85f } } } } };
	for (std::size_t i = 0; i <= count; ++i) {
		previous = current;
		for (unsigned axis = 0; axis < 3; ++axis) current[axis] = float(nodes[i][axis]) / 65536.0f;
		if (!i) continue;
		const auto offset = multiply(normal(previous, current, eye), radius);
		if (i == 1) previous = add(previous, multiply(subtract(previous, current), width / 4.0f));
		if (i == count) current = add(current, divide(multiply(subtract(current, previous), width), 4.0f));
		result.vertices.insert(result.vertices.end(), { add(previous, offset), subtract(previous, offset), subtract(current, offset), add(current, offset) });
		result.uv.insert(result.uv.end(), texture[0].begin(), texture[0].end());
	}
	std::copy(texture[2].begin(), texture[2].end(), result.uv.end() - 4);
	std::copy(texture[1].begin(), texture[1].end(), result.uv.begin());
	for (std::size_t i = 2, j = 4; i < 4 * (count - 1); i += 4, j += 4) {
		result.vertices[i + 1] = result.vertices[j] = average(result.vertices[i + 1], result.vertices[j]);
		result.vertices[i] = result.vertices[j + 1] = average(result.vertices[i], result.vertices[j + 1]);
	}
	if (outer_glow) {
		const auto layer = result.vertices.size();
		result.vertices.resize(layer * 2);
		result.uv.resize(layer * 2);
		for (std::size_t i = 0; i < layer; i += 2) {
			const auto center = average(result.vertices[i], result.vertices[i + 1]);
			const auto offset = divide(subtract(result.vertices[i], result.vertices[i + 1]), 8.0f);
			result.vertices[layer + i] = add(center, offset);
			result.vertices[layer + i + 1] = subtract(center, offset);
		}
		std::copy_n(result.uv.begin(), layer, result.uv.begin() + layer);
	}
	return result;
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
