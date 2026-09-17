#ifndef AUTOSELECT_RUNTIME_H
#define AUTOSELECT_RUNTIME_H

/* Included after the save/rewind file adapter and engine weapon declarations */
extern int delayed_primary_autoselect_weapon_index;
extern int delayed_secondary_autoselect_weapon_index;

static inline void autoselect_write_runtime_state(PHYSFS_file *fp)
{
	const int state[4] = { PrimaryWeaponPickedUp, SecondaryWeaponPickedUp,
		                   delayed_primary_autoselect_weapon_index, delayed_secondary_autoselect_weapon_index };
	PHYSFS_write(fp, state, sizeof(state[0]), 4);
}

/* Validation also consumes the record, without changing the current ship */
static inline int autoselect_read_runtime_state(PHYSFS_file *fp, int swap, int apply)
{
	int state[4];
	int i;
	if (PHYSFS_read(fp, state, sizeof(state[0]), 4) != 4)
		return 0;
	if (swap)
		for (i = 0; i < 4; ++i)
			state[i] = SWAPINT(state[i]);
	if (state[0] < 0 || state[0] > 1 || state[1] < 0 || state[1] > 1 ||
	    state[2] < -1 ||
#ifdef DXX_BUILD_DESCENT_II
	    state[2] >= MAX_PRIMARY_WEAPONS ||
#else
	    (state[2] >= MAX_PRIMARY_WEAPONS && state[2] != 16) ||
#endif
	    state[3] < -1 || state[3] >= MAX_SECONDARY_WEAPONS)
		return 0;
	if (apply) {
		PrimaryWeaponPickedUp = state[0];
		SecondaryWeaponPickedUp = state[1];
		delayed_primary_autoselect_weapon_index = state[2];
		delayed_secondary_autoselect_weapon_index = state[3];
	}
	return 1;
}

#endif
