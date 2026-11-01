#ifndef DXX_XL_SHIELD_SPHERE_H
#define DXX_XL_SHIELD_SPHERE_H
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>
namespace xl_compat
{
struct ShieldSphereVertex {
	std::array<float, 3> position{};
	std::array<float, 2> uv{};
};
inline int shield_sphere_quality(int texture_quality, int cached = 0)
{
	if (texture_quality < 0 || texture_quality > 3 || (cached && (cached < 2 || cached > 4))) throw std::runtime_error("Shield sphere quality exceeds release profile");
	return cached ? cached : texture_quality < 1 ? 2
	                                             : texture_quality + 1;
}
inline std::vector<ShieldSphereVertex> shield_sphere_mesh(int quality)
{
	if (quality < 2 || quality > 4) throw std::runtime_error("Shield sphere tessellation exceeds release profile");
	using Quad = std::array<ShieldSphereVertex, 4>;
	const float cube[6][4][3] = {
		{ { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 } },
		{ { 1, -1, -1 }, { 1, -1, 1 }, { 1, 1, 1 }, { 1, 1, -1 } },
		{ { 1, -1, 1 }, { -1, -1, 1 }, { -1, 1, 1 }, { 1, 1, 1 } },
		{ { -1, -1, 1 }, { -1, -1, -1 }, { -1, 1, -1 }, { -1, 1, 1 } },
		{ { -1, 1, -1 }, { 1, 1, -1 }, { 1, 1, 1 }, { -1, 1, 1 } },
		{ { 1, -1, -1 }, { -1, -1, -1 }, { -1, -1, 1 }, { 1, -1, 1 } }
	};
	const std::array<std::array<float, 2>, 4> uv{ { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } } };
	const auto add = [](ShieldSphereVertex a, const ShieldSphereVertex &b) {
		for (unsigned i = 0; i < 3; ++i) a.position[i] += b.position[i];
		for (unsigned i = 0; i < 2; ++i) a.uv[i] += b.uv[i];
		return a;
	};
	const auto scale = [](ShieldSphereVertex a, float amount) {
		for (auto &value : a.position) value *= amount;
		for (auto &value : a.uv) value *= amount;
		return a;
	};
	const auto normalize = [](ShieldSphereVertex &a) {
		auto &v = a.position;
		const float magnitude = float(::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));
		for (auto &value : v) value /= magnitude;
	};
	const auto center = [&](const Quad &quad) {
		auto value = quad[0];
		for (unsigned i = 1; i < 4; ++i) value = add(value, quad[i]);
		value = scale(value, .25f);
		normalize(value);
		return value;
	};
	std::vector<Quad> quads(6);
	for (unsigned face = 0; face < 6; ++face)
		for (unsigned vertex = 0; vertex < 4; ++vertex) {
			for (unsigned axis = 0; axis < 3; ++axis) quads[face][vertex].position[axis] = cube[face][vertex][axis];
			quads[face][vertex].uv = uv[vertex];
		}
	const unsigned order[4][4] = { { 0, 4, 8, 7 }, { 4, 1, 5, 8 }, { 8, 5, 2, 6 }, { 7, 8, 6, 3 } };
	for (int level = 0; level < quality - 1; ++level) {
		std::vector<Quad> next;
		next.reserve(quads.size() * 4);
		for (const auto &quad : quads) {
			std::array<ShieldSphereVertex, 9> vertices{};
			for (unsigned i = 0; i < 4; ++i) {
				vertices[i] = quad[i];
				vertices[i + 4] = scale(add(quad[i], quad[(i + 1) % 4]), .5f);
			}
			vertices[8] = center(quad);
			// Source normalizes the already normalized center again in quad subdivision
			for (auto &vertex : vertices) normalize(vertex);
			for (const auto &indices : order) next.push_back({ vertices[indices[0]], vertices[indices[1]], vertices[indices[2]], vertices[indices[3]] });
		}
		quads.swap(next);
	}
	std::vector<ShieldSphereVertex> result;
	result.reserve(quads.size() * 12);
	for (const auto &quad : quads) {
		auto corners = quad;
		for (auto &vertex : corners) normalize(vertex);
		const auto middle = center(quad);
		for (unsigned i = 0; i < 4; ++i) {
			result.push_back(corners[i]);
			result.push_back(corners[(i + 1) % 4]);
			result.push_back(middle);
		}
	}
	return result;
}
} // namespace xl_compat
#endif
