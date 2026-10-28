#include <array>
#include "projectile.h"
#include "../projectile_history.h"
#include "preview.h"
extern "C" {
#include "inferno.h"
#include "object.h"
#include "laser.h"
}

namespace
{
std::array<xl_projectile_state, MAX_OBJECTS> states{};
bool live(int owner)
{
	return owner >= 0 && owner <= Highest_object_index && owner < object_capacity() &&
	       Objects[owner].type == OBJ_WEAPON && Objects[owner].control_type == CT_WEAPON;
}
const xl_projectile_state *state_for(int owner)
{
	if (!xl_level_extensions_active() || !live(owner)) return nullptr;
	const auto &state = states[owner];
	return state.count && state.signature == Objects[owner].signature ? &state : nullptr;
}
} // namespace

extern "C" void xl_projectile_clear(void)
{
	states.fill({});
}
extern "C" void xl_projectile_forget(int owner)
{
	if (owner >= 0 && owner < MAX_OBJECTS) states[owner] = {};
}
extern "C" int xl_projectile_has_hit(int owner, int victim)
{
	const auto *state = state_for(owner);
	return state && xl_compat::projectile_has_hit(*state, victim);
}
extern "C" int xl_projectile_add_hit(int owner, int victim)
{
	if (!xl_level_extensions_active() || !live(owner) || victim < 0 || victim >= MAX_OBJECTS) return -1;
	auto &state = states[owner];
	if (!state_for(owner)) state = { owner, Objects[owner].signature, 0, {} };
	const auto result = xl_compat::projectile_add_hit(state, victim);
	if (result > 0) Objects[owner].ctype.laser_info.last_hitobj = victim;
	return result;
}
extern "C" int xl_projectile_state_get(int owner, xl_projectile_state *result)
{
	const auto *state = state_for(owner);
	if (!result || !state) return 0;
	*result = *state;
	return 1;
}
extern "C" int xl_projectile_restore(const xl_projectile_state *input, int count, int apply)
{
	if (!xl_level_extensions_active() || count < 0 || count > MAX_OBJECTS || (!input && count)) return 0;
	int previous = -1;
	for (int row = 0; row < count; ++row) {
		const auto &state = input[row];
		if (state.owner <= previous || !live(state.owner) || state.signature != Objects[state.owner].signature ||
		    state.count < 1 || state.count > XL_PROJECTILE_HITS ||
		    Objects[state.owner].ctype.laser_info.last_hitobj != state.victims[state.count - 1]) return 0;
		for (int i = 0; i < state.count; ++i) {
			// Victims retain source slot identity even after deletion or reuse
			if (state.victims[i] < 0 || state.victims[i] >= object_capacity() ||
			    std::find(state.victims, state.victims + i, state.victims[i]) != state.victims + i) return 0;
		}
		previous = state.owner;
	}
	int row = 0;
	for (int owner = 0; owner <= Highest_object_index; ++owner) {
		if (!live(owner)) continue;
		if (Objects[owner].ctype.laser_info.last_hitobj < -1) return 0;
		if (Objects[owner].ctype.laser_info.last_hitobj >= 0 && (row == count || input[row++].owner != owner)) return 0;
	}
	if (row != count) return 0;
	if (apply) {
		states.fill({});
		for (int i = 0; i < count; ++i) states[input[i].owner] = input[i];
	}
	return 1;
}
extern "C" void xl_projectile_impact(int owner, int other)
{
	if (!xl_level_extensions_active() || !live(owner) || other < 0 || other > Highest_object_index) return;
	auto &weapon = Objects[owner];
	const auto &target = Objects[other];
	if (weapon.id == ROBOT_SUPERPROX_ID || is_proximity_bomb_or_smart_mine_or_placed_mine(static_cast<weapon_type_t>(weapon.id)) || !(weapon.mtype.phys_info.flags & PF_PERSISTENT)) {
		weapon.flags |= OF_SHOULD_BE_DEAD;
	} else if (!(target.mtype.phys_info.flags & PF_PERSISTENT)) {
		weapon.shields -= target.shields / (target.type == OBJ_WEAPON ? 2 : 4);
		if (weapon.shields <= 0) {
			weapon.shields = 0;
			weapon.flags |= OF_SHOULD_BE_DEAD;
		}
	}
}
