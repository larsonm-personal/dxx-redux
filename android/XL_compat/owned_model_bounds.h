#ifndef DXX_XL_OWNED_MODEL_BOUNDS_H
#define DXX_XL_OWNED_MODEL_BOUNDS_H
#include "model_render_geometry.h"
namespace xl_compat
{
// CModel::Size uses face-used local bounds and separately truncated fixed offsets
inline std::vector<ModelHitboxBounds> owned_model_hitboxes(const std::vector<RenderModelPart> &parts)
{
	if (parts.empty() || parts.size() > 256) throw std::runtime_error("Invalid owned model part count");
	const auto fixed = [](double value) {
		if (!std::isfinite(value) || value < INT32_MIN || value > INT32_MAX) throw std::runtime_error("Owned model bound overflows");
		return int32_t(value);
	};
	std::vector<ModelHitboxBounds> result(1);
	result[0].low.fill(INT32_MAX);
	result[0].high.fill(-INT32_MAX);
	for (std::size_t i = 0; i < parts.size(); ++i) {
		const auto &part = parts[i];
		if (parts.size() != 1 && part.excluded_from_hitboxes) continue;
		if (!part.faces) throw std::runtime_error("Owned model body part has no faces");
		ModelHitboxBounds box;
		for (unsigned axis = 0; axis < 3; ++axis) {
			box.low[axis] = fixed(double(part.low[axis]) * 65536.0);
			box.high[axis] = fixed(double(part.high[axis]) * 65536.0);
			if (box.low[axis] > box.high[axis]) throw std::runtime_error("Owned model body bounds are inverted");
			int parent = int(i);
			int64_t offset = 0;
			for (std::size_t depth = 0; parent != -1; ++depth) {
				if (parent < 0 || std::size_t(parent) >= parts.size() || depth >= parts.size()) throw std::runtime_error("Invalid owned model bound hierarchy");
				offset += parts[parent].offset[axis];
				fixed(double(offset));
				parent = parts[parent].parent;
			}
			box.offset[axis] = int32_t(offset);
			const auto span = int64_t(box.high[axis]) - box.low[axis];
			if (span > INT32_MAX) throw std::runtime_error("Owned model body extent overflows");
			box.half_size[axis] = int32_t(span / 2);
			result[0].low[axis] = (std::min) (result[0].low[axis], fixed(double(int64_t(box.low[axis]) + offset)));
			result[0].high[axis] = (std::max) (result[0].high[axis], fixed(double(int64_t(box.high[axis]) + offset)));
		}
		result.push_back(box);
	}
	if (result.size() == 1) throw std::runtime_error("Owned model has no hitbox body");
	for (unsigned axis = 0; axis < 3; ++axis) {
		const auto span = int64_t(result[0].high[axis]) - result[0].low[axis];
		if (span > INT32_MAX) throw std::runtime_error("Owned combined model extent overflows");
		// CModel::Size stores the full span in index0, unlike CPolyModel::Size
		result[0].half_size[axis] = int32_t(span);
	}
	return result;
}
} // namespace xl_compat
#endif
