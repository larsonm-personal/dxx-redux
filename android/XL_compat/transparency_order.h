#ifndef DXX_XL_TRANSPARENCY_ORDER_H
#define DXX_XL_TRANSPARENCY_ORDER_H
#include "particle_world.h"
#include <algorithm>
#include <optional>
namespace xl_compat
{
struct TransparencyPosition {
	ParticlePoint position;
	// Fixed world units; source particles use ten, other transparent items zero
	int bias = 0;
	// Source faces clamp to the visible depth bound; particles/objects drop
	bool clamp = false;
};
inline int transparency_depth(const ParticlePoint &position, const ParticlePoint &viewer)
{
	const auto difference = particle_difference(position, viewer);
	const double magnitude = std::sqrt(double(difference[0]) * difference[0] + double(difference[1]) * difference[1] + double(difference[2]) * difference[2]);
	if (magnitude > INT32_MAX) throw std::runtime_error("XL transparency render distance exceeds fixed range");
	return int(magnitude);
}
inline std::optional<TransparencyPosition> lightning_transparency_position(const ParticlePoint &start, const ParticlePoint &end, const ParticlePoint &viewer, int maximum_depth)
{
	if (maximum_depth < 0) throw std::runtime_error("XL lightning scene depth is negative");
	const int first = transparency_depth(start, viewer), last = transparency_depth(end, viewer);
	if (std::min(first, last) > maximum_depth) return std::nullopt;
	return TransparencyPosition{ first < last ? end : start, 0, true };
}
inline std::optional<int> transparency_bucket(const TransparencyPosition &position, const ParticlePoint &viewer, int maximum_depth)
{
	if (maximum_depth < 0 || position.bias < 0) throw std::runtime_error("XL transparency depth or bias is negative");
	int depth = transparency_depth(position.position, viewer);
	if (depth >= position.bias) depth -= position.bias;
	if (depth > maximum_depth) {
		if (!position.clamp) return std::nullopt;
		depth = maximum_depth;
	}
	const double scale = maximum_depth ? std::min(1.0, 249999.0 / maximum_depth) : 1.0;
	const int bucket = int(double(depth) * scale);
	return bucket < 250000 ? std::optional<int>(bucket) : std::nullopt;
}
// Single-thread source transparency profile. Each bucket prepends new items,
// so equal buckets render in reverse insertion order, not a stable depth sort
inline std::vector<std::size_t> transparency_draw_order(const std::vector<TransparencyPosition> &positions, const ParticlePoint &viewer, int maximum_depth)
{
	if (maximum_depth < 0) throw std::runtime_error("XL transparency render depth is negative");
	std::vector<std::pair<int, std::size_t>> keys;
	keys.reserve(positions.size());
	for (std::size_t i = 0; i < positions.size(); ++i) {
		if (const auto bucket = transparency_bucket(positions[i], viewer, maximum_depth)) keys.emplace_back(*bucket, i);
	}
	std::sort(keys.begin(), keys.end(), std::greater<>());
	std::vector<std::size_t> result;
	result.reserve(keys.size());
	for (const auto &key : keys) result.push_back(key.second);
	return result;
}
} // namespace xl_compat
#endif
