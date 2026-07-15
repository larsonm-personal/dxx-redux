#include "coop_start_positions.h"
#include "game.h"
#include "gameseq.h"
#include "gameseg.h"
#include "object.h"
#include "player.h"
#include "polyobj.h"

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
