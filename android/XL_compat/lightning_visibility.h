#ifndef DXX_XL_LIGHTNING_VISIBILITY_H
#define DXX_XL_LIGHTNING_VISIBILITY_H
#include "particle_visibility.h"

namespace xl_compat
{
template <class Locate>
bool lightning_visible(bool owner_rendered, int segment, const ParticlePoint &start, const ParticlePoint &end, const std::vector<bool> &visible, Locate locate)
{
	// Lightning uses the same initialized scratch visibility state as emitters
	if (owner_rendered || particle_segment_visible(segment, visible)) return true;
	if (particle_segment_visible(locate(end), visible)) return true;
	ParticlePoint middle;
	for (unsigned axis = 0; axis < 3; ++axis) middle[axis] = particle_detail::signed_checked((std::int64_t(start[axis]) + end[axis]) / 2);
	return particle_segment_visible(locate(middle), visible);
}
} // namespace xl_compat
#endif
