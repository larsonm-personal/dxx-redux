#include "object_pool.h"
#include "preview.h"
#include <algorithm>
#include <vector>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "weapon.h"
#include "laser.h"
}

extern "C" void xl_object_reclaim(int requested)
{
	if (!xl_level_extensions_active() || requested < 0 || requested >= object_capacity()) return;
	std::vector<int> candidates;
	int occupied = 0;
	for (int i = 0; i <= Highest_object_index; ++i) {
		const auto &object = Objects[i];
		if (object.type == OBJ_NONE || (object.flags & OF_SHOULD_BE_DEAD)) continue;
		++occupied;
		if (object.type == OBJ_DEBRIS || object.type == OBJ_FIREBALL || object.type == OBJ_WEAPON) candidates.push_back(i);
	}
	int remaining = (std::min) (occupied - requested, static_cast<int>(candidates.size()));
	// The reference decrements zero below zero at exact occupancy; make it a no-op
	if (remaining <= 0) return;
	for (int pass = 0; pass < 4; ++pass) {
		for (std::size_t i = 0; i < candidates.size();) {
			auto &object = Objects[candidates[i]];
			const bool match = pass == 0 ? object.type == OBJ_DEBRIS : pass == 1 ? object.type == OBJ_FIREBALL && object.ctype.expl_info.delete_objnum == -1
			                                                       : pass == 2   ? object.type == OBJ_WEAPON && object.id == FLARE_ID
			                                                                     : object.type == OBJ_WEAPON;
			if (!match) {
				++i;
				continue;
			}
			candidates[i] = candidates.back();
			candidates.pop_back();
			object.flags |= OF_SHOULD_BE_DEAD;
			if (!--remaining) return;
		}
	}
}
