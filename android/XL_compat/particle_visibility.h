#ifndef DXX_XL_PARTICLE_VISIBILITY_H
#define DXX_XL_PARTICLE_VISIBILITY_H
#include "particle_world.h"

namespace xl_compat
{
inline int particle_depth_limit(const std::vector<ParticleSegment> &segments, const std::vector<int> &visible, const ParticlePoint &eye)
{
	int maximum = 0;
	for (int index : visible) {
		if (index < 0 || std::size_t(index) >= segments.size()) throw std::runtime_error("XL particle visibility references an invalid segment");
		const auto &segment = segments[index];
		std::array<float, 3> delta;
		for (unsigned axis = 0; axis < 3; ++axis) delta[axis] = float(segment.center[axis]) / 65536.0f - float(eye[axis]) / 65536.0f;
		const float distance = std::sqrt(delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2]) * 65536.0f;
		if (!std::isfinite(distance) || double(distance) > INT32_MAX) throw std::runtime_error("XL particle visibility exceeds the source depth range");
		maximum = std::max(maximum, particle_detail::signed_checked(std::int64_t(distance) + segment.outer_radius));
	}
	return maximum;
}

inline bool particle_segment_visible(int segment, const std::vector<bool> &visible)
{
	return segment >= 0 && std::size_t(segment) < visible.size() && visible[segment];
}

inline bool particle_emitter_visible(int segment, bool skybox, const std::vector<bool> &visible)
{
	// The pinned manager uses a separate scratch visibility state whose local
	// visible flags remain clear. Only its global early-visible test can pass
	return segment < 0 || skybox || particle_segment_visible(segment, visible);
}

template <class Locate>
bool particle_visible(FireParticle &particle, const std::vector<ParticleSegment> &segments, const std::vector<bool> &visible, Locate locate)
{
	if (particle.segment < 0 || std::size_t(particle.segment) >= segments.size()) return false;
	if (particle_segment_visible(particle.segment, visible)) return true;
	for (const auto &side : segments[particle.segment].sides)
		if (particle_segment_visible(side.child, visible)) return true;
	const int located = locate(particle.position, particle.skybox, particle.segment);
	if (located < 0) return false;
	particle.segment = located;
	return particle_segment_visible(located, visible);
}
} // namespace xl_compat
#endif
