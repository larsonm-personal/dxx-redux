#include "crossing_path.h"
#include "preview.h"
#include <algorithm>
extern "C" {
#include "fvi.h"
}

namespace
{
bool valid_path(const fvi_info &hit, int start)
{
	if (hit.n_segs < 1 || hit.n_segs > MAX_FVI_SEGS || hit.seglist[0] != start || hit.seglist[hit.n_segs - 1] != hit.hit_seg) return false;
	for (int index = 0; index < hit.n_segs; ++index) {
		const int segment = hit.seglist[index];
		if (segment < 0 || segment > Highest_segment_index) return false;
		if (index && segment != hit.seglist[index - 1] && find_connect_side(&Segments[segment], &Segments[hit.seglist[index - 1]]) < 0) return false;
	}
	return true;
}
} // namespace

extern "C" void xl_repair_crossing_path(const fvi_query *query, fvi_info *hit)
{
	if (!xl_level_extensions_active() || !(query->flags & FQ_GET_SEGLIST) || query->rad <= 0 || hit->hit_seg < 0 || hit->hit_type == HIT_BAD_P0 || valid_path(*hit, query->startseg)) return;
	// A swept hull can report the final cell without the intermediate portals
	// Trace only to its accepted point; never extend movement or dispatch triggers
	auto center = *query;
	center.rad = 0;
	center.p1 = &hit->hit_pnt;
	center.flags &= ~FQ_CHECK_OBJS;
	fvi_info path{};
	find_vector_intersection(&center, &path);
	// The zero-radius query cannot recurse into this repair
	if (path.hit_type == HIT_BAD_P0 || path.hit_seg != hit->hit_seg || !valid_path(path, query->startseg)) return;
	hit->n_segs = path.n_segs;
	std::copy_n(path.seglist, path.n_segs, hit->seglist);
}
