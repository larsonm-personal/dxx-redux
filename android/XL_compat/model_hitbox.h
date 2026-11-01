#ifndef DXX_XL_MODEL_HITBOX_H
#define DXX_XL_MODEL_HITBOX_H
#include "model_geometry.h"
#include "lighting.h"
namespace xl_compat
{
inline constexpr std::array<std::array<unsigned, 4>, 6> model_hitbox_faces{ { { 0, 1, 2, 3 }, { 0, 3, 7, 4 }, { 5, 6, 2, 1 }, { 4, 7, 6, 5 }, { 4, 5, 1, 0 }, { 6, 7, 3, 2 } } };
struct ModelHitbox {
	std::array<FixedLightPoint, 8> vertices{};
	std::array<FixedLightPoint, 6> normals{};
};
inline ModelHitbox model_hitbox(const ModelHitboxBounds &bounds)
{
	constexpr unsigned corners[8][3] = { { 1, 0, 1 }, { 0, 0, 1 }, { 0, 1, 1 }, { 1, 1, 1 }, { 1, 0, 0 }, { 0, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 } };
	ModelHitbox box;
	for (unsigned i = 0; i < 8; ++i)
		for (unsigned axis = 0; axis < 3; ++axis) {
			if (bounds.low[axis] > bounds.high[axis]) throw std::runtime_error("Inverted model hitbox");
			const auto value = int64_t(corners[i][axis] ? bounds.low[axis] : bounds.high[axis]) + bounds.offset[axis];
			if (value < INT32_MIN || value > INT32_MAX) throw std::runtime_error("Model hitbox vertex overflows");
			box.vertices[i][axis] = int32_t(value);
		}
	for (unsigned i = 0; i < 6; ++i) {
		const auto &face = model_hitbox_faces[i];
		box.normals[i] = geometry_default_normal(box.vertices[face[0]], box.vertices[face[1]], box.vertices[face[2]]);
	}
	return box;
}
struct WorldHitbox {
	std::array<std::array<FixedLightPoint, 4>, 6> faces{};
	std::array<FixedLightPoint, 6> normals{};
};
// One instance per model, matching release TransformHitboxes' model/frame cache
class ModelHitboxTransform
{
	std::vector<ModelHitbox> local;
	std::vector<WorldHitbox> world;
	int frame;

  public:
	explicit ModelHitboxTransform(const std::vector<ModelHitboxBounds> &bounds, int initial_frame = 0) : world(bounds.size()), frame(initial_frame)
	{
		if (bounds.size() < 2) throw std::runtime_error("Model hitboxes require a combined and root box");
		for (const auto &box : bounds) local.push_back(model_hitbox(box));
	}
	const std::vector<WorldHitbox> &transform(const std::array<FixedLightPoint, 3> &orientation, const FixedLightPoint &position, int current_frame, int collision_model)
	{
		if (frame == current_frame) return world;
		auto next = world;
		const auto first = collision_model == 1 ? 0u : 1u;
		const auto last = collision_model == 1 ? 1u : local.size() - 1;
		for (std::size_t box = first; box <= last; ++box) {
			std::array<FixedLightPoint, 8> vertices;
			for (unsigned vertex = 0; vertex < 8; ++vertex)
				for (unsigned axis = 0; axis < 3; ++axis) {
					const auto &v = local[box].vertices[vertex];
					// View(0) is the object's transposed orientation; source Dot
					// truncates before the fixed-point position is added
					const auto rotated = (double(v[0]) * orientation[0][axis] + double(v[1]) * orientation[1][axis] + double(v[2]) * orientation[2][axis]) / 65536.0;
					if (!std::isfinite(rotated) || rotated < INT32_MIN || rotated > INT32_MAX) throw std::runtime_error("Hitbox transform rotation overflows");
					const auto value = int64_t(int32_t(rotated)) + position[axis];
					if (value < INT32_MIN || value > INT32_MAX) throw std::runtime_error("Hitbox transform position overflows");
					vertices[vertex][axis] = int32_t(value);
				}
			for (unsigned face = 0; face < 6; ++face) {
				for (unsigned corner = 0; corner < 4; ++corner) next[box].faces[face][corner] = vertices[model_hitbox_faces[face][corner]];
				const auto &points = next[box].faces[face];
				next[box].normals[face] = geometry_default_normal(points[0], points[1], points[2]);
			}
		}
		world.swap(next);
		frame = current_frame;
		return world;
	}
};
} // namespace xl_compat
#endif
