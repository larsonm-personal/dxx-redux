#ifndef SEISMIC_RUNTIME_H
#define SEISMIC_RUNTIME_H

#include "save_epoch_format.h"

/* Include after weapon declarations and the disk/rewind file adapter
 * Store full absolute timestamps alongside the saved native clock epoch */
#define SEISMIC_RUNTIME_DISK_BYTES (SEISMIC_RUNTIME_DETONATION_SLOTS * 8 + 8 + 8 + 4)

/* Match the narrowed fixed-point arithmetic in the original shake routines */
static inline int seismic_runtime_state_is_valid(const seismic_runtime_state *state, fix64 epoch)
{
	int i;
	if (!save_epoch_valid(epoch) || state->tremor_magnitude < -F1_0 || state->tremor_magnitude > F1_0)
		return 0;
	for (i = 0; i < SEISMIC_RUNTIME_DETONATION_SLOTS; ++i) {
		const fix64 time = state->detonation_times[i];
		if (!save_timestamp_valid(time) ||
		    (time != 0 && (time > epoch || epoch - time > INT32_MAX)))
			return 0;
	}
	if (!save_timestamp_valid(state->disturbance_start_time) || !save_timestamp_valid(state->disturbance_end_time))
		return 0;
	if (state->disturbance_start_time != 0 || state->disturbance_end_time != 0) {
		const fix64 duration = state->disturbance_end_time - state->disturbance_start_time;
		if (state->disturbance_start_time > epoch || duration <= 0 || duration > INT32_MAX)
			return 0;
		/* Expired ambient endpoints persist; elapsed time is only narrowed while active */
	}
	return 1;
}

static inline int seismic_runtime_write(PHYSFS_file *fp)
{
	seismic_runtime_state state;
	PHYSFS_uint32 words[SEISMIC_RUNTIME_DISK_BYTES / 4];
	int i;
	weapon_get_seismic_runtime_state(&state);
	if (!seismic_runtime_state_is_valid(&state, GameTime64))
		return 0;
	for (i = 0; i < SEISMIC_RUNTIME_DETONATION_SLOTS + 2; ++i) {
		const fix64 value = i < SEISMIC_RUNTIME_DETONATION_SLOTS ? state.detonation_times[i] :
			i == SEISMIC_RUNTIME_DETONATION_SLOTS ? state.disturbance_start_time : state.disturbance_end_time;
		const PHYSFS_uint64 bits = (PHYSFS_uint64)value;
		words[i * 2] = (PHYSFS_uint32)bits;
		words[i * 2 + 1] = (PHYSFS_uint32)(bits >> 32);
	}
	words[SEISMIC_RUNTIME_DISK_BYTES / 4 - 1] = (PHYSFS_uint32)state.tremor_magnitude;
	return PHYSFS_write(fp, words, sizeof(words), 1) == 1;
}

/* A short record must not partially replace active seismic state */
static inline int seismic_runtime_read(PHYSFS_file *fp, int swap, int apply, fix64 saved_epoch, fix64 chosen_epoch)
{
	seismic_runtime_state state;
	PHYSFS_uint32 words[SEISMIC_RUNTIME_DISK_BYTES / 4];
	int i;
	if (PHYSFS_read(fp, words, sizeof(words), 1) != 1)
		return 0;
	if (swap)
		for (i = 0; i < SEISMIC_RUNTIME_DISK_BYTES / 4; ++i)
			words[i] = SWAPINT(words[i]);
	for (i = 0; i < SEISMIC_RUNTIME_DETONATION_SLOTS + 2; ++i) {
		const PHYSFS_uint64 bits = (PHYSFS_uint64)words[i * 2] | ((PHYSFS_uint64)words[i * 2 + 1] << 32);
		fix64 value;
		memcpy(&value, &bits, sizeof(value));
		if (i < SEISMIC_RUNTIME_DETONATION_SLOTS)
			state.detonation_times[i] = value;
		else if (i == SEISMIC_RUNTIME_DETONATION_SLOTS)
			state.disturbance_start_time = value;
		else
			state.disturbance_end_time = value;
	}
	memcpy(&state.tremor_magnitude, &words[SEISMIC_RUNTIME_DISK_BYTES / 4 - 1], sizeof(state.tremor_magnitude));
	if (!seismic_runtime_state_is_valid(&state, saved_epoch) || !save_epoch_valid(chosen_epoch))
		return 0;
	for (i = 0; i < SEISMIC_RUNTIME_DETONATION_SLOTS; ++i) {
		if (state.detonation_times[i] != 0) {
			const fix64 rebased = chosen_epoch + (state.detonation_times[i] - saved_epoch);
			if (!save_timestamp_valid(rebased))
				return 0;
			/* Replay/rewind overrides can land an active event on the inactive sentinel
			 * Match the existing death-timer convention, within one fixed-point tick */
			state.detonation_times[i] = rebased ? rebased : -1;
		}
	}
	/* Ambient start zero is valid when an event began at the original epoch zero */
	if (state.disturbance_start_time != 0 || state.disturbance_end_time != 0) {
		state.disturbance_start_time = chosen_epoch + (state.disturbance_start_time - saved_epoch);
		state.disturbance_end_time = chosen_epoch + (state.disturbance_end_time - saved_epoch);
		if (!save_timestamp_valid(state.disturbance_start_time) || !save_timestamp_valid(state.disturbance_end_time))
			return 0;
	}
	if (!seismic_runtime_state_is_valid(&state, chosen_epoch))
		return 0;
	if (apply)
		weapon_set_seismic_runtime_state(&state);
	return 1;
}

#endif
