#ifndef CADENCE_RUNTIME_H
#define CADENCE_RUNTIME_H

/* Include after the engine declarations and disk/rewind file adapter */
#define CADENCE_D1_SAVE_VERSION    18
#define CADENCE_D2_SAVE_VERSION    37
#define CADENCE_RUNTIME_DISK_BYTES 24

typedef struct cadence_runtime_state {
	fix64 fusion;
	fix64 refuel;
	fix64 collision;
} cadence_runtime_state;

static inline void cadence_runtime_apply(const cadence_runtime_state *state)
{
	game_set_fusion_next_sound_time(state->fusion);
	fuelcen_set_last_sound_time(state->refuel);
	collide_set_collision_delay_last_play_time(state->collision);
}

static inline void cadence_runtime_reset(void)
{
	const cadence_runtime_state state = { 0, 0, 0 };
	cadence_runtime_apply(&state);
}

/* Low word first, each word in the save's byte order, matching effect clocks
 * Unsigned arithmetic preserves all 64 bits without signed overflow at rebases */
static inline int cadence_runtime_decode(const void *data, size_t size, int swap,
                                         fix64 epoch, cadence_runtime_state *state)
{
	PHYSFS_uint32 words[6];
	fix64 clocks[3];
	int i;
	if (size < CADENCE_RUNTIME_DISK_BYTES)
		return 0;
	memcpy(words, data, sizeof(words));
	for (i = 0; i < 3; ++i) {
		PHYSFS_uint32 low = words[i * 2], high = words[i * 2 + 1];
		PHYSFS_uint64 delta;
		if (swap) {
			low = SWAPINT(low);
			high = SWAPINT(high);
		}
		delta = ((PHYSFS_uint64) high << 32) | low;
		clocks[i] = (fix64) ((PHYSFS_uint64) epoch + delta);
	}
	state->fusion = clocks[0];
	state->refuel = clocks[1];
	state->collision = clocks[2];
	return 1;
}

static inline void cadence_runtime_write(PHYSFS_file *fp, fix64 epoch)
{
	const fix64 clocks[3] = { game_get_fusion_next_sound_time(),
		                      fuelcen_get_last_sound_time(),
		                      collide_get_collision_delay_last_play_time() };
	PHYSFS_uint32 words[6];
	int i;
	for (i = 0; i < 3; ++i) {
		const PHYSFS_uint64 delta = (PHYSFS_uint64) clocks[i] - (PHYSFS_uint64) epoch;
		words[i * 2] = (PHYSFS_uint32) delta;
		words[i * 2 + 1] = (PHYSFS_uint32) (delta >> 32);
	}
	PHYSFS_write(fp, words, sizeof(words), 1);
}

/* Validation consumes the complete record before any clock can change */
static inline int cadence_runtime_read(PHYSFS_file *fp, int swap, int apply, fix64 epoch)
{
	PHYSFS_uint32 words[6];
	cadence_runtime_state state;
	if (PHYSFS_read(fp, words, sizeof(words), 1) != 1 ||
	    !cadence_runtime_decode(words, sizeof(words), swap, epoch, &state))
		return 0;
	if (apply)
		cadence_runtime_apply(&state);
	return 1;
}

#endif
