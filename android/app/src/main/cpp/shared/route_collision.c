#include "route_collision.h"

#include "gameseg.h"
#include "segment.h"
#include "wall.h"

/* Reuse the native face primitive; D1 also requires its owning segment */
#ifdef DXX_BUILD_DESCENT_II
extern int check_sphere_to_face(vms_vector *pnt, side *s, int facenum,
                                int nv, fix rad, int *vertex_list);
#else
extern int check_sphere_to_face(vms_vector *pnt, segment *sp, side *s, int facenum,
                                int nv, fix rad, int *vertex_list);
#endif

int route_object_intersects_blocking_wall(const object *objp)
{
	unsigned char visited[MAX_SEGMENTS] = { 0 };
	int pending[MAX_SEGMENTS];
	int count = 0;
	vms_vector point;
	if (!objp || objp->segnum < 0 || objp->segnum > Highest_segment_index || objp->size < 0)
		return 1;
	point = objp->pos;
	pending[count++] = objp->segnum;
	visited[objp->segnum] = 1;
	while (count) {
		const int segnum = pending[--count];
		segment *seg = &Segments[segnum];
		const int facemask = get_seg_masks(&point, segnum, objp->size, __FILE__, __LINE__).facemask;
		for (int sidenum = 0; sidenum < 6; ++sidenum) {
			int vertex_list[6], num_faces;
			if (!(facemask & (3 << (sidenum * 2))))
				continue;
			create_abs_vertex_lists(&num_faces, vertex_list, segnum, sidenum, __FILE__, __LINE__);
			for (int face = 0; face < 2; ++face) {
				if (!(facemask & (1 << (sidenum * 2 + face))))
					continue;
				const int hit = check_sphere_to_face(&point,
#ifndef DXX_BUILD_DESCENT_II
				                                     seg,
#endif
				                                     &seg->sides[sidenum], face, num_faces == 1 ? 4 : 3,
				                                     objp->size, vertex_list);
				if (!hit)
					continue;
				const int child = seg->children[sidenum];
				if (!(WALL_IS_DOORWAY(seg, sidenum) & WID_FLY_FLAG) ||
				    child < 0 || child > Highest_segment_index)
					return 1;
				if (!visited[child]) {
					visited[child] = 1;
					pending[count++] = child;
				}
			}
		}
	}
	return 0;
}
