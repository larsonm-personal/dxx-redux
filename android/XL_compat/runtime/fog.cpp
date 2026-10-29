#include "fog.h"
#include "../fog.h"
extern "C" {
#include "inferno.h"
#include "segment.h"
#include "gameseg.h"
#include "game.h"
}

namespace
{
const xl_compat::Inventory *source = nullptr;
xl_compat::FogColors colors{};
} // namespace
void xl_fog_bind(const xl_compat::Inventory *level)
{
	source = level && xl_compat::volumetric_fog_required(*level) ? level : nullptr;
	colors = source ? xl_compat::fog_colors(*source) : xl_compat::FogColors{};
}
extern "C" int xl_fog_active(void)
{
	return source && !(Game_mode & GM_MULTI);
}
extern "C" int xl_fog_colors(float out[4][4])
{
	if (!xl_fog_active()) return 0;
	for (int type = 0; type < 4; ++type)
		for (int component = 0; component < 4; ++component) out[type][component] = colors[type][component];
	return 1;
}
extern "C" int xl_fog_boundary_points(int segment, int side, int near_cap, int32_t points[4][3], int *reversed)
{
	if (!xl_fog_active()) return 0;
	const auto boundary = xl_compat::fog_boundary(*source, segment, side, near_cap != 0);
	if (!boundary.type) return 0;
	int vertices[4];
	get_side_verts(vertices, boundary.segment, boundary.side);
	const auto &face = Segments[boundary.segment].sides[boundary.side];
	for (int i = 0; i < 4; ++i) {
		const auto &point = Vertices[vertices[(i + (face.type == SIDE_IS_TRI_13 ? 1 : 0)) % 4]];
		points[i][0] = point.x;
		points[i][1] = point.y;
		points[i][2] = point.z;
	}
	*reversed = boundary.reversed;
	return boundary.type;
}
