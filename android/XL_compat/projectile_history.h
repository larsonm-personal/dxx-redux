#ifndef DXX_XL_PROJECTILE_HISTORY_H
#define DXX_XL_PROJECTILE_HISTORY_H
#include "runtime/projectile.h"
#include <algorithm>
namespace xl_compat
{
inline bool projectile_has_hit(const xl_projectile_state &state, int victim)
{
	return std::find(state.victims, state.victims + state.count, victim) != state.victims + state.count;
}
inline int projectile_add_hit(xl_projectile_state &state, int victim)
{
	if (projectile_has_hit(state, victim)) return -1;
	if (state.count == XL_PROJECTILE_HITS) {
		// Intentional correction: source shifts right using overlapping memcpy,
		// duplicating the oldest entry and losing recent victims. Evict oldest
		std::move(state.victims + 1, state.victims + XL_PROJECTILE_HITS, state.victims);
		--state.count;
	}
	state.victims[state.count++] = victim;
	return 1;
}
} // namespace xl_compat
#endif
