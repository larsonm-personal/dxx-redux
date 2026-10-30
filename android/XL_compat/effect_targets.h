#ifndef DXX_XL_EFFECT_TARGETS_H
#define DXX_XL_EFFECT_TARGETS_H
#include "inventory.h"
namespace xl_compat
{
inline std::vector<std::array<int, 2>> effect_targets(const Trigger &action, std::size_t segments, std::size_t objects, bool attached)
{
	auto result = action.targets;
	if (action.type != 32 && action.type != 33) return result;
	// CTriggerTargets::Check compacts invalid indices during source loading
	std::size_t count = 0;
	for (const auto &target : action.targets) {
		const bool valid = target[0] >= 0 && (target[1] < 0 ? std::size_t(target[0]) < objects : std::size_t(target[0]) < segments && target[1] < 6);
		if (valid) result[count++] = target;
	}
	result.resize(count);
	// The later geometry-trigger pass accepts only -1 and removes other negative
	// sides by swapping in the last target. Object-attached lists skip this pass
	if (!attached)
		for (std::size_t i = 0; i < result.size();) {
			if (result[i][1] >= -1) ++i;
			else {
				result[i] = result.back();
				result.pop_back();
			}
		}
	return result;
}
} // namespace xl_compat
#endif
