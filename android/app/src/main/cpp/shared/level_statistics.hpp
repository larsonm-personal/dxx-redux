#ifndef DXX_LEVEL_STATISTICS_HPP
#define DXX_LEVEL_STATISTICS_HPP

#include <vector>

#ifdef DXX_BUILD_DESCENT_II
static const int LEVEL_STATISTICS_TEXTURE_LIMIT = 910;
#else
static const int LEVEL_STATISTICS_TEXTURE_LIMIT = 584;
#endif

struct level_statistics
{
	int segment_count;
	int wall_count;
	int trigger_count;
	int object_count;
	int texture_count;
};

template <typename Segment>
level_statistics collect_level_statistics(const Segment *segments, int segment_count,
                                           int wall_count, int trigger_count,
                                           int object_count, int texture_limit)
{
	level_statistics result = {
		segment_count > 0 ? segment_count : 0,
		wall_count > 0 ? wall_count : 0,
		trigger_count > 0 ? trigger_count : 0,
		object_count > 0 ? object_count : 0,
		0,
	};
	if (!segments || segment_count <= 0 || texture_limit <= 0)
		return result;

	std::vector<unsigned char> used(static_cast<size_t>(texture_limit));
	for (int segnum = 0; segnum < segment_count; ++segnum) {
		for (int sidenum = 0; sidenum < 6; ++sidenum) {
			const auto &side = segments[segnum].sides[sidenum];
			if (segments[segnum].children[sidenum] != -1 &&
			    (side.wall_num < 0 || side.wall_num >= wall_count))
				continue;
			const int base = side.tmap_num;
			const int overlay = static_cast<unsigned short>(side.tmap_num2) & 0x3fff;
			if (base >= 0 && base < texture_limit && !used[base]) {
				used[base] = 1;
				++result.texture_count;
			}
			if (overlay > 0 && overlay < texture_limit && !used[overlay]) {
				used[overlay] = 1;
				++result.texture_count;
			}
		}
	}
	return result;
}

#endif
