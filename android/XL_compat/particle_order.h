#ifndef DXX_XL_PARTICLE_ORDER_H
#define DXX_XL_PARTICLE_ORDER_H
#include "particle_world.h"
#include <algorithm>
namespace xl_compat
{
// Single-thread source transparency profile. Each bucket prepends new items,
// so equal buckets render in reverse insertion order, not a stable depth sort
inline std::vector<std::size_t> particle_draw_order(const std::vector<ParticlePoint> &positions, const ParticlePoint &viewer, int maximum_depth)
{
	if (maximum_depth < 0) throw std::runtime_error("XL particle render depth is negative");
	const double scale = maximum_depth ? std::min(1.0, 249999.0 / maximum_depth) : 1.0;
	std::vector<std::pair<int, std::size_t>> keys;
	keys.reserve(positions.size());
	for (std::size_t i = 0; i < positions.size(); ++i) {
		const auto difference = particle_difference(positions[i], viewer);
		const double magnitude = std::sqrt(double(difference[0]) * difference[0] + double(difference[1]) * difference[1] + double(difference[2]) * difference[2]);
		if (magnitude > INT32_MAX) throw std::runtime_error("XL particle render distance exceeds fixed range");
		int depth = int(magnitude);
		if (depth >= 10 * 65536) depth -= 10 * 65536;
		if (depth > maximum_depth) continue;
		const int bucket = int(double(depth) * scale);
		if (bucket >= 250000) continue;
		keys.emplace_back(bucket, i);
	}
	std::sort(keys.begin(), keys.end(), std::greater<>());
	std::vector<std::size_t> result;
	result.reserve(keys.size());
	for (const auto &key : keys) result.push_back(key.second);
	return result;
}
} // namespace xl_compat
#endif
