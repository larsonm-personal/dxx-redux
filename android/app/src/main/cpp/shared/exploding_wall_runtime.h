#ifndef EXPLODING_WALL_RUNTIME_H
#define EXPLODING_WALL_RUNTIME_H

/* Include after fireball, segment and file-adapter declarations
 * Native D1 version 19 appends this pool; D2 already stores it in its base save */
#define EXPLODING_WALL_D1_SAVE_VERSION    19
#define EXPLODING_WALL_RUNTIME_DISK_BYTES (MAX_EXPLODING_WALLS * 3 * 4)

static inline int exploding_wall_runtime_decode(const void *data, size_t size, int swap,
                                                expl_wall *state)
{
	PHYSFS_sint32 words[MAX_EXPLODING_WALLS * 3];
	expl_wall decoded[MAX_EXPLODING_WALLS];
	int i, j;
	if (size < sizeof(words))
		return 0;
	memcpy(words, data, sizeof(words));
	for (i = 0; i < MAX_EXPLODING_WALLS; ++i) {
		if (swap)
			for (j = 0; j < 3; ++j) words[i * 3 + j] = SWAPINT(words[i * 3 + j]);
		decoded[i].segnum = words[i * 3];
		decoded[i].sidenum = words[i * 3 + 1];
		decoded[i].time = words[i * 3 + 2];
		if (decoded[i].segnum < -1 || decoded[i].segnum > Highest_segment_index)
			return 0;
		if (decoded[i].segnum != -1 &&
		    (decoded[i].sidenum < 0 || decoded[i].sidenum >= MAX_SIDES_PER_SEGMENT ||
		     decoded[i].time < 0 || decoded[i].time > F1_0))
			return 0;
		if (decoded[i].segnum != -1) {
			const segment *seg = &Segments[decoded[i].segnum];
			const int wall = seg->sides[decoded[i].sidenum].wall_num;
			const int child = seg->children[decoded[i].sidenum];
			if (wall < 0 || wall >= Num_walls || child < 0 || child > Highest_segment_index ||
			    Walls[wall].clip_num < 0 || Walls[wall].clip_num >= Num_wall_anims)
				return 0;
			for (j = 0; j < MAX_SIDES_PER_SEGMENT; ++j)
				if (Segments[child].children[j] == decoded[i].segnum) break;
			if (j == MAX_SIDES_PER_SEGMENT || Segments[child].sides[j].wall_num < 0 ||
			    Segments[child].sides[j].wall_num >= Num_walls)
				return 0;
		}
	}
	memcpy(state, decoded, sizeof(decoded));
	return 1;
}

static inline void exploding_wall_runtime_write(PHYSFS_file *fp)
{
	PHYSFS_sint32 words[MAX_EXPLODING_WALLS * 3];
	int i;
	for (i = 0; i < MAX_EXPLODING_WALLS; ++i) {
		const expl_wall *wall = &expl_wall_list[i];
		words[i * 3] = wall->segnum;
		/* Inactive slots are overwritten before use; do not persist dead history */
		words[i * 3 + 1] = wall->segnum == -1 ? 0 : wall->sidenum;
		words[i * 3 + 2] = wall->segnum == -1 ? 0 : wall->time;
	}
	PHYSFS_write(fp, words, sizeof(words), 1);
}

static inline int exploding_wall_runtime_read(PHYSFS_file *fp, int swap, int apply)
{
	PHYSFS_sint32 words[MAX_EXPLODING_WALLS * 3];
	expl_wall state[MAX_EXPLODING_WALLS];
	if (PHYSFS_read(fp, words, sizeof(words), 1) != 1 ||
	    !exploding_wall_runtime_decode(words, sizeof(words), swap, state))
		return 0;
	if (apply) memcpy(expl_wall_list, state, sizeof(state));
	return 1;
}

#endif
