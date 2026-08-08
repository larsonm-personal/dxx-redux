#ifndef DXX_REDUX_MERGED_WALL_GEOMETRY_HIT_H
#define DXX_REDUX_MERGED_WALL_GEOMETRY_HIT_H

enum merged_wall_geometry_outcome {
	MERGED_WALL_GEOMETRY_NO_WALL = 0,
	MERGED_WALL_GEOMETRY_WALL_HIT,
	MERGED_WALL_GEOMETRY_BAD_STARTSEG,
	MERGED_WALL_GEOMETRY_INVALID_WALL_HIT
};

static inline enum merged_wall_geometry_outcome
merged_wall_classify_geometry_hit(int fate, int hit_wall, int hit_bad_p0,
                                  int hit_side_seg, int hit_seg, int hit_side,
                                  int highest_segment_index, int max_sides,
                                  int *segnum_out, int *sidenum_out)
{
	int segnum;

	if (segnum_out)
		*segnum_out = -1;
	if (sidenum_out)
		*sidenum_out = -1;
	if (fate != hit_wall)
		return fate == hit_bad_p0 ? MERGED_WALL_GEOMETRY_BAD_STARTSEG
		                          : MERGED_WALL_GEOMETRY_NO_WALL;

	segnum = hit_side_seg >= 0 ? hit_side_seg : hit_seg;
	if (segnum < 0 || segnum > highest_segment_index ||
	    hit_side < 0 || hit_side >= max_sides)
		return MERGED_WALL_GEOMETRY_INVALID_WALL_HIT;
	if (segnum_out)
		*segnum_out = segnum;
	if (sidenum_out)
		*sidenum_out = hit_side;
	return MERGED_WALL_GEOMETRY_WALL_HIT;
}

static inline const char *
merged_wall_geometry_outcome_status(enum merged_wall_geometry_outcome outcome)
{
	switch (outcome) {
		case MERGED_WALL_GEOMETRY_WALL_HIT:
			return "hit";
		case MERGED_WALL_GEOMETRY_BAD_STARTSEG:
			return "bad_startseg";
		case MERGED_WALL_GEOMETRY_INVALID_WALL_HIT:
			return "invalid_wall_hit";
		default:
			return "no_wall";
	}
}

#endif
