#ifndef DXX_XL_MODEL_HITBOX_QUERY_H
#define DXX_XL_MODEL_HITBOX_QUERY_H
#include "model_hitbox.h"
namespace xl_compat
{
inline int32_t hitbox_fixed(double value)
{
	if (!std::isfinite(value) || value < INT32_MIN || value > INT32_MAX) throw std::runtime_error("Hitbox query arithmetic overflows");
	return int32_t(value);
}
inline FixedLightPoint hitbox_difference(const FixedLightPoint &a, const FixedLightPoint &b)
{
	return { hitbox_fixed(double(a[0]) - b[0]), hitbox_fixed(double(a[1]) - b[1]), hitbox_fixed(double(a[2]) - b[2]) };
}
inline int32_t hitbox_dot(const FixedLightPoint &a, const FixedLightPoint &b)
{
	return hitbox_fixed((double(a[0]) * b[0] + double(a[1]) * b[1] + double(a[2]) * b[2]) / 65536.0);
}
inline FixedLightPoint hitbox_closest_point(const FixedLightPoint &a, const FixedLightPoint &b, const FixedLightPoint &point)
{
	const auto delta = hitbox_difference(b, a), relative = hitbox_difference(point, a);
	const auto length = double(delta[0]) * delta[0] + double(delta[1]) * delta[1] + double(delta[2]) * delta[2];
	if (!length) return a;
	const auto u = (double(relative[0]) * delta[0] + double(relative[1]) * delta[1] + double(relative[2]) * delta[2]) / length;
	if (u < 0) return a;
	if (u > 1) return b;
	FixedLightPoint nearest;
	for (unsigned axis = 0; axis < 3; ++axis) nearest[axis] = hitbox_fixed(double(a[axis]) + hitbox_fixed(u * delta[axis]));
	return nearest;
}
inline bool hitbox_point_in_quad(const FixedLightPoint &point, const std::array<FixedLightPoint, 4> &vertices, const FixedLightPoint &normal)
{
	std::array<int64_t, 3> magnitude;
	for (unsigned i = 0; i < 3; ++i) magnitude[i] = std::abs(int64_t(normal[i]));
	const unsigned plane = magnitude[0] > magnitude[1] ? (magnitude[0] > magnitude[2] ? 0 : 2) : (magnitude[1] > magnitude[2] ? 1 : 2);
	constexpr unsigned axes[3][2] = { { 2, 1 }, { 0, 2 }, { 1, 0 } };
	const auto i = axes[plane][normal[plane] > 0 ? 0 : 1], j = axes[plane][normal[plane] > 0 ? 1 : 0];
	for (unsigned edge = 0; edge < 4; ++edge) {
		const auto delta = hitbox_difference(vertices[(edge + 1) % 4], vertices[edge]);
		const auto check = hitbox_difference(point, vertices[edge]);
		const auto a = hitbox_fixed(double(check[i]) * delta[j] / 65536.0);
		const auto b = hitbox_fixed(double(check[j]) * delta[i] / 65536.0);
		if (int64_t(a) - b < 0) return false;
	}
	return true;
}
inline int32_t hitbox_distance_to_quad(const FixedLightPoint &point, const std::array<FixedLightPoint, 4> &vertices, const FixedLightPoint &normal)
{
	if (hitbox_point_in_quad(point, vertices, normal)) return 0;
	int32_t best = INT32_MAX;
	for (unsigned edge = 0; edge < 4; ++edge) {
		const auto &a = vertices[edge], &b = vertices[(edge + 1) % 4];
		const auto nearest = hitbox_closest_point(a, b, point);
		best = std::min(best, geometry_default_magnitude(hitbox_difference(point, nearest)));
	}
	return best;
}
inline bool hitbox_line_plane(FixedLightPoint &hit, const FixedLightPoint &plane, const FixedLightPoint &normal, const FixedLightPoint &start, const FixedLightPoint &end)
{
	const auto delta = hitbox_difference(end, start), relative = hitbox_difference(start, plane);
	std::array<float, 3> u, n, w;
	for (unsigned axis = 0; axis < 3; ++axis) {
		u[axis] = float(delta[axis]) / 65536.0f;
		n[axis] = float(normal[axis]) / 65536.0f;
		w[axis] = float(relative[axis]) / 65536.0f;
	}
	const float denominator = -(n[0] * u[0] + n[1] * u[1] + n[2] * u[2]);
	if (denominator > -1e-6f && denominator < 1e-6f) return false;
	float fraction = (n[0] * w[0] + n[1] * w[1] + n[2] * w[2]) / denominator;
	if (fraction < -0.001f || fraction > 1.001f) return false;
	fraction = std::clamp(fraction, 0.0f, 1.0f);
	for (unsigned axis = 0; axis < 3; ++axis) hit[axis] = hitbox_fixed(double(start[axis]) + hitbox_fixed((u[axis] * fraction) * 65536.0f));
	return true;
}
inline int32_t hitbox_line_quad(FixedLightPoint &hit, const std::array<FixedLightPoint, 4> &vertices, const FixedLightPoint &normal, const FixedLightPoint &start, const FixedLightPoint &end, int32_t radius)
{
	FixedLightPoint candidate;
	if (!hitbox_line_plane(candidate, vertices[0], normal, start, end)) return INT32_MAX;
	if (!radius && hitbox_dot(hitbox_difference(candidate, start), hitbox_difference(candidate, end)) > 0) return INT32_MAX;
	const auto distance = hitbox_distance_to_quad(candidate, vertices, normal);
	if (radius < distance) return INT32_MAX;
	hit = candidate;
	return distance;
}
inline bool hitbox_register(FixedLightPoint &hit, FixedLightPoint &normal, int32_t &nearest,
                            const FixedLightPoint &candidate, const FixedLightPoint &candidate_normal, const FixedLightPoint &reference)
{
	const auto distance = geometry_default_magnitude(hitbox_difference(reference, candidate));
	if (nearest < distance) return false;
	// RegisterHit replaces equal distances, so later faces/boxes win
	nearest = distance;
	hit = candidate;
	normal = candidate_normal;
	return true;
}
inline int32_t hitbox_vector_query(FixedLightPoint &hit, FixedLightPoint &normal, int16_t &model,
                                   ModelHitboxTransform &cache, const std::array<FixedLightPoint, 3> &orientation,
                                   const FixedLightPoint &position, int frame, int collision_model,
                                   const FixedLightPoint &start, const FixedLightPoint &end, int32_t radius)
{
	const auto &boxes = cache.transform(orientation, position, frame, collision_model);
	const std::size_t first = collision_model == 1 ? 0 : 1, last = collision_model == 1 ? 0 : boxes.size() - 1;
	if (last > INT16_MAX) throw std::runtime_error("Hitbox query submodel exceeds source range");
	int32_t nearest = INT32_MAX;
	for (std::size_t box = first; box <= last; ++box) {
		bool accepted = false;
		for (unsigned face = 0; face < 6; ++face) {
			FixedLightPoint candidate;
			if (hitbox_line_quad(candidate, boxes[box].faces[face], boxes[box].normals[face], start, end, radius) == INT32_MAX) continue;
			accepted |= hitbox_register(hit, normal, nearest, candidate, boxes[box].normals[face], start);
		}
		if (accepted) model = int16_t(box);
	}
	return nearest;
}
inline bool hitbox_quad_edges(FixedLightPoint &average, const std::array<FixedLightPoint, 4> &edges,
                              const std::array<FixedLightPoint, 4> &plane, const FixedLightPoint &normal)
{
	average = {};
	int count = 0;
	for (unsigned edge = 0; edge < 4; ++edge) {
		FixedLightPoint candidate;
		if (hitbox_line_quad(candidate, plane, normal, edges[edge], edges[(edge + 1) % 4], 0) == INT32_MAX) continue;
		++count;
		for (unsigned axis = 0; axis < 3; ++axis) average[axis] = hitbox_fixed(double(average[axis]) + candidate[axis]);
	}
	if (count > 1)
		for (auto &value : average) value = hitbox_fixed(double(value) / (count * 65536.0) * 65536.0);
	return count != 0;
}
inline int hitbox_pair_faces(FixedLightPoint &hit, FixedLightPoint &normal, int32_t &nearest,
                             const WorldHitbox &a, const WorldHitbox &b, const FixedLightPoint &reference)
{
	int count = 0;
	for (unsigned first = 0; first < 6; ++first)
		for (unsigned second = 0; second < 6; ++second) {
			if (hitbox_dot(a.normals[first], b.normals[second]) >= 0) continue;
			FixedLightPoint average;
			if (hitbox_quad_edges(average, a.faces[first], b.faces[second], b.normals[second]))
				count += hitbox_register(hit, normal, nearest, average, b.normals[second], reference);
			if (hitbox_quad_edges(average, b.faces[second], a.faces[first], a.normals[first]))
				count += hitbox_register(hit, normal, nearest, average, b.normals[second], reference);
		}
	return count;
}
inline int32_t hitbox_pair_query(FixedLightPoint &hit, FixedLightPoint &normal, int16_t &model,
                                 ModelHitboxTransform &moving, const std::array<FixedLightPoint, 3> &moving_orientation,
                                 ModelHitboxTransform &stationary, const std::array<FixedLightPoint, 3> &stationary_orientation,
                                 const FixedLightPoint &stationary_position, int frame, int collision_model,
                                 const FixedLightPoint &start, const FixedLightPoint &end)
{
	const auto &a = moving.transform(moving_orientation, end, frame, collision_model);
	const auto &b = stationary.transform(stationary_orientation, stationary_position, frame, collision_model);
	const std::size_t first = collision_model == 1 ? 0 : 1;
	const auto last_a = collision_model == 1 ? 0 : a.size() - 1, last_b = collision_model == 1 ? 0 : b.size() - 1;
	int32_t nearest = INT32_MAX;
	int hits = 0, total = 0;
	for (std::size_t i = first; i <= last_a; ++i)
		for (std::size_t j = first; j <= last_b; ++j) {
			hits = hitbox_pair_faces(hit, normal, nearest, a[i], b[j], start);
			if (hits) {
				total += hits;
				model = int16_t(first);
			}
		}
	// Source tests the final pair's count, not the total accumulated hits
	if (!hits)
		for (std::size_t j = first; j <= last_b; ++j) {
			hits = 0;
			for (unsigned face = 0; face < 6; ++face) {
				FixedLightPoint candidate;
				if (hitbox_line_quad(candidate, b[j].faces[face], b[j].normals[face], start, end, 0) != INT32_MAX)
					hits += hitbox_register(hit, normal, nearest, candidate, b[j].normals[face], start);
			}
			if (hits) {
				total += hits;
				model = int16_t(first);
			}
		}
	return total ? (nearest ? nearest : 1) : 0;
}
struct HitboxQueryActor {
	ModelHitboxTransform *cache;
	std::array<FixedLightPoint, 3> orientation;
	FixedLightPoint position;
	int32_t size, model_radius;
	FixedLightPoint center_offset{};
};
struct HitboxRegistration {
	bool reached = false;
	FixedLightPoint point{};
	int16_t model = -1;
};
inline FixedLightPoint hitbox_actor_center(const HitboxQueryActor &actor)
{
	auto center = actor.position;
	if (actor.cache)
		for (unsigned axis = 0; axis < 3; ++axis) {
			const auto &v = actor.center_offset;
			const auto rotated = hitbox_fixed((double(v[0]) * actor.orientation[0][axis] + double(v[1]) * actor.orientation[1][axis] + double(v[2]) * actor.orientation[2][axis]) / 65536.0);
			center[axis] = hitbox_fixed(double(center[axis]) + rotated);
		}
	return center;
}
// Qualified hitbox branch; sphere-only profiles and RegisterHit side effects
// remain the caller's responsibility
inline int32_t hitbox_object_query(FixedLightPoint &hit, FixedLightPoint &normal, int16_t &model,
                                   const HitboxQueryActor &stationary, const HitboxQueryActor &moving,
                                   int frame, int collision_model, const FixedLightPoint &start, const FixedLightPoint &end, HitboxRegistration *registration = nullptr)
{
	if (registration) *registration = {};
	if (!stationary.cache && !moving.cache) throw std::runtime_error("Object query requires a hitbox actor");
	const auto distance = geometry_default_magnitude(hitbox_difference(stationary.position, hitbox_closest_point(start, end, stationary.position)));
	if (distance > int64_t(stationary.model_radius) + moving.model_radius) return 0;
	auto candidate = hitbox_closest_point(start, end, hitbox_actor_center(stationary));
	FixedLightPoint candidate_normal{};
	int32_t result;
	if (stationary.cache && moving.cache)
		result = hitbox_pair_query(candidate, candidate_normal, model, *moving.cache, moving.orientation, *stationary.cache, stationary.orientation, stationary.position, frame, collision_model, start, end);
	else if (stationary.cache)
		result = hitbox_vector_query(candidate, candidate_normal, model, *stationary.cache, stationary.orientation, stationary.position, frame, collision_model, start, end, moving.size);
	else
		// Source passes v0 for BOTH endpoints and overrides the moving hitbox
		// position with the sweep endpoint, even for a nonzero sweep
		result = hitbox_vector_query(candidate, candidate_normal, model, *moving.cache, moving.orientation, end, frame, collision_model, stationary.position, stationary.position, stationary.size);
	if (result == INT32_MAX) return 0;
	if (registration) *registration = { true, candidate, model };
	if (result) {
		hit = candidate;
		normal = candidate_normal;
	}
	return result;
}
} // namespace xl_compat
#endif
