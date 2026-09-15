#include "coop_start_positions.h"
#include "game.h"
#include "gameseq.h"
#include "gameseg.h"
#include "object.h"
#include "player.h"
#include "polyobj.h"
#include "wall.h"
#include "fvi.h"

typedef struct coop_start_offset {
	sbyte r, u, f;
} coop_start_offset;

static int coop_start_too_close(vms_vector *pos, int assigned_count,
                                fix min_dist)
{
	int i;

	for (i = 0; i < assigned_count; i++)
		if (vm_vec_dist_quick(pos, &Player_init[i].pos) < min_dist)
			return 1;

	return 0;
}

int coop_find_fanout_start(int source, int assigned_count, vms_vector *pos,
                           short *segnum)
{
	static const coop_start_offset offsets[] = {
		{ 1, 0, 0 },
		{ -1, 0, 0 },
		{ 0, 1, 0 },
		{ 0, -1, 0 },
		{ 1, 1, 0 },
		{ -1, 1, 0 },
		{ 1, -1, 0 },
		{ -1, -1, 0 },
		{ 0, 0, 1 },
		{ 0, 0, -1 },
	};
	const fix ship_radius = Polygon_models[Player_ship->model_num].rad;
	const fix min_dist = ship_radius * 2;
	int scale_index, offset_index;

	for (scale_index = 0; scale_index < 2; scale_index++) {
		const fix scale = ship_radius * (scale_index ? 2 : 3);

		for (offset_index = 0;
		     offset_index < (int) (sizeof(offsets) / sizeof(offsets[0]));
		     offset_index++) {
			const coop_start_offset *offset = &offsets[offset_index];
			vms_vector candidate = Player_init[source].pos;
			int candidate_seg;

			if (offset->r)
				vm_vec_scale_add2(&candidate, &Player_init[source].orient.rvec,
				                  scale * offset->r);
			if (offset->u)
				vm_vec_scale_add2(&candidate, &Player_init[source].orient.uvec,
				                  scale * offset->u);
			if (offset->f)
				vm_vec_scale_add2(&candidate, &Player_init[source].orient.fvec,
				                  scale * offset->f);

			candidate_seg = find_point_seg(&candidate, Player_init[source].segnum);
			if (candidate_seg < 0)
				continue;
			if (coop_start_too_close(&candidate, assigned_count, min_dist))
				continue;

			*pos = candidate;
			*segnum = (short) candidate_seg;
			return 1;
		}
	}

	*pos = Player_init[source].pos;
	*segnum = Player_init[source].segnum;
	return 0;
}

static int arrival_position_clear(const vms_vector *pos, int segnum, fix radius,
                                  const obj_position *positions, int count)
{
	/* Staying fully inside the segment also excludes closed doors and lava
	 * surfaces, without relying on the point-only legacy start test */
	if (get_seg_masks(pos, segnum, radius + F1_0, __FILE__, __LINE__).facemask) return 0;
	for (int i = 0; i < count; ++i)
		if (vm_vec_dist(pos, &positions[i].pos) < 2 * radius + F1_0) return 0;
	for (int i = 0; i <= Highest_object_index; ++i) {
		const object *obj = &Objects[i];
		if (obj->flags & OF_SHOULD_BE_DEAD) continue;
		if (obj->type != OBJ_ROBOT && obj->type != OBJ_CNTRLCEN && obj->type != OBJ_CLUTTER) continue;
		if (vm_vec_dist(pos, &obj->pos) < radius + obj->size + F1_0) return 0;
	}
	return 1;
}

int coop_find_arrival_positions(const obj_position *anchor, int count, obj_position *positions)
{
	short queue[32];
	unsigned char visited[MAX_SEGMENTS] = { 0 };
	const fix radius = Polygon_models[Player_ship->model_num].rad;
	int head = 0, tail = 1, assigned = 0;
	if (!anchor || !positions || count < 1 || count > MAX_PLAYERS ||
	    anchor->segnum < 0 || anchor->segnum > Highest_segment_index || radius <= 0 ||
	    get_seg_masks(&anchor->pos, anchor->segnum, 0, __FILE__, __LINE__).centermask) return 0;
	queue[0] = anchor->segnum;
	visited[anchor->segnum] = 1;
	while (head < tail) {
		const int segnum = queue[head++];
		vms_vector center;
		compute_segment_center(&center, &Segments[segnum]);
		/* Prefer the authored anchor, then the center and surrounding positions */
		for (int pass = 0; pass < 4; ++pass) {
			const fix scale = radius * (pass == 2 ? 3 : 2) + F1_0;
			for (int r = -1; r <= 1; ++r)
				for (int u = -1; u <= 1; ++u)
					for (int f = -1; f <= 1; ++f) {
						if (pass < 2 && (r || u || f)) continue;
						if (pass >= 2 && !r && !u && !f) continue;
						vms_vector candidate = pass == 0 && segnum == anchor->segnum ? anchor->pos : center;
						if (pass >= 2) {
							vm_vec_scale_add2(&candidate, &anchor->orient.rvec, scale * r);
							vm_vec_scale_add2(&candidate, &anchor->orient.uvec, scale * u);
							vm_vec_scale_add2(&candidate, &anchor->orient.fvec, scale * f);
						}
						if (!arrival_position_clear(&candidate, segnum, radius, positions, assigned)) continue;
						positions[assigned] = *anchor;
						positions[assigned].pos = candidate;
						positions[assigned++].segnum = (short) segnum;
						if (assigned == count) return 1;
					}
		}
		for (int side = 0; side < 6 && tail < (int) (sizeof(queue) / sizeof(queue[0])); ++side) {
			const int child = Segments[segnum].children[side];
			vms_vector next_center;
			fvi_query query = { 0 };
			fvi_info hit;
			if (child < 0 || child > Highest_segment_index || visited[child] ||
			    !(WALL_IS_DOORWAY(&Segments[segnum], side) & WID_FLY_FLAG)) continue;
			compute_segment_center(&next_center, &Segments[child]);
			query.p0 = &center;
			query.p1 = &next_center;
			query.startseg = segnum;
			query.rad = radius + F1_0;
			query.thisobjnum = -1;
			if (find_vector_intersection(&query, &hit) != HIT_NONE) continue;
			visited[child] = 1;
			queue[tail++] = (short) child;
		}
	}
	return 0;
}
